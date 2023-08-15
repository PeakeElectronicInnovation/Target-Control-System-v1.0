#ifndef DEFINES_H
#define DEFINES_H

//********************************************************************//
//-----------------------------Definitions----------------------------//
//********************************************************************//

//-------------Config Defines------------//

// Target control base station pairing listen address (DO NOT CHANGE)
#define TCS_PAIR_ID "TCS00"

//----------------MCU Pins---------------//

#define IN_1 47
#define IN_2 16
#define IN_3 15
#define IN_4 17
#define IN_5 21
#define IN_6 20
#define IN_7 50
#define IN_8 49
#define IN_9 19
#define IN_10 18
#define IN_11 29
#define IN_12 27
#define IN_13 52
#define IN_14 51
#define IN_15 36
#define IN_CUSTOM 14
#define IN_START 41
#define IN_CANCEL 42

#define OUT_1 9
#define OUT_2 8
#define OUT_3 1

#define LED_DAT 40
#define LED_NUM 16  //Number of WS2812B LEDs

#define NRF_CE 10
#define NRF_CS 3
#define NRF_IRQ 2

#define TFT_BL 6
#define TFT_CS 5
#define TFT_DC 48
#define TFT_RST 45
#define TFT_T_IRQ 46

//----------------Timer Lib--------------//

#define USING_TIMER_TC3 true
#define SELECTED_TIMER TIMER_TC3
#define TIMER_INTERVAL_MS 10

//------------Timing Constants-----------//

#define START_DELAY_SECONDS 5
#define END_DELAY_SECONDS 5

#define TOUCH_MIN_REPEAT_TIME_MS 200

#define NVM_WRITE_DELAY_MS 2000

//------------Serial Instances-----------//

#define NRF_SPI SPI
#define TFT_SPI SPI1

//--------------LED Colours--------------//

#define WHITE 0xFFFFFF
#define RED 0xFF0000
#define GREEN 0x00FF00
#define BLUE 0x0000FF
#define AMBER 0xFFAA00
#define OFF 0x000000

//-------------Text Colours--------------//

#define ILI9341_TEAL 0x0333

//-------------Value Defines-------------//

#define START_DELAY_PHASE 0
#define SHOOT_TIMER_PHASE 1
#define END_DELAY_PHASE 2
#define TIMER_INACTIVE 3

#define CUSTOM_TIMER_PTR 15

#define INCR 1
#define DECR -1

#define PAIR_BYTE 0xF0
#define CANCEL_VAL 17

#define FACING 0
#define UNFACED 1

//--------Touch Button Addresses--------//

// Home screen
#define UP_BTN 11
#define DOWN_BTN 17
#define SETTINGS_BTN 18
#define TARGET1_BTN 20
#define TARGET2_BTN 21
#define TARGET3_BTN 22

// Settings screen
#define HOME_BTN 18
#define SET_CLOCK_BTN 8
#define PAIR_REMOTE_BTN 14
#define ENABLE_TARGETS_BTN 20

// Clock set screen
#define HOUR_BTN 7
#define MIN_BTN 8
#define SEC_BTN 9
#define DAY_BTN 13
#define MONTH_BTN 14
#define YEAR_BTN 15
#define BACK_BTN 18
#define SAVE_BTN 23

// Remote pair screen
// ---> Nothing to see here :-)

// Output enable screen
#define OUT1_ENABLE_BTN 8
#define OUT2_ENABLE_BTN 14
#define OUT3_ENABLE_BTN 20

//-------------Screen Menus-------------//

#define HOME_SCREEN 0
#define SETTINGS_SCREEN 1
#define SET_CLOCK_SCREEN 2
#define PAIR_REMOTE_SCREEN 3
#define ENABLE_TARGETS_SCREEN 4

//--------------NRF values--------------//


//-----------Enumerated values----------//

// Clock config pointers for time setting
#define CLK_CFG_HOUR 0
#define CLK_CFG_MIN 1
#define CLK_CFG_SEC 2
#define CLK_CFG_DAY 3
#define CLK_CFG_MONTH 4
#define CLK_CFG_YEAR 5

//********************************************************************//
//------------------------------Libraries-----------------------------//
//********************************************************************//

#include "Adafruit_FT6206.h"
#include "Adafruit_GFX.h"
#include "Fonts/calibri9pt7b.h"
#include "Fonts/calibrib9pt7b.h"
#include "Fonts/calibri12pt7b.h"
#include "Fonts/calibrib12pt7b.h"
#include "Fonts/calibrib48pt7b.h"
#include "Fonts/Org_01.h"
#include "bitmaps.h"
#include "Adafruit_ILI9341.h"
#include "Adafruit_ZeroDMA.h"
#include "Adafruit_NeoPixel.h"
#include "FlashStorage.h"
#include "Wire.h"
#include "RTCx.h"
#include "RF24.h"
#include "SAMDTimerInterrupt.h"


