/*  Target Control System base station firmware v1.0
**  All design files are here - https://github.com/PeakeElectronicInnovation/Target-Control-System-v1.0
**  Custom control board based on the Microsoft uf2 bootloader, using the ATSAMD51J20A Arm Cortex M4 MCU
**  This version finalised on 13th August 2023 - By J Peake (peakeElectronicInnovation@gmail.com)
**  
**  This software uses opensource libraries to function. Special thanks to Adafruit for their great GFX and
**  ILI9341 display drivers (LCD displays used in this system are supplied by Adafruit). Also thanks to the
**  RF24 library creators, Khoi Hoang and his SAMD_TimerInterrupt library, and Steve Marple for his RTCx lib
*/

// All definitions, included files, global objects and vaiables are specified in defines.h
#include "defines.h"

//********************************************************************//
//--------------------------------Setup-------------------------------//
//********************************************************************//

void setup() {
  pins_setup();

  nvm_flash_read();

  Serial.begin(250000);
  if (debug) {
    while (!Serial)
      ;  //Wait for serial
    Serial.println("Starting TC system in debug mode");
  }

  peripherals_setup();
}

//********************************************************************//
//--------------------------------Loop--------------------------------//
//********************************************************************//

void loop() {
  if (second_flag) sec_flag_handler();

  if (screen_menu == HOME_SCREEN) {
    if (!target_timer_running && timer_button_pressed()) timer_button_handler();
    if (start_pressed) start_timer();
    if (cancel_pressed) cancel_timer();
    if (timer_ended) change_timer_phase();
    if (target_timer_second_flag || target_timer_half_second_flag) draw_timer(target_timer_seconds_left);
  }

  if (ctp.touched()) touch_screen_handler();

  if (is_paired || pairing) nrf_check();
}

//********************************************************************//
//------------------------------Functions-----------------------------//
//********************************************************************//

//----------------------ISR functions----------------------//

// General 10ms timer overflow interrupt
void timer_ISR(void) {
  timer_general_10ms_inc++;
  if ((timer_general_10ms_inc % 100) == 0) second_flag = true;
  if (target_timer_running) {
    timer_targets_10ms_inc++;
    float dec_val = 0.5;
    if (custom_timer_is_int || (timer_set_ptr != CUSTOM_TIMER_PTR) || (target_timer_phase != SHOOT_TIMER_PHASE)) dec_val = 1.0;
    if ((timer_targets_10ms_inc % (int)(dec_val * 100)) == 0) {
      if (debug) {
        Serial.print("dec_val = ");
        Serial.print(dec_val);
        Serial.print(" timer_val = ");
        Serial.println(target_timer_seconds_left);
      }
      if (target_timer_seconds_left <= 0.0) {
        timer_ended = true;
        if (debug) Serial.println("timer phase ended");
        return;
      }
      target_timer_seconds_left -= dec_val;
      if ((timer_targets_10ms_inc % 100) == 0) target_timer_second_flag = true;
      else if (target_timer_seconds_left < 10.0) target_timer_half_second_flag = true;
    }
  }
}

// Start button pressed interrupt
void start_ISR(void) {
  if (target_timer_running) return;
  else start_pressed = true;
}

// Cancel button pressed interrupt
void cancel_ISR(void) {
  if (!target_timer_running) return;
  else cancel_pressed = true;
}

//-------------------Main loop functions-------------------//

// handler function for 1s general timer flag
void sec_flag_handler(void) {
  second_flag = false;
  draw_time();
  if (custom_timer_changed_timestamp > 0) {
    if ((custom_timer_changed_timestamp + NVM_WRITE_DELAY_MS) < millis()) {
      nvm_flash_save();
      custom_timer_changed_timestamp = 0;
    }
  }
}

// Check button status, return true if a button was pressed
bool timer_button_pressed(void) {
  for (int i = 0; i < 16; i++) {
    if (!digitalRead(button_input[i])) {
      if (timer_set_ptr != i) {
        timer_set_ptr = i;
        return true;
      } else return false;
    }
  }
  return false;
}

// Start timed target cycle
void start_timer(void) {
  start_pressed = false;
  timer_targets_10ms_inc = 0;
  target_timer_running = true;
  set_outputs(UNFACED);
  target_face = false;
  target_timer_phase = START_DELAY_PHASE;
  target_timer_seconds_left = START_DELAY_SECONDS - 1;
  draw_timer(target_timer_seconds_left);
  leds.setPixelColor(timer_set_ptr, RED);
  leds.show();
  draw_targets(START_DELAY_SECONDS);
}

