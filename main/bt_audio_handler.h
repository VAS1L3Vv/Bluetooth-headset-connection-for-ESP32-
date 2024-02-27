#ifndef BT_AUDIO_HANDLER_H
#define BT_AUDIO_HANDLER_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack.h"
#include "audio_control.h"
#include "btstack_config.h"
#include "btstack_stdin.h"

#define LISTENING 0
#define RECORDING 1
#define OFF 0
#define ON 1
#define AUDIO_SECONDS 10
#define BYTES_PER_SAMPLE 2
#define SECONDS_TO_BYTES AUDIO_SECONDS*BYTES_PER_SAMPLE*SAMPLE_RATE_8KHZ
#define MAX_BUFFER_SIZE_BYTES SECONDS_TO_BYTES*2 // maximum rb size = double 160k to avoid overflow
#define CODEC2_FRAME_SIZE 320
#define FRAME_SIZE CODEC2_FRAME_SIZE
#define NUMBER_OF_FRAMES SECONDS_TO_BYTES / FRAME_SIZE

typedef struct {

    int16_t * audio_buffer;
    int buffer_byte_size;
    bool codec2_enabled;
    int record_cycle_num;
    int playback_cycle_num;
    bool sco_conn_state;

} audio_struct_t;

static audio_struct_t audio_struct = {
        .audio_buffer = NULL,
        .buffer_byte_size = 0,
        .codec2_enabled = OFF,
        .record_cycle_num = 0,
        .playback_cycle_num = 0,
        .sco_conn_state = LISTENING, // при инициализации стоит в режиме "прослушивания" пакетов
    }; // изначальные данные

static audio_struct_t * audio_handle = &audio_struct;

bool current_bt_mode();
void report_audio_status();
void toggle_codec2();
void record_bt_microphone(hci_con_handle_t acl_handle, int16_t * audio_buffer, int buf_size);
void playback_bt_speaker(hci_con_handle_t acl_handle, int16_t * audio_buffer, int bytes);
void abort_audio(hci_con_handle_t acl_handle);
void setup_audio(int16_t *buffer_ptr, int byte_size);

#endif
