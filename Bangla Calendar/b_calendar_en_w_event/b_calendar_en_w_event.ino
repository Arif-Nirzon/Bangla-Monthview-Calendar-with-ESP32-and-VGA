#include <ESP32Video.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include "RTClib.h"

// Fonts
#include <Ressources/CodePage437_8x14.h>
#include <Ressources/CodePage437_8x16.h>
#include <Ressources/CodePage437_8x19.h>
#include <Ressources/Font6x8.h>

// VGA Pins
const int redPin = 25, greenPin = 14, bluePin = 13, hsyncPin = 32, vsyncPin = 33;  // vsy-yello     hsy-orange
VGA3Bit videodisplay;

// RTC Pins
const int sda = 21, scl = 22;
RTC_DS3231 rtc;

// global offset for Calendar print
const int calendarYOffset = 20; // adjust as needed (try 10–20 px)


// Date-Time Globals
/*const char* months[] = {"Invalid", "January", "February", "March", "April", "May", "June",
                        "July", "August", "September", "October", "November", "December"};
const char* daysOfWeek[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};*/


// ==== Globals ====
const char* daysOfWeek[] = {
  "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};

const char* Months[] = {
  "Boishakh", "Joishtho", "Ashar", "Srabon", "Bhadro", "Ashshin",
  "Kartik", "Ogrohayon", "Poush", "Magh", "Falgun", "Chaitro"
};

int current_date, current_month, current_year;
int current_hour, current_min, current_sec;
const char* ampm = "AM";
const char* current_day_str = "Monday";
int startDay = 0, daysInMonth = 30;
bool showDate = true;
unsigned long lastToggle = 0;
const int blinkInterval = 500;
int prevSec = -1, prevDate = -1, prevMonth = -1;

const char* current_month_str;



// ==== Event and Holiday Handling ====
struct Holiday {
  int day;
  int month;
  const char* name;
};

Holiday holidayList[20] = {
  {6, 7, "Mohorom"},
};
int totalHolidays = 1;

bool needRedraw = true;

// ==== WiFi & WebServer ====
const char* ssid = "ESP32_UI";
const char* password = "12345678";
WebServer server(80);

const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Event Scheduler</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial; text-align: center; margin-top: 50px; }
    input[type=number], input[type=text] {
      width: 200px; padding: 10px; margin: 5px;
    }
    button { padding: 10px 20px; margin: 10px; }
  </style>
</head>
<body>
  <h2>Add Holiday/Event</h2>
  <form action="/submit" method="POST">
    <input type="number" name="day" min="1" max="31" placeholder="Day" required><br>
    <input type="number" name="month" min="1" max="12" placeholder="Month" required><br>
    <input type="text" name="event" placeholder="Event Name" required><br>
    <button type="submit">Save</button>
  </form>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

void handleSubmit() {
  int d = server.arg("day").toInt();
  int m = server.arg("month").toInt();
  String ev = server.arg("event");

  if (totalHolidays < 20) {
    holidayList[totalHolidays++] = {d, m, strdup(ev.c_str())};
    needRedraw = true;
  }

  String response = "<h3>Saved!</h3><p>Date: " + String(d) + "/" + String(m) +
                    "<br>Event: " + ev + "</p><a href='/'>Back</a>";
  server.send(200, "text/html", response);
}

int banglaMonthLengths[12] = {31, 31, 31, 31, 31, 30, 30, 30, 30, 30, 28, 30};

// ==== Helpers ====
int getDaysInMonth(int month, int year) {
  if (month == 2) return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 29 : 28;
  if (month == 4 || month == 6 || month == 9 || month == 11) return 30;
  return 31;
}

int getStartDayOfBanglaMonth(const DateTime& now) {
  int weekdayToday = now.dayOfTheWeek();
  int start = (weekdayToday - (current_date - 1)) % 7;
  if (start < 0) start += 7;
  return start;
}



