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

#define LOG_TAG "AUDIO_DRIVER"

#define BTSTACK_AUDIO_I2S_BCK GPIO_NUM_5
#define BTSTACK_AUDIO_I2S_WS  GPIO_NUM_25
#define BTSTACK_AUDIO_I2S_OUT GPIO_NUM_26
#define BTSTACK_AUDIO_I2S_IN  GPIO_NUM_35

#define BTSTACK_AUDIO_I2S_NUM  (I2S_NUM_0)

#define DRIVER_POLL_INTERVAL_MS          5
#define DMA_BUFFER_COUNT                 2
#define BYTES_PER_SAMPLE_STEREO          4

// one DMA buffer for max sample rate
#define MAX_DMA_BUFFER_SAMPLES (48000 * 2 * DRIVER_POLL_INTERVAL_MS/ 1000)

typedef enum {
    BTSTACK_AUDIO_ESP32_OFF = 0,
    BTSTACK_AUDIO_ESP32_INITIALIZED,
    BTSTACK_AUDIO_ESP32_STREAMING
} btstack_audio_esp32_state_t;

static bool btstack_audio_esp32_i2s_installed;
static bool btstack_audio_esp32_i2s_streaming;
static uint32_t btstack_audio_esp32_i2s_samplerate;
static uint16_t btstack_audio_esp32_samples_per_dma_buffer;

// timer to fill output ring buffer
static btstack_timer_source_t  btstack_audio_esp32_driver_timer;

static uint8_t  btstack_audio_esp32_sink_num_channels;
static uint32_t btstack_audio_esp32_sink_samplerate;

static uint8_t  btstack_audio_esp32_source_num_channels;
static uint32_t btstack_audio_esp32_source_samplerate;

static btstack_audio_esp32_state_t btstack_audio_esp32_sink_state;
static btstack_audio_esp32_state_t btstack_audio_esp32_source_state;

// client
static void (*btstack_audio_esp32_sink_playback_callback)(int16_t * buffer, uint16_t num_samples);
static void (*btstack_audio_esp32_source_recording_callback)(const int16_t * buffer, uint16_t num_samples);

// queue for RX/TX done events
static QueueHandle_t btstack_audio_esp32_i2s_event_queue;

// prototypes
static void btstack_audio_esp32_sink_fill_buffer(void);
static void btstack_audio_esp32_source_process_buffer(void);

