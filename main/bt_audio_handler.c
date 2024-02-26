#include "bt_audio_handler.h"

static bool codec2_enabled() // проверка на включенность кодек2
{
    return audio_handle->codec2_enabled;
}
static void set_codec2_state(bool on_off)
{
    audio_handle->codec2_enabled = on_off;
}
void report_status()
{   if(audio_handle->sco_conn_state == SENDING_PACKET)
        printf("\nMode: Sending audio to speaker.\n");
    if(audio_handle->sco_conn_state == RECIEVING_PACKET)
        printf("Mode: recording microphone.");
    if(audio_handle->audio_buffer) {
        printf("Audio buffer initialized. \n");
        printf("Buffer size in bytes:\t %d", sizeof(*(audio_handle->audio_buffer))); 
        printf("Equvilent to: %d seconds", sizeof(*(audio_handle->audio_buffer))/SAMPLE_RATE_8KHZ/BYTES_PER_SAMPLE);
        printf("Expected buffer size: %i",audio_handle->buffer_byte_size);
    }
    else
        printf("Audio buffer NOT initialiazed. \n");
    if(codec2_enabled)
        printf("Codec2 processing ENABLED. \n");
    else
        printf("Codec2 processing DISABLED. \n");
    if(audio_handle->record_cycle == 0)
        printf("Nothing recorded yet. Press 4 to record voice audio. \n");
    else
        printf("Record cycle: currently on %d \n");
}
void toggle_codec2() // вкл-выкл обработки буффера с кодек2
{
    if(codec2_enabled) {
        set_codec2_state(OFF); // выключает кодек2 если прежде был включен
        printf("Codec2 processing has been disabled \n ");
        return;
    }
    else {
        set_codec2_state(ON); // включает кодек2 если прежде был выключен
        printf("Codec2 processing has been enabled \n ");
        return;
}

void abort_audio()
{
    
}

audio_struct_t * audio_handle_init(int16_t * buffer_ptr, int byte_size)
{
    audio_struct_t audio_struct = {
        .audio_buffer = buffer_ptr,
        .buffer_byte_size = byte_size,
        .bytes_recieved = 0,
        .bytes_sent = 0,
        .codec2_enabled = OFF,
        .record_cycle = 0,
        .sco_conn_state = SENDING_PACKET,
    };
    return &audio_struct;
}

void record_bt_microphone(int16_t audio_buffer, int bytes){
    if(!audio_handle) {
        audio_handle = audio_handle_init(audio_buffer, bytes);
        report_status();
    }
    set_bt_mode(RECIEVING_PACKET);
    hfp_ag_establish_audio_connection();
    int bt_rd = 0;
    return bt_rd;
}

int playback_bt_speaker(int16_t audio_buffer, int bytes){
    if(audio_handle->record_cycle == 0) {
        printf("Nothing recorded yet. Press 4 to record voice audio. \n");
    } 
    set_bt_mode(SENDING_PACKET);
    
    return bt_wr;
}