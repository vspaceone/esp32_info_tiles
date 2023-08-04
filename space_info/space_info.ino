/*
Compile with minimal SPIFFS Partition Scheme

Layout is grid of blocks. Each block has an icon and text. Status is shown as color and in text form.

Homeassistant sends states via REST. Uses jinja templating of homeassistant to insert states.
*/

#include <ESP32Lib.h>  //https://github.com/bitluni/ESP32Lib
#ifdef USE_WM
#include <WiFiManager.h>  //https://github.com/tzapu/WiFiManager/
#endif
#define ARDUINOJSON_DECODE_UNICODE 1
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

#include "config.h"
#include "sprites.h"

VGA3Bit vga;
WebServer srv;
DDCVCP ddc;
WiFiUDP ntpUDP;
NTPClient ntp(ntpUDP);

const char* conf_ssid = "space_info config AP";
const char* conf_pass = "PLEASE_change_me";

#include "stuff.h"

auto color_ok = vga.RGB(0, 255, 0);
auto color_bad = vga.RGB(255, 0, 255);
auto color_unknown = vga.RGB(255, 255, 255);
auto white = vga.RGB(255, 255, 255);
auto black = vga.RGB(0, 0, 0);

const int redPin = 12;
const int greenPin = 13;
const int bluePin = 15;
const int hsyncPin = 14;
const int vsyncPin = 2;
const int sdaPin = 16;
const int sclPin = 0;

#define BLOCK_SIZE 128  //each block is 128x128
String layout_path = "/layout.json";

DynamicJsonDocument layout(8192);

typedef typeof vga.RGB(0, 0, 0) vga_color_t;
vga_color_t st_to_col(uint8_t s) {
  switch (s) {
    default: return color_unknown; break;
    case 1: return color_bad; break;
    case 2: return color_ok; break;
  }
}

char info_text[32] = "";

void update_status_bar() {
  String status_str = ntp.getFormattedTime();
  status_str += " UTC ";
  status_str += WiFi.RSSI();
  status_str += "dBm ";
  uint16_t str_px_len = status_str.length() * 8;
  vga.setTextColor(white);
  vga.setCursor(0, 258);
  vga.fillRect(0, 257, str_px_len, 15, black);
  vga.print(status_str.c_str() );
  vga.fillRect(str_px_len + 1, 257, 384 - str_px_len, 15, black);
  vga.setCursor(str_px_len + 1, 258);
  vga.print(info_text);
}


/*
XY Block-Koordinaten,
Beschreibung 15x3 Zeichen,
Icon

{
  "test_block":{
    "x":2, "y":2,
    "desc": ["Test","Block",""],
    "icon": "ha"
  }
}
*/

bool draw_block(String name, uint8_t state) {
  Serial.print("Drawing block ");
  Serial.println(name);
  if (!layout.containsKey(name)) return false;
  if (!layout[name]["x"].is<uint8_t>() or !layout[name]["x"].is<uint8_t>()) return false;

  uint16_t x = layout[name]["x"].as<uint8_t>() * BLOCK_SIZE;
  uint16_t y = layout[name]["y"].as<uint8_t>() * BLOCK_SIZE;
  auto c = st_to_col(state);
  auto text_c = black;
  auto bg_c = c;

  //clear space and draw rect
  vga.fillRect(x + 1, y + 1, BLOCK_SIZE - 1, BLOCK_SIZE - 1, bg_c);
  vga.rect(x, y, BLOCK_SIZE, BLOCK_SIZE, text_c);
  //vga.rect(x+1, y+1, BLOCK_SIZE-2, BLOCK_SIZE-2, text_c);

  //Draw icon
  if (layout[name].containsKey("icon")) {
    uint8_t id = name_to_sprite[layout[name]["icon"]];
    if (state == 2) if (layout[name].containsKey("icon_on")) id = name_to_sprite[layout[name]["icon_on"]];
    //expecting 64x64 icons
    if (id < 255) sprites.drawMix(vga, id, x + 32, y + 2);
  }

  //Draw description
  vga.setTextColor(text_c);
  for (uint8_t l = 0; l < 3; l++) {
    const char* line = layout[name]["desc"][l];
    uint16_t line_x = x + 2 + BLOCK_SIZE / 2 - strlen(line) * 4;  //TODO, center text
    vga.setCursor(line_x, y + 80 + (l * 16));
    vga.print(line);
  }
  vga.setTextColor(white);

  Serial.println("done");
  return true;
}

uint8_t ha_to_st(String jval) {
  jval.toLowerCase();
  if (jval == "on") return 2;
  else if (jval == "off") return 1;
  else return 0;
}

