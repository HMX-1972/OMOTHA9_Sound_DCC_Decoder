
// The original of this file is:
// file:     Timer_test2_752_31.ino
// author:   fujigaya
// License:  Open source(Not specified)
// URL:      https://fujigaya2.blog.ss-blog.jp/2022-01-16

#include <hardware/pwm.h>
#include <LittleFS.h>

// #define DebugSound

#define TIMER1_INTERVAL_us 62  // 16kHz 62
#define BUF_LENGTH 2048
#define WAVE_CHORDS 8   //Number of chords
#define WAVE_OFFSET 44  //Wave offset. There is 43 header area at the beginning of the wave file, so skip that.

//wave_mode enum
#define WAVE_MODE_STOP 0
#define WAVE_MODE_RUN_START 1
#define WAVE_MODE_RUNNING 2
#define WAVE_MODE_RUN_END 3
#define WAVE_MODE_RUN_REPEAT 4
#define WAVE_MODE_RUN_NEXT 5  // Add by HMX 20220117
#define WAVE_MODE_NONE 6      // Chage from 5 to 6 by HMX 20220117

#define WAVE_DEFAULT_VALUE 127  // Value for silence

#define BUF_CH_A 0
#define BUF_CH_B 1

//Sound_Ch
#define SOUND_CH_ENGINE  0
#define SOUND_CH_BRAKE   1
#define SOUND_CH_HORN    2
#define SOUND_CH_RAIL1   3
#define SOUND_CH_RAIL2   4
#define SOUND_CH_OPTION1 5
#define SOUND_CH_OPTION2 6
#define SOUND_CH_OPTION3 7

//timer object
repeating_timer_t timer;

//Each Ch
uint32_t wave_position[WAVE_CHORDS];       //Current
uint32_t wave_length[WAVE_CHORDS];         //Length
uint16_t wave_volume[WAVE_CHORDS];         //volume 0-255
String wave_name[WAVE_CHORDS];             //File Name
String wave_next_name[WAVE_CHORDS];        //File Name
String wave_next_after_name[WAVE_CHORDS];  //File Name
uint8_t wave_last_val[WAVE_CHORDS];        //A value to keep the last sound to eliminate pops.
int wave_status[WAVE_CHORDS];              //Status : WAVE_MODE_STOP,WAVE_MODE_RUN
int wave_order[WAVE_CHORDS];               //Order : WAVE_MODE_STOP,WAVE_MODE_RUN

//Output part: Volatile, because it is used in interrupts.
volatile uint slice_num = 0;
volatile uint16_t wave_buf_a[BUF_LENGTH];
volatile uint16_t wave_buf_b[BUF_LENGTH];
volatile int buf_ab = 1;
volatile uint16_t buf_count = 0;
volatile bool buf_flag;  //false: next buffer must be written, true: already written

//For LittelFS
File f;

#ifdef DebugSound
//For UART
String inputString = "";      // a String to hold incoming data
bool stringComplete = false;  // whether the string is complete
#endif

bool timer_callback(repeating_timer_t *rt) {
  //Called by interrupts
  //Writing to PWM is divided into CH A and B.
  if (buf_ab == BUF_CH_A) {
    pwm_set_chan_level(slice_num, PWM_CHAN_B, wave_buf_a[buf_count]);  // Mod by HMX 2024-06-02 GPIO5 is PWM_CHAN_B
  } else {
    pwm_set_chan_level(slice_num, PWM_CHAN_B, wave_buf_b[buf_count]);  // Mod by HMX 2024-06-02 GPIO5 is PWM_CHAN_B
  }
  buf_count++;
  //buffer switch
  if (buf_count == BUF_LENGTH) {
    if (buf_ab == BUF_CH_A) {
      buf_ab = BUF_CH_B;
    } else {
      buf_ab = BUF_CH_A;
    }
    buf_flag = false;
    buf_count = 0;
  }
  return true;
}