// Move to next phase of timed target cycle (faced fire, edge stop, faced score)
void change_timer_phase(void) {
  target_timer_phase++;
  timer_targets_10ms_inc = 0;
  switch (target_timer_phase) {

    case SHOOT_TIMER_PHASE:
      timer_ended = false;
      for (int i = 0; i < 3; i++)
        set_outputs(FACING);
      target_face = true;
      target_timer_seconds_left = timer_val[timer_set_ptr];
      if (custom_timer_is_int || (timer_set_ptr != CUSTOM_TIMER_PTR) || (target_timer_phase != SHOOT_TIMER_PHASE)) target_timer_seconds_left -= 1.0;
      else target_timer_seconds_left -= 0.5;
      draw_timer(target_timer_seconds_left);
      leds.setPixelColor(timer_set_ptr, GREEN);
      leds.show();
      draw_targets(timer_val[timer_set_ptr]);
      break;

    case END_DELAY_PHASE:
      timer_ended = false;
      for (int i = 0; i < 3; i++)
        set_outputs(UNFACED);
      target_face = false;
      target_timer_seconds_left = END_DELAY_SECONDS - 1;
      draw_timer(target_timer_seconds_left);
      leds.setPixelColor(timer_set_ptr, RED);
      leds.show();
      draw_targets(END_DELAY_SECONDS);
      break;

    case TIMER_INACTIVE:
      stop_timer();
      break;

    default:
      stop_timer();
      break;
  }
}

// Stop a running phase and return to normal state
void cancel_timer(void) {
  cancel_pressed = false;
  target_timer_running = false;
  timer_targets_10ms_inc = 0;
  target_timer_seconds_left = 0;
  for (int i = 0; i < 3; i++)
    set_outputs(FACING);
  target_face = true;
  draw_targets(0);
  leds.setPixelColor(timer_set_ptr, AMBER);
  leds.show();
  draw_timer(timer_val[timer_set_ptr]);
}

// Stop timed target run after completion
void stop_timer(void) {
  timer_ended = false;
  target_timer_running = false;
  timer_targets_10ms_inc = 0;
  for (int i = 0; i < 3; i++)
    set_outputs(FACING);
  target_face = true;
  draw_targets(0);
  leds.setPixelColor(timer_set_ptr, WHITE);
  leds.show();
  draw_timer(timer_val[timer_set_ptr]);
}

//----------------------TFT functions----------------------//

// Initial setup to initialise screen
void tft_setup(void) {
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(ILI9341_WHITE);

  time_sprite.setTextWrap(false);
  time_sprite.setFont(&calibri12pt7b);
  time_sprite.setTextColor(ILI9341_WHITE);

  timer_sprite.setTextWrap(false);
  timer_sprite.setFont(&calibrib48pt7b);
  timer_sprite.setTextColor(ILI9341_WHITE);

  status_sprite.setTextWrap(false);
  status_sprite.setFont(&calibrib12pt7b);
  status_sprite.setTextColor(ILI9341_WHITE);

  clk_cfg_sprite.setTextWrap(false);
  clk_cfg_sprite.setFont(&calibrib12pt7b);
  clk_cfg_sprite.setTextColor(ILI9341_WHITE);
}

// Start capacitive touch interface
bool ctp_setup(void) {
  return ctp.begin();
}

//---> Screen handler functions <---//

// Touch screen press handler
void touch_screen_handler(void) {
  if (target_timer_running) return;
  if ((millis() - touch_timestamp) < TOUCH_MIN_REPEAT_TIME_MS) return;  //Debounce timer
  touch_timestamp = millis();
  TS_Point tp = ctp.getPoint();
  if (!(tp.x || tp.y)) return;  //Discard a 0, 0 touch point
  int x = tp.y;
  int y = 239 - tp.x;  //Transform for screen rotation
  if (debug) {
    tft.fillCircle(x, y, 10, ILI9341_RED);
    char buf[50];
    sprintf(buf, "x:%i y:%i .. x_raw:%i y_raw:%i", x, y, tp.x, tp.y);
    Serial.println(buf);
  }
  // Screen is divided into an 6x4 matrix (each square is 54x60px), matrix is numbered left to right, top to bottom from 0 - 23
  //  _____________________________
  // |__0_|__1_|__2_|__3_|__4_|__5_|
  // |__6_|__7_|__8_|__9_|_10_|_11_|
  // |_12_|_13_|_14_|_15_|_16_|_17_|
  // |_18_|_19_|_20_|_21_|_22_|_23_|
  //
  int touch_point_location = (x / 54) + (6 * (y / 60));
  if (debug) {
    Serial.print("TP location = ");
    Serial.println(touch_point_location);
  }
  // Find the function associated with touch button press...

  // From the Home Screen
  if (screen_menu == HOME_SCREEN) {
    if (touch_point_location == UP_BTN) change_custom_time(INCR);
    else if (touch_point_location == DOWN_BTN) change_custom_time(DECR);
    else if (touch_point_location == SETTINGS_BTN) draw_settings_screen();
    /*else if (touch_point_location == TARGET1_BTN) spin_target(0);
    else if (touch_point_location == TARGET2_BTN) spin_target(1);
    else if (touch_point_location == TARGET3_BTN) spin_target(2);*/

    // From the Main Settings menu
  } else if (screen_menu == SETTINGS_SCREEN) {
    if (touch_point_location == HOME_BTN) draw_homeScreen();
    else if ((touch_point_location >= SET_CLOCK_BTN) && (touch_point_location < (SET_CLOCK_BTN + 3))) draw_set_clock_screen();
    else if ((touch_point_location >= PAIR_REMOTE_BTN) && (touch_point_location < (PAIR_REMOTE_BTN + 3))) draw_pair_remote_screen();
    else if ((touch_point_location >= ENABLE_TARGETS_BTN) && (touch_point_location < (ENABLE_TARGETS_BTN + 3))) draw_enable_targets_screen();

    // In the Clock Set screen
  } else if (screen_menu == SET_CLOCK_SCREEN) {
    if (touch_point_location == BACK_BTN) draw_settings_screen();
    else if ((touch_point_location >= HOUR_BTN) && (touch_point_location <= SEC_BTN)) draw_selected_time_set_box(touch_point_location - HOUR_BTN);
    else if ((touch_point_location >= DAY_BTN) && (touch_point_location <= YEAR_BTN)) draw_selected_time_set_box(touch_point_location - DAY_BTN + 3);
    else if ((touch_point_location == UP_BTN) && (tm_box_ptr >= 0)) change_clock_time(INCR);
    else if ((touch_point_location == DOWN_BTN) && (tm_box_ptr >= 0)) change_clock_time(DECR);
    else if ((touch_point_location == SAVE_BTN) && (tm_box_ptr >= 0)) save_clock_time();

    // In the Remote Pairing screen
  } else if (screen_menu == PAIR_REMOTE_SCREEN) {
    if (touch_point_location == BACK_BTN) draw_settings_screen();

    // In the Outputs Enable screen
  } else if (screen_menu == ENABLE_TARGETS_SCREEN) {
    if (touch_point_location == BACK_BTN) {
      nvm_flash_save();
      draw_settings_screen();
    } else if ((touch_point_location >= OUT1_ENABLE_BTN) && (touch_point_location < (OUT1_ENABLE_BTN + 3))) toggle_target_enable(0);
    else if ((touch_point_location >= OUT2_ENABLE_BTN) && (touch_point_location < (OUT2_ENABLE_BTN + 3))) toggle_target_enable(1);
    else if ((touch_point_location >= OUT3_ENABLE_BTN) && (touch_point_location < (OUT3_ENABLE_BTN + 3))) toggle_target_enable(2);
  }
}

