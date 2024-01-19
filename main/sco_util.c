#define BTSTACK_FILE__ "sco_util.c"

#include <stdio.h>
#include "sco_util.h"
#include "btstack_audio.h"
#include "btstack_debug.h"
#include "btstack_ring_buffer.h"
#include "classic/btstack_cvsd_plc.h"
#include "classic/btstack_sbc.h"
#include "classic/hfp.h"

#ifdef _MSC_VER
// ignore deprecated warning for fopen
#pragma warning(disable : 4996)
#endif

// number of sco packets until 'report' on console
#define SCO_REPORT_PERIOD           100

// constants
#define NUM_CHANNELS            1
#define SAMPLE_RATE_8KHZ        8000
#define BYTES_PER_FRAME         2
#define REFILL_SAMPLES          16 // проверка и запись в буффер передачи по 16 элементов

// audio pre-buffer - also defines latency
#define SCO_PREBUFFER_MS      50
#define PREBUFFER_BYTES_8KHZ  (SCO_PREBUFFER_MS *  SAMPLE_RATE_8KHZ/1000 * BYTES_PER_FRAME)

#define PREBUFFER_BYTES_MAX PREBUFFER_BYTES_8KHZ
#define SAMPLES_PER_FRAME_MAX 60

static uint16_t              audio_prebuffer_bytes;

// output
static int                   audio_output_paused  = 0;
static uint8_t               audio_output_ring_buffer_storage[2 * PREBUFFER_BYTES_MAX];
static btstack_ring_buffer_t audio_output_ring_buffer;

// input

#define USE_AUDIO_INPUT

static void (*add_speech_to_buffer)(uint16_t num_samples, int16_t * data);

static int                   audio_input_paused  = 0;
static uint8_t               audio_input_ring_buffer_storage[2 * PREBUFFER_BYTES_MAX];
static btstack_ring_buffer_t audio_input_ring_buffer;

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
    //
    uint16_t sample_rate;
} codec_support_t;

// current configuration
static const codec_support_t * codec_current = NULL;

// Sine Wave
static uint16_t array_position;
#define SPEECH_SAMPLE_RATE SAMPLE_RATE_8KHZ
#define CODEC2_SPEECH_BUFFER_SIZE 320

// input signal: pre-computed int16 sine wave, 32000 Hz at 266 Hz
static int16_t speech_buffer[CODEC2_SPEECH_BUFFER_SIZE];

#ifdef USE_AUDIO_INPUT
static void audio_recording_callback(const int16_t * buffer, uint16_t num_samples)
{
    btstack_ring_buffer_write(&audio_input_ring_buffer, (uint8_t *)buffer, num_samples * 2);
}
#endif

static void buffer_to_host_endian(uint16_t num_samples, int16_t * data){
    unsigned int i;
    for (i=0; i < num_samples; i++){
        data[i] = speech_buffer[array_position];
        array_position++;
        if (array_position >= (sizeof(speech_buffer) / sizeof(int16_t))){
            array_position = 0;
        }
    }
}

// Audio Playback / Recording
static void audio_playback_callback(int16_t * buffer, uint16_t num_samples){

    // fill with silence while paused
    if (audio_output_paused){
        if (btstack_ring_buffer_bytes_available(&audio_output_ring_buffer) < audio_prebuffer_bytes){
            memset(buffer, 0, num_samples * BYTES_PER_FRAME);
           return;
        } else {
            // resume playback
            audio_output_paused = 0;
        }
    }

    // get data from ringbuffer
    uint32_t bytes_read = 0;
    btstack_ring_buffer_read(&audio_output_ring_buffer, (uint8_t *) buffer, num_samples * BYTES_PER_FRAME, &bytes_read);
    num_samples -= bytes_read / BYTES_PER_FRAME;
    buffer      += bytes_read / BYTES_PER_FRAME;

    // fill with 0 if not enough
    if (num_samples)
    {
        memset(buffer, 0, num_samples * BYTES_PER_FRAME);
        audio_output_paused = 1;
    }
}