//********************************************************************//
//---------------------Global Objects & Variables---------------------//
//********************************************************************//

// Object instances
Adafruit_FT6206 ctp = Adafruit_FT6206();
Adafruit_ILI9341 tft = Adafruit_ILI9341(&TFT_SPI, TFT_DC, TFT_CS, TFT_RST);
Adafruit_NeoPixel leds = Adafruit_NeoPixel(LED_NUM, LED_DAT, NEO_GRB + NEO_KHZ800);
GFXcanvas1 time_sprite(90, 16);
GFXcanvas1 status_sprite(210, 20);
GFXcanvas1 timer_sprite(190, 62);
GFXcanvas1 clk_cfg_sprite(43, 16);
RF24 radio(NRF_CE, NRF_CS);
SAMDTimer ITimer(SELECTED_TIMER);

// Saved config data staorage structure
typedef struct {
  bool flash_valid;
  float custom_timer_val;
  bool target_enabled[3];
  bool tx_paired;
  uint8_t tx_pipe_uid[6];
} NVM_config;

// NVM storage containers
FlashStorage(nvm_config_data, NVM_config);

// Button co-ordinates for quick drawing reference
typedef struct {
  uint16_t x;
  uint16_t y;
  uint16_t w;
  uint16_t h;
  uint16_t cur_x;
  uint16_t cur_y;
} button_def;

// Clock button co-ordinates
button_def clk_btn[6] = {
  {55, 65, 52, 52, 67, 97},     // Hours box
  {109, 65, 52, 52, 121, 97},   // Minutes box
  {163, 65, 52, 52, 175, 97},   // Seconds box
  {55, 125, 52, 52, 67, 157},   // Month day box
  {109, 125, 52, 52, 115, 157}, // Month box
  {163, 125, 52, 52, 170, 157}  // Year box
};

typedef struct {
  int val;
  int min;
  int max;
} time_def;

time_def tm_box[6] = {
  {0, 0, 23},     // Hour default, min and max values
  {0, 0, 59},     // Minutes
  {0, 0, 59},     // Seconds
  {1, 1, 31},     // Month day
  {0, 0, 11},     // Month ptr
  {2023, 2023, 2100} // Year
};

int tm_box_ptr = -1;
int month_max_days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

// Global flags
bool debug = true;
bool second_flag = false;
bool target_timer_second_flag = false;
bool target_timer_half_second_flag = false;
bool custom_timer_is_int = false;
bool timer_ended = false;
bool target_face = true;
bool target_enabled[3] = { true, true, true };
bool target_timer_running = false;
bool start_pressed = false;
bool cancel_pressed = false;
bool is_paired = false;
bool pairing = false;
bool remote_has_connected = false;
bool outputs_inverted = false;

uint8_t screen_menu = HOME_SCREEN;

// Global ints
uint32_t button_input[18] = { IN_1, IN_2, IN_3, IN_4, IN_5, IN_6, IN_7, IN_8, IN_9, IN_10, IN_11, IN_12, IN_13, IN_14, IN_15, IN_CUSTOM, IN_START, IN_CANCEL };
uint32_t mosfet_output[3] = { OUT_1, OUT_2, OUT_3 };
uint32_t timer_general_10ms_inc = 0, timer_targets_10ms_inc = 0, timer_set_ptr = 0, touch_timestamp = 0, custom_timer_changed_timestamp = 0;
uint32_t target_timer_phase = TIMER_INACTIVE;
uint32_t batt_level = 0;

// Global floats
float timer_val[16] = { 3, 4, 6, 8, 10, 12, 15, 20, 25, 35, 90, 150, 165, 210, 300, 7.5 };
float target_timer_seconds_left = 0;

// Global strings
const char *month[12] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };
const char *ok = { "OK" }, *failed = { "FAILED" };
const char *target_status[2] = { "Edge", "Facing" };
const char *settings_button_text[3] = {"Set Clock", "Pair Remote", "Enable Targets"};
const char *time_labels[3] = {"HOUR", "MIN", "SEC"};
const char *date_labels[3] = {" DAY", "MONTH", "YEAR"};

uint8_t tx_pipe_uid[6] = {0}, tcs_pair_pipe[6] = { TCS_PAIR_ID };

#endif  //DEFINES_H