// Custom time adjustment (button 16)
void change_custom_time(int val) {
  if (timer_set_ptr != CUSTOM_TIMER_PTR) return;
  if ((val > 0) && (timer_val[CUSTOM_TIMER_PTR] < 10)) timer_val[CUSTOM_TIMER_PTR] += 0.5;
  else if ((val < 0) && (timer_val[CUSTOM_TIMER_PTR] < 11)) timer_val[CUSTOM_TIMER_PTR] -= 0.5;
  else timer_val[CUSTOM_TIMER_PTR] += val;
  if (timer_val[CUSTOM_TIMER_PTR] < 1.0) timer_val[CUSTOM_TIMER_PTR] = 1.0;
  else if (timer_val[CUSTOM_TIMER_PTR] > 999) timer_val[CUSTOM_TIMER_PTR] = 999;
  else custom_timer_changed_timestamp = millis();
  custom_timer_is_int = false;  // Flag as false so that draw_timer always draws the decimal while editing custom timer value
  draw_timer(timer_val[CUSTOM_TIMER_PTR]);
  int timer_val_int = (int)timer_val[CUSTOM_TIMER_PTR];
  if ((timer_val[CUSTOM_TIMER_PTR] - (float)timer_val_int) == 0) custom_timer_is_int = true;  // Check for a half second, flag as integer if not
}

//---> Screen draw functions <---//

// Home screen
void draw_homeScreen(void) {
  screen_menu = HOME_SCREEN;

  draw_top_status_bar();
  tft.fillRect(2, 188, 316, 50, ILI9341_BLACK);

  draw_timer(timer_val[timer_set_ptr]);

  draw_up_down_btns();

  drawSpriteNoBG(5, 190, 46, 46, settings_icon_bmp, ILI9341_BLACK);

  draw_targets(0);
  tft_printFormatted(105, 200, ILI9341_WHITE, &calibrib9pt7b, "1");
  tft_printFormatted(159, 200, ILI9341_WHITE, &calibrib9pt7b, "2");
  tft_printFormatted(213, 200, ILI9341_WHITE, &calibrib9pt7b, "3");
}

// Settings screen
void draw_settings_screen(void) {
  if (screen_menu == PAIR_REMOTE_SCREEN) {
    pairing = false;
    radio.closeReadingPipe(0);
  }
  screen_menu = SETTINGS_SCREEN;

  draw_top_status_bar();
  draw_heading_text("Settings");
  drawSpriteNoBG(11, 84, 85, 85, settings_bg_bmp, ILI9341_WHITE);

  for (int i = 0; i < 3; i++) {
    drawSpriteNoBG(106, 67 + (60 * i), 168, 52, btn_outline_1x3_bmp, ILI9341_WHITE);
    tft_printFormatted(115, 97 + (60 * i), ILI9341_DARKGREY, &calibrib12pt7b, settings_button_text[i]);
  }

  drawSpriteNoBG(5, 190, 46, 46, home_icon_bmp, ILI9341_WHITE);
}

