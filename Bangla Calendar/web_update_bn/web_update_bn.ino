#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Video.h>
#include "BanglaTextPrinter.h"
#include "BanglaMNRegular_21pt.h"

// === VGA Pins ===
const int redPin = 25, greenPin = 14, bluePin = 13, hsyncPin = 32, vsyncPin = 33;
VGA3Bit videodisplay;

// === WiFi Access Point Credentials ===
const char* ssid = "ESP32_Bangla";
const char* password = "12345678";
WebServer server(80);

// === HTML Form Page ===
const char* htmlForm = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Bangla VGA Display</title>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Kalpurush, Arial; padding: 20px; text-align: center; }
    textarea { width: 90%; height: 100px; font-size: 20px; direction: rtl; }
    button { font-size: 18px; padding: 10px 20px; margin-top: 10px; }
  </style>
</head>
<body>
  <h2>Bangla Text to VGA</h2>
  <form action="/display" method="POST">
    <textarea name="msg" placeholder="এখানে বাংলা লিখুন..."></textarea><br>
    <button type="submit">Show on Display</button>
  </form>
</body>
</html>
)rawliteral";

// === Handle GET "/" ===
void handleRoot() {
  server.send(200, "text/html; charset=UTF-8", htmlForm);
}

// === Handle POST "/display" ===
void handleDisplay() {
  if (server.hasArg("msg")) {
    String banglaText = server.arg("msg");
    Serial.println("Received Bangla Text: " + banglaText);

    // Clear and display
    videodisplay.clear(videodisplay.RGB(0, 0, 0));
    BanglaPrinter::drawBanglaLine(
      banglaText.c_str(),
      50, 10,
      &BanglaMNRegular_21pt,
      videodisplay,
      21,
      255, 255, 255
    );
    videodisplay.show();

    server.send(200, "text/html; charset=UTF-8", "<h3>Displayed!</h3><a href='/'>Go Back</a>");
  } else {
    server.send(400, "text/plain", "Missing 'msg' field.");
  }
}

void setup() {
  Serial.begin(115200);

  // VGA Init
  videodisplay.init(VGAMode::MODE320x240, redPin, greenPin, bluePin, hsyncPin, vsyncPin);
  videodisplay.clear(videodisplay.RGB(0, 0, 0));
  videodisplay.show();

  // Start WiFi AP
  WiFi.softAP(ssid, password);
  Serial.print("ESP32 AP started. Connect to: ");
  Serial.println(ssid);
  Serial.print("Open in browser: http://");
  Serial.println(WiFi.softAPIP());

  // Web routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/display", HTTP_POST, handleDisplay);
  server.begin();
  Serial.println("Web server running.");
}

void loop() {
  server.handleClient();
}
