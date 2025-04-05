#ifndef AUDIO_HW_H
#define AUDIO_HW_H

#include <hardware/audio.h>
#include <hardware/hardware.h>
#include <system/audio.h>
#include <utils/Mutex.h>
#include <tinyalsa/asoundlib.h>

using namespace android;

class AudioHAL {
public:
    static int CreateInstance(const struct hw_module_t* module,
                            struct audio_hw_device** device);

    AudioHAL();
    ~AudioHAL();

    // Device operations
    int OpenOutputStream(audio_io_handle_t handle,
                        audio_devices_t devices,
                        audio_config_t* config,
                        struct audio_stream_out** stream_out);

    int OpenInputStream(audio_io_handle_t handle,
                       audio_devices_t devices,
                       audio_config_t* config,
                       struct audio_stream_in** stream_in);

    int CloseOutputStream(struct audio_stream_out* stream);
    int CloseInputStream(struct audio_stream_in* stream);

    // Stream operations
    static uint32_t GetSampleRate(const struct audio_stream* stream);
    static int SetSampleRate(struct audio_stream* stream, uint32_t rate);
    static size_t GetBufferSize(const struct audio_stream* stream);
    static uint32_t GetChannelMask(const struct audio_stream* stream);
    static audio_format_t GetFormat(const struct audio_stream* stream);
    static int SetFormat(struct audio_stream* stream, audio_format_t format);
    static int Standby(struct audio_stream* stream);
    static int SetParameters(struct audio_stream* stream, const char* kvpairs);
    static char* GetParameters(const struct audio_stream* stream, const char* keys);
    static int AddAudioEffect(const struct audio_stream* stream, effect_handle_t effect);
    static int RemoveAudioEffect(const struct audio_stream* stream, effect_handle_t effect);

private:
    struct StreamConfig {
        uint32_t sampleRate;
        audio_channel_mask_t channelMask;
        audio_format_t format;
        size_t bufferSize;
    };

    struct Stream {
        struct audio_stream_out stream;
        AudioHAL* hal;
        StreamConfig config;
        struct pcm* pcm;
        bool standby;
    };

    // Device state
    Mutex mLock;
    bool mInitialized;
    audio_devices_t mOutDevice;
    audio_devices_t mInDevice;
    Stream* mOutputStream;
    Stream* mInputStream;

    // ALSA configuration
    static constexpr unsigned int CARD = 0;
    static constexpr unsigned int DEVICE = 0;
    static constexpr unsigned int PERIOD_SIZE = 1024;
    static constexpr unsigned int PERIOD_COUNT = 4;

    // Device capabilities
    static constexpr uint32_t SUPPORTED_SAMPLE_RATES[] = {
        44100, 48000, 96000, 192000
    };
    static constexpr audio_channel_mask_t SUPPORTED_CHANNEL_MASKS =
        AUDIO_CHANNEL_OUT_STEREO | AUDIO_CHANNEL_OUT_5POINT1;
    static constexpr audio_format_t SUPPORTED_FORMATS =
        AUDIO_FORMAT_PCM_16_BIT | AUDIO_FORMAT_PCM_24_BIT;

    // Helper functions
    int InitializeALSA();
    void DeinitializeALSA();
    int ConfigureALSADevice(const StreamConfig* config);
};

#endif // AUDIO_HW_H 