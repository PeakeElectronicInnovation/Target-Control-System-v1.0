#ifndef DEFINES_H
#define DEFINES_H

//********************************************************************//
//-----------------------------Definitions----------------------------//
//********************************************************************//

//-------------Config Defines------------//

// Remote comms address (5 bytes) must be unique for every remote
#define REMOTE_UID "10323"  // Remote control Tx pipe (xxx = UID yy = year) - leave at the last used value

// Target control base station pairing listen address (DO NOT CHANGE)
#define TCS_PAIR_ID "TCS00"

// Pairing mode timeout time
#define MAX_PAIR_ATTEMPTS 20

//----------------MCU Pins---------------//

#define VBATT_PIN PIN_PD6
#define NRF_PS_EN PIN_PA2
#define NRF_CE PIN_PA3
#define NRF_CS PIN_PA7
#define BTN_OUT_C1 PIN_PC0
#define BTN_OUT_C2 PIN_PC1
#define BTN_OUT_C3 PIN_PC2
#define BTN_OUT_C4 PIN_PC3
#define BTN_IN_R1 PIN_PD1
#define BTN_IN_R2 PIN_PD2
#define BTN_IN_R3 PIN_PD3
#define BTN_IN_R4 PIN_PD4
#define BTN_CANCEL PIN_PD5
#define LED_RED PIN_PF3
#define LED_GREEN PIN_PF4
#define LED_BLUE PIN_PF5

//-------------Defined Values------------//

#define SerialDebug Serial

#define PAIR_BYTE 0xF0
#define CANCEL_VAL 17

#define VBAT_FULL_mV  3100    // Most new AA batteries are about 1.55 - 1.6V
#define VBAT_EMPTY_mV 2700    // System should operate down to 2.6V, 2.7V to be safe (at 1.35V batteries are virtually dead anyway)
#define VBAT_DIV  4           // Voltage divider value

//********************************************************************//
//------------------------------Libraries-----------------------------//
//********************************************************************//

#include "RF24.h"
#include "avr/sleep.h"


//********************************************************************//
//---------------------Global Objects & Variables---------------------//
//********************************************************************//

// Object instances
RF24 radio(NRF_CE, NRF_CS);

// Global flags
bool debug = false;
bool pairing = false;

volatile uint8_t button_pressed = 0;

// Global ints

int row_input_pin[4] = {BTN_IN_R1, BTN_IN_R2, BTN_IN_R3, BTN_IN_R4};
int col_output_pin[4] = {BTN_OUT_C1, BTN_OUT_C2, BTN_OUT_C3, BTN_OUT_C4};

uint8_t tx_pipe_uid[6] = { REMOTE_UID };

uint8_t batt_level = 5;

uint8_t tx_packet_num = 0;

unsigned long debounce_time = 0;
unsigned long pair_led_timeout_time = 0;


#endif  //DEFINES_H
