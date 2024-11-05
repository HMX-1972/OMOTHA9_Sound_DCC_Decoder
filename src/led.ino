//////////////////////////////////////////////////////////////////////////////////
//
// P029 DCC Sound Loco
//
// led.ino
//
// Copyright(C)'2022 - 2024 HMX
// This software is released under the MIT License, see LICENSE.txt
//
//////////////////////////////////////////////////////////////////////////////////

#include <Adafruit_NeoPixel.h>

#define LED1_SP 0
#define LED1_HL 1
#define LED1_LT 2
#define LED1_LH 3
#define LED1_RH 4
#define LED1_RT 5
#define LED2_SP 6
#define LED2_HL 7
#define LED2_LT 8
#define LED2_LH 9
#define LED2_RH 10
#define LED2_RT 11
#define MAX_LED_NUMBER 12

#define RGB_OFF 0
#define RGB_ALL 1
#define RGB_R   2
#define RGB_G   3
#define RGB_B   4

// create a pixel strand with 1 pixel on PIN_NEOPIXEL
Adafruit_NeoPixel pixels(1, PIN_LED);
uint8_t color = 0, count = 0;
uint32_t colors[] = {pixels.Color(0, 0, 0), pixels.Color(125, 125, 125), pixels.Color(125, 0, 0), pixels.Color(0, 125, 0), pixels.Color(0, 0, 125)};
const uint8_t COLORS_LEN = (uint8_t)(sizeof(colors) / sizeof(colors[0]));

uint8_t LED_CURRENT[MAX_LED_NUMBER] ;
uint8_t LED_TARGET[MAX_LED_NUMBER] ;
uint8_t LED_PIN[MAX_LED_NUMBER] ;

void led_Setup() {

  // initialize
  for (int i = 0; i < MAX_FUNC_NUMBER; i++) {
    global_FuncValue[i] = 0;
  }
  // initialize the LED pin as an output:
  pixels.begin();  // initialize the pixel

  // initialize the Light pin as an output:
  LED_PIN[0] = PIN_LED1_SP ;
  LED_PIN[1] = PIN_LED1_HL ;
  LED_PIN[2] = PIN_LED1_LT ;
  LED_PIN[3] = PIN_LED1_LH ;
  LED_PIN[4] = PIN_LED1_RH ;
  LED_PIN[5] = PIN_LED1_RT ;
  LED_PIN[6] = PIN_LED2_SP ;
  LED_PIN[7] = PIN_LED2_HL ;
  LED_PIN[8] = PIN_LED2_LT ;
  LED_PIN[9] = PIN_LED2_LH ;
  LED_PIN[10] = PIN_LED2_RH ;
  LED_PIN[11] = PIN_LED2_RT ;

  for (uint8_t i = 0; i < MAX_LED_NUMBER; i++) {
    pinMode(LED_PIN[i] , OUTPUT);
  }

  Led_rev = Dcc.getCV(CV62_LED_REV);
}

