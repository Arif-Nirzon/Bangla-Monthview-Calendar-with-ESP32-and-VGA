#include <Wire.h>
#include "RTClib.h"
#include <ESP32Video.h>
#include "BanglaTextPrinter.h"
#include "BanglaMNRegular_21pt.h"
#include "BanglaMNRegular_25pt.h"
#include "Kalpurush_20pt.h"

// Fonts
#include <Ressources/CodePage437_8x14.h>
#include <Ressources/CodePage437_8x16.h>
#include <Ressources/CodePage437_8x19.h>
#include <Ressources/Font6x8.h>

// VGA Pins
const int redPin = 25, greenPin = 14, bluePin = 13, hsyncPin = 32, vsyncPin = 33;
VGA3Bit videodisplay;

// RTC
const int sda = 21, scl = 22;
RTC_DS3231 rtc;

// === Global calendar values ===
int current_date = 15, current_month = 3, current_year = 1432;
int current_hour = 9, current_min = 46, current_sec = 32;
int startDay = 0, daysInMonth = 30;
const char* ampm = "AM";

// === Bangla data ===
const char* banglaWeekdaysFull[7] = {"রবিবার", "সোমবার", "মঙ্গলবার", "বুধবার", "বৃহস্পতিবার", "শুক্রবার", "শনিবার"};
const char* banglaWeekdays[7]     = {"র", "সো", " ম", "বু", "বৃ", "শু", "শ"};
const char* banglaMonths[12]      = {"বৈশাখ", "জ্যৈষ্ঠ", "আষাঢ়", "শ্রাবণ", "ভাদ্র", "আশ্বিন", "কার্তিক", "অগ্রহায়ণ", "পৌষ", "মাঘ", "ফাল্গুন", "চৈত্র"};
int banglaMonthLengths[12] = {31, 31, 31, 31, 31, 30, 30, 30, 30, 30, 28, 30};

// === Utilities ===
int getDaysInMonth(int month, int year) {
  if (month == 2) return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 29 : 28;
  if (month == 4 || month == 6 || month == 9 || month == 11) return 30;
  return 31;
}

int getStartDayOfBanglaMonth(int banglaMonthIndex, int gregorianYear) {
  // Fixed civic calendar used in Bangladesh
  const int fixedStarts[12][2] = {
    {14, 4},  // Boishakh
    {15, 5},  // Joishtho
    {15, 6},  // Ashar
    {16, 7},  // Srabon
    {16, 8},  // Bhadro
    {16, 9},  // Ashshin
    {17, 10}, // Kartik
    {16, 11}, // Ogrohayon
    {16, 12}, // Poush
    {15, 1},  // Magh (next Gregorian year)
    {0, 2},   // Falgun — set below
    {15, 3}   // Chaitro
  };

  int day, month, year;

  if (banglaMonthIndex == 10) {
    // Falgun special case: Feb 13 if leap year, else Feb 14
    bool isLeap = (gregorianYear % 4 == 0 && gregorianYear % 100 != 0) || (gregorianYear % 400 == 0);
    day = isLeap ? 13 : 14;
    month = 2;
    year = gregorianYear + 1;
  } else if (banglaMonthIndex == 11) {
    // Chaitro starts in next Gregorian year
    day = 15;
    month = 3;
    year = gregorianYear + 1;
  } else if (banglaMonthIndex == 9) {
    // Magh also starts in next Gregorian year
    day = fixedStarts[banglaMonthIndex][0];
    month = fixedStarts[banglaMonthIndex][1];
    year = gregorianYear + 1;
  } else {
    day = fixedStarts[banglaMonthIndex][0];
    month = fixedStarts[banglaMonthIndex][1];
    year = gregorianYear;
  }

  DateTime dt(year, month, day);
  return dt.dayOfTheWeek(); // 0 = Sunday ... 6 = Saturday
}


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
  char buffer[6];  // Enough for 5 digits + null terminator
  sprintf(buffer, "%0*d", width, num);  // Pad with leading zeros

  String result = "";
  for (char c : String(buffer)) {
    result += banglaNums[c - '0'];
  }
  return result;
}

const char* getBanglaDayPart(int hour24) {
  if (hour24 >= 4 && hour24 < 6)   return "ভোর";   // 4AM–6AM
  if (hour24 >= 6 && hour24 < 12)  return "সকাল";  // 6AM–12PM
  if (hour24 >= 12 && hour24 < 15) return "দুপুর";   // 12PM–3PM
  if (hour24 >= 15 && hour24 < 17) return "বিকাল";  // 3PM–5PM
  if (hour24 >= 17 && hour24 < 19) return "সন্ধ্যা";   // 5PM–7PM
  return "রাত";                                     // 7PM–4AM
}


