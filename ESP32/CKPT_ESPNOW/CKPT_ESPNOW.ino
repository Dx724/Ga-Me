#include <TFT_eSPI.h>
#include <WiFi.h>
#include <Button2.h>
#include "secrets.h"
#include "common.h"
#include <esp_now.h>

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
#define BUTTON_1            0
#define BUTTON_2            35

#define SCREEN_WIDTH 135
#define SCREEN_HEIGHT 240
#define FIELD_WIDTH (SCREEN_WIDTH * 3)

// TODO: Define roles
#define BOARD_ROLE 1

// OTHER_MAC_A will be directly to left if possible
// OTHER_MAC_B will be directly to right if possible
#if BOARD_ROLE == 1
const uint8_t OWN_MAC[] = MAC1;
const uint8_t OTHER_MAC_A[] = MAC3;
const uint8_t OTHER_MAC_B[] = MAC2;
#elif BOARD_ROLE == 2
const uint8_t OWN_MAC[] = MAC2;
const uint8_t OTHER_MAC_A[] = MAC1;
const uint8_t OTHER_MAC_B[] = MAC3;
#elif BOARD_ROLE == 3
const uint8_t OWN_MAC[] = MAC3;
const uint8_t OTHER_MAC_A[] = MAC1;
const uint8_t OTHER_MAC_B[] = MAC2;
#endif

enum c_state {
  idle,
  game
};

enum msg_type {
  ctrl,
  btn
};

c_state this_state = idle;

struct ball {
  double x;
  double y;
  int vel_x;
  int vel_y;
};

struct ball translate_coords(struct ball *global_ball) {
  struct ball local_ball = *global_ball;
  local_ball.x = global_ball->x - (SCREEN_WIDTH * (BOARD_ROLE - 1));
  return local_ball;
}

typedef struct struct_msg {
  int btn1;
  int btn2;
  struct ball ball;
  msg_type type;
} struct_msg;

struct_msg msg_out, msg_in_a, msg_in_b;

TFT_eSPI tft = TFT_eSPI(135, 240);
Button2 btn1(BUTTON_1);
Button2 btn2(BUTTON_2);

int activeScreen = 1;

void data_send_callback(const uint8_t *mac, esp_now_send_status_t status) {
  if (status != ESP_NOW_SEND_SUCCESS) {
    Serial.println("ESP NOW SEND FAILED!\n");
  }
}

bool mac_equals(const uint8_t *mac1, const uint8_t *mac2) {
  for (int i = 0; i < 6; i++) {
    if (mac1[i] != mac2[i])
      return false;
  }
  return true;
}

void data_recv_callback(const uint8_t *mac, const uint8_t *data_in, int len) {
  struct_msg *msg_dest = mac_equals(OTHER_MAC_A, mac) ? &msg_in_a : &msg_in_b;
  memcpy(msg_dest, data_in, sizeof(*msg_dest));
  Serial.println("Received message!");
  Serial.println("B1: " + String(msg_dest->btn1));
  Serial.println("B2: " + String(msg_dest->btn2));
  switch (msg_dest->type) {
    case ctrl:
      memcpy(&msg_out.ball, &msg_dest->ball, sizeof(msg_out.ball));
      break;
    case btn:
      break;
    default:
      Serial.print("Invalid message type ");
      Serial.println(msg_dest->type);
      break;
  }
}

void showTouch() { // TODO: Using touch?
  static uint64_t timeStamp = 0;
  if (millis() - timeStamp > 1000) {
    timeStamp = millis();
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Touch 2:" + String(touchRead(T2)),  tft.width() / 2, tft.height() / 2 );
  }
}

void wifi_init() { // TODO: Need to connect to network? May interfere with ESP NOW performance
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

bool enow_init() {
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Initialization Failed");
    return false;
  }
  esp_now_register_send_cb(data_send_callback);
  esp_now_register_recv_cb(data_recv_callback);
  return true;
}