void led_loop() {
  static uint8_t ex_FuncValue[MAX_FUNC_NUMBER]; // Func 0-28
  static uint8_t ex_Direction = CurrentDirection;

  // Func==0 : LED //
  if (ex_FuncValue[0] != global_FuncValue[0] || ex_Direction != CurrentDirection) {
    head_light(global_FuncValue[0]);
    tail_light(global_FuncValue[0]);
    ex_FuncValue[0] = global_FuncValue[0];
    ex_Direction = CurrentDirection;
  }

  // Func==6 : SWITCH //
  if (ex_FuncValue[6] != global_FuncValue[6]) {
    if (global_FuncValue[6] == 1) {
      switch_light(1);
    } else {
      head_light(global_FuncValue[0]);
      tail_light(global_FuncValue[0]);
    }
    ex_FuncValue[6] = global_FuncValue[6];
  }

  // Func==7 : Bell //
  if (global_FuncValue[7] == 0 && ex_FuncValue[7] != global_FuncValue[7]) {
    head_light(global_FuncValue[0]);
    tail_light(global_FuncValue[0]);
    ex_FuncValue[7] = global_FuncValue[7];
  }
  if (global_FuncValue[7] == 1 ) {
    static int LED_Cycle = 0;
    LED_Cycle++;
    switch (LED_Cycle) {
      case 20:
        if (CurrentDirection == Led_rev) {
          LED_TARGET[LED1_LH] = 1 ;
          //LED_TARGET[LED1_LT] = 0 ;
          //LED_TARGET[LED1_SP] = 0 ;
          //LED_TARGET[LED1_HL] = 0 ;
          //LED_TARGET[LED1_RT] = 1 ;
          LED_TARGET[LED1_RH] = 0 ;
        } else {
          LED_TARGET[LED2_LH] = 1 ;
          //LED_TARGET[LED2_LT] = 0 ;
          //LED_TARGET[LED2_SP] = 0 ;
          //LED_TARGET[LED2_HL] = 0 ;
          //LED_TARGET[LED2_RT] = 1 ;
          LED_TARGET[LED2_RH] = 0 ;
        }
        break;
      case 40:
        if (CurrentDirection == Led_rev) {
          LED_TARGET[LED1_LH] = 0 ;
          //LED_TARGET[LED1_LT] = 1 ;
          //LED_TARGET[LED1_SP] = 1 ;
          //LED_TARGET[LED1_HL] = 1 ;
          //LED_TARGET[LED1_RT] = 0 ;
          LED_TARGET[LED1_RH] = 1 ;
        } else {
          LED_TARGET[LED2_LH] = 0 ;
          //LED_TARGET[LED2_LT] = 1 ;
          //LED_TARGET[LED2_SP] = 0 ;
          //LED_TARGET[LED2_HL] = 0 ;
          //LED_TARGET[LED2_RT] = 0 ;
          LED_TARGET[LED2_RH] = 1 ;
        }
        LED_Cycle = 0;
        break;

      default:
        break;
    }
    ex_FuncValue[7] = global_FuncValue[7];
  }

  // Func==8 : SOUND //
  if (global_FuncValue[8] == 0 && ex_FuncValue[8] != global_FuncValue[8]) {
    head_light(global_FuncValue[0]);
    tail_light(global_FuncValue[0]);
    pixels.clear();
    pixels.show();
    ex_FuncValue[8] = global_FuncValue[8];
  }
  if (global_FuncValue[8] == 1 ) {
    static int LED_Cycle = 0;
    LED_Cycle++;
    switch (LED_Cycle) {
      case 15:
        LED_TARGET[LED1_LH] = 1 ;
        LED_TARGET[LED1_LT] = 0 ;
        //LED_TARGET[LED1_SP] = 0 ;
        //LED_TARGET[LED1_HL] = 0 ;
        LED_TARGET[LED1_RT] = 1 ;
        LED_TARGET[LED1_RH] = 0 ;
        LED_TARGET[LED2_LH] = 1 ;
        LED_TARGET[LED2_LT] = 0 ;
        //LED_TARGET[LED2_SP] = 0 ;
        //LED_TARGET[LED2_HL] = 0 ;
        LED_TARGET[LED2_RT] = 1 ;
        LED_TARGET[LED2_RH] = 0 ;
        pixels.setPixelColor(0, colors[RGB_R]);
        pixels.show();
        break;
      case 30:
        LED_TARGET[LED1_LH] = 0 ;
        LED_TARGET[LED1_LT] = 1 ;
        //LED_TARGET[LED1_SP] = 1 ;
        //LED_TARGET[LED1_HL] = 1 ;
        LED_TARGET[LED1_RT] = 0 ;
        LED_TARGET[LED1_RH] = 1 ;
        LED_TARGET[LED2_LH] = 0 ;
        LED_TARGET[LED2_LT] = 1 ;
        //LED_TARGET[LED2_SP] = 0 ;
        //LED_TARGET[LED2_HL] = 0 ;
        LED_TARGET[LED2_RT] = 0 ;
        LED_TARGET[LED2_RH] = 1 ;
        pixels.setPixelColor(0, colors[RGB_G]);
        pixels.show();
        LED_Cycle = 0;
        break;

      default:
        break;
    }
    ex_FuncValue[8] = global_FuncValue[8];
  }

  // Func==28 : LED //
  if (ex_FuncValue[28] != global_FuncValue[28]) {
    for (uint8_t i = 0; i < MAX_LED_NUMBER; i++) {
      if (global_FuncValue[28] == 0) {
        LED_TARGET[i] = 0;
      } else {
        LED_TARGET[i] = 1;
      }
    }
    ex_FuncValue[28] = global_FuncValue[28];
  }
  led_set();
}

