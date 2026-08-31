
/*
Remote wifi control console
Irkutsk, 2019
ra0sms@bk.ru

June 2025 - Add CSS, improve visability, add authentification
*/


#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>


const bool AUTH = true;

const char* www_username = "admin";
const char* www_password = "password123";
 

const char *softAP_ssid = "RKconsole_sn322";
const char *softAP_password = "1234567890";
const char *myHostname = "esp8266";
 
char ssid[32] = "";
char password[32] = "";
char label0[32] ="";
char label1[32] ="";
char label2[32] ="";
char label3[32] ="";
char label4[32] ="";
char label5[32] ="";
char label6[32] ="";
char label7[32] ="";
String currentlabel ="";
String javaScript, XML;

/* Static HTML chunks served from flash (PROGMEM) so we never build a large
   page in RAM - this removes the main source of heap fragmentation. */
static const char P_TOGGLE_HEAD[] PROGMEM = R"(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Remote Toggle Control</title>
  <style>
    :root {
      --primary: #3498db;
      --secondary: #2ecc71;
      --danger: #e74c3c;
      --dark: #2c3e50;
      --light: #ecf0f1;
    }
    
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      background-color: #f5f7fa;
      margin: 0;
      padding: 20px;
      color: var(--dark);
    }
    
    .container {
      max-width: 800px;
      margin: 0 auto;
      background: white;
      border-radius: 10px;
      padding: 25px;
      box-shadow: 0 4px 12px rgba(0,0,0,0.1);
    }
    
    h1 {
      color: var(--primary);
      text-align: center;
      margin-bottom: 30px;
    }
    
    .toggle-grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(250px, 1fr));
      gap: 15px;
      margin-bottom: 30px;
    }
    
    .toggle-item {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 10px;
      padding: 12px;
      background: white;
      border-radius: 8px;
      box-shadow: 0 2px 5px rgba(0,0,0,0.1);
    }
    
    .toggle-label {
      flex-grow: 1;
      font-weight: 500;
    }
    
    .toggle-label.active {
      color: var(--danger);
      font-weight: bold;
    }
    
    .btn {
      border: none;
      color: white;
      padding: 8px 16px;
      text-align: center;
      text-decoration: none;
      display: inline-block;
      font-size: 14px;
      font-weight: 500;
      border-radius: 6px;
      cursor: pointer;
      transition: all 0.3s ease;
      min-width: 80px;
    }
    
    .btn-on {
      background-color: var(--secondary);
    }
    
    .btn-off {
      background-color: var(--danger);
    }
    
    .btn:hover {
      opacity: 0.9;
      transform: translateY(-2px);
    }
    
    .status-display {
      font-family: 'Lucida Console', monospace;
      font-size: 1.2em;
      margin: 20px 0;
      padding: 15px;
      background: var(--light);
      border-radius: 5px;
      text-align: center;
    }
    
    .footer {
      text-align: center;
      margin-top: 30px;
    }
    
    .footer a {
      color: var(--primary);
      text-decoration: none;
    }
    
    .footer a:hover {
      text-decoration: underline;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>Toggle Switch Control</h1>
    
    <div class="status-display">
      <span id="runtime"></span>
    </div>
    
    <div class="toggle-grid">
)";

static const char P_TOGGLE_FOOT[] PROGMEM = R"(
    </div>
    
    <div class="footer">
      <a href="/">← Return to Home Page</a>
    </div>
  </div>
)";

static const char P_CLOSE[] PROGMEM = R"(
</body>
</html>
)";

static const char P_SWR_HEAD[] PROGMEM = R"(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>WI-FI Remote Console</title>
  <style>
    :root {
      --primary: #3498db;
      --secondary: #2ecc71;
      --danger: #e74c3c;
      --dark: #2c3e50;
      --light: #ecf0f1;
    }
    
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      background-color: #f5f7fa;
      margin: 0;
      padding: 20px;
      color: var(--dark);
    }
    
    .container {
      max-width: 800px;
      margin: 0 auto;
      background: white;
      border-radius: 10px;
      padding: 25px;
      box-shadow: 0 4px 12px rgba(0,0,0,0.1);
    }
    
    h1 {
      color: var(--primary);
      text-align: center;
      margin-bottom: 30px;
    }
    
    .toggle-grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(250px, 1fr));
      gap: 15px;
      margin-bottom: 30px;
    }
    
    .toggle-item {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 10px;
      padding: 12px;
      background: white;
      border-radius: 8px;
      box-shadow: 0 2px 5px rgba(0,0,0,0.1);
    }
    
    .toggle-label {
      flex-grow: 1;
      font-weight: 500;
    }
    
    .btn {
      border: none;
      color: white;
      padding: 8px 16px;
      text-align: center;
      text-decoration: none;
      display: inline-block;
      font-size: 14px;
      font-weight: 500;
      border-radius: 6px;
      cursor: pointer;
      transition: all 0.3s ease;
      min-width: 80px;
    }
    
    .btn-on {
      background-color: var(--secondary);
    }
    
    .btn-off {
      background-color: var(--danger);
    }
    
    .btn:hover {
      opacity: 0.9;
      transform: translateY(-2px);
    }
    
    .status-display {
      font-family: 'Lucida Console', monospace;
      font-size: 1.2em;
      margin: 20px 0;
      padding: 15px;
      background: var(--light);
      border-radius: 5px;
      text-align: center;
    }
    
    .footer {
      text-align: center;
      margin-top: 30px;
    }
    
    .footer a {
      color: var(--primary);
      text-decoration: none;
    }
    
    .footer a:hover {
      text-decoration: underline;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>WI-FI Remote Console</h1>
    
    <div class="status-display">
      <span id="runtime"></span>
    </div>
    
    <div class="toggle-grid">
)";

static const char P_SWR_SCRIPT[] PROGMEM = R"(
  <script>
  function updateButtons() {
    fetch('/xml')
      .then(response => response.text())
      .then(str => (new DOMParser()).parseFromString(str, "text/xml"))
      .then(xml => {
        const statusText = xml.querySelector("response").textContent;
        document.getElementById('runtime').textContent = statusText;
        
        const statuses = statusText.split(' ');
        
        for (let i = 0; i < 8; i++) {
          const btn = document.getElementById(`btn${i}`);
          if (btn) {
            const isActive = statusText.includes(btn.dataset.label);
            btn.textContent = isActive ? "OFF" : "ON";
            btn.className = isActive ? "btn btn-off" : "btn btn-on";
            btn.href = isActive ? `socket${i}Off` : `socket${i}On`;
          }
        }
      });
  }
  
  setInterval(updateButtons, 1000);
  updateButtons(); // Вызываем сразу при загрузке
  </script>
)";

