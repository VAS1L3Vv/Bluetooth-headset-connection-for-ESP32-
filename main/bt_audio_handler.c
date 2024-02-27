#include "bt_audio_handler.h"

static bool codec2_enabled() // проверка на включенность кодек2
{
    return audio_handle->codec2_enabled;
}

static void set_codec2_state(bool on_off)
{
    audio_handle->codec2_enabled = on_off;
}

bool current_bt_mode() {
    return audio_handle->sco_conn_state;
}

static void set_bt_mode(bool mode) {
    audio_handle->sco_conn_state = mode;
}


void report_audio_status()
{
    if(!audio_handle) {
        printf("\n\n\n Audio not initialized!! Press 'record micorphone' button to start audio setup once. \n");
            return; }
    if(audio_handle->sco_conn_state == LISTENING)
        printf("\n\n\n Mode: Sending audio to speaker.\n\n");

    if(audio_handle->sco_conn_state == RECORDING)
        printf("\n\n\n Mode: recording microphone.\n\n");

    if(audio_handle->audio_buffer) {
        printf("Audio buffer initialized. \n\n");
        printf("Buffer size in bytes:\t %d \n\n", sizeof(*(audio_handle->audio_buffer))); 
        printf(" Equvilent to: %i seconds\n\n", (int)(sizeof((audio_handle->audio_buffer))/SAMPLE_RATE_8KHZ/BYTES_PER_SAMPLE));
        printf("Expected buffer size: %i \n\n",audio_handle->buffer_byte_size);
    }
    else
        printf("Audio buffer NOT initialiazed. \n\n");
    if(codec2_enabled())
        printf("Codec2 processing ENABLED. \n\n");
    else
        printf("Codec2 processing DISABLED. \n\n");
    if(audio_handle->record_cycle_num != 0)
    {
        printf("Recorded audio: %d times \n\n", audio_handle->record_cycle_num);
        if(audio_handle->playback_cycle_num == 0)
            printf("Did not play recorded data yet. Press 5 to listen to recorded audio. \n\n");
        else
            printf("Listened to audio: %d times \n\n", audio_handle->playback_cycle_num);
    }
    else
        printf("Nothing recorded yet. Press 4 to record voice audio. \n\n");
}

void toggle_codec2() // вкл-выкл обработки буффера с кодек2
{
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

void setup_audio(int16_t * buffer_ptr, int byte_size) // put here what needs to be configured
{
    audio_handle->audio_buffer = buffer_ptr;
    audio_handle->buffer_byte_size = byte_size;
}

void record_bt_microphone(hci_con_handle_t acl_handle, int16_t * audio_buffer, int buf_size)
{
    if(audio_handle) {
        setup_audio(audio_buffer, buf_size);
        printf("First time init. Allocating buffers for %d second sound.", buf_size);
        report_audio_status();
    }
    else { printf("Error: unknown handle. Provide correct handle to proceed."); return; }
    set_bt_mode(RECORDING);
    // hfp_ag_establish_audio_connection(acl_handle);

    audio_handle->record_cycle_num++; // AT THE VERY END
    report_audio_status();
}

void playback_bt_speaker(hci_con_handle_t acl_handle, int16_t * audio_buffer, int bytes) {
    if(audio_handle->record_cycle_num == 0) {
        printf("Nothing recorded yet. Press 4 to record voice audio. \n");
        return; }
    set_bt_mode(LISTENING);
    // hfp_ag_establish_audio_connection(acl_handle);

    audio_handle->playback_cycle_num++; // AT THE VERY END
    report_audio_status();
}