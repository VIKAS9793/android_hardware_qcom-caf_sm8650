#ifndef QC_OMX_CORE_H
#define QC_OMX_CORE_H

#include <OMX_Core.h>
#include <OMX_Component.h>

#ifdef __cplusplus
extern "C" {
#endif

// Component name definitions
#define OMX_COMP_VIDEO_ENCODER    "OMX.qcom.video.encoder"
#define OMX_COMP_VIDEO_DECODER    "OMX.qcom.video.decoder"
#define OMX_COMP_AUDIO_ENCODER    "OMX.qcom.audio.encoder"
#define OMX_COMP_AUDIO_DECODER    "OMX.qcom.audio.decoder"

// Supported codecs
typedef enum {
    CODEC_TYPE_H264 = 0,
    CODEC_TYPE_H265,
    CODEC_TYPE_VP8,
    CODEC_TYPE_VP9,
    CODEC_TYPE_MPEG4,
    CODEC_TYPE_AAC,
    CODEC_TYPE_MP3,
    CODEC_TYPE_MAX
} codec_type;

// Component role definitions
typedef struct {
    const char* role;
    const codec_type type;
} component_role;

// Core functions
OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_Init(void);
OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_Deinit(void);
OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_ComponentNameEnum(
    OMX_STRING cComponentName,
    OMX_U32 nNameLength,
    OMX_U32 nIndex);
OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_GetHandle(
    OMX_HANDLETYPE* pHandle,
    OMX_STRING cComponentName,
    OMX_PTR pAppData,
    OMX_CALLBACKTYPE* pCallBacks);
OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_FreeHandle(
    OMX_HANDLETYPE hComponent);
OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_GetRolesOfComponent(
    OMX_STRING compName,
    OMX_U32* pNumRoles,
    OMX_U8** roles);

#ifdef __cplusplus
}
#endif

#endif // QC_OMX_CORE_H 