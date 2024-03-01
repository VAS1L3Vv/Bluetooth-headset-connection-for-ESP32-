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

#define RECORDING 1
#define LISTENING 0
#define ON 1
#define OFF 0
#define AUDIO_SECONDS 10
#define BYTES_PER_SAMPLE 2
#define SECONDS_TO_BYTES AUDIO_SECONDS*BYTES_PER_SAMPLE*SAMPLE_RATE_8KHZ
#define MAX_BUFFER_SIZE_BYTES SECONDS_TO_BYTES*2 // maximum rb size = double 160k to avoid overflow
#define CODEC2_FRAME_SIZE 320
#define FRAME_SIZE CODEC2_FRAME_SIZE
#define FRAME_LENGTH FRAME_SIZE/BYTES_PER_SAMPLE
#define NUMBER_OF_FRAMES SECONDS_TO_BYTES / FRAME_SIZE

typedef struct {
    int16_t * mic_buff;
    int16_t * spkr_buff;
    int buf_size;
    int buf_length;
    int buf_time;
    int rb_size;
    int rb_length;
    int rb_time;
    float rb_coef;
    bool codec2_enabled;
    int record_cycle_num;
    int playback_cycle_num;
    bool sco_conn_state;
} audio_struct_t;

audio_struct_t * get_audio_handle(void);
bool current_bt_mode(void);
void report_audio_status(void);
void toggle_codec2(void);
void record_bt_microphone(hci_con_handle_t acl_handle);
void playback_bt_speaker(hci_con_handle_t acl_handle);
void abort_audio(hci_con_handle_t acl_handle);
void setup_audio(int16_t *mic_buf_ptr, int16_t *spkr_buf_ptr, int rb_size);

#endif
