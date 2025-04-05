#define LOG_TAG "gralloc_sm8650"

#include <log/log.h>
#include <errno.h>
#include <string.h>
#include "gralloc_device.h"

GrallocDevice::GrallocDevice() {
}

GrallocDevice::~GrallocDevice() {
    // Clean up any remaining buffers
    std::lock_guard<Mutex> lock(mBufferLock);
    mBuffers.clear();
}

int GrallocDevice::HookDevOpen(const struct hw_module_t* module, const char* name,
                              struct hw_device_t** device) {
    if (strcmp(name, GRALLOC_HARDWARE_MODULE_ID) != 0) {
        ALOGE("Invalid device name: %s", name);
        return -EINVAL;
    }

    GrallocDevice* dev = new GrallocDevice();
    if (!dev) {
        ALOGE("Failed to allocate GrallocDevice");
        return -ENOMEM;
    }

    gralloc1_device_t* gralloc1_dev = new gralloc1_device_t();
    if (!gralloc1_dev) {
        delete dev;
        return -ENOMEM;
    }

    gralloc1_dev->common.tag = HARDWARE_DEVICE_TAG;
    gralloc1_dev->common.version = GRALLOC1_DEVICE_API_VERSION_1_0;
    gralloc1_dev->common.module = const_cast<hw_module_t*>(module);
    gralloc1_dev->common.close = nullptr;  // Will be set by framework

    // Set function hooks
    gralloc1_dev->createBuffer = CreateBuffer;
    gralloc1_dev->releaseBuffer = ReleaseBuffer;
    gralloc1_dev->getBufferProperties = GetBufferProperties;

    *device = &gralloc1_dev->common;
    return 0;
}

gralloc1_error_t GrallocDevice::CreateBuffer(gralloc1_device_t* device,
                                           gralloc1_buffer_descriptor_t descriptor,
                                           buffer_handle_t* outBuffer) {
    GrallocDevice* dev = static_cast<GrallocDevice*>(device);
    
    // Validate descriptor
    BufferDescriptor desc;
    if (desc.width > MAX_BUFFER_WIDTH || desc.height > MAX_BUFFER_HEIGHT) {
        ALOGE("Buffer dimensions exceed maximum supported size");
        return GRALLOC1_ERROR_BAD_VALUE;
    }

    // Validate usage flags
    if ((desc.producerUsage | desc.consumerUsage) & ~SUPPORTED_USAGE) {
        ALOGE("Unsupported buffer usage flags requested");
        return GRALLOC1_ERROR_BAD_VALUE;
    }

    // Allocate buffer handle (simplified implementation)
    native_handle_t* handle = native_handle_create(1, 3); // 1 fd, 3 ints
    if (!handle) {
        ALOGE("Failed to create buffer handle");
        return GRALLOC1_ERROR_NO_RESOURCES;
    }

    // Store buffer information
    {
        std::lock_guard<Mutex> lock(dev->mBufferLock);
        dev->mBuffers[handle] = desc;
    }

    *outBuffer = handle;
    return GRALLOC1_ERROR_NONE;
}

gralloc1_error_t GrallocDevice::ReleaseBuffer(gralloc1_device_t* device,
                                            buffer_handle_t buffer) {
    GrallocDevice* dev = static_cast<GrallocDevice*>(device);
    
    if (!buffer) {
        return GRALLOC1_ERROR_BAD_HANDLE;
    }

    {
        std::lock_guard<Mutex> lock(dev->mBufferLock);
        auto it = dev->mBuffers.find(buffer);
        if (it == dev->mBuffers.end()) {
            return GRALLOC1_ERROR_BAD_HANDLE;
        }
        dev->mBuffers.erase(it);
    }

    native_handle_delete(const_cast<native_handle_t*>(buffer));
    return GRALLOC1_ERROR_NONE;
}

gralloc1_error_t GrallocDevice::GetBufferProperties(gralloc1_device_t* device,
                                                  buffer_handle_t buffer,
                                                  uint32_t* outWidth,
                                                  uint32_t* outHeight,
                                                  uint32_t* outFormat,
                                                  uint64_t* outProducerUsage,
                                                  uint64_t* outConsumerUsage) {
    GrallocDevice* dev = static_cast<GrallocDevice*>(device);
    
    if (!buffer) {
        return GRALLOC1_ERROR_BAD_HANDLE;
    }

    std::lock_guard<Mutex> lock(dev->mBufferLock);
    auto it = dev->mBuffers.find(buffer);
    if (it == dev->mBuffers.end()) {
        return GRALLOC1_ERROR_BAD_HANDLE;
    }

    const BufferDescriptor& desc = it->second;
    if (outWidth) *outWidth = desc.width;
    if (outHeight) *outHeight = desc.height;
    if (outFormat) *outFormat = desc.format;
    if (outProducerUsage) *outProducerUsage = desc.producerUsage;
    if (outConsumerUsage) *outConsumerUsage = desc.consumerUsage;

    return GRALLOC1_ERROR_NONE;
} 