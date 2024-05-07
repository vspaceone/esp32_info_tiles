uint8_t ha_to_st(String jval) {
  jval.toLowerCase();
  if (jval == "on") return STATE_OK;
  else if (jval == "off") return STATE_BAD;
  else return STATE_UNKNOWN;
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
      for (JsonPair kv : root)  //draw all received tiles
        resp["success"][kv.key().c_str()] =
          draw_tile(kv.key().c_str(), ha_to_st(kv.value()));
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

void setup_srv() {
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
    srv.send(200, "application/json; charset=ibm437", buf);
  });
  srv.on("/states", HTTP_POST, handle_state_update);
  srv.on("/power", HTTP_POST, []() {
    uint8_t val = ha_to_st(srv.arg("plain"));
    if (val == STATE_UNKNOWN) srv.send(400, "text/plain", "bad format");
    else {
      ddc.setPower(val == STATE_OK);
      srv.send(200, "text/plain", "ok");
    }
  });
  srv.on("/brightness", HTTP_POST, []() {
    uint8_t brght = srv.arg("plain").toInt();
    ddc.setBrightness(brght);
    srv.send(200, "text/plain", "ok");
  });
  srv.on("/text", HTTP_POST, []() {
    String text = srv.arg("plain");
    if (text.length() < 32) {
      strcpy(info_text, text.c_str());
      draw_status_bar_itext();
      srv.send(200, "text/plain", "ok");
    } else srv.send(400, "text/plain", "too long");
  });
}