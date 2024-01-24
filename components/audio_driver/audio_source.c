#include <stdio.h>
#include "audio_driver.h"
#include "btstack_config.h"
#include "btstack_debug.h"
#include "btstack_run_loop.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/i2s.h"
#include "esp_log.h"
#include <inttypes.h>
#include <string.h>

#define LOG_TAG "AUDIO_SOURCE"

// SOURCE Implementation

static uint8_t  btstack_audio_esp32_source_num_channels;
static uint32_t btstack_audio_esp32_source_samplerate;
static btstack_audio_esp32_state_t btstack_audio_esp32_source_state;
static void (*btstack_audio_esp32_source_recording_callback)(const int16_t * buffer, uint16_t num_samples);

static void btstack_audio_esp32_source_process_buffer(void){
    size_t bytes_read;
    uint8_t buffer[MAX_DMA_BUFFER_SAMPLES * BYTES_PER_SAMPLE_STEREO];

    btstack_assert(btstack_audio_esp32_samples_per_dma_buffer <= MAX_DMA_BUFFER_SAMPLES);

    uint16_t data_len = btstack_audio_esp32_samples_per_dma_buffer * BYTES_PER_SAMPLE_STEREO;
    i2s_read(BTSTACK_AUDIO_I2S_NUM, buffer, data_len, &bytes_read, 0);
    btstack_assert(bytes_read == data_len);

    int16_t * buffer16 = (int16_t *) buffer;
    if (btstack_audio_esp32_source_state == BTSTACK_AUDIO_ESP32_STREAMING) {
        // drop second channel if configured for mono
        if (btstack_audio_esp32_source_num_channels == 1){
            uint16_t i;
            for (i=0;i<btstack_audio_esp32_samples_per_dma_buffer;i++){
                buffer16[i] = buffer16[2*i];
            }
        }
        (*btstack_audio_esp32_source_recording_callback)(buffer16, btstack_audio_esp32_samples_per_dma_buffer);
    }
}

static int btstack_audio_esp32_source_init(uint8_t channels, uint32_t samplerate, void (*recording)(const int16_t * buffer, uint16_t num_samples)){
    btstack_assert(recording != NULL);

    // store config
    btstack_audio_esp32_source_recording_callback = recording;
    btstack_audio_esp32_source_num_channels       = channels;
    btstack_audio_esp32_source_samplerate         = samplerate;

    btstack_audio_esp32_source_state = BTSTACK_AUDIO_ESP32_INITIALIZED;

    // init i2s and codec
    btstack_audio_esp32_init();
    return 0;
}

static uint32_t btstack_audio_esp32_source_get_samplerate(void) {
    return btstack_audio_esp32_source_samplerate;
}

static void btstack_audio_esp32_source_set_gain(uint8_t gain) {
    UNUSED(gain);
}

static void btstack_audio_esp32_source_start_stream(void){

    if (btstack_audio_esp32_source_state != BTSTACK_AUDIO_ESP32_INITIALIZED) return;

    // validate samplerate
    btstack_assert(btstack_audio_esp32_source_samplerate == btstack_audio_esp32_i2s_samplerate);

    // state
    btstack_audio_esp32_source_state = BTSTACK_AUDIO_ESP32_STREAMING;

    btstack_audio_esp32_stream_start();
}

static void btstack_audio_esp32_source_stop_stream(void){

    if (btstack_audio_esp32_source_state != BTSTACK_AUDIO_ESP32_STREAMING) return;

    // state
    btstack_audio_esp32_source_state = BTSTACK_AUDIO_ESP32_INITIALIZED;

    btstack_audio_esp32_stream_stop();
}

static void btstack_audio_esp32_source_close(void){

    if (btstack_audio_esp32_source_state == BTSTACK_AUDIO_ESP32_STREAMING) {
        btstack_audio_esp32_source_stop_stream();
    }

    // state
    btstack_audio_esp32_source_state = BTSTACK_AUDIO_ESP32_OFF;

    btstack_audio_esp32_deinit();
}

static const btstack_audio_source_t btstack_audio_esp32_source = {
    .init           = &btstack_audio_esp32_source_init,
    .get_samplerate = &btstack_audio_esp32_source_get_samplerate,
    .set_gain       = &btstack_audio_esp32_source_set_gain,
    .start_stream   = &btstack_audio_esp32_source_start_stream,
    .stop_stream    = &btstack_audio_esp32_source_stop_stream,
    .close          = &btstack_audio_esp32_source_close
};

const btstack_audio_source_t * btstack_audio_esp32_source_get_instance(void){
    return &btstack_audio_esp32_source;
}