static const char P_SWR_FOOT[] PROGMEM = R"(
    </div>
    
    <div class="footer">
      <a href="/">← Return to Home Page</a>
    </div>
  </div>
</body>
</html>
)";
 
int gpio0_pin = 16;
int gpio1_pin = 5;
int gpio2_pin = 4;
int gpio3_pin = 0;
int gpio4_pin = 2;
int gpio5_pin = 14;
int gpio6_pin = 12;
int gpio7_pin = 13;
int gpio8_pin = 15;
char stat[8] =  "";
int tim = 0;
int sec = 0;
int minute = 0;
int hour = 0;
int day = 0;
int flagAP=0;
int flag_off=0;


const byte DNS_PORT = 53;
DNSServer dnsServer;

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
boolean connect;
unsigned long lastConnectTry = 0;
unsigned int status = WL_IDLE_STATUS;
// Counts consecutive failed connect attempts since the last success (or
// credential change). Drives the retry backoff and picks whether a retry
// does a full WiFi.disconnect()+mode reset (only the first attempt) or a
// plain WiFi.begin() (subsequent retries) - see startWifiConnect().
unsigned int wifiRetryCount = 0;


void CheckStat() {
  currentlabel = "";
  if (stat[0] == '1') currentlabel += String(label0) + " ";
  if (stat[1] == '1') currentlabel += String(label1) + " ";
  if (stat[2] == '1') currentlabel += String(label2) + " ";
  if (stat[3] == '1') currentlabel += String(label3) + " ";
  if (stat[4] == '1') currentlabel += String(label4) + " ";
  if (stat[5] == '1') currentlabel += String(label5) + " ";
  if (stat[6] == '1') currentlabel += String(label6) + " ";
  if (stat[7] == '1') currentlabel += String(label7);
}

void handleXML() {
  if (!authorized()) return;
  // No "Connection: close" here on purpose: this endpoint is polled every
  // second by the /switch and /toggle pages for as long as they stay open,
  // so keeping the connection alive (default HTTP/1.1 behavior) avoids
  // tearing down and re-establishing a TCP connection every second, which
  // was exhausting ESP8266's small lwIP/heap budget after ~30 min and
  // triggering a watchdog reset.
  buildXML();
  server.send(200, "text/xml", XML);
}

void buildJavascript() {
  javaScript = R"(
<script>
const updateStatus = () => {
  fetch('/xml', { method: 'PUT' })
    .then(response => response.text())
    .then(str => (new DOMParser()).parseFromString(str, "text/xml"))
    .then(xml => {
      const message = xml.querySelector("response").textContent;
      document.getElementById('runtime').textContent = message;
      
      // Update button states
      for (let i = 0; i < 8; i++) {
        const btn = document.getElementById(`toggleBtn${i}`);
        if (btn) {
          const isActive = message.includes(btn.dataset.label);
          btn.textContent = isActive ? "OFF" : "ON";
          btn.className = isActive ? "btn btn-off" : "btn btn-on";
          btn.href = isActive ? `soc${i}Off` : `soc${i}On`;
        }
      }
    })
    .catch(err => console.error('Error:', err));
  
  setTimeout(updateStatus, 1000);
};

document.addEventListener('DOMContentLoaded', updateStatus);
</script>
)";
}

void buildXML() {
  XML = "<?xml version='1.0'?>";
  XML += "<response>";
  XML += currentlabel;
  XML += "</response>";
}

