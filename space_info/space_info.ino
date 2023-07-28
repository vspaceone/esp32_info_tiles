/*
Compile with minimal SPIFFS Partition Scheme

Layout is grid of blocks. Each block has an icon and text. Status is shown as color and in text form.

Homeassistant sends states via REST. Uses jinja templating of homeassistant to insert states.
*/

#include <ESP32Lib.h>  //https://github.com/bitluni/ESP32Lib
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <Ressources/CodePage437_8x14.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <map>
#include <LittleFS.h>
#include <Wire.h>
#include <DDCVCP.h>  //https://github.com/tttttx2/ddcvcp

#include "sprites.h"

VGA3Bit vga;
WebServer srv;
DDCVCP ddc;

const char* conf_ssid = "space_info config AP";
const char* conf_pass = "PLEASE_change_me";

#include "stuff.h"

auto color_ok = vga.RGB(0, 255, 255);
auto color_bad = vga.RGB(255, 0, 255);
auto color_unknown = vga.RGB(255, 255, 0);
auto white = vga.RGB(255, 255, 255);
auto black = vga.RGB(0,0,0);

const int redPin = 27;
const int greenPin = 26;
const int bluePin = 25;
const int hsyncPin = 12;
const int vsyncPin = 14;
const int sdaPin = 33;
const int sclPin = 32;

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

void draw_block(String name, uint8_t state) {
  if (!layout.containsKey(name)) return;

  uint16_t x = layout[name]["x"].as<uint8_t>() * BLOCK_SIZE;
  uint16_t y = layout[name]["y"].as<uint8_t>() * BLOCK_SIZE;
  auto c = st_to_col(state);

  //clear space and draw rect
  vga.fillRect(x+1, y+1, BLOCK_SIZE-1, BLOCK_SIZE-1, black);
  vga.rect(x, y, BLOCK_SIZE, BLOCK_SIZE, c);

  //Draw icon
  if (layout[name].containsKey("icon")) {
    uint8_t id = name_to_sprite[layout[name]["icon"]];
    //expecting 64x64 icons
    if (id < 255) sprites.drawMix(vga, id, x + 32, y + 2);
  }

  //Draw description
  vga.setTextColor(c);
  for (uint8_t l = 0; l < 3; l++) {
    vga.setCursor(x, y + 80 + (l * 16));
    const char* line = layout[name]["desc"][l];
    vga.print(line);
  }
  vga.setTextColor(white);

  vga.show();
}

/*
{ "binary_sensor.window_1: "on",
  "binary_sensor.window_2": off }
*/
void handle_state_update() {
  if (srv.hasArg("plain")) {
    String json = srv.arg("plain");
    DynamicJsonDocument doc(4096);
    auto err = deserializeJson(layout, json);
    if (err == DeserializationError::Ok) {

      JsonObject root = doc.as<JsonObject>();
      for (JsonPair kv : root) {
        String jval = kv.value();
        jval.toLowerCase();
        uint8_t val;
        if (jval == "on") val = 2;
        else if (jval == "off") val = 1;
        else val = 0;
        draw_block(kv.key().c_str(), val);
      }

      srv.send(200, "application/json", "{\"success\":\"states set\"}");
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
  Serial.begin(115200);
  Serial.print("\nvspace.one Info-Monitor, compiled ");
  Serial.println(__DATE__);

  //VGA
  Serial.println("VGA...");
  Mode monitor_res = vga.MODE800x600.custom(640, 512);
  //vga.setFrameBufferCount(2);
  vga.init(monitor_res, redPin, greenPin, bluePin, hsyncPin, vsyncPin);
  vga.setFont(CodePage437_8x14);
  vga.clear(0);

  //WiFi
  Serial.println("WiFi... ");
  vga.print("WiFi... ");
  vga.show();
  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  WiFi.hostname("space_info");
  wm.setAPCallback(wm_ap_c);
  if (!wm.autoConnect(conf_ssid, conf_pass)) {
    ESP.restart();
  }
  vga.println("OK");
  vga.print("IP is ");
  vga.println(WiFi.localIP());
  vga.show();

  //OTA
  Serial.println("OTA... ");
  vga.print("OTA... ");
  vga.show();
  init_vga_ota();
  ArduinoOTA.setHostname("space_info");
  ArduinoOTA.begin();
  vga.println("OK");
  vga.show();

  //LittleFS
  Serial.println("LittleFS... ");
  vga.print("LittleFS... ");
  vga.show();
  if (!LittleFS.begin(true)) {
    Serial.println("ERROR");
    vga.println("ERROR");
  } else vga.println("OK");
  vga.show();

  //Read config
  Serial.println("Config... ");
  vga.print("Config... ");
  vga.show();
  File lf = LittleFS.open(layout_path, FILE_READ);
  auto err = deserializeJson(layout, lf);
  lf.close();
  if (err != DeserializationError::Ok) {
    Serial.println("ERROR");
    vga.println("ERROR");
  } else vga.println("OK");
  vga.show();


  //REST Server
  Serial.println("REST... ");
  vga.print("REST... ");
  vga.show();
  srv.on("/", []() {
    srv.send(200, "text/plain", "API Documentation at: [TODO]");
  });
  srv.on("/layout", HTTP_POST, handle_layout);
  srv.on("/layout", HTTP_GET, []() {
    uint32_t size = measureJson(layout) + 1;
    char buf[size];
    serializeJson(layout, buf, size);
    srv.send(200, "application/json", buf);
  });
  srv.on("/layout", HTTP_POST, handle_state_update);
  srv.begin();
  vga.println("OK");
  vga.show();

  // DDC/CI
  Serial.print("DDC... ");
  vga.print("DDC... ");
  vga.show();
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
  vga.show();
}

void loop() {
  srv.handleClient();
  ArduinoOTA.handle();
  delay(5);
}
