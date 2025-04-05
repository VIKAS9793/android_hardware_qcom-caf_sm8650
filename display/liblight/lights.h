#ifndef LIGHTS_H
#define LIGHTS_H

#include <hardware/hardware.h>
#include <hardware/lights.h>
#include <utils/Mutex.h>

using namespace android;

class Lights {
public:
    static int HookDevOpen(const struct hw_module_t* module, const char* name,
                          struct hw_device_t** device);

    Lights();
    ~Lights();

    // Light control functions
    static int SetLight(struct light_device_t* dev, struct light_state_t const* state);
    static int SetBacklight(struct light_state_t const* state);
    static int SetNotificationLight(struct light_state_t const* state);
    static int SetAttentionLight(struct light_state_t const* state);

private:
    struct LightState {
        int color;
        int flashMode;
        int flashOnMS;
        int flashOffMS;
        int brightnessMode;
    };

    // Device state
    Mutex mLock;
    LightState mBacklight;
    LightState mNotification;
    LightState mAttention;

    // Device paths
    static const char* const LCD_BACKLIGHT;
    static const char* const LED_BLINK;
    static const char* const LED_BRIGHTNESS;

    // Device capabilities
    static constexpr int MAX_BRIGHTNESS = 255;
    static constexpr int DEFAULT_MAX_BRIGHTNESS = 255;
    
    // Helper functions
    int WriteSysfs(const char* path, const char* content) const;
    int ReadSysfs(const char* path, char* buffer, size_t size) const;
    void ApplyNotificationState();
};

#endif // LIGHTS_H 