// Clock set screen
void draw_set_clock_screen(void) {
  screen_menu = SET_CLOCK_SCREEN;

  draw_top_status_bar();

  struct RTCx::tm tm;
  rtc.readClock(tm);
  rtc.mktime(tm);
  tm_box[0].val = tm.tm_hour;
  tm_box[1].val = tm.tm_min;
  tm_box[2].val = tm.tm_sec;
  tm_box[3].val = tm.tm_mday;
  tm_box[4].val = tm.tm_mon;
  tm_box[5].val = tm.tm_year + 1900;
  tft.setFont(&calibrib12pt7b);
  tft.setTextColor(ILI9341_DARKGREY);
  for (int i = 0; i < 6; i++) drawSpriteNoBG(clk_btn[i].x, clk_btn[i].y, clk_btn[i].w, clk_btn[i].h, btn_outline_1x1_bmp, ILI9341_WHITE);

  for (int i = 0; i < 4; i++) {
    clk_cfg_sprite.fillScreen(0);
    clk_cfg_sprite.setCursor(10, 15);
    clk_cfg_sprite.print(tm_box[i].val);
    tft.drawBitmap(clk_btn[i].x + 3, clk_btn[i].y + 17, clk_cfg_sprite.getBuffer(), clk_cfg_sprite.width(), clk_cfg_sprite.height(), ILI9341_DARKGREY, ILI9341_WHITE);
  }

  clk_cfg_sprite.setFont(&calibrib9pt7b);
  clk_cfg_sprite.fillScreen(0);
  clk_cfg_sprite.setCursor(6, 15);
  clk_cfg_sprite.print(month[tm_box[CLK_CFG_MONTH].val]);
  tft.drawBitmap(clk_btn[4].x + 3, clk_btn[4].y + 17, clk_cfg_sprite.getBuffer(), clk_cfg_sprite.width(), clk_cfg_sprite.height(), ILI9341_DARKGREY, ILI9341_WHITE);

  clk_cfg_sprite.fillScreen(0);
  clk_cfg_sprite.setCursor(4, 15);
  clk_cfg_sprite.print(tm_box[CLK_CFG_YEAR].val);
  tft.drawBitmap(clk_btn[5].x + 3, clk_btn[5].y + 17, clk_cfg_sprite.getBuffer(), clk_cfg_sprite.width(), clk_cfg_sprite.height(), ILI9341_DARKGREY, ILI9341_WHITE);
  clk_cfg_sprite.setFont(&calibrib12pt7b);

  draw_up_down_btns();
  drawSpriteNoBG(5, 195, 39, 36, return_icon_bmp, ILI9341_WHITE);
  drawSpriteNoBG(273, 192, 39, 39, save_icon_bmp, ILI9341_WHITE);
  tft_printFormatted(110, 220, ILI9341_DARKGREY, &calibrib12pt7b, "Set Clock");

  tft.setFont(&Org_01);
  for (int i = 0; i < 3; i++) {
    tft.setCursor(70 + (i * 54), 62);
    tft.print(time_labels[i]);
  }
  for (int i = 0; i < 3; i++) {
    tft.setCursor(66 + (i * 54), 182);
    tft.print(date_labels[i]);
  }
}

void draw_selected_time_set_box(int box_id) {
  int ptr = tm_box_ptr;
  tm_box_ptr = box_id;
  if (ptr >= 0) drawSpriteNoBG(clk_btn[ptr].x, clk_btn[ptr].y, clk_btn[ptr].w, clk_btn[ptr].h, btn_outline_1x1_bmp, ILI9341_WHITE);
  drawSpriteNoBG(clk_btn[box_id].x, clk_btn[box_id].y, clk_btn[box_id].w, clk_btn[box_id].h, btn_outline_selected_1x1_bmp, ILI9341_WHITE);
}

void change_clock_time(int val) {
  int max_val = tm_box[tm_box_ptr].max;
  int min_val = tm_box[tm_box_ptr].min;
  if (tm_box_ptr == CLK_CFG_DAY) max_val = month_max_days[tm_box[CLK_CFG_MONTH].val];

  tm_box[tm_box_ptr].val += val;
  if (tm_box[tm_box_ptr].val > max_val) tm_box[tm_box_ptr].val = min_val;
  else if (tm_box[tm_box_ptr].val < min_val) tm_box[tm_box_ptr].val = max_val;

  clk_cfg_sprite.fillScreen(0);

  if (tm_box_ptr == CLK_CFG_MONTH) {
    clk_cfg_sprite.setFont(&calibrib9pt7b);
    clk_cfg_sprite.setCursor(6, 15);
    clk_cfg_sprite.print(month[tm_box[CLK_CFG_MONTH].val]);
    clk_cfg_sprite.setFont(&calibrib12pt7b);
  } else if (tm_box_ptr == CLK_CFG_YEAR) {
    clk_cfg_sprite.setFont(&calibrib9pt7b);
    clk_cfg_sprite.setCursor(4, 15);
    clk_cfg_sprite.print(tm_box[CLK_CFG_YEAR].val);
    clk_cfg_sprite.setFont(&calibrib12pt7b);
  } else {
    clk_cfg_sprite.setCursor(10, 15);
    clk_cfg_sprite.print(tm_box[tm_box_ptr].val);
  }
  tft.drawBitmap(clk_btn[tm_box_ptr].x + 3, clk_btn[tm_box_ptr].y + 17, clk_cfg_sprite.getBuffer(), clk_cfg_sprite.width(), clk_cfg_sprite.height(), ILI9341_TEAL, ILI9341_WHITE);
}

