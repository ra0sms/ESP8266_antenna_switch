
/*
Remote wifi control console
Irkutsk, 2019
ra0sms@bk.ru

June 2025 - Add CSS, improve visability, add authentification
November 2026 - WiFi works the same way as in K9AY controller: non-blocking STA,
                SoftAP with captive portal, mDNS (rkconsole.local), async scan
                Web interface works without page reloads

HTTP API:
  GET  /api/state            -> {"mask":5,"labels":["A1",...]}  bit N of mask - channel N is on
  POST /set  sel=N           only this channel on, all others off
             ch=N on=0|1     switch one channel, others are not changed
             off=1           all channels off
       N is 0..NUM_OUTPUTS-1
       example: curl -d "sel=2" http://rkconsole.local/set
*/


#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>

#define FW_VERSION "2026-11-05"

/* Board variant: 8 outputs, or 4 outputs for 12 V relays. */
#ifndef NUM_OUTPUTS
#define NUM_OUTPUTS 8
#endif
#if NUM_OUTPUTS != 8 && NUM_OUTPUTS != 4
#error "NUM_OUTPUTS must be 8 or 4"
#endif

const char *myHostname = "rkconsole";   // http://rkconsole.local


const bool AUTH = false;

const char* www_username = "admin";
const char* www_password = "password123";
 
// SoftAP name is built from chip ID: RKconsole_xxxxxx
const char *softAP_password = "1234567890";

 
char ssid[33] = "";
char password[65] = "";
char labels[NUM_OUTPUTS][32];

#if NUM_OUTPUTS == 8
const uint8_t relayPins[NUM_OUTPUTS] = {16, 5, 4, 0, 2, 14, 12, 13};
#else
const uint8_t relayPins[NUM_OUTPUTS] = {16, 5, 4, 0};
#endif
const uint8_t ledPin = 15;
uint8_t outputs = 0;   // bit N - channel N is on
char apSsid[33];


ESP8266WebServer server(80);

/** Returns true if the request is authorized; sends 401 when denied. */
bool authorized() {
  if (!AUTH) return true;
  if (!server.authenticate(www_username, www_password)) {
    server.requestAuthentication();
    return false;
  }
  return true;
}


IPAddress apIP(192, 168, 4, 1);
IPAddress netMsk(255, 255, 255, 0);
const byte DNS_PORT = 53;
DNSServer dnsServer;
const unsigned long RECONNECT_INTERVAL = 60000;   // retry after failed attempt
const unsigned long RETRY_ABORTED = 3000;         // retry after attempt aborted (e.g. by scan)
const unsigned long CONNECT_TIMEOUT = 30000;
bool connectRequested = false;
unsigned long lastConnectTry = 0;
unsigned long nextConnectAt = 0;
bool connecting = false;   // STA connection attempt in progress
// Channel/BSSID of networks from the last scan: first connect after saving goes
// directly to the router channel, STA does not hop over channels (SoftAP stays stable)
struct ScanNet {
  char ssid[33];
  uint8_t bssid[6];
  int32_t channel;
  int32_t rssi;
};
const uint8_t SCAN_CACHE_SIZE = 20;
ScanNet scanCache[SCAN_CACHE_SIZE];
uint8_t scanCacheCount = 0;
const ScanNet *connectHint = nullptr;   // used once by connectWifi()
WiFiEventHandler staDisconnectedHandler;
volatile int lastDisconnectReason = -1;
wl_status_t wifiStatus = WL_IDLE_STATUS;
uint32_t uptimeSec = 0;
uint32_t staConnectedAt = 0;
// SoftAP is switched off only after WLAN has been connected for AP_OFF_DELAY,
// and switched on again when WLAN has been lost for AP_ON_DELAY
const unsigned long AP_OFF_DELAY = 30000;
const unsigned long AP_ON_DELAY = 15000;
bool apOff = false;      // disable SoftAP while connected to WLAN (saved in flash)
bool apActive = false;
bool staUp = false;
unsigned long staChangedAt = 0;

void sendNoCache() {
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
}

void redirect(const char *to) {
  server.sendHeader("Location", to, true);
  server.send(303, "text/plain", "");
}

bool isIp(const String &s) {
  for (char c : s)
    if (c != '.' && (c < '0' || c > '9')) return false;
  return s.length() > 0;
}

// SoftAP clients asking for a foreign host are redirected to our page (captive portal)
bool captivePortal() {
  if (server.client().localIP() != apIP) return false;
  String host = server.hostHeader();
  if (isIp(host) || host == String(myHostname) + ".local") return false;
  server.sendHeader("Location", "http://" + apIP.toString() + "/", true);
  server.send(302, "text/plain", "");
  return true;
}