void handleToggle() {
  if (!authorized()) return;
  server.sendHeader("Connection", "close");
  buildJavascript();

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");

  server.sendContent_P(PSTR(R"(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Remote Toggle Control</title>
  <style>
    :root {
      --primary: #3498db;
      --secondary: #2ecc71;
      --danger: #e74c3c;
      --dark: #2c3e50;
      --light: #ecf0f1;
    }
    
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      background-color: #f5f7fa;
      margin: 0;
      padding: 20px;
      color: var(--dark);
    }
    
    .container {
      max-width: 800px;
      margin: 0 auto;
      background: white;
      border-radius: 10px;
      padding: 25px;
      box-shadow: 0 4px 12px rgba(0,0,0,0.1);
    }
    
    h1 {
      color: var(--primary);
      text-align: center;
      margin-bottom: 30px;
    }
    
    .toggle-grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(250px, 1fr));
      gap: 15px;
      margin-bottom: 30px;
    }
    
    .toggle-item {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 10px;
      padding: 12px;
      background: white;
      border-radius: 8px;
      box-shadow: 0 2px 5px rgba(0,0,0,0.1);
    }
    
    .toggle-label {
      flex-grow: 1;
      font-weight: 500;
    }
    
    .toggle-label.active {
      color: var(--danger);
      font-weight: bold;
    }
    
    .btn {
      border: none;
      color: white;
      padding: 8px 16px;
      text-align: center;
      text-decoration: none;
      display: inline-block;
      font-size: 14px;
      font-weight: 500;
      border-radius: 6px;
      cursor: pointer;
      transition: all 0.3s ease;
      min-width: 80px;
    }
    
    .btn-on {
      background-color: var(--secondary);
    }
    
    .btn-off {
      background-color: var(--danger);
    }
    
    .btn:hover {
      opacity: 0.9;
      transform: translateY(-2px);
    }
    
    .status-display {
      font-family: 'Lucida Console', monospace;
      font-size: 1.2em;
      margin: 20px 0;
      padding: 15px;
      background: var(--light);
      border-radius: 5px;
      text-align: center;
    }
    
    .footer {
      text-align: center;
      margin-top: 30px;
    }
    
    .footer a {
      color: var(--primary);
      text-decoration: none;
    }
    
    .footer a:hover {
      text-decoration: underline;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>Toggle Switch Control</h1>
    
    <div class="status-display">
      <span id="runtime"></span>
    </div>
    
    <div class="toggle-grid">
)"));

  // Генерация элементов управления
  for (int i = 0; i < 8; i++) {
    String label = (i == 0) ? String(label0) :
                  (i == 1) ? String(label1) :
                  (i == 2) ? String(label2) :
                  (i == 3) ? String(label3) :
                  (i == 4) ? String(label4) :
                  (i == 5) ? String(label5) :
                  (i == 6) ? String(label6) : String(label7);
    
    bool isActive = stat[i] == '1';
    
    server.sendContent("<div class=\"toggle-item\">");
    server.sendContent("<span class=\"toggle-label" + String(isActive ? " active\"" : "\"") + ">");
    server.sendContent(label);
    server.sendContent("</span>");
    server.sendContent("<a id=\"toggleBtn" + String(i) + "\" ");
    server.sendContent("data-label=\"" + label + "\" ");
    server.sendContent("class=\"btn " + String(isActive ? "btn-off\"" : "btn-on\"") + " ");
    server.sendContent("href=\"" + String(isActive ? "soc" + String(i) + "Off" : "soc" + String(i) + "On") + "\">");
    server.sendContent(String(isActive ? "OFF" : "ON"));
    server.sendContent("</a>");
    server.sendContent("</div>");
  }

  server.sendContent_P(PSTR(R"(
    </div>
    
    <div class="footer">
      <a href="/">← Return to Home Page</a>
    </div>
  </div>
)"));
  server.sendContent(javaScript);
  server.sendContent_P(PSTR(R"(
</body>
</html>
)"));
  server.sendContent("");
}

void handleSWR() {
  if (!authorized()) return;
  server.sendHeader("Connection", "close");

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");

  server.sendContent_P(PSTR(R"(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>WI-FI Remote Console</title>
  <style>
    :root {
      --primary: #3498db;
      --secondary: #2ecc71;
      --danger: #e74c3c;
      --dark: #2c3e50;
      --light: #ecf0f1;
    }
    
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      background-color: #f5f7fa;
      margin: 0;
      padding: 20px;
      color: var(--dark);
    }
    
    .container {
      max-width: 800px;
      margin: 0 auto;
      background: white;
      border-radius: 10px;
      padding: 25px;
      box-shadow: 0 4px 12px rgba(0,0,0,0.1);
    }
    
    h1 {
      color: var(--primary);
      text-align: center;
      margin-bottom: 30px;
    }
    
    .toggle-grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(250px, 1fr));
      gap: 15px;
      margin-bottom: 30px;
    }
    
    .toggle-item {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 10px;
      padding: 12px;
      background: white;
      border-radius: 8px;
      box-shadow: 0 2px 5px rgba(0,0,0,0.1);
    }
    
    .toggle-label {
      flex-grow: 1;
      font-weight: 500;
    }
    
    .btn {
      border: none;
      color: white;
      padding: 8px 16px;
      text-align: center;
      text-decoration: none;
      display: inline-block;
      font-size: 14px;
      font-weight: 500;
      border-radius: 6px;
      cursor: pointer;
      transition: all 0.3s ease;
      min-width: 80px;
    }
    
    .btn-on {
      background-color: var(--secondary);
    }
    
    .btn-off {
      background-color: var(--danger);
    }
    
    .btn:hover {
      opacity: 0.9;
      transform: translateY(-2px);
    }
    
    .status-display {
      font-family: 'Lucida Console', monospace;
      font-size: 1.2em;
      margin: 20px 0;
      padding: 15px;
      background: var(--light);
      border-radius: 5px;
      text-align: center;
    }
    
    .footer {
      text-align: center;
      margin-top: 30px;
    }
    
    .footer a {
      color: var(--primary);
      text-decoration: none;
    }
    
    .footer a:hover {
      text-decoration: underline;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>WI-FI Remote Console</h1>
    
    <div class="status-display">
      <span id="runtime"></span>
    </div>
    
    <div class="toggle-grid">
)"));

  // JavaScript для динамического обновления
  server.sendContent_P(PSTR(R"(
  <script>
  function updateButtons() {
    fetch('/xml')
      .then(response => response.text())
      .then(str => (new DOMParser()).parseFromString(str, "text/xml"))
      .then(xml => {
        const statusText = xml.querySelector("response").textContent;
        document.getElementById('runtime').textContent = statusText;
        
        const statuses = statusText.split(' ');
        
        for (let i = 0; i < 8; i++) {
          const btn = document.getElementById(`btn${i}`);
          if (btn) {
            const isActive = statusText.includes(btn.dataset.label);
            btn.textContent = isActive ? "OFF" : "ON";
            btn.className = isActive ? "btn btn-off" : "btn btn-on";
            btn.href = isActive ? `socket${i}Off` : `socket${i}On`;
          }
        }
      });
  }
  
  setInterval(updateButtons, 1000);
  updateButtons(); // Вызываем сразу при загрузке
  </script>
  )"));

  // Генерация кнопок для каждого канала
  for (int i = 0; i < 8; i++) {
    String label = (i == 0) ? String(label0) :
                  (i == 1) ? String(label1) :
                  (i == 2) ? String(label2) :
                  (i == 3) ? String(label3) :
                  (i == 4) ? String(label4) :
                  (i == 5) ? String(label5) :
                  (i == 6) ? String(label6) : String(label7);
    
    bool isActive = stat[i] == '1';
    
    server.sendContent("<div class=\"toggle-item\">");
    server.sendContent("<span class=\"toggle-label\">" + label + "</span>");
    server.sendContent("<a id=\"btn" + String(i) + "\" data-label=\"" + label + "\" ");
    server.sendContent("href=\"" + String(isActive ? "socket" + String(i) + "Off" : "socket" + String(i) + "On") + "\" ");
    server.sendContent("class=\"btn " + String(isActive ? "btn-off\"" : "btn-on\"") + ">");
    server.sendContent(String(isActive ? "OFF" : "ON"));
    server.sendContent("</a>");
    server.sendContent("</div>");
  }

  server.sendContent_P(PSTR(R"(
    </div>
    
    <div class="footer">
      <a href="/">← Return to Home Page</a>
    </div>
  </div>
</body>
</html>
)"));
  server.sendContent("");
}


void handleRoot() {
  if (!authorized()) return;
  server.sendHeader("Connection", "close");
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  
  String Page = String(R"(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Remote Console</title>
  <style>
    :root {
      --primary: #3498db;
      --secondary: #2ecc71;
      --dark: #2c3e50;
      --light: #ecf0f1;
    }
    
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      line-height: 1.6;
      color: var(--dark);
      max-width: 800px;
      margin: 0 auto;
      padding: 20px;
      background-color: #f5f7fa;
    }
    
    .container {
      background: white;
      border-radius: 10px;
      padding: 25px;
      box-shadow: 0 4px 12px rgba(0,0,0,0.1);
    }
    
    h1 {
      color: var(--primary);
      text-align: center;
      margin-bottom: 30px;
    }
    
    .connection-info {
      background: var(--light);
      padding: 15px;
      border-radius: 8px;
      margin-bottom: 20px;
    }
    
    .nav-links {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
      gap: 15px;
      margin: 30px 0;
    }
    
    .nav-links a {
      display: block;
      padding: 12px;
      background: var(--primary);
      color: white;
      text-decoration: none;
      text-align: center;
      border-radius: 6px;
      transition: all 0.3s ease;
    }
    
    .nav-links a:hover {
      background: #2980b9;
      transform: translateY(-2px);
      box-shadow: 0 4px 8px rgba(0,0,0,0.1);
    }
    
    @media (max-width: 600px) {
      .nav-links {
        grid-template-columns: 1fr;
      }
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>Remote Console</h1>
    
    <div class="connection-info">
)");

  if (server.client().localIP() == apIP) {
    Page += F("<p>🔵 Connected via <strong>Soft AP</strong>: ");
    Page += softAP_ssid;
    Page += F("</p>");
  } else {
    Page += F("<p>🟢 Connected to <strong>WiFi Network</strong>: ");
    Page += ssid;
    Page += F("</p>");
  }

  Page += F(R"(
    </div>
    
    <div class="nav-links">
      <a href='/wifi'>WiFi Configuration</a>
      <a href='/switch'>Antenna Switch Control</a>
      <a href='/toggle'>Toggle Switch</a>
      <a href='/label'>Label Configuration</a>
    </div>
  </div>
</body>
</html>
)");

  server.send(200, "text/html", Page);
}

void handleLabel() {
  if (!authorized()) return;
  server.sendHeader("Connection", "close");
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");

  String Page2 = String(R"(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Labels Configuration</title>
  <style>
    :root {
      --primary: #3498db;
      --secondary: #2ecc71;
      --dark: #2c3e50;
      --light: #ecf0f1;
      --gray: #95a5a6;
    }
    
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      background-color: #f5f7fa;
      margin: 0;
      padding: 20px;
      color: var(--dark);
    }
    
    .container {
      max-width: 600px;
      margin: 0 auto;
      background: white;
      border-radius: 10px;
      padding: 25px;
      box-shadow: 0 4px 12px rgba(0,0,0,0.1);
    }
    
    h1 {
      color: var(--primary);
      text-align: center;
      margin-bottom: 30px;
    }

    h3 {
      color: var(--primary);
      text-align: center;
      margin-bottom: 15px;
    }
    
    .form-group {
      margin-bottom: 20px;
    }
    
    .form-label {
      display: block;
      margin-bottom: 8px;
      font-weight: 500;
    }
    
    .form-input {
      width: 100%;
      padding: 10px 15px;
      font-size: 16px;
      border: 1px solid #ddd;
      border-radius: 6px;
      transition: border 0.3s ease;
    }
    
    .form-input:focus {
      outline: none;
      border-color: var(--primary);
    }
    
    .btn {
      background-color: var(--primary);
      color: white;
      border: none;
      padding: 12px 20px;
      font-size: 16px;
      border-radius: 6px;
      cursor: pointer;
      transition: all 0.3s ease;
      width: 100%;
      margin-top: 10px;
    }
    
    .btn:hover {
      background-color: #2980b9;
      transform: translateY(-2px);
    }
    
    .footer {
      text-align: center;
      margin-top: 30px;
    }
    
    .footer a {
      color: var(--primary);
      text-decoration: none;
    }
    
    .footer a:hover {
      text-decoration: underline;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>Labels Configuration</h1>
    <h3>Use only uniq names (max 32 simbols)</h3>
    
    <form method='POST' action='labelsave'>
)");

  // Добавляем текущие значения меток в поля ввода
  for (int i = 0; i < 8; i++) {
    String currentLabel = (i == 0) ? String(label0) : 
                        (i == 1) ? String(label1) :
                        (i == 2) ? String(label2) :
                        (i == 3) ? String(label3) :
                        (i == 4) ? String(label4) :
                        (i == 5) ? String(label5) :
                        (i == 6) ? String(label6) : String(label7);
    
    Page2 += "<div class='form-group'>";
    Page2 += "<label class='form-label' for='l" + String(i) + "'>Label " + String(i) + "</label>";
    Page2 += "<input class='form-input' type='text' id='l" + String(i) + "' name='l" + String(i) + "' value='" + currentLabel + "' placeholder='Enter label name'/>";
    Page2 += "</div>";
  }

  Page2 += R"(
      <button type='submit' class='btn'>Save Labels</button>
    </form>
    
    <div class="footer">
      <a href="/">← Return to Home Page</a>
    </div>
  </div>
</body>
</html>
)";

  server.send(200, "text/html", Page2);
}

void handleLabelSave() {
  Serial.println("labels save");
  if (!authorized()) return;
  server.sendHeader("Connection", "close");
  // Reject saving when every label is empty (prevents accidental wipe)
  if (server.arg("l0").length() == 0 && server.arg("l1").length() == 0 &&
      server.arg("l2").length() == 0 && server.arg("l3").length() == 0 &&
      server.arg("l4").length() == 0 && server.arg("l5").length() == 0 &&
      server.arg("l6").length() == 0 && server.arg("l7").length() == 0) {
    Serial.println("labels save rejected: all labels empty");
    server.sendHeader("Location", "label", true);
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
    server.send(302, "text/plain", "");
    server.client().stop();
    return;
  }
  server.arg("l0").toCharArray(label0, sizeof(label0) - 1);
  server.arg("l1").toCharArray(label1, sizeof(label1) - 1);
  server.arg("l2").toCharArray(label2, sizeof(label2) - 1);
  server.arg("l3").toCharArray(label3, sizeof(label3) - 1);
  server.arg("l4").toCharArray(label4, sizeof(label4) - 1);
  server.arg("l5").toCharArray(label5, sizeof(label5) - 1);
  server.arg("l6").toCharArray(label6, sizeof(label6) - 1);
  server.arg("l7").toCharArray(label7, sizeof(label7) - 1);
  server.sendHeader("Location", "switch", true);
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.send(302, "text/plain", "");    // Empty content inhibits Content-length header so we have to close the socket ourselves.
  server.client().stop(); // Stop is needed because we sent no content length
  saveLabels();
  /*connect = strlen(ssid) > 0; // Request WLAN connect with new credentials if there is a SSID*/
}

/* ---------------------------------------------------------------------------
 * Persistent settings storage.
 * Uses a flash sector computed from the REAL flash size, so saving works on
 * small (1M) boards regardless of the configured board flash-size. The stock
 * EEPROM library selects a sector from the CONFIGURED size (e.g. 4M), which on
 * a physically 1M chip lies beyond the memory -> writes silently fail.
 * ------------------------------------------------------------------------- */
#define CFG_SEC_SIZE 4096

struct Settings {
  char magic[4];
  char ssid[32];
  char password[32];
  char labels[8][32];
};
static Settings cfgStore;

// union guarantees 4-byte alignment required by the flash API
static union {
  uint32_t w[(sizeof(Settings) + 3) / 4];
  uint8_t  b[sizeof(Settings)];
} cfgBuf;

static uint32_t cfgSector() {
  uint32_t sectors = ESP.getFlashChipRealSize() / CFG_SEC_SIZE;
  // use a sector safely inside the real flash, leaving the last one alone
  return (sectors > 2) ? (sectors - 2) : 1;
}

static void cfgLoad() {
  memset(&cfgStore, 0, sizeof(cfgStore));
  memset(cfgBuf.b, 0, sizeof(cfgBuf.b));
  ESP.flashRead(cfgSector() * CFG_SEC_SIZE, cfgBuf.w, sizeof(Settings));
  memcpy(&cfgStore, cfgBuf.b, sizeof(Settings));
  if (memcmp(cfgStore.magic, "RKC1", 4) != 0) memset(&cfgStore, 0, sizeof(cfgStore));
}

static bool cfgSave() {
  memcpy(cfgStore.magic, "RKC1", 4);
  memcpy(cfgBuf.b, &cfgStore, sizeof(Settings));
  if (!ESP.flashEraseSector(cfgSector())) return false;
  return ESP.flashWrite(cfgSector() * CFG_SEC_SIZE, cfgBuf.w, sizeof(Settings));
}

void loadLabels() {
  cfgLoad();
  memcpy(label0, cfgStore.labels[0], sizeof(label0));
  memcpy(label1, cfgStore.labels[1], sizeof(label1));
  memcpy(label2, cfgStore.labels[2], sizeof(label2));
  memcpy(label3, cfgStore.labels[3], sizeof(label3));
  memcpy(label4, cfgStore.labels[4], sizeof(label4));
  memcpy(label5, cfgStore.labels[5], sizeof(label5));
  memcpy(label6, cfgStore.labels[6], sizeof(label6));
  memcpy(label7, cfgStore.labels[7], sizeof(label7));
  if (label0[0] == '\0') strcpy(label0, "A1");
  if (label1[0] == '\0') strcpy(label1, "A2");
  if (label2[0] == '\0') strcpy(label2, "A3");
  if (label3[0] == '\0') strcpy(label3, "A4");
  if (label4[0] == '\0') strcpy(label4, "A5");
  if (label5[0] == '\0') strcpy(label5, "A6");
  if (label6[0] == '\0') strcpy(label6, "A7");
  if (label7[0] == '\0') strcpy(label7, "A8");
  Serial.println("Labels:");
  Serial.println(label0);
  Serial.println(label1);
  Serial.println(label2);
  Serial.println(label3);
  Serial.println(label4);
  Serial.println(label5);
  Serial.println(label6);
  Serial.println(label7);
}

/** Store labels to flash. */
void saveLabels() {
  cfgLoad(); // preserve ssid/password fields
  memcpy(cfgStore.labels[0], label0, sizeof(label0));
  memcpy(cfgStore.labels[1], label1, sizeof(label1));
  memcpy(cfgStore.labels[2], label2, sizeof(label2));
  memcpy(cfgStore.labels[3], label3, sizeof(label3));
  memcpy(cfgStore.labels[4], label4, sizeof(label4));
  memcpy(cfgStore.labels[5], label5, sizeof(label5));
  memcpy(cfgStore.labels[6], label6, sizeof(label6));
  memcpy(cfgStore.labels[7], label7, sizeof(label7));
  cfgSave();
}

void handleWifi() {
  if (!authorized()) return;
  server.sendHeader("Connection", "close");
  // Запрещаем кэширование страницы
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");

  // Формируем HTML страницу
  String Page = String(R"(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>WiFi Configuration</title>
  <style>
    :root {
      --primary: #3498db;
      --secondary: #2ecc71;
      --danger: #e74c3c;
      --dark: #2c3e50;
      --light: #ecf0f1;
    }
    
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      line-height: 1.6;
      color: var(--dark);
      max-width: 800px;
      margin: 0 auto;
      padding: 20px;
      background-color: #f5f7fa;
    }
    
    h1 {
      color: var(--primary);
      text-align: center;
      margin-bottom: 20px;
    }
    
    .status-card {
      background: white;
      border-radius: 8px;
      padding: 20px;
      margin-bottom: 20px;
      box-shadow: 0 2px 10px rgba(0,0,0,0.1);
    }
    
    .btn {
      border: none;
      color: white;
      padding: 8px 16px;
      text-align: center;
      text-decoration: none;
      display: inline-block;
      font-size: 14px;
      font-weight: 500;
      border-radius: 6px;
      cursor: pointer;
      transition: all 0.3s ease;
      min-width: 60px;
    }
    
    .btn-on {
      background-color: var(--secondary);
    }
    
    .btn-off {
      background-color: var(--danger);
    }
    
    .btn:hover {
      opacity: 0.9;
      transform: translateY(-1px);
    }
    
    table {
      width: 100%;
      border-collapse: collapse;
      margin: 15px 0;
      background: white;
      border-radius: 8px;
      overflow: hidden;
      box-shadow: 0 2px 10px rgba(0,0,0,0.1);
    }
    
    th, td {
      padding: 12px 15px;
      text-align: left;
      border-bottom: 1px solid #ddd;
    }
    
    th {
      background-color: var(--primary);
      color: white;
    }
    
    tr:hover {
      background-color: #f5f5f5;
    }
    
    form {
      background: white;
      padding: 20px;
      border-radius: 8px;
      box-shadow: 0 2px 10px rgba(0,0,0,0.1);
      margin: 20px 0;
    }
    
    input[type="text"],
    input[type="password"] {
      width: 100%;
      padding: 10px;
      margin: 8px 0;
      display: inline-block;
      border: 1px solid #ccc;
      border-radius: 4px;
      box-sizing: border-box;
    }
    
    input[type="submit"] {
      width: 100%;
      background-color: var(--primary);
      color: white;
      padding: 12px 20px;
      margin: 8px 0;
      border: none;
      border-radius: 4px;
      cursor: pointer;
      font-size: 16px;
    }
    
    input[type="submit"]:hover {
      background-color: #2980b9;
    }
    
    .ap-status {
      display: inline-block;
      padding: 4px 8px;
      border-radius: 4px;
      font-weight: bold;
    }
    
    .ap-on {
      background-color: var(--secondary);
      color: white;
    }
    
    .ap-off {
      background-color: var(--danger);
      color: white;
    }
    
    .footer {
      text-align: center;
      margin-top: 20px;
    }
    
    .footer a {
      color: var(--primary);
      text-decoration: none;
    }
    
    .footer a:hover {
      text-decoration: underline;
    }
  </style>
</head>
<body>
  <h1>WiFi Configuration</h1>
  
  <div class="status-card">
)");

  // Добавляем информацию о подключении
  if (server.client().localIP() == apIP) {
    Page += F("<p>🔵 Connected via <strong>Soft AP</strong>: ");
    Page += softAP_ssid;
    Page += F("</p>");
  } else {
    Page += F("<p>🟢 Connected to <strong>WiFi Network</strong>: ");
    Page += ssid;
    Page += F("</p>");
  }

  // Добавляем uptime
  Page += F("<p>Uptime: <strong>");
  Page += String(day) + "d " + String(hour) + "h " + String(minute) + "m " + String(sec) + "s";
  Page += F("</strong></p>");
  
  // Добавляем управление AP
  /*Page += F(R"(
    <p>
      <a href="APon" class="btn btn-on">ON</a>
      <a href="APoff" class="btn btn-off">OFF</a>
      Access Point Status: <span class="ap-status )");
  Page += (flag_off == 0 ? "ap-on\">ON" : "ap-off\">OFF");*/
  Page += F(R"(</span>
    </p>
  </div>
  
  <div class="status-card">
    <h3>SoftAP Configuration</h3>
    <table>
      <tr><th>Parameter</th><th>Value</th></tr>
      <tr><td>SSID</td><td>)");
  Page += softAP_ssid;
  Page += F(R"(</td></tr>
      <tr><td>IP Address</td><td>)");
  Page += WiFi.softAPIP().toString();
  Page += F(R"(</td></tr>
    </table>
  </div>
  
  <div class="status-card">
    <h3>WLAN Configuration</h3>
    <table>
      <tr><th>Parameter</th><th>Value</th></tr>
      <tr><td>SSID</td><td>)");
  Page += ssid;
  Page += F(R"(</td></tr>
      <tr><td>IP Address</td><td>)");
  Page += WiFi.localIP().toString();
  Page += F(R"(</td></tr>
    </table>
  </div>
  
  <div class="status-card">
    <h3>Available Networks</h3>
    <p>Click refresh if networks are missing</p>
    <table>
      <tr><th>Network Name</th><th>Signal</th></tr>
)");

  // Сканируем сети WiFi
  Serial.println("scan start");
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  
  if (n > 0) {
    for (int i = 0; i < n; i++) {
      Page += F("<tr><td>");
      Page += WiFi.SSID(i);
      if (WiFi.encryptionType(i) != ENC_TYPE_NONE) {
        Page += F(" <small>(secured)</small>");
      }
      Page += F("</td><td>");
      Page += WiFi.RSSI(i);
      Page += F(" dBm</td></tr>");
    }
  } else {
    Page += F("<tr><td colspan=\"2\">No WiFi networks found</td></tr>");
  }

  Page += F(R"(
    </table>
  </div>
  
  <form method='POST' action='wifisave'>
    <h3>Connect to Network</h3>
    <label for="n">Network SSID:</label>
    <input type="text" id="n" name="n" placeholder="Enter network name" required>
    
    <label for="p">Password:</label>
    <input type="password" id="p" name="p" placeholder="Enter password">
    
    <input type="submit" value="Connect">
  </form>
  
  <div class="footer">
    <a href="/">← Return to Home Page</a>
  </div>
</body>
</html>
)");

  server.send(200, "text/html", Page);
}


