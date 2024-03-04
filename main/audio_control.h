#ifndef SCO_DEMO_UTIL_H
#define SCO_DEMO_UTIL_H

#include "hci.h"

#if defined __cplusplus
extern "C" {
#endif

#define NUM_CHANNELS            1
#define SAMPLE_RATE_8KHZ        8000
#define BYTES_PER_FRAME         2
#define ON                      1
#define OFF                     0
#define AUDIO_SECONDS           10
#define BYTES_PER_SAMPLE        2
#define SECONDS_TO_BYTES AUDIO_SECONDS*BYTES_PER_SAMPLE*SAMPLE_RATE_8KHZ
#define CODEC2_FRAME_SIZE       320
#define SCO_PACKET_SIZE         120
#define FRAME_SIZE CODEC2_FRAME_SIZE
#define FRAME_LENGTH FRAME_SIZE/BYTES_PER_SAMPLE
#define NUMBER_OF_FRAMES        10
#define MAX_BUFFER_SIZE_BYTES FRAME_SIZE*NUMBER_OF_FRAMES
#define LISTENING               0 
#define RECORDING               1

bool codec2_enabled();
void set_codec2_state(bool on_off);

/**
 * @brief Init demo SCO data production/consumtion
 */
void sco_init(void);

/**
 * @brief Set codec (cvsd:0x01, msbc:0x02) and initalize wav writter and portaudio .
 * @param codec
 */
 void sco_set_codec(uint8_t codec);

/**
 * @brief Send next data on con_handle
 */
void sco_send(hci_con_handle_t con_handle);

/**
 * @brief Process received data
 */
void sco_receive(uint8_t * packet, uint16_t size);

/**
 * @brief Close WAV writer, stop portaudio stream
 */
void sco_close(void);

#if defined __cplusplus
}
#endif

#endif
