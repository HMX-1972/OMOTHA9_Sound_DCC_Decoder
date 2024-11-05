#include <hardware/pwm.h>
#include <LittleFS.h>

#define TIMER1_INTERVAL_us 62  // 16kHz 62
#define BUF_LENGTH 1024
#define WAVE_CHORDS 8   //和音数
#define WAVE_OFFSET 44  //waveのオフセット Waveファイルの始めに43個のヘッダー領域があるため、そこを飛ばす。

//wave_modeのenum
#define WAVE_MODE_STOP 0
#define WAVE_MODE_RUN_START 1
#define WAVE_MODE_RUNNING 2
#define WAVE_MODE_RUN_END 3
#define WAVE_MODE_RUN_REPEAT 4
#define WAVE_MODE_RUN_NEXT 5  // 追加 <*HMX 20220117
#define WAVE_MODE_NONE 6      // 5から6へ変更 <*HMX 20220117

#define WAVE_DEFAULT_VALUE 127  //8bitの時127，16bitの時は0 無音状態

#define BUF_CH_A 0
#define BUF_CH_B 1

//timer オブジェクト
repeating_timer_t timer;

//各chの音
uint32_t wave_position[WAVE_CHORDS];       //現在値
uint32_t wave_length[WAVE_CHORDS];         //長さ
uint16_t wave_volume[WAVE_CHORDS];         //volume 0-255
String wave_name[WAVE_CHORDS];             //ファイル名
String wave_next_name[WAVE_CHORDS];        //ファイル名
String wave_next_after_name[WAVE_CHORDS];  //ファイル名
uint8_t wave_last_val[WAVE_CHORDS];        //プチ音を消すため最後の音を保つための値。
int wave_status[WAVE_CHORDS];              //状態 WAVE_MODE_STOP,WAVE_MODE_RUN
int wave_order[WAVE_CHORDS];               //要求 WAVE_MODE_STOP,WAVE_MODE_RUN
int wave_fnc_status[WAVE_CHORDS];
int loco_dir;

//出力部分 割込みで使用するので、volatile
volatile uint slice_num = 0;
volatile uint16_t wave_buf_a[BUF_LENGTH];
volatile uint16_t wave_buf_b[BUF_LENGTH];
volatile int buf_ab = 1;
volatile uint16_t buf_count = 0;
volatile bool buf_flag;  //false:次のバッファを書かないといけない、true:書き込み済み

uint8_t global_LocoValue[LOCO_LENGTH];  // 0:Speed, 1:Acceraretion, 2:Speed Stage
uint8_t global_Draft = 0;               // 0:OFF
uint8_t global_ave_Draft = 0;           // 0:OFF

//LittelFS用
File f;

//UART用
String inputString = "";      // a String to hold incoming data
bool stringComplete = false;  // whether the string is complete

