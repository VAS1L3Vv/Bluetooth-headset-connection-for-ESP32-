#define BTSTACK_FILE__ "sco_util.c"

#include <stdio.h>
#include "audio_control.h"
#include "btstack_debug.h"
#include "btstack_ring_buffer.h"
#include "classic/btstack_cvsd_plc.h"
#include "classic/btstack_sbc.h"
#include "classic/hfp.h"
#include "bt_audio_handler.h"

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

// output
static int                   audio_output_paused  = 0;
static int                   audio_input_paused   = 0;
static uint8_t               audio_ring_buffer_storage[MAX_BUFFER_SIZE_BYTES]; // указатель памяти кольцевого буффера
static btstack_ring_buffer_t audio_ring_buffer;
// input
#define USE_AUDIO_INPUT
static int count_sent = 0;
static int count_received = 0;
static btstack_cvsd_plc_state_t cvsd_plc_state;

int num_samples_to_write;
int num_audio_frames;

// generic codec support
typedef struct {
    void (*init)(void);
    void(*receive)(const uint8_t * packet, uint16_t size);
    void (*fill_payload)(uint8_t * payload_buffer, uint16_t sco_payload_length);
    void (*close)(void);
    uint16_t sample_rate;
} codec_support_t;

static audio_struct_t audio_data = {
        .mic_buff = NULL,
        .spkr_buff = NULL,
        .buf_size = 0,
        .buf_length = 0,
        .buf_time = 0,
        .rb_size = 0,
        .rb_length = 0,
        .rb_time = 0,
        .rb_coef = 1.5,
        .codec2_enabled = OFF,
        .record_cycle_num = 0,
        .playback_cycle_num = 0,
        .sco_conn_state = LISTENING, // при инициализации стоит в режиме "прослушивания" пакетов
    }; // изначальные данные

// current configuration
static const codec_support_t * codec_current = NULL;
// static const audio_struct_t * audio_handle = get_audio_handle();

static bool is_codec2_on = OFF;

bool codec2_enabled()
{
    return is_codec2_on;
}

void set_codec2_state(bool on_off)
{
    is_codec2_on = on_off;
}

// return 1 if ok
static int audio_initialize(int sample_rate) {
    memset(audio_ring_buffer_storage, 0, sizeof(audio_ring_buffer_storage));
    btstack_ring_buffer_init(&audio_ring_buffer, audio_ring_buffer_storage, sizeof(audio_ring_buffer_storage));
    audio_output_paused  = 1;
    return 1;
}

// CVSD - 8 kHz
static void cvsd_init(void) { 
    printf("SCO Demo: Init CVSD\n");
    btstack_cvsd_plc_init(&cvsd_plc_state);
}

static void cvsd_receive(const uint8_t * packet, uint16_t size) {
    int16_t audio_frame_out[128];
    if (size > sizeof(audio_frame_out)){
        printf("cvsd_receive: SCO packet larger than local output buffer - dropping data.\n");
        return;
    }

    const int audio_bytes_read = size - 3;
    const int num_samples = audio_bytes_read / BYTES_PER_FRAME;

    // convert into host endian
    int16_t audio_frame_in[128];
    int i;
    for (i=0;i<num_samples;i++){
        audio_frame_in[i] = little_endian_read_16(packet, 3 + i * 2);
    }

    // treat packet as bad frame if controller does not report 'all good'
    bool bad_frame = (packet[1] & 0x30) != 0;
    btstack_cvsd_plc_process_data(&cvsd_plc_state, bad_frame, audio_frame_in, num_samples, audio_frame_out);
    btstack_ring_buffer_write(&audio_ring_buffer, (uint8_t *)audio_frame_out, audio_bytes_read);
}

static void cvsd_fill_payload(uint8_t * payload_buffer, uint16_t sco_payload_length) {
    uint16_t bytes_to_copy = sco_payload_length;
    // get data from ringbuffer
    uint16_t pos = 0;
    if (!audio_input_paused){
        uint16_t samples_to_copy = sco_payload_length / 2;
        uint32_t bytes_read = 0;
        btstack_ring_buffer_read(&audio_ring_buffer, payload_buffer, bytes_to_copy, &bytes_read);
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

static void cvsd_close(void) { 
    printf("Used CVSD with PLC, number of proccesed frames: \n - %d good frames, \n - %d bad frames.\n", cvsd_plc_state.good_frames_nr, cvsd_plc_state.bad_frames_nr);
}

static const codec_support_t codec_cvsd = {
        .init         = &cvsd_init,
        .receive      = &cvsd_receive,
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
    // Set SCO for CVSD (mSBC or other codecs automatically use 8-bit transparent mode)
    hci_set_sco_voice_setting(0x60);    // linear, unsigned, 16-bit, CVSD
}

void sco_set_codec(uint8_t negotiated_codec)
{
    switch (negotiated_codec){
        case HFP_CODEC_CVSD:
            codec_current = &codec_cvsd;
            break;
        default:
            printf("SCO_UTIL ERROR: CSVD CODEC NOT SET.");
            btstack_assert(false);
            break;
    }
    codec_current->init();
    audio_initialize(codec_current->sample_rate);
    audio_prebuffer_bytes = SCO_PREBUFFER_MS * (codec_current->sample_rate/1000) * BYTES_PER_FRAME;
}

void sco_receive(uint8_t * packet, uint16_t size){
    static uint32_t packets = 0;
    static uint32_t crc_errors = 0;
    static uint32_t data_received = 0;
    static uint32_t byte_errors = 0;

    count_received++;

    data_received += size - 3;
    packets++;
    if (data_received > 100000){
        printf("Summary: data %07u, packets %04u, packet with crc errors %0u, byte errors %04u\n",  (unsigned int) data_received,  (unsigned int) packets, (unsigned int) crc_errors, (unsigned int) byte_errors);
        crc_errors = 0;
        byte_errors = 0;
        data_received = 0;
        packets = 0;
    }

    codec_current->receive(packet, size);
}

static uint8_t sec = 0;

void sco_send(hci_con_handle_t sco_handle){

    if (sco_handle == HCI_CON_HANDLE_INVALID) return;

    int sco_packet_length = hci_get_sco_packet_length();
    int sco_payload_length = sco_packet_length - 3;
    // printf("sco payload length: %d \n\n", sco_payload_length);
    hci_reserve_packet_buffer(); // подготовка к отправке пакета
    uint8_t * sco_packet = hci_get_outgoing_packet_buffer(); // получаем указатель на передаваемый sco пакет

    // printf("sco payload size: %d", );
    // resume if pre-buffer is filled
    if (audio_input_paused){
        if (btstack_ring_buffer_bytes_available(&audio_ring_buffer) >= audio_prebuffer_bytes){
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

    
    count_sent++;
    if ((count_sent % SCO_REPORT_PERIOD) == 0) {
        // printf("SCO: sent %u, received %u\n", count_sent, count_received);
        printf("\n%d\n",sec);
    sec++;
    }
    if(sec == 10)
    sec = 0;
}

void sco_close(void){
    sec = 0;
    printf("SCO demo close\n");
    printf("SCO demo statistics: ");
    codec_current->close();
    codec_current = NULL;
}