void manage_wave_sub(int ch, uint8_t *temp_buf) {
  //The last buffer write position is stored.
  uint32_t wave_pos;

  //Advance the StateMachine in each channel until the buffer is acquired.
  bool buf_flag = false;
  //Initialize
  for (int j = 0; j < BUF_LENGTH; j++) {
    temp_buf[j] = wave_last_val[ch];
  }
  //Run the StateMachine until the buffer is filled.
  while (!buf_flag) {
    //Forced termination
    if (wave_order[ch] == WAVE_MODE_STOP) {
      wave_status[ch] = WAVE_MODE_STOP;
    }
    //StateMachine
    switch (wave_status[ch]) {
      case WAVE_MODE_STOP:
#ifdef DebugSound
        Serial.println("WAVE_MODE_STOP");
#endif
        //When stopped
        switch (wave_order[ch]) {
          case WAVE_MODE_STOP:
            //Suspended state continues
            wave_status[ch] = WAVE_MODE_STOP;
            buf_flag = true;
            break;
          case WAVE_MODE_RUN_START:
            //Transition to RUN
            wave_status[ch] = WAVE_MODE_RUN_START;
            break;
          default:
            //WAVE_MODE_NONE
            buf_flag = true;
            break;
        }
        break;
      case WAVE_MODE_RUN_START:
#ifdef DebugSound
        Serial.println("WAVE_MODE_RUN_START");
#endif
        //Opening a file and figuring out initial values
        f = LittleFS.open(wave_name[ch], "r");
        wave_length[ch] = f.size() - WAVE_OFFSET;
        wave_position[ch] = 0;
        f.close();
#ifdef DebugSound
        Serial.print(wave_length[ch]);
        Serial.print(" / ");
        Serial.println(wave_name[ch]);
#endif
        //RUNNINGへ移行
        wave_status[ch] = WAVE_MODE_RUNNING;
        break;
      case WAVE_MODE_RUNNING:
#ifdef DebugSound
        Serial.println("WAVE_MODE_RUNNING");
#endif
        //Allocation based on number of data
        //There are three destinations: RUNNNING, END, and REPEAT.
        if (wave_length[ch] - wave_position[ch] < BUF_LENGTH) {
          //When data ends midway through the buffer
          if (wave_next_name[ch] == "none") {
            //When it ends
            wave_status[ch] = WAVE_MODE_RUN_END;
          } else if (wave_next_name[ch] == "repeat") {  // Mod by HMX 20220117
            //When it repeat
            wave_status[ch] = WAVE_MODE_RUN_REPEAT;
          } else {  // Add by HMX 20220117
            //When a file name is specified
            wave_status[ch] = WAVE_MODE_RUN_NEXT;  // Add by HMX 20220117
          }
        } else {
          //Store the buffer
          f = LittleFS.open(wave_name[ch], "r");
          f.seek(wave_position[ch] + WAVE_OFFSET, SeekSet);
          f.read(temp_buf, BUF_LENGTH);
          wave_last_val[ch] = temp_buf[BUF_LENGTH - 1];
          wave_position[ch] = f.position() - WAVE_OFFSET;
          f.close();
          wave_status[ch] = WAVE_MODE_RUNNING;
          buf_flag = true;
        }
        break;
      case WAVE_MODE_RUN_END:
        //Store the remaining data and fill the rest with the final data.
        wave_name[ch] = wave_next_name[ch];
        f = LittleFS.open(wave_name[ch], "r");
        f.seek(wave_position[ch] + WAVE_OFFSET, SeekSet);
        f.read(temp_buf, (wave_length[ch] - wave_position[ch]));
        f.close();
        wave_last_val[ch] = temp_buf[wave_length[ch] - wave_position[ch] - 2];
        //Fill the rest with the same value.
        for (int j = wave_length[ch] - wave_position[ch] - 1; j < BUF_LENGTH; j++) {
          temp_buf[j] = wave_last_val[ch];
        }
        buf_flag = true;
        wave_status[ch] = WAVE_MODE_STOP;
        break;
      case WAVE_MODE_RUN_REPEAT:
#ifdef DebugSound
        Serial.println("WAVE_MODE_RUN_REPEAT");
#endif
        //Store the remaining data.
        f = LittleFS.open(wave_name[ch], "r");
        f.seek(wave_position[ch] + WAVE_OFFSET, SeekSet);
        f.read(temp_buf, (wave_length[ch] - wave_position[ch]));
        f.close();

        wave_pos = wave_length[ch] - wave_position[ch] - 1;
        //Store the following data
        f = LittleFS.open(wave_name[ch], "r");
        wave_length[ch] = f.size() - WAVE_OFFSET;
        wave_position[ch] = 0;
        f.seek(wave_position[ch] + WAVE_OFFSET, SeekSet);
        f.read(temp_buf + wave_pos, (BUF_LENGTH - wave_pos));
        wave_last_val[ch] = temp_buf[BUF_LENGTH - 1];
        wave_position[ch] = f.position() - WAVE_OFFSET;
        f.close();

        wave_status[ch] = WAVE_MODE_RUNNING;
        buf_flag = true;
        break;
      case WAVE_MODE_RUN_NEXT:  // <*HMX 20220117
#ifdef DebugSound
        Serial.println("WAVE_MODE_RUN_NEXT");
#endif
        //Store the remaining data.
        f = LittleFS.open(wave_name[ch], "r");
        f.seek(wave_position[ch] + WAVE_OFFSET, SeekSet);
        f.read(temp_buf, (wave_length[ch] - wave_position[ch]));
        f.close();
        wave_pos = wave_length[ch] - wave_position[ch] - 1;

        //Store the following data
        wave_name[ch] = wave_next_name[ch];
#ifdef DebugSound
        Serial.println(wave_name[ch]);
#endif
        wave_next_name[ch] = wave_next_after_name[ch];
        wave_next_after_name[ch] = "none";
        f = LittleFS.open(wave_name[ch], "r");
        wave_length[ch] = f.size() - WAVE_OFFSET;
        wave_position[ch] = 0;
        f.seek(wave_position[ch] + WAVE_OFFSET, SeekSet);
        f.read(temp_buf + wave_pos, (BUF_LENGTH - wave_pos));
        wave_last_val[ch] = temp_buf[BUF_LENGTH - 1];
        wave_position[ch] = f.position() - WAVE_OFFSET;
        f.close();
        wave_status[ch] = WAVE_MODE_RUNNING;
        buf_flag = true;
        break;
      default:
        //just in case
        break;
    }
    wave_order[ch] = WAVE_MODE_NONE;
  }
}

