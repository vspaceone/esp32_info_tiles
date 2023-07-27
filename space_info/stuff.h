std::map<String, uint8_t> name_to_sprite{
  { "none", 255},
  { "building_power", 3},
  { "window", 2},
  { "door", 1},
  { "dishwasher", 0}
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
      vga.show();
      Serial.println("Start updating " + ota_type);
      delay(1000);
    })
    .onEnd([]() {
      vga.clear(vga.RGB(0, 255, 0));
      vga.backColor = vga.RGB(0, 255, 0);
      vga.setTextColor(vga.RGB(255, 255, 255));
      vga.setCursor(1, 1);
      vga.println("OTA Successful!");
      vga.show();
      Serial.println("\nEnd");
      delay(1000);
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      static bool set_color = false;
      if (!set_color) {
        vga.clear(vga.RGB(255, 0, 255));
        vga.backColor = vga.RGB(255, 0, 255);
        vga.setTextColor(vga.RGB(255, 255, 255));
      } else {
        vga.fillRect(1, 1, 320, 8, vga.RGB(255, 0, 255));
      }
      set_color = true;
      vga.setCursor(1, 1);
      vga.print("OTA Download Progress: ");
      vga.print(progress / (total / 100));
      vga.println("%");

      vga.rect(0, 100, 320, 10, vga.RGB(255, 255, 255));
      vga.fillRect(2, 102, map(progress / (total / 100), 0, 100, 2, 316), 6, vga.RGB(255, 255, 0));

      vga.setCursor(0, 112);
      vga.print("0%");
      vga.setCursor(284, 112);
      vga.print("100%");

      vga.show();

      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      vga.clear(vga.RGB(255, 0, 0));
      vga.backColor = vga.RGB(255, 0, 0);
      vga.setTextColor(vga.RGB(255, 255, 255));
      vga.setCursor(1, 1);
      vga.println("OTA ERROR!");
      vga.println(error);
      if (error == OTA_AUTH_ERROR) vga.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) vga.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) vga.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) vga.println("Receive Failed");
      else if (error == OTA_END_ERROR) vga.println("End Failed");

      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");

      vga.show();

      delay(5000);
    });
}

void wm_ap_c(WiFiManager *myWiFiManager) {
  Serial.print(F("WiFi not found. Please configure via AP."));
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
