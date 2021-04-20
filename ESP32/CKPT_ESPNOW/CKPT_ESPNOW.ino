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
  game,
  over
};

enum msg_type {
  ctrl,
  btn
};

c_state this_state = idle;

#define PADDLE_WIDTH 12
#define PADDLE_HEIGHT 35
#define PADDLE_VEL 7

struct paddle {
  double y;
  double last_y;
};

struct ball { // This represents not just the ball but the gamestate of that entire ballgame
  double x;
  double y;
  int vel_x;
  int vel_y;
  struct paddle p_left;
  struct paddle p_right;
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

// Ordered messages
#if BOARD_ROLE == 1
const struct_msg *ord_msg[3] = {&msg_out, &msg_in_b, &msg_in_a};
#elif BOARD_ROLE == 2
const struct_msg *ord_msg[3] = {&msg_in_a, &msg_out, &msg_in_b};
#elif BOARD_ROLE == 3
const struct_msg *ord_msg[3] = {&msg_in_a, &msg_in_b, &msg_out};
#endif

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

void on_control() {
  this_state = game;
  tft.fillScreen(TFT_BLACK);
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
      on_control();
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
    .vel_x = random(3,9)*(random(0,2) == 0 ? -1 : 1),
    .vel_y = random(2,6)*(random(0,2) == 0 ? -1 : 1),
    .p_left = {
      .y = (SCREEN_HEIGHT)/2.0,
      .last_y = (SCREEN_HEIGHT)/2.0,
    },
    .p_right = {
      .y = (SCREEN_HEIGHT)/2.0,
      .last_y = (SCREEN_HEIGHT)/2.0,
    }
  };
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

  if (BOARD_ROLE == 2) {
    game_init();
    on_control();
  }
  //button_init();

  
  // TESTING CODE
  game_init();
  msg_out.ball.vel_x = -2;
  msg_out.ball.x = 45;
  on_control();
  
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

void update_input() {
  int b1 = !digitalRead(BUTTON_1); // Buttons are normally-closed push buttons, so invert
  int b2 = !digitalRead(BUTTON_2);
  char should_send = (msg_out.btn1 ^ b1) | (msg_out.btn2 ^ b2);
  msg_out.btn1 = b1;
  msg_out.btn2 = b2;
  if (should_send) {
    msg_out.type = btn;
    do_broadcast();
  } 
}

/*
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

    char should_send = (msg_out.btn1 ^ b1) | (msg_out.btn2 ^ b2);
    msg_out.btn1 = b1;
    msg_out.btn2 = b2;
    if (should_send) {
      msg_out.type = btn;
      do_broadcast();
    }    
  }
}
*/

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
  tft.fillScreen(random(0xFFFF));
  this_state = idle;
}

void doublep_clamp(double *v, double low, double high) {
  if (*v > high)
    *v = high;
  else if (*v < low)
    *v = low;
}

bool paddle_hits(struct paddle *p, struct ball *b) { // Don't account for ball radius here to allow for ball to "clip through" the paddle corners
  return p->y <= b->y && (p->y + PADDLE_HEIGHT) >= b->y;
}

#define BALL_RADIUS 5

#define CLEAR_EXTRA 5
#define PADDLE_THIN 5