void save_clock_time(void) {
  struct RTCx::tm tm;
  if (tm_box[CLK_CFG_DAY].val > month_max_days[tm_box[CLK_CFG_MONTH].val]) tm_box[CLK_CFG_DAY].val = month_max_days[tm_box[CLK_CFG_MONTH].val];
  tm.tm_hour = tm_box[CLK_CFG_HOUR].val;
  tm.tm_min = tm_box[CLK_CFG_MIN].val;
  tm.tm_sec = tm_box[CLK_CFG_SEC].val;
  tm.tm_mday = tm_box[CLK_CFG_DAY].val;
  tm.tm_mon = tm_box[CLK_CFG_MONTH].val;
  tm.tm_year = tm_box[CLK_CFG_YEAR].val - 1900;
  rtc.mktime(&tm);
  rtc.setClock(&tm);
  draw_set_clock_screen();
}

// Pair remote screen
void draw_pair_remote_screen(void) {
  screen_menu = PAIR_REMOTE_SCREEN;

  pairing = true;
  is_paired = false;

  draw_top_status_bar();

  drawSpriteNoBG(2, 44, 316, 194, remote_pair_bg_bmp, ILI9341_WHITE);
  
  radio.stopListening();
  radio.setPALevel(RF24_PA_LOW);
  radio.openReadingPipe(1, 0xC2C2C2C2C2);
  radio.closeReadingPipe(1);
  radio.setPayloadSize(8);
  radio.openReadingPipe(0, tcs_pair_pipe);
  radio.startListening();

  if (debug) {
    Serial.print("Pairing started, listening on pipe ");
    Serial.println((char *)tcs_pair_pipe);
    char buf[870];
    radio.sprintfPrettyDetails(buf);
    Serial.println(buf);
  }
}

// Enable outputs screen
void draw_enable_targets_screen(void) {
  screen_menu = ENABLE_TARGETS_SCREEN;

  draw_top_status_bar();

  draw_heading_text("Enable/Disable Target Outputs");
  drawSpriteNoBG(11, 84, 85, 85, settings_bg_bmp, ILI9341_WHITE);

  for (int i = 0; i < 3; i++) draw_target_enable_button(i, target_enabled[i]);

  drawSpriteNoBG(5, 195, 39, 36, return_icon_bmp, ILI9341_WHITE);
}

void toggle_target_enable(uint32_t target_ptr) {
  if (target_ptr > 2) return;
  target_enabled[target_ptr] ^= 1;
  draw_target_enable_button(target_ptr, target_enabled[target_ptr]);
}

//---> Common elements draw functions <---//

// Top status bar
void draw_top_status_bar(void) {
  tft.fillScreen(ILI9341_WHITE);
  tft.fillRect(2, 2, 316, 40, ILI9341_BLACK);

  drawSpriteNoBG(10, 7, 29, 29, clock_icon_bmp, ILI9341_BLACK);
  draw_time();

  if (is_paired) draw_remote();
}

// RTC Time update
void draw_time(void) {
  struct RTCx::tm tm;
  rtc.readClock(tm);
  rtc.mktime(tm);
  char buf[20];
  sprintf(buf, "%2i:%02i:%02i", tm.tm_hour, tm.tm_min, tm.tm_sec);
  time_sprite.fillScreen(0);
  time_sprite.setCursor(0, 15);
  time_sprite.print(buf);
  tft.drawBitmap(45, 14, time_sprite.getBuffer(), time_sprite.width(), time_sprite.height(), ILI9341_WHITE, ILI9341_BLACK);
}

// Remote icon and battery status
void draw_remote(void) {
  if (remote_has_connected && (batt_level < 8)) { // Draw battery icon only if we have recieved a valid battery level since power up
    if (batt_level == 7) drawSprite(270, 14, 32, 16, batt_full_icon_bmp);
    else if (batt_level > 0) {
      drawSprite(270, 14, 32, 16, batt_icon_bmp);
      for (int i = 1; i < batt_level; i++) drawSprite(272 + (i * 4), 16, 5, 12, batt_bar_bmp);
    } else drawSprite(270, 14, 32, 16, batt_low_icon_bmp);
  }
  if (is_paired) drawSpriteNoBG(230, 8, 30, 29, remote_icon_bmp, ILI9341_BLACK);
}

