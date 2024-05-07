std::map<String, uint8_t> name_to_sprite{
  { "none", 255 },
  { "soldering_iron", 12 },
  { "building_power", 11 },
  { "shutter_open", 10 },
  { "shutter", 9 },
  { "lightbulb_on", 8 },
  { "lightbulb", 7 },
  { "window_open", 6 },
  { "window", 5 },
  { "door_open", 4 },
  { "door_closed", 3 },
  { "door", 2 },
  { "dishwasher_alert", 1 },
  { "dishwasher", 0 }
};

std::map<ota_error_t, const char *> ota_err_to_str{
  { OTA_AUTH_ERROR, "Auth Failed" },
  { OTA_BEGIN_ERROR, "Begin Failed" },
  { OTA_CONNECT_ERROR, "Connect Failed" },
  { OTA_RECEIVE_ERROR, "Receive Failed" },
  { OTA_END_ERROR, "End Failed" }
};

enum state_id_t {
  STATE_UNKNOWN=0,
  STATE_BAD=1,
  STATE_OK=2
};

void init_vga_ota() {
  ArduinoOTA
    .onStart([]() {
      String ota_type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        ota_type = "sketch";
      else  // U_SPIFFS
        ota_type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      vga.clear(vga.RGB(0, 0, 255));
      vga.backColor = vga.RGB(0, 0, 255);
      vga.setTextColor(vga.RGB(255, 255, 255));
      vga.setCursor(1, 1);
      vga.println("Starting OTA...");
      vga.print("OTA Type: ");
      vga.println(ota_type.c_str());
      Serial.println("Start updating " + ota_type);
      delay(1000);
    })
    .onEnd([]() {
      vga.clear(vga.RGB(0, 255, 0));
      vga.backColor = vga.RGB(0, 255, 0);
      vga.setTextColor(vga.RGB(255, 255, 255));
      vga.setCursor(1, 1);
      vga.println("OTA Successful!");
      Serial.println("\nEnd");
      delay(1000);
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      static bool set_color = true;
      if (set_color) {
        vga.clear(vga.RGB(255, 0, 255));
        vga.backColor = vga.RGB(255, 0, 255);
        vga.setTextColor(vga.RGB(255, 255, 255));
      } else {
        vga.fillRect(1, 1, res_x, 8, vga.RGB(255, 0, 255));
      }
      set_color = false;
      vga.setCursor(1, 1);
      vga.print("OTA Download Progress: ");
      vga.print(progress / (total / 100));
      vga.println("%");

      vga.rect(0, 100, res_x, 10, vga.RGB(255, 255, 255));
      vga.fillRect(2, 102, map(progress / (total / 100), 0, 100, 2, 316), 6, vga.RGB(255, 255, 0));

      vga.setCursor(0, 112);
      vga.print("0%");
      vga.setCursor(res_x - (4 * 8) - 1, 112);
      vga.print("100%");

      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      vga.clear(vga.RGB(255, 0, 0));
      vga.backColor = vga.RGB(255, 0, 0);
      vga.setTextColor(vga.RGB(255, 255, 255));
      vga.setCursor(1, 1);
      vga.println("OTA ERROR!");
      vga.print(error);
      vga.print(" -> ");
      vga.println(ota_err_to_str[error]);

      Serial.printf("Error[%u]: ", error);
      Serial.println(ota_err_to_str[error]);

      delay(5000);
    });
}

#ifdef USE_WM
void wm_ap_c(WiFiManager *myWiFiManager) {
  Serial.println(F("WiFi not found. Please configure via AP."));
  vga.println("ERROR");
  vga.clear(vga.RGB(255, 0, 0));
  vga.backColor = vga.RGB(255, 0, 0);
  vga.setTextColor(vga.RGB(0, 0, 0));

  vga.println("Please configure WiFi via AP.");
  vga.print("SSID: ");
  vga.println(conf_ssid);
  vga.print("PASS: ");
  vga.println(conf_pass);
}
#endif