// ==== Main Update Function ====
void updateDateTime() {
  DateTime now = rtc.now();

  int gYear = now.year();
  int gMonth = now.month();
  int gDay = now.day();
  int hour = now.hour();

  current_day_str = daysOfWeek[now.dayOfTheWeek()];
  current_hour = hour % 12;
  if (current_hour == 0) current_hour = 12;
  current_min = now.minute();
  current_sec = now.second();
  ampm = (hour >= 12) ? "PM" : "AM";

  if (hour < 6) {
    gDay--;
    if (gDay <= 0) {
      gMonth--;
      if (gMonth <= 0) {
        gMonth = 12;
        gYear--;
      }
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

  int bMonth = 0;
  int bDay = daysSinceBanglaNewYear + 1;
  while (bDay > banglaMonthLengths[bMonth]) {
    bDay -= banglaMonthLengths[bMonth];
    bMonth++;
  }

  current_year = bYear;
  current_month = bMonth + 1;  // 1-based
  current_date = bDay;
  current_month_str = Months[bMonth];
  daysInMonth = banglaMonthLengths[bMonth];
  startDay = getStartDayOfBanglaMonth(now);
}


// ==== Calendar Logic ====
/*int getDaysInMonth(int month, int year) {
  if (month == 2) return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 29 : 28;
  if (month == 4 || month == 6 || month == 9 || month == 11) return 30;
  return 31;
}

int getStartDay(int year, int month) {
  if (month < 3) { month += 12; year--; }
  int K = year % 100, J = year / 100;
  int h = (1 + (13*(month + 1))/5 + K + K/4 + J/4 + 5*J) % 7;
  return (h + 6) % 7;
}

void updateDateTime() {
  DateTime now = rtc.now();
  current_year = now.year();
  current_month = now.month();
  current_date = now.day();
  current_day_str = daysOfWeek[now.dayOfTheWeek()];
  current_hour = now.hour() % 12; if (current_hour == 0) current_hour = 12;
  current_min = now.minute();
  current_sec = now.second();
  ampm = (now.hour() >= 12) ? "PM" : "AM";
  daysInMonth = getDaysInMonth(current_month, current_year);
  startDay = getStartDay(current_year, current_month);
}*/

bool isHoliday(int date) {
  for (int i = 0; i < totalHolidays; i++)
    if (holidayList[i].month == current_month && holidayList[i].day == date) return true;

  int dow = (startDay + date - 1) % 7;
  return (dow == 4 || dow == 5); // Thu, Fri
}

void drawDate(int date, bool visible, int offset) {
  videodisplay.setFont(CodePage437_8x16);
  int x = 0 + ((date - 1 + offset) % 7) * 3;
  //int y =5 + ((date - 1 + offset) / 7);          //                                 5   
  //videodisplay.setCursor(x * 8, y * 16);
  int baseY = 64 + calendarYOffset;
  int y = baseY + ((date - 1 + offset) / 7) * 16;  // correct: row * font height
  videodisplay.setCursor(x * 8, y);               // ✅ pixel-level y value

  char buf[4]; sprintf(buf, date < 10 ? " %d" : "%d", date);

  if (!visible)
    videodisplay.setTextColor(videodisplay.RGB(0,0,0), videodisplay.RGB(0,0,0));
  else if (date == current_date)
    videodisplay.setTextColor(videodisplay.RGB(0,255,255), videodisplay.RGB(0,0,0));
  else if (isHoliday(date))
    videodisplay.setTextColor(videodisplay.RGB(255,0,0), videodisplay.RGB(0,0,0));
  else
    videodisplay.setTextColor(videodisplay.RGB(255,255,255), videodisplay.RGB(0,0,0));

  videodisplay.print(buf);
}

void show_date(int current_date, int startDayOffset, int daysInMonth) {
  for (int date = 1; date <= daysInMonth; date++) {
    if (date == current_date) continue;
    drawDate(date, true, startDayOffset);
  }
}

void show_weekdays(const char* days[7]) {
  videodisplay.setFont(CodePage437_8x16);
  for (int i = 0; i < 7; i++) {
    if (i == 4 || i == 5)
      videodisplay.setTextColor(videodisplay.RGB(255,0,0), videodisplay.RGB(0,0,0));
    else
      videodisplay.setTextColor(videodisplay.RGB(255,255,255), videodisplay.RGB(0,0,0));
    videodisplay.print(days[i]); videodisplay.print(" ");
  }
  videodisplay.println();
}

void show_holiday_list(int currMonth) {
  videodisplay.setFont(CodePage437_8x14);
  int x = 184, y = 5, line = 0;

  videodisplay.setCursor(x, y + line++ * 10);
  videodisplay.setTextColor(videodisplay.RGB(255, 0, 0));
  videodisplay.println("----------------");
  videodisplay.setCursor(x, y + line++ * 10); videodisplay.println(" Holidays/Events");
  videodisplay.setCursor(x, y + line++ * 10); videodisplay.println("   This Month   ");
  videodisplay.setCursor(x, y + line++ * 10); videodisplay.println("----------------");

  for (int i = 0; i < totalHolidays; i++) {
    if (holidayList[i].month == currMonth) {
      char buf[40];
      sprintf(buf, "%d - %s", holidayList[i].day, holidayList[i].name);
      videodisplay.setCursor(x, y + line++ * 10);
      videodisplay.setTextColor(videodisplay.RGB(255, 255, 255));
      videodisplay.println(buf);
    }
  }
}

void show_day(const char* day) {
  videodisplay.setFont(CodePage437_8x19);
  videodisplay.setCursor(0, 0 + calendarYOffset);
  videodisplay.setTextColor(videodisplay.RGB(255,255,255));
  videodisplay.print(day);
}

/*void show_time(int hh, int mm, int ss, const char* ampm) {
  videodisplay.setFont(CodePage437_8x19);
  
  // Clear time area
  videodisplay.setCursor(80, 0); // Time starts after the day
  videodisplay.setTextColor(videodisplay.RGB(0,0,0)); // Black to erase
  videodisplay.print("            "); // Enough spaces to cover previous text
  
  // Now draw the time
  videodisplay.setCursor(80, 0);
  char buf[12];
  sprintf(buf, "-%02d:%02d:%02d%s", hh, mm, ss, ampm);
  videodisplay.setTextColor(videodisplay.RGB(255,255,255));
  videodisplay.print(buf);
}*/
void show_time(int hh, int mm, int ss, const char* ampm) {
  videodisplay.setFont(CodePage437_8x19);

  // Set cursor to where time is displayed
  videodisplay.setCursor(80, 0 + calendarYOffset);

  // Prepare buffer
  char buf[16];
  sprintf(buf, "-%02d:%02d:%02d%s", hh, mm, ss, ampm);

  // Set text and background color to overwrite old time
  videodisplay.setTextColor(videodisplay.RGB(255, 255, 255), videodisplay.RGB(0, 0, 0));
  videodisplay.print(buf);
}

void show_month_year(const char* month, int year) {
  videodisplay.setFont(CodePage437_8x16);
  videodisplay.setCursor(0, 32 + calendarYOffset);
  char buf[16]; sprintf(buf, "%s %d", month, year);
  videodisplay.setTextColor(videodisplay.RGB(255,255,255));
  videodisplay.println(buf);
}

// ==== Setup & Loop ====
void setup() {
  Serial.begin(115200);
  Wire.begin(sda, scl);
  rtc.begin();
  if (rtc.lostPower()) rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  videodisplay.init(VGAMode::MODE320x200, redPin, greenPin, bluePin, hsyncPin, vsyncPin);
  videodisplay.line(180, 0, 180, 199, videodisplay.RGB(255, 255, 255));

  WiFi.softAP(ssid, password);
  server.on("/", handleRoot);
  server.on("/submit", HTTP_POST, handleSubmit);
  server.begin();
  Serial.println("Web UI ready.");
}

void loop() {
  server.handleClient();
  updateDateTime();

  if (current_sec != prevSec) {
    show_day(current_day_str);
    show_time(current_hour, current_min, current_sec, ampm);
    prevSec = current_sec;
  }

  if (current_date != prevDate || current_month != prevMonth || needRedraw) {
    videodisplay.clear(videodisplay.RGB(0, 0, 0));
    videodisplay.line(180, 0, 180, 199, videodisplay.RGB(255, 255, 255));

    show_day(current_day_str);
    show_time(current_hour, current_min, current_sec, ampm);
    show_month_year(Months[current_month-1], current_year);//                           changed only current_monts

    const char* days[7] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
    show_weekdays(days);
    show_date(current_date, startDay, daysInMonth);
    show_holiday_list(current_month);

    prevDate = current_date;
    prevMonth = current_month;
    needRedraw = false;
  }

  if (millis() - lastToggle >= blinkInterval) {
    showDate = !showDate;
    drawDate(current_date, showDate, startDay);
    lastToggle = millis();
  }
}
