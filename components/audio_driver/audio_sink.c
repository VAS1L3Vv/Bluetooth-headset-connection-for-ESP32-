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

#define LOG_TAG "AUDIO_SINK"

// SINK Implementation

static uint8_t btstack_audio_esp32_sink_buffer[MAX_DMA_BUFFER_SAMPLES * BYTES_PER_SAMPLE_STEREO];
static bool btstack_audio_esp32_sink_buffer_ready;

static void btstack_audio_esp32_sink_fill_buffer(void){

    btstack_assert(btstack_audio_esp32_samples_per_dma_buffer <= MAX_DMA_BUFFER_SAMPLES);

    // fetch new data
    size_t bytes_written;
    uint16_t data_len = btstack_audio_esp32_samples_per_dma_buffer * BYTES_PER_SAMPLE_STEREO;
    if (btstack_audio_esp32_sink_buffer_ready == false){
        if (btstack_audio_esp32_sink_state == BTSTACK_AUDIO_ESP32_STREAMING)
        {
            (*btstack_audio_esp32_sink_playback_callback)((int16_t *) btstack_audio_esp32_sink_buffer, btstack_audio_esp32_samples_per_dma_buffer);
            // duplicate samples for mono
            if (btstack_audio_esp32_sink_num_channels == 1)
            {
                int16_t i;
                int16_t * buffer16 = (int16_t *) btstack_audio_esp32_sink_buffer;
                for (i=btstack_audio_esp32_samples_per_dma_buffer-1;i >= 0; i--){
                    buffer16[2*i  ] = buffer16[i];
                    buffer16[2*i+1] = buffer16[i];
                }
            }
            btstack_audio_esp32_sink_buffer_ready = true;
        } else {
            memset(btstack_audio_esp32_sink_buffer, 0, data_len);
        }
    }

    i2s_write(BTSTACK_AUDIO_I2S_NUM, btstack_audio_esp32_sink_buffer, data_len, &bytes_written, 0);

    // check if all data has been written. tolerate writing zero bytes (->retry), but assert on partial write
    if (bytes_written == data_len){
        btstack_audio_esp32_sink_buffer_ready = false;
    } else if (bytes_written == 0){
        ESP_LOGW(LOG_TAG, "i2s_write: couldn't write after I2S_EVENT_TX_DONE\n");
    } else {
        ESP_LOGE(LOG_TAG, "i2s_write: only %u of %u!!!\n", (int) bytes_written, data_len);
        btstack_assert(false);
    }
}

static int btstack_audio_esp32_sink_init(uint8_t channels, uint32_t samplerate, void (*playback)(int16_t * buffer, uint16_t num_samples)){

    btstack_assert(playback != NULL);
    btstack_assert((1 <= channels) && (channels <= 2));

    // store config
    btstack_audio_esp32_sink_playback_callback  = playback;
    btstack_audio_esp32_sink_num_channels       = channels;
    btstack_audio_esp32_sink_samplerate         = samplerate;

    btstack_audio_esp32_sink_state = BTSTACK_AUDIO_ESP32_INITIALIZED;
    
    // init i2s and codec
    btstack_audio_esp32_init();
    return 0;
}

static uint32_t btstack_audio_esp32_sink_get_samplerate(void) {
    return btstack_audio_esp32_sink_samplerate;
}

static void btstack_audio_esp32_sink_set_volume(uint8_t gain) {
    UNUSED(gain);
}

static void btstack_audio_esp32_sink_start_stream(void){

    if (btstack_audio_esp32_sink_state != BTSTACK_AUDIO_ESP32_INITIALIZED) return;

    // validate samplerate
    btstack_assert(btstack_audio_esp32_sink_samplerate == btstack_audio_esp32_i2s_samplerate);

    // state
    btstack_audio_esp32_sink_state = BTSTACK_AUDIO_ESP32_STREAMING;
    btstack_audio_esp32_sink_buffer_ready = false;
    
    // note: conceptually, it would make sense to pre-fill all I2S buffers and then feed new ones when they are
    // marked as complete. However, it looks like we get additoinal events and then assert below, 
    // so we just don't pre-fill them here

    btstack_audio_esp32_stream_start();
}

static void btstack_audio_esp32_sink_stop_stream(void){

    if (btstack_audio_esp32_sink_state != BTSTACK_AUDIO_ESP32_STREAMING) return;

    // state
    btstack_audio_esp32_sink_state = BTSTACK_AUDIO_ESP32_INITIALIZED;

    btstack_audio_esp32_stream_stop();
}

static void btstack_audio_esp32_sink_close(void){

    if (btstack_audio_esp32_sink_state == BTSTACK_AUDIO_ESP32_STREAMING) {
        btstack_audio_esp32_sink_stop_stream();
    }

    // state
    btstack_audio_esp32_sink_state = BTSTACK_AUDIO_ESP32_OFF;

    btstack_audio_esp32_deinit();
}

static const btstack_audio_sink_t btstack_audio_esp32_sink = {
    .init           = &btstack_audio_esp32_sink_init,
    .get_samplerate = &btstack_audio_esp32_sink_get_samplerate,
    .set_volume     = &btstack_audio_esp32_sink_set_volume,
    .start_stream   = &btstack_audio_esp32_sink_start_stream,
    .stop_stream    = &btstack_audio_esp32_sink_stop_stream,
    .close          = &btstack_audio_esp32_sink_close
};

const btstack_audio_sink_t * btstack_audio_esp32_sink_get_instance(void){
    return &btstack_audio_esp32_sink;
}

