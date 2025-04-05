#ifndef HWC_DEVICE_H
#define HWC_DEVICE_H

#include <hardware/hwcomposer2.h>
#include <utils/Mutex.h>

using namespace android;

class HWCDevice {
public:
    static int HookDevOpen(const struct hw_module_t* module, const char* name,
                          struct hw_device_t** device);

    HWCDevice();
    ~HWCDevice();

    static int GetDisplayAttribute(hwc2_device_t* device, hwc2_display_t display,
                                 hwc2_config_t config,
                                 hwc2_attribute_t attribute, int32_t* outValue);

    static int PresentDisplay(hwc2_device_t* device, hwc2_display_t display,
                            int32_t* outRetireFence);

    static int ValidateDisplay(hwc2_device_t* device, hwc2_display_t display,
                             uint32_t* outNumTypes,
                             uint32_t* outNumRequests);

private:
    // Display attributes
    struct DisplayConfig {
        int32_t width;
        int32_t height;
        int32_t vsyncPeriod;
        int32_t dpiX;
        int32_t dpiY;
    };

    // Device state
    Mutex mStateLock;
    bool mPowerMode;
    DisplayConfig mActiveConfig;
};

#endif // HWC_DEVICE_H 