static const char STYLE_CSS[] PROGMEM = R"rawliteral(
:root{--primary:#2980b9;--on:#1e8449;--danger:#c0392b;--dark:#2c3e50;--light:#ecf0f1;--line:#d5dbe1;--muted:#66747f}
*{box-sizing:border-box}
html{overflow-y:scroll}
body{font-family:'Segoe UI',Roboto,Tahoma,Verdana,sans-serif;line-height:1.5;color:var(--dark);background:#f5f7fa;max-width:800px;margin:0 auto;padding:12px 16px 24px}
h1{color:var(--primary);text-align:center;font-size:24px;margin:4px 0 12px}
h3{margin:0 0 12px}
p{margin:0 0 8px}
.tabs{display:flex;gap:4px;margin-bottom:16px}
.tabs a{flex:1;padding:10px 2px;text-align:center;background:#fff;color:var(--primary);text-decoration:none;border:1px solid var(--line);border-radius:6px;font-size:15px;white-space:nowrap}
.p-home a[href="/"],.p-switch a[href="/switch"],.p-toggle a[href="/toggle"],.p-label a[href="/label"],.p-wifi a[href="/wifi"]{background:var(--primary);border-color:var(--primary);color:#fff}
.card{background:#fff;border-radius:8px;padding:20px;margin-bottom:16px;box-shadow:0 1px 4px rgba(0,0,0,.12)}
.status{height:48px;line-height:48px;padding:0 12px;margin-bottom:16px;background:var(--light);border-radius:6px;text-align:center;font-size:18px;font-weight:600;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.status.err{background:var(--danger);color:#fff}
.grid{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:10px;margin-bottom:16px}
button{font:inherit;cursor:pointer;touch-action:manipulation}
.tile{display:block;width:100%;height:76px;padding:8px 10px;background:#fff;color:var(--dark);border:2px solid var(--line);border-radius:8px;text-align:center;transition:background-color .12s,border-color .12s,color .12s}
.tile b,.tile span{display:block;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.tile b{font-size:18px;min-height:27px}
.tile span{font-size:13px;color:var(--muted)}
.tile.on{background:var(--on);border-color:var(--on);color:#fff}
.tile.on span{color:#fff}
.wide{display:block;width:100%;padding:12px;background:var(--primary);color:#fff;border:0;border-radius:6px;font-size:16px}
.wide.danger{background:var(--danger)}
.wide:disabled{opacity:.45;cursor:default}
@media(hover:hover){.tile:hover{border-color:var(--primary)}.tile.on:hover{border-color:var(--dark)}.wide:hover:enabled{filter:brightness(.92)}.tabs a:hover{border-color:var(--primary)}}
:focus-visible{outline:3px solid #f39c12;outline-offset:1px}
table{width:100%;border-collapse:collapse;margin:0 0 12px}
th,td{padding:10px 12px;text-align:left;border-bottom:1px solid #ddd;word-break:break-all}
th{background:var(--primary);color:#fff}
tr.net{cursor:pointer}
@media(hover:hover){tr.net:hover{background:#f5f5f5}}
.nets{height:230px;overflow-y:auto;margin-bottom:12px}
.nets table{margin:0}
label{display:block;margin-bottom:12px}
input[type=text],input[type=password]{display:block;width:100%;padding:10px;margin-top:4px;border:1px solid #ccc;border-radius:4px;font-size:16px}
input[type=checkbox]{width:18px;height:18px;vertical-align:middle;margin:0 6px 0 0}
.lbl-grid{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:0 12px}
@media(max-width:600px){.grid,.lbl-grid{grid-template-columns:repeat(2,minmax(0,1fr))}}
.row2{display:grid;grid-template-columns:2fr 1fr;gap:10px}
.note{height:24px;margin:8px 0 0;text-align:center;font-size:14px;color:var(--on)}
.muted{color:var(--muted);font-size:14px}
)rawliteral";

static const char APP_JS[] PROGMEM = R"rawliteral(
function $(id){return document.getElementById(id)}
function each(sel,f){Array.prototype.forEach.call(document.querySelectorAll(sel),f)}
function fmtUp(s){return Math.floor(s/86400)+'d '+Math.floor(s/3600)%24+'h '+Math.floor(s/60)%60+'m '+s%60+'s'}
function chk(r){if(!r.ok)throw r.status;return r.json()}
function getJSON(u){return fetch(u,{cache:'no-store'}).then(chk)}
function post(u,b){return fetch(u,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b}).then(chk)}
function showConn(i){
  $('conn').textContent=i.viaAP?'\u{1F535} Connected via Soft AP: '+i.apSsid:'\u{1F7E2} Connected to WiFi network: '+i.staSsid;
  $('up').textContent=fmtUp(i.uptime);
  $('fw').textContent='Firmware '+i.fw+' · http://'+i.host+'.local';
}
)rawliteral";

// Common beginning of all pages: page class goes between PAGE_HEAD and PAGE_NAV.
// Version in URLs drops CSS/JS from the browser cache after a firmware update.
static const char PAGE_HEAD[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Remote Console</title>
  <link rel="stylesheet" href="/style.css?v=)rawliteral" __TIME__ R"rawliteral(">
  <script src="/app.js?v=)rawliteral" __TIME__ R"rawliteral("></script>
</head>
<body class=")rawliteral";

static const char PAGE_NAV[] PROGMEM = R"rawliteral(">
  <h1>Remote Console</h1>
  <nav class="tabs">
    <a href="/">Home</a>
    <a href="/switch">Switch</a>
    <a href="/toggle">Toggle</a>
    <a href="/label">Labels</a>
    <a href="/wifi">WiFi</a>
  </nav>
)rawliteral";

static const char PAGE_ROOT[] PROGMEM = R"rawliteral(
  <div class="status" id="status">&hellip;</div>
  <div class="card">
    <p id="conn">&hellip;</p>
    <p>Uptime: <strong id="up">&hellip;</strong></p>
    <p class="muted" id="fw">&nbsp;</p>
  </div>
<script>
function st(s){
  var t=s.labels.filter(function(l,i){return s.mask>>i&1});
  $('status').textContent=t.length?t.join(', '):'All OFF';
}
st(S);
function poll(){
  if(document.hidden)return;
  getJSON('/api/info').then(showConn);
  getJSON('/api/state').then(st);
}
poll();
setInterval(poll,5000);
</script>
</body>
</html>
)rawliteral";

// The same page for both modes: /switch - one antenna at a time, /toggle - independent
static const char PAGE_CHANNELS[] PROGMEM = R"rawliteral(
  <div class="status" id="status" role="status">&nbsp;</div>
  <div class="grid">
    <button class="tile" data-i="0"><b>&nbsp;</b><span>&nbsp;</span></button>
    <button class="tile" data-i="1"><b>&nbsp;</b><span>&nbsp;</span></button>
    <button class="tile" data-i="2"><b>&nbsp;</b><span>&nbsp;</span></button>
    <button class="tile" data-i="3"><b>&nbsp;</b><span>&nbsp;</span></button>
)rawliteral"
#if NUM_OUTPUTS == 8
R"rawliteral(
    <button class="tile" data-i="4"><b>&nbsp;</b><span>&nbsp;</span></button>
    <button class="tile" data-i="5"><b>&nbsp;</b><span>&nbsp;</span></button>
    <button class="tile" data-i="6"><b>&nbsp;</b><span>&nbsp;</span></button>
    <button class="tile" data-i="7"><b>&nbsp;</b><span>&nbsp;</span></button>
)rawliteral"
#endif
R"rawliteral(
  </div>
  <button class="wide danger" id="allOff">Turn all OFF</button>
<script>
var mask=S.mask,labels=S.labels,seq=0,pending=0,failed=false,queue=Promise.resolve();
var toggleMode=document.body.className=='p-toggle';
function paint(){
  var t=[];
  each('[data-i]',function(b){
    var i=+b.dataset.i,on=!!(mask>>i&1);
    b.querySelector('b').textContent=labels[i];
    b.querySelector('span').textContent=on?'ON':'OFF';
    b.classList.toggle('on',on);
    b.setAttribute('aria-pressed',on);
    if(on)t.push(labels[i]);
  });
  $('status').classList.toggle('err',failed);
  $('status').textContent=failed?'No connection to console':(t.length?t.join(', '):'All OFF');
  $('allOff').disabled=!mask;
}
function apply(s){failed=false;mask=s.mask;labels=s.labels;paint()}
function fail(){failed=true;paint()}
// The button reacts at once, commands go to the console strictly in click order,
// and only the answer to the last click is shown
function send(next,body){
  var my=++seq;
  pending++;
  mask=next;
  failed=false;
  paint();
  queue=queue.then(function(){return post('/set',body)}).then(
    function(s){pending--;if(my==seq)apply(s)},
    function(){pending--;if(my==seq)fail()});
}
function poll(){
  if(document.hidden||pending)return;
  var my=seq;
  getJSON('/api/state').then(
    function(s){if(my==seq&&!pending)apply(s)},
    function(){if(my==seq&&!pending)fail()});
}
each('[data-i]',function(b){
  var i=+b.dataset.i,bit=1<<i;
  b.onclick=function(){
    if(toggleMode)send(mask^bit,'ch='+i+'&on='+(mask&bit?0:1));
    else if(mask==bit)send(0,'off=1');
    else send(bit,'sel='+i);
  };
});
$('allOff').onclick=function(){send(0,'off=1')};
paint();
setInterval(poll,2000);
document.addEventListener('visibilitychange',poll);
</script>
</body>
</html>
)rawliteral";

static const char PAGE_LABEL[] PROGMEM = R"rawliteral(
  <form class="card" id="f">
    <p class="muted">Names of the antennas, up to 31 bytes (15 Cyrillic letters). Empty field - default name.</p>
    <div class="lbl-grid" id="fields"></div>
    <div class="row2">
      <button class="wide" type="submit">Save</button>
      <button class="wide danger" type="button" id="rst">Reset</button>
    </div>
    <p class="note" id="note"></p>
  </form>
<script>
S.labels.forEach(function(v,i){
  var l=document.createElement('label'),inp=document.createElement('input');
  l.textContent='Antenna '+(i+1);inp.type='text';inp.name='l'+i;inp.id='l'+i;inp.maxLength=31;inp.value=v;
  l.appendChild(inp);$('fields').appendChild(l);
});
function save(body){
  $('note').textContent='';
  post('/labelsave',body).then(function(s){
    s.labels.forEach(function(v,i){$('l'+i).value=v});
    $('note').textContent='Saved';
  },function(){$('note').textContent='Not saved: no connection to console'});
}
$('f').onsubmit=function(e){e.preventDefault();save(new URLSearchParams(new FormData(this)).toString())};
$('rst').onclick=function(){save('reset=1')};
</script>
</body>
</html>
)rawliteral";

// Page begins with the current state (var S), so it is drawn correctly at once
void sendPage(const char *pageClass, PGM_P body) {
  sendNoCache();
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html; charset=utf-8", "");
  server.sendContent_P(PAGE_HEAD);
  server.sendContent(pageClass);
  server.sendContent_P(PAGE_NAV);
  server.sendContent("<script>var S=" + stateJson() + ";</script>");
  server.sendContent_P(body);
  server.sendContent("");
}

void handleRoot() {
  if (captivePortal()) return;
  if (!authorized()) return;
  sendPage("p-home", PAGE_ROOT);
}

void handleSwitch() {
  if (!authorized()) return;
  sendPage("p-switch", PAGE_CHANNELS);
}

void handleToggle() {
  if (!authorized()) return;
  sendPage("p-toggle", PAGE_CHANNELS);
}

void handleLabel() {
  if (!authorized()) return;
  sendPage("p-label", PAGE_LABEL);
}

void handleStatic(PGM_P content, const char *type) {
  server.sendHeader("Cache-Control", "max-age=86400");
  server.send_P(200, type, content);
}

String stateJson() {
  String j;
  j.reserve(160);
  j = F("{\"mask\":");
  j += outputs;
  j += F(",\"labels\":[");
  for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
    if (i) j += ',';
    j += '"';
    j += jsonEscape(labels[i]);
    j += '"';
  }
  j += F("]}");
  return j;
}

void handleState() {
  if (!authorized()) return;
  sendJson(stateJson());
}

// Channel number from a request argument, -1 if there is no such channel
int channelArg(const char *name) {
  String v = server.arg(name);
  return (v.length() == 1 && v[0] >= '0' && v[0] < '0' + NUM_OUTPUTS) ? v[0] - '0' : -1;
}

// Validate all arguments first, then apply
void handleSet() {
  if (!authorized()) return;
  uint8_t out = outputs;
  if (server.hasArg("off")) out = 0;
  if (server.hasArg("sel")) {
    int ch = channelArg("sel");
    if (ch < 0) {
      server.send(400, "text/plain", "Bad sel");
      return;
    }
    out = 1 << ch;
  }
  if (server.hasArg("ch")) {
    int ch = channelArg("ch");
    String on = server.arg("on");
    if (ch < 0 || (on != "0" && on != "1")) {
      server.send(400, "text/plain", "Bad ch or on");
      return;
    }
    if (on == "1") out |= 1 << ch;
    else out &= ~(1 << ch);
  }
  outputs = out;
  applyOutputs();
  sendJson(stateJson());
}

void setLabel(uint8_t i, const String &v) {
  const size_t cap = sizeof(labels[i]) - 1;
  size_t n = v.length();
  if (n > cap) {
    n = cap;
    // do not cut UTF-8 character in the middle
    while (n > 0 && ((uint8_t)v[n] & 0xC0) == 0x80) n--;
  }
  if (n == 0) {
    snprintf(labels[i], sizeof(labels[i]), "A%d", i + 1);
    return;
  }
  memcpy(labels[i], v.c_str(), n);
  labels[i][n] = 0;
}

void handleLabelSave() {
  if (!authorized()) return;
  bool reset = server.hasArg("reset");
  for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
    String v = reset ? String() : server.arg("l" + String(i));
    v.trim();
    setLabel(i, v);
  }
  saveLabels();
  sendJson(stateJson());
}

/* ---------------------------------------------------------------------------
 * Persistent settings storage.
 * Uses a flash sector computed from the smaller of the REAL and the CONFIGURED
 * flash size, so saving works with any mix of them. The stock EEPROM library
 * selects a sector from the CONFIGURED size (e.g. 4M), which on a physically 1M
 * chip lies beyond the memory; the SDK in turn refuses to access anything
 * beyond the CONFIGURED size (1M build on a physically 4M chip).
 * Last 4 sectors of the flash belong to the SDK (RF calibration and WiFi
 * parameters), the 5th from the end is the sector reserved for EEPROM.
 * ------------------------------------------------------------------------- */
#define CFG_SEC_SIZE 4096

// Flash layout is the same for all board variants: always 8 labels
struct Settings {
  char magic[4];
  char ssid[33];
  char password[65];
  char labels[8][32];
  uint8_t apOff;     // 1 - disable SoftAP while connected to WLAN
  uint8_t reserved;  // size must stay a multiple of 4 for the flash API
};
static_assert(sizeof(Settings) % 4 == 0, "flash API writes whole 32-bit words");
static Settings cfgStore;

// Settings of the previous firmware: other layout, stored in sector N-2
struct SettingsV1 {
  char magic[4];
  char ssid[32];
  char password[32];
  char labels[8][32];
};

// union guarantees 4-byte alignment required by the flash API
static union {
  uint32_t w[(sizeof(Settings) + 3) / 4];
  uint8_t  b[sizeof(Settings)];
} cfgBuf;

static uint32_t cfgSectors() {
  return min(ESP.getFlashChipRealSize(), ESP.getFlashChipSize()) / CFG_SEC_SIZE;
}

static uint32_t cfgSector() {
  return cfgSectors() - 5;
}

static bool cfgSave() {
  memcpy(cfgStore.magic, "RKC2", 4);
  memcpy(cfgBuf.b, &cfgStore, sizeof(Settings));
  if (!ESP.flashEraseSector(cfgSector())) return false;
  return ESP.flashWrite(cfgSector() * CFG_SEC_SIZE, cfgBuf.w, sizeof(Settings));
}

static void cfgLoad() {
  memset(&cfgStore, 0, sizeof(cfgStore));
  memset(cfgBuf.b, 0, sizeof(cfgBuf.b));
  ESP.flashRead(cfgSector() * CFG_SEC_SIZE, cfgBuf.w, sizeof(Settings));
  memcpy(&cfgStore, cfgBuf.b, sizeof(Settings));
  if (memcmp(cfgStore.magic, "RKC2", 4) != 0) memset(&cfgStore, 0, sizeof(cfgStore));
}

/* Settings of the previous firmware were stored in sector N-2, which is one of
 * the two SDK WiFi parameter banks; the SDK treats them as its own and copies
 * them to the other bank (N-3). Running with this data as WiFi parameters gives
 * Exception 9 in ieee80211_getmgtframe (excvaddr 0x0000000f) on STA retries.
 * Move the settings to the new sector, wipe the whole SDK area and reboot,
 * because the SDK has already loaded the bad parameters to RAM. */
static void cfgMigrate() {
  union {
    uint32_t w[(sizeof(SettingsV1) + 3) / 4];
    SettingsV1 s;
  } old;
  uint32_t sectors = cfgSectors();
  bool found = false;
  cfgLoad();
  bool saved = memcmp(cfgStore.magic, "RKC2", 4) == 0;
  for (uint32_t sector = sectors - 2; sector >= sectors - 3; sector--) {
    if (!ESP.flashRead(sector * CFG_SEC_SIZE, old.w, sizeof(SettingsV1))) continue;
    if (memcmp(old.s.magic, "RKC1", 4) != 0) continue;
    found = true;
    if (saved) continue;
    memcpy(cfgStore.ssid, old.s.ssid, sizeof(old.s.ssid));
    memcpy(cfgStore.password, old.s.password, sizeof(old.s.password));
    memcpy(cfgStore.labels, old.s.labels, sizeof(old.s.labels));
    saved = cfgSave();
  }
  if (!found || !saved) return;
  Serial.println("Settings of old firmware moved, SDK sectors wiped, restart");
  for (uint32_t sector = sectors - 4; sector < sectors; sector++) ESP.flashEraseSector(sector);
  ESP.restart();
}

void loadLabels() {
  cfgLoad();
  Serial.println("Labels:");
  for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
    cfgStore.labels[i][sizeof(labels[i]) - 1] = 0;
    setLabel(i, cfgStore.labels[i]);
    Serial.println(labels[i]);
  }
}

/** Store labels to flash. */
void saveLabels() {
  cfgLoad(); // preserve ssid/password fields
  memcpy(cfgStore.labels, labels, sizeof(labels));
  cfgSave();
}

static const char PAGE_WIFI[] PROGMEM = R"rawliteral(
  <div class="card">
    <p id="conn">&hellip;</p>
    <p>Uptime: <strong id="up">&hellip;</strong></p>
    <p class="muted" id="fw">&nbsp;</p>
  </div>

  <div class="card">
    <h3>SoftAP Configuration</h3>
    <table>
      <tr><th>Parameter</th><th>Value</th></tr>
      <tr><td>SSID</td><td id="apSsid"></td></tr>
      <tr><td>Status</td><td id="apState"></td></tr>
      <tr><td>IP Address</td><td id="apIp"></td></tr>
    </table>
    <form method="POST" action="/apsave">
      <label><input type="checkbox" name="apoff" value="1" id="apoff"> Disable access point while connected to WLAN</label>
      <p class="muted">The access point turns on automatically if the saved network is unavailable for 15 s,
        and always after power-on until the WLAN connection is established.
        Without the access point the console is reachable only via the WLAN IP address or http://rkconsole.local</p>
      <button class="wide" type="submit">Save</button>
    </form>
  </div>

  <div class="card">
    <h3>WLAN Configuration</h3>
    <table>
      <tr><th>Parameter</th><th>Value</th></tr>
      <tr><td>SSID</td><td id="staSsid"></td></tr>
      <tr><td>Status</td><td id="staState"></td></tr>
      <tr><td>IP Address</td><td id="staIp"></td></tr>
      <tr><td>Signal</td><td id="staRssi"></td></tr>
    </table>
  </div>

  <div class="card">
    <h3>Available Networks</h3>
    <p class="muted">Click a network to select it</p>
    <div class="nets"><table id="nets"><tr><th>Network Name</th><th>Signal</th></tr></table></div>
    <button class="wide" id="scanBtn" type="button">Scan</button>
  </div>

  <form class="card" method="POST" action="/wifisave">
    <h3>Connect to Network</h3>
    <label>Network SSID:
      <input type="text" id="n" name="n" maxlength="32" placeholder="Leave empty to forget network"></label>
    <label>Password:
      <input type="password" id="p" name="p" minlength="8" maxlength="64" placeholder="Enter password"></label>
    <button class="wide" type="submit">Connect</button>
  </form>

<script>
var apoffTouched=false;
$('apoff').onchange=function(){apoffTouched=true};
function info(){
  getJSON('/api/info').then(function(i){
    showConn(i);
    $('apSsid').textContent=i.apSsid;
    $('apState').textContent=i.apActive?'On':'Off';
    $('apIp').textContent=i.apActive?i.apIp:'—';
    if(!apoffTouched)$('apoff').checked=i.apOff;
    $('staSsid').textContent=i.staSsid||'(not configured)';
    $('staState').textContent=i.connected?'Connected '+fmtUp(i.wlanUptime):(i.staSsid?'Not connected':'—');
    $('staIp').textContent=i.connected?i.staIp:'—';
    $('staRssi').textContent=i.connected?i.rssi+' dBm':'—';
  });
}
function row(a,b,cls){
  var tr=document.createElement('tr'),td1=document.createElement('td'),td2=document.createElement('td');
  td1.textContent=a;td2.textContent=b;tr.appendChild(td1);tr.appendChild(td2);
  if(cls)tr.className=cls;
  return tr;
}
function clearNets(){var t=$('nets');while(t.rows.length>1)t.deleteRow(1)}
function scan(){
  $('scanBtn').disabled=true;$('scanBtn').textContent='Scanning…';
  getJSON('/api/scan').then(function(r){
    if(r.scanning){setTimeout(scan,1000);return}
    var best={};
    r.nets.forEach(function(n){if(n.ssid&&(!best[n.ssid]||best[n.ssid].rssi<n.rssi))best[n.ssid]=n});
    var list=Object.keys(best).map(function(k){return best[k]}).sort(function(a,b){return b.rssi-a.rssi});
    clearNets();
    if(!list.length)$('nets').appendChild(row('No WiFi networks found',''));
    list.forEach(function(n){
      var tr=row(n.ssid+(n.secure?' (secured)':''),n.rssi+' dBm','net');
      tr.onclick=function(){$('n').value=n.ssid;$('p').focus()};
      $('nets').appendChild(tr);
    });
    $('scanBtn').disabled=false;$('scanBtn').textContent='Scan';
  },function(){$('scanBtn').disabled=false;$('scanBtn').textContent='Scan'});
}
$('scanBtn').onclick=scan;
info();
setInterval(info,5000);
scan();
</script>
</body>
</html>
)rawliteral";

void handleWifi() {
  if (!authorized()) return;
  sendPage("p-wifi", PAGE_WIFI);
}

String jsonEscape(const char *s) {
  String o;
  for (; *s; s++) {
    char c = *s;
    if (c == '"' || c == '\\') {
      o += '\\';
      o += c;
    } else if ((uint8_t)c < 0x20 || c == '<') {
      char b[7];
      snprintf(b, sizeof(b), "\\u%04x", c);
      o += b;
    } else {
      o += c;
    }
  }
  return o;
}

void addJsonStr(String &j, const char *key, const char *val) {
  j += '"';
  j += key;
  j += F("\":\"");
  j += jsonEscape(val);
  j += F("\",");
}

void sendJson(const String &j) {
  sendNoCache();
  server.send(200, "application/json; charset=utf-8", j);
}

void handleInfo() {
  if (!authorized()) return;
  bool sta = WiFi.status() == WL_CONNECTED;
  String j;
  j.reserve(320);
  j = F("{\"viaAP\":");
  j += server.client().localIP() == apIP ? F("true,") : F("false,");
  addJsonStr(j, "apSsid", apSsid);
  addJsonStr(j, "apIp", WiFi.softAPIP().toString().c_str());
  addJsonStr(j, "staSsid", ssid);
  addJsonStr(j, "staIp", WiFi.localIP().toString().c_str());
  addJsonStr(j, "host", myHostname);
  addJsonStr(j, "fw", FW_VERSION);
  j += F("\"apActive\":");
  j += apActive ? F("true,") : F("false,");
  j += F("\"apOff\":");
  j += apOff ? F("true,") : F("false,");
  j += F("\"connected\":");
  j += sta ? F("true") : F("false");
  j += F(",\"rssi\":");
  j += sta ? WiFi.RSSI() : 0;
  j += F(",\"uptime\":");
  j += uptimeSec;
  j += F(",\"wlanUptime\":");
  j += sta ? uptimeSec - staConnectedAt : 0;
  j += F(",\"heap\":");
  j += ESP.getFreeHeap();
  j += '}';
  sendJson(j);
}

// Asynchronous scan: first call starts it, client polls until results are ready
void handleScan() {
  if (!authorized()) return;
  int n = WiFi.scanComplete();
  // Scan aborts STA connection attempt - postpone it, the page polls again
  if (n == WIFI_SCAN_FAILED && connecting) n = WIFI_SCAN_RUNNING;
  if (n == WIFI_SCAN_FAILED) {
    WiFi.scanNetworks(true);
    n = WIFI_SCAN_RUNNING;
  }
  if (n == WIFI_SCAN_RUNNING) {
    sendJson(F("{\"scanning\":true}"));
    return;
  }
  String j = F("{\"scanning\":false,\"nets\":[");
  for (int i = 0; i < n; i++) {
    if (i) j += ',';
    j += F("{\"ssid\":\"");
    j += jsonEscape(WiFi.SSID(i).c_str());
    j += F("\",\"rssi\":");
    j += WiFi.RSSI(i);
    j += F(",\"secure\":");
    j += WiFi.encryptionType(i) != ENC_TYPE_NONE ? F("true") : F("false");
    j += '}';
  }
  j += F("]}");
  updateScanCache(n);
  WiFi.scanDelete();
  sendJson(j);
}

void updateScanCache(int n) {
  scanCacheCount = 0;
  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    if (!s.length() || s.length() > 32) continue;
    // keep the strongest access point for each SSID
    ScanNet *e = nullptr;
    for (uint8_t k = 0; k < scanCacheCount; k++)
      if (s == scanCache[k].ssid) e = &scanCache[k];
    if (e && e->rssi >= WiFi.RSSI(i)) continue;
    if (!e) {
      if (scanCacheCount >= SCAN_CACHE_SIZE) continue;
      e = &scanCache[scanCacheCount++];
    }
    strlcpy(e->ssid, s.c_str(), sizeof(e->ssid));
    memcpy(e->bssid, WiFi.BSSID(i), 6);
    e->channel = WiFi.channel(i);
    e->rssi = WiFi.RSSI(i);
  }
}


/* Captive-portal connectivity-check endpoints (/generate_204, /fwlink).
 * These are probed constantly and WITHOUT credentials by Windows (NCSI),
 * Android and browsers the moment any new device (e.g. a laptop) joins the
 * network. They must NOT go through authorized(): a 401 Digest challenge is
 * never "authenticated" by the OS probe, so it retries aggressively and
 * forever, generating a storm of 401 responses (each one allocating Strings
 * and Digest nonces on a device with ~40KB of heap). Replying with a bare
 * 204 tells the OS "internet is fine" and the probes stop. */
void handleCaptiveProbe() {
  if (captivePortal()) return;
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.send(204);
}

void handleNotFound() {
  if (captivePortal()) return;
  if (!authorized()) return;
  server.sendHeader("Connection", "close");
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.send(404, "text/plain", message);
}

void handleWifiSave() {
  Serial.println("wifi save");
  if (!authorized()) return;
  String newSsid = server.arg("n");
  String newPass = server.arg("p");
  newSsid.trim();
  // A WPA passphrase shorter than 8 characters is invalid; feeding it to
  // WiFi.begin() is a known trigger of the SDK's association crash
  // (Exception 9 in ieee80211_getmgtframe, excvaddr 0x0000000f).
  if (newSsid.length() > 32 || newPass.length() > 64 ||
      (newPass.length() > 0 && newPass.length() < 8)) {
    server.send(400, "text/plain", "SSID max 32, password 8-64 characters");
    return;
  }
  strlcpy(ssid, newSsid.c_str(), sizeof(ssid));
  strlcpy(password, newPass.c_str(), sizeof(password));
  saveCredentials();
  redirect("/wifi");
  WiFi.disconnect();
  connecting = false;
  connectHint = nullptr;
  for (uint8_t k = 0; k < scanCacheCount; k++)
    if (strcmp(scanCache[k].ssid, ssid) == 0) connectHint = &scanCache[k];
  connectRequested = ssid[0] != 0;
}

void handleApSave() {
  if (!authorized()) return;
  apOff = server.arg("apoff") == "1";
  saveCredentials();
  redirect("/wifi");
}

void loadCredentials() {
  cfgLoad();
  memcpy(ssid, cfgStore.ssid, sizeof(ssid));
  memcpy(password, cfgStore.password, sizeof(password));
  ssid[sizeof(ssid) - 1] = 0;
  password[sizeof(password) - 1] = 0;
  apOff = cfgStore.apOff == 1;
  Serial.println("Recovered credentials:");
  Serial.println(ssid);
  Serial.println(strlen(password) > 0 ? "********" : "<no password>");
}

/** Store WLAN credentials and SoftAP setting to flash. */
void saveCredentials() {
  cfgLoad(); // preserve labels fields
  memcpy(cfgStore.ssid, ssid, sizeof(ssid));
  memcpy(cfgStore.password, password, sizeof(password));
  cfgStore.apOff = apOff ? 1 : 0;
  cfgSave();
}

// Break before make: first release everything that has to go off
void applyOutputs() {
  for (uint8_t i = 0; i < NUM_OUTPUTS; i++)
    if (!(outputs >> i & 1)) digitalWrite(relayPins[i], LOW);
  for (uint8_t i = 0; i < NUM_OUTPUTS; i++)
    if (outputs >> i & 1) digitalWrite(relayPins[i], HIGH);
}

void setup(void){
  delay(100);
  for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW);
  }
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  Serial.begin(115200);
  Serial.println();
  Serial.print("Reset reason: ");
  Serial.println(ESP.getResetReason());
  cfgMigrate();
  loadCredentials(); // Load WLAN credentials from flash
  loadLabels();

  snprintf(apSsid, sizeof(apSsid), "RKconsole_%06X", ESP.getChipId());

  Serial.print("Configuring access point...");
  /* Credentials are stored in our own flash sector (cfgStore); do not let
   * the SDK also write its config sector on every WiFi.begin() - extra
   * flash erases right at connection time are pointless (and stall the CPU
   * while the SDK is trying to associate). */
  WiFi.persistent(false);
  /* WiFi modem-sleep (default in STA) is a proven source of sys-context
   * lwIP corruption on ESP8266: Exception 28 in pbuf_free_LWIP2 with a
   * garbage pointer, exactly while a burst of incoming packets (device
   * joining the network) races with the web server sending a response.
   * The PM nullfunc frames / bb on-off switching showed up in both crash
   * stacks. This device is mains-powered - disable power save entirely. */
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  // SDK retries transient failures (handshake timeout etc.) within an attempt,
  // otherwise status hangs in WL_DISCONNECTED. Hard failures and timeout are handled in wifiTask()
  WiFi.setAutoReconnect(true);
  staDisconnectedHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected &e) {
    lastDisconnectReason = e.reason;
  });
  WiFi.mode(WIFI_AP_STA);
  WiFi.hostname(myHostname);
  startAP();   // always on after power-on, apTask() may switch it off later
  delay(500); // Without delay I've seen the IP address blank
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  if (MDNS.begin(myHostname)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("mDNS: http://%s.local\n", myHostname);
  }

  server.on("/", handleRoot);
  server.on("/wifi", handleWifi);
  server.on("/label", handleLabel);
  server.on("/labelsave", HTTP_POST, handleLabelSave);
  server.on("/switch", handleSwitch);
  server.on("/toggle", handleToggle);
  server.on("/wifisave", HTTP_POST, handleWifiSave);
  server.on("/apsave", HTTP_POST, handleApSave);
  server.on("/set", HTTP_POST, handleSet);
  server.on("/style.css", HTTP_GET, []() { handleStatic(STYLE_CSS, "text/css"); });
  server.on("/app.js", HTTP_GET, []() { handleStatic(APP_JS, "application/javascript"); });
  server.on("/api/state", HTTP_GET, handleState);
  server.on("/api/info", HTTP_GET, handleInfo);
  server.on("/api/scan", HTTP_GET, handleScan);
  server.on("/generate_204", handleCaptiveProbe);
  server.on("/fwlink", handleCaptiveProbe);
  server.onNotFound(handleNotFound);
  server.begin(); // Web server start
  Serial.println("HTTP server started");
  connectRequested = ssid[0] != 0; // Request WLAN connect if there is a SSID
  
}

String macToString(const uint8_t *m) {
  char b[18];
  snprintf(b, sizeof(b), "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
  return b;
}

void connectWifi() {
  Serial.print("Connecting as wifi client to ");
  Serial.println(ssid);
  WiFi.disconnect();
  // non-blocking, result is checked in wifiTask()
  if (connectHint) {
    Serial.printf("Channel %d, BSSID %s\n", connectHint->channel, macToString(connectHint->bssid).c_str());
    WiFi.begin(ssid, password, connectHint->channel, connectHint->bssid);
    connectHint = nullptr;
  } else {
    WiFi.begin(ssid, password);
  }
  lastConnectTry = millis();
  connecting = true;
}

void connectFinished(bool failed, unsigned long retryIn) {
  connecting = false;
  nextConnectAt = millis() + retryIn;
  if (failed) {
    WiFi.disconnect();   // stop STA channel scanning, it makes SoftAP unstable
    Serial.printf("Connection failed, retry in %lu s\n", retryIn / 1000);
  }
}

void wifiTask() {
  bool haveSsid = ssid[0] != 0;
  bool scanBusy = WiFi.scanComplete() == WIFI_SCAN_RUNNING;   // begin() during scan fails
  if (connectRequested && !scanBusy) {
    connectRequested = false;
    if (haveSsid) connectWifi();
  }
  int reason = lastDisconnectReason;
  if (reason >= 0) {
    lastDisconnectReason = -1;
    Serial.printf("WiFi disconnect reason: %d\n", reason);
  }
  wl_status_t s = WiFi.status();
  if ((s == WL_CONNECTED) != staUp) {
    staUp = s == WL_CONNECTED;
    staChangedAt = millis();
  }
  if (s != wifiStatus) {   // WLAN status change
    Serial.print("Status: ");
    Serial.println(s);
    wifiStatus = s;
    if (s == WL_CONNECTED) {
      Serial.print("Connected to ");
      Serial.println(ssid);
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      digitalWrite(ledPin, HIGH);
      staConnectedAt = uptimeSec;
    } else {
      digitalWrite(ledPin, LOW);
    }
  }
  if (connecting) {
    if (s == WL_CONNECTED) {
      connecting = false;
    } else if (s == WL_NO_SSID_AVAIL || s == WL_CONNECT_FAILED || s == WL_WRONG_PASSWORD) {
      connectFinished(true, RECONNECT_INTERVAL);
    } else if (s == WL_IDLE_STATUS && millis() - lastConnectTry > 500) {
      Serial.println("Connection attempt aborted");
      connectFinished(false, RETRY_ABORTED);
    } else if (millis() - lastConnectTry > CONNECT_TIMEOUT) {
      connectFinished(true, RECONNECT_INTERVAL);
    }
  } else if (haveSsid && !scanBusy && s != WL_CONNECTED && (long)(millis() - nextConnectAt) >= 0) {
    connectWifi();
  }
}

void startAP() {
  if (apActive) return;
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP(apSsid, softAP_password);
  /* Setup the DNS server redirecting all the domains to the apIP */
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", apIP);
  apActive = true;
  Serial.print("SoftAP on: ");
  Serial.println(apSsid);
}

void stopAP() {
  if (!apActive) return;
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  apActive = false;
  Serial.println("SoftAP off");
}

void apTask() {
  unsigned long since = millis() - staChangedAt;
  if (apOff && ssid[0]) {
    if (staUp && since > AP_OFF_DELAY) stopAP();
    else if (!staUp && since > AP_ON_DELAY) startAP();
  } else {
    startAP();
  }
}

void uptimeTask() {
  static unsigned long last = 0;
  while (millis() - last >= 1000) {
    last += 1000;
    uptimeSec++;
  }
}

void loop(void){
  uptimeTask();
  wifiTask();
  apTask();
  if (apActive) dnsServer.processNextRequest();
  MDNS.update();
  server.handleClient();
  delay(1);
}
