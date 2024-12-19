/*
Compile with minimal SPIFFS Partition Scheme

Layout is grid of tiles. Each tile has an icon and text. Status is shown as color and in text form.

Homeassistant sends states via REST. Uses jinja templating of homeassistant to insert states.
*/
#include <WiFi.h>
#include <ESP32Lib.h>  //https://github.com/bitluni/ESP32Lib
#ifdef USE_WM
#include <WiFiManager.h>  //https://github.com/tzapu/WiFiManager/
#endif
#include <ArduinoJson.h>  //https://arduinojson.org/
#include <Ressources/CodePage437_8x14.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <map>
#include <LittleFS.h>
#include <Wire.h>
#include <DDCVCP.h>  //https://github.com/tttttx2/ddcvcp
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <WiFiUdp.h>
#include <NTPClient.h>  //https://github.com/arduino-libraries/NTPClient
#include <HTTPClient.h>
#ifdef USE_HTTPS_WEBHOOK
#include <NetworkClientSecure.h>
#endif

#include "config.h"

VGA3Bit vga;
WebServer srv;
DDCVCP ddc;
WiFiUDP ntpUDP;
NTPClient ntp(ntpUDP,NTP_SERVER);

auto color_ok = vga.RGB(0, 255, 0);
auto color_bad = vga.RGB(255, 0, 255);
auto color_unknown = vga.RGB(255, 255, 255);
auto white = vga.RGB(255, 255, 255);
auto black = vga.RGB(0, 0, 0);

uint16_t res_x = 384;
uint16_t res_y = 272;

const uint8_t ledPin = 33;
const uint8_t redPin = 12;
const uint8_t greenPin = 13;
const uint8_t bluePin = 15;
const uint8_t hsyncPin = 14;
const uint8_t vsyncPin = 2;
const uint8_t sdaPin = 16;
const uint8_t sclPin = 0;

#define TILE_SIZE 128  //each tile is 128x128
String layout_path = "/layout.json";

DynamicJsonDocument layout(8192);
char info_text[32] = "";
uint16_t info_text_x = 0;

#include "stuff.h"
#include "sprites.h"
#include "graphics.h"
#include "rest.h"

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  Serial.begin(115200);
  Serial.print("\nvspace.one Info-Monitor, compiled ");
  Serial.println(__DATE__);

  //VGA
  Serial.println("VGA...");
  Mode monitor_res = vga.MODE400x300.custom(res_x, res_y);
  //vga.setFrameBufferCount(2);
  vga.init(monitor_res, redPin, greenPin, bluePin, hsyncPin, vsyncPin);
  vga.setFont(CodePage437_8x14);

  uint8_t y_steps;
#ifdef TRANS_FLAG
  y_steps = (res_y - 16) / 5;
  vga.fillRect(0, 0, res_x, 45, vga.RGB(0, 255, 255));
  vga.fillRect(0, y_steps * 1, res_x, y_steps, vga.RGB(255, 0, 225));
  vga.fillRect(0, y_steps * 2, res_x, y_steps, white);
  vga.fillRect(0, y_steps * 3, res_x, y_steps, vga.RGB(255, 0, 255));
  vga.fillRect(0, y_steps * 4, res_x, y_steps, vga.RGB(0, 255, 255));
  vga.setTextColor(black);
#endif
#ifdef LGBTQ_FLAG
  y_steps = (res_y - 16) / 6;
  vga.fillRect(0, 0, res_x, 45, vga.RGB(255, 0, 0));
  dith_rect(0, y_steps * 1, res_x, y_steps, vga.RGB(255, 0, 0), vga.RGB(255, 255, 0));
  vga.fillRect(0, y_steps * 2, res_x, y_steps, vga.RGB(255, 255, 0));
  vga.fillRect(0, y_steps * 3, res_x, y_steps, vga.RGB(0, 255, 0));
  vga.fillRect(0, y_steps * 4, res_x, y_steps, vga.RGB(0, 0, 255));
  dith_rect(0, y_steps * 5, res_x, y_steps, vga.RGB(255, 0, 255), black);
  vga.setTextColor(white);