// Timer value update
void draw_timer(float timer_display_val) {
  target_timer_second_flag = false;
  target_timer_half_second_flag = false;
  int seconds = timer_display_val;
  float half_seconds = timer_display_val - (float)seconds;
  String buf;
  // Only show half seconds if we are under 10s and in custom timer mode, and not in the shoot phase while running
  if ((timer_display_val > 9.5) || (timer_set_ptr != CUSTOM_TIMER_PTR) || ((target_timer_phase != SHOOT_TIMER_PHASE) && target_timer_running) || custom_timer_is_int) {
    buf = (String)seconds + "s";
  } else {
    if (half_seconds > 0.0) buf = (String)seconds + ".5s";
    else buf = (String)seconds + ".0s";
  }
  timer_sprite.fillScreen(0);
  timer_sprite.setCursor(0, 60);
  timer_sprite.print(buf);
  tft.drawBitmap(40, 90, timer_sprite.getBuffer(), timer_sprite.width(), timer_sprite.height(), ILI9341_BLACK, ILI9341_WHITE);
}

// Up down buttons
void draw_up_down_btns(void) {
  drawSpriteNoBG(266, 65, 52, 52, btn_outline_1x1_bmp, ILI9341_WHITE);
  drawSpriteNoBG(266, 125, 52, 52, btn_outline_1x1_bmp, ILI9341_WHITE);
  drawSpriteNoBG(279, 83, 25, 15, up_icon_bmp, ILI9341_WHITE);
  drawSpriteNoBG(279, 143, 25, 15, down_icon_bmp, ILI9341_WHITE);
}

// Target icon image update
void draw_targets(uint32_t phase_time) {
  char buf[30];
  if (phase_time) sprintf(buf, "Targets - %s (%is)", target_status[target_face], phase_time);
  else sprintf(buf, "Targets - %s", target_status[target_face]);
  draw_heading_text(buf);
  for (int i = 0; i < 3; i++) {
    if (target_enabled[i]) {
      if (target_face) drawSpriteNoBG((54 * i) + 115, 195, 35, 35, face_icon_bmp, ILI9341_BLACK);
      else drawSprite((54 * i) + 115, 195, 35, 35, edge_icon_bmp);
    } else drawSpriteNoBG((54 * i) + 115, 195, 35, 35, off_icon_bmp, ILI9341_BLACK);
  }
}

// Target enable buttons for target enable settings screen
void draw_target_enable_button(uint32_t target_ptr, bool enabled_state) {
  if (target_ptr > 2) return;
  drawSprite(106, 67 + (60 * target_ptr), 168, 52, btn_outline_1x3_bmp);
  uint16_t text_colour = ILI9341_DARKGREEN;
  if (enabled_state) text_colour = ILI9341_RED;
  const char *state_text[2] = { "En", "Dis" };
  char buf[20];
  sprintf(buf, "Out %i %sable", target_ptr + 1, state_text[enabled_state]);
  tft_printFormatted(115, 97 + (60 * target_ptr), text_colour, &calibrib12pt7b, buf);
}

//---> Text drawing helpers <---//

// Heading text
void draw_heading_text(const char *text) {
  status_sprite.fillScreen(0);
  status_sprite.setCursor(0, 15);
  status_sprite.print(text);
  tft.drawBitmap(5, 46, status_sprite.getBuffer(), status_sprite.width(), status_sprite.height(), ILI9341_DARKGREY, ILI9341_WHITE);
}

// Print text in the specified colour
void tft_colourPrint(uint16_t colour, const char *text) {
  tft.setTextColor(colour);
  tft.print(text);
}

void tft_printFormatted(uint16_t x, uint16_t y, uint16_t colour, const GFXfont *fnt, const char *text) {
  tft.setFont(fnt);
  tft.setCursor(x, y);
  tft_colourPrint(colour, text);
}


//---> Custom bitmap drawing functions (fast) <---//

// Draw a bitmap
void drawSprite(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *bitmap) {
  int bp = 0;
  for (int row = 0; row < h; row++) {
    for (int col = 0; col < w; col++) {
      uint16_t colour = ((uint16_t)bitmap[bp++] << 8) | bitmap[bp++];
      tft.drawPixel(col + x, row + y, colour);
    }
  }
}

// Draw a bitmap with specified background colour to ignore
void drawSpriteNoBG(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *bitmap, uint16_t BG_colour) {
  int bp = 0;
  for (int row = 0; row < h; row++) {
    for (int col = 0; col < w; col++) {
      uint16_t colour = ((uint16_t)bitmap[bp++] << 8) | bitmap[bp++];
      if (colour != BG_colour) tft.drawPixel(col + x, row + y, colour);
    }
  }
}

// Erase a bitmap ignoring specified background colour
void clearSpriteNoBG(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *bitmap, uint16_t BG_colour) {
  int bp = 0;
  for (int row = 0; row < h; row++) {
    for (int col = 0; col < w; col++) {
      uint16_t colour = ((uint16_t)bitmap[bp++] << 8) | bitmap[bp++];
      if (colour != BG_colour) tft.drawPixel(col + x, row + y, BG_colour);
    }
  }
}

//----------------------NRF functions----------------------//

