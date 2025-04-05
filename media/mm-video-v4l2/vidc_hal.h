#ifndef __VIDC_HAL_H__
#define __VIDC_HAL_H__

#include <linux/videodev2.h>
#include <media/hardware/VideoAPI.h>
#include <media/hardware/HardwareAPI.h>
#include <utils/Mutex.h>
#include <vector>

using namespace android;

class VidecHAL {
public:
    static int CreateInstance(const struct hw_module_t* module,
                            void* device);
    
    VidecHAL();
    ~VidecHAL();

    // Video codec operations
    int OpenCodec(video_codec_type_t codec_type);
    int CloseCodec();
    int ConfigureCodec(const video_config_t* config);
    int StartCodec();
    int StopCodec();
    int QueueBuffer(const video_buffer_t* buffer);
    int DequeueBuffer(video_buffer_t* buffer);

private:
    struct CodecState {
        bool isOpen;
        bool isConfigured;
        bool isRunning;
        video_codec_type_t codecType;
        video_config_t currentConfig;
    };

    // Device state
    Mutex mLock;
    CodecState mState;
    int mDeviceFd;

    // Buffer management
    std::vector<video_buffer_t> mInputBuffers;
    std::vector<video_buffer_t> mOutputBuffers;

    // V4L2 specific functions
    int SetupV4L2Device();
    int ConfigureV4L2Format(const video_config_t* config);
    int AllocateV4L2Buffers();
    int FreeV4L2Buffers();

    // Device capabilities
    static constexpr int MAX_INPUT_BUFFERS = 32;
    static constexpr int MAX_OUTPUT_BUFFERS = 32;
    static constexpr int MAX_WIDTH = 4096;
    static constexpr int MAX_HEIGHT = 2160;

    // Supported codecs
    static constexpr uint32_t SUPPORTED_CODECS =
        (1 << VIDEO_CODEC_H264) |
        (1 << VIDEO_CODEC_H265) |
        (1 << VIDEO_CODEC_VP8) |
        (1 << VIDEO_CODEC_VP9);
};

#endif // __VIDC_HAL_H__ 