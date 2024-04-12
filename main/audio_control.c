#define BTSTACK_FILE__ "sco_util.c"

#include <stdio.h>
#include "audio_control.h"
#include "btstack_debug.h"
#include "btstack_ring_buffer.h"
#include "classic/btstack_cvsd_plc.h"
#include "classic/btstack_sbc.h"
#include "classic/hfp.h"
#include "codec2.h"

// number of sco packets until 'report' on console
#define SCO_REPORT_PERIOD           130
// constants
#define REFILL_SAMPLES          16 // проверка и запись в буффер передачи по 16 элементов
// audio pre-buffer - also defines latency
#define SCO_PREBUFFER_MS      20
#define PREBUFFER_BYTES_8KHZ  (SCO_PREBUFFER_MS *  SAMPLE_RATE_8KHZ/1000 * BYTES_PER_FRAME)
#define PREBUFFER_BYTES_MAX PREBUFFER_BYTES_8KHZ
#define SAMPLES_PER_FRAME_MAX 60
static uint16_t              audio_prebuffer_bytes;
static int                   audio_output_paused  = 0;
static int                   audio_input_paused   = 0;
static uint8_t               codec2_frame_storage[NUMBER_OF_FRAMES*CODEC2_FRAME_SIZE]; // указатель памяти кольцевого буффера
static uint8_t               encode_ring_buffer_storage[ENC_BUFFER_SIZE_BYTES];
static uint8_t               decode_ring_buffer_storage[ENC_BUFFER_SIZE_BYTES];

static btstack_ring_buffer_t encode_ring_buffer;
static btstack_ring_buffer_t decode_ring_buffer;

static int count_sent = 0;
static int count_received = 0;
static btstack_cvsd_plc_state_t cvsd_plc_state;
static uint8_t sec = 0;
static int wi = 0;
static int ri = 0;

// generic codec support
typedef struct {
    void (*init)(void);
    void(*receive)(const uint8_t * packet, uint16_t size);
    void (*fill_payload)(uint8_t * payload_buffer, uint16_t sco_payload_length);
    void (*close)(void);
    uint16_t sample_rate;
} codec_support_t;

static const codec_support_t * codec_current = NULL;

static bool is_codec2_on = OFF;

bool codec2_enabled()
{
    return is_codec2_on;
}

void set_codec2_state(bool on_off)
{
    is_codec2_on = on_off;
}

struct CODEC2 * cdc2_s;

// return 1 if ok
static int audio_initialize(int sample_rate) {
    cdc2_s = codec2_create(8);
    ri = 0;
    wi = 0;
    memset(codec2_frame_storage, 0, sizeof(codec2_frame_storage));
    memset(encode_ring_buffer_storage, 0, sizeof(encode_ring_buffer_storage));
    memset(decode_ring_buffer_storage, 0, sizeof(decode_ring_buffer_storage));
    btstack_ring_buffer_init(&encode_ring_buffer, encode_ring_buffer_storage, sizeof(encode_ring_buffer_storage));
    btstack_ring_buffer_init(&decode_ring_buffer, decode_ring_buffer_storage, sizeof(decode_ring_buffer_storage));
    audio_output_paused  = 1;
    return 1;
}

// CVSD - 8 kHz
static void cvsd_init(void) { 
    printf("SCO Demo: Init CVSD\n");
    btstack_cvsd_plc_init(&cvsd_plc_state);
}

static void cvsd_receive(const uint8_t * packet, uint16_t size) {
    static int16_t audio_frame_out[SCO_PACKET_SIZE];
    if (size > sizeof(audio_frame_out)){
        printf("cvsd_receive: SCO packet larger than local output buffer - dropping data.\n");
        return;
    }
    const int audio_bytes_read = size - 3;
    const int num_samples = audio_bytes_read / BYTES_PER_FRAME;

    // convert into host endian
    static int16_t audio_frame_in[SCO_PACKET_SIZE];

    for (int i=0;i<num_samples;i++){
        audio_frame_in[i] = little_endian_read_16(packet, 3 + i * 2);
    }

    // treat packet as bad frame if controller does not report 'all good'
    bool bad_frame = (packet[1] & 0x30) != 0;
    btstack_cvsd_plc_process_data(&cvsd_plc_state, bad_frame, audio_frame_in, num_samples, audio_frame_out);
        btstack_ring_buffer_write(&encode_ring_buffer, (uint8_t *)audio_frame_out, audio_bytes_read);        
}

#define FRAME_ENCODE_SUCCESS 0
#define NOT_ENOUGH_BYTES 1
#define FRAME_BUFFER_FULL 2

static const uint32_t audio_packet_length = CODEC2_AUDIO_SIZE;
    
static int encode_and_write() 
{
    printf("\n Bytes to read: %d \n", btstack_ring_buffer_bytes_available(&encode_ring_buffer));
    if(btstack_ring_buffer_bytes_available(&encode_ring_buffer) < 640)
    return NOT_ENOUGH_BYTES;

        if(wi == NUMBER_OF_FRAMES)
        {
            return FRAME_BUFFER_FULL;
        }
        uint8_t audio_to_encode[CODEC2_AUDIO_SIZE*BYTES_PER_SAMPLE];
        uint8_t encoded_frame_bytes[CODEC2_FRAME_SIZE*BYTES_PER_SAMPLE];
    static uint32_t bytes_read = 0;
    btstack_ring_buffer_read(&encode_ring_buffer, (uint8_t*)audio_to_encode, audio_packet_length*2, &bytes_read);
    codec2_encode(cdc2_s, &encoded_frame_bytes, &audio_to_encode);
    memcpy(&codec2_frame_storage+wi, &encoded_frame_bytes, CODEC2_FRAME_SIZE);
    wi++;
    return FRAME_ENCODE_SUCCESS;
}