void manage_wave() {
  //Writing to the buffer
  //Check if it need to write
  if (buf_flag == true) {
    return;
  }

  //Reset sound buffer to 0
  if (buf_ab == BUF_CH_A) {
    for (int i = 0; i < BUF_LENGTH; i++) {
      wave_buf_b[i] = 511;
    }
  } else {
    for (int i = 0; i < BUF_LENGTH; i++) {
      wave_buf_a[i] = 511;
    }
  }

  uint8_t temp_buf[BUF_LENGTH];

  //Repeat the number of chords
  for (int i = 0; i < WAVE_CHORDS; i++) {
    ////Case by wave_status and process each channel buffer input
    manage_wave_sub(i, temp_buf);
    //Mixing the buffer and storing in the A or B channel master buffers
    if (buf_ab == BUF_CH_A) {
      for (int j = 0; j < BUF_LENGTH; j++) {
        wave_buf_b[j] += (uint16_t)temp_buf[j] * wave_volume[i] / 32;
        wave_buf_b[j] -= 127 * wave_volume[i] / 32;
      }
    } else {
      for (int j = 0; j < BUF_LENGTH; j++) {
        wave_buf_a[j] += (uint16_t)temp_buf[j] * wave_volume[i] / 32;
        wave_buf_a[j] -= 127 * wave_volume[i] / 32;
      }
    }
  }

  // Auto volume Controle
  int amax = 0;
  if (buf_ab == BUF_CH_A) {
    for (int j = 0; j < BUF_LENGTH; j++) {
      wave_buf_b[j] = min(max(wave_buf_b[j], 1), 1023);
      amax = max(wave_buf_b[j], amax);
    }
    if (amax > 959) {
      for (int k = 0; k < WAVE_CHORDS; k++) {
        wave_volume[k] -= 8;
      }
    }
  } else {
    for (int j = 0; j < BUF_LENGTH; j++) {
      wave_buf_a[j] = min(max(wave_buf_a[j], 1), 1023);
      amax = max(wave_buf_a[j], amax);
    }
    if (amax > 959) {
      for (int k = 0; k < WAVE_CHORDS; k++) {
        wave_volume[k] -= 8;
      }
    }
  }
  // Auto volume Controle

  buf_flag = true;
  return;
}