static void btstack_audio_esp32_driver_timer_handler(btstack_timer_source_t * ts){
    // read max 2 events from i2s event queue
    i2s_event_t i2s_event;
    int i;
    for (i=0;i<2;i++){
        if( xQueueReceive( btstack_audio_esp32_i2s_event_queue, &i2s_event, 0) == false) break;
        switch (i2s_event.type){
            case I2S_EVENT_TX_DONE:
                log_debug("I2S_EVENT_TX_DONE");
                btstack_audio_esp32_sink_fill_buffer();
                break;
            case I2S_EVENT_RX_DONE:
                log_debug("I2S_EVENT_RX_DONE");
                btstack_audio_esp32_source_process_buffer();
                break;
            default:
                break;
        }
    }

    // re-set timer
    btstack_run_loop_set_timer(ts, DRIVER_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(ts);
}

static void btstack_audio_esp32_stream_start(void){
    if (btstack_audio_esp32_i2s_streaming) return;

    // start i2s
    log_info("i2s stream start");
    i2s_start(BTSTACK_AUDIO_I2S_NUM);

    // start timer
    btstack_run_loop_set_timer_handler(&btstack_audio_esp32_driver_timer, &btstack_audio_esp32_driver_timer_handler);
    btstack_run_loop_set_timer(&btstack_audio_esp32_driver_timer, DRIVER_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(&btstack_audio_esp32_driver_timer);

    btstack_audio_esp32_i2s_streaming = true;
}

static void btstack_audio_esp32_stream_stop(void){
    if (btstack_audio_esp32_i2s_streaming == false) return;

    // check if still needed
    bool still_needed = (btstack_audio_esp32_sink_state   == BTSTACK_AUDIO_ESP32_STREAMING)
                     || (btstack_audio_esp32_source_state == BTSTACK_AUDIO_ESP32_STREAMING);
    if (still_needed) return;

    // stop timer
    btstack_run_loop_remove_timer(&btstack_audio_esp32_driver_timer);

    // stop i2s
    log_info("i2s stream stop");
    i2s_stop(BTSTACK_AUDIO_I2S_NUM);

    btstack_audio_esp32_i2s_streaming = false;
}

static void btstack_audio_esp32_init(void){

    // de-register driver if already installed
    if (btstack_audio_esp32_i2s_installed){
        i2s_driver_uninstall(BTSTACK_AUDIO_I2S_NUM);
    }

    // set i2s mode, sample rate and pins based on sink / source config
    i2s_mode_t i2s_mode  = I2S_MODE_MASTER;
    int i2s_data_out_pin = I2S_PIN_NO_CHANGE;
    int i2s_data_in_pin  = I2S_PIN_NO_CHANGE;
    btstack_audio_esp32_i2s_samplerate = 0;

    if (btstack_audio_esp32_sink_state != BTSTACK_AUDIO_ESP32_OFF){
        i2s_mode |= I2S_MODE_TX; // playback
        i2s_data_out_pin = BTSTACK_AUDIO_I2S_OUT;
        if (btstack_audio_esp32_i2s_samplerate != 0){
            btstack_assert(btstack_audio_esp32_i2s_samplerate == btstack_audio_esp32_sink_samplerate);
        }
        btstack_audio_esp32_i2s_samplerate = btstack_audio_esp32_sink_samplerate;
        btstack_audio_esp32_samples_per_dma_buffer = btstack_audio_esp32_i2s_samplerate * 2 * DRIVER_POLL_INTERVAL_MS / 1000;
    }

    if (btstack_audio_esp32_source_state != BTSTACK_AUDIO_ESP32_OFF){
        i2s_mode |= I2S_MODE_RX; // recording
        i2s_data_in_pin = BTSTACK_AUDIO_I2S_IN;
        if (btstack_audio_esp32_i2s_samplerate != 0){
            btstack_assert(btstack_audio_esp32_i2s_samplerate == btstack_audio_esp32_source_samplerate);
        }
        btstack_audio_esp32_i2s_samplerate = btstack_audio_esp32_source_samplerate;
        btstack_audio_esp32_samples_per_dma_buffer = btstack_audio_esp32_i2s_samplerate * 2 * DRIVER_POLL_INTERVAL_MS / 1000;
    }

    btstack_assert(btstack_audio_esp32_samples_per_dma_buffer <= MAX_DMA_BUFFER_SAMPLES);

    i2s_config_t config =
    {
        .mode                 = i2s_mode,
        .sample_rate          = btstack_audio_esp32_i2s_samplerate,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .dma_buf_count        = DMA_BUFFER_COUNT,                           // Number of DMA buffers. Max 128.
        .dma_buf_len          = btstack_audio_esp32_samples_per_dma_buffer, // Size of each DMA buffer in samples. Max 1024.
        .use_apll             = true,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1
    };

    i2s_pin_config_t pins =
    {
        .bck_io_num           = BTSTACK_AUDIO_I2S_BCK,
        .ws_io_num            = BTSTACK_AUDIO_I2S_WS,
        .data_out_num         = i2s_data_out_pin,
        .data_in_num          = i2s_data_in_pin
    };

    log_info("i2s init mode 0x%02x, samplerate %" PRIu32 ", samples per DMA buffer: %u", 
        i2s_mode, btstack_audio_esp32_sink_samplerate, btstack_audio_esp32_samples_per_dma_buffer);

    i2s_driver_install(BTSTACK_AUDIO_I2S_NUM, &config, DMA_BUFFER_COUNT, &btstack_audio_esp32_i2s_event_queue);
    i2s_set_pin(BTSTACK_AUDIO_I2S_NUM, &pins);

    btstack_audio_esp32_i2s_installed = true;
}

static void btstack_audio_esp32_deinit(void){
    if  (btstack_audio_esp32_i2s_installed == false) return;

    // check if still needed
    bool still_needed = (btstack_audio_esp32_sink_state   != BTSTACK_AUDIO_ESP32_OFF)
                    ||  (btstack_audio_esp32_source_state != BTSTACK_AUDIO_ESP32_OFF);
    if (still_needed) return;

    // uninstall driver
    log_info("i2s close");
    i2s_driver_uninstall(BTSTACK_AUDIO_I2S_NUM);

    btstack_audio_esp32_i2s_installed = false;
}