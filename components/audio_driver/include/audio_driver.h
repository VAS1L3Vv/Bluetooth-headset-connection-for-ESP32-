
#ifndef AUDIO_DRIVER_H
#define AUDIO_DRIVER_H

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

typedef struct {

    /**
     * @brief Setup audio codec for specified samplerate and number of channels
     * @param Channels (1=mono, 2=stereo)
     * @param Sample rate
     * @param Playback callback
     * @return 1 on success
     */
    int (*init)(uint8_t channels,
                uint32_t samplerate, 
                void (*playback) (int16_t * buffer, uint16_t num_samples));

    /**
     * @brief Get the current playback sample rate, may differ from the
     *        specified sample rate
     */
    uint32_t (*get_samplerate)(void);

    /**
     * @brief Set volume
     * @param Volume 0..127
     */
    void (*set_volume)(uint8_t volume);

    /**
     * @brief Start stream
     */
    void (*start_stream)(void);

    /** 
     * @brief Stop stream
     */
    void (*stop_stream)(void);

    /**
     * @brief Close audio codec
     */
    void (*close)(void);

} btstack_audio_sink_t;


typedef struct {

    /**
     * @brief Setup audio codec for specified samplerate and number of channels
     * @param Channels (1=mono, 2=stereo)
     * @param Sample rate
     * @param Recording callback
     * @return 1 on success
     */
    int (*init)(uint8_t channels,
                uint32_t samplerate, 
                void (*recording)(const int16_t * buffer, uint16_t num_samples));

    /**
     * @brief Get the current recording sample rate, may differ from the
     *        specified sameple rate
     */
    uint32_t (*get_samplerate)(void);

    /**
     * @brief Set Gain
     * @param Gain 0..127
     */
    void (*set_gain)(uint8_t gain);

    /**
     * @brief Start stream
     */
    void (*start_stream)(void);

    /** 
     * @brief Stop stream
     */
    void (*stop_stream)(void);

    /**
     * @brief Close audio codec
     */
    void (*close)(void);

} btstack_audio_source_t;

const btstack_audio_sink_t *    btstack_audio_esp32_sink_get_instance(void);
const btstack_audio_source_t *  btstack_audio_esp32_source_get_instance(void);

#if defined __cplusplus
}
#endif

#endif
