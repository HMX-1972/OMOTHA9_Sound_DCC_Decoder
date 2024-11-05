//////////////////////////////////////////////////////////////////////////////////
//
// P029 DCC Sound Loco
//
// Basic Program
//
// Copyright(C)'2022 - 2024 HMX
// This software is released under the MIT License, see LICENSE.txt
//
// This software uses, diverts or references the following software.
// Please confirm the original text for each license.
//
// file:     NmraDcc.cpp
// file:     NmraDcc.h
// file:     NmraDccMultiFunctionMotorDecoder.ino
// author:   Alex Shepherd
// License:  LGPL-2.1 license
//
// file:     NmraDcc_pio.cpp
// file:     NmraDcc_pio.h
// file:     dcc_pulse_dec.h
// author:   Desktop Station
// License:  Open source(Not specified)
//
// file:     DccCV.h
// author:   Ayanosuke(Maison de DCC) / Desktop Station
// License:  MIT License
//
// file:     Timer_test2_752_31.ino
// author:   fujigaya
// License:  Open source(Not specified)
//
//
//////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "dcc_pulse_dec.h"
#include "DccCV.h"
#include "NmraDcc_pio.h"

#define Version 0.01

// Pin //
#define PIN_MOTOR_A  1       // For ZERO
#define PIN_MOTOR_B  2       // For ZERO
//#define PIN_PCM    5       // For ZERO
//#define PIN_AMP_SW 6       // For ZERO
#define PIN_DCC      7       // For ZERO
#define PIN_LED     16       // For ZERO

#define PIN_LED1_SP 13       // For ZERO
#define PIN_LED1_HL 12       // For ZERO
#define PIN_LED1_LT 11       // For ZERO
#define PIN_LED1_LH 10       // For ZERO
#define PIN_LED1_RH  9       // For ZERO
#define PIN_LED1_RT  8       // For ZERO

#define PIN_LED2_SP 14       // For ZERO
#define PIN_LED2_HL 15       // For ZERO
#define PIN_LED2_LT 26       // For ZERO
#define PIN_LED2_LH 27       // For ZERO
#define PIN_LED2_RH 28       // For ZERO
#define PIN_LED2_RT 29       // For ZERO

// Dcc //
#define MAX_FUNC_NUMBER 29

// Variables //
uint8_t global_FuncValue[MAX_FUNC_NUMBER]; // Func 01-28
unsigned long loop_time_10 = 0;

void setup() {
  // for debug
  Serial.begin(115200);

  // DCC
  dcc_setup();
  // LED
  led_Setup();
  // timer
  loop_time_10 = millis();
}

void loop() {
  // DCC
  dcc_loop();
  // 10ms Loop
  if ( millis() - loop_time_10  > 10)
  {
    led_loop();
    loop_time_10 = millis();
  }
}