void head_light(uint8_t sw) {
  if (sw == 0) {
    LED_TARGET[LED1_LT] = 0;
    LED_TARGET[LED1_RT] = 0;
    LED_TARGET[LED2_LT] = 0;
    LED_TARGET[LED2_RT] = 0;
  } else {
    if (CurrentDirection == Led_rev) {
      LED_TARGET[LED1_LT] = 0;
      LED_TARGET[LED1_RT] = 0;
      LED_TARGET[LED2_LT] = 1;
      LED_TARGET[LED2_RT] = 1;
    } else {
      LED_TARGET[LED1_LT] = 1;
      LED_TARGET[LED1_RT] = 1;
      LED_TARGET[LED2_LT] = 0;
      LED_TARGET[LED2_RT] = 0;
    }
    Serial.print("Direction: ");
    Serial.print(CurrentDirection);
    Serial.print("/");
    Serial.println(Led_rev);
  }
}

void tail_light(uint8_t sw) {
  if (sw == 0) {
    LED_TARGET[LED1_SP] = 0 ;
    LED_TARGET[LED1_HL] = 0 ;
    LED_TARGET[LED1_LH] = 0 ;
    LED_TARGET[LED1_RH] = 0 ;
    LED_TARGET[LED2_SP] = 0 ;
    LED_TARGET[LED2_HL] = 0 ;
    LED_TARGET[LED2_LH] = 0 ;
    LED_TARGET[LED2_RH] = 0 ;
  } else {
    if (CurrentDirection == Led_rev) {
      LED_TARGET[LED1_SP] = 1 ;
      LED_TARGET[LED1_HL] = 1 ;
      LED_TARGET[LED1_LH] = 1 ;
      LED_TARGET[LED1_RH] = 1 ;
      LED_TARGET[LED2_SP] = 0 ;
      LED_TARGET[LED2_HL] = 0 ;
      LED_TARGET[LED2_LH] = 0 ;
      LED_TARGET[LED2_RH] = 0 ;
    } else {
      LED_TARGET[LED1_SP] = 0 ;
      LED_TARGET[LED1_HL] = 0 ;
      LED_TARGET[LED1_LH] = 0 ;
      LED_TARGET[LED1_RH] = 0 ;
      LED_TARGET[LED2_SP] = 1 ;
      LED_TARGET[LED2_HL] = 1 ;
      LED_TARGET[LED2_LH] = 1 ;
      LED_TARGET[LED2_RH] = 1 ;
    }
  }
}

void switch_light(uint8_t sw) {
  if (sw == 0) {
    LED_TARGET[LED1_LT] = 0;
    LED_TARGET[LED1_RT] = 0;
    LED_TARGET[LED2_LT] = 0;
    LED_TARGET[LED2_RT] = 0;
  } else {
    LED_TARGET[LED1_LT] = 1;
    LED_TARGET[LED1_RT] = 0;
    LED_TARGET[LED2_LT] = 1;
    LED_TARGET[LED2_RT] = 0;
  }
}

void led_all_on() {
  for (uint8_t i = 0; i < MAX_LED_NUMBER; i++) {
    LED_TARGET[i] = 1 ;
  }
}

void led_all_off() {
  for (uint8_t i = 0; i < MAX_LED_NUMBER; i++) {
    LED_TARGET[i] = 0 ;
  }
}

void led_set() {
  for (uint8_t i = 0; i < MAX_LED_NUMBER; i++) {
    if (LED_CURRENT[i] != LED_TARGET[i]) {
      if ( LED_TARGET[i] == 0 ) {
        digitalWrite(LED_PIN[i], LOW);
      } else {
        digitalWrite(LED_PIN[i], HIGH);
      }
      LED_CURRENT[i] = LED_TARGET[i];
    }
  }
}