void play_wav(int8_t ch, String wave_filename, int16_t s_vol) {
  wave_name[ch] = wave_filename;
  wave_status[ch] = WAVE_MODE_STOP;
  wave_order[ch] = WAVE_MODE_RUN_START;
  wave_next_name[ch] = "none";
  wave_next_after_name[ch] = "none";
  wave_volume[ch] = s_vol;
}

void play_wav_repeat(int8_t ch, String wave_filename, int16_t s_vol) {
  wave_name[ch] = wave_filename;
  wave_order[ch] = WAVE_MODE_RUN_START;
  wave_next_name[ch] = "repeat";  // Mod by HMX 20220117
  wave_next_after_name[ch] = "none";
  wave_volume[ch] = s_vol;
}

// Add by HMX 20220117
void play_wav_next(int8_t ch, String next_filename, int16_t s_vol) {
  wave_next_name[ch] = next_filename;
  wave_next_after_name[ch] = "none";
  wave_volume[ch] = s_vol;
}

// Add by HMX 20220117
void play_wav_after_next(int8_t ch, String next_after_filename, int16_t s_vol) {
  wave_next_after_name[ch] = next_after_filename;
  wave_volume[ch] = s_vol;
}

void stop_wav_all() {
  for (int i = 0; i < WAVE_CHORDS; i++) {
    stop_wav(i);
  }
}

void stop_wav(int8_t ch) {
  wave_order[ch] = WAVE_MODE_STOP;
  wave_next_name[ch] = "none";
}

#if DebugSound
void serialEvent() {
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read();
    if (inChar == '\n') {
      stringComplete = true;
      return;
    }
    inputString += inChar;
  }
}
#endif

void sound_setup() {

  // Add by HMX @2021-01-21 form Here
  // put your setup code here, to run once:

  // initialize the BUZZER pin as an output:
  pinMode(PIN_AMP_SW, OUTPUT);
  // AMP_SW
  digitalWrite(PIN_AMP_SW, HIGH);

  gpio_set_function(PIN_PCM, GPIO_FUNC_PWM);
  slice_num = pwm_gpio_to_slice_num(PIN_PCM);
  pwm_set_wrap(slice_num, 1023);
  pwm_set_chan_level(slice_num, PWM_CHAN_B, 0);  // Mod by HMX 2024-06-02 GPIO5 is PWM_CHAN_B
  pwm_set_enabled(slice_num, true);

  LittleFS.begin();

  // Initialization
  for (int i = 0; i < MAX_FUNC_NUMBER; i++) {
    global_FuncValue[i] = 0;
  }
  for (int i = 0; i < LOCO_LENGTH; i++) {
    global_LocoValue[i] = 0;
  }

  //Initialization
  for (int i = 0; i < WAVE_CHORDS; i++) {
    wave_position[i] = 0;
    wave_length[i] = 0;
    wave_volume[i] = 127;
    wave_last_val[i] = 127;
    //wave_last_val[i] = 0;
    wave_status[i] = WAVE_MODE_STOP;
    wave_order[i] = WAVE_MODE_NONE;
    wave_name[i] = "none";
    wave_next_name[i] = "none";
  }

  //Reset the buffer to 0
  for (int i = 0; i < BUF_LENGTH; i++) {
    wave_buf_a[i] = 0;
    wave_buf_b[i] = 0;
  }

  pwm_set_enabled(slice_num, true);
  //Timer start
  add_repeating_timer_us(-1 * TIMER1_INTERVAL_us, &timer_callback, NULL, &timer);

  MasterVol = Dcc.getCV(CV60_MASTERVOL);
#ifdef DebugSound
  Serial.println("Sound Setup Done.");
#endif
}

