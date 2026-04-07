/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 *
 * @Hardwares: M5Atom series/M5AtomS3 series + Atomic Audio-3.5 Base
 *
 * @Dependent Library:
 * M5Atomic-EchoBase: https://github.com/m5stack/M5Atomic-EchoBase
 *
 * @Note: In this example, we recommend using an external microphone.
 *
 */

#include <M5Unified.h>
#include <M5EchoBase.h>

#define RECORD_SIZE (1024 * 4)

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
M5EchoBase echobase;
#else
M5EchoBase echobase(I2S_NUM_0);
#endif

static uint8_t *buffer = nullptr;  // Pointer to hold the audio buffer.

void setup()
{
    delay(1000);  // Delay for a moment to allow the system to stabilize.
    auto cfg            = M5.config();
    cfg.serial_baudrate = 115200;
    M5.begin(cfg);

    // Initialize the EchoBase with ATOMS3 pinmap.
#if defined(CONFIG_IDF_TARGET_ESP32S3)
    if (!echobase.init(44100 /*Sample Rate*/, 38 /*I2C SDA*/, 39 /*I2C SCL*/, 7 /*I2S DIN*/, 6 /*I2S WS*/,
                       5 /*I2S DOUT*/, 8 /*I2S BCK*/, Wire) != 0) {
        Serial.println("Failed to initialize EchoBase!");
        while (true) {
            delay(1000);
        }
    }
#elif defined(CONFIG_IDF_TARGET_ESP32)
    // Initialize the EchoBase with ATOM pinmap.
    if (!echobase.init(44100 /*Sample Rate*/, 25 /*I2C SDA*/, 21 /*I2C SCL*/, 23 /*I2S DIN*/, 19 /*I2S WS*/, 22 /*I2S
    DOUT*/, 33 /*I2S BCK*/, Wire) != 0) {
        Serial.println("Failed to initialize EchoBase!");
        while (true) {
            delay(1000);
        }
    }
#endif

    echobase.setSpeakerVolume(65);             // Set speaker volume to 65%.
    echobase.setMicGain(ES8311_MIC_GAIN_0DB);  // Set microphone gain to 0dB.

    buffer = (uint8_t *)malloc(RECORD_SIZE);  // Allocate memory for the record buffer.
    // Check if memory allocation was successful.
    if (buffer == nullptr) {
        // If memory allocation fails, enter an infinite loop.
        while (true) {
            Serial.println("Failed to allocate memory :(");
            delay(1000);
        }
    }

    Serial.println("EchoBase ready, start recording and playing!");
    echobase.setMute(false);
}

void loop()
{
    if (!echobase.record(buffer, RECORD_SIZE)) {
        Serial.println("record failed");
        return;
    }

    if (!echobase.play(buffer, RECORD_SIZE, false)) {
        Serial.println("play failed");
        return;
    }
}