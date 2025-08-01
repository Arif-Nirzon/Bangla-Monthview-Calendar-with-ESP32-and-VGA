#include <Wire.h>
#include <ESP32Video.h>
#include "BanglaTextPrinter.h"
#include "BanglaMNRegular_21pt.h"
#include "BanglaMNRegular_25pt.h"
#include "Kalpurush_20pt.h"
#include <Ressources/CodePage437_8x14.h>
#include <Ressources/CodePage437_8x16.h>
#include <Ressources/CodePage437_8x19.h>
#include <Ressources/Font6x8.h>
#include <RTClib.h>  // For DateTime

// VGA Pins
const int redPin = 25, greenPin = 14, bluePin = 13, hsyncPin = 32, vsyncPin = 33;
VGA3Bit videodisplay;

// === Bangla Calendar Data ===
const char* banglaWeekdaysFull[7] = {"রবিবার", "সোমবার", "মঙ্গলবার", "বুধবার", "বৃহস্পতিবার", "শুক্রবার", "শনিবার"};
const char* banglaWeekdays[7]     = {"র", "সো", " ম", "বু", "বৃ", "শু", "শ"};
const char* banglaMonths[12]      = {"বৈশাখ", "জ্যৈষ্ঠ", "আষাঢ়", "শ্রাবণ", "ভাদ্র", "আশ্বিন", "কার্তিক", "অগ্রহায়ণ", "পৌষ", "মাঘ", "ফাল্গুন", "চৈত্র"};
int banglaMonthLengths[12]        = {31, 31, 31, 31, 31, 30, 30, 30, 30, 30, 28, 30};

// Gregorian start dates for Bangla months in 2025 (Bangladesh)
const int banglaMonthStartDates2025[12][3] = {
  {14, 4, 2025},  // Boishakh
  {15, 5, 2025},  // Joishtho
  {15, 6, 2025},  // Ashar
  {16, 7, 2025},  // Srabon
  {16, 8, 2025},  // Bhadro
  {16, 9, 2025},  // Ashshin
  {17, 10, 2025}, // Kartik
  {16, 11, 2025}, // Ogrohayon
  {16, 12, 2025}, // Poush
  {15, 1, 2026},  // Magh
  {14, 2, 2026},  // Falgun
  {15, 3, 2026}   // Chaitro
};

// Calendar state
int current_year = 1432;
int current_month_index = 0;
int daysInMonth = banglaMonthLengths[0];
int startDay = 0;
unsigned long lastUpdate = 0;

// === Utility Functions ===
String toBanglaDigits(int num) {
  const char* banglaNums[] = {"০", "১", "২", "৩", "৪", "৫", "৬", "৭", "৮", "৯"};
  String result = "";
  String temp = String(num);
  for (char c : temp) {
    if (c >= '0' && c <= '9') result += banglaNums[c - '0'];
    else result += c;
  }
  return result;
}

String toBanglaDigitsPadded(int num, int width = 2) {
  const char* banglaNums[] = {"০", "১", "২", "৩", "৪", "৫", "৬", "৭", "৮", "৯"};
  char buffer[6];
  sprintf(buffer, "%0*d", width, num);
  String result = "";
  for (char c : String(buffer)) {
    result += banglaNums[c - '0'];
  }
  return result;
}

int getBanglaMonthStartWeekday(int monthIndex) {
  int day = banglaMonthStartDates2025[monthIndex][0];
  int month = banglaMonthStartDates2025[monthIndex][1];
  int year = banglaMonthStartDates2025[monthIndex][2];
  DateTime dt(year, month, day);
  return dt.dayOfTheWeek();  // 0 = Sunday, ..., 6 = Saturday
}

// === Display ===
void drawMonth(int monthIndex, int year) {
  String monthYearStr = String(banglaMonths[monthIndex]) + " " + toBanglaDigits(year);
  BanglaPrinter::drawBanglaLine(monthYearStr.c_str(), 40, 10, &Kalpurush_20pt, videodisplay, 20, 255, 255, 255);
}

void drawWeekdayLabels() {
  int x_off = 10;
  for (int i = 0; i < 7; i++) {
    int r = 255, g = (i == 4 || i == 5) ? 0 : 255, b = (i == 4 || i == 5) ? 0 : 255;
    int y_offset = (i == 3) ? 74 : (i == 4) ? 76 : 70;
    BanglaPrinter::drawBanglaLine(banglaWeekdays[i], y_offset, x_off, &Kalpurush_20pt, videodisplay, 20, r, g, b);
    x_off += 40;
  }
}

void drawCalendarDates(int highlightedDay) {
  int x_off = 12, y_off = 100, day = 1;
  for (int i = 0; i < startDay; i++) x_off += 40;

  for (int i = 0; i < daysInMonth; i++) {
    int weekday = (startDay + i) % 7;
    int r = 255, g = 255, b = 255;

    if (weekday == 4 || weekday == 5) { r = 255; g = 0; b = 0; }
    if (day == highlightedDay) { r = 0; g = 255; b = 255; }

    String banglaNum = toBanglaDigits(day++);
    BanglaPrinter::drawBanglaLine(banglaNum.c_str(), y_off, x_off, &Kalpurush_20pt, videodisplay, 20, r, g, b);
    x_off += 40;
    if ((i + startDay + 1) % 7 == 0) {
      y_off += 25;
      x_off = 12;
    }
  }
}

// === Setup ===
void setup() {
  Serial.begin(115200);
  videodisplay.init(VGAMode::MODE320x240, redPin, greenPin, bluePin, hsyncPin, vsyncPin);
  videodisplay.clear(videodisplay.RGB(0, 0, 0));

  // Initial draw
  startDay = getBanglaMonthStartWeekday(current_month_index);
  daysInMonth = banglaMonthLengths[current_month_index];

  drawMonth(current_month_index, current_year);
  drawWeekdayLabels();
  drawCalendarDates(1);
}

// === Loop ===
void loop() {
  unsigned long now = millis();
  if (now - lastUpdate >= 3000) {  // every 3 seconds
    lastUpdate = now;

    // Cycle to next month
    current_month_index++;
    if (current_month_index >= 12) {
      current_month_index = 0;
    }

    // Update calendar data
    startDay = getBanglaMonthStartWeekday(current_month_index);
    daysInMonth = banglaMonthLengths[current_month_index];

    // Redraw screen
    videodisplay.clear(videodisplay.RGB(0, 0, 0));
    drawMonth(current_month_index, current_year);
    drawWeekdayLabels();
    drawCalendarDates(1); // highlight 1st
  }
}
