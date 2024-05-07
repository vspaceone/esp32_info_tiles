typedef typeof vga.RGB(0, 0, 0) vga_color_t;
vga_color_t st_to_col(uint8_t s) {
  switch (s) {
    default: return color_unknown; break;
    case STATE_BAD: return color_bad; break;
    case STATE_OK: return color_ok; break;
  }
}

void dith_rect(int16_t x, int16_t y, int16_t w, int16_t h, vga_color_t c1, vga_color_t c2) {
  for (int16_t line = y; line < h + y; line++)
    for (int16_t col = x; col < w + x; col++)
      vga.dot(col, line, (line /*xor col*/) & 0x01 ? c1 : c2);
}

void draw_status_bar_itext() {
  vga.fillRect(info_text_x, res_y - 15, res_x - info_text_x, 15, black);
  vga.setCursor(info_text_x, res_y - 14);
  vga.print(info_text);
}

void update_status_bar() {
  String status_str = ntp.getFormattedTime();
  status_str += " UTC ";
  status_str += WiFi.RSSI();
  status_str += "dBm ";
  uint16_t str_px_len = status_str.length() * 8;
  vga.setTextColor(white);
  vga.setCursor(0, res_y - 14);
  vga.fillRect(0, res_y - 15, str_px_len, 15, black);
  vga.print(status_str.c_str());
  if (info_text_x != str_px_len) {  //only redraw when needed
    info_text_x = str_px_len;
    draw_status_bar_itext();
  }
}


/*
XY Kachel-Koordinaten,
Beschreibung 15x3 Zeichen,
Icon

{
  "test_tile":{
    "x":2, "y":2,
    "desc": ["Test","Kachel",""],
    "icon": "lightbulb_on",
    "icon_ok": "lightbulb"
  }
}
*/

bool draw_tile(String name, uint8_t state) {
  Serial.print("Drawing tile ");
  Serial.println(name);
  if (!layout.containsKey(name)) return false;
  if (!layout[name]["x"].is<uint8_t>() or !layout[name]["x"].is<uint8_t>()) return false;

  uint16_t x = layout[name]["x"].as<uint8_t>() * TILE_SIZE;
  uint16_t y = layout[name]["y"].as<uint8_t>() * TILE_SIZE;
  vga_color_t c = st_to_col(state);
  vga_color_t text_c = black;
  vga_color_t bg_c = c;

  //clear space and draw rect
  vga.fillRect(x + 1, y + 1, TILE_SIZE - 1, TILE_SIZE - 1, bg_c);
  vga.rect(x, y, TILE_SIZE, TILE_SIZE, text_c);
  //vga.rect(x+1, y+1, TILE_SIZE-2, TILE_SIZE-2, text_c);

  //Draw icon
  if (layout[name].containsKey("icon")) {
    uint8_t id = name_to_sprite[layout[name]["icon"]];
    if (state == STATE_OK)
      if (layout[name].containsKey("icon_ok")) id = name_to_sprite[layout[name]["icon_ok"]];
    //expecting 64x64 icons
    if (id < 255) sprites.drawMix(vga, id, x + 32, y + 2);
  }

  //Draw description
  vga.setTextColor(text_c);
  for (uint8_t l = 0; l < 3; l++) {
    const char* line_unkn = layout[name]["desc"][l];
    const char* line_bad = layout[name]["desc_bad"][l];
    const char* line_ok = layout[name]["desc_ok"][l];
    char* line = const_cast<char*>(line_unkn);
    if (state == STATE_OK and layout[name].containsKey("desc_ok")) line = const_cast<char*>(line_ok);
    else if (state == STATE_BAD and layout[name].containsKey("desc_bad")) line = const_cast<char*>(line_bad);

    uint16_t line_x = x + 2 + TILE_SIZE / 2 - strlen(line) * 4;  //TODO, center text
    vga.setCursor(line_x, y + 80 + (l * 16));
    vga.print(line);
  }
  vga.setTextColor(white);

  Serial.println("done");
  return true;
}
