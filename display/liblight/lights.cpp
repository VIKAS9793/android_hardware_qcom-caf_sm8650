#define LOG_TAG "lights_sm8650"

#include <log/log.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "lights.h"

// Device paths
const char* const Lights::LCD_BACKLIGHT = "/sys/class/backlight/panel0-backlight/brightness";
const char* const Lights::LED_BLINK = "/sys/class/leds/rgb/blink";
const char* const Lights::LED_BRIGHTNESS = "/sys/class/leds/rgb/brightness";

Lights::Lights() {
    memset(&mBacklight, 0, sizeof(mBacklight));
    memset(&mNotification, 0, sizeof(mNotification));
    memset(&mAttention, 0, sizeof(mAttention));
}

Lights::~Lights() {
}

int Lights::HookDevOpen(const struct hw_module_t* module, const char* name,
                       struct hw_device_t** device) {
    if (strcmp(name, LIGHTS_HARDWARE_MODULE_ID) != 0) {
        ALOGE("Invalid device name: %s", name);
        return -EINVAL;
    }

    Lights* dev = new Lights();
    if (!dev) {
        ALOGE("Failed to allocate Lights device");
        return -ENOMEM;
    }

    light_device_t* light_dev = new light_device_t();
    if (!light_dev) {
        delete dev;
        return -ENOMEM;
    }

    light_dev->common.tag = HARDWARE_DEVICE_TAG;
    light_dev->common.version = LIGHTS_DEVICE_API_VERSION_2_0;
    light_dev->common.module = const_cast<hw_module_t*>(module);
    light_dev->common.close = nullptr;  // Will be set by framework
    light_dev->set_light = SetLight;

    *device = &light_dev->common;
    return 0;
}

int Lights::SetLight(struct light_device_t* dev, struct light_state_t const* state) {
    if (!dev) {
        ALOGE("Invalid device");
        return -EINVAL;
    }

    Lights* lights = static_cast<Lights*>(dev);
    int err = 0;

    switch (state->flashMode) {
        case LIGHT_ID_BACKLIGHT:
            err = lights->SetBacklight(state);
            break;
        case LIGHT_ID_NOTIFICATIONS:
            err = lights->SetNotificationLight(state);
            break;
        case LIGHT_ID_ATTENTION:
            err = lights->SetAttentionLight(state);
            break;
        default:
            return -EINVAL;
    }

    return err;
}

int Lights::SetBacklight(struct light_state_t const* state) {
    std::lock_guard<Mutex> lock(mLock);
    
    int brightness = state->color & 0xFF;
    // Scale brightness if necessary
    brightness = brightness * MAX_BRIGHTNESS / DEFAULT_MAX_BRIGHTNESS;
    
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "%d", brightness);
    return WriteSysfs(LCD_BACKLIGHT, buffer);
}

int Lights::SetNotificationLight(struct light_state_t const* state) {
    std::lock_guard<Mutex> lock(mLock);
    
    mNotification.color = state->color;
    mNotification.flashMode = state->flashMode;
    mNotification.flashOnMS = state->flashOnMS;
    mNotification.flashOffMS = state->flashOffMS;
    mNotification.brightnessMode = state->brightnessMode;
    
    ApplyNotificationState();
    return 0;
}

int Lights::SetAttentionLight(struct light_state_t const* state) {
    std::lock_guard<Mutex> lock(mLock);
    
    mAttention.color = state->color;
    mAttention.flashMode = state->flashMode;
    mAttention.flashOnMS = state->flashOnMS;
    mAttention.flashOffMS = state->flashOffMS;
    mAttention.brightnessMode = state->brightnessMode;
    
    ApplyNotificationState();
    return 0;
}

void Lights::ApplyNotificationState() {
    // Priority: Attention > Notification
    const LightState* activeState = nullptr;
    
    if (mAttention.color != 0) {
        activeState = &mAttention;
    } else if (mNotification.color != 0) {
        activeState = &mNotification;
    }
    
    if (activeState != nullptr) {
        // Set LED color
        char buffer[20];
        snprintf(buffer, sizeof(buffer), "0x%06x", activeState->color);
        WriteSysfs(LED_BRIGHTNESS, buffer);
        
        // Set LED blinking if requested
        if (activeState->flashMode == LIGHT_FLASH_TIMED) {
            snprintf(buffer, sizeof(buffer), "%d %d", 
                    activeState->flashOnMS, activeState->flashOffMS);
            WriteSysfs(LED_BLINK, buffer);
        }
    } else {
        // Turn off LED
        WriteSysfs(LED_BRIGHTNESS, "0");
        WriteSysfs(LED_BLINK, "0 0");
    }
}

int Lights::WriteSysfs(const char* path, const char* content) const {
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        ALOGE("Failed to open %s (%s)", path, strerror(errno));
        return -errno;
    }
    
    ssize_t len = strlen(content);
    ssize_t ret = write(fd, content, len);
    close(fd);
    
    return (ret == len) ? 0 : -EINVAL;
}

int Lights::ReadSysfs(const char* path, char* buffer, size_t size) const {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        ALOGE("Failed to open %s (%s)", path, strerror(errno));
        return -errno;
    }
    
    ssize_t ret = read(fd, buffer, size - 1);
    close(fd);
    
    if (ret >= 0) {
        buffer[ret] = '\0';
        return 0;
    }
    
    return -errno;
} 