/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "M5EchoBase.h"
#include "es8311.h"

#define TAG "EchoBase"

// Define I2C address for PI4IOE5V6408 I/O expander
#define PI4IOE_ADDR          0x43
#define PI4IOE_REG_CTRL      0x00
#define PI4IOE_REG_IO_PP     0x07
#define PI4IOE_REG_IO_DIR    0x03
#define PI4IOE_REG_IO_OUT    0x05
#define PI4IOE_REG_IO_PULLUP 0x0D

// I2C and I2S pin definitions for ES8311 audio codec
#define ES8311_ADDR 0x18

bool M5EchoBase::init(int sample_rate, int i2c_sda, int i2c_scl, int i2s_di, int i2s_ws, int i2s_do, int i2s_bck,
                      TwoWire& wire)
{
    _wire = &wire;
    // Store IO pins
    _i2c_sda = i2c_sda;
    _i2c_scl = i2c_scl;
    _i2s_di  = i2s_di;
    _i2s_ws  = i2s_ws;
    _i2s_do  = i2s_do;
    _i2s_bck = i2s_bck;

    // Initialize I2C
    _wire->begin(_i2c_sda, _i2c_scl, 100000U);
    ESP_LOGI(TAG, "I2C initialized");

    // Initialize I2S driver
    if (!i2s_driver_init(sample_rate)) {
        ESP_LOGI(TAG, "I2S driver initialization failed");
        return false;
    }

    // Initialize ES8311 codec
    if (!es8311_codec_init(sample_rate)) {
        ESP_LOGI(TAG, "ES8311 codec initialization failed");
        return false;
    }

    // Initialize PI4IOE5V6408 I2C expander
    if (!pi4ioe_init()) {
        ESP_LOGI(TAG, "PI4IOE5V6408 initialization failed");
        return false;
    }

    setMicGain(ES8311_MIC_GAIN_0DB);

    return true;
}

bool M5EchoBase::es8311_codec_init(int sample_rate)
{
    es8311_set_twowire(_wire);
    es_handle = es8311_create(((i2c_port_t)0), ES8311_ADDR);  // I2C PORT is not used
    if (!es_handle) {
        ESP_LOGI(TAG, "Failed to create ES8311 handle");
        return false;
    }

    es8311_clock_config_t es_clk = {
        .mclk_inverted      = false,
        .sclk_inverted      = false,
        .mclk_from_mclk_pin = false,  // MCLK taken from SCK pin
        .mclk_frequency     = 0,      // Not used
        .sample_frequency   = sample_rate,
    };

    if (es8311_init(es_handle, &es_clk, ES8311_RESOLUTION_32, ES8311_RESOLUTION_32) != ESP_OK) {
        ESP_LOGI(TAG, "ES8311 initialization failed");
        return false;
    }

    if (es8311_voice_volume_set(es_handle, 50, NULL) != ESP_OK) {
        ESP_LOGI(TAG, "Failed to set ES8311 volume");
        return false;
    }

    if (es8311_microphone_config(es_handle, false) != ESP_OK) {
        ESP_LOGI(TAG, "Failed to configure ES8311 microphone");
        return false;
    }

    ESP_LOGI(TAG, "ES8311 codec initialized successfully");
    return true;
}

bool M5EchoBase::i2s_driver_init(int sample_rate)
{
    // Initialize I2S driver
    i2s_cfg.mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX);
    i2s_cfg.sample_rate          = (uint32_t)sample_rate;
    i2s_cfg.bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT;
    i2s_cfg.channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT;
    i2s_cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    i2s_cfg.intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1;
    i2s_cfg.dma_buf_count        = 8;
    i2s_cfg.dma_buf_len          = 256;
    i2s_cfg.use_apll             = false;
    i2s_cfg.tx_desc_auto_clear   = false;
    i2s_cfg.fixed_mclk           = 0;

    // I2S pin configuration
    i2s_pin_cfg.mck_io_num   = -1;
    i2s_pin_cfg.bck_io_num   = _i2s_bck;
    i2s_pin_cfg.ws_io_num    = _i2s_ws;
    i2s_pin_cfg.data_out_num = _i2s_do;
    i2s_pin_cfg.data_in_num  = _i2s_di;

    if (i2s_driver_install(i2s_num, &i2s_cfg, 0, NULL) != ESP_OK) {
        ESP_LOGI(TAG, "Failed to install I2S driver");
        return false;
    }

    if (i2s_set_pin(i2s_num, &i2s_pin_cfg) != ESP_OK) {
        ESP_LOGI(TAG, "Failed to set I2S pins");
        return false;
    }

    i2s_zero_dma_buffer(i2s_num);
    // idf 4.x
    i2s_start(i2s_num);

    return true;
}

bool M5EchoBase::setSpeakerVolume(int volume)
{
    if (volume < 0 || volume > 100) {
        ESP_LOGI(TAG, "Volume out of range (0-100)");
        return false;
    }

    if (es8311_voice_volume_set(es_handle, volume, NULL) != ESP_OK) {
        ESP_LOGI(TAG, "Failed to set speaker volume");
        return false;
    }
    return true;
}

bool M5EchoBase::setMicGain(es8311_mic_gain_t gain)
{
    if (es8311_microphone_gain_set(es_handle, gain) != ESP_OK) {
        ESP_LOGI(TAG, "Failed to set microphone gain");
        return false;
    }
    return true;
}