// === Display functions ===
void drawHeader(int hour, int minute, int second, int weekday) {
  videodisplay.fillRect(0, 0, 320, 35, videodisplay.RGB(0, 0, 0));
  String period = getBanglaDayPart(rtc.now().hour());
  String timeStr = String(period) + " " + toBanglaDigits(hour) + ":" + toBanglaDigitsPadded(minute) + ":" + toBanglaDigitsPadded(second);
  String headerStr = String(banglaWeekdaysFull[weekday]) + " - " + timeStr;
  BanglaPrinter::drawBanglaLine(headerStr.c_str(), 10, 10, &BanglaMNRegular_25pt, videodisplay, 25, 255, 255, 255);
}

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
  int x_off = 12;
  int y_off = 100;
  int day = 1;

  // Move x_off to align with the starting day of the week
  for (int i = 0; i < startDay; i++) {
    x_off += 40;
  }

  for (int i = 0; i < daysInMonth; i++) {
    int r = 255, g = 255, b = 255;

    // Calculate the weekday for this date cell
    int weekday = (startDay + i) % 7;

    // Make Thursday (4) and Friday (5) red
    if (weekday == 4 || weekday == 5) {
      r = 255; g = 0; b = 0;
    }

    // Highlight current day (override color)
    if (day == highlightedDay) {
      r = 0; g = 255; b = 255;
    }

    String banglaNum = toBanglaDigits(day++);
    BanglaPrinter::drawBanglaLine(banglaNum.c_str(), y_off, x_off, &Kalpurush_20pt, videodisplay, 20, r, g, b);

    x_off += 40;
    if ((i + startDay + 1) % 7 == 0) {
      y_off += 25;
      x_off = 12;
    }
  }
}

// === RTC date update ===
void updateDateTime() {
  DateTime now = rtc.now();
  int gYear = now.year(), gMonth = now.month(), gDay = now.day();
  int hour = now.hour();

  current_hour = hour % 12;
  if (current_hour == 0) current_hour = 12;
  current_min = now.minute();
  current_sec = now.second();
  ampm = (hour >= 12) ? "PM" : "AM";

  if (hour < 6) {
    gDay--;
    if (gDay <= 0) {
      gMonth--;
      if (gMonth <= 0) { gMonth = 12; gYear--; }
      gDay = getDaysInMonth(gMonth, gYear);
    }
  }

  bool isLeap = (gYear % 4 == 0 && gYear % 100 != 0) || (gYear % 400 == 0);
  banglaMonthLengths[10] = isLeap ? 29 : 28;

  int offsetDays[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  int gDaysSinceStart = offsetDays[gMonth - 1] + gDay;
  if (isLeap && gMonth > 2) gDaysSinceStart += 1;

  int banglaStartDay = offsetDays[3] + 14;
  int daysSinceBanglaNewYear = gDaysSinceStart - banglaStartDay;

  int bYear = gYear - 593;
  if (daysSinceBanglaNewYear < 0) {
    bYear--;
    daysSinceBanglaNewYear += (isLeap ? 366 : 365);
  }

  int bMonth = 0, bDay = daysSinceBanglaNewYear + 1;
  while (bDay > banglaMonthLengths[bMonth]) {
    bDay -= banglaMonthLengths[bMonth];
    bMonth++;
  }

  current_year = bYear;
  current_month = bMonth + 1;
  current_date = bDay;
  daysInMonth = banglaMonthLengths[bMonth];
  startDay = getStartDayOfBanglaMonth(current_month - 1, now.year());

}

// === Setup and Loop ===
unsigned long lastUpdate = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin(sda, scl);
  rtc.begin();
  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  //rtc.adjust(DateTime(2025, 7, 30, 15, 21, 0));  // year, month, day, hour, min, sec

  videodisplay.init(VGAMode::MODE320x240, redPin, greenPin, bluePin, hsyncPin, vsyncPin);
  videodisplay.clear(videodisplay.RGB(0, 0, 0));

  updateDateTime();  // initial draw
  drawHeader(current_hour, current_min, current_sec, rtc.now().dayOfTheWeek());
  drawMonth(current_month - 1, current_year);
  drawWeekdayLabels();
  drawCalendarDates(current_date);
  videodisplay.show();
}

// Store previous values to detect change
int prevSec = -1;
int prevDate = -1;
int prevMonth = -1;

void loop() {
  updateDateTime();  // Always update from RTC
  // Update time only if seconds changed
  if (current_sec != prevSec) {
    drawHeader(current_hour, current_min, current_sec, rtc.now().dayOfTheWeek());
    prevSec = current_sec;
  }

  // Redraw calendar only if date/month changed
  if (current_date != prevDate || current_month != prevMonth) {
    videodisplay.clear(videodisplay.RGB(0, 0, 0));

    drawHeader(current_hour, current_min, current_sec, rtc.now().dayOfTheWeek());
    drawMonth(current_month - 1, current_year);  // Bangla months are 0-indexed
    drawWeekdayLabels();
    drawCalendarDates(current_date);

    prevDate = current_date;
    prevMonth = current_month;
  }
}

