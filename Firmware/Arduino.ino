#include "driver/i2s_std.h"
#include <ESP32Servo.h>

// ============================================================
// AUDIO
// ============================================================

#define FS      16000
#define N       2048
#define CHUNK   256
#define SHIFT   16
#define WARMUP  30

// ============================================================
// SERVO
// ============================================================

#define SERVO_PIN 13

Servo servo;

// ============================================================
// UART2 FOR ANGLE COMMAND
// ============================================================

#define RX2_PIN 16
#define TX2_PIN 17

HardwareSerial AngleSerial(2);


// ============================================================
// I2S CHANNELS
// ============================================================

static i2s_chan_handle_t rxA, rxB;

static int16_t blk[2][N * 3];

static volatile int readyIdx = -1;

static const uint8_t HDR[4] = {
  0xAA, 0x55, 0xAA, 0x55
};


// ============================================================
// CREATE I2S RECEIVER
// ============================================================

static i2s_chan_handle_t makeRx(
    i2s_port_t port,
    i2s_role_t role,
    gpio_num_t bclk,
    gpio_num_t ws,
    gpio_num_t din
) {

  i2s_chan_config_t cc =
      I2S_CHANNEL_DEFAULT_CONFIG(port, role);

  cc.dma_desc_num = 8;
  cc.dma_frame_num = 256;

  i2s_chan_handle_t h;

  ESP_ERROR_CHECK(
      i2s_new_channel(&cc, NULL, &h)
  );

  i2s_std_config_t sc = {

    .clk_cfg =
        I2S_STD_CLK_DEFAULT_CONFIG(FS),

    .slot_cfg =
        I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_32BIT,
            I2S_SLOT_MODE_STEREO
        ),

    .gpio_cfg = {

      .mclk = I2S_GPIO_UNUSED,

      .bclk = bclk,

      .ws = ws,

      .dout = I2S_GPIO_UNUSED,

      .din = din,

      .invert_flags = {
        .mclk_inv = false,
        .bclk_inv = false,
        .ws_inv = false
      }
    }
  };

  ESP_ERROR_CHECK(
      i2s_channel_init_std_mode(h, &sc)
  );

  return h;
}


// ============================================================
// CONVERT I2S 32-BIT → INT16
// ============================================================

static inline int16_t cvt(int32_t v) {

  int32_t s = v >> SHIFT;

  if (s > 32767)
    s = 32767;

  if (s < -32768)
    s = -32768;

  return (int16_t)s;
}


// ============================================================
// AUDIO READER TASK
// ============================================================

void readerTask(void *) {

  static int32_t bufA[CHUNK * 2];
  static int32_t bufB[CHUNK * 2];

  int w = 0;
  int pos = 0;
  int warm = WARMUP;

  while (true) {

    size_t ra = 0;
    size_t rb = 0;

    // Mic 1 + Mic 2
    i2s_channel_read(
        rxA,
        bufA,
        sizeof(bufA),
        &ra,
        portMAX_DELAY
    );

    // Mic 3
    i2s_channel_read(
        rxB,
        bufB,
        sizeof(bufB),
        &rb,
        portMAX_DELAY
    );

    if (ra != sizeof(bufA) ||
        rb != sizeof(bufB)) {

      continue;
    }

    if (warm > 0) {

      warm--;

      continue;
    }

    // --------------------------------------------------------
    // STORE THREE MICROPHONES
    // --------------------------------------------------------

    for (int k = 0; k < CHUNK; k++) {

      int idx = (pos + k) * 3;

      // Mic 1
      blk[w][idx + 0] =
          cvt(bufA[2 * k]);

      // Mic 2
      blk[w][idx + 1] =
          cvt(bufA[2 * k + 1]);

      // Mic 3
      blk[w][idx + 2] =
          cvt(bufB[2 * k]);
    }

    pos += CHUNK;

    // --------------------------------------------------------
    // COMPLETE BLOCK
    // --------------------------------------------------------

    if (pos >= N) {

      pos = 0;

      if (readyIdx == -1) {

        readyIdx = w;

        w ^= 1;
      }
    }
  }
}


// ============================================================
// READ SERVO ANGLE
// ============================================================

void readServoCommand() {

  static String input = "";

  while (AngleSerial.available()) {

    char c = AngleSerial.read();

    if (c == '\n') {

      input.trim();

      if (input.startsWith("ANGLE:")) {

        String value =
            input.substring(6);

        float angle =
            value.toFloat();

        angle = constrain(
            angle,
            0.0,
            180.0
        );

        servo.write(
            (int)round(angle)
        );
      }

      input = "";

    } else {

      input += c;

      // Prevent runaway String
      if (input.length() > 20) {
        input = "";
      }
    }
  }
}


// ============================================================
// SETUP
// ============================================================

void setup() {

  // ==========================================================
  // USB SERIAL
  // Dedicated to AUDIO
  // ==========================================================

  Serial.begin(921600);


  // ==========================================================
  // SERVO
  // ==========================================================

  servo.setPeriodHertz(50);

  servo.attach(
      SERVO_PIN,
      500,
      2400
  );

  // Start center
  servo.write(90);

  delay(500);


  // ==========================================================
  // UART2
  //
  // RX2 = GPIO16
  // TX2 = GPIO17
  // ==========================================================

  AngleSerial.begin(
      115200,
      SERIAL_8N1,
      RX2_PIN,
      TX2_PIN
  );


  // ==========================================================
  // I2S0
  //
  // Mic 1 + Mic 2
  //
  // BCLK = GPIO26
  // WS   = GPIO25
  // DATA = GPIO32
  // ==========================================================

  rxA = makeRx(
      I2S_NUM_0,
      I2S_ROLE_MASTER,
      GPIO_NUM_26,
      GPIO_NUM_25,
      GPIO_NUM_32
  );


  // ==========================================================
  // I2S1
  //
  // Mic 3
  //
  // BCLK = GPIO18
  // WS   = GPIO19
  // DATA = GPIO33
  //
  // GPIO18 ← jumper from GPIO26
  // GPIO19 ← jumper from GPIO25
  // ==========================================================

  rxB = makeRx(
      I2S_NUM_1,
      I2S_ROLE_SLAVE,
      GPIO_NUM_18,
      GPIO_NUM_19,
      GPIO_NUM_33
  );


  // Slave first
  ESP_ERROR_CHECK(
      i2s_channel_enable(rxB)
  );

  // Master second
  ESP_ERROR_CHECK(
      i2s_channel_enable(rxA)
  );


  // ==========================================================
  // AUDIO READER
  // ==========================================================

  xTaskCreatePinnedToCore(
      readerTask,
      "reader",
      4096,
      NULL,
      5,
      NULL,
      0
  );
}


// ============================================================
// LOOP
// ============================================================

void loop() {

  // ----------------------------------------------------------
  // CHECK SERVO COMMAND
  // ----------------------------------------------------------

  readServoCommand();


  // ----------------------------------------------------------
  // SEND AUDIO BLOCK
  // ----------------------------------------------------------

  int r = readyIdx;

  if (r >= 0) {

    // Synchronization header
    Serial.write(
        HDR,
        4
    );

    // Mic1, Mic2, Mic3...
    Serial.write(
        (uint8_t *)blk[r],
        N * 3 * 2
    );

    readyIdx = -1;

  } else {

    delay(1);
  }
}
