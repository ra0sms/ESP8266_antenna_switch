/*
K9AY loop control box
Generic ESP8266 Module
Irkutsk, 2024
https://ra0sms.com

PCF8575 library - https://github.com/RobTillaart/PCF8575
WIFI manager - https://github.com/tzapu/WiFiManager

*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <EEPROM.h>
#include <SPI.h>
#include "PCF8575.h"
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiManager.h>
//**********************************************
PCF8575 PCF(0x20);
ESP8266WiFiMulti WiFiMulti;
//********************************************
char destination_address[20] = "";
String response= "";
String buffer = "";
//*********************************************/ 
int PA_pin = 4;
int LED_pin = 5;
int OE_pin = 12;
/***SPI*/
int latchPin = 15;
int clockPin = 14;
int dataPin = 13;
/**********************************************/
int out1=0;
int out2=0;
uint16_t current_pcf_value;
uint16_t start_pcf_value;
int flag_failed_connection = 0;
int flag_ip_updated = 0;
int counter_for_getting_data = 0;
/**************************************************/

void read_from_uart (){
  static unsigned char i = 0;
  static char buff[30];
  while (Serial.available())
  {
    char inChar = (char)Serial.read();
    if (inChar == '\r')
    {
      buff[i] = 0;
      //Serial.println(buff);
      buffer = String(buff);
      if ((buff[0]=='S')&&(buff[1]=='E')&&(buff[2]=='T')) {
        buffer = buffer.substring(3,i); 
        buffer.toCharArray(destination_address, 20); 
        flag_ip_updated = 1;
      }
      if ((buff[0]=='G')&&(buff[1]=='E')&&(buff[2]=='T')) read_ip_from_eeprom();
      Serial.println(destination_address);
      i = 0;
    }
    else
      buff[i++] = inChar;
  }
}

void read_ip_from_eeprom(){
  EEPROM.begin(512);
  EEPROM.get(0, destination_address);
  EEPROM.end();
}

void save_ip_to_eeprom(){
  if (flag_ip_updated){
    EEPROM.begin(512);
    flag_ip_updated = 0;
    EEPROM.put(0, destination_address);
    delay(3);
    if (EEPROM.commit()) {
      Serial.println("EEPROM successfully committed");
    } else {
      Serial.println("ERROR! EEPROM commit failed");
    }
    EEPROM.end();
  }
}

void set_alarm_led(){
    out1 = 0b00000001;
    digitalWrite(latchPin, LOW);
    SPI.transfer(out1);
    SPI.transfer(out2);
    digitalWrite(latchPin, HIGH);
    delay(200);
    out1 = 0b00000000;
    digitalWrite(latchPin, LOW);
    SPI.transfer(out1);
    SPI.transfer(out2);
    digitalWrite(latchPin, HIGH);
    delay(200);
}

void init_pcf (){
{
  Serial.println(__FILE__);
  Serial.print("PCF8575_LIB_VERSION:\t");
  Serial.println(PCF8575_LIB_VERSION);
  Wire.begin();
  if (!PCF.begin())
  {
    Serial.println("could not initialize...");
  }
  if (!PCF.isConnected())
  {
    Serial.println("=> not connected");
  }
  else
  {
    Serial.println("=> connected!!");
  }
}
}

void init_wifi (){
  WiFiManager wm;
  bool res;
  res = wm.autoConnect("AutoConnectAP","password");
    if(!res) {
        Serial.println("Failed to connect");
    } 
    else {  
        Serial.println("connected...");
    }
}

void get_http(String command){
  if ((WiFiMulti.run() == WL_CONNECTED)) {
    WiFiClient client;
    HTTPClient http;
    //Serial.print("[HTTP] begin...\n");
    if (http.begin(client, "http://" + String(destination_address)+command)) { 
      //Serial.print("[HTTP] GET...\n");
      int httpCode = http.GET();
      if (httpCode > 0) {
        //Serial.printf("[HTTP] GET... code: %d\n", httpCode);
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = http.getString();
          //Serial.println(payload);
          response = payload;
          flag_failed_connection = 0;
        }
      } else {
        Serial.printf("[HTTP] GET failed, error: %s\n", http.errorToString(httpCode).c_str());
        flag_failed_connection = 1;
      }

      http.end();
    } else {
      Serial.printf("[HTTP} Unable to connect\n");
    }
  }
}

void set_N(){
    get_http("/socket0On");
    //out1 = 0b00000000;
    out2 = 0b00000001;
    digitalWrite(latchPin, LOW);
    SPI.transfer(out1);
    SPI.transfer(out2);
    digitalWrite(latchPin, HIGH);
}

void set_NE(){
    get_http("/socket1On");
    //out1 = 0b00000000;
    out2 = 0b00000010;
    digitalWrite(latchPin, LOW);
    SPI.transfer(out1);
    SPI.transfer(out2);
    digitalWrite(latchPin, HIGH);
}

void set_E(){
    get_http("/socket2On");
    //out1 = 0b00000000;
    out2 = 0b00000100;
    digitalWrite(latchPin, LOW);
    SPI.transfer(out1);
    SPI.transfer(out2);
    digitalWrite(latchPin, HIGH);
}

void set_SE(){
    get_http("/socket3On");
    //out1 = 0b00000000;
    out2 = 0b00001000;
    digitalWrite(latchPin, LOW);
    SPI.transfer(out1);
    SPI.transfer(out2);
    digitalWrite(latchPin, HIGH);
}