bool add_peer(const uint8_t *mac) {
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, mac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("ESP-NOW Peer Add Failed");
    return false;
  }
  return true;
}

void game_init() {
  msg_out.ball = {
    .x = (SCREEN_WIDTH * 3.0)/2.0,
    .y = (SCREEN_HEIGHT)/2.0,
    .vel_x = 5,
    .vel_y = 2
  };
  this_state = game;
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
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON); // Set in the TFT_eSPI library configuration

  pinMode(BUTTON_1, INPUT);
  pinMode(BUTTON_2, INPUT);

  tft.setSwapBytes(true);
  tft.fillScreen(TFT_RED);

  if (enow_init() && add_peer(OTHER_MAC_A) && add_peer(OTHER_MAC_B))
    tft.fillScreen(TFT_GREEN);

  //button_init();
}

void do_send(const uint8_t *mac) {
  esp_err_t result = esp_now_send(mac, (uint8_t *) &msg_out, sizeof(msg_out));
  if (result != ESP_OK)
    Serial.println("Data send error.");
}

void do_broadcast() {
  do_send(OTHER_MAC_A);
  do_send(OTHER_MAC_B);
}

void show_buttons() {
  static uint64_t timeStamp = 0;
  if (millis() - timeStamp > 1000) {
    timeStamp = millis();
    int b1 = !digitalRead(BUTTON_1); // Buttons are normally-closed push buttons, so invert
    int b2 = !digitalRead(BUTTON_2);
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Buttons:" + String(b1) + " " + String(b2),  tft.width() / 2, tft.height() / 2 );
    tft.drawString("Btn_A:" + String(msg_in_a.btn1) + " " + String(msg_in_a.btn2),  tft.width() / 2, tft.height() / 4 );
    tft.drawString("Btn_B:" + String(msg_in_b.btn1) + " " + String(msg_in_b.btn2),  tft.width() / 2, 3 * tft.height() / 4 );

    msg_out.btn1 = b1;
    msg_out.btn2 = b2;
    msg_out.type = btn;
    do_broadcast();
    delay(1000);
  }
}

#define GAME_TICK 10.0

void transfer_control(int direction) {
  msg_out.type = ctrl;
  switch (direction) {
    case 1:
      do_send(OTHER_MAC_B);
      break;
    case -1:
      do_send(OTHER_MAC_A);
      break;
    default:
      Serial.print("Invalid transfer direction ");
      Serial.println(direction);
      break;
  }
}

#define BALL_RADIUS 3

void game_loop() {
  struct ball *the_ball = &msg_out.ball;
  the_ball->x += the_ball->vel_x / GAME_TICK;
  the_ball->y += the_ball->vel_y / GAME_TICK;
  if (the_ball->y > SCREEN_HEIGHT) {
    the_ball->vel_y *= -1;
    the_ball->y = SCREEN_HEIGHT - (the_ball->y - SCREEN_HEIGHT);
  }
  else if (the_ball->y < 0) {
    the_ball->vel_y *= -1;
    the_ball->y = -the_ball->y;
  }
  struct ball local_ball = translate_coords(the_ball);
  if (local_ball.x > SCREEN_WIDTH) {
    if (BOARD_ROLE < 3) {
      transfer_control(1);
    }
    else {
      the_ball->vel_x *= -1;
      the_ball->x = FIELD_WIDTH - (the_ball->x - FIELD_WIDTH);
    }
  }
  else if (local_ball.x < 0) {
    if (BOARD_ROLE > 1) {
      transfer_control(-1);
    }
    else {
      the_ball->vel_x *= -1;
      the_ball->x = -the_ball->x;
    }
  }

  // Then draw
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(local_ball.x, local_ball.y, BALL_RADIUS, BALL_RADIUS, TFT_WHITE);
}

void loop() {
//  if (activeScreen == 2) {
//    showTouch();
//  }
  //button_loop();
  switch (this_state) {
    case idle:
      show_buttons();
      break;
    case game:
      game_loop();
      break;
  }
  delay(GAME_TICK);
}