void handleNotFound() {
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
  server.sendHeader("Connection", "close");
  // Reject empty SSID to prevent accidental wiping of saved credentials
  String newSsid = server.arg("n");
  newSsid.trim();
  if (newSsid.length() == 0) {
    Serial.println("wifi save rejected: empty SSID");
    server.sendHeader("Location", "wifi", true);
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
    server.send(302, "text/plain", "");
    server.client().stop();
    return;
  }
  newSsid.toCharArray(ssid, sizeof(ssid) - 1);
  server.arg("p").toCharArray(password, sizeof(password) - 1);
  server.sendHeader("Location", "wifi", true);
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.send(302, "text/plain", "");    // Empty content inhibits Content-length header so we have to close the socket ourselves.
  server.client().stop(); // Stop is needed because we sent no content length
  saveCredentials();
  wifiRetryCount = 0; // new credentials: next attempt should do a fresh disconnect+begin
  connect = strlen(ssid) > 0; // Request WLAN connect with new credentials if there is a SSID
}

void loadCredentials() {
  cfgLoad();
  memcpy(ssid, cfgStore.ssid, sizeof(ssid));
  memcpy(password, cfgStore.password, sizeof(password));
  Serial.println("Recovered credentials:");
  Serial.println(ssid);
  Serial.println(strlen(password) > 0 ? "********" : "<no password>");
}

