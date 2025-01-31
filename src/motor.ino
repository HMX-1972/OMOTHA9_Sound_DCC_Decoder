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

//PWMスライス
uint32_t gSliceNum1;
uint32_t gSliceNum2;

#define DebugMotor

void motor_setup() {

  // Handle resetting CVs back to Factory Defaults
  // notifyCVResetFactoryDefault();
  // Plase Write CV8 for Default

  // Motor Setup
  gpio_set_function(PIN_MOTOR_A, GPIO_FUNC_PWM);
  gpio_set_function(PIN_MOTOR_B, GPIO_FUNC_PWM);

  gSliceNum1 = pwm_gpio_to_slice_num(PIN_MOTOR_A);
  gSliceNum2 = pwm_gpio_to_slice_num(PIN_MOTOR_B);

  pwm_set_wrap(gSliceNum1, 4095);
  pwm_set_wrap(gSliceNum2, 4095);
  pwm_set_clkdiv(gSliceNum1, 1.0147);
  pwm_set_clkdiv(gSliceNum2, 1.0147);

  // Set PWM output
  MOTOR_SetPWM(0, 0);

  // Set the PWM running
  pwm_set_enabled(gSliceNum1, true);
  pwm_set_enabled(gSliceNum2, true);

  vStart = Dcc.getCV(CV_VSTART);
  vHIGH = Dcc.getCV(CV_VHIGH);
  AccRatio = Dcc.getCV(CV_ACCRATIO);
  DeccRatio = Dcc.getCV(CV_DECCRATIO);
  Dir_rev = Dcc.getCV(CV61_DIR_REV);
  Kick_Time = Dcc.getCV(CV64_KICKSTART_T);
  Kick_Voltage = Dcc.getCV(CV65_KICKSTART_V);

  global_LocoValue[Loco_TargetSpeed] = 1;
  global_LocoValue[Loco_Speed] = 1;

}

void motor_kick() {
  static int kick_count = 0;
  uint8_t  newPwm = CurrentSpeed;
  if (newPwm > 1) {
    if (kick_count  > Kick_Time) {
      kick_count = 0;
      newPwm = max(CurrentSpeed ,  Kick_Voltage);
    }
    kick_count ++;
    Motor_Run(CurrentDirection, newPwm);
  }
}

void motor_loop() {
  static int DirectionChange = 0;

  // Handle Direction
  if (CurrentDirection != global_LocoValue[Loco_Directon]) {
    if (global_LocoValue[Loco_Speed] < 2) {
      CurrentDirection = global_LocoValue[Loco_Directon];
      DirectionChange = 0;
    } else {
      DirectionChange = 1;
    }
  }

  // Handle Speed changes
  if (global_LocoValue[Loco_Speed] != global_LocoValue[Loco_TargetSpeed] || DirectionChange ) {
    if (DirectionChange) {
      // Deceleration due to switch direction.
      global_LocoValue[Loco_Speed] = max(global_LocoValue[Loco_Speed] - DeccRatio, 1)  ;
    } else if (global_LocoValue[Loco_Speed] > global_LocoValue[Loco_TargetSpeed] ) {
      // Deceleration
      global_LocoValue[Loco_Speed] = max(global_LocoValue[Loco_Speed] - DeccRatio, global_LocoValue[Loco_TargetSpeed])  ;
    } else {
      if (global_run_Delay == 0 ) {
        // Acceleration
        global_LocoValue[Loco_Speed] = min(global_LocoValue[Loco_Speed] + AccRatio, global_LocoValue[Loco_TargetSpeed]) ;
      }
    }
  }

  // Handle Motor
  // Stop if speed = 0 or 1
  if (global_LocoValue[Loco_TargetSpeed] == 0) {
    global_LocoValue[Loco_Speed] = 1;
    CurrentSpeed = 0;
    Motor_Run(CurrentDirection, 0);
#ifdef DebugMotor
    Serial.println(" EMR / newPwm: 0");
#endif
  } else if (global_LocoValue[Loco_Speed] == 1) {
    CurrentSpeed = 0;
    Motor_Run(CurrentDirection, 0);
#ifdef DebugMotor
    Serial.println(" Zero / newPwm: 0");
#endif
  } else {
    uint8_t vScaleFactor;
    if ((vHIGH > 1) && (vHIGH > vStart))
      vScaleFactor = vHIGH - vStart;
    else
      vScaleFactor = 255 - vStart;

    uint8_t modSpeed = global_LocoValue[Loco_Speed] - 1;
    uint8_t modSteps = numSpeedSteps - 1;
    CurrentSpeed = (uint8_t)vStart + modSpeed * vScaleFactor / modSteps;

    Motor_Run(CurrentDirection, CurrentSpeed);

#ifdef DebugMotor
    Serial.print("New Speed: vStart: ");
    Serial.print(vStart);
    Serial.print(" vHIGH: ");
    Serial.print(vHIGH);
    Serial.print(" TargetSpeed: ");
    Serial.print(global_LocoValue[Loco_TargetSpeed]);
    Serial.print(" modSpeed: ");
    Serial.print(modSpeed);
    Serial.print(" vScaleFactor: ");
    Serial.print(vScaleFactor);
    Serial.print(" modSteps: ");
    Serial.print(modSteps);
    Serial.print(" newPwm: ");
    Serial.println(CurrentSpeed);
#endif
  }
}

void MOTOR_SetPWM(uint16_t inPWM_A, uint16_t inPWM_B) {
  pwm_set_chan_level(gSliceNum2, PWM_CHAN_A, inPWM_A);
  pwm_set_chan_level(gSliceNum1, PWM_CHAN_B, inPWM_B);
}

void Motor_Run(uint8_t Direction, uint8_t Speed) {
  if (Speed < 1) {
    MOTOR_SetPWM(0, 0);
  } else {
    if (Direction == Dir_rev) {
      /*
        // Motor Assist *Experimental*
        if (global_FuncValue[9] == 1 && Speed < 64) {
        MOTOR_SetPWM(4095, 0);
        delay(2);
        }
      */
      MOTOR_SetPWM(Speed << 4, 0);
    } else {
      /*
        // Motor Assist *Experimental*
        if (global_FuncValue[9] == 1 && Speed < 64) {
        MOTOR_SetPWM(0, 4095);
        delay(2);
        }
      */
      MOTOR_SetPWM(0, Speed << 4);
    }
  }
#ifdef DebugMotor
  Serial.print("Direction: ");
  Serial.print(Direction);
  Serial.print("/");
  Serial.println(Dir_rev);
#endif
}
