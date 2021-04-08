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

void showTouch() {
  static uint64_t timeStamp = 0;
  if (millis() - timeStamp > 1000) {
    timeStamp = millis();
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Touch 2:" + String(touchRead(T2)),  tft.width() / 2, tft.height() / 2 );
  }
}

void button_init() {
  btn1.setClickHandler([](Button2 & b) {
    activeScreen = 1;
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

  tft.setSwapBytes(true);
  tft.fillScreen(TFT_BLUE);

  button_init();
}

void loop() {
  if (activeScreen == 2) {
    showTouch();
  }
  button_loop();
}