/** Store WLAN credentials to flash. */
void saveCredentials() {
  cfgLoad(); // preserve labels fields
  memcpy(cfgStore.ssid, ssid, sizeof(ssid));
  memcpy(cfgStore.password, password, sizeof(password));
  cfgSave();
}

void turnOffAll()
{
  currentlabel="";
  stat[0]=stat[1]=stat[2]=stat[3]=stat[4]=stat[5]=stat[6]=stat[7]='0';
  digitalWrite(gpio0_pin, LOW);
  digitalWrite(gpio1_pin, LOW);
  digitalWrite(gpio2_pin, LOW);
  digitalWrite(gpio3_pin, LOW);
  digitalWrite(gpio4_pin, LOW);
  digitalWrite(gpio5_pin, LOW);
  digitalWrite(gpio6_pin, LOW);
  digitalWrite(gpio7_pin, LOW);
}
 
void setup(void){
  delay(100);
  pinMode(gpio0_pin, OUTPUT);
  digitalWrite(gpio0_pin, LOW);
  pinMode(gpio1_pin, OUTPUT);
  digitalWrite(gpio1_pin, LOW);
  pinMode(gpio2_pin, OUTPUT);
  digitalWrite(gpio2_pin, LOW);
  pinMode(gpio3_pin, OUTPUT);
  digitalWrite(gpio3_pin, LOW);
  pinMode(gpio4_pin, OUTPUT);
  digitalWrite(gpio4_pin, LOW);
  pinMode(gpio5_pin, OUTPUT);
  digitalWrite(gpio5_pin, LOW);
  pinMode(gpio6_pin, OUTPUT);
  digitalWrite(gpio6_pin, LOW);
  pinMode(gpio7_pin, OUTPUT);
  digitalWrite(gpio7_pin, LOW);
  pinMode(gpio8_pin, OUTPUT);
  digitalWrite(gpio8_pin, LOW);

  Serial.begin(115200);
  Serial.println();
  Serial.print("Reset reason: ");
  Serial.println(ESP.getResetReason());
  Serial.print("Configuring access point...");
  /* You can remove the password parameter if you want the AP to be open. */
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP(softAP_ssid, softAP_password);
  delay(500); // Without delay I've seen the IP address blank
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  /* Setup the DNS server redirecting all the domains to the apIP */
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", apIP);

  /*WiFiManager wm;
  bool res;
  res = wm.autoConnect("RKConsole","1234567890"); // password protected ap
  if(!res) {
        Serial.println("Failed to connect");
    } 
    else { 
        Serial.println("connected...yeey :)");
    }*/
  
  server.on("/", handleRoot);
  server.on("/wifi", handleWifi);
  server.on("/label", handleLabel);
  server.on("/labelsave", handleLabelSave);
  server.on("/switch", handleSWR);
  server.on("/toggle", handleToggle);
  server.on("/wifisave", handleWifiSave);
  server.on("/generate_204", handleRoot);
  server.on("/fwlink", handleRoot);  
  server.on("/xml", handleXML);
  server.onNotFound(handleNotFound);
  server.begin(); // Web server start
  Serial.println("HTTP server started");
  loadCredentials(); // Load WLAN credentials from network
  loadLabels();
  connect = strlen(ssid) > 0; // Request WLAN connect if there is a SSID
  dnsServer.processNextRequest();
  server.handleClient();

  
  server.on("/APon", [](){
    if (!authorized()) return;
    if (flag_off==1)
    {
        WiFi.softAPConfig(apIP, apIP, netMsk);
        WiFi.softAP(softAP_ssid, softAP_password);
        delay(500); 
        flag_off=0;
    }
    handleWifi();
  });

  server.on("/APoff", [](){
    if (!authorized()) return;
    flag_off=1;
    WiFi.softAPdisconnect(true);
    handleWifi();
  });
  
  server.on("/socket0On", [](){
    if (!authorized()) return;
    turnOffAll();
    currentlabel=String(label0);
    digitalWrite(gpio0_pin, HIGH);
    handleSWR();
  });
  server.on("/socket0Off", [](){
    if (!authorized()) return;
    turnOffAll();
    currentlabel="OFF";
    handleSWR();
  });
  server.on("/socket1On", [](){
    if (!authorized()) return;
    turnOffAll();
    currentlabel=String(label1);
    digitalWrite(gpio1_pin, HIGH);
    handleSWR();
  });
  server.on("/socket1Off", [](){
    if (!authorized()) return;
    turnOffAll();
    currentlabel="OFF";
    handleSWR();
  });
  server.on("/socket2On", [](){
    if (!authorized()) return;
    turnOffAll();
    digitalWrite(gpio2_pin, HIGH);
    currentlabel=String(label2);
    handleSWR();
  });
  server.on("/socket2Off", [](){
    if (!authorized()) return;
    turnOffAll();
    currentlabel="OFF";
    handleSWR();
  });
  server.on("/socket3On", [](){
    if (!authorized()) return;
    turnOffAll();
    digitalWrite(gpio3_pin, HIGH);
    currentlabel=String(label3);
    handleSWR();
  });
  server.on("/socket3Off", [](){
    if (!authorized()) return;
    turnOffAll();
    currentlabel="OFF";
    handleSWR();
  });
  server.on("/socket4On", [](){
    if (!authorized()) return;
    turnOffAll();
    digitalWrite(gpio4_pin, HIGH);
    currentlabel=String(label4);
    handleSWR();
  });
  server.on("/socket4Off", [](){
    if (!authorized()) return;
    turnOffAll();
    currentlabel="OFF";
    handleSWR();
  });
  server.on("/socket5On", [](){
    if (!authorized()) return;
    turnOffAll();
    digitalWrite(gpio5_pin, HIGH);
    currentlabel=String(label5);
    handleSWR();
  });
  server.on("/socket5Off", [](){
    if (!authorized()) return;
    turnOffAll();
    currentlabel="OFF";
    handleSWR();
  });
  server.on("/socket6On", [](){
    if (!authorized()) return;
    turnOffAll();
    digitalWrite(gpio6_pin, HIGH);
    currentlabel=String(label6);
    handleSWR();
  });
  server.on("/socket6Off", [](){
    if (!authorized()) return;
    turnOffAll();
    currentlabel="OFF";
    handleSWR();
  });
  server.on("/socket7On", [](){
    if (!authorized()) return;
    turnOffAll();
    digitalWrite(gpio7_pin, HIGH);
    currentlabel=String(label7);
    handleSWR();
  });
  server.on("/socket7Off", [](){
    if (!authorized()) return;
    turnOffAll();
    currentlabel="OFF";
    handleSWR();
  });
/*---------------------------------------*/  
  server.on("/soc0On", [](){
    if (!authorized()) return;
    stat[0] = '1';
    digitalWrite(gpio0_pin, HIGH);
    CheckStat();
    handleToggle();
  });
  server.on("/soc0Off", [](){
    if (!authorized()) return;
    digitalWrite(gpio0_pin, LOW);
    stat[0] = '0';
    CheckStat();
    handleToggle();
  });
  server.on("/soc1On", [](){
    if (!authorized()) return;
    stat[1] = '1';
    digitalWrite(gpio1_pin, HIGH);
    CheckStat();
    handleToggle();
  });
  server.on("/soc1Off", [](){
    if (!authorized()) return;
    stat[1] = '0';
    digitalWrite(gpio1_pin, LOW);
    CheckStat();
    handleToggle();
  });
  server.on("/soc2On", [](){
    if (!authorized()) return;
    stat[2] = '1';
    digitalWrite(gpio2_pin, HIGH);
    CheckStat();
    handleToggle();
  });
  server.on("/soc2Off", [](){
    if (!authorized()) return;
    stat[2] = '0';
    digitalWrite(gpio2_pin, LOW);
    CheckStat();
    handleToggle();
  });
  server.on("/soc3On", [](){
    if (!authorized()) return;
    stat[3] = '1';
    digitalWrite(gpio3_pin, HIGH);
    CheckStat();
    handleToggle();
  });
  server.on("/soc3Off", [](){
    if (!authorized()) return;
    stat[3] = '0';
    digitalWrite(gpio3_pin, LOW);
    CheckStat();
    handleToggle();
  });
  server.on("/soc4On", [](){
    if (!authorized()) return;
    stat[4] = '1';
    digitalWrite(gpio4_pin, HIGH);
    CheckStat();
    handleToggle();
  });
  server.on("/soc4Off", [](){
    if (!authorized()) return;
    stat[4] = '0';
    digitalWrite(gpio4_pin, LOW);
    CheckStat();
    handleToggle();
  });
  server.on("/soc5On", [](){
    if (!authorized()) return;
    stat[5] = '1';
    digitalWrite(gpio5_pin, HIGH);
    CheckStat();
    handleToggle();
  });
  server.on("/soc5Off", [](){
    if (!authorized()) return;
    stat[5] = '0';
    digitalWrite(gpio5_pin, LOW);
    CheckStat();
    handleToggle();
  });
  server.on("/soc6On", [](){
    if (!authorized()) return;
    stat[6] = '1';
    digitalWrite(gpio6_pin, HIGH);
    CheckStat();
    handleToggle();
  });
  server.on("/soc6Off", [](){
    if (!authorized()) return;
    stat[6] = '0';
    digitalWrite(gpio6_pin, LOW);
    CheckStat();
    handleToggle();
  });
  server.on("/soc7On", [](){
    if (!authorized()) return;
    stat[7] = '1';
    digitalWrite(gpio7_pin, HIGH);
    CheckStat();
    handleToggle();
  });
  server.on("/soc7Off", [](){
    if (!authorized()) return;
    stat[7] = '0';
    digitalWrite(gpio7_pin, LOW);
    CheckStat();
    handleToggle();
  });
  
}

