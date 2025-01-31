/////////////////////////////////////////////////////////////////////////////////
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
// license:  LGPL-2.1 license
//
// file:     NmraDcc_pio.cpp
// file:     NmraDcc_pio.h
// file:     dcc_pulse_dec.h
// author:   Desktop Station
// license:  Open source(Not specified)
//
// file:     DccCV.h
// author:   Ayanosuke(Maison de DCC) / Desktop Station
// license:  MIT License
//
// file:     Timer_test2_752_31.ino
// author:   fujigaya
// license:  Open source(Not specified)
//
//
//////////////////////////////////////////////////////////////////////////////////
//
// Ver 1.01 First release
// Ver 1.03 Update watchdog
//
//////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/watchdog.h"
#include "dcc_pulse_dec.h"
#include "DccCV.h"
#include "NmraDcc_pio.h"

#define Version 1.03

// Pin //
#define PIN_MOTOR_A 1  // For ZERO
#define PIN_MOTOR_B 2  // For ZERO
#define PIN_PCM 5      // For ZERO
#define PIN_AMP_SW 6   // For ZERO
#define PIN_DCC 7      // For ZERO
#define PIN_LED 16     // For ZERO

#define PIN_LED1_SP 13  // For ZERO
#define PIN_LED1_HL 12  // For ZERO
#define PIN_LED1_LT 11  // For ZERO
#define PIN_LED1_LH 10  // For ZERO
#define PIN_LED1_RH 9   // For ZERO
#define PIN_LED1_RT 8   // For ZERO

#define PIN_LED2_SP 14  // For ZERO
#define PIN_LED2_HL 15  // For ZERO 
#define PIN_LED2_LT 26  // For ZERO
#define PIN_LED2_LH 27  // For ZERO
#define PIN_LED2_RH 28  // For ZERO
#define PIN_LED2_RT 29  // For ZERO

#define Loco_Speed 0         // For Speed
#define Loco_Directon 1      // For Acceraretion
#define Loco_TargetSpeed 2   // For Stage
#define Loco_Acceraretion 3  // For Acceraretion

// Dcc //
#define MAX_FUNC_NUMBER 29
#define LOCO_LENGTH 8

// Variables //
uint8_t global_FuncValue[MAX_FUNC_NUMBER];  // Func 01-28
uint8_t global_LocoValue[LOCO_LENGTH];      // 0:Speed, 1:Direction , 2:Target Speed, 4:Acceraretion
uint8_t global_Draft = 0;                   // 0:OFF
uint8_t global_ave_Draft = 0;               // 0:OFF
uint8_t global_run_Delay = 0;               // 0:OFF
unsigned long loop_time_2;
unsigned long loop_time_10;
unsigned long loop_time_250;
bool watch_dog_dcc = false;
bool watch_dog_run = false;

void setup() {
  // for debug
  Serial.begin(115200);
  // DCC
  dcc_setup();
  // Motor
  motor_setup();
  // Sound
  sound_setup();
  // LED
  led_Setup();
  // timer
  loop_time_2 = millis();
  loop_time_10 = millis();
  loop_time_250 = millis() - 250;
  //watchdog
  watchdog_reboot(0, 0, 500);
}

void loop() {
  // DCC
  dcc_loop();
  // Sound
  sound_loop();
  // 1ms Loop
  if (millis() - loop_time_2 > 2) {
    // Motor
    motor_kick();
    loop_time_2 = millis();
  }
  // 10ms Loop
  if (millis() - loop_time_10 > 10) {
    // LED
    led_loop();
    // Timer
    loop_time_10 = millis();
  }
  // 250ms Loop
  if (millis() - loop_time_250 > 250) {
    // Motor
    motor_loop();
    //watchdog for dcc
    if (not watch_dog_dcc && watch_dog_run) {
      dcc_setup();
      watch_dog_dcc = false;
    }
    // Timer
    loop_time_250 = millis();
  }
  //watchdog for all
  watchdog_update();
}