uint8_t M5EchoBase::wire_read_byte(uint8_t i2c_addr, uint8_t reg_addr)
{
    _wire->beginTransmission(i2c_addr);
    _wire->write(reg_addr);
    _wire->endTransmission(false);             // Keep the connection active
    _wire->requestFrom(i2c_addr, (uint8_t)1);  // Request 1 byte from the register
    if (_wire->available()) {
        return _wire->read();
    }
    return 0xFF;  // Return an invalid byte in case of failure
}

void M5EchoBase::wire_write_byte(uint8_t i2c_addr, uint8_t reg_addr, uint8_t value)
{
    _wire->beginTransmission(i2c_addr);
    _wire->write(reg_addr);
    _wire->write(value);
    _wire->endTransmission();
}

bool M5EchoBase::pi4ioe_init()
{
    // Read CTRL register to get the current state
    wire_read_byte(PI4IOE_ADDR, PI4IOE_REG_CTRL);

    // Set outputs to high-impedance
    wire_write_byte(PI4IOE_ADDR, PI4IOE_REG_IO_PP, 0x00);  // Set to high-impedance
    wire_read_byte(PI4IOE_ADDR, PI4IOE_REG_IO_PP);

    // Enable pull-up resistors
    wire_write_byte(PI4IOE_ADDR, PI4IOE_REG_IO_PULLUP, 0xFF);  // Enable pull-up
    wire_write_byte(PI4IOE_ADDR, PI4IOE_REG_IO_DIR, 0x6F);     // Set input=0, output=1, set P0 as output
    wire_read_byte(PI4IOE_ADDR, PI4IOE_REG_IO_DIR);

    // Set outputs to 1
    wire_write_byte(PI4IOE_ADDR, PI4IOE_REG_IO_OUT, 0xFF);
    wire_read_byte(PI4IOE_ADDR, PI4IOE_REG_IO_OUT);
    return true;
}

bool M5EchoBase::setMute(bool mute)
{
    // PI4IOE register definitions
    wire_write_byte(PI4IOE_ADDR, PI4IOE_REG_IO_OUT, mute ? 0x00 : 0xFF);
    return true;
}

int M5EchoBase::getBufferSize(int duration, int sample_rate)
{
    if (!sample_rate) {
        sample_rate = i2s_cfg.sample_rate;
    }
    return duration * sample_rate * 2 * 2;  // sample_rate * bytes_per_sample * channels
}

int M5EchoBase::getDuration(int size, int sample_rate)
{
    if (!sample_rate) {
        sample_rate = i2s_cfg.sample_rate;
    }
    return size / (sample_rate * 2 * 2);  // sample_rate * bytes_per_sample * channels
}

bool M5EchoBase::record(FS& fs, const char* filename, int size)
{
    // Open file for writing
    File file = fs.open(filename, FILE_WRITE);
    if (!file) {
        ESP_LOGI(TAG, "Failed to open file for recording");
        return false;
    }

    // Define chunk size (e.g., 1024 bytes)
    const size_t CHUNK_SIZE = 1024;
    uint8_t buffer[CHUNK_SIZE];
    size_t total_bytes_to_record = size;  // sample_rate * bytes_per_sample * channels
    size_t bytes_recorded        = 0;

    while (bytes_recorded < total_bytes_to_record) {
        size_t bytes_to_read = CHUNK_SIZE;
        if (bytes_recorded + CHUNK_SIZE > total_bytes_to_record) {
            bytes_to_read = total_bytes_to_record - bytes_recorded;
        }

        size_t bytes_read = 0;
        esp_err_t err     = i2s_read(i2s_num, buffer, bytes_to_read, &bytes_read, portMAX_DELAY);
        if (err != ESP_OK) {
            ESP_LOGI(TAG, "Recording failed during I2S read");
            file.close();
            return false;
        }

        file.write(buffer, bytes_read);
        bytes_recorded += bytes_read;
    }

    file.close();
    return true;
}

bool M5EchoBase::record(uint8_t* buffer, int size)
{
    // Calculate the number of bytes to record
    size_t bytes_to_record = size;  // sample_rate * bytes_per_sample * channels

    size_t bytes_read = 0;
    esp_err_t err     = i2s_read(i2s_num, buffer, bytes_to_record, &bytes_read, getDuration(size) * 1000 + 1000);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Recording failed");
        return false;
    }
    return true;
}

bool M5EchoBase::play(FS& fs, const char* filename)
{
    // Open file for reading
    File file = fs.open(filename, FILE_READ);
    if (!file) {
        ESP_LOGI(TAG, "Failed to open file for playback");
        return false;
    }

    // Define chunk size (e.g., 1024 bytes)
    const size_t CHUNK_SIZE = 1024;
    uint8_t buffer[CHUNK_SIZE];

    while (file.available()) {
        size_t bytes_to_read = CHUNK_SIZE;
        if (file.available() < CHUNK_SIZE) {
            bytes_to_read = file.available();
        }

        size_t bytes_read = file.read(buffer, bytes_to_read);
        if (bytes_read > 0) {
            size_t bytes_written = 0;
            esp_err_t err        = i2s_write(i2s_num, buffer, bytes_read, &bytes_written, portMAX_DELAY);
            if (err != ESP_OK) {
                ESP_LOGI(TAG, "Playback failed during I2S write");
                file.close();
                return false;
            }
        }
    }

    file.close();
    return true;
}

bool M5EchoBase::play(const uint8_t* buffer, int size)
{
    size_t bytes_written = 0;
    esp_err_t err        = i2s_write(i2s_num, buffer, size, &bytes_written, portMAX_DELAY);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Playback failed");
        return false;
    }
    i2s_zero_dma_buffer(i2s_num);
    return true;
}