int nrf_setup(void) {
  if (!radio.begin()) {
    if (debug) Serial.println("NRF24L01 radio hardware is not responding");
    return false;
  }
  if (debug) Serial.println("NRF24L01 initialised OK");

  radio.setPALevel(RF24_PA_MAX, true);  // RF24_PA_MAX is default.
  if (is_paired) {
    radio.setPayloadSize(8);
    radio.openReadingPipe(1, tx_pipe_uid);
    radio.startListening();
  }

  if (debug) {
    char buf[870];
    radio.sprintfPrettyDetails(buf);
    Serial.println(buf);
  }

  return true;
}

bool nrf_check(void) {
  uint8_t pipe;
  if (radio.available(&pipe)) {              // is there a payload? get the pipe number that recieved it
    uint8_t num_bytes = radio.getPayloadSize();  // get the size of the payload
    uint8_t data[num_bytes];
    radio.read(data, num_bytes);  // fetch payload from FIFO

    if (debug) {
      String data_info = (String)num_bytes + " received on pipe " + (String)pipe + ", data: 0x";
      Serial.print(data_info);
      for (int i = 0; i < num_bytes; i++) Serial.print(data[i], HEX);
      Serial.println("\n");
    }

    if (pairing && (data[0] == PAIR_BYTE)) {
      if (debug) Serial.println("Data ID is pairing packet");
      for (int i = 0; i < 6; i++) tx_pipe_uid[i] = data[i + 1];
      radio.openReadingPipe(1, tx_pipe_uid);
      radio.closeReadingPipe(0);
      if (debug) {
        Serial.print("Opened new pipe ");
        Serial.println((char *)tx_pipe_uid);
        Serial.println("Waiting for followup packet");
      }
      uint32_t timeout_target_ms = millis() + 1000;
      bool pairing_succeeded = false;
      while (timeout_target_ms > millis()) {
        delay(10);
        if (radio.available(&pipe)) {  // is there a payload? get the pipe number that recieved it
          if (debug) Serial.println("Radio data available...");
          radio.read(data, num_bytes);  // fetch payload from FIFO
          if (debug) {
            Serial.print("Got data on pipe ");
            Serial.println(pipe);
            Serial.println("Pairing succeeded");
          }
          pairing_succeeded = true;
          break;
        }
      }
      if (pairing_succeeded) {
        batt_level = data[1];
        pairing = false;
        is_paired = true;
        nvm_flash_save();
        tft.setFont(&calibrib12pt7b);
        tft.setCursor(60, 212);
        tft_colourPrint(ILI9341_DARKGREEN, "   -Pairing Successful-");
        tft.setCursor(60, 234);
        tft.setFont(&calibri12pt7b);
        tft.print("Paired with Tx ID ");
        tft.print((char *)tx_pipe_uid);

        draw_remote();

        radio.setPALevel(RF24_PA_MAX);
        radio.startListening();

        if (debug) Serial.println("Recieved followup packet, pairing complete");

      } else if (debug) Serial.println("Pair attempt failed, no followup packet received");
    } else {
      int remote_button = data[0];
      batt_level = data[1];
      draw_remote();
      if (debug) {
        Serial.print("Got button press ");
        Serial.println(remote_button);
        Serial.print("Got batt level ");
        Serial.println(batt_level);
      }
      if (screen_menu == HOME_SCREEN) {
        if ((remote_button > 0) && (remote_button < 17) && (!target_timer_running)) {
          timer_set_ptr = remote_button - 1;
          timer_button_handler();
          start_timer();
        } else if ((remote_button == CANCEL_VAL) && target_timer_running) {
          cancel_timer();
        }
      }
    }
    remote_has_connected = true;
    return true;
  }
  return false;
}

//--------------------Output functions---------------------//

void set_outputs(bool state) {
  bool output_state = state;
  if (outputs_inverted) output_state ^= 1;
  for (int i = 0; i < 3; i++) {
    if (target_enabled[i]) {
      digitalWrite(mosfet_output[i], output_state);
    }
  }
}

//---------------------Timer functions---------------------//

int timer_setup(void) {
  if (ITimer.attachInterruptInterval_MS(TIMER_INTERVAL_MS, timer_ISR)) {
    if (debug) Serial.println("Started ITimer OK");
    return 1;
  }
  if (debug) Serial.println("Can't set ITimer. Select another freq. or timer");

  return 0;
}

//------------------Button & LED functions-----------------//

// Start WS2812B interface and initialise LEDs
void led_setup(void) {
  if (debug) Serial.println("Starting WS2812B LED interface");
  leds.begin();
  leds.setBrightness(255);
  leds.show();
  leds.fill(WHITE, 0, 16);
  leds.show();
}

// Timer button press handler
void timer_button_handler(void) {
  if (target_timer_running) return;
  leds.clear();
  leds.setPixelColor(timer_set_ptr, WHITE);
  leds.show();
  if (screen_menu == HOME_SCREEN) draw_timer(timer_val[timer_set_ptr]);
}

// Make all LEDs white in sequence, then black in sequence (part of startup sequence)
void led_wipe(void) {
  leds.fill(OFF, 0, 16);
  leds.show();
  for (int i = 0; i < LED_NUM; i++) {
    leds.setPixelColor(i, WHITE);
    leds.show();
    delay(35);
  }
  for (int i = 0; i < LED_NUM; i++) {
    leds.setPixelColor(i, OFF);
    leds.show();
    delay(35);
  }
  leds.setPixelColor(timer_set_ptr, WHITE);
  leds.show();
}