static int read_and_decode()
{
    printf("\n Bytes to write: %d \n", btstack_ring_buffer_bytes_free(&decode_ring_buffer));
    if(btstack_ring_buffer_bytes_free(&decode_ring_buffer) < 640) 
    return NOT_ENOUGH_BYTES; 

        if(ri == NUMBER_OF_FRAMES)
        {
            return FRAME_BUFFER_FULL;
        }
        uint8_t decoded_audio[CODEC2_AUDIO_SIZE*BYTES_PER_SAMPLE];
        uint8_t encoded_frame_bytes[CODEC2_FRAME_SIZE*BYTES_PER_SAMPLE];
    memcpy(&encoded_frame_bytes, &codec2_frame_storage+ri, CODEC2_FRAME_SIZE);
    codec2_decode(cdc2_s, &decoded_audio, &encoded_frame_bytes);
    btstack_ring_buffer_write(&decode_ring_buffer, (uint8_t*)decoded_audio, audio_packet_length*2);
    ri++;
    return FRAME_ENCODE_SUCCESS;
}

static void cvsd_fill_payload(uint8_t * payload_buffer, uint16_t sco_payload_length) {
    uint16_t bytes_to_copy = sco_payload_length;
    uint16_t pos = 0;
    if (!audio_input_paused){
        uint16_t samples_to_copy = sco_payload_length / 2;
        uint32_t bytes_read = 0;
        btstack_ring_buffer_read(&decode_ring_buffer, payload_buffer, sco_payload_length, &bytes_read);
        if (btstack_is_big_endian()) {
            uint16_t i;
            for (i=0;i<samples_to_copy/2;i+=2){
                uint8_t tmp           = payload_buffer[i*2];
                payload_buffer[i*2]   = payload_buffer[i*2+1];
                payload_buffer[i*2+1] = tmp;
            }
        }
        bytes_to_copy -= bytes_read;
        pos           += bytes_read;
    }
    // fill with 0 if not enough
    if (bytes_to_copy){
        memset(payload_buffer + pos, 0, bytes_to_copy);
        audio_input_paused = 1;
    }
}

static void cvsd_fill_null(uint8_t * payload_buffer, uint16_t sco_payload_length){}
static void cvsd_receive_null(const uint8_t * packet, uint16_t size){}

static void cvsd_close(void) { 
    printf("Used CVSD with PLC, number of proccesed frames: \n - %d good frames, \n - %d bad frames.\n", cvsd_plc_state.good_frames_nr, cvsd_plc_state.bad_frames_nr);
}

static const codec_support_t recieve_cvsd = {
        .init         = &cvsd_init,
        .receive      = &cvsd_receive,
        .fill_payload = &cvsd_fill_payload,
        .close        = &cvsd_close,
        .sample_rate = SAMPLE_RATE_8KHZ
};

static const codec_support_t playback_cvsd = {
        .init         = &cvsd_init,
        .receive      = &cvsd_receive_null,
        .fill_payload = &cvsd_fill_payload,
        .close        = &cvsd_close,
        .sample_rate = SAMPLE_RATE_8KHZ
};

void sco_init(void)
{
#ifdef ENABLE_CLASSIC_LEGACY_CONNECTIONS_FOR_SCO_DEMOS
    printf("Disable BR/EDR Secure Connections due to incompatibilities with SCO connections\n");
    gap_secure_connections_enable(false);
#endif
    hci_set_sco_voice_setting(0x60);
}

void sco_set_codec(uint8_t negotiated_codec, bool con_mode)
{
    switch (con_mode)
    {
    case PLAYBACK:
        codec_current = &playback_cvsd;
        break;
    case RECORDING:
        codec_current = &recieve_cvsd;
        audio_initialize(codec_current->sample_rate);
    default:
        break;
    }
    codec_current->init();

    // Will probably change this
    audio_prebuffer_bytes = SCO_PREBUFFER_MS * (codec_current->sample_rate/1000) * BYTES_PER_FRAME;
}

void sco_receive(uint8_t * packet, uint16_t size){

    int stat = encode_and_write();
    if(stat == FRAME_BUFFER_FULL) return;

    codec_current->receive(packet, size);
    count_sent++;
    if ((count_sent % SCO_REPORT_PERIOD) == 0) {
        printf("\n%d\n",sec);
    sec++;
    }
}

void sco_send(hci_con_handle_t sco_handle){

    if (sco_handle == HCI_CON_HANDLE_INVALID) return;

    int stat = read_and_decode();
    if(stat == FRAME_BUFFER_FULL) return;

    int sco_packet_length = hci_get_sco_packet_length();
    int sco_payload_length = sco_packet_length - 3;
    uint8_t * sco_packet = hci_get_outgoing_packet_buffer(); // получаем указатель на передаваемый sco пакет
    hci_reserve_packet_buffer(); // подготовка к отправке пакета

    if (audio_input_paused){
        if (btstack_ring_buffer_bytes_available(&decode_ring_buffer) >= audio_prebuffer_bytes){
            // resume sending
            audio_input_paused = 0;
        }
    }

    // fill payload by codec
    codec_current->fill_payload(&sco_packet[3], sco_payload_length);

    // set handle + flags
    little_endian_store_16(sco_packet, 0, sco_handle);
    // set len
    sco_packet[2] = sco_payload_length;
    // finally send packet
    hci_send_sco_packet_buffer(sco_packet_length);

    // request another send event
    hci_request_sco_can_send_now_event();

}

void sco_close(void){
    sec = 0;
    printf("SCO demo close\n");
    printf("SCO demo statistics: ");
    codec_current->close();
    codec_current = NULL;
}