/*
{ "binary_sensor.window_1: "on",
  "binary_sensor.window_2": off }
*/
void handle_state_update() {
  Serial.println(F("Incoming State Update"));
  if (srv.hasArg("plain")) {
    String json = srv.arg("plain");
    DynamicJsonDocument doc(4096);
    auto err = deserializeJson(doc, json);
    if (err == DeserializationError::Ok) {
      Serial.println(F("DeSer OK"));
      DynamicJsonDocument resp(1024);
      JsonObject root = doc.as<JsonObject>();
      for (JsonPair kv : root)  //draw all received blocks
        resp["success"][kv.key().c_str()] =
          draw_block(kv.key().c_str(), ha_to_st(kv.value()));
      uint16_t len = measureJson(resp) + 1;
      char resp_buf[len];
      serializeJson(resp, resp_buf, len);
      srv.send(200, "application/json", resp_buf);
    } else {
      String err_txt = "{\"error\":\"";
      err_txt += err.f_str();
      err_txt += "\"}";
      srv.send(400, "application/json", err_txt);
    }
  } else srv.send(400, "application/json", "{\"error\":\"no data\"}");
}

void handle_layout() {
  if (srv.hasArg("plain")) {
    String json = srv.arg("plain");
    auto err = deserializeJson(layout, json);
    if (err == DeserializationError::Ok) {
      File lf = LittleFS.open(layout_path, FILE_WRITE);
      lf.print(json);
      lf.close();
      srv.send(200, "application/json", "{\"success\":\"new layout set\"}");
    } else {
      String err_txt = "{\"error\":\"";
      err_txt += err.f_str();
      err_txt += "\"}";
      srv.send(400, "application/json", err_txt);
    }
  } else srv.send(400, "application/json", "{\"error\":\"no data\"}");
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  delay(10000);

  Serial.begin(115200);
  Serial.print("\nvspace.one Info-Monitor, compiled ");
  Serial.println(__DATE__);

  //VGA
  Serial.println("VGA...");
  Mode monitor_res = vga.MODE400x300.custom(384, 272);
  //vga.setFrameBufferCount(2);
  vga.init(monitor_res, redPin, greenPin, bluePin, hsyncPin, vsyncPin);
  vga.setFont(CodePage437_8x14);
  vga.clear(0);

  //WiFi
  Serial.println("WiFi... ");
  vga.print("WiFi...");
#ifdef USE_WM
  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  WiFi.hostname("space_info");
  wm.setAPCallback(wm_ap_c);
  if (!wm.autoConnect(conf_ssid, conf_pass)) {
    ESP.restart();
  }
#else
  WiFi.mode(WIFI_STA);
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

  //OTA
  Serial.println("NTP... ");
  vga.print("NTP... ");
  ntp.begin();
  if (ntp.forceUpdate()) {
    Serial.println("OK");
    vga.println("OK");
    vga.show();
  } else {
    Serial.println("ERROR");
    vga.println("ERROR");
    vga.show();
  }

  //OTA
  Serial.println("OTA... ");
  vga.print("OTA... ");
  init_vga_ota();
  ArduinoOTA.setHostname("space_info");
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
  srv.on("/", []() {
    String resp = "API Documentation at: [TODO]\nFree Heap: ";
    resp += ESP.getFreeHeap();
    resp += "\nRSSI: ";
    resp += WiFi.RSSI();
    srv.send(200, "text/plain", resp);
  });
  srv.on("/layout", HTTP_POST, handle_layout);
  srv.on("/layout", HTTP_GET, []() {
    uint32_t size = measureJson(layout) + 1;
    char buf[size];
    serializeJson(layout, buf, size);
    srv.send(200, "application/json", buf);
  });
  srv.on("/states", HTTP_POST, handle_state_update);
  srv.on("/power", HTTP_POST, []() {
    uint8_t val = ha_to_st(srv.arg("plain"));
    if (val == 0) srv.send(400, "text/plain", "bad format");
    else {
      ddc.setPower(val - 1);
      srv.send(200, "text/plain", "ok");
    }
  });
  srv.on("/brightness", HTTP_POST, []() {
    uint8_t brght = srv.arg("plain").toInt();
    ddc.setBrightness(brght);
    srv.send(200, "text/plain", "ok");
  });
  srv.begin();
  vga.println("OK");

  // DDC/CI
  Serial.print("DDC... ");
  vga.print("DDC... ");
  Wire.begin(sdaPin, sclPin);
  if (ddc.begin()) {
    Serial.println("OK");
    vga.println("OK");
    vga.show();
  } else {
    Serial.println("ERROR");
    vga.println("ERROR");
    vga.show();
  }

  vga.println("Waiting for data... ");
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