void set_S(){
    get_http("/socket4On");
   // out1 = 0b00000000;
    out2 = 0b00010000;
    digitalWrite(latchPin, LOW);
    SPI.transfer(out1);
    SPI.transfer(out2);
    digitalWrite(latchPin, HIGH);
}

void set_SW(){
    get_http("/socket5On");
    //out1 = 0b00000000;
    out2 = 0b00100000;
    digitalWrite(latchPin, LOW);
    SPI.transfer(out1);
    SPI.transfer(out2);
    digitalWrite(latchPin, HIGH);
}

void set_W(){
    get_http("/socket6On");
    //out1 = 0b00000000;
    out2 = 0b01000000;
    digitalWrite(latchPin, LOW);
    SPI.transfer(out1);
    SPI.transfer(out2);
    digitalWrite(latchPin, HIGH);
}

void set_NW(){
    get_http("/socket7On");
    //out1 = 0b00000000;
    out2 = 0b10000000;
    digitalWrite(latchPin, LOW);
    SPI.transfer(out1);
    SPI.transfer(out2);
    digitalWrite(latchPin, HIGH);
}

void set_PA(){
    get_http("/xml");
    out1 = 0b00000001;
    //out2 = 0b10000000;
    digitalWrite(latchPin, LOW);
    SPI.transfer(out1);
    SPI.transfer(out2);
    digitalWrite(latchPin, HIGH);
}

void set_all_off(){
      out1 = 0b00000000;
      out2 = 0b00000000;
    digitalWrite(latchPin, LOW);
    SPI.transfer(out1);
    SPI.transfer(out2);
    digitalWrite(latchPin, HIGH);
}

void check_buttons (int x){
  if ((start_pcf_value - x)==1)set_N();
  if ((start_pcf_value - x)==2)set_NE();
  if ((start_pcf_value - x)==4)set_E();
  if ((start_pcf_value - x)==8)set_SE();
  if ((start_pcf_value - x)==16)set_S();
  if ((start_pcf_value - x)==32)set_SW();
  if ((start_pcf_value - x)==64)set_W();
  if ((start_pcf_value - x)==128)set_NW();
  if ((start_pcf_value - x)==256)set_PA();
}

void check_rotary_switch(int x){
 switch (x){
   case 32255:
   case 31743:
   case 30719:
   case 28671:
   case 24575:
   case 16383: start_pcf_value = x; break;
 }
}

void get_data_from_server(){
  get_http("/xml");
  if (response[32]=='1') set_N();
  if (response[32]=='2') set_NE();
  if (response[32]=='3') set_E();
  if (response[32]=='4') set_SE();
  if (response[32]=='5') set_S();
  if (response[32]=='6') set_SW();
  if (response[32]=='7') set_W();
  if (response[32]=='8') set_NW();
  if ((response[31]=='O')&&(response[32]=='F')&&(response[33]=='F')) set_all_off();

}

void setup(void){
  delay(100);
  pinMode(PA_pin, OUTPUT);
  digitalWrite(PA_pin, LOW);
  pinMode(LED_pin, OUTPUT);
  digitalWrite(LED_pin, LOW);
  pinMode(OE_pin, OUTPUT);
  digitalWrite(OE_pin, HIGH);
  pinMode(dataPin, OUTPUT);
  digitalWrite(dataPin, LOW);
  pinMode(latchPin, OUTPUT);
  digitalWrite(latchPin, LOW);
  pinMode(clockPin, OUTPUT);
  digitalWrite(clockPin, LOW);
  delay(200);
  digitalWrite(OE_pin, LOW);
  SPI.begin();
  digitalWrite(latchPin, LOW);
  SPI.transfer(0);
  SPI.transfer(0);
  digitalWrite(latchPin, HIGH);
  pinMode(OE_pin, OUTPUT);
  digitalWrite(OE_pin, LOW);
  digitalWrite(clockPin, LOW);
  out1 = 0b11111111;
  out2 = 0b11111111;
  digitalWrite(latchPin, LOW);
  SPI.transfer(out1);
  SPI.transfer(out2);
  digitalWrite(latchPin, HIGH);
  delay(1000);
  out1 = 0;
  out2 = 0;
  digitalWrite(latchPin, LOW);
  SPI.transfer(out1);
  SPI.transfer(out2);
  digitalWrite(latchPin, HIGH);
  delay(100);
  Serial.begin(115200);
  Serial.println();

  init_wifi(); 
  init_pcf();
  digitalWrite(OE_pin, LOW);
  delay(100);
  start_pcf_value = PCF.read16();
  delay(1000);
  start_pcf_value = PCF.read16();
  delay(100);
  Serial.print("Start pcf value ");
  Serial.println(start_pcf_value);
  delay(200);

  read_ip_from_eeprom();
  get_data_from_server();
}
 
void loop(void){
  current_pcf_value = PCF.read16();
  check_rotary_switch(current_pcf_value);
  check_buttons(current_pcf_value);
  if (flag_failed_connection == 1) set_alarm_led();
  read_from_uart();
  save_ip_to_eeprom();
  counter_for_getting_data++;
  if (counter_for_getting_data == 10) {
    get_data_from_server();
    counter_for_getting_data=0;
  }
  delay(20);
}