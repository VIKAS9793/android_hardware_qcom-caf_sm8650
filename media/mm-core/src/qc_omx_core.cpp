#define LOG_TAG "qc_omx_core"

#include <dlfcn.h>
#include <string.h>
#include <log/log.h>
#include "qc_omx_core.h"

// Component roles
static const component_role component_roles[] = {
    { "video_encoder.avc", CODEC_TYPE_H264 },
    { "video_encoder.hevc", CODEC_TYPE_H265 },
    { "video_encoder.vp8", CODEC_TYPE_VP8 },
    { "video_encoder.vp9", CODEC_TYPE_VP9 },
    { "video_decoder.avc", CODEC_TYPE_H264 },
    { "video_decoder.hevc", CODEC_TYPE_H265 },
    { "video_decoder.vp8", CODEC_TYPE_VP8 },
    { "video_decoder.vp9", CODEC_TYPE_VP9 },
    { "audio_encoder.aac", CODEC_TYPE_AAC },
    { "audio_encoder.mp3", CODEC_TYPE_MP3 },
    { "audio_decoder.aac", CODEC_TYPE_AAC },
    { "audio_decoder.mp3", CODEC_TYPE_MP3 }
};

// Component library paths
static const char* component_libs[] = {
    "libOmxVenc.so",
    "libOmxVdec.so",
    "libOmxAacEnc.so",
    "libOmxAacDec.so"
};

