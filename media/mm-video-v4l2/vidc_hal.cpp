#define LOG_TAG "vidc_hal"

#include <log/log.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "vidc_hal.h"

VidecHAL::VidecHAL()
    : mDeviceFd(-1) {
    memset(&mState, 0, sizeof(mState));
}

VidecHAL::~VidecHAL() {
    if (mDeviceFd >= 0) {
        CloseCodec();
    }
}

int VidecHAL::CreateInstance(const struct hw_module_t* module,
                           void* device) {
    VidecHAL* hal = new VidecHAL();
    if (!hal) {
        ALOGE("Failed to allocate VidecHAL");
        return -ENOMEM;
    }

    video_device_t* video_dev = static_cast<video_device_t*>(device);
    video_dev->common.tag = HARDWARE_DEVICE_TAG;
    video_dev->common.version = VIDEO_DEVICE_API_VERSION_1_0;
    video_dev->common.module = const_cast<hw_module_t*>(module);
    video_dev->common.close = nullptr;  // Will be set by framework
    video_dev->device = hal;

    return 0;
}

int VidecHAL::OpenCodec(video_codec_type_t codec_type) {
    std::lock_guard<Mutex> lock(mLock);

    if (mState.isOpen) {
        ALOGE("Codec already open");
        return -EINVAL;
    }

    // Check if codec is supported
    if (!(SUPPORTED_CODECS & (1 << codec_type))) {
        ALOGE("Unsupported codec type: %d", codec_type);
        return -EINVAL;
    }

    int ret = SetupV4L2Device();
    if (ret != 0) {
        return ret;
    }

    mState.isOpen = true;
    mState.codecType = codec_type;
    return 0;
}

int VidecHAL::CloseCodec() {
    std::lock_guard<Mutex> lock(mLock);

    if (!mState.isOpen) {
        return 0;
    }

    if (mState.isRunning) {
        StopCodec();
    }

    FreeV4L2Buffers();

    if (mDeviceFd >= 0) {
        close(mDeviceFd);
        mDeviceFd = -1;
    }

    memset(&mState, 0, sizeof(mState));
    return 0;
}

int VidecHAL::ConfigureCodec(const video_config_t* config) {
    std::lock_guard<Mutex> lock(mLock);

    if (!mState.isOpen) {
        ALOGE("Codec not open");
        return -EINVAL;
    }

    if (mState.isConfigured) {
        ALOGE("Codec already configured");
        return -EINVAL;
    }

    // Validate configuration
    if (config->width > MAX_WIDTH || config->height > MAX_HEIGHT) {
        ALOGE("Resolution not supported: %dx%d", config->width, config->height);
        return -EINVAL;
    }

    int ret = ConfigureV4L2Format(config);
    if (ret != 0) {
        return ret;
    }

    ret = AllocateV4L2Buffers();
    if (ret != 0) {
        return ret;
    }

    mState.isConfigured = true;
    mState.currentConfig = *config;
    return 0;
}

