// All definitions, included files, global objects and vaiables are specified in defines.h
#include "defines.h"

//********************************************************************//
//--------------------------------Setup-------------------------------//
//********************************************************************//

void setup() {
  adc_setup();
  pin_setup();
  if (debug) serial_for_debug_setup();
  nrf_setup();
  isr_setup();
  digitalWrite(LED_RED, HIGH);
  delay(1000);
  digitalWrite(LED_GREEN, HIGH);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  system_sleep();
}

//********************************************************************//
//--------------------------------Loop--------------------------------//
//********************************************************************//

void loop() {
  if (button_pressed > 0) {
    digitalWrite(LED_RED, LOW);
    uint8_t payload[8] = { 0 };
    payload[0] = button_pressed;
    payload[1] = get_batt_level();
    if (payload[1] < 8) { // Battery ok
      bool tx_status = radio.write(&payload, sizeof(payload));
      if (tx_status) {
        if (debug) {
          SerialDebug.print("Transmission successful, ");  // payload was delivered
          SerialDebug.print("sent: ");
          SerialDebug.println(button_pressed);  // print payload sent
        }
        digitalWrite(LED_RED, HIGH);
        digitalWrite(LED_GREEN, LOW);
      } else if (debug) SerialDebug.println("Transmission failed or timed out");  // payload was not delivered
      delay(200);
      digitalWrite(LED_RED, HIGH);
      digitalWrite(LED_GREEN, HIGH);
    } else {  // Low battery!
      for (int i = 0; i < 50; i++) {
        digitalWrite(LED_RED, (digitalRead(LED_RED) ^ 1));
        delay(100);
      }
      delay(5000);
       digitalWrite(LED_RED, HIGH);
    }
    
    button_pressed = 0;

    // Check for Cancel and Custom held down for 1s for pairing mode
    if ((!digitalRead(BTN_CANCEL)) && (!digitalRead(row_input_pin[3]))) {
      delay(1000);
      if ((!digitalRead(BTN_CANCEL)) && (!digitalRead(row_input_pin[3]))) pairing_mode();
      while (pair_led_timeout_time > 0) {
        if (millis() >= pair_led_timeout_time) {
          pair_led_timeout_time = 0;
          digitalWrite(LED_GREEN, HIGH);
        }
      }
    }
  }
  // Always go back to sleep at the end of the main loop
  system_sleep();
}

//********************************************************************//
//------------------------------Functions-----------------------------//
//********************************************************************//

//---------------------------ISR Functions----------------------------//

void button_press_ISR(void) {
  if (pairing) {
    return;
  }
  
  system_wake();

  if (button_pressed == 0) {
    if (!digitalRead(BTN_CANCEL)) {
      button_pressed = CANCEL_VAL;
      return;
    }
    for (int col = 0; col < 4; col++) digitalWrite(col_output_pin[col], HIGH);
    for (int col = 0; col < 4; col++) {
      if (col > 0) digitalWrite(col_output_pin[col - 1], HIGH);
      digitalWrite(col_output_pin[col], LOW);
      delay(1);  // Allow capacitor to discharge
      for (int row = 0; row < 4; row++) {
        if (!digitalRead(row_input_pin[row])) {
          button_pressed = (row * 4) + (col + 1);
        }
      }
    }
    for (int col = 0; col < 3; col++) digitalWrite(col_output_pin[col], LOW);
  }
}

//---------------------------Main Functions----------------------------//

void system_sleep(void) {
  digitalWrite(NRF_PS_EN, HIGH);
  ADC0.CTRLA &= ~ADC_ENABLE_bm;
  sleep_cpu();
}

void system_wake(void) {
  digitalWrite(NRF_PS_EN, LOW);   // Power up NRF module
  ADC0.CTRLA |= ADC_ENABLE_bm;    // Enable ADC for battery voltage check
  delay(5);                       // Wait for NRF module to be ready
  nrf_setup();                    // Re-initialise NRF module
}

// Get battery volts/level (0-7)
uint8_t get_batt_level(void) {
  uint16_t v_batt_raw = analogRead(VBATT_PIN);

  if (debug) {
    SerialDebug.print("V batt raw = ");
    SerialDebug.println(v_batt_raw);
  }

  // Check if battery is below min voltage (Tx will not work reliably below this voltage)
  if (v_batt_raw < (VBAT_EMPTY_mV / VBAT_DIV)) {
    return 8;
  }

  batt_level = (uint8_t)map(v_batt_raw, VBAT_EMPTY_mV / VBAT_DIV, VBAT_FULL_mV / VBAT_DIV, 0, 7);
  if (batt_level > 7) batt_level = 7;
  return batt_level;
}

