//
//  https://desktopstation.net/blog/2022/05/23/pioasmでDCCパルス幅をカウントしたものをパケットデコードする/
//
//  pioasmを使って、dccpulse.pioからdcc_pulse_dec.hを生成しています。
//  https://wokwi.com/tools/pioasm
//

#include "pico/stdlib.h"
#include "hardware/pwm.h"

PIO gPio;
uint gPio_SM;
unsigned long gRecvPulseWidth[8];
byte gRecvPulseBit[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

//使用クラスの宣言
NmraDcc Dcc;
DCC_MSG Packet;

struct CVPair {
  uint16_t CV;
  uint8_t Value;
};

// Default CV Values Table
CVPair FactoryDefaultCVs[] = {
  // The CV Below defines the Short DCC Address
  { CV_MULTIFUNCTION_PRIMARY_ADDRESS, DEFAULT_DECODER_ADDRESS },

  // Three Step Speed Table
  { CV_VSTART, 30 },
  { CV_VHIGH, 255 },
  { CV_ACCRATIO, 4 },
  { CV_DECCRATIO, 4 },
  { CV60_MASTERVOL, 2 },
  { CV61_DIR_REV, 0 },
  { CV62_LED_REV, 0 },

  // These two CVs define the Long DCC Address
  { CV_MULTIFUNCTION_EXTENDED_ADDRESS_MSB, CALC_MULTIFUNCTION_EXTENDED_ADDRESS_MSB(DEFAULT_DECODER_ADDRESS) },
  { CV_MULTIFUNCTION_EXTENDED_ADDRESS_LSB, CALC_MULTIFUNCTION_EXTENDED_ADDRESS_LSB(DEFAULT_DECODER_ADDRESS) },

  // ONLY uncomment 1 CV_29_CONFIG line below as approprate
  { CV_29_CONFIG, CV29_F0_LOCATION },  // Short Address 28/128 Speed Steps
  //{CV_29_CONFIG, CV29_EXT_ADDRESSING | CV29_F0_LOCATION}, // Long  Address 28/128 Speed Steps
};

//uint8_t FactoryDefaultCVIndex = sizeof(FactoryDefaultCVs) / sizeof(CVPair);
uint8_t FactoryDefaultCVIndex = 0;

// Some global state variables

uint8_t CurrentDirection = 0;
uint8_t numSpeedSteps = SPEED_STEP_128;

uint8_t vStart;
uint8_t vHIGH;
uint8_t AccRatio;
uint8_t DeccRatio;
uint8_t MasterVol;
uint8_t Dir_rev;
uint8_t Led_rev;
//---------------------------------------------------------------------
//---------------------------------------------------------------------
//---------------------------------------------------------------------
void dcc_setup() {
  //PIOのch 0を確保
  gPio = pio0;

  //アセンブラのPIOプログラムへのアドレス
  uint offset = pio_add_program(gPio, &dcc_pulse_dec_program);

  // 空いているステートマシンの番号を取得
  gPio_SM = pio_claim_unused_sm(gPio, true);

  // PIOプログラムを初期化
  dcc_pulse_dec_init(gPio, gPio_SM, offset, PIN_DCC);

  // DCC信号ピンをPullupする HMX 2024-06-02
  gpio_pull_up(PIN_DCC);

  // Call the main DCC Init function to enable the DCC Receiver
  Dcc.init(140, 1, FLAGS_MY_ADDRESS_ONLY, 0);
}

//---------------------------------------------------------------------
//---------------------------------------------------------------------
//---------------------------------------------------------------------
void dcc_loop() {
  // You MUST call the NmraDcc.process() method frequently from the Arduino loop() function for correct library operation
  Dcc.process();

  //PIO RX FIFO check
  if (pio_sm_is_rx_fifo_full(gPio, gPio_SM) == true) {
    //FIFOから速く抜き取る(抜かないと処理が止まる)
    for (int i = 0; i < 8; i++) {
      gRecvPulseWidth[i] = pio_sm_get(gPio, gPio_SM);
    }

    //中身の表示
    //Serial.println("RECV:");

    for (int i = 0; i < 8; i++) {
      //print32bits(gRecvPulseWidth[i]);

      byte aDCCBits = 0;

      for (int j = 0; j < 8; j++) {
        unsigned long aBitPulse = getBit((gRecvPulseWidth[i] >> (j * 4)) & 15);
        aDCCBits = aDCCBits + (aBitPulse << j);
        PushDCCbit(aBitPulse);
      }

      //Little Endian
      gRecvPulseBit[i] = aDCCBits;
      //Serial.println(aDCCBits, BIN);
    }

  } else {
    //Serial.print(".");
  }

  // Handle resetting CVs back to Factory Defaults
  if (FactoryDefaultCVIndex && Dcc.isSetCVReady()) {
    FactoryDefaultCVIndex--;  // Decrement first as initially it is the size of the array
    Dcc.setCV(FactoryDefaultCVs[FactoryDefaultCVIndex].CV, FactoryDefaultCVs[FactoryDefaultCVIndex].Value);
  }
}

//---------------------------------------------------------------------
//---------------------------------------------------------------------
//---------------------------------------------------------------------

void print32bits(unsigned long inBuf) {

  Serial.print(getUS(inBuf & 15));
  Serial.print(" ");
  Serial.print(getUS((inBuf >> 4) & 15));
  Serial.print(" ");
  Serial.print(getUS((inBuf >> 8) & 15));
  Serial.print(" ");
  Serial.print(getUS((inBuf >> 12) & 15));
  Serial.print(" ");
  Serial.print(getUS((inBuf >> 16) & 15));
  Serial.print(" ");
  Serial.print(getUS((inBuf >> 20) & 15));
  Serial.print(" ");
  Serial.print(getUS((inBuf >> 24) & 15));
  Serial.print(" ");
  Serial.println(getUS((inBuf >> 28) & 15));
}

unsigned long getUS(unsigned long inVal) {
  return (15 - inVal) * 14;
}

byte getBit(unsigned long inVal) {
  byte aRet = 0;
  switch (15 - inVal) {
    case 3:
    case 4:
    case 5:
      aRet = 1;
      break;
    default:
      aRet = 0;
      break;
  }
  return aRet;
}

//---------------------------------------------------------------------
//DCC速度信号の受信によるイベント
// SppedSteps 127で,Speed 127。MAX aSpeedRefは127:65019
//---------------------------------------------------------------------
// Delete

//---------------------------------------------------------------------------
//ファンクション信号受信のイベント
//---------------------------------------------------------------------------
extern void notifyDccFunc(uint16_t Addr, DCC_ADDR_TYPE AddrType, FN_GROUP FuncGrp, uint8_t FuncState) {
  // デバッグメッセージ
  /*
    Serial.println("notifyDccFunc()");
    Serial.print("Addr:");
    Serial.print(Addr);
    Serial.print(", AddrType:");
    Serial.print(AddrType);
    Serial.print(", FuncGrp: ");
    Serial.print(FuncGrp, HEX);
    Serial.print(", FuncState: ");
    Serial.println(FuncState, HEX);
  */
  switch (FuncGrp) {
#ifdef NMRA_DCC_ENABLE_14_SPEED_STEP_MODE
    case FN_0:
      if (FuncState & FN_BIT_00) {
        global_FuncValue[0] = 1;
      } else {
        global_FuncValue[0] = 0;
      }
      break;
#endif
    case FN_0_4:
      if (Dcc.getCV(CV_29_CONFIG) & CV29_F0_LOCATION)  // Only process Function 0 in this packet if we're not in Speed Step 14 Mode
      {
        if (FuncState & FN_BIT_00) {
          global_FuncValue[0] = 1;
        } else {
          global_FuncValue[0] = 0;
        }
      }
      if (FuncState & FN_BIT_01) {
        global_FuncValue[1] = 1;
      } else {
        global_FuncValue[1] = 0;
      }
      if (FuncState & FN_BIT_02) {
        global_FuncValue[2] = 1;
      } else {
        global_FuncValue[2] = 0;
      }
      if (FuncState & FN_BIT_03) {
        global_FuncValue[3] = 1;
      } else {
        global_FuncValue[3] = 0;
      }
      if (FuncState & FN_BIT_04) {
        global_FuncValue[4] = 1;
      } else {
        global_FuncValue[4] = 0;
      }
      break;

    case FN_5_8:
      if (FuncState & FN_BIT_05) {
        global_FuncValue[5] = 1;
      } else {
        global_FuncValue[5] = 0;
      }
      if (FuncState & FN_BIT_06) {
        global_FuncValue[6] = 1;
      } else {
        global_FuncValue[6] = 0;
      }
      if (FuncState & FN_BIT_07) {
        global_FuncValue[7] = 1;
      } else {
        global_FuncValue[7] = 0;
      }
      if (FuncState & FN_BIT_08) {
        global_FuncValue[8] = 1;
      } else {
        global_FuncValue[8] = 0;
      }
      break;

    case FN_9_12:
      if (FuncState & FN_BIT_09) {
        global_FuncValue[9] = 1;
      } else {
        global_FuncValue[9] = 0;
      }
      if (FuncState & FN_BIT_10) {
        global_FuncValue[10] = 1;
      } else {
        global_FuncValue[10] = 0;
      }
      if (FuncState & FN_BIT_11) {
        global_FuncValue[11] = 1;
      } else {
        global_FuncValue[11] = 0;
      }
      if (FuncState & FN_BIT_12) {
        global_FuncValue[12] = 1;
      } else {
        global_FuncValue[12] = 0;
      }
      break;

    case FN_13_20:
      if (FuncState & FN_BIT_13) {
        global_FuncValue[13] = 1;
      } else {
        global_FuncValue[13] = 0;
      }
      if (FuncState & FN_BIT_14) {
        global_FuncValue[14] = 1;
      } else {
        global_FuncValue[14] = 0;
      }
      if (FuncState & FN_BIT_15) {
        global_FuncValue[15] = 1;
      } else {
        global_FuncValue[15] = 0;
      }
      if (FuncState & FN_BIT_16) {
        global_FuncValue[16] = 1;
      } else {
        global_FuncValue[16] = 0;
      }
      if (FuncState & FN_BIT_17) {
        global_FuncValue[17] = 1;
      } else {
        global_FuncValue[17] = 0;
      }
      if (FuncState & FN_BIT_18) {
        global_FuncValue[18] = 1;
      } else {
        global_FuncValue[18] = 0;
      }
      if (FuncState & FN_BIT_19) {
        global_FuncValue[19] = 1;
      } else {
        global_FuncValue[19] = 0;
      }
      if (FuncState & FN_BIT_20) {
        global_FuncValue[20] = 1;
      } else {
        global_FuncValue[20] = 0;
      }
      break;

    case FN_21_28:
      if (FuncState & FN_BIT_21) {
        global_FuncValue[21] = 1;
      } else {
        global_FuncValue[21] = 0;
      }
      if (FuncState & FN_BIT_22) {
        global_FuncValue[22] = 1;
      } else {
        global_FuncValue[22] = 0;
      }
      if (FuncState & FN_BIT_23) {
        global_FuncValue[23] = 1;
      } else {
        global_FuncValue[23] = 0;
      }
      if (FuncState & FN_BIT_24) {
        global_FuncValue[24] = 1;
      } else {
        global_FuncValue[24] = 0;
      }
      if (FuncState & FN_BIT_25) {
        global_FuncValue[25] = 1;
      } else {
        global_FuncValue[25] = 0;
      }
      if (FuncState & FN_BIT_26) {
        global_FuncValue[26] = 1;
      } else {
        global_FuncValue[26] = 0;
      }
      if (FuncState & FN_BIT_27) {
        global_FuncValue[27] = 1;
      } else {
        global_FuncValue[27] = 0;
      }
      if (FuncState & FN_BIT_28) {
        global_FuncValue[28] = 1;
      } else {
        global_FuncValue[28] = 0;
      }
      break;
  }
}

//---------------------------------------------------------------------------
// HMX Modefied
//---------------------------------------------------------------------------

void notifyCVAck(void) {
#ifdef DEBUG_DCC_ACK
  Serial.println("notifyCVAck");
#endif

  MOTOR_SetPWM(4095, 0);
  delay(8);
  MOTOR_SetPWM(0, 0);
}

// This call-back function is called when a CV Value changes so we can update CVs we're using
void notifyCVChange(uint16_t CV, uint8_t Value) {
  switch (CV) {
    case CV_VSTART:
      vStart = Value;
      break;

    case CV_VHIGH:
      vHIGH = Value;
      break;

    case CV_ACCRATIO:
      AccRatio = Value;
      break;

    case CV_DECCRATIO:
      DeccRatio = Value;
      break;

    case CV60_MASTERVOL:
      MasterVol = Value;
      break;

    case CV61_DIR_REV:
      Dir_rev = Value;
      break;

    case CV62_LED_REV:
      Led_rev = Value;
      break;
  }
}

void notifyCVResetFactoryDefault() {
  // Make FactoryDefaultCVIndex non-zero and equal to num CV's to be reset
  // to flag to the loop() function that a reset to Factory Defaults needs to be done
  FactoryDefaultCVIndex = sizeof(FactoryDefaultCVs) / sizeof(CVPair);
};


// This call-back function is called whenever we receive a DCC Speed packet for our address
void notifyDccSpeed(uint16_t Addr, DCC_ADDR_TYPE AddrType, uint8_t Speed, DCC_DIRECTION Dir, DCC_SPEED_STEPS SpeedSteps) {
#ifdef DEBUG_SPEED
  Serial.print("notifyDccSpeed: Addr: ");
  Serial.print(Addr, DEC);
  Serial.print((AddrType == DCC_ADDR_SHORT) ? "-S" : "-L");
  Serial.print(" Speed: ");
  Serial.print(Speed, DEC);
  Serial.print(" Steps: ");
  Serial.print(SpeedSteps, DEC);
  Serial.print(" Dir: ");
  Serial.println((Dir == DCC_DIR_FWD) ? "Forward" : "Reverse");
#endif

  global_LocoValue[Loco_TargetSpeed] = Speed, DEC;
  if (Dir == DCC_DIR_FWD) {
    global_LocoValue[Loco_Directon] = 0;
  } else {
    global_LocoValue[Loco_Directon] = 1;
  }
  watch_dog_run = true;
  watch_dog_dcc = true;
};
