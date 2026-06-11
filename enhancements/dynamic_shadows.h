#ifndef DYNAMIC_SHADOWS_H
#define DYNAMIC_SHADOWS_H

#include <PR/ultratypes.h>
#include "types.h"

#define DYNAMIC_SHADOW_GFX_MAGIC 0x44534800u
#define DYNAMIC_SHADOW_GFX_ENABLE (DYNAMIC_SHADOW_GFX_MAGIC | 1u)
#define DYNAMIC_SHADOW_GFX_DISABLE (DYNAMIC_SHADOW_GFX_MAGIC | 0u)
#define DYNAMIC_SHADOW_GFX_PASS_MAGIC 0x44535000u
#define DYNAMIC_SHADOW_GFX_PASS_NORMAL (DYNAMIC_SHADOW_GFX_PASS_MAGIC | 0u)
#define DYNAMIC_SHADOW_GFX_PASS_DEPTH (DYNAMIC_SHADOW_GFX_PASS_MAGIC | 1u)
#define DYNAMIC_SHADOW_GFX_PASS_RECEIVER (DYNAMIC_SHADOW_GFX_PASS_MAGIC | 2u)
#define DYNAMIC_SHADOW_GFX_RECEIVER_MAGIC 0x44535200u
#define DYNAMIC_SHADOW_GFX_RECEIVER_ENABLE (DYNAMIC_SHADOW_GFX_RECEIVER_MAGIC | 1u)
#define DYNAMIC_SHADOW_GFX_RECEIVER_DISABLE (DYNAMIC_SHADOW_GFX_RECEIVER_MAGIC | 0u)
#define DYNAMIC_SHADOW_GFX_MASK_MAGIC 0x44534D00u
#define DYNAMIC_SHADOW_GFX_MASK_DISABLE (DYNAMIC_SHADOW_GFX_MASK_MAGIC | 0u)
#define DYNAMIC_SHADOW_GFX_MASK_WRITE(stencilRef) \
    (DYNAMIC_SHADOW_GFX_MASK_MAGIC | ((stencilRef) & 0xFFu))
// Safety fuse only. Real dynamic-shadow selection is distance/visibility based,
// so this should not decide which visible objects get shadows.
#define DYNAMIC_SHADOW_MAX_APPENDED_LISTS 128

struct DynamicShadowLight {
    s16 area;
    s16 yaw;
    f32 length;
    f32 lift;
    u8 alpha;
};

extern Mat4 gDynamicShadowCameraInvMtx;
extern Mat4 gDynamicShadowLightVpMtx;
extern u8 gDynamicShadowLightMtxReady;
extern u32 gDynamicShadowDebugLightReady;
extern u32 gDynamicShadowDebugShadowNodes;
extern u32 gDynamicShadowDebugCandidateObjects;
extern u32 gDynamicShadowDebugAppendLists;
extern u32 gDynamicShadowDebugCasterLists;
extern u32 gDynamicShadowDebugDepthTris;
extern u32 gDynamicShadowDebugReceiverTris;
extern u32 gDynamicShadowDebugRejectMode;
extern u32 gDynamicShadowDebugRejectContext;
extern u32 gDynamicShadowDebugRejectChildren;
extern u32 gDynamicShadowDebugRejectBobomb;
extern u32 gDynamicShadowDebugRejectPlatform;
extern u32 gDynamicShadowDebugRejectBillboard;
extern u32 gDynamicShadowDebugRejectFar;
extern u32 gDynamicShadowDebugRejectLight;
extern u8 gDynamicShadowCasterBoundsReady;
extern f32 gDynamicShadowCasterMinU;
extern f32 gDynamicShadowCasterMaxU;
extern f32 gDynamicShadowCasterMinV;
extern f32 gDynamicShadowCasterMaxV;

u8 dynamic_shadows_should_render(void);
const struct DynamicShadowLight *dynamic_shadows_get_light(void);
void dynamic_shadows_set_rendering_mode(u8 enabled);
u8 dynamic_shadows_is_rendering_mode(void);

#endif