void pairing_mode(void) {
  uint8_t batt_level = get_batt_level();
  if (batt_level > 7) return;   // Not enough battery voltage to attempt pairing
  
  pairing = true;
  
  uint8_t pair_packet[8];
  pair_packet[0] = PAIR_BYTE;
  for (int i = 0; i < 6; i++) pair_packet[i + 1] = tx_pipe_uid[i];
  pair_packet[7] = batt_level;
  uint8_t rx_pair_address[6] = { TCS_PAIR_ID };

  radio.startListening();
  radio.setPayloadSize(8);
  radio.setPALevel(RF24_PA_LOW);
  radio.stopListening();
  radio.openWritingPipe(rx_pair_address);

  uint8_t pair_attempts = 0;
  while (pair_attempts < MAX_PAIR_ATTEMPTS) {
    digitalWrite(LED_BLUE, LOW);

    bool rx_ack = radio.write(&pair_packet, sizeof(pair_packet));
    delay(10);

    if (rx_ack) {
      digitalWrite(LED_BLUE, HIGH);
      digitalWrite(LED_GREEN, LOW);
      pair_led_timeout_time = millis() + 10000;   //10s LED on time after successful pairing
      radio.startListening();
      radio.setPayloadSize(8);
      radio.stopListening();
      radio.openWritingPipe(tx_pipe_uid);
      delay(200); // Wait for base station to switch to UID pipe

      uint8_t payload[8] = { 0 };
      payload[1] = get_batt_level();
      uint8_t ack_attempts = 0;
      while (ack_attempts < 20) {
        if (radio.write(&payload, sizeof(payload))) {
          digitalWrite(LED_BLUE, HIGH);
          for (int col = 0; col < 4; col++) digitalWrite(col_output_pin[col], LOW);
          radio.setPALevel(RF24_PA_HIGH);
          pairing = false;
          return true;
        }
        ack_attempts ++;
        delay(10);
      }
      for (int col = 0; col < 4; col++) digitalWrite(col_output_pin[col], LOW);
      radio.setPALevel(RF24_PA_HIGH);
      pairing = false;
      return false;
    }
    delay(500);
    digitalWrite(LED_BLUE, HIGH);
    delay(500);
    pair_attempts ++;
  }
  digitalWrite(LED_BLUE, HIGH);
  digitalWrite(LED_RED, LOW);
  delay(2000);
  digitalWrite(LED_RED, HIGH);
  for (int col = 0; col < 4; col++) digitalWrite(col_output_pin[col], LOW);
  radio.setPALevel(RF24_PA_HIGH);
  pairing = false;
  return false;
}


//--------------------------Setup Functions---------------------------//

// ADC setup
void adc_setup(void) {
  analogReference(INTERNAL1V024);
  analogReadResolution(10);
}

// MCU pins initialisation
void pin_setup(void) {
  for (int i = 0; i < 4; i++) {
    pinMode(col_output_pin[i], OUTPUT);
    digitalWrite(col_output_pin[i], LOW);
    pinMode(row_input_pin[i], INPUT);
  }
  pinMode(BTN_CANCEL, INPUT);
  pinMode(NRF_PS_EN, OUTPUT);
  digitalWrite(NRF_PS_EN, HIGH);
  pinMode(LED_RED, OUTPUT);
  digitalWrite(LED_RED, LOW);
  pinMode(LED_GREEN, OUTPUT);
  digitalWrite(LED_GREEN, LOW);
  pinMode(LED_BLUE, OUTPUT);
  digitalWrite(LED_BLUE, HIGH);
  delay(1000);
  digitalWrite(NRF_PS_EN, LOW);
}

// Interrupt Service Routines attachment
void isr_setup(void) {
  for (int i = 0; i < 4; i++) attachInterrupt(digitalPinToInterrupt(row_input_pin[i]), button_press_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(BTN_CANCEL), button_press_ISR, FALLING);
}

// Radio Tx setup
bool nrf_setup(void) {
  if (!radio.begin()) {
    if (debug) SerialDebug.println("NRF24L01 radio hardware is not responding");
    return false;
  }
  if (debug) SerialDebug.println("NRF24L01 initialised OK");
  radio.setPayloadSize(8);
  radio.setPALevel(RF24_PA_HIGH);  // RF24_PA_MAX is default.
  radio.stopListening();
  radio.openWritingPipe(tx_pipe_uid);

  return true;
}

// Serial for debug setup
void serial_for_debug_setup(void) {
  SerialDebug.begin(115200);
  SerialDebug.println("Starting remote control system...");
}
