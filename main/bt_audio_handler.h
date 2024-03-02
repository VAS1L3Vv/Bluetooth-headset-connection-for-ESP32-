#ifndef BT_AUDIO_HANDLER_H
#define BT_AUDIO_HANDLER_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "btstack.h"
#include "btstack_config.h"
#include "btstack_stdin.h"
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