void sound_loop() {
  // put your main code here, to run repeatedly:

  static unsigned long DraftCh = 0;
  static uint8_t ex_FuncValue[MAX_FUNC_NUMBER];  // Func 01-28

  // ---
#ifdef DebugSound
  if (stringComplete) {
    static int vol = 63;
    Serial.println(inputString);
    if (inputString.equals("1")) {
      play_wav(1, "Engine_A1.wav", vol);
      play_wav_next(1, "Engine_A2.wav", vol);
      play_wav_after_next(1, "repeat", vol);
    } else if (inputString.equals("2")) {
      play_wav_next(1, "Engine_A3.wav", vol);
    } else if (inputString.equals("3")) {
      play_wav_repeat(3, "Bell.wav", vol);
    } else if (inputString.equals("u")) {
      vol = vol + 8;
      Serial.print("Volume : ");
      Serial.println(vol);
    } else if (inputString.equals("d")) {
      vol = vol - 8;
      Serial.print("Volume : ");
      Serial.println(vol);
    }
    inputString = "";
    stringComplete = false;
  }
#endif
  // ---
  int FunctionNumber;

  // Func==1 : Whistle //
  FunctionNumber = 1;
  if (ex_FuncValue[FunctionNumber] != global_FuncValue[FunctionNumber]) {
    if (global_FuncValue[FunctionNumber]) {
      play_wav(SOUND_CH_OPTION1, "Whistle_A1.wav", 63);
      play_wav_next(SOUND_CH_OPTION1, "Whistle_A2.wav", 63);
      play_wav_after_next(SOUND_CH_OPTION1, "repeat", 63);
    } else {
      play_wav(SOUND_CH_OPTION1, "Whistle_A3.wav", 63);
    }
    ex_FuncValue[1] = global_FuncValue[FunctionNumber];
  }

  // Func==2 : Horn //
  FunctionNumber = 2;
  if (ex_FuncValue[FunctionNumber] != global_FuncValue[FunctionNumber]) {
    if (global_FuncValue[FunctionNumber]) {
      play_wav(SOUND_CH_HORN, "Horn_A1.wav", 96);
      play_wav_next(SOUND_CH_HORN, "Horn_A2.wav", 96);
      play_wav_after_next(SOUND_CH_HORN, "repeat", 96);
    } else {
      play_wav(SOUND_CH_HORN, "Horn_A3.wav", 96);
    }
    ex_FuncValue[FunctionNumber] = global_FuncValue[FunctionNumber];
  }

  // Func==3 : Horn short //
  FunctionNumber = 3;
  if (ex_FuncValue[FunctionNumber] != global_FuncValue[FunctionNumber]) {
    if (global_FuncValue[FunctionNumber]) {
      play_wav(SOUND_CH_HORN, "Horn_A1.wav", 96);
      play_wav_next(SOUND_CH_HORN, "Horn_A3.wav", 96);
    }
    ex_FuncValue[FunctionNumber] = global_FuncValue[FunctionNumber];
  }

  // Func==7 : Bell //
  FunctionNumber = 7;
  if (ex_FuncValue[FunctionNumber] != global_FuncValue[FunctionNumber]) {
    if (global_FuncValue[FunctionNumber]) {
      play_wav_repeat(SOUND_CH_OPTION2, "Bell.wav", 63);
    } else {
      if (wave_name[SOUND_CH_OPTION2] == "Bell.wav") {
        play_wav_next(SOUND_CH_OPTION2, "none", 63);
      }
    }
    ex_FuncValue[FunctionNumber] = global_FuncValue[FunctionNumber];
  }

  // Func==8 : Sound //
  FunctionNumber = 8;
  if (ex_FuncValue[FunctionNumber] != global_FuncValue[FunctionNumber]) {
    if (global_FuncValue[FunctionNumber]) {
      play_wav_repeat(SOUND_CH_OPTION3, "Sound_1.wav", 150);
    } else {
      if (wave_name[SOUND_CH_OPTION3] == "Sound_1.wav")
      {
        play_wav_next(SOUND_CH_OPTION3, "none", 100);
      }
    }
    ex_FuncValue[FunctionNumber] = global_FuncValue[FunctionNumber];
  }

  // Func==9 : Sound //
  FunctionNumber = 9;
  if (ex_FuncValue[FunctionNumber] != global_FuncValue[FunctionNumber]) {
    if (global_FuncValue[FunctionNumber]) {
      play_wav_repeat(SOUND_CH_OPTION3, "Sound_2.wav", 150);
    } else {
      if (wave_name[SOUND_CH_OPTION3] == "Sound_2.wav")
      {
        play_wav_next(SOUND_CH_OPTION3, "none", 100);
      }
    }
    ex_FuncValue[FunctionNumber] = global_FuncValue[FunctionNumber];
  }
  
  // Run Sound
  if (global_FuncValue[4] == 1) {
    int DelayCycle = 4; // Delay for 250ms x 4

    // When Stop //
    // Initialize if stopped
    if (global_Draft == 0 && global_LocoValue[Loco_TargetSpeed] == 1) {
      global_run_Delay = DelayCycle; // Delay for 250ms x 4
    } else if (global_Draft > 0 && global_LocoValue[Loco_Speed] == 1) {
      // Stop the brake noise when stopping
      if (wave_name[SOUND_CH_BRAKE] == "Bk_A1.wav" || wave_name[SOUND_CH_BRAKE] == "Bk_A2.wav")  {
        play_wav_next(SOUND_CH_BRAKE, "Bk_A3.wav", 64);
      }
      //Start brake noise when speed is below 10 and deceleration.
    } else if (global_Draft > 0 && global_Draft < 10 && global_Draft  < global_ave_Draft  ) {
      if (not(wave_name[SOUND_CH_BRAKE] == "Bk_A1.wav" || wave_name[SOUND_CH_BRAKE] == "Bk_A2.wav")) {
        play_wav(SOUND_CH_BRAKE, "Bk_A1.wav", 64);
        play_wav_next(SOUND_CH_BRAKE, "Bk_A2.wav", 64);
        play_wav_after_next(SOUND_CH_BRAKE, "repeat", 64);
      }
    } else if (wave_name[SOUND_CH_BRAKE] == "Bk_A1.wav" || wave_name[SOUND_CH_BRAKE] == "Bk_A2.wav") {
      if (global_Draft  > global_ave_Draft ) {
        play_wav_next(SOUND_CH_BRAKE, "none", 64);
      }
    }
    // Brake air sound
    if ( global_run_Delay > 0 && global_LocoValue[Loco_TargetSpeed] > 1 ) {
      if (wave_name[SOUND_CH_BRAKE] == "none" && global_run_Delay == DelayCycle ) {
        play_wav(SOUND_CH_BRAKE, "Bk_off.wav", 64);
      }
      global_run_Delay--;
    }
    // Update Values
    global_Draft = max(1, global_LocoValue[Loco_Speed]) - 1;
    global_ave_Draft = 0.5 * (global_ave_Draft + global_Draft);
    if (global_Draft > 127) {
      global_Draft = 127;
    }

#ifdef DebugSound
    Serial.print(" TargetSpeed: ");
    Serial.print(global_LocoValue[Loco_TargetSpeed]);
    Serial.print(" Loco_Speed: ");
    Serial.print(global_LocoValue[Loco_Speed]);
    Serial.print(" global_Draft: ");
    Serial.print(global_Draft );
    Serial.print(" global_ave_Draft: ");
    Serial.print(global_ave_Draft);
    Serial.println("");
#endif

    // When Run //
    if (global_Draft > 0) {
      // When Accelerate
      if ( global_Draft > global_ave_Draft &&  global_LocoValue[Loco_TargetSpeed] > global_LocoValue[Loco_Speed]) {
        if (wave_name[SOUND_CH_ENGINE] == "Engine_A2.wav" || wave_name[SOUND_CH_ENGINE] == "Engine_B3.wav") {
          play_wav(SOUND_CH_ENGINE, "Engine_B1.wav", 96);
          play_wav_next(SOUND_CH_ENGINE, "Engine_B2.wav",  96);
          play_wav_after_next(SOUND_CH_ENGINE, "repeat",  96);
        }
      }
      // When Deccelerate
      if ( global_LocoValue[Loco_TargetSpeed] == global_LocoValue[Loco_Speed] || global_LocoValue[Loco_TargetSpeed] < global_LocoValue[Loco_Speed]) {
        if (wave_name[SOUND_CH_ENGINE] == "Engine_B2.wav") {
          play_wav(SOUND_CH_ENGINE, "Engine_B3.wav",  96);
          play_wav_next(SOUND_CH_ENGINE, "Engine_A2.wav",  96);
          play_wav_after_next(SOUND_CH_ENGINE, "repeat",  96);
        }
      }
      // Func==5 : Rail //
      if (ex_FuncValue[5] != global_FuncValue[5]) {
        if (global_FuncValue[5]) {
          play_wav(SOUND_CH_RAIL1, "Rail_A1.wav", 64);
          play_wav_next(SOUND_CH_RAIL1, "Rail_A2.wav", 64);
          play_wav_after_next(SOUND_CH_RAIL1, "repeat", 64);
        } else {
          play_wav(SOUND_CH_RAIL1, "Rail_A3.wav", 64);
        }
        ex_FuncValue[5] = global_FuncValue[5];
      }
      if (global_FuncValue[5] && global_Draft < 10) {
        if (wave_name[SOUND_CH_RAIL1] == "Rail_A2.wav") {
          play_wav(SOUND_CH_RAIL1, "Rail_A3.wav",  64);
        } else if (wave_name[SOUND_CH_RAIL1] == "Rail_A1.wav") {
          play_wav(SOUND_CH_RAIL1, "none",  64);
        }
      }
    } else {
      // Still Stop //
      // Turn off unnecessary sounds just in case
      if (wave_name[SOUND_CH_ENGINE] == "none") {
        play_wav(SOUND_CH_ENGINE, "Engine_A1.wav",  96);
        play_wav_next(SOUND_CH_ENGINE, "Engine_A2.wav",  96);
        play_wav_after_next(SOUND_CH_ENGINE, "repeat",  96);
      } else if (wave_name[SOUND_CH_ENGINE] == "Engine_B2.wav") {
        play_wav(SOUND_CH_ENGINE, "Engine_B3.wav",  96);
        play_wav_next(SOUND_CH_ENGINE, "Engine_A2.wav",  96);
        play_wav_after_next(SOUND_CH_ENGINE, "repeat",  96);
      }
    }
  } else {
    // When the function is turned off
    if (wave_name[SOUND_CH_ENGINE] != "none" && wave_name[SOUND_CH_ENGINE] != "Engine_A3.wav" ) {
      play_wav_next(SOUND_CH_ENGINE, "Engine_A3.wav",  96);
    }
    if (wave_name[SOUND_CH_RAIL1] != "none" && wave_name[SOUND_CH_RAIL1] != "Rail_A3.wav" ) {
      play_wav_next(SOUND_CH_RAIL1, "Rail_A3.wav",  64);
    }
    global_run_Delay = 0;
  }
  manage_wave();
}