void game_loop() {
  // Ball updates  
  struct ball *the_ball = &msg_out.ball;
  struct ball prev_ball = translate_coords(the_ball);
  
  the_ball->x += the_ball->vel_x / GAME_TICK;
  the_ball->y += the_ball->vel_y / GAME_TICK;
  if (the_ball->y > SCREEN_HEIGHT - BALL_RADIUS) {
    the_ball->vel_y *= -1;
    the_ball->y = (SCREEN_HEIGHT - BALL_RADIUS) - (the_ball->y - (SCREEN_HEIGHT - BALL_RADIUS));
  }
  else if (the_ball->y < BALL_RADIUS) {
    the_ball->vel_y *= -1;
    the_ball->y = BALL_RADIUS - (the_ball->y - BALL_RADIUS);
  }

  struct ball local_ball = translate_coords(the_ball);
  if (local_ball.x > SCREEN_WIDTH) { // Transfer control at center, not boundary of ball
    if (BOARD_ROLE < 3) {
      transfer_control(1);
      return;
    }
    else {
      /*
      the_ball->vel_x *= -1;
      the_ball->x = FIELD_WIDTH - (the_ball->x - FIELD_WIDTH);
      */
      on_over();
    }
  }
  else if (BOARD_ROLE == 3 && the_ball->y > SCREEN_WIDTH - PADDLE_WIDTH - BALL_RADIUS) {
    if (paddle_hits(&the_ball->p_right, the_ball)) {
      the_ball->vel_x *= -1;
      the_ball->x = (FIELD_WIDTH - PADDLE_WIDTH - BALL_RADIUS) - (the_ball->x - (FIELD_WIDTH - PADDLE_WIDTH - BALL_RADIUS));
    }
  }
  else if (local_ball.x < 0) {
    if (BOARD_ROLE > 1) {
      transfer_control(-1);
      return;
    }
    else {
      /*
      the_ball->vel_x *= -1;
      the_ball->x = -the_ball->x;
      */
      on_over();
    }
  }
  else if (local_ball.x < PADDLE_WIDTH + BALL_RADIUS) {
    if (paddle_hits(&the_ball->p_left, the_ball)) {
      the_ball->vel_x *= -1;
      the_ball->x = (PADDLE_WIDTH + BALL_RADIUS) - (the_ball->x - (PADDLE_WIDTH + BALL_RADIUS));
    }
  }

  // Paddle updates
  the_ball->p_left.y += PADDLE_VEL * (ord_msg[0]->btn1 - ord_msg[0]->btn2) / GAME_TICK;
  the_ball->p_right.y += PADDLE_VEL * (ord_msg[2]->btn1 - ord_msg[2]->btn2) / GAME_TICK;

  doublep_clamp(&the_ball->p_left.y, 0, SCREEN_HEIGHT - PADDLE_HEIGHT);
  doublep_clamp(&the_ball->p_right.y, 0, SCREEN_HEIGHT - PADDLE_HEIGHT);

  // Then draw
  //tft.fillScreen(TFT_BLACK);
  tft.drawCircle(prev_ball.x, prev_ball.y, BALL_RADIUS, TFT_BLACK); // Just erase an outline to reduce flicker
  tft.fillCircle(local_ball.x, local_ball.y, BALL_RADIUS, TFT_WHITE);

  int pd_width = BOARD_ROLE == 1 ? PADDLE_WIDTH : PADDLE_THIN;
  int pd_pos = 0;

  // Clear only differential region to prevent screen flicker
  // (there isn't enough RAM for a full screen buffer)
  double l_diff = the_ball->p_left.y - the_ball->p_left.last_y;
  if (l_diff > 0) {
    tft.fillRect(pd_pos, the_ball->p_left.last_y, pd_width, l_diff + CLEAR_EXTRA, TFT_BLACK);
  }
  else if (l_diff < 0) {
    tft.fillRect(pd_pos, the_ball->p_left.y + PADDLE_HEIGHT, pd_width, -l_diff + CLEAR_EXTRA, TFT_BLACK);
  }
  tft.fillRect(pd_pos, the_ball->p_left.y, pd_width, PADDLE_HEIGHT, TFT_WHITE);

  pd_width = BOARD_ROLE == 3 ? PADDLE_WIDTH : PADDLE_THIN;
  pd_pos = SCREEN_WIDTH - pd_width;

  double r_diff = the_ball->p_right.y - the_ball->p_right.last_y;
  if (r_diff > 0) {
    tft.fillRect(pd_pos, the_ball->p_right.last_y, pd_width, r_diff + CLEAR_EXTRA, TFT_BLACK);
  }
  else if (r_diff < 0) {
    tft.fillRect(pd_pos, the_ball->p_right.y + PADDLE_HEIGHT, pd_width, -r_diff + CLEAR_EXTRA, TFT_BLACK);
  }
  tft.fillRect(pd_pos, the_ball->p_right.y, pd_width, PADDLE_HEIGHT, TFT_WHITE);

  the_ball->p_left.last_y = the_ball->p_left.y;
  the_ball->p_right.last_y = the_ball->p_right.y;
}

void over_loop() {
  tft.fillScreen(TFT_RED);
}

void idle_loop() {
  static int i = 0;
  static double r = 0.1;
  tft.drawPixel(SCREEN_WIDTH / 2 * (1+r*cos(i*PI/180.0)), SCREEN_HEIGHT / 2 * (1+r*sin(i*PI/180.0)), random(0xFFFF));
  i++;
  r *= 1.01;
  if (r > sqrt(2.1))
    r = 0.1;
}

void on_over() {
  if (BOARD_ROLE == 2)
    Serial.println("ERROR: OVER ON CENTER");
  game_init();
  transfer_control(BOARD_ROLE == 1 ? 1 : -1);
  this_state = over;
}

void loop() {
//  if (activeScreen == 2) {
//    showTouch();
//  }
  //button_loop();
  switch (this_state) {
    case idle:
      idle_loop();
      break;
    case game:
      game_loop();
      break;
    case over:
      over_loop();
      break;
  }
  update_input();
  delay(GAME_TICK);
}
