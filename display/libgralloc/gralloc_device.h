#ifndef GRALLOC_DEVICE_H
#define GRALLOC_DEVICE_H

#include <hardware/gralloc1.h>
#include <utils/Mutex.h>
#include <map>

using namespace android;

class GrallocDevice {
public:
    static int HookDevOpen(const struct hw_module_t* module, const char* name,
                          struct hw_device_t** device);

    GrallocDevice();
    ~GrallocDevice();

    // Gralloc1 function hooks
    static gralloc1_error_t CreateBuffer(gralloc1_device_t* device,
                                       gralloc1_buffer_descriptor_t descriptor,
                                       buffer_handle_t* outBuffer);

    static gralloc1_error_t ReleaseBuffer(gralloc1_device_t* device,
                                        buffer_handle_t buffer);

    static gralloc1_error_t GetBufferProperties(gralloc1_device_t* device,
                                              buffer_handle_t buffer,
                                              uint32_t* outWidth,
                                              uint32_t* outHeight,
                                              uint32_t* outFormat,
                                              uint64_t* outProducerUsage,
                                              uint64_t* outConsumerUsage);

private:
    struct BufferDescriptor {
        uint32_t width;
        uint32_t height;
        uint32_t format;
        uint64_t producerUsage;
        uint64_t consumerUsage;
    };

    // Buffer management
    Mutex mBufferLock;
    std::map<buffer_handle_t, BufferDescriptor> mBuffers;

    // Device capabilities
    static constexpr uint32_t MAX_BUFFER_WIDTH = 4096;
    static constexpr uint32_t MAX_BUFFER_HEIGHT = 4096;
    static constexpr uint64_t SUPPORTED_USAGE = GRALLOC1_CONSUMER_USAGE_HWCOMPOSER |
                                               GRALLOC1_CONSUMER_USAGE_CPU_READ |
                                               GRALLOC1_PRODUCER_USAGE_CPU_WRITE |
                                               GRALLOC1_PRODUCER_USAGE_GPU_RENDER_TARGET;
};

#endif // GRALLOC_DEVICE_H 