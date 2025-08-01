#ifndef BANGLA_TEXT_PRINTER_H
#define BANGLA_TEXT_PRINTER_H

#include "BanglaText.h"
#include <ESP32Video.h>

namespace BanglaPrinter {
  // Internal drawing state
  int x_offset = 0;
  int y_offset = 0;
  int color = 0;
  int text_height = 0;
  VGA3Bit* vga = nullptr;

  // Drawing callback (no global videodisplay use!)
  void draw_pixel(int16_t x, int16_t y) {
    vga->dot(x_offset + y, y_offset + (text_height - 1 - x), color);
  }

  // Width measurement dummy
  int max_x = 0;
  void measure_pixel_width(int16_t x, int16_t y) {
    if (x > max_x) max_x = x;
  }

  // Public function to draw a Bangla line
  void drawBanglaLine(const char* text, int y, int x, const BTfont* font, VGA3Bit& display, int font_height, int r = 255, int g = 255, int b = 255)
 {
    x_offset = x;
    y_offset = y;
    color = display.RGB(r, g, b);
    text_height = font_height;
    vga = &display;

    max_x = 0;
    BanglaTextRenderer renderer(const_cast<BTfont*>(font));
    renderer.renderText(text, measure_pixel_width);
    renderer.renderText(text, draw_pixel);
  }
}

#endif