#endif
#ifdef COLOR_BARS
  y_steps = (res_y - 16) / 6;
  vga.fillRect(0, 0, res_x, 45, vga.RGB(255, 0, 0));
  vga.fillRect(0, y_steps * 1, res_x, y_steps, vga.RGB(255, 255, 0));
  vga.fillRect(0, y_steps * 2, res_x, y_steps, vga.RGB(0, 255, 0));
  vga.fillRect(0, y_steps * 3, res_x, y_steps, vga.RGB(0, 255, 255));
  vga.fillRect(0, y_steps * 4, res_x, y_steps, vga.RGB(0, 0, 255));
  vga.fillRect(0, y_steps * 5, res_x, y_steps, vga.RGB(255, 0, 255));
  vga.setTextColor(white);
#endif

  delay(2000);

  //WiFi
  Serial.println("WiFi... ");
  vga.print("WiFi...");
  WiFi.hostname(hostname);
#ifdef USE_WM
  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  wm.setAPCallback(wm_ap_c);
  if (!wm.autoConnect(conf_ssid, conf_pass)) {
    ESP.restart();
  }
#else
  WiFi.mode(WIFI_STA);

#ifdef USE_STATIC_IP
  WiFi.config(static_ip, gateway, subnet, dns1, dns2);
#endif

  WiFi.begin(wifi_ssid, wifi_pass);
  while (!WiFi.isConnected()) {
    Serial.print('.');
    vga.print(".");
    delay(1000);
  }
#endif
  vga.println(" OK");
  vga.print("IP is ");
  vga.println(WiFi.localIP().toString().c_str());  //super hacky

  //NTP
  Serial.println("NTP... ");
  vga.print("NTP... ");
  ntp.begin();
  if (ntp.forceUpdate()) {
    Serial.println("OK");
    vga.println("OK");
  } else {
    Serial.println("ERROR");
    vga.println("ERROR");
  }

  //OTA
  Serial.println("OTA... ");
  vga.print("OTA... ");
  init_vga_ota();
  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.setPassword(ota_pass);
  ArduinoOTA.begin();
  vga.println("OK");

  //LittleFS
  Serial.println("LittleFS... ");
  vga.print("LittleFS... ");
  if (!LittleFS.begin(true)) {
    Serial.println("ERROR");
    vga.println("ERROR");
  } else vga.println("OK");

  //Read config
  Serial.println("Config... ");
  vga.print("Config... ");
  File lf = LittleFS.open(layout_path, FILE_READ);
  auto err = deserializeJson(layout, lf);
  lf.close();
  if (err != DeserializationError::Ok) {
    Serial.println("ERROR");
    vga.println("ERROR");
  } else vga.println("OK");


  //REST Server
  Serial.println("REST... ");
  vga.print("REST... ");
  setup_srv();
  srv.begin();
  vga.println("OK");

  // DDC/CI
  Serial.print("DDC... ");
  vga.print("DDC... ");
  Wire.begin(sdaPin, sclPin);
  if (ddc.begin()) {
    ddc.setPower(true);
    ddc.setSource(0x01);     //Set monitor to VGA-1 input
    ddc.setVCP(0x86, 0x02);  //Scaling: Max image, no aspect ration distortion
    ddc.setVCP(0xDC, 0x09);  //Display: Standard/Default mode with low power consumption
    Serial.println("OK");
    Serial.print("VCP Version ");
    Serial.println(ddc.getVCP(0xDF));
    vga.println("OK");
  } else {
    Serial.println("ERROR");
    vga.println("ERROR");
  }

  //webhook
  if (strlen(bootup_request_data_webhook) > 6) {
    HTTPClient http;
    Serial.print("Requesting current data... ");
    vga.print("Requesting current data... ");
#ifdef USE_HTTPS_WEBHOOK
    NetworkClientSecure* client = new NetworkClientSecure;
    client->setInsecure();
    http.begin(*client, bootup_request_data_webhook);
#else
    http.begin(bootup_request_data_webhook);
#endif
    const int code = http.POST(hostname);
    Serial.print(code);
    vga.print(code);
    if (code == HTTP_CODE_OK) {
      Serial.println(" OK");
      vga.println(" OK");
    } else {
      const char* err = http.errorToString(code).c_str();
      Serial.println(err);
      vga.println(err);
    }
    http.end();
    delete client;
  }

  vga.println("Waiting for data... ");
  digitalWrite(ledPin, HIGH);
}

void loop() {
  srv.handleClient();
  ntp.update();
  static uint64_t last_status_update = 0;
  if (millis() - last_status_update > 500) {
    last_status_update = millis();
    update_status_bar();
  }
  ArduinoOTA.handle();
  delay(5);
}
