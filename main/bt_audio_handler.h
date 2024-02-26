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

#define SENDING_PACKET 0
#define RECIEVING_PACKET 1
#define ON 0 
#define OFF 1
#define AUDIO_SECONDS 10
#define SECONDS_TO_BYTES AUDIO_SECONDS*BYTES_PER_FRAME*SAMPLE_RATE_8KHZ

#define MAX_BUFFER_SIZE_BYTES SECONDS_TO_BYTES*2 // maximum rb size to avoid overflow
#define CODEC2_FRAME_SIZE 320
#define FRAME_SIZE CODEC2_FRAME_SIZE
#define NUMBER_OF_FRAMES SECONDS_TO_BYTES / FRAME_SIZE

typedef struct {

    bool sco_conn_state;
    int bytes_recieved;
    int bytes_sent;
    int16_t * audio_buffer;
    int buffer_byte_size;
    bool codec2_enabled;
    int record_cycle;

} audio_struct_t;
audio_struct_t * audio_handle = NULL;

void report_status();
void toggle_codec2();
void record_bt_microphone(int16_t audio_buffer, int bytes);
int playback_bt_speaker(int16_t audio_buffer, int bytes);
void abort_audio();
audio_struct_t *audio_handle_init(int16_t *buffer_ptr, int byte_size);
#endif