// Component handles
static void* component_handles[CODEC_TYPE_MAX] = { nullptr };

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_Init(void) {
    ALOGI("Initializing OMX core");
    
    // Load component libraries
    for (size_t i = 0; i < sizeof(component_libs)/sizeof(component_libs[0]); i++) {
        void* handle = dlopen(component_libs[i], RTLD_NOW);
        if (!handle) {
            ALOGE("Failed to load %s: %s", component_libs[i], dlerror());
            continue;
        }
        component_handles[i] = handle;
    }
    
    return OMX_ErrorNone;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_Deinit(void) {
    ALOGI("Deinitializing OMX core");
    
    // Unload component libraries
    for (size_t i = 0; i < CODEC_TYPE_MAX; i++) {
        if (component_handles[i]) {
            dlclose(component_handles[i]);
            component_handles[i] = nullptr;
        }
    }
    
    return OMX_ErrorNone;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_ComponentNameEnum(
    OMX_STRING cComponentName,
    OMX_U32 nNameLength,
    OMX_U32 nIndex) {
    
    if (!cComponentName || nNameLength == 0) {
        return OMX_ErrorBadParameter;
    }
    
    if (nIndex >= sizeof(component_roles)/sizeof(component_roles[0])) {
        return OMX_ErrorNoMore;
    }
    
    const char* name = nullptr;
    switch (component_roles[nIndex].type) {
        case CODEC_TYPE_H264:
        case CODEC_TYPE_H265:
        case CODEC_TYPE_VP8:
        case CODEC_TYPE_VP9:
            name = strstr(component_roles[nIndex].role, "encoder") ?
                  OMX_COMP_VIDEO_ENCODER : OMX_COMP_VIDEO_DECODER;
            break;
        case CODEC_TYPE_AAC:
        case CODEC_TYPE_MP3:
            name = strstr(component_roles[nIndex].role, "encoder") ?
                  OMX_COMP_AUDIO_ENCODER : OMX_COMP_AUDIO_DECODER;
            break;
        default:
            return OMX_ErrorUndefined;
    }
    
    strlcpy(cComponentName, name, nNameLength);
    return OMX_ErrorNone;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_GetHandle(
    OMX_HANDLETYPE* pHandle,
    OMX_STRING cComponentName,
    OMX_PTR pAppData,
    OMX_CALLBACKTYPE* pCallBacks) {
    
    if (!pHandle || !cComponentName || !pCallBacks) {
        return OMX_ErrorBadParameter;
    }
    
    // Find component type
    void* lib_handle = nullptr;
    if (strstr(cComponentName, "video.encoder")) {
        lib_handle = component_handles[0];
    } else if (strstr(cComponentName, "video.decoder")) {
        lib_handle = component_handles[1];
    } else if (strstr(cComponentName, "audio.encoder")) {
        lib_handle = component_handles[2];
    } else if (strstr(cComponentName, "audio.decoder")) {
        lib_handle = component_handles[3];
    }
    
    if (!lib_handle) {
        ALOGE("Component %s not found", cComponentName);
        return OMX_ErrorComponentNotFound;
    }
    
    // Get component factory function
    typedef OMX_ERRORTYPE (*GetHandleFunc)(
        OMX_HANDLETYPE*, OMX_STRING, OMX_PTR, OMX_CALLBACKTYPE*);
    GetHandleFunc get_handle = (GetHandleFunc)dlsym(lib_handle, "OMX_ComponentInit");
    
    if (!get_handle) {
        ALOGE("Component %s entry point not found", cComponentName);
        return OMX_ErrorInvalidComponent;
    }
    
    // Create component instance
    return get_handle(pHandle, cComponentName, pAppData, pCallBacks);
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_FreeHandle(
    OMX_HANDLETYPE hComponent) {
    
    if (!hComponent) {
        return OMX_ErrorBadParameter;
    }
    
    // Component will be freed by its own FreeHandle implementation
    typedef OMX_ERRORTYPE (*FreeHandleFunc)(OMX_HANDLETYPE);
    FreeHandleFunc free_handle = (FreeHandleFunc)dlsym(
        component_handles[0], "OMX_ComponentDeInit");
    
    if (!free_handle) {
        ALOGE("Component free function not found");
        return OMX_ErrorInvalidComponent;
    }
    
    return free_handle(hComponent);
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_GetRolesOfComponent(
    OMX_STRING compName,
    OMX_U32* pNumRoles,
    OMX_U8** roles) {
    
    if (!compName || !pNumRoles) {
        return OMX_ErrorBadParameter;
    }
    
    // Count matching roles
    OMX_U32 count = 0;
    for (size_t i = 0; i < sizeof(component_roles)/sizeof(component_roles[0]); i++) {
        const char* name = nullptr;
        switch (component_roles[i].type) {
            case CODEC_TYPE_H264:
            case CODEC_TYPE_H265:
            case CODEC_TYPE_VP8:
            case CODEC_TYPE_VP9:
                name = strstr(component_roles[i].role, "encoder") ?
                      OMX_COMP_VIDEO_ENCODER : OMX_COMP_VIDEO_DECODER;
                break;
            case CODEC_TYPE_AAC:
            case CODEC_TYPE_MP3:
                name = strstr(component_roles[i].role, "encoder") ?
                      OMX_COMP_AUDIO_ENCODER : OMX_COMP_AUDIO_DECODER;
                break;
            default:
                continue;
        }
        
        if (strcmp(name, compName) == 0) {
            count++;
        }
    }
    
    if (count == 0) {
        return OMX_ErrorComponentNotFound;
    }
    
    *pNumRoles = count;
    
    if (!roles) {
        return OMX_ErrorNone;
    }
    
    // Fill in roles
    OMX_STRING* role_array = new OMX_STRING[count];
    count = 0;
    
    for (size_t i = 0; i < sizeof(component_roles)/sizeof(component_roles[0]); i++) {
        const char* name = nullptr;
        switch (component_roles[i].type) {
            case CODEC_TYPE_H264:
            case CODEC_TYPE_H265:
            case CODEC_TYPE_VP8:
            case CODEC_TYPE_VP9:
                name = strstr(component_roles[i].role, "encoder") ?
                      OMX_COMP_VIDEO_ENCODER : OMX_COMP_VIDEO_DECODER;
                break;
            case CODEC_TYPE_AAC:
            case CODEC_TYPE_MP3:
                name = strstr(component_roles[i].role, "encoder") ?
                      OMX_COMP_AUDIO_ENCODER : OMX_COMP_AUDIO_DECODER;
                break;
            default:
                continue;
        }
        
        if (strcmp(name, compName) == 0) {
            role_array[count] = const_cast<OMX_STRING>(component_roles[i].role);
            count++;
        }
    }
    
    *roles = reinterpret_cast<OMX_U8**>(role_array);
    return OMX_ErrorNone;
} 