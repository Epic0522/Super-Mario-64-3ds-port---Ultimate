#include "src/pc/configfile.h"
#include "game/area.h"
#include "game/camera.h"
#include "level_table.h"
#include "enhancements/dynamic_shadows.h"
#include "engine/behavior_script.h"

static u8 sDynamicShadowRenderingMode = FALSE;
static struct DynamicShadowLight sDynamicShadowResolvedLight;
static s16 sDynamicShadowLightSeedLevel = LEVEL_NONE;
static s16 sDynamicShadowLightSeedArea = -1;
static s16 sDynamicShadowLightYawOffset = 0;
static f32 sDynamicShadowLightLengthScale = 1.0f;

Mat4 gDynamicShadowCameraInvMtx;
Mat4 gDynamicShadowLightVpMtx;
u8 gDynamicShadowLightMtxReady = FALSE;
u32 gDynamicShadowDebugLightReady = 0;
u32 gDynamicShadowDebugShadowNodes = 0;
u32 gDynamicShadowDebugCandidateObjects = 0;
u32 gDynamicShadowDebugAppendLists = 0;
u32 gDynamicShadowDebugCasterLists = 0;
u32 gDynamicShadowDebugDepthTris = 0;
u32 gDynamicShadowDebugReceiverTris = 0;
u32 gDynamicShadowDebugRejectMode = 0;
u32 gDynamicShadowDebugRejectContext = 0;
u32 gDynamicShadowDebugRejectChildren = 0;
u32 gDynamicShadowDebugRejectBobomb = 0;
u32 gDynamicShadowDebugRejectPlatform = 0;
u32 gDynamicShadowDebugRejectBillboard = 0;
u32 gDynamicShadowDebugRejectFar = 0;
u32 gDynamicShadowDebugRejectLight = 0;
u8 gDynamicShadowUnderwaterTint = FALSE;
u8 gDynamicShadowCasterBoundsReady = FALSE;
f32 gDynamicShadowCasterMinU = 1.0f;
f32 gDynamicShadowCasterMaxU = 0.0f;
f32 gDynamicShadowCasterMinV = 1.0f;
f32 gDynamicShadowCasterMaxV = 0.0f;

static const struct DynamicShadowLight sDynamicShadowLights[] = {
    { AREA_BOB,            0x2000, 0.60f, 0.35f, 92 },
    { AREA_SSL_OUTSIDE,   -0x1800, 0.72f, 0.35f, 86 },
    { AREA_SSL_PYRAMID,   -0x1000, 0.58f, 0.35f, 82 },
    { AREA_WF,             0x1800, 0.62f, 0.35f, 88 },
    { AREA_CCM_OUTSIDE,    0x3000, 0.54f, 0.35f, 80 },
    { AREA_SL_OUTSIDE,     0x3000, 0.54f, 0.35f, 78 },
    { AREA_TTM_OUTSIDE,    0x1800, 0.72f, 0.35f, 88 },
    { AREA_THI_HUGE,       0x2800, 0.60f, 0.35f, 84 },
    { AREA_THI_TINY,       0x2800, 0.60f, 0.35f, 84 },
    { AREA_CASTLE_GROUNDS, 0x2400, 0.52f, 0.35f, 72 },
    { AREA_VCUTM,          0x2400, 0.58f, 0.35f, 82 },
    { AREA_COTMC,          0x2000, 0.56f, 0.35f, 80 },
    { 0,                   0x2000, 0.56f, 0.35f, 80 },
};

u8 dynamic_shadows_should_render(void) {
    return dynamic_shadows_enabled != 0;
}

const struct DynamicShadowLight *dynamic_shadows_get_light(void) {
    s16 area = LEVEL_AREA_INDEX(gCurrLevelNum, gCurrAreaIndex);
    const struct DynamicShadowLight *fallback = &sDynamicShadowLights[sizeof(sDynamicShadowLights) / sizeof(sDynamicShadowLights[0]) - 1];
    const struct DynamicShadowLight *baseLight = fallback;

    for (u32 i = 0; i < sizeof(sDynamicShadowLights) / sizeof(sDynamicShadowLights[0]); i++) {
        if (sDynamicShadowLights[i].area == area) {
            baseLight = &sDynamicShadowLights[i];
            break;
        }
    }

    if (sDynamicShadowLightSeedLevel != gCurrLevelNum
        || sDynamicShadowLightSeedArea != gCurrAreaIndex) {
        s16 majorYawOffset;
        s16 minorYawOffset;

        sDynamicShadowLightSeedLevel = gCurrLevelNum;
        sDynamicShadowLightSeedArea = gCurrAreaIndex;
        majorYawOffset = (random_u16() & 0x0003) * 0x4000;
        minorYawOffset = (random_u16() & 0x0FFF) - 0x0800;
        sDynamicShadowLightYawOffset = majorYawOffset + minorYawOffset;
        sDynamicShadowLightLengthScale = 0.78f + ((random_u16() & 0x00FF) / 255.0f) * 0.62f;
    }

    sDynamicShadowResolvedLight = *baseLight;
    sDynamicShadowResolvedLight.yaw += sDynamicShadowLightYawOffset;
    sDynamicShadowResolvedLight.length *= sDynamicShadowLightLengthScale;
    if (sDynamicShadowResolvedLight.length < 0.35f) {
        sDynamicShadowResolvedLight.length = 0.35f;
    }
    if (sDynamicShadowResolvedLight.length > 1.05f) {
        sDynamicShadowResolvedLight.length = 1.05f;
    }
    return &sDynamicShadowResolvedLight;
}

void dynamic_shadows_set_rendering_mode(u8 enabled) {
    sDynamicShadowRenderingMode = enabled;
}

u8 dynamic_shadows_is_rendering_mode(void) {
    return sDynamicShadowRenderingMode;
}