int VidecHAL::StartCodec() {
    std::lock_guard<Mutex> lock(mLock);

    if (!mState.isConfigured) {
        ALOGE("Codec not configured");
        return -EINVAL;
    }

    if (mState.isRunning) {
        return 0;
    }

    // Start V4L2 streaming
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    if (ioctl(mDeviceFd, VIDIOC_STREAMON, &type) < 0) {
        ALOGE("Failed to start output streaming: %s", strerror(errno));
        return -errno;
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (ioctl(mDeviceFd, VIDIOC_STREAMON, &type) < 0) {
        ALOGE("Failed to start capture streaming: %s", strerror(errno));
        return -errno;
    }

    mState.isRunning = true;
    return 0;
}

int VidecHAL::StopCodec() {
    std::lock_guard<Mutex> lock(mLock);

    if (!mState.isRunning) {
        return 0;
    }

    // Stop V4L2 streaming
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    if (ioctl(mDeviceFd, VIDIOC_STREAMOFF, &type) < 0) {
        ALOGE("Failed to stop output streaming: %s", strerror(errno));
        return -errno;
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (ioctl(mDeviceFd, VIDIOC_STREAMOFF, &type) < 0) {
        ALOGE("Failed to stop capture streaming: %s", strerror(errno));
        return -errno;
    }

    mState.isRunning = false;
    return 0;
}

int VidecHAL::QueueBuffer(const video_buffer_t* buffer) {
    std::lock_guard<Mutex> lock(mLock);

    if (!mState.isRunning) {
        ALOGE("Codec not running");
        return -EINVAL;
    }

    // Queue buffer to V4L2
    v4l2_buffer buf = {};
    buf.type = buffer->type == VIDEO_BUFFER_TYPE_INPUT ?
               V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE :
               V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buf.memory = V4L2_MEMORY_DMABUF;
    buf.index = buffer->index;

    if (ioctl(mDeviceFd, VIDIOC_QBUF, &buf) < 0) {
        ALOGE("Failed to queue buffer: %s", strerror(errno));
        return -errno;
    }

    return 0;
}

int VidecHAL::DequeueBuffer(video_buffer_t* buffer) {
    std::lock_guard<Mutex> lock(mLock);

    if (!mState.isRunning) {
        ALOGE("Codec not running");
        return -EINVAL;
    }

    // Dequeue buffer from V4L2
    v4l2_buffer buf = {};
    buf.type = buffer->type == VIDEO_BUFFER_TYPE_INPUT ?
               V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE :
               V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buf.memory = V4L2_MEMORY_DMABUF;

    if (ioctl(mDeviceFd, VIDIOC_DQBUF, &buf) < 0) {
        if (errno == EAGAIN) {
            return -EAGAIN;
        }
        ALOGE("Failed to dequeue buffer: %s", strerror(errno));
        return -errno;
    }

    buffer->index = buf.index;
    buffer->bytesused = buf.bytesused;
    buffer->flags = buf.flags;
    buffer->timestamp = buf.timestamp.tv_sec * 1000000000LL + buf.timestamp.tv_usec * 1000;

    return 0;
}

int VidecHAL::SetupV4L2Device() {
    // Open V4L2 device
    const char* dev_name = "/dev/video0";  // This should be configurable
    mDeviceFd = open(dev_name, O_RDWR);
    if (mDeviceFd < 0) {
        ALOGE("Failed to open device %s: %s", dev_name, strerror(errno));
        return -errno;
    }

    // Query capabilities
    v4l2_capability cap = {};
    if (ioctl(mDeviceFd, VIDIOC_QUERYCAP, &cap) < 0) {
        ALOGE("Failed to query capabilities: %s", strerror(errno));
        close(mDeviceFd);
        mDeviceFd = -1;
        return -errno;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_M2M_MPLANE)) {
        ALOGE("Device does not support multiplanar M2M");
        close(mDeviceFd);
        mDeviceFd = -1;
        return -EINVAL;
    }

    return 0;
}

int VidecHAL::ConfigureV4L2Format(const video_config_t* config) {
    // Configure output format (input to encoder)
    v4l2_format fmt = {};
    fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    fmt.fmt.pix_mp.width = config->width;
    fmt.fmt.pix_mp.height = config->height;
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12;
    fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;
    fmt.fmt.pix_mp.num_planes = 2;

    if (ioctl(mDeviceFd, VIDIOC_S_FMT, &fmt) < 0) {
        ALOGE("Failed to set output format: %s", strerror(errno));
        return -errno;
    }

    // Configure capture format (output from encoder)
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt.fmt.pix_mp.pixelformat = mState.codecType == VIDEO_CODEC_H264 ?
                                V4L2_PIX_FMT_H264 : V4L2_PIX_FMT_HEVC;

    if (ioctl(mDeviceFd, VIDIOC_S_FMT, &fmt) < 0) {
        ALOGE("Failed to set capture format: %s", strerror(errno));
        return -errno;
    }

    return 0;
}

int VidecHAL::AllocateV4L2Buffers() {
    // Request buffers for output
    v4l2_requestbuffers req = {};
    req.count = MAX_INPUT_BUFFERS;
    req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    req.memory = V4L2_MEMORY_DMABUF;

    if (ioctl(mDeviceFd, VIDIOC_REQBUFS, &req) < 0) {
        ALOGE("Failed to request output buffers: %s", strerror(errno));
        return -errno;
    }

    // Request buffers for capture
    req.count = MAX_OUTPUT_BUFFERS;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

    if (ioctl(mDeviceFd, VIDIOC_REQBUFS, &req) < 0) {
        ALOGE("Failed to request capture buffers: %s", strerror(errno));
        return -errno;
    }

    return 0;
}

int VidecHAL::FreeV4L2Buffers() {
    // Free output buffers
    v4l2_requestbuffers req = {};
    req.count = 0;
    req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    req.memory = V4L2_MEMORY_DMABUF;

    if (ioctl(mDeviceFd, VIDIOC_REQBUFS, &req) < 0) {
        ALOGE("Failed to free output buffers: %s", strerror(errno));
        return -errno;
    }

    // Free capture buffers
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

    if (ioctl(mDeviceFd, VIDIOC_REQBUFS, &req) < 0) {
        ALOGE("Failed to free capture buffers: %s", strerror(errno));
        return -errno;
    }

    return 0;
} 