//----------------------RTC functions----------------------//

int rtc_setup(void) {
  Wire.begin();
  if (rtc.autoprobe()) {
    if (debug) {
      Serial.println("\nAutoprobing for a RTC...");
      Serial.print("Autoprobe found ");
      Serial.print(rtc.getDeviceName());
      Serial.print(" at 0x");
      Serial.println(rtc.getAddress(), HEX);
    }
    rtc.enableBatteryBackup();
    rtc.startClock();
    rtc.setCalibration(63);
    return 1;
  }
  if (debug) Serial.println("\nAutoprobing for a RTC failed");
  return 0;
}

//----------------------NVM functions----------------------//

bool nvm_flash_read(void) {
  bool flash_written = false;
  NVM_config nvm_config_read_data;
  nvm_config_read_data = nvm_config_data.read();

  if (!nvm_config_read_data.flash_valid) {
    nvm_config_read_data.custom_timer_val = 7.5;
    for (int i = 0; i < 3; i++) nvm_config_read_data.target_enabled[i] = true;
    nvm_config_read_data.tx_paired = false;
    for (int i = 0; i < 6; i++) nvm_config_read_data.tx_pipe_uid[i] = tx_pipe_uid[i];
    nvm_config_read_data.flash_valid = true;
    nvm_config_data.write(nvm_config_read_data);
    flash_written = true;
  }
  timer_val[15] = nvm_config_read_data.custom_timer_val;
  for (int i = 0; i < 3; i++) target_enabled[i] = nvm_config_read_data.target_enabled[i];
  is_paired = nvm_config_read_data.tx_paired;
  for (int i = 0; i < 6; i++) tx_pipe_uid[i] = nvm_config_read_data.tx_pipe_uid[i];

  return flash_written;
}

void nvm_flash_save(void) {
  NVM_config nvm_config_write_data;
  nvm_config_write_data.flash_valid = true;
  nvm_config_write_data.custom_timer_val = timer_val[15];
  for (int i = 0; i < 3; i++) nvm_config_write_data.target_enabled[i] = target_enabled[i];
  nvm_config_write_data.tx_paired = is_paired;
  for (int i = 0; i < 6; i++) nvm_config_write_data.tx_pipe_uid[i] = tx_pipe_uid[i];
  nvm_config_data.write(nvm_config_write_data);

  if (debug) Serial.println("Saving data to NVM");
}

//---------------------Setup functions---------------------//

void pins_setup(void) {
  for (int i = 0; i < 18; i++) pinMode(button_input[i], INPUT_PULLUP);
  for (int i = 0; i < 3; i++) {
    pinMode(mosfet_output[i], OUTPUT);
    digitalWrite(mosfet_output[i], outputs_inverted);  // LOW if outputs normal, HIGH if outputs inverted
  }
  pinMode(NRF_IRQ, INPUT_PULLUP);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  if (digitalRead(IN_START) && digitalRead(IN_CANCEL)) debug = false;
  attachInterrupt(digitalPinToInterrupt(IN_START), start_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(IN_CANCEL), cancel_ISR, FALLING);
  //attachInterrupt(digitalPinToInterrupt(NRF_IRQ), nrf_ISR, FALLING);
}

int peripherals_setup(void) {
  led_setup();
  tft_setup();
  drawSprite(45, 160, 230, 68, logo_full_bmp);
  tft_printFormatted(25, 20, ILI9341_DARKGREY, &calibrib9pt7b, "Starting Target Control System\n");

  tft_printFormatted(25, 40, ILI9341_DARKGREY, &calibri9pt7b, "Starting RTC... ");
  if (rtc_setup()) tft_colourPrint(ILI9341_GREEN, ok);
  else tft_colourPrint(ILI9341_RED, failed);

  tft_printFormatted(25, 60, ILI9341_DARKGREY, &calibri9pt7b, "Starting Timer... ");
  if (timer_setup()) tft_colourPrint(ILI9341_GREEN, ok);
  else tft_colourPrint(ILI9341_RED, failed);

  tft_printFormatted(25, 80, ILI9341_DARKGREY, &calibri9pt7b, "Starting NRF24L01... ");
  if (nrf_setup()) tft_colourPrint(ILI9341_GREEN, ok);
  else tft_colourPrint(ILI9341_RED, failed);

  tft_printFormatted(25, 100, ILI9341_DARKGREY, &calibri9pt7b, "Starting CTP... ");
  if (ctp_setup()) tft_colourPrint(ILI9341_GREEN, ok);
  else tft_colourPrint(ILI9341_RED, failed);

  tft_printFormatted(25, 120, ILI9341_DARKGREY, &calibri9pt7b, "Getting NVM config... ");
  if (nvm_flash_read()) tft_colourPrint(ILI9341_BLUE, "INIT");
  else tft_colourPrint(ILI9341_GREEN, ok);

  delay(500);

  draw_homeScreen();

  led_wipe();
  return 0;
}