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

#define MAX_BUFFER_SIZE_BYTES 320000 // 8000 * 2 = 16K BYTES PER SECOND. 10 SECOND AUDIO = 160K bytes
#define CODEC2_FRAME_SIZE 320
#define BUFFER_SIZE CODEC2_FRAME_SIZE

typedef struct
{
    bool sco_conn_state;
    int bytes_recieved;
    int bytes_sent;
    int16_t audio_buffer;
    int audio_buffer_size;
    bool codec2_enabled;

} audio_struct;
static audio_struct * audio_handle;

void toggle_codec2();
int record_bt_mic(int16_t audio_buffer, uint8_t seconds);
int playback_bt_speaker(int16_t audio_buffer, uint8_t seconds);
#endif