bool timer_callback(repeating_timer_t *rt) {
  //割込みで呼ばれるところ
  //PWMへの書き込み CH A,Bで分かれる。
  if (buf_ab == BUF_CH_A) {
    pwm_set_chan_level(slice_num, PWM_CHAN_B, wave_buf_a[buf_count]);  // HMX 2024-06-02 GPIO5 is B
  } else {
    pwm_set_chan_level(slice_num, PWM_CHAN_B, wave_buf_b[buf_count]);  // HMX 2024-06-02 GPIO5 is B
  }
  buf_count++;
  //bufferの切り替え
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
  //bufferの最終書き込み位置を記憶する。
  uint32_t wave_pos;

  //各ch内のStateMachineをバッファ取得まで進める。
  bool buf_flag = false;
  //デフォルト値初期化
  for (int j = 0; j < BUF_LENGTH; j++) {
    temp_buf[j] = wave_last_val[ch];
  }
  //バッファが入るまでStateMachineを回す。
  while (!buf_flag) {
    //強制終了(どのStateに行っても一緒なのでここに書く）
    if (wave_order[ch] == WAVE_MODE_STOP) {
      wave_status[ch] = WAVE_MODE_STOP;
    }
    //StateMachine
    switch (wave_status[ch]) {
      case WAVE_MODE_STOP:
        //Serial.println("WAVE_MODE_STOP");
        //停止時
        switch (wave_order[ch]) {
          case WAVE_MODE_STOP:
            //中止状態継続
            wave_status[ch] = WAVE_MODE_STOP;
            buf_flag = true;
            break;
          case WAVE_MODE_RUN_START:
            //RUNへ移行
            wave_status[ch] = WAVE_MODE_RUN_START;
            break;
          default:
            //WAVE_MODE_NONE
            buf_flag = true;
            break;
        }
        break;
      case WAVE_MODE_RUN_START:
        //Serial.println("WAVE_MODE_RUN_START");
        //ファイルオープンと初期値把握
        f = LittleFS.open(wave_name[ch], "r");
        wave_length[ch] = f.size() - WAVE_OFFSET;
        wave_position[ch] = 0;
        f.close();
        Serial.print(wave_length[ch]);
        Serial.print(" / ");
        Serial.println(wave_name[ch]);
        //RUNNINGへ移行
        wave_status[ch] = WAVE_MODE_RUNNING;
        break;
      case WAVE_MODE_RUNNING:
        //Serial.println("WAVE_MODE_RUNNING");
        //データ数により振り分け
        //行き先はRUNNNINGとENDとREPEATの三つ
        if (wave_length[ch] - wave_position[ch] < BUF_LENGTH) {
          //データがバッファ途中で終了の時
          if (wave_next_name[ch] == "none") {
            //終了のとき
            wave_status[ch] = WAVE_MODE_RUN_END;
          } else if (wave_next_name[ch] == "repeat") {  // 修正 <*HMX 20220117
            //Repeatのとき
            wave_status[ch] = WAVE_MODE_RUN_REPEAT;
          } else {  // 追加 <*HMX 20220117
            //ファイル名が指定されているとき
            wave_status[ch] = WAVE_MODE_RUN_NEXT;  // 追加 <*HMX 20220117
          }
        } else {
          //Buffer分 格納する
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
        //Serial.println("WAVE_MODE_RUN_END");
        //残りのdataを格納しあとは最終データで埋める。
        wave_name[ch] = wave_next_name[ch];
        f = LittleFS.open(wave_name[ch], "r");
        f.seek(wave_position[ch] + WAVE_OFFSET, SeekSet);
        f.read(temp_buf, (wave_length[ch] - wave_position[ch]));
        f.close();
        wave_last_val[ch] = temp_buf[wave_length[ch] - wave_position[ch] - 2];
        //残りを同じ値で埋めておく。
        for (int j = wave_length[ch] - wave_position[ch] - 1; j < BUF_LENGTH; j++) {
          temp_buf[j] = wave_last_val[ch];
        }
        buf_flag = true;
        wave_status[ch] = WAVE_MODE_STOP;
        break;
      case WAVE_MODE_RUN_REPEAT:
        //Serial.println("WAVE_MODE_RUN_REPEAT");
        //残りのdataを格納する。
        f = LittleFS.open(wave_name[ch], "r");
        f.seek(wave_position[ch] + WAVE_OFFSET, SeekSet);
        f.read(temp_buf, (wave_length[ch] - wave_position[ch]));
        f.close();

        wave_pos = wave_length[ch] - wave_position[ch] - 1;
        //次のデータで埋める
        f = LittleFS.open(wave_name[ch], "r");
        wave_length[ch] = f.size() - WAVE_OFFSET;
        wave_position[ch] = 0;
        f.seek(wave_position[ch] + WAVE_OFFSET, SeekSet);
        //f.read(temp_buf + wave_pos + 1,(BUF_LENGTH - wave_pos));
        f.read(temp_buf + wave_pos, (BUF_LENGTH - wave_pos));
        wave_last_val[ch] = temp_buf[BUF_LENGTH - 1];
        wave_position[ch] = f.position() - WAVE_OFFSET;
        f.close();

        wave_status[ch] = WAVE_MODE_RUNNING;
        buf_flag = true;
        break;
      case WAVE_MODE_RUN_NEXT:  // <*HMX 20220117
        //Serial.println("WAVE_MODE_RUN_NEXT");
        //残りのdataを格納する。
        f = LittleFS.open(wave_name[ch], "r");
        f.seek(wave_position[ch] + WAVE_OFFSET, SeekSet);
        f.read(temp_buf, (wave_length[ch] - wave_position[ch]));
        f.close();
        wave_pos = wave_length[ch] - wave_position[ch] - 1;

        //次のデータで埋める
        wave_name[ch] = wave_next_name[ch];
        Serial.println(wave_name[ch]);
        wave_next_name[ch] = wave_next_after_name[ch];
        wave_next_after_name[ch] = "none";
        f = LittleFS.open(wave_name[ch], "r");
        wave_length[ch] = f.size() - WAVE_OFFSET;
        wave_position[ch] = 0;
        f.seek(wave_position[ch] + WAVE_OFFSET, SeekSet);
        //f.read(temp_buf + wave_pos + 1,(BUF_LENGTH - wave_pos));
        f.read(temp_buf + wave_pos, (BUF_LENGTH - wave_pos));
        wave_last_val[ch] = temp_buf[BUF_LENGTH - 1];
        wave_position[ch] = f.position() - WAVE_OFFSET;
        f.close();
        wave_status[ch] = WAVE_MODE_RUNNING;
        buf_flag = true;
        break;
      default:
        //Serial.println("WAVE_MODE_NONE");
        //WAVE_MODE_NONE
        //ここには来ないはずだが。
        break;
    }
    wave_order[ch] = WAVE_MODE_NONE;
  }
}

void manage_wave() {
  //バッファへの書き込み
  //書く必要があるかの確認
  if (buf_flag == true) {
    return;
  }

  //音バッファを０リセット
  if (buf_ab == BUF_CH_A) {
    for (int i = 0; i < BUF_LENGTH; i++) {
      wave_buf_b[i] = 511;
    }
  } else {
    for (int i = 0; i < BUF_LENGTH; i++) {
      wave_buf_a[i] = 511;
    }
  }

  //buffer  globalやstaticにした方が初期化を毎回しないので高速。（あとでやる）
  uint8_t temp_buf[BUF_LENGTH];

  //和音回数分繰り返す
  for (int i = 0; i < WAVE_CHORDS; i++) {
    //wave_statusで場合分けして、各ch buffer入力まで処理
    manage_wave_sub(i, temp_buf);
    //BufferのMixとA,Bchマスタバッファへの格納
    //音量変更はそのうち
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
  wave_next_name[ch] = "repeat";  // 修正 <*HMX 20220117
  wave_next_after_name[ch] = "none";
  wave_volume[ch] = s_vol;
}

// 追加 <*HMX 20220117
void play_wav_next(int8_t ch, String next_filename, int16_t s_vol) {
  wave_next_name[ch] = next_filename;
  wave_next_after_name[ch] = "none";
  wave_volume[ch] = s_vol;
}

// 追加 <*HMX 20220117
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

void serialEvent() {
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read();
    if (inChar == '\n') {
      stringComplete = true;
      return;
    }
    // add it to the inputString:
    inputString += inChar;
  }
}

void sound_setup() {

  // <- HMX @2021-01-21 Add form Here
  // put your setup code here, to run once:
  //ITimer1.attachInterruptInterval(TIMER1_INTERVAL_us, TimerHandler1);

  // initialize the BUZZER pin as an output:
  pinMode(PIN_AMP_SW, OUTPUT);
  // AMP_SW
  digitalWrite(PIN_AMP_SW, HIGH);

  // gpio_set_function(4, GPIO_FUNC_PWM);
  gpio_set_function(PIN_PCM, GPIO_FUNC_PWM);
  slice_num = pwm_gpio_to_slice_num(PIN_PCM);
  pwm_set_wrap(slice_num, 1023);
  // pwm_set_chan_level(slice_num, PWM_CHAN_A, 0);
  pwm_set_chan_level(slice_num, PWM_CHAN_B, 0);  // HMX 2024-06-02 GPIO5 is B
  pwm_set_enabled(slice_num, true);

  LittleFS.begin();

  // 初期化
  for (int i = 0; i < MAX_FUNC_NUMBER; i++) {
    global_FuncValue[i] = 0;
  }
  for (int i = 0; i < LOCO_LENGTH; i++) {
    global_LocoValue[i] = 0;
  }

  //初期化
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

  //当該バッファを０リセット
  for (int i = 0; i < BUF_LENGTH; i++) {
    wave_buf_a[i] = 0;
    wave_buf_b[i] = 0;
  }

  pwm_set_enabled(slice_num, true);
  //Timer開始
  add_repeating_timer_us(-1 * TIMER1_INTERVAL_us, &timer_callback, NULL, &timer);

  Serial.println("Run");
  //pinMode(12,OUTPUT);
  //digitalWrite(12,HIGH);
}

void sound_loop() {
  // put your main code here, to run repeatedly:

  static unsigned long DraftCh = 0;
  static uint8_t ex_FuncValue[MAX_FUNC_NUMBER];  // Func 01-28


  // ---
  if (stringComplete) {
    Serial.println(inputString);
    if (inputString.equals("0")) {
      play_wav(1, "GE_FaA_01.wav", 63);
      play_wav_next(1, "GE_FaA_02.wav", 63);
      play_wav_after_next(1, "repeat", 63);
    } else if (inputString.equals("1")) {
      play_wav_next(1, "GE_FaA_03.wav", 63);
    } else if (inputString.equals("2")) {
      play_wav(1, "GE_Wh_01.wav", 63);
      play_wav_next(1, "GE_Wh_02.wav", 63);
      play_wav_after_next(1, "repeat", 63);
    } else if (inputString.equals("3")) {
      play_wav(1, "GE_Wh_03.wav", 63);
    } else if (inputString.equals("4")) {
      play_wav(1, "GE_Wh_01.wav", 63);
      play_wav_next(1, "GE_Wh_03.wav", 63);
    }
    inputString = "";
    stringComplete = false;
  }
  // ---

  // Func==1 : Conducter Whistle //
  if (ex_FuncValue[1] != global_FuncValue[1]) {
    if (global_FuncValue[1] == 0) {
      play_wav(7, "Cw_03.wav", 63);
    } else {
      play_wav(7, "Cw_01.wav", 63);
      play_wav_next(7, "Cw_02.wav", 63);
      play_wav_after_next(7, "repeat", 63);
    }
    ex_FuncValue[1] = global_FuncValue[1];
  }

  // Func==2 : Whistle //
  if (ex_FuncValue[2] != global_FuncValue[2]) {
    if (global_FuncValue[2] == 0) {
      play_wav(6, "GE_Wh_03.wav", 63);
    } else {
      play_wav(6, "GE_Wh_01.wav", 63);
      play_wav_next(6, "GE_Wh_02.wav", 63);
      play_wav_after_next(6, "repeat", 63);
    }
    ex_FuncValue[2] = global_FuncValue[2];
  }

  // Func==3 : Whistle short //
  if (ex_FuncValue[3] != global_FuncValue[3]) {
    if (global_FuncValue[3] != 0) {
      play_wav(6, "GE_WhA.wav", 63);
    }
    ex_FuncValue[3] = global_FuncValue[3];
  }

  // Run Sound
  if (global_FuncValue[4] == 1) {
    // When Stop //
    if (global_Draft > 0 and global_LocoValue[0] == 1) {

      play_wav_repeat(2, "Bk_01.wav", 32);
      play_wav_next(2, "Bk_02.wav", 32);
      play_wav_after_next(2, "Bk_03.wav", 32);

      play_wav(4, "GE_Ra_03.wav", 0);

      play_wav(5, "GE_FaB_03.wav", 32);
      play_wav_next(5, "GE_FaA_02.wav", 32);
      play_wav_after_next(5, "repeat", 32);

      if (wave_name[3] != "none") {
        play_wav(3, "GE_Mo_03.wav", 16);
      }
    }
    global_Draft = global_LocoValue[0] - 1;
    global_ave_Draft = 0.5 * (global_ave_Draft + global_Draft);

    // When Run //
    if (global_Draft > 0) {
      if (global_Draft > 127) {
        global_Draft = 127;
      }
      if (not(wave_name[5] == "GE_FaB_01.wav" or wave_name[5] == "GE_FaB_02.wav")) {
        play_wav(5, "GE_FaB_01.wav", 32);
        play_wav_next(5, "GE_FaB_02.wav", 32);
        play_wav_after_next(5, "repeat", 32);
      }
      if (global_FuncValue[5] == 1 and wave_name[3] == "none" and global_Draft > global_ave_Draft) {
        play_wav(3, "GE_Mo_01.wav", 32);
        play_wav_next(3, "GE_Mo_02.wav", 32);
        play_wav_after_next(3, "repeat", 32);
      } else if (global_FuncValue[5] == 1 and wave_name[3] == "GE_Mo_02.wav" and global_Draft <= global_ave_Draft) {
        play_wav(3, "GE_Mo_03.wav", 32);
      }

      // Func==6 : Rail //
      if (ex_FuncValue[6] != global_FuncValue[6]) {
        if (global_FuncValue[6] == 0) {
          play_wav_next(4, "GE_Ra_03.wav", 32);
        } else {
          play_wav(4, "GE_Ra_01.wav", 32);
          play_wav_next(4, "GE_Ra_02.wav", 32);
          play_wav_after_next(4, "repeat", 32);
        }
        ex_FuncValue[6] = global_FuncValue[6];
      }

      // Still Stop //
    } else {
      if (wave_name[5] == "none") {
        play_wav(5, "GE_FaA_01.wav", 32);
        play_wav_next(5, "GE_FaA_02.wav", 32);
        play_wav_after_next(5, "repeat", 32);
      }
    }
  } else {
    if (wave_next_name[5] != "none") {
      play_wav_next(5, "GE_FaA_03.wav", 32);
    }
  }
  manage_wave();
}
