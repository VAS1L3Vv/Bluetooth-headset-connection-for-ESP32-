#include "bt_audio_handler.h"

static bool codec2_enabled() // проверка на включенность кодек2
{
    return audio_handle->codec2_enabled;
}
static void set_codec2_state(bool on_off)
{
    audio_handle->codec2_enabled = on_off;
}
void toggle_codec2() // вкл-выкл обработки буффера с кодек2
{
    if(codec2_enabled) {
        set_codec2_state(OFF); // выключает кодек2 если прежде был включен
        printf("Codec2 proccessing has been disabled \n ");
        return;
    }
    else set_codec2_state(ON); // включает кодек2 если прежде был выключен
        return;
}

int record_bt_mic(int16_t audio_buffer, uint8_t seconds){ 
    int bt_rd = 0;
    set_bt_mode(SENDING_PACKET);

    return bt_rd;
}

int playback_bt_speaker(int16_t audio_buffer, uint8_t seconds){
    
    set_bt_mode(SENDING_PACKET);
    return bt_wr;
}