/** Start connecting to WLAN without blocking (status is polled in loop).
 * fresh=true (first attempt after boot or new credentials) does a full
 * WiFi.disconnect()+mode reset before begin(); fresh=false (automatic
 * retries against an unreachable network) skips that and just calls
 * begin() again. Repeating the disconnect()+mode() reset on every retry
 * while the softAP stays active is what was crashing the WiFi SDK
 * (Exception 9 / LoadStoreError) after a handful of retries when the
 * saved network was unreachable. */
void startWifiConnect(bool fresh) {
  Serial.println("Connecting as wifi client (non-blocking)...");
  if (fresh) {
    // Keep the softAP active alongside the station connection
    WiFi.mode(WIFI_AP_STA);
    // Clear stale station state so begin() reliably initiates a connection
    WiFi.disconnect();
    delay(50);
  }
  WiFi.begin(ssid, password);
}


 
void loop(void){
  dnsServer.processNextRequest();

  if (connect) {
    Serial.println("Connect requested");
    connect = false;
    lastConnectTry = millis();
    startWifiConnect(wifiRetryCount == 0);
  }

  {
    unsigned int s = WiFi.status();
    // Non-blocking auto-reconnect: retry only if not connected, never block.
    // A fresh (first) retry does WiFi.disconnect()+mode reset, so its window
    // must stay longer than a normal WPA2 handshake + DHCP lease (commonly
    // 5-15s, longer with the softAP sharing the radio) - otherwise we abort
    // our own in-progress connection attempt before it finishes.
    //
    // Repeated WiFi.begin() calls against a network that stays unreachable
    // corrupt the ESP8266 WiFi SDK's internal state after a handful of
    // repeats and crash it (Exception 9) - this happens even without the
    // disconnect()/mode() reset, so it's the repetition of begin() itself
    // that's unsafe, not just the reset. The original firmware only ever
    // retried once every 100 minutes and never hit this. We keep automatic
    // recovery (unlike the original, which was effectively "retry never"),
    // but back off aggressively - doubling from 20s up to a 30-minute cap -
    // so WiFi.begin() gets called only a handful of times total instead of
    // dozens of times per hour.
    unsigned long backoff = 20000UL << min(wifiRetryCount, 7U); // 20/40/80/160/320/640/1280s
    if (backoff > 1800000UL) backoff = 1800000UL; // cap at 30 min
    if (s != WL_CONNECTED && strlen(ssid) > 0 && !connect &&
        millis() > (lastConnectTry + backoff)) {
      connect = true;
      wifiRetryCount++;
    }
    if (status != s) { // WLAN status change
      Serial.print("Status: ");
      Serial.println(s);
      status = s;
      if (s == WL_CONNECTED) {
        /* Just connected to WLAN */
        Serial.println("");
        Serial.print("Connected to ");
        Serial.println(ssid);
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        digitalWrite(gpio8_pin, HIGH);
        flagAP=1;
        wifiRetryCount = 0; // reset backoff so the next disconnect starts fresh
      } else {
        /* Disconnected or unavailable */
        digitalWrite(gpio8_pin, LOW);
        flagAP=0;
        /* Deliberately do NOT call WiFi.disconnect() here - it can disrupt
           the radio/softAP and cause instability while the network is scanned. */
      }
    }
  }
  if (flagAP==1) 
  {
    tim++; 
    if (tim==1000) {sec++; tim=0;}
    if (sec==60) {minute++; sec=0;}
    if (minute==60) {hour++; minute=0;}
    if (hour==24) {day++; hour=0;}
  }
  if ((flagAP==0)&&(tim>0)) tim=sec=minute=hour=day=0;

  static unsigned long lastHeapLog = 0;
  if (millis() - lastHeapLog > 30000) {
    lastHeapLog = millis();
    Serial.print("Free heap: ");
    Serial.println(ESP.getFreeHeap());
  }

  server.handleClient();
  delay(1);
}