// return 1 if ok
static int audio_initialize(int sample_rate){

    // -- output -- //

    // init buffers
    memset(audio_output_ring_buffer_storage, 0, sizeof(audio_output_ring_buffer_storage));
    btstack_ring_buffer_init(&audio_output_ring_buffer, audio_output_ring_buffer_storage, sizeof(audio_output_ring_buffer_storage));

    // config and setup audio playback
    const btstack_audio_sink_t * audio_sink = btstack_audio_sink_get_instance();
    if (audio_sink != NULL){
        audio_sink->init(1, sample_rate, &audio_playback_callback);
        audio_sink->start_stream();

        audio_output_paused  = 1;
    }

    // -- input -- //

    // init buffers
    memset(audio_input_ring_buffer_storage, 0, sizeof(audio_input_ring_buffer_storage));
    btstack_ring_buffer_init(&audio_input_ring_buffer, audio_input_ring_buffer_storage, sizeof(audio_input_ring_buffer_storage));

#ifdef USE_AUDIO_INPUT
    // config and setup audio recording
    const btstack_audio_source_t * audio_source = btstack_audio_source_get_instance();
    if (audio_source != NULL){
        audio_source->init(1, sample_rate, &audio_recording_callback);
        audio_source->start_stream();

        audio_input_paused = 1;
    }
#endif

    return 1;
}

static void audio_terminate(void)
{
    const btstack_audio_sink_t * audio_sink = btstack_audio_sink_get_instance();
    if (!audio_sink) return;
    audio_sink->close();

#ifdef USE_AUDIO_INPUT
    const btstack_audio_source_t * audio_source= btstack_audio_source_get_instance();
    if (!audio_source) return;
    audio_source->close();
#endif
}

// CVSD - 8 kHz
static void cvsd_init(void)
{
    printf("SCO Demo: Init CVSD\n");
    btstack_cvsd_plc_init(&cvsd_plc_state);
}

static void cvsd_receive(const uint8_t * packet, uint16_t size)
{

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
    btstack_ring_buffer_write(&audio_output_ring_buffer, (uint8_t *)audio_frame_out, audio_bytes_read);
}

static void cvsd_fill_payload(uint8_t * payload_buffer, uint16_t sco_payload_length)
{
    uint16_t bytes_to_copy = sco_payload_length;

    // get data from ringbuffer
    uint16_t pos = 0;
    if (!audio_input_paused){
        uint16_t samples_to_copy = sco_payload_length / 2;
        uint32_t bytes_read = 0;
        btstack_ring_buffer_read(&audio_input_ring_buffer, payload_buffer, bytes_to_copy, &bytes_read);
        // flip 16 on big endian systems
        // @note We don't use (uint16_t *) casts since all sample addresses are odd which causes crahses on some systems
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

static void cvsd_close(void)
{ 
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

    add_speech_to_buffer = &buffer_to_host_endian;
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

void sco_send(hci_con_handle_t sco_handle){

    if (sco_handle == HCI_CON_HANDLE_INVALID) return;

    int sco_packet_length = hci_get_sco_packet_length();
    int sco_payload_length = sco_packet_length - 3;

    hci_reserve_packet_buffer(); // подготовка к отправке пакета
    uint8_t * sco_packet = hci_get_outgoing_packet_buffer(); // получаем указатель на передаваемый sco пакет

    // re-fill audio buffer
    uint16_t samples_free = btstack_ring_buffer_bytes_free(&audio_input_ring_buffer) / 2; // получения свободных в буфере байт
   
    while (samples_free > 0){
        int16_t samples_buffer[REFILL_SAMPLES]; 
        uint16_t samples_to_add = btstack_min(samples_free, REFILL_SAMPLES);
        (*add_speech_to_buffer)(samples_to_add, samples_buffer); // записываем в каждой итерации на свободное количесво байт звуковые данные
        btstack_ring_buffer_write(&audio_input_ring_buffer, (uint8_t *)samples_buffer, samples_to_add * 2);
        samples_free -= samples_to_add; // пополняет буффер на сколько это возможно
    }

    // resume if pre-buffer is filled
    if (audio_input_paused){
        if (btstack_ring_buffer_bytes_available(&audio_input_ring_buffer) >= audio_prebuffer_bytes){
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
        printf("SCO: sent %u, received %u\n", count_sent, count_received);
    }
}

void sco_close(void){
    printf("SCO demo close\n");

    printf("SCO demo statistics: ");

    codec_current->close();
    codec_current = NULL;

    audio_terminate();
}