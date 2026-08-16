#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSans9pt7b.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

const char* ssid = "PLN";
const char* password = "12345678";

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 23400;
const int daylightOffset_sec = 0;

void setup() {
  Serial.begin(115200);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED failed");
    while (true);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setFont();
  display.setTextSize(1);
  display.setCursor(12, 28);
  display.println("Connecting WiFi...");
  display.display();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(20, 28);
  display.println("WiFi Connected!");
  display.display();
  delay(1000);
}

void loop() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return;
  }

  char timeHHMM[6];
  char timeAmpm[3];
  char timeSec[3];
  char dateString[20];

  strftime(timeHHMM, sizeof(timeHHMM), "%I:%M", &timeinfo);
  strftime(timeAmpm, sizeof(timeAmpm), "%p", &timeinfo);
  strftime(timeSec, sizeof(timeSec), "%S", &timeinfo);
  strftime(dateString, sizeof(dateString), "%a, %b %d", &timeinfo);

  for (int i = 0; dateString[i]; i++) {
    dateString[i] = toupper(dateString[i]);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setFont(&FreeSansBold18pt7b);
  display.setCursor(2, 32);
  display.print(timeHHMM);

  display.setFont(&FreeSans9pt7b);
  display.setCursor(96, 16);
  display.print(timeAmpm);

  display.setFont();
  display.setCursor(98, 24);
  display.print(timeSec);

  display.drawFastHLine(6, 40, 116, SSD1306_WHITE);

  display.setFont(&FreeSans9pt7b);

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(dateString, 0, 0, &x1, &y1, &w, &h);

  int16_t xPos = (SCREEN_WIDTH - w) / 2 - x1;
  display.setCursor(xPos, 58);
  display.print(dateString);

  display.display();
  delay(1000);
}