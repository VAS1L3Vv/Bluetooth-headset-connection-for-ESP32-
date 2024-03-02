#include "bt_audio_handler.h"


// static audio_struct_t * audio_handle = &audio_data;

static bool codec2_enabled() // проверка на включенность кодек2
{
    return audio_handle->codec2_enabled;
}

static void set_codec2_state(bool on_off)
{
    audio_handle->codec2_enabled = on_off;
}

// audio_struct_t * get_audio_handle(void) {
//     return &audio_data;
// }

bool current_bt_mode()
{
    return audio_handle->sco_conn_state;
}

static void set_bt_mode(bool mode) {
    audio_handle->sco_conn_state = mode;
}

void toggle_codec2() { // вкл-выкл обработки буффера с кодек2
    if(codec2_enabled()) {
        set_codec2_state(OFF); // выключает кодек2 если прежде был включен
        printf("Codec2 processing has been disabled \n ");
        return;
    }
    else {
        set_codec2_state(ON); // включает кодек2 если прежде был выключен
        printf("Codec2 processing has been enabled \n ");
        return; }
}
void abort_audio(hci_con_handle_t acl_handle) {
    hfp_ag_release_audio_connection(acl_handle);
}

// void setup_audio(int16_t *mic_buf_ptr, int16_t *spkr_buf_ptr, int rb_size) // put here what needs to be configured
// {
//     audio_handle->mic_buff = mic_buf_ptr;
//     audio_handle->mic_buff = spkr_buf_ptr;
//     audio_handle->rb_size = rb_size;
//     audio_handle->rb_length =rb_size/2;
//     audio_handle->rb_time = rb_size/2/SAMPLE_RATE_8KHZ;
//     audio_handle->buf_size = sizeof(mic_buf_ptr);
//     audio_handle->buf_length = sizeof(mic_buf_ptr)/2;
//     audio_handle->buf_time = sizeof(mic_buf_ptr)/2/SAMPLE_RATE_8KHZ;
// }

// void report_audio_status() {
//     if(audio_handle == NULL) {
//         printf("Error: unknown handle. Provide audio handle to proceed."); return; }
//     if(audio_handle->mic_buff != NULL &&
//        audio_handle->spkr_buff != NULL ) {
//         printf("\n\n Audio buffers initialized. \n\n");
//         printf("Buffer size: %d bytes\n\n", audio_handle->buf_size); 
//         printf("Buffer length: %d elements\n\n",audio_handle->buf_length);
//         printf("Time equivalent: %f ms\n\n",(float)(audio_handle->buf_time)/1000.0);
//        }
//     else {
//         printf("Error: Mic or speaker audio buffer NOT initialiazed. \n\n"); 
//         return; }
//     printf("Total record time: %d seconds \n\n", audio_handle->rb_time);
//     printf("Total byte size: %d bytes\n\n", audio_handle->rb_size);
//     printf("Total elements in array: %d\n\n", audio_handle->rb_length);
//     printf("ringbuffer increase coefficient: %f", audio_handle->rb_coef);
//     if(audio_handle->sco_conn_state == LISTENING)
//         printf("\n\n\n Mode: Sending audio to speaker.\n\n");

//     if(audio_handle->sco_conn_state == RECORDING)
//         printf("\n\n\n Mode: recording microphone.\n\n");

//     if(codec2_enabled())
//         printf("Codec2 processing ENABLED. \n\n");
//     else
//         printf("Codec2 processing DISABLED. \n\n");
//     if(audio_handle->record_cycle_num != 0) {
//         printf("Recorded audio: %d times \n\n", audio_handle->record_cycle_num);
//         if(audio_handle->playback_cycle_num == 0)
//             printf("Did not play recorded data yet. Press 5 to listen to recorded audio. \n\n");
//         else
//             printf("Listened to audio: %d times \n\n", audio_handle->playback_cycle_num); }
//     else
//         printf("othing recorded yet. Press 4 to record voice audio. \n\n");
// }