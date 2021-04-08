#include <TFT_eSPI.h>
#include <WiFi.h>
#include <Button2.h>
#include "secrets.h"

#ifndef TFT_DISPOFF
#define TFT_DISPOFF 0x28
#endif

#ifndef TFT_SLPIN
#define TFT_SLPIN   0x10
#endif

#define TFT_MOSI            19
#define TFT_SCLK            18
#define TFT_CS              5
#define TFT_DC              16
#define TFT_RST             23

#define TFT_BL              4
#define BUTTON_1            35
#define BUTTON_2            0

TFT_eSPI tft = TFT_eSPI(135, 240);
Button2 btn1(BUTTON_1);
Button2 btn2(BUTTON_2);

int activeScreen = 1;

void showTouch() { // TODO: Using touch?
  static uint64_t timeStamp = 0;
  if (millis() - timeStamp > 1000) {
    timeStamp = millis();
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Touch 2:" + String(touchRead(T2)),  tft.width() / 2, tft.height() / 2 );
  }
}

void wifi_init() {
  WiFi.begin(WIFI_SSID, WIFI_PASS); // From secrets.h
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void button_init() { // TODO: Using button2 library?
  btn1.setClickHandler([](Button2 & b) {
    activeScreen = 1;
    tft.fillScreen(TFT_BLACK);
  });
  btn1.setLongClickHandler([](Button2 & b) {
    activeScreen = 3;
  });

  btn2.setClickHandler([](Button2 & b) {
    activeScreen = 2;
  });
  btn2.setLongClickHandler([](Button2 & b) {
    activeScreen = 4;
  });
}

void button_loop() {
  btn1.loop();
  btn2.loop();
}

void setup() {
  Serial.begin(115200);
  Serial.println("Started.");

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(TFT_GREEN);
  tft.setCursor(0, 0);
  tft.setTextDatum(MC_DATUM);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  pinMode(BUTTON_1, INPUT);
  pinMode(BUTTON_2, INPUT);

  tft.setSwapBytes(true);
  tft.fillScreen(TFT_RED);

  wifi_init();
  tft.fillScreen(TFT_GREEN);

  //button_init();
}

void show_buttons() {
  int b1 = digitalRead(BUTTON_1);
  int b2 = digitalRead(BUTTON_2);
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Buttons:" + String(b1) + " " + String(b2),  tft.width() / 2, tft.height() / 2 );
}

void loop() {
//  if (activeScreen == 2) {
//    showTouch();
//  }
  //button_loop();
  show_buttons();
}
