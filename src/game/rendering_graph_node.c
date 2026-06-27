#include <PR/ultratypes.h>

#include "area.h"
#include "camera.h"
#include "engine/behavior_script.h"
#include "engine/math_util.h"
#include "engine/surface_load.h"
#include "game_init.h"
#include "geo_misc.h"
#include "gfx_dimensions.h"
#include "main.h"
#include "memory.h"
#include "print.h"
#include "rendering_graph_node.h"
#include "shadow.h"
#include "sm64.h"
#include "model_ids.h"
#include "engine/surface_collision.h"
#include "surface_terrains.h"
#include "behavior_data.h"
#include "level_update.h"
#include "object_fields.h"
#include "object_list_processor.h"
#include "save_file.h"
#include "segment2.h"

#ifdef TARGET_N3DS
#include "actors/common0.h"
#include "actors/group11.h"
#include "src/pc/gfx/gfx_citro3d.h"
#include "src/pc/gfx/color_conversion.h"
#include "enhancements/dynamic_shadows.h"
#include "enhancements/death_ragdoll.h"
#endif

/**
 * This file contains the code that processes the scene graph for rendering.
 * The scene graph is responsible for drawing everything except the HUD / text boxes.
 * First the root of the scene graph is processed when geo_process_root
 * is called from level_script.c. The rest of the tree is traversed recursively
 * using the function geo_process_node_and_siblings, which switches over all
 * geo node types and calls a specialized function accordingly.
 * The types are defined in engine/graph_node.h
 *
 * The scene graph typically looks like:
 * - Root (viewport)
 *  - Master list
 *   - Ortho projection
 *    - Background (skybox)
 *  - Master list
 *   - Perspective
 *    - Camera
 *     - <area-specific display lists>
 *     - Object parent
 *      - <group with 240 object nodes>
 *  - Master list
 *   - Script node (Cannon overlay)
 *
 */

s16 gMatStackIndex;
Mat4 gMatStack[32];
Mtx *gMatStackFixed[32];
Mat4 gMatStackInterpolated[32];
Mtx *gMatStackInterpolatedFixed[32];

/**
 * Animation nodes have state in global variables, so this struct captures
 * the animation state so a 'context switch' can be made when rendering the
 * held object.
 */
struct GeoAnimState {
    /*0x00*/ u8 type;
    /*0x01*/ u8 enabled;
    /*0x02*/ s16 frame;
    /*0x04*/ f32 translationMultiplier;
    /*0x08*/ u16 *attribute;
    /*0x0C*/ s16 *data;
    s16 prevFrame;
};

// For some reason, this is a GeoAnimState struct, but the current state consists
// of separate global variables. It won't match EU otherwise.
struct GeoAnimState gGeoTempState;

u8 gCurAnimType;
u8 gCurAnimEnabled;
s16 gCurrAnimFrame;
s16 gPrevAnimFrame;
f32 gCurAnimTranslationMultiplier;
u16 *gCurrAnimAttribute;
s16 *gCurAnimData;

struct AllocOnlyPool *gDisplayListHeap;

struct RenderModeContainer {
    u32 modes[8];
};

/* Rendermode settings for cycle 1 for all 8 layers. */
struct RenderModeContainer renderModeTable_1Cycle[2] = { { {
    G_RM_OPA_SURF,
    G_RM_AA_OPA_SURF,
    G_RM_AA_OPA_SURF,
    G_RM_AA_OPA_SURF,
    G_RM_AA_TEX_EDGE,
    G_RM_AA_XLU_SURF,
    G_RM_AA_XLU_SURF,
    G_RM_AA_XLU_SURF,
    } },
    { {
    /* z-buffered */
    G_RM_ZB_OPA_SURF,
    G_RM_AA_ZB_OPA_SURF,
    G_RM_AA_ZB_OPA_DECAL,
    G_RM_AA_ZB_OPA_INTER,
    G_RM_AA_ZB_TEX_EDGE,
    G_RM_AA_ZB_XLU_SURF,
    G_RM_AA_ZB_XLU_DECAL,
    G_RM_AA_ZB_XLU_INTER,
    } } };

/* Rendermode settings for cycle 2 for all 8 layers. */
struct RenderModeContainer renderModeTable_2Cycle[2] = { { {
    G_RM_OPA_SURF2,
    G_RM_AA_OPA_SURF2,
    G_RM_AA_OPA_SURF2,
    G_RM_AA_OPA_SURF2,
    G_RM_AA_TEX_EDGE2,
    G_RM_AA_XLU_SURF2,
    G_RM_AA_XLU_SURF2,
    G_RM_AA_XLU_SURF2,
    } },
    { {
    /* z-buffered */
    G_RM_ZB_OPA_SURF2,
    G_RM_AA_ZB_OPA_SURF2,
    G_RM_AA_ZB_OPA_DECAL2,
    G_RM_AA_ZB_OPA_INTER2,
    G_RM_AA_ZB_TEX_EDGE2,
    G_RM_AA_ZB_XLU_SURF2,
    G_RM_AA_ZB_XLU_DECAL2,
    G_RM_AA_ZB_XLU_INTER2,
    } } };

struct GraphNodeRoot *gCurGraphNodeRoot = NULL;
struct GraphNodeMasterList *gCurGraphNodeMasterList = NULL;
struct GraphNodePerspective *gCurGraphNodeCamFrustum = NULL;
struct GraphNodeCamera *gCurGraphNodeCamera = NULL;
struct GraphNodeObject *gCurGraphNodeObject = NULL;
struct GraphNodeHeldObject *gCurGraphNodeHeldObject = NULL;
u16 gAreaUpdateCounter = 0;

#ifdef F3DEX_GBI_2
LookAt lookAt;
#endif

static Gfx *sPerspectivePos;
static Mtx *sPerspectiveMtx;
static u8 sAppendingDynamicShadow = FALSE;
static u8 sSuppressDynamicShadowReceiver = FALSE;
#ifdef TARGET_N3DS
static u8 dynamic_shadow_allows_billboard_caster(void);
static u8 dynamic_shadow_allows_billboard_component_caster(void);
static u8 dynamic_shadow_should_skip_billboard_circle(struct GraphNodeBillboard *node,
                                                      Vec3f circleCenter);
static u8 dynamic_shadow_is_mario_object(void);
static u8 dynamic_shadow_is_cannon_base_object(void);
static u8 dynamic_shadow_allows_shadow_alpha_layer(void);
static u8 dynamic_shadow_is_platform_interaction_caster(void);
static u8 dynamic_shadow_requires_static_floor_receiver(void);
static u8 dynamic_shadow_object_is_held(struct Object *obj);
static u8 dynamic_shadow_never_keep_original_shadow(void);
static u8 dynamic_shadow_behavior_is(const BehaviorScript *behavior,
                                     const BehaviorScript *candidate);
static u8 dynamic_shadow_behavior_is_bowser_course_platform(const BehaviorScript *behavior);
static u8 dynamic_shadow_surface_requires_blob_fallback(struct Surface *surface);
static const BehaviorScript *dynamic_shadow_current_behavior(void);
static void dynamic_shadow_get_model_anchor(Vec3f dst, Vec3f shadowPos);
static f32 dynamic_shadow_get_held_object_blob_scale(void);
static u8 dynamic_shadow_held_object_needs_blob_fallback(void);
static u8 sHeldObjectShadowRendered = FALSE;
static u8 sCurrentObjectDynamicShadowGenerated = FALSE;
static u8 sCurrentObjectPlatformInteractionCaster = FALSE;
static u8 sUseShadowNodeAnchor = FALSE;
static void *sAppendingDynamicShadowMask;
static Mtx *sAppendingDynamicShadowMaskTransform;
static Mtx *sAppendingDynamicShadowMaskTransformInterpolated;
static struct Surface *sAppendingDynamicShadowSurface;
static u16 sAppendingDynamicShadowGroup;
static u16 sNextDynamicShadowGroup = 1;
static Mat4 *sDynamicShadowModelMtxOverride;
static Mat4 *sDynamicShadowModelMtxInterpolatedOverride;
#define DYNAMIC_SHADOW_MAX_MASK_CACHE 48
struct DynamicShadowMaskCacheEntry {
    struct Surface *anchor;
    void *displayList;
    Mtx *transform;
    Mtx *transformInterpolated;
    u16 group;
};
static struct DynamicShadowMaskCacheEntry
    sDynamicShadowMaskCache[DYNAMIC_SHADOW_MAX_MASK_CACHE];
static u8 sDynamicShadowMaskCacheCount;
#endif

struct {
    Gfx *pos;
    void *mtx;
    void *displayList;
} gMtxTbl[6400];
s32 gMtxTblSize;

#ifdef TARGET_N3DS
// Shadow scale passed from geo_process_shadow() to geo_process_billboard()
static f32 sDynamicShadowBillboardScale = 0.0f;
static f32 sDynamicShadowBillboardGeoScale = 1.0f;
#define DYNAMIC_SHADOW_SURFACE_SINK 2.0f

#include "enhancements/dynamic_shadows.inc.c"

static void dynamic_shadow_update_light_matrices(void);
static void geo_append_display_list2(void *displayList, void *displayListInterpolated, s16 layer);

static void geo_emit_dynamic_shadow_mode(u8 enabled) {
    gDPForceFlush(gDisplayListHead++);
    gDisplayListHead->words.w0 = _SHIFTL(G_SPECIAL_2, 24, 8);
    gDisplayListHead->words.w1 = enabled ? DYNAMIC_SHADOW_GFX_ENABLE : DYNAMIC_SHADOW_GFX_DISABLE;
    gDisplayListHead++;
}

static void geo_emit_dynamic_shadow_pass(u32 pass) {
    gDPForceFlush(gDisplayListHead++);
    gDisplayListHead->words.w0 = _SHIFTL(G_SPECIAL_2, 24, 8);
    gDisplayListHead->words.w1 = pass;
    gDisplayListHead++;
}

static void geo_emit_dynamic_shadow_receiver_marking(u8 enabled) {
    gDPForceFlush(gDisplayListHead++);
    gDisplayListHead->words.w0 = _SHIFTL(G_SPECIAL_2, 24, 8);
    gDisplayListHead->words.w1 = enabled ? DYNAMIC_SHADOW_GFX_RECEIVER_ENABLE : DYNAMIC_SHADOW_GFX_RECEIVER_DISABLE;
    gDisplayListHead++;
}

static void geo_emit_dynamic_shadow_mask_mode(u32 mode) {
    gDPForceFlush(gDisplayListHead++);
    gDisplayListHead->words.w0 = _SHIFTL(G_SPECIAL_2, 24, 8);
    gDisplayListHead->words.w1 = mode;
    gDisplayListHead++;
}

static void geo_emit_dynamic_shadow_tint(u8 tintMode) {
    gDPForceFlush(gDisplayListHead++);
    gDisplayListHead->words.w0 = _SHIFTL(G_SPECIAL_2, 24, 8);
    gDisplayListHead->words.w1 = DYNAMIC_SHADOW_GFX_TINT_MAGIC | tintMode;
    gDisplayListHead++;
}

static void geo_draw_dynamic_shadow_mask(struct DisplayListNode *node, u32 mode) {
    if (node == NULL || node->dynamicShadowMask == NULL
        || node->dynamicShadowMaskTransformInterpolated == NULL) {
        return;
    }

    geo_emit_dynamic_shadow_mask_mode(mode);
    gSPMatrix(gDisplayListHead++, VIRTUAL_TO_PHYSICAL(node->dynamicShadowMaskTransformInterpolated),
              G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPDisplayList(gDisplayListHead++, node->dynamicShadowMask);
    geo_emit_dynamic_shadow_mask_mode(DYNAMIC_SHADOW_GFX_MASK_DISABLE);
}

static u8 dynamic_shadow_layer_can_cast(s16 layer) {
    return layer == LAYER_OPAQUE
        || layer == LAYER_OPAQUE_DECAL
        || layer == LAYER_OPAQUE_INTER;
}

static void *dynamic_shadow_gfx_arg_to_virtual(uintptr_t addr) {
#ifndef NO_SEGMENTED_MEMORY
    if ((addr >> 24) < 0x10) {
        return segmented_to_virtual((void *) addr);
    }
#endif
    return (void *) addr;
}

static s32 dynamic_shadow_get_vtx_count(Gfx *cmd) {
#if defined(F3DEX_GBI_2)
    return (cmd->words.w0 >> 12) & 0xFF;
#elif defined(F3DEX_GBI) || defined(F3DLP_GBI)
    return (cmd->words.w0 & 0xFFFF) >> 10;
#else
    return (cmd->words.w0 & 0xFFFF) / sizeof(Vtx);
#endif
}

static void dynamic_shadow_measure_vtx_bounds(Vtx *verts, s32 count, Vec3f min, Vec3f max,
                                              u8 *found) {
    s32 i;
    s32 axis;

    for (i = 0; i < count; i++) {
        for (axis = 0; axis < 3; axis++) {
            f32 value = verts[i].v.ob[axis];
            if (!*found || value < min[axis]) {
                min[axis] = value;
            }
            if (!*found || value > max[axis]) {
                max[axis] = value;
            }
        }
        *found = TRUE;
    }
}

static void dynamic_shadow_measure_gfx_bounds(Gfx *displayList, Vec3f min, Vec3f max,
                                              u8 *found, s32 depth) {
    Gfx *cmd;

    if (displayList == NULL || depth > 8) {
        return;
    }

    for (cmd = dynamic_shadow_gfx_arg_to_virtual((uintptr_t) displayList); cmd != NULL; cmd++) {
        u8 op = cmd->words.w0 >> 24;

        if (op == (u8) G_ENDDL) {
            return;
        }
        if (op == (u8) G_VTX) {
            Vtx *verts = dynamic_shadow_gfx_arg_to_virtual(cmd->words.w1);
            dynamic_shadow_measure_vtx_bounds(verts, dynamic_shadow_get_vtx_count(cmd),
                                              min, max, found);
        } else if (op == (u8) G_DL) {
            dynamic_shadow_measure_gfx_bounds(
                dynamic_shadow_gfx_arg_to_virtual(cmd->words.w1), min, max, found, depth + 1);
            if (((cmd->words.w0 >> 16) & 0xFF) == G_DL_NOPUSH) {
                return;
            }
        }
    }
}

static void dynamic_shadow_measure_billboard_bounds(struct GraphNode *node, Vec3f min,
                                                    Vec3f max, u8 *found) {
    struct GraphNode *first = node;
    struct GraphNode *cur = first;

    if (cur == NULL) {
        return;
    }

    do {
        switch (cur->type) {
            case GRAPH_NODE_TYPE_ANIMATED_PART:
                dynamic_shadow_measure_gfx_bounds(
                    ((struct GraphNodeAnimatedPart *) cur)->displayList, min, max, found, 0);
                break;
            case GRAPH_NODE_TYPE_BILLBOARD:
                dynamic_shadow_measure_gfx_bounds(
                    ((struct GraphNodeBillboard *) cur)->displayList, min, max, found, 0);
                break;
            case GRAPH_NODE_TYPE_DISPLAY_LIST:
                dynamic_shadow_measure_gfx_bounds(
                    ((struct GraphNodeDisplayList *) cur)->displayList, min, max, found, 0);
                break;
            case GRAPH_NODE_TYPE_SCALE:
                dynamic_shadow_measure_gfx_bounds(
                    ((struct GraphNodeScale *) cur)->displayList, min, max, found, 0);
                break;
        }

        if (cur->children != NULL) {
            dynamic_shadow_measure_billboard_bounds(cur->children, min, max, found);
        }
        cur = cur->next;
    } while (cur != NULL && cur != first);
}

static u8 dynamic_shadow_node_contains_display_list(struct GraphNode *node, const Gfx *target) {
    struct GraphNode *first = node;
    struct GraphNode *cur = first;
    void *targetVirtual;

    if (cur == NULL || target == NULL) {
        return FALSE;
    }

    targetVirtual = dynamic_shadow_gfx_arg_to_virtual((uintptr_t) target);
    do {
        switch (cur->type) {
            case GRAPH_NODE_TYPE_ANIMATED_PART:
                if (dynamic_shadow_gfx_arg_to_virtual(
                        (uintptr_t) ((struct GraphNodeAnimatedPart *) cur)->displayList)
                    == targetVirtual) {
                    return TRUE;
                }
                break;
            case GRAPH_NODE_TYPE_BILLBOARD:
                if (dynamic_shadow_gfx_arg_to_virtual(
                        (uintptr_t) ((struct GraphNodeBillboard *) cur)->displayList)
                    == targetVirtual) {
                    return TRUE;
                }
                break;
            case GRAPH_NODE_TYPE_DISPLAY_LIST:
                if (dynamic_shadow_gfx_arg_to_virtual(
                        (uintptr_t) ((struct GraphNodeDisplayList *) cur)->displayList)
                    == targetVirtual) {
                    return TRUE;
                }
                break;
            case GRAPH_NODE_TYPE_SCALE:
                if (dynamic_shadow_gfx_arg_to_virtual(
                        (uintptr_t) ((struct GraphNodeScale *) cur)->displayList)
                    == targetVirtual) {
                    return TRUE;
                }
                break;
        }

        if (cur->children != NULL
            && dynamic_shadow_node_contains_display_list(cur->children, target)) {
            return TRUE;
        }
        cur = cur->next;
    } while (cur != NULL && cur != first);

    return FALSE;
}

static u8 dynamic_shadow_should_emit_billboard_circle(f32 radius) {
    return radius >= 4.0f;
}

static Gfx *dynamic_shadow_create_circle(f32 radius) {
    enum { CIRCLE_SEGMENTS = 12 };
    Vtx *verts;
    Gfx *baseDisplayList;
    Gfx *displayList;
    s32 i;
    u8 alpha = 255;

    if (radius <= 0.0f) {
        return NULL;
    }

    verts = alloc_display_list((CIRCLE_SEGMENTS + 1) * sizeof(*verts));
    baseDisplayList = alloc_display_list((CIRCLE_SEGMENTS + 4) * sizeof(*displayList));
    displayList = baseDisplayList;
    if (verts == NULL || displayList == NULL) {
        return NULL;
    }

    make_vertex(verts, 0, 0, 0, 0, 0, 0, 255, 255, 255, alpha);
    for (i = 0; i < CIRCLE_SEGMENTS; i++) {
        s16 angle = (0x10000 / CIRCLE_SEGMENTS) * i;
        make_vertex(verts, i + 1, radius * coss(angle), 0, radius * sins(angle),
                    0, 0, 255, 255, 255, alpha);
    }

    gSPClearGeometryMode(displayList++, G_CULL_BOTH);
    gSPVertex(displayList++, verts, CIRCLE_SEGMENTS + 1, 0);
    for (i = 0; i < CIRCLE_SEGMENTS; i++) {
        gSP1Triangle(displayList++, 0, i + 1, (i + 1) % CIRCLE_SEGMENTS + 1, 0);
    }
    gSPDisplayList(displayList++, dl_shadow_end);
    gSPEndDisplayList(displayList);

    return baseDisplayList;
}

static void dynamic_shadow_transform_point(Vec3f dst, Mat4 mtx, Vec3f point) {
    dst[0] = point[0] * mtx[0][0] + point[1] * mtx[1][0] + point[2] * mtx[2][0] + mtx[3][0];
    dst[1] = point[0] * mtx[0][1] + point[1] * mtx[1][1] + point[2] * mtx[2][1] + mtx[3][1];
    dst[2] = point[0] * mtx[0][2] + point[1] * mtx[1][2] + point[2] * mtx[2][2] + mtx[3][2];
}

static void dynamic_shadow_camera_point_to_world(Vec3f dst, Vec3f cameraPoint, Mat4 cameraMtx) {
    f32 camX = cameraMtx[3][0] * cameraMtx[0][0]
        + cameraMtx[3][1] * cameraMtx[0][1]
        + cameraMtx[3][2] * cameraMtx[0][2];
    f32 camY = cameraMtx[3][0] * cameraMtx[1][0]
        + cameraMtx[3][1] * cameraMtx[1][1]
        + cameraMtx[3][2] * cameraMtx[1][2];
    f32 camZ = cameraMtx[3][0] * cameraMtx[2][0]
        + cameraMtx[3][1] * cameraMtx[2][1]
        + cameraMtx[3][2] * cameraMtx[2][2];

    dst[0] = cameraPoint[0] * cameraMtx[0][0]
        + cameraPoint[1] * cameraMtx[0][1]
        + cameraPoint[2] * cameraMtx[0][2] - camX;
    dst[1] = cameraPoint[0] * cameraMtx[1][0]
        + cameraPoint[1] * cameraMtx[1][1]
        + cameraPoint[2] * cameraMtx[1][2] - camY;
    dst[2] = cameraPoint[0] * cameraMtx[2][0]
        + cameraPoint[1] * cameraMtx[2][1]
        + cameraPoint[2] * cameraMtx[2][2] - camZ;
}

static void dynamic_shadow_projected_local_point_to_world(Vec3f dst, Mat4 projectedMtx,
                                                          Mat4 cameraMtx, Vec3f point) {
    Vec3f cameraPoint;

    dynamic_shadow_transform_point(cameraPoint, projectedMtx, point);
    dynamic_shadow_camera_point_to_world(dst, cameraPoint, cameraMtx);
}

static Gfx *dynamic_shadow_create_billboard_circle(struct GraphNodeBillboard *node,
                                                   Vec3f center) {
    Vec3f min;
    Vec3f max;
    f32 width;
    f32 height;
    f32 depth;
    f32 radius;
    u8 found = FALSE;

    dynamic_shadow_measure_gfx_bounds(node->displayList, min, max, &found, 0);
    dynamic_shadow_measure_billboard_bounds(node->node.children, min, max, &found);

    if (found) {
        width = max[0] - min[0];
        height = max[1] - min[1];
        depth = max[2] - min[2];
        center[0] = (min[0] + max[0]) * 0.5f;
        center[1] = (min[1] + max[1]) * 0.5f;
        center[2] = (min[2] + max[2]) * 0.5f;
        radius = width;
        if (height > radius) {
            radius = height;
        }
        if (depth > radius) {
            radius = depth;
        }
        radius *= 0.5f;
        if (!dynamic_shadow_should_emit_billboard_circle(radius)) {
            return NULL;
        }
        if (dynamic_shadow_behavior_is(dynamic_shadow_current_behavior(), bhvKingBobomb)
            && radius < sDynamicShadowBillboardScale * 1.35f) {
            radius = sDynamicShadowBillboardScale * 1.35f;
        }
        if (dynamic_shadow_behavior_is(dynamic_shadow_current_behavior(), bhvChuckya)
            && dynamic_shadow_node_contains_display_list(&node->node, chuckya_seg8_dl_0800A068)) {
            radius = sDynamicShadowBillboardScale > 0.0f
                ? sDynamicShadowBillboardScale * 1.05f : 105.0f;
        }
    } else {
        vec3f_set(center, 0.0f, 0.0f, 0.0f);
        radius = sDynamicShadowBillboardScale > 0.0f ? sDynamicShadowBillboardScale * 0.25f : 32.0f;
    }

    return dynamic_shadow_create_circle(radius);
}

static void dynamic_shadow_append_circle_on_receiver(Gfx *circleList, Vec3f circleCenter) {
    Mat4 projectedMtx;
    Mat4 projectedMtxInterpolated;
    Mat4 circleMtx;
    Vec3f circleWorld;
    Vec3f circleWorldInterpolated;
    Vec3f surfaceNormal;
    Mtx *mtx;
    Mtx *mtxInterpolated;

    if (circleList == NULL || gCurGraphNodeCamera == NULL) {
        return;
    }

    mtx = alloc_display_list(sizeof(*mtx));
    mtxInterpolated = alloc_display_list(sizeof(*mtxInterpolated));
    if (mtx == NULL || mtxInterpolated == NULL) {
        return;
    }

    mtxf_copy(projectedMtx, gMatStack[gMatStackIndex]);
    mtxf_copy(projectedMtxInterpolated, gMatStackInterpolated[gMatStackIndex]);
    dynamic_shadow_projected_local_point_to_world(circleWorld, projectedMtx,
                                                 *gCurGraphNodeCamera->matrixPtr,
                                                 circleCenter);
    dynamic_shadow_projected_local_point_to_world(circleWorldInterpolated,
                                                 projectedMtxInterpolated,
                                                 *gCurGraphNodeCamera->matrixPtrInterpolated,
                                                 circleCenter);

    gMatStackIndex++;
    if (sAppendingDynamicShadowSurface != NULL) {
        surfaceNormal[0] = sAppendingDynamicShadowSurface->normal.x;
        surfaceNormal[1] = sAppendingDynamicShadowSurface->normal.y;
        surfaceNormal[2] = sAppendingDynamicShadowSurface->normal.z;
        mtxf_align_terrain_normal(circleMtx, surfaceNormal, circleWorld, 0);
    } else {
        mtxf_translate(circleMtx, circleWorld);
    }
    mtxf_mul(gMatStack[gMatStackIndex], circleMtx, *gCurGraphNodeCamera->matrixPtr);
    mtxf_to_mtx(mtx, gMatStack[gMatStackIndex]);
    gMatStackFixed[gMatStackIndex] = mtx;

    if (sAppendingDynamicShadowSurface != NULL) {
        surfaceNormal[0] = sAppendingDynamicShadowSurface->normal.x;
        surfaceNormal[1] = sAppendingDynamicShadowSurface->normal.y;
        surfaceNormal[2] = sAppendingDynamicShadowSurface->normal.z;
        mtxf_align_terrain_normal(circleMtx, surfaceNormal, circleWorldInterpolated, 0);
    } else {
        mtxf_translate(circleMtx, circleWorldInterpolated);
    }
    mtxf_mul(gMatStackInterpolated[gMatStackIndex], circleMtx,
             *gCurGraphNodeCamera->matrixPtrInterpolated);
    mtxf_to_mtx(mtxInterpolated, gMatStackInterpolated[gMatStackIndex]);
    gMatStackInterpolatedFixed[gMatStackIndex] = mtxInterpolated;

    geo_append_display_list2((void *) VIRTUAL_TO_PHYSICAL(circleList),
                             (void *) VIRTUAL_TO_PHYSICAL(circleList), LAYER_OPAQUE);
    gMatStackIndex--;
}
#endif

static Gfx *sViewportPos;
static Vp sPrevViewport;

void mtx_patch_interpolated(void) {
    s32 i;

    if (sPerspectivePos != NULL) {
        gSPMatrix(sPerspectivePos, VIRTUAL_TO_PHYSICAL(sPerspectiveMtx), G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    }

    for (i = 0; i < gMtxTblSize; i++) {
        Gfx *pos = gMtxTbl[i].pos;
        gSPMatrix(pos++, VIRTUAL_TO_PHYSICAL(gMtxTbl[i].mtx),
                  G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
        gSPDisplayList(pos++, gMtxTbl[i].displayList);
    }

    if (sViewportPos != NULL) {
        Gfx *saved = gDisplayListHead;
        gDisplayListHead = sViewportPos;
        make_viewport_clip_rect(&sPrevViewport);
        gSPViewport(gDisplayListHead, VIRTUAL_TO_PHYSICAL(&sPrevViewport));
        gDisplayListHead = saved;
    }

    gMtxTblSize = 0;
    sPerspectivePos = NULL;
    sViewportPos = NULL;
}

/**
 * Process a master list node.
 */
static void geo_process_master_list_sub(struct GraphNodeMasterList *node) {
    struct DisplayListNode *currList;
    s32 i;
    s32 enableZBuffer = (node->node.flags & GRAPH_RENDER_Z_BUFFER) != 0;
    struct RenderModeContainer *modeList = &renderModeTable_1Cycle[enableZBuffer];
    struct RenderModeContainer *mode2List = &renderModeTable_2Cycle[enableZBuffer];
#ifdef TARGET_N3DS
    struct DisplayListNode *shadowGroupNodes[DYNAMIC_SHADOW_MAX_MASK_CACHE];
    u16 shadowGroups[DYNAMIC_SHADOW_MAX_MASK_CACHE];
    s32 shadowGroupCount = 0;
    s32 shadowGroupIndex;
#endif

    // @bug This is where the LookAt values should be calculated but aren't.
    // As a result, environment mapping is broken on Fast3DEX2 without the
    // changes below.
#ifdef F3DEX_GBI_2
    Mtx lMtx;
    guLookAtReflect(&lMtx, &lookAt, 0, 0, 0, /* eye */ 0, 0, 1, /* at */ 1, 0, 0 /* up */);
#endif

    if (enableZBuffer != 0) {
        gDPPipeSync(gDisplayListHead++);
        gSPSetGeometryMode(gDisplayListHead++, G_ZBUFFER);
    }

#ifdef TARGET_N3DS
    // Receiver stencil is now populated per caster from the blob-anchored
    // connected collision mesh, never from every opaque polygon on screen.
    geo_emit_dynamic_shadow_receiver_marking(FALSE);
#endif
    for (i = 0; i < GFX_NUM_MASTER_LISTS; i++) {
        if ((currList = node->listHeads[i]) != NULL) {
            gDPSetRenderMode(gDisplayListHead++, modeList->modes[i], mode2List->modes[i]);
            while (currList != NULL) {
#ifdef TARGET_N3DS
                if (currList->dynamicShadow == 1) {
                    currList = currList->next;
                    continue;
                }
#endif
                if ((u32) gMtxTblSize < sizeof(gMtxTbl) / sizeof(gMtxTbl[0])) {
                    gMtxTbl[gMtxTblSize].pos = gDisplayListHead;
                    gMtxTbl[gMtxTblSize].mtx = currList->transform;
                    gMtxTbl[gMtxTblSize++].displayList = currList->displayList;
                }
                gSPMatrix(gDisplayListHead++, VIRTUAL_TO_PHYSICAL(currList->transformInterpolated),
                          G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
                gSPDisplayList(gDisplayListHead++, currList->displayListInterpolated);
#ifdef TARGET_N3DS
#endif
                currList = currList->next;
            }
        }
    }
#ifdef TARGET_N3DS
    geo_emit_dynamic_shadow_receiver_marking(FALSE);
    geo_emit_dynamic_shadow_mode(TRUE);
    for (i = 0; i < GFX_NUM_MASTER_LISTS; i++) {
        for (currList = node->listHeads[i]; currList != NULL; currList = currList->next) {
            if (currList->dynamicShadow == 1) {
                for (shadowGroupIndex = 0; shadowGroupIndex < shadowGroupCount;
                     shadowGroupIndex++) {
                    if (shadowGroups[shadowGroupIndex] == currList->dynamicShadowGroup) {
                        break;
                    }
                }
                if (shadowGroupIndex == shadowGroupCount
                    && shadowGroupCount < DYNAMIC_SHADOW_MAX_MASK_CACHE) {
                    shadowGroups[shadowGroupCount] = currList->dynamicShadowGroup;
                    shadowGroupNodes[shadowGroupCount] = currList;
                    shadowGroupCount++;
                }
            }
        }
    }
    for (shadowGroupIndex = 0; shadowGroupIndex < shadowGroupCount; shadowGroupIndex++) {
        geo_draw_dynamic_shadow_mask(
            shadowGroupNodes[shadowGroupIndex],
            DYNAMIC_SHADOW_GFX_MASK_WRITE(shadowGroups[shadowGroupIndex]));
        for (i = 0; i < GFX_NUM_MASTER_LISTS; i++) {
            gDPSetRenderMode(gDisplayListHead++, modeList->modes[i], mode2List->modes[i]);
            for (currList = node->listHeads[i]; currList != NULL; currList = currList->next) {
                if (currList->dynamicShadow == 1
                    && currList->dynamicShadowGroup == shadowGroups[shadowGroupIndex]) {
                    gDynamicShadowDebugCasterLists++;
                    geo_emit_dynamic_shadow_tint(currList->dynamicShadowUnderwaterTint);
                    gSPMatrix(gDisplayListHead++, VIRTUAL_TO_PHYSICAL(currList->transformInterpolated),
                              G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
                    gSPDisplayList(gDisplayListHead++, currList->displayListInterpolated);
                }
            }
        }
    }
    geo_emit_dynamic_shadow_tint(FALSE);
    geo_emit_dynamic_shadow_mode(FALSE);
    // Receiver coverage is supplied by each caster's collision-mesh mask.
    // Leaving global marking enabled leaks into later scene lists and visibly
    // darkens nearby castle geometry.
    geo_emit_dynamic_shadow_receiver_marking(FALSE);
#endif
    if (enableZBuffer != 0) {
        gDPPipeSync(gDisplayListHead++);
        gSPClearGeometryMode(gDisplayListHead++, G_ZBUFFER);
    }
}

/**
 * Appends the display list to one of the master lists based on the layer
 * parameter. Look at the RenderModeContainer struct to see the corresponding
 * render modes of layers.
 */
static void geo_append_display_list2(void *displayList, void *displayListInterpolated, s16 layer) {

#ifdef F3DEX_GBI_2
    gSPLookAt(gDisplayListHead++, &lookAt);
#endif
    if (gCurGraphNodeMasterList != 0) {
#ifdef TARGET_N3DS
        if (sAppendingDynamicShadow && !dynamic_shadow_layer_can_cast(layer)
            && !dynamic_shadow_allows_shadow_alpha_layer()) {
            return;
        }
        if (sAppendingDynamicShadow && dynamic_shadow_is_cannon_base_object()
            && dynamic_shadow_gfx_arg_to_virtual((uintptr_t) displayList)
                == dynamic_shadow_gfx_arg_to_virtual((uintptr_t) cannon_base_seg8_dl_080057F8)) {
            return;
        }
        // Per-display-list safety cap (redundant with the per-object cap in
        // can_render_model_object, but guards against objects with many parts)
        if (sAppendingDynamicShadow && !dynamic_shadow_is_platform_interaction_caster()
            && gDynamicShadowDebugAppendLists >= DYNAMIC_SHADOW_MAX_APPENDED_LISTS) {
            return;
        }
#endif
        struct DisplayListNode *listNode =
            alloc_only_pool_alloc(gDisplayListHeap, sizeof(struct DisplayListNode));

        listNode->transform = gMatStackFixed[gMatStackIndex];
        listNode->transformInterpolated = gMatStackInterpolatedFixed[gMatStackIndex];
        listNode->displayList = displayList;
        listNode->displayListInterpolated = displayListInterpolated;
        listNode->dynamicShadowMaskTransform = NULL;
        listNode->dynamicShadowMaskTransformInterpolated = NULL;
        listNode->dynamicShadowMask = NULL;
        listNode->dynamicShadowGroup = 0;
        listNode->dynamicShadowUnderwaterTint = FALSE;
#ifdef TARGET_N3DS
        if (sAppendingDynamicShadow) {
            listNode->dynamicShadow = 1;
            listNode->dynamicShadowMaskTransform = sAppendingDynamicShadowMaskTransform;
            listNode->dynamicShadowMaskTransformInterpolated =
                sAppendingDynamicShadowMaskTransformInterpolated;
            listNode->dynamicShadowMask = sAppendingDynamicShadowMask;
            listNode->dynamicShadowGroup = sAppendingDynamicShadowGroup;
            listNode->dynamicShadowUnderwaterTint = gDynamicShadowUnderwaterTint;
            layer = LAYER_OPAQUE;
        } else if (sSuppressDynamicShadowReceiver || gCurGraphNodeHeldObject != NULL) {
            listNode->dynamicShadow = 2;
        } else {
            listNode->dynamicShadow = 0;
        }
#else
        listNode->dynamicShadow = 0;
#endif
#ifdef TARGET_N3DS
        if (sAppendingDynamicShadow) {
            gDynamicShadowDebugAppendLists++;
        }
#endif
        listNode->next = 0;
        if (gCurGraphNodeMasterList->listHeads[layer] == 0) {
            gCurGraphNodeMasterList->listHeads[layer] = listNode;
        } else {
            gCurGraphNodeMasterList->listTails[layer]->next = listNode;
        }
        gCurGraphNodeMasterList->listTails[layer] = listNode;
    }
}

static void geo_append_display_list(void *displayList, s16 layer) {
    geo_append_display_list2(displayList, displayList, layer);
}

/**
 * Process the master list node.
 */
static void geo_process_master_list(struct GraphNodeMasterList *node) {
    s32 i;
    UNUSED s32 sp1C;

    if (gCurGraphNodeMasterList == NULL && node->node.children != NULL) {
        gCurGraphNodeMasterList = node;
        for (i = 0; i < GFX_NUM_MASTER_LISTS; i++) {
            node->listHeads[i] = NULL;
        }
        geo_process_node_and_siblings(node->node.children);
        geo_process_master_list_sub(node);
        gCurGraphNodeMasterList = NULL;
    }
}

/**
 * Process an orthographic projection node.
 */
static void geo_process_ortho_projection(struct GraphNodeOrthoProjection *node) {
    if (node->node.children != NULL) {
        Mtx *mtx = alloc_display_list(sizeof(*mtx));
        f32 left = (gCurGraphNodeRoot->x - gCurGraphNodeRoot->width) / 2.0f * node->scale;
        f32 right = (gCurGraphNodeRoot->x + gCurGraphNodeRoot->width) / 2.0f * node->scale;
        f32 top = (gCurGraphNodeRoot->y - gCurGraphNodeRoot->height) / 2.0f * node->scale;
        f32 bottom = (gCurGraphNodeRoot->y + gCurGraphNodeRoot->height) / 2.0f * node->scale;

        guOrtho(mtx, left, right, bottom, top, -2.0f, 2.0f, 1.0f);
        gSPPerspNormalize(gDisplayListHead++, 0xFFFF);
        gSPMatrix(gDisplayListHead++, VIRTUAL_TO_PHYSICAL(mtx), G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);

        geo_process_node_and_siblings(node->node.children);
    }
}

/**
 * Process a perspective projection node.
 */
static void geo_process_perspective(struct GraphNodePerspective *node) {
    if (node->fnNode.func != NULL) {
        node->fnNode.func(GEO_CONTEXT_RENDER, &node->fnNode.node, gMatStack[gMatStackIndex]);
    }
    if (node->fnNode.node.children != NULL) {
        u16 perspNorm;
        Mtx *mtxInterpolated = alloc_display_list(sizeof(*mtxInterpolated));
        Mtx *mtx = alloc_display_list(sizeof(*mtx));
        f32 fovInterpolated;

#ifdef VERSION_EU
        f32 aspect = ((f32) gCurGraphNodeRoot->width / (f32) gCurGraphNodeRoot->height) * 1.1f;
#else
        f32 aspect = (f32) gCurGraphNodeRoot->width / (f32) gCurGraphNodeRoot->height;
#endif

        guPerspective(mtx, &perspNorm, node->fov, aspect, node->near, node->far, 1.0f);

        if (gGlobalTimer == node->prevTimestamp + 1 && gGlobalTimer != gLakituState.skipCameraInterpolationTimestamp) {

            fovInterpolated = (node->prevFov + node->fov) / 2.0f;
            guPerspective(mtxInterpolated, &perspNorm, fovInterpolated, aspect, node->near, node->far, 1.0f);
            gSPPerspNormalize(gDisplayListHead++, perspNorm);

            sPerspectivePos = gDisplayListHead;
            sPerspectiveMtx = mtx;
            gSPMatrix(gDisplayListHead++, VIRTUAL_TO_PHYSICAL(mtxInterpolated),
                      G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
        } else {
            gSPPerspNormalize(gDisplayListHead++, perspNorm);
            gSPMatrix(gDisplayListHead++, VIRTUAL_TO_PHYSICAL(mtx), G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
        }
        node->prevFov = node->fov;
        node->prevTimestamp = gGlobalTimer;

        gCurGraphNodeCamFrustum = node;
        geo_process_node_and_siblings(node->fnNode.node.children);
        gCurGraphNodeCamFrustum = NULL;
    }
}

/**
 * Process a level of detail node. From the current transformation matrix,
 * the perpendicular distance to the camera is extracted and the children
 * of this node are only processed if that distance is within the render
 * range of this node.
 */
static void geo_process_level_of_detail(struct GraphNodeLevelOfDetail *node) {
#ifdef GBI_FLOATS
    Mtx *mtx = gMatStackFixed[gMatStackIndex];
    s16 distanceFromCam = (s32) -mtx->m[3][2]; // z-component of the translation column
#else
    // The fixed point Mtx type is defined as 16 longs, but it's actually 16
    // shorts for the integer parts followed by 16 shorts for the fraction parts
    Mtx *mtx = gMatStackFixed[gMatStackIndex];
    s16 distanceFromCam = -GET_HIGH_S16_OF_32(mtx->m[1][3]); // z-component of the translation column
#endif

#ifndef TARGET_N64
    // We assume modern hardware is powerful enough to draw the most detailed variant
    distanceFromCam = 0;
#endif

    if (node->minDistance <= distanceFromCam && distanceFromCam < node->maxDistance) {
        if (node->node.children != 0) {
            geo_process_node_and_siblings(node->node.children);
        }
    }
}

/**
 * Process a switch case node. The node's selection function is called
 * if it is 0, and among the node's children, only the selected child is
 * processed next.
 */
static void geo_process_switch(struct GraphNodeSwitchCase *node) {
    struct GraphNode *selectedChild = node->fnNode.node.children;
    s32 i;

    if (node->fnNode.func != NULL) {
        node->fnNode.func(GEO_CONTEXT_RENDER, &node->fnNode.node, gMatStack[gMatStackIndex]);
    }
    for (i = 0; selectedChild != NULL && node->selectedCase > i; i++) {
        selectedChild = selectedChild->next;
    }
    if (selectedChild != NULL) {
        geo_process_node_and_siblings(selectedChild);
    }
}

void interpolate_vectors(Vec3f res, Vec3f a, Vec3f b) {
    res[0] = (a[0] + b[0]) / 2.0f;
    res[1] = (a[1] + b[1]) / 2.0f;
    res[2] = (a[2] + b[2]) / 2.0f;
}

void interpolate_vectors_s16(Vec3s res, Vec3s a, Vec3s b) {
    res[0] = (a[0] + b[0]) / 2;
    res[1] = (a[1] + b[1]) / 2;
    res[2] = (a[2] + b[2]) / 2;
}

static s16 interpolate_angle(s16 a, s16 b) {
    s32 absDiff = b - a;
    if (absDiff < 0) {
        absDiff = -absDiff;
    }
    if (absDiff >= 0x4000 && absDiff <= 0xC000) {
        return b;
    }
    if (absDiff <= 0x8000) {
        return (a + b) / 2;
    } else {
        return (a + b) / 2 + 0x8000;
    }
}

static void interpolate_angles(Vec3s res, Vec3s a, Vec3s b) {
    res[0] = interpolate_angle(a[0], b[0]);
    res[1] = interpolate_angle(a[1], b[1]);
    res[2] = interpolate_angle(a[2], b[2]);
}

/**
 * Process a camera node.
 */
static void geo_process_camera(struct GraphNodeCamera *node) {
    Mat4 cameraTransform;
    Mtx *rollMtx = alloc_display_list(sizeof(*rollMtx));
    Mtx *mtx = alloc_display_list(sizeof(*mtx));
    Mtx *mtxInterpolated = alloc_display_list(sizeof(*mtxInterpolated));
    Vec3f posInterpolated;
    Vec3f focusInterpolated;

    if (node->fnNode.func != NULL) {
        node->fnNode.func(GEO_CONTEXT_RENDER, &node->fnNode.node, gMatStack[gMatStackIndex]);
    }
    mtxf_rotate_xy(rollMtx, node->rollScreen);

    gSPMatrix(gDisplayListHead++, VIRTUAL_TO_PHYSICAL(rollMtx), G_MTX_PROJECTION | G_MTX_MUL | G_MTX_NOPUSH);

    mtxf_lookat(cameraTransform, node->pos, node->focus, node->roll);
    mtxf_mul(gMatStack[gMatStackIndex + 1], cameraTransform, gMatStack[gMatStackIndex]);

    if (gGlobalTimer == node->prevTimestamp + 1 && gGlobalTimer != gLakituState.skipCameraInterpolationTimestamp) {
        interpolate_vectors(posInterpolated, node->prevPos, node->pos);
        interpolate_vectors(focusInterpolated, node->prevFocus, node->focus);
        float magnitude = 0;
        for (int i = 0; i < 3; i++) {
            float diff = node->pos[i] - node->prevPos[i];
            magnitude += diff * diff;
        }
        if (magnitude > 500000) {
            // Observed ~479000 in BBH when toggling R camera
            // Can get over 3 million in VCUTM though...
            vec3f_copy(posInterpolated, node->pos);
            vec3f_copy(focusInterpolated, node->focus);
        }
    } else {
        vec3f_copy(posInterpolated, node->pos);
        vec3f_copy(focusInterpolated, node->focus);
    }
    vec3f_copy(node->prevPos, node->pos);
    vec3f_copy(node->prevFocus, node->focus);
    node->prevTimestamp = gGlobalTimer;
    vec3f_copy(node->posInterpolated, posInterpolated);
    vec3f_copy(node->focusInterpolated, focusInterpolated);
    mtxf_lookat(cameraTransform, posInterpolated, focusInterpolated, node->roll);
    mtxf_mul(gMatStackInterpolated[gMatStackIndex + 1], cameraTransform, gMatStackInterpolated[gMatStackIndex]);

    gMatStackIndex++;
    mtxf_to_mtx(mtx, gMatStack[gMatStackIndex]);
    gMatStackFixed[gMatStackIndex] = mtx;
    mtxf_to_mtx(mtxInterpolated, gMatStackInterpolated[gMatStackIndex]);
    gMatStackInterpolatedFixed[gMatStackIndex] = mtxInterpolated;
    if (node->fnNode.node.children != 0) {
        gCurGraphNodeCamera = node;
        node->matrixPtr = &gMatStack[gMatStackIndex];
        node->matrixPtrInterpolated = &gMatStackInterpolated[gMatStackIndex];
        geo_process_node_and_siblings(node->fnNode.node.children);
        gCurGraphNodeCamera = NULL;
    }
    gMatStackIndex--;
}

/**
 * Process a translation / rotation node. A transformation matrix based
 * on the node's translation and rotation is created and pushed on both
 * the float and fixed point matrix stacks.
 * For the rest it acts as a normal display list node.
 */
static void geo_process_translation_rotation(struct GraphNodeTranslationRotation *node) {
    Mat4 mtxf;
    Vec3f translation;
    Mtx *mtx = alloc_display_list(sizeof(*mtx));
    Mtx *mtxInterpolated = alloc_display_list(sizeof(*mtxInterpolated));

    vec3s_to_vec3f(translation, node->translation);
    mtxf_rotate_zxy_and_translate(mtxf, translation, node->rotation);
    mtxf_mul(gMatStack[gMatStackIndex + 1], mtxf, gMatStack[gMatStackIndex]);
    mtxf_mul(gMatStackInterpolated[gMatStackIndex + 1], mtxf, gMatStackInterpolated[gMatStackIndex]);
    gMatStackIndex++;
    mtxf_to_mtx(mtx, gMatStack[gMatStackIndex]);
    gMatStackFixed[gMatStackIndex] = mtx;
    mtxf_to_mtx(mtxInterpolated, gMatStackInterpolated[gMatStackIndex]);
    gMatStackInterpolatedFixed[gMatStackIndex] = mtxInterpolated;
    if (node->displayList != NULL) {
        geo_append_display_list(node->displayList, node->node.flags >> 8);
    }
    if (node->node.children != NULL) {
        geo_process_node_and_siblings(node->node.children);
    }
    gMatStackIndex--;
}

/**
 * Process a translation node. A transformation matrix based on the node's
 * translation is created and pushed on both the float and fixed point matrix stacks.
 * For the rest it acts as a normal display list node.
 */
static void geo_process_translation(struct GraphNodeTranslation *node) {
    Mat4 mtxf;
    Vec3f translation;
    Mtx *mtx = alloc_display_list(sizeof(*mtx));
    Mtx *mtxInterpolated = alloc_display_list(sizeof(*mtxInterpolated));

    vec3s_to_vec3f(translation, node->translation);
    mtxf_rotate_zxy_and_translate(mtxf, translation, gVec3sZero);
    mtxf_mul(gMatStack[gMatStackIndex + 1], mtxf, gMatStack[gMatStackIndex]);
    mtxf_mul(gMatStackInterpolated[gMatStackIndex + 1], mtxf, gMatStackInterpolated[gMatStackIndex]);
    gMatStackIndex++;
    mtxf_to_mtx(mtx, gMatStack[gMatStackIndex]);
    gMatStackFixed[gMatStackIndex] = mtx;
    mtxf_to_mtx(mtxInterpolated, gMatStackInterpolated[gMatStackIndex]);
    gMatStackInterpolatedFixed[gMatStackIndex] = mtxInterpolated;
    if (node->displayList != NULL) {
        geo_append_display_list(node->displayList, node->node.flags >> 8);
    }
    if (node->node.children != NULL) {
        geo_process_node_and_siblings(node->node.children);
    }
    gMatStackIndex--;
}

/**
 * Process a rotation node. A transformation matrix based on the node's
 * rotation is created and pushed on both the float and fixed point matrix stacks.
 * For the rest it acts as a normal display list node.
 */
static void geo_process_rotation(struct GraphNodeRotation *node) {
    Mat4 mtxf;
    Mtx *mtx = alloc_display_list(sizeof(*mtx));
    Mtx *mtxInterpolated = alloc_display_list(sizeof(*mtxInterpolated));
    Vec3s rotationInterpolated;

    mtxf_rotate_zxy_and_translate(mtxf, gVec3fZero, node->rotation);
    mtxf_mul(gMatStack[gMatStackIndex + 1], mtxf, gMatStack[gMatStackIndex]);
    if (gGlobalTimer == node->prevTimestamp + 1) {
        interpolate_angles(rotationInterpolated, node->prevRotation, node->rotation);
        mtxf_rotate_zxy_and_translate(mtxf, gVec3fZero, rotationInterpolated);
    }
    vec3s_copy(node->prevRotation, node->rotation);
    node->prevTimestamp = gGlobalTimer;
    mtxf_mul(gMatStackInterpolated[gMatStackIndex + 1], mtxf, gMatStackInterpolated[gMatStackIndex]);
    gMatStackIndex++;
    mtxf_to_mtx(mtx, gMatStack[gMatStackIndex]);
    gMatStackFixed[gMatStackIndex] = mtx;
    mtxf_to_mtx(mtxInterpolated, gMatStackInterpolated[gMatStackIndex]);
    gMatStackInterpolatedFixed[gMatStackIndex] = mtxInterpolated;
    if (node->displayList != NULL) {
        geo_append_display_list(node->displayList, node->node.flags >> 8);
    }
    if (node->node.children != NULL) {
        geo_process_node_and_siblings(node->node.children);
    }
    gMatStackIndex--;
}

/**
 * Process a scaling node. A transformation matrix based on the node's
 * scale is created and pushed on both the float and fixed point matrix stacks.
 * For the rest it acts as a normal display list node.
 */
static void geo_process_scale(struct GraphNodeScale *node) {
    UNUSED Mat4 transform;
    Vec3f scaleVec;
    Mtx *mtx = alloc_display_list(sizeof(*mtx));
    Mtx *mtxInterpolated = alloc_display_list(sizeof(*mtxInterpolated));
#ifdef TARGET_N3DS
    f32 savedDynamicShadowBillboardGeoScale = sDynamicShadowBillboardGeoScale;
#endif

    vec3f_set(scaleVec, node->scale, node->scale, node->scale);
    mtxf_scale_vec3f(gMatStack[gMatStackIndex + 1], gMatStack[gMatStackIndex], scaleVec);
    mtxf_scale_vec3f(gMatStackInterpolated[gMatStackIndex + 1], gMatStackInterpolated[gMatStackIndex], scaleVec);
#ifdef TARGET_N3DS
    if (sAppendingDynamicShadow) {
        sDynamicShadowBillboardGeoScale *= node->scale;
    }
#endif
    gMatStackIndex++;
    mtxf_to_mtx(mtx, gMatStack[gMatStackIndex]);
    gMatStackFixed[gMatStackIndex] = mtx;
    mtxf_to_mtx(mtxInterpolated, gMatStackInterpolated[gMatStackIndex]);
    gMatStackInterpolatedFixed[gMatStackIndex] = mtxInterpolated;
    if (node->displayList != NULL) {
        geo_append_display_list(node->displayList, node->node.flags >> 8);
    }
    if (node->node.children != NULL) {
        geo_process_node_and_siblings(node->node.children);
    }
    gMatStackIndex--;
#ifdef TARGET_N3DS
    sDynamicShadowBillboardGeoScale = savedDynamicShadowBillboardGeoScale;
#endif
}

/**
 * Process a billboard node. A transformation matrix is created that makes its
 * children face the camera, and it is pushed on the floating point and fixed
 * point matrix stacks.
 * For the rest it acts as a normal display list node.
 */
static void geo_process_billboard(struct GraphNodeBillboard *node) {
    Vec3f translation;
    Mtx *mtx;
    Mtx *mtxInterpolated;

#ifdef TARGET_N3DS
    // Billboard body during dynamic shadow pass: use the projected matrix only
    // to find where the billboard center lands, then draw an independent circle
    // aligned to the receiver surface so projection angle cannot distort it.
    if (sAppendingDynamicShadow) {
        Gfx *circleList;
        Vec3f circleCenter;

        mtx = alloc_display_list(sizeof(*mtx));
        mtxInterpolated = alloc_display_list(sizeof(*mtxInterpolated));
        if (mtx == NULL || mtxInterpolated == NULL) {
            return;
        }

        gMatStackIndex++;
        vec3s_to_vec3f(translation, node->translation);
        mtxf_rotate_zxy_and_translate(gMatStack[gMatStackIndex], translation, gVec3sZero);
        mtxf_mul(gMatStack[gMatStackIndex], gMatStack[gMatStackIndex],
                 gMatStack[gMatStackIndex - 1]);
        mtxf_rotate_zxy_and_translate(gMatStackInterpolated[gMatStackIndex], translation,
                                      gVec3sZero);
        mtxf_mul(gMatStackInterpolated[gMatStackIndex],
                 gMatStackInterpolated[gMatStackIndex], gMatStackInterpolated[gMatStackIndex - 1]);
        mtxf_to_mtx(mtx, gMatStack[gMatStackIndex]);
        gMatStackFixed[gMatStackIndex] = mtx;
        mtxf_to_mtx(mtxInterpolated, gMatStackInterpolated[gMatStackIndex]);
        gMatStackInterpolatedFixed[gMatStackIndex] = mtxInterpolated;

        circleList = dynamic_shadow_create_billboard_circle(node, circleCenter);
        if (!dynamic_shadow_should_skip_billboard_circle(node, circleCenter)) {
            dynamic_shadow_append_circle_on_receiver(circleList, circleCenter);
        }
        gMatStackIndex--;
        return;
    }

    // Other camera-facing quads (eyes, hands, decorations) do not have a
    // meaningful world-space silhouette. Keep them out of projected shadows.
    if (sAppendingDynamicShadow && !dynamic_shadow_allows_shadow_alpha_layer()) {
        return;
    }
#endif

    mtx = alloc_display_list(sizeof(*mtx));
    mtxInterpolated = alloc_display_list(sizeof(*mtxInterpolated));
    gMatStackIndex++;
    vec3s_to_vec3f(translation, node->translation);
    mtxf_billboard(gMatStack[gMatStackIndex], gMatStack[gMatStackIndex - 1], translation,
                   gCurGraphNodeCamera->roll);
    mtxf_billboard(gMatStackInterpolated[gMatStackIndex], gMatStackInterpolated[gMatStackIndex - 1], translation,
                   gCurGraphNodeCamera->roll);
    if (gCurGraphNodeHeldObject != NULL) {
        mtxf_scale_vec3f(gMatStack[gMatStackIndex], gMatStack[gMatStackIndex],
                         gCurGraphNodeHeldObject->objNode->header.gfx.scale);
        mtxf_scale_vec3f(gMatStackInterpolated[gMatStackIndex], gMatStackInterpolated[gMatStackIndex],
                         gCurGraphNodeHeldObject->objNode->header.gfx.scale);
    } else if (gCurGraphNodeObject != NULL) {
        mtxf_scale_vec3f(gMatStack[gMatStackIndex], gMatStack[gMatStackIndex],
                         gCurGraphNodeObject->scale);
        mtxf_scale_vec3f(gMatStackInterpolated[gMatStackIndex], gMatStackInterpolated[gMatStackIndex],
                         gCurGraphNodeObject->scale);
    }

    mtxf_to_mtx(mtx, gMatStack[gMatStackIndex]);
    gMatStackFixed[gMatStackIndex] = mtx;
    mtxf_to_mtx(mtxInterpolated, gMatStackInterpolated[gMatStackIndex]);
    gMatStackInterpolatedFixed[gMatStackIndex] = mtxInterpolated;
    if (node->displayList != NULL) {
        geo_append_display_list(node->displayList, node->node.flags >> 8);
    }
    if (node->node.children != NULL) {
        geo_process_node_and_siblings(node->node.children);
    }
    gMatStackIndex--;
}

/**
 * Process a display list node. It draws a display list without first pushing
 * a transformation on the stack, so all transformations are inherited from the
 * parent node. It processes its children if it has them.
 */
static void geo_process_display_list(struct GraphNodeDisplayList *node) {
    if (node->displayList != NULL) {
        geo_append_display_list(node->displayList, node->node.flags >> 8);
    }
    if (node->node.children != NULL) {
        geo_process_node_and_siblings(node->node.children);
    }
}

/**
 * Process a generated list. Instead of storing a pointer to a display list,
 * the list is generated on the fly by a function.
 */
static void geo_process_generated_list(struct GraphNodeGenerated *node) {
    if (node->fnNode.func != NULL) {
        Gfx *list = node->fnNode.func(GEO_CONTEXT_RENDER, &node->fnNode.node,
                                     (struct AllocOnlyPool *) gMatStack[gMatStackIndex]);

#ifdef TARGET_N3DS
        if (sAppendingDynamicShadow && node->fnNode.func != (GraphNodeFunc) death_ragdoll_geo_render) {
            list = NULL;
        }
#endif
        if (list != 0) {
            geo_append_display_list((void *) VIRTUAL_TO_PHYSICAL(list), node->fnNode.node.flags >> 8);
        }
    }
    if (node->fnNode.node.children != NULL) {
        geo_process_node_and_siblings(node->fnNode.node.children);
    }
}

/**
 * Process a background node. Tries to retrieve a background display list from
 * the function of the node. If that function is null or returns null, a black
 * rectangle is drawn instead.
 */
static void geo_process_background(struct GraphNodeBackground *node) {
    Gfx *list = NULL;
    Gfx *listInterpolated = NULL;

    if (node->fnNode.func != NULL) {
        Vec3f posCopy;
        Vec3f focusCopy;
        Vec3f posInterpolated;
        Vec3f focusInterpolated;

        if (gGlobalTimer == node->prevCameraTimestamp + 1 &&
            gGlobalTimer != gLakituState.skipCameraInterpolationTimestamp) {
            interpolate_vectors(posInterpolated, node->prevCameraPos, gLakituState.pos);
            interpolate_vectors(focusInterpolated, node->prevCameraFocus, gLakituState.focus);
        } else {
            vec3f_copy(posInterpolated, gLakituState.pos);
            vec3f_copy(focusInterpolated, gLakituState.focus);
        }
        vec3f_copy(node->prevCameraPos, gLakituState.pos);
        vec3f_copy(node->prevCameraFocus, gLakituState.focus);
        node->prevCameraTimestamp = gGlobalTimer;

        list = node->fnNode.func(GEO_CONTEXT_RENDER, &node->fnNode.node,
                                 (struct AllocOnlyPool *) gMatStack[gMatStackIndex]);
        vec3f_copy(posCopy, gLakituState.pos);
        vec3f_copy(focusCopy, gLakituState.focus);
        vec3f_copy(gLakituState.pos, posInterpolated);
        vec3f_copy(gLakituState.focus, focusInterpolated);
        listInterpolated = node->fnNode.func(GEO_CONTEXT_RENDER, &node->fnNode.node, NULL);
        vec3f_copy(gLakituState.pos, posCopy);
        vec3f_copy(gLakituState.focus, focusCopy);
    }
    if (list != 0) {
        geo_append_display_list2((void *) VIRTUAL_TO_PHYSICAL(list),
                                 (void *) VIRTUAL_TO_PHYSICAL(listInterpolated), node->fnNode.node.flags >> 8);
    } else if (gCurGraphNodeMasterList != NULL) {
#ifdef TARGET_N3DS
        Gfx *gfxStart = alloc_display_list(sizeof(Gfx) * 12);
#else
        Gfx *gfxStart = alloc_display_list(sizeof(Gfx) * 8);
#endif
        Gfx *gfx = gfxStart;

        gDPPipeSync(gfx++);
        gDPSetCycleType(gfx++, G_CYC_FILL);
        gDPSetFillColor(gfx++, node->background);
#ifdef TARGET_N3DS
        gDPForceFlush(gfx++);
        gDPSet2d(gfx++, 1);
#endif
        gDPFillRectangle(gfx++, GFX_DIMENSIONS_RECT_FROM_LEFT_EDGE(0), BORDER_HEIGHT,
        GFX_DIMENSIONS_RECT_FROM_RIGHT_EDGE(0) - 1, SCREEN_HEIGHT - BORDER_HEIGHT - 1);
#ifdef TARGET_N3DS
        gDPForceFlush(gfx++);
        gDPSet2d(gfx++, 0);
#endif
        gDPPipeSync(gfx++);
        gDPSetCycleType(gfx++, G_CYC_1CYCLE);
        gSPEndDisplayList(gfx++);

        geo_append_display_list((void *) VIRTUAL_TO_PHYSICAL(gfxStart), 0);
    }
    if (node->fnNode.node.children != NULL) {
        geo_process_node_and_siblings(node->fnNode.node.children);
    }
}

static void anim_process(Vec3f translation, Vec3s rotation, u8 *animType, s16 animFrame, u16 **animAttribute) {
    if (*animType == ANIM_TYPE_TRANSLATION) {
        translation[0] += gCurAnimData[retrieve_animation_index(animFrame, animAttribute)]
                          * gCurAnimTranslationMultiplier;
        translation[1] += gCurAnimData[retrieve_animation_index(animFrame, animAttribute)]
                          * gCurAnimTranslationMultiplier;
        translation[2] += gCurAnimData[retrieve_animation_index(animFrame, animAttribute)]
                          * gCurAnimTranslationMultiplier;
        *animType = ANIM_TYPE_ROTATION;
    } else {
        if (*animType == ANIM_TYPE_LATERAL_TRANSLATION) {
            translation[0] +=
                gCurAnimData[retrieve_animation_index(animFrame, animAttribute)]
                * gCurAnimTranslationMultiplier;
            *animAttribute += 2;
            translation[2] +=
                gCurAnimData[retrieve_animation_index(animFrame, animAttribute)]
                * gCurAnimTranslationMultiplier;
            *animType = ANIM_TYPE_ROTATION;
        } else {
            if (*animType == ANIM_TYPE_VERTICAL_TRANSLATION) {
                *animAttribute += 2;
                translation[1] +=
                    gCurAnimData[retrieve_animation_index(animFrame, animAttribute)]
                    * gCurAnimTranslationMultiplier;
                *animAttribute += 2;
                *animType = ANIM_TYPE_ROTATION;
            } else if (*animType == ANIM_TYPE_NO_TRANSLATION) {
                *animAttribute += 6;
                *animType = ANIM_TYPE_ROTATION;
            }
        }
    }

    if (*animType == ANIM_TYPE_ROTATION) {
        rotation[0] = gCurAnimData[retrieve_animation_index(animFrame, animAttribute)];
        rotation[1] = gCurAnimData[retrieve_animation_index(animFrame, animAttribute)];
        rotation[2] = gCurAnimData[retrieve_animation_index(animFrame, animAttribute)];
    }
}

/**
 * Render an animated part. The current animation state is not part of the node
 * but set in global variables. If an animated part is skipped, everything afterwards desyncs.
 */
static void geo_process_animated_part(struct GraphNodeAnimatedPart *node) {
    Mat4 matrix;
    Vec3s rotation;
    Vec3f translation;
    Vec3s rotationInterpolated;
    Vec3f translationInterpolated;
    Mtx *matrixPtr = alloc_display_list(sizeof(*matrixPtr));
    Mtx *mtxInterpolated = alloc_display_list(sizeof(*mtxInterpolated));
    u16 *animAttribute = gCurrAnimAttribute;
    u8 animType = gCurAnimType;
#ifdef TARGET_N3DS
    u8 skipChuckyaFaceLayer = FALSE;
#endif

    vec3s_copy(rotation, gVec3sZero);
    vec3f_set(translation, node->translation[0], node->translation[1], node->translation[2]);
    vec3s_copy(rotationInterpolated, rotation);
    vec3f_copy(translationInterpolated, translation);

    anim_process(translationInterpolated, rotationInterpolated, &animType, gPrevAnimFrame, &animAttribute);
    anim_process(translation, rotation, &gCurAnimType, gCurrAnimFrame, &gCurrAnimAttribute);
    interpolate_vectors(translationInterpolated, translationInterpolated, translation);
    interpolate_angles(rotationInterpolated, rotationInterpolated, rotation);

    mtxf_rotate_xyz_and_translate(matrix, translation, rotation);
    mtxf_mul(gMatStack[gMatStackIndex + 1], matrix, gMatStack[gMatStackIndex]);
    mtxf_rotate_xyz_and_translate(matrix, translationInterpolated, rotationInterpolated);
    mtxf_mul(gMatStackInterpolated[gMatStackIndex + 1], matrix, gMatStackInterpolated[gMatStackIndex]);
    gMatStackIndex++;
    mtxf_to_mtx(matrixPtr, gMatStack[gMatStackIndex]);
    gMatStackFixed[gMatStackIndex] = matrixPtr;
    mtxf_to_mtx(mtxInterpolated, gMatStackInterpolated[gMatStackIndex]);
    gMatStackInterpolatedFixed[gMatStackIndex] = mtxInterpolated;
    if (node->displayList != NULL) {
#ifdef TARGET_N3DS
        skipChuckyaFaceLayer =
            sAppendingDynamicShadow
            && dynamic_shadow_behavior_is(dynamic_shadow_current_behavior(), bhvChuckya)
            && dynamic_shadow_gfx_arg_to_virtual((uintptr_t) node->displayList)
                == dynamic_shadow_gfx_arg_to_virtual((uintptr_t) chuckya_seg8_dl_0800A758);
        if (!skipChuckyaFaceLayer)
#endif
        geo_append_display_list(node->displayList, node->node.flags >> 8);
    }
    if (node->node.children != NULL) {
        geo_process_node_and_siblings(node->node.children);
    }
    gMatStackIndex--;
}

#ifdef TARGET_N3DS
static void dynamic_shadow_make_projection_mtx(Mat4 dest, struct FloorGeometry *floorGeo,
                                               const struct DynamicShadowLight *light) {
    f32 lift = light->lift > 0.12f ? 0.12f : light->lift;
    f32 dirX = -sins(light->yaw) * light->length;
    f32 dirY = 1.0f;
    f32 dirZ = -coss(light->yaw) * light->length;
    f32 normalX = floorGeo->normalX;
    f32 normalY = floorGeo->normalY;
    f32 normalZ = floorGeo->normalZ;
    f32 originOffset = floorGeo->originOffset - lift;
    f32 normalDotDir = normalX * dirX + normalY * dirY + normalZ * dirZ;

    mtxf_identity(dest);
    if (normalDotDir > -0.05f && normalDotDir < 0.05f) {
        normalDotDir = normalDotDir < 0.0f ? -0.05f : 0.05f;
    }

    dest[0][0] = 1.0f - dirX * normalX / normalDotDir;
    dest[1][0] = -dirX * normalY / normalDotDir;
    dest[2][0] = -dirX * normalZ / normalDotDir;
    dest[3][0] = -dirX * originOffset / normalDotDir;

    dest[0][1] = -dirY * normalX / normalDotDir;
    dest[1][1] = 1.0f - dirY * normalY / normalDotDir;
    dest[2][1] = -dirY * normalZ / normalDotDir;
    dest[3][1] = -dirY * originOffset / normalDotDir;

    dest[0][2] = -dirZ * normalX / normalDotDir;
    dest[1][2] = -dirZ * normalY / normalDotDir;
    dest[2][2] = 1.0f - dirZ * normalZ / normalDotDir;
    dest[3][2] = -dirZ * originOffset / normalDotDir;
}

static void dynamic_shadow_make_camera_inv_mtx(Mat4 dest, Mat4 viewMtx, Vec3f cameraPos) {
    dest[0][0] = viewMtx[0][0];
    dest[0][1] = viewMtx[1][0];
    dest[0][2] = viewMtx[2][0];
    dest[0][3] = 0.0f;

    dest[1][0] = viewMtx[0][1];
    dest[1][1] = viewMtx[1][1];
    dest[1][2] = viewMtx[2][1];
    dest[1][3] = 0.0f;

    dest[2][0] = viewMtx[0][2];
    dest[2][1] = viewMtx[1][2];
    dest[2][2] = viewMtx[2][2];
    dest[2][3] = 0.0f;

    dest[3][0] = cameraPos[0];
    dest[3][1] = cameraPos[1];
    dest[3][2] = cameraPos[2];
    dest[3][3] = 1.0f;
}

static void dynamic_shadow_make_ortho_mtx(Mat4 dest, f32 left, f32 right, f32 bottom, f32 top, f32 near, f32 far) {
    mtxf_identity(dest);
    dest[0][0] = 2.0f / (right - left);
    dest[1][1] = 2.0f / (top - bottom);
    dest[2][2] = -2.0f / (far - near);
    dest[3][0] = -(right + left) / (right - left);
    dest[3][1] = -(top + bottom) / (top - bottom);
    dest[3][2] = -(far + near) / (far - near);
}

static void dynamic_shadow_update_light_matrices(void) {
    Vec3f center;
    Vec3f lightPos;
    Vec3f up;
    Mat4 lightView;
    Mat4 lightProj;
    const struct DynamicShadowLight *light;
    f32 horizontalDist;
    f32 verticalDist;
    f32 range = 5200.0f;

    gDynamicShadowLightMtxReady = FALSE;
    if (gCurGraphNodeCamera == NULL || gMarioState == NULL || gMarioObject == NULL) {
        return;
    }

    vec3f_copy(center, gMarioObject->header.gfx.pos);
    center[1] += 500.0f;
    light = dynamic_shadows_get_light();

    horizontalDist = 2800.0f + light->length * 2600.0f;
    verticalDist = 5400.0f - light->length * 1700.0f;
    lightPos[0] = center[0] + sins(light->yaw) * horizontalDist;
    lightPos[1] = center[1] + verticalDist;
    lightPos[2] = center[2] + coss(light->yaw) * horizontalDist;
    vec3f_set(up, 0.0f, 1.0f, 0.0f);

    dynamic_shadow_make_camera_inv_mtx(gDynamicShadowCameraInvMtx,
                                       *gCurGraphNodeCamera->matrixPtrInterpolated,
                                       gCurGraphNodeCamera->posInterpolated);
    mtxf_lookat(lightView, lightPos, center, 0);
    dynamic_shadow_make_ortho_mtx(lightProj, -range, range, -range, range, 100.0f, 12000.0f);
    mtxf_mul(gDynamicShadowLightVpMtx, lightView, lightProj);
    gDynamicShadowLightMtxReady = TRUE;
}

static u8 dynamic_shadow_is_mario_object(void) {
    return gCurGraphNodeObject != NULL
        && gMarioObject != NULL
        && (struct Object *) gCurGraphNodeObject == gMarioObject;
}

static u8 dynamic_shadow_is_ending_peach_cutscene_actor(void) {
    const BehaviorScript *behavior = dynamic_shadow_current_behavior();

    if (gMarioState == NULL || gMarioState->action != ACT_END_PEACH_CUTSCENE) {
        return FALSE;
    }

    return dynamic_shadow_is_mario_object()
        || dynamic_shadow_behavior_is(behavior, bhvEndPeach)
        || dynamic_shadow_behavior_is(behavior, bhvEndToad);
}

static u8 dynamic_shadow_should_disable_ending_actor_dynamic_shadow(void) {
    const BehaviorScript *behavior = dynamic_shadow_current_behavior();

    if (gMarioState == NULL || gMarioState->action != ACT_END_PEACH_CUTSCENE) {
        return FALSE;
    }

    return !dynamic_shadow_is_mario_object()
        && (dynamic_shadow_behavior_is(behavior, bhvEndPeach)
            || dynamic_shadow_behavior_is(behavior, bhvEndToad));
}

static u8 dynamic_shadow_should_hide_ending_peach_blob(void) {
    return gMarioState != NULL
        && gMarioState->action == ACT_END_PEACH_CUTSCENE
        && dynamic_shadow_behavior_is(dynamic_shadow_current_behavior(), bhvEndPeach);
}

static void dynamic_shadow_sync_ending_cutscene_object_pos(struct Object *obj) {
    if (gMarioState == NULL || obj == NULL || obj == gMarioObject) {
        return;
    }

    if (gMarioState->action != ACT_CREDITS_CUTSCENE) {
        return;
    }

    obj->header.gfx.pos[0] = obj->oPosX;
    obj->header.gfx.pos[1] = obj->oPosY + obj->oGraphYOffset;
    obj->header.gfx.pos[2] = obj->oPosZ;
}

static f32 dynamic_shadow_dist_sq_3f(Vec3f a, Vec3f b) {
    f32 dx = a[0] - b[0];
    f32 dy = a[1] - b[1];
    f32 dz = a[2] - b[2];
    return dx * dx + dy * dy + dz * dz;
}

static f32 dynamic_shadow_dist_sq_xz(Vec3f a, Vec3f b) {
    f32 dx = a[0] - b[0];
    f32 dz = a[2] - b[2];
    return dx * dx + dz * dz;
}

static s16 dynamic_shadow_get_object_culling_radius(void) {
    struct GraphNode *geo;
    struct Object *obj;
    f32 radius;

    if (gCurGraphNodeObject == NULL) {
        return 300;
    }

    obj = (struct Object *) gCurGraphNodeObject;
    if (obj->behavior == segmented_to_virtual(bhvSmallWhomp)
        || obj->behavior == segmented_to_virtual(bhvWhompKingBoss)) {
        radius = 900.0f * obj->header.gfx.scale[0];
        if (radius < 900.0f) {
            radius = 900.0f;
        }
        if (radius > 1800.0f) {
            radius = 1800.0f;
        }
        return (s16) radius;
    }
    if (obj->behavior == segmented_to_virtual(bhvThwomp)
        || obj->behavior == segmented_to_virtual(bhvThwomp2)) {
        radius = 900.0f * obj->header.gfx.scale[0];
        if (radius < 900.0f) {
            radius = 900.0f;
        }
        if (radius > 1800.0f) {
            radius = 1800.0f;
        }
        return (s16) radius;
    }
    if (obj->behavior == segmented_to_virtual(bhvToxBox)
        || obj->behavior == segmented_to_virtual(bhvGrindel)
        || obj->behavior == segmented_to_virtual(bhvHorizontalGrindel)
        || obj->behavior == segmented_to_virtual(bhvSpindel)) {
        radius = 760.0f * obj->header.gfx.scale[0];
        if (radius < 760.0f) {
            radius = 760.0f;
        }
        if (radius > 1600.0f) {
            radius = 1600.0f;
        }
        return (s16) radius;
    }
    if (obj->behavior == segmented_to_virtual(bhvWoodenPost)) {
        radius = 500.0f * obj->header.gfx.scale[0];
        if (radius < 500.0f) {
            radius = 500.0f;
        }
        if (radius > 1000.0f) {
            radius = 1000.0f;
        }
        return (s16) radius;
    }
    if (obj->behavior == segmented_to_virtual(bhvSwingPlatform)
        || obj->behavior == segmented_to_virtual(bhvDonutPlatform)
        || obj->behavior == segmented_to_virtual(bhvRrElevatorPlatform)
        || obj->behavior == segmented_to_virtual(bhvRrRotatingBridgePlatform)
        || obj->behavior == segmented_to_virtual(bhvSlidingPlatform2)
        || obj->behavior == segmented_to_virtual(bhvPlatformOnTrack)
        || obj->behavior == segmented_to_virtual(bhvFallingBowserPlatform)) {
        radius = 980.0f * obj->header.gfx.scale[0];
        if (radius < 980.0f) {
            radius = 980.0f;
        }
        if (radius > 1900.0f) {
            radius = 1900.0f;
        }
        return (s16) radius;
    }

    geo = gCurGraphNodeObject->sharedChild;
    if (geo != NULL && geo->type == GRAPH_NODE_TYPE_CULLING_RADIUS) {
        return ((struct GraphNodeCullingRadius *) geo)->cullingRadius;
    }

    return 300;
}

static u8 dynamic_shadow_object_is_clear_on_screen(void) {
    s16 cullingRadius;
    s16 halfFov;
    f32 hScreenEdge;
    f32 depth;
    f32 screenRatio;

    if (dynamic_shadow_is_mario_object()) {
        return TRUE;
    }
    if (gCurGraphNodeCamFrustum == NULL || gCurGraphNodeObject == NULL) {
        return FALSE;
    }

    cullingRadius = dynamic_shadow_get_object_culling_radius();
    depth = -gCurGraphNodeObject->cameraToObject[2];
    if (depth < -cullingRadius || depth > 7600.0f + cullingRadius) {
        return FALSE;
    }
    if (depth < 120.0f) {
        depth = 120.0f;
    }

    halfFov = (gCurGraphNodeCamFrustum->fov / 2.0f + 1.0f) * 32768.0f / 180.0f + 0.5f;
    hScreenEdge = depth * sins(halfFov) / coss(halfFov);
#ifdef WIDESCREEN
    hScreenEdge *= GFX_DIMENSIONS_ASPECT_RATIO;
#endif
    if (gCurGraphNodeObject->cameraToObject[0] > hScreenEdge + cullingRadius) {
        return FALSE;
    }
    if (gCurGraphNodeObject->cameraToObject[0] < -hScreenEdge - cullingRadius) {
        return FALSE;
    }

    // Tiny distant objects are hard to read and cost more than they add.
    screenRatio = cullingRadius / depth;
    return screenRatio > 0.030f;
}

static u8 dynamic_shadow_is_bobomb_like(void) {
    return FALSE;
}

static u8 dynamic_shadow_is_cannon_base_object(void) {
    const BehaviorScript *behavior = dynamic_shadow_current_behavior();

    if (behavior == NULL) {
        return FALSE;
    }

    return dynamic_shadow_behavior_is(behavior, bhvCannon)
        || dynamic_shadow_behavior_is(behavior, bhvWaterBombCannon)
        || dynamic_shadow_behavior_is(behavior, bhvCannonBaseUnused);
}

static u8 dynamic_shadow_is_cannon_barrel_object(void) {
    const BehaviorScript *behavior = dynamic_shadow_current_behavior();

    if (behavior == NULL) {
        return FALSE;
    }

    return dynamic_shadow_behavior_is(behavior, bhvCannonBarrel)
        || dynamic_shadow_behavior_is(behavior, bhvCannonBarrelBubbles);
}

static const BehaviorScript *dynamic_shadow_current_behavior(void) {
    if (gCurGraphNodeObject == NULL) {
        return NULL;
    }

    return ((struct Object *) gCurGraphNodeObject)->behavior;
}

static u8 dynamic_shadow_behavior_is(const BehaviorScript *behavior,
                                     const BehaviorScript *candidate) {
    return behavior == segmented_to_virtual(candidate);
}

static u8 dynamic_shadow_is_water_surface_effect(void) {
    const BehaviorScript *behavior = dynamic_shadow_current_behavior();

    if (behavior == NULL) {
        return FALSE;
    }

    // Keep ordinary particles and droplets. These flat water-surface effects
    // are the expensive casters that create long black strips while swimming.
    return dynamic_shadow_behavior_is(behavior, bhvWaterSplash)
        || dynamic_shadow_behavior_is(behavior, bhvWaterDropletSplash)
        || dynamic_shadow_behavior_is(behavior, bhvBubbleSplash)
        || dynamic_shadow_behavior_is(behavior, bhvIdleWaterWave)
        || dynamic_shadow_behavior_is(behavior, bhvObjectWaterSplash)
        || dynamic_shadow_behavior_is(behavior, bhvShallowWaterWave)
        || dynamic_shadow_behavior_is(behavior, bhvShallowWaterSplash)
        || dynamic_shadow_behavior_is(behavior, bhvObjectWaveTrail)
        || dynamic_shadow_behavior_is(behavior, bhvWaveTrail)
        || dynamic_shadow_behavior_is(behavior, bhvObjectWaterWave)
        || dynamic_shadow_behavior_is(behavior, bhvSkeeterWave);
}

static u8 dynamic_shadow_is_coin(void) {
    const BehaviorScript *behavior = dynamic_shadow_current_behavior();

    if (behavior == NULL) {
        return FALSE;
    }

    return dynamic_shadow_behavior_is(behavior, bhvMrIBlueCoin)
        || dynamic_shadow_behavior_is(behavior, bhvCoinInsideBoo)
        || dynamic_shadow_behavior_is(behavior, bhvOneCoin)
        || dynamic_shadow_behavior_is(behavior, bhvYellowCoin)
        || dynamic_shadow_behavior_is(behavior, bhvTemporaryYellowCoin)
        || dynamic_shadow_behavior_is(behavior, bhvSingleCoinGetsSpawned)
        || dynamic_shadow_behavior_is(behavior, bhvMovingYellowCoin)
        || dynamic_shadow_behavior_is(behavior, bhvMovingBlueCoin)
        || dynamic_shadow_behavior_is(behavior, bhvBlueCoinSliding)
        || dynamic_shadow_behavior_is(behavior, bhvBlueCoinJumping)
        || dynamic_shadow_behavior_is(behavior, bhvRedCoin);
}

static u8 dynamic_shadow_should_skip_billboard_circle(struct GraphNodeBillboard *node,
                                                      Vec3f circleCenter) {
    const BehaviorScript *behavior = dynamic_shadow_current_behavior();

    (void) circleCenter;

    if (gCurGraphNodeObject == NULL || behavior == NULL) {
        return FALSE;
    }

    if (!dynamic_shadow_behavior_is(behavior, bhvWigglerHead)
        && !dynamic_shadow_behavior_is(behavior, bhvWigglerBody)) {
        return FALSE;
    }

    return !dynamic_shadow_node_contains_display_list(&node->node, wiggler_seg5_dl_0500C278);
}

static u8 dynamic_shadow_allows_billboard_caster(void) {
    const BehaviorScript *behavior = dynamic_shadow_current_behavior();

    if (behavior == NULL) {
        return FALSE;
    }

    // Billboard actors: their body is billboard (camera-facing) but limbs are
    // modeled.  We allow them through the billboard gate so that the billboard
    // body is replaced with a projected circle and the modeled limbs keep their
    // dynamic projected shadow.
    return dynamic_shadow_behavior_is(behavior, bhvButterfly)
        || dynamic_shadow_behavior_is(behavior, bhvTripletButterfly)
        || dynamic_shadow_behavior_is(behavior, bhvScuttlebug)
        || dynamic_shadow_behavior_is(behavior, bhvSkeeter)
        || dynamic_shadow_behavior_is(behavior, bhvHomingAmp)
        || dynamic_shadow_behavior_is(behavior, bhvCirclingAmp)
        || dynamic_shadow_behavior_is(behavior, bhvSeaweed)
        || dynamic_shadow_behavior_is(behavior, bhvSeaweedBundle)
        || dynamic_shadow_behavior_is(behavior, bhvKingBobomb)
        || dynamic_shadow_behavior_is(behavior, bhvSmallBully)
        || dynamic_shadow_behavior_is(behavior, bhvBigBully)
        || dynamic_shadow_behavior_is(behavior, bhvBigChillBully)
        || dynamic_shadow_behavior_is(behavior, bhvBigBullyWithMinions)
        || dynamic_shadow_behavior_is(behavior, bhvSwoop)
        || dynamic_shadow_behavior_is(behavior, bhvChainChomp)
        || dynamic_shadow_behavior_is(behavior, bhvBobomb)
        || dynamic_shadow_behavior_is(behavior, bhvBobombBuddy)
        || dynamic_shadow_behavior_is(behavior, bhvBobombBuddyOpensCannon)
        || dynamic_shadow_behavior_is(behavior, bhvChuckya)
        || dynamic_shadow_behavior_is(behavior, bhvHeaveHo)
        || dynamic_shadow_behavior_is(behavior, bhvSnufit)
        || dynamic_shadow_behavior_is(behavior, bhvMrBlizzard)
        || dynamic_shadow_behavior_is(behavior, bhvSnowmansBottom)
        || dynamic_shadow_behavior_is(behavior, bhvSnowmansHead)
        || dynamic_shadow_behavior_is(behavior, bhvBubba)
        || dynamic_shadow_behavior_is(behavior, bhvPiranhaPlant)
        || dynamic_shadow_behavior_is(behavior, bhvFirePiranhaPlant);
}

static u8 dynamic_shadow_allows_billboard_component_caster(void) {
    const BehaviorScript *behavior = dynamic_shadow_current_behavior();

    if (behavior == NULL) {
        return FALSE;
    }

    // Modeled actors with billboard accessories. Keep those accessories as
    // normal projected geometry instead of replacing them with body circles.
    return dynamic_shadow_behavior_is(behavior, bhvSpindrift)
        || dynamic_shadow_behavior_is(behavior, bhvFlyGuy)
        || dynamic_shadow_behavior_is(behavior, bhvEnemyLakitu)
        || dynamic_shadow_behavior_is(behavior, bhvCameraLakitu)
        || dynamic_shadow_behavior_is(behavior, bhvBeginningLakitu)
        || dynamic_shadow_behavior_is(behavior, bhvWaterBomb);
}

static u8 dynamic_shadow_allows_shadow_alpha_layer(void) {
    const BehaviorScript *behavior = dynamic_shadow_current_behavior();

    if (dynamic_shadow_is_mario_object()
        || dynamic_shadow_allows_billboard_caster()
        || dynamic_shadow_allows_billboard_component_caster()) {
        return TRUE;
    }
    if (behavior == NULL) {
        return FALSE;
    }

    return dynamic_shadow_behavior_is(behavior, bhvStar)
        || dynamic_shadow_behavior_is(behavior, bhvStarSpawnCoordinates)
        || dynamic_shadow_behavior_is(behavior, bhvRedCoinStarMarker)
        || dynamic_shadow_behavior_is(behavior, bhvWaterBomb);
}

static u8 dynamic_shadow_is_tree_like(void) {
    struct Object *obj;
    const BehaviorScript *behavior;

    if (gCurGraphNodeObject == NULL) {
        return FALSE;
    }

    obj = (struct Object *) gCurGraphNodeObject;
    behavior = obj->behavior;
    return behavior == segmented_to_virtual(bhvTree)
        || behavior == segmented_to_virtual(bhvTreeSnow)
        || behavior == segmented_to_virtual(bhvTreeLeaf);
}

static u8 dynamic_shadow_behavior_is_whomp_like(const BehaviorScript *behavior) {
    if (behavior == NULL) {
        return FALSE;
    }

    return behavior == segmented_to_virtual(bhvWhompKingBoss)
        || behavior == segmented_to_virtual(bhvSmallWhomp);
}

static u8 dynamic_shadow_behavior_is_crushing_platform_like(
    const BehaviorScript *behavior) {
    if (behavior == NULL) {
        return FALSE;
    }

    return dynamic_shadow_behavior_is_whomp_like(behavior)
        || behavior == segmented_to_virtual(bhvToxBox)
        || behavior == segmented_to_virtual(bhvGrindel)
        || behavior == segmented_to_virtual(bhvHorizontalGrindel)
        || behavior == segmented_to_virtual(bhvSpindel)
        || behavior == segmented_to_virtual(bhvThwomp)
        || behavior == segmented_to_virtual(bhvThwomp2);
}

static u8 dynamic_shadow_is_whomp_like(void) {
    return dynamic_shadow_behavior_is_whomp_like(dynamic_shadow_current_behavior());
}

static u8 dynamic_shadow_is_excluded_platform(void) {
    struct Object *obj;

    if (gCurGraphNodeObject == NULL) {
        return FALSE;
    }

    obj = (struct Object *) gCurGraphNodeObject;
    if (gCurrLevelNum == LEVEL_BOB
        && obj->behavior == segmented_to_virtual(bhvSeesawPlatform)
        && obj->header.gfx.sharedChild == gLoadedGraphNodes[MODEL_BOB_SEESAW_PLATFORM]) {
        return TRUE;
    }
    if (gCurrLevelNum == LEVEL_WF
        && obj->behavior == segmented_to_virtual(bhvRotatingPlatform)
        && obj->header.gfx.sharedChild == gLoadedGraphNodes[MODEL_WF_ROTATING_PLATFORM]) {
        return TRUE;
    }
    return FALSE;
}

static u8 dynamic_shadow_is_ghost_platform_caster(void) {
    struct Object *obj;
    const BehaviorScript *behavior;

    if (gCurGraphNodeObject == NULL) {
        return FALSE;
    }

    obj = (struct Object *) gCurGraphNodeObject;
    behavior = obj->behavior;

    if (gCurrLevelNum == LEVEL_TTM
        && behavior == segmented_to_virtual(bhvStaticObject)) {
        return TRUE;
    }
    if (gCurrLevelNum == LEVEL_CCM
        && behavior == segmented_to_virtual(bhvStaticObject)) {
        return TRUE;
    }
    if (dynamic_shadow_behavior_is_bowser_course_platform(behavior)) {
        return TRUE;
    }

    return FALSE;
}

static u8 dynamic_shadow_behavior_is_bowser_course_platform(const BehaviorScript *behavior) {
    if (behavior == NULL) {
        return FALSE;
    }
    if (gCurrLevelNum != LEVEL_BITDW && gCurrLevelNum != LEVEL_BITFS
        && gCurrLevelNum != LEVEL_BITS) {
        return FALSE;
    }

    return behavior == segmented_to_virtual(bhvSquarishPathMoving)
        || behavior == segmented_to_virtual(bhvSlidingPlatform2)
        || behavior == segmented_to_virtual(bhvSeesawPlatform)
        || behavior == segmented_to_virtual(bhvFerrisWheelAxle)
        || behavior == segmented_to_virtual(bhvFerrisWheelPlatform)
        || behavior == segmented_to_virtual(bhvPlatformOnTrack)
        || behavior == segmented_to_virtual(bhvOctagonalPlatformRotating)
        || behavior == segmented_to_virtual(bhvActivatedBackAndForthPlatform)
        || behavior == segmented_to_virtual(bhvAnimatesOnFloorSwitchPress)
        || behavior == segmented_to_virtual(bhvFloorSwitchAnimatesObject)
        || behavior == segmented_to_virtual(bhvWarpPipe)
        || behavior == segmented_to_virtual(bhvBitfsSinkingPlatforms)
        || behavior == segmented_to_virtual(bhvBitfsSinkingCagePlatform)
        || behavior == segmented_to_virtual(bhvBitfsTiltingInvertedPyramid)
        || behavior == segmented_to_virtual(bhvSquishablePlatform)
        || behavior == segmented_to_virtual(bhvWfTumblingBridge);
}

static u8 dynamic_shadow_current_caster_is_bowser_course_platform(void) {
    return dynamic_shadow_behavior_is_bowser_course_platform(dynamic_shadow_current_behavior());
}

static u8 dynamic_shadow_is_wf_terrain_object(void) {
    const BehaviorScript *behavior = dynamic_shadow_current_behavior();

    if (gCurrLevelNum != LEVEL_WF || behavior == NULL) {
        return FALSE;
    }

    // WF builds much of the fortress out of collision-bearing objects. They
    // must receive character shadows, but casting their own full model creates
    // the large "ghost fortress" silhouettes seen on neighboring paths.
    return dynamic_shadow_behavior_is(behavior, bhvStaticObject)
        || dynamic_shadow_behavior_is(behavior, bhvGiantPole)
        || dynamic_shadow_behavior_is(behavior, bhvSmallBomp)
        || dynamic_shadow_behavior_is(behavior, bhvLargeBomp)
        || dynamic_shadow_behavior_is(behavior, bhvWfSlidingPlatform)
        || dynamic_shadow_behavior_is(behavior, bhvWfTumblingBridge)
        || dynamic_shadow_behavior_is(behavior, bhvWfBreakableWallRight)
        || dynamic_shadow_behavior_is(behavior, bhvWfBreakableWallLeft)
        || dynamic_shadow_behavior_is(behavior, bhvKickableBoard)
        || dynamic_shadow_behavior_is(behavior, bhvRotatingPlatform)
        || dynamic_shadow_behavior_is(behavior, bhvCheckerboardElevatorGroup)
        || dynamic_shadow_behavior_is(behavior, bhvTower)
        || dynamic_shadow_behavior_is(behavior, bhvBulletBillCannon)
        || dynamic_shadow_behavior_is(behavior, bhvTowerPlatformGroup)
        || dynamic_shadow_behavior_is(behavior, bhvTowerDoor)
        || dynamic_shadow_behavior_is(behavior, bhvWfSlidingTowerPlatform)
        || dynamic_shadow_behavior_is(behavior, bhvWfElevatorTowerPlatform)
        || dynamic_shadow_behavior_is(behavior, bhvWfSolidTowerPlatform);
}

static u8 dynamic_shadow_is_jrb_terrain_object(void) {
    const BehaviorScript *behavior = dynamic_shadow_current_behavior();

    if (gCurrLevelNum != LEVEL_JRB || behavior == NULL) {
        return FALSE;
    }

    // JRB uses many solid model objects as course terrain. They should receive
    // Mario/enemy shadows, but casting their own shapes causes underwater clutter
    // and can trip the platform fallback path when Mario gets close.
    return dynamic_shadow_behavior_is(behavior, bhvRockSolid)
        || dynamic_shadow_behavior_is(behavior, bhvPillarBase)
        || dynamic_shadow_behavior_is(behavior, bhvSunkenShipPart)
        || dynamic_shadow_behavior_is(behavior, bhvSunkenShipPart2)
        || dynamic_shadow_behavior_is(behavior, bhvShipPart3);
}

static u8 dynamic_shadow_is_fixed_interaction_object(void) {
    const BehaviorScript *behavior = dynamic_shadow_current_behavior();

    if (behavior == NULL) {
        return FALSE;
    }

    // Doors and decorative level fixtures are large enough to land on roofs or
    // floors above them, while their baked presentation already reads correctly.
    return dynamic_shadow_behavior_is(behavior, bhvDoor)
        || dynamic_shadow_behavior_is(behavior, bhvDoorWarp)
        || dynamic_shadow_behavior_is(behavior, bhvStarDoor)
        || dynamic_shadow_behavior_is(behavior, bhvTowerDoor)
        || dynamic_shadow_behavior_is(behavior, bhvBowserKeyUnlockDoor)
        || dynamic_shadow_behavior_is(behavior, bhvBowserSubDoor)
        || dynamic_shadow_behavior_is(behavior, bhvOpenableCageDoor)
        || dynamic_shadow_behavior_is(behavior, bhvOpenableGrill)
        || dynamic_shadow_behavior_is(behavior, bhvChainChompGate)
        || dynamic_shadow_behavior_is(behavior, bhvMoatGrills)
        || dynamic_shadow_behavior_is(behavior, bhvUnlockDoorStar)
        || dynamic_shadow_behavior_is(behavior, bhvAnimatesOnFloorSwitchPress)
        || dynamic_shadow_behavior_is(behavior, bhvFloorSwitchAnimatesObject)
        || dynamic_shadow_behavior_is(behavior, bhvFloorSwitchGrills)
        || dynamic_shadow_behavior_is(behavior, bhvFloorSwitchHardcodedModel)
        || dynamic_shadow_behavior_is(behavior, bhvFloorSwitchHiddenObjects)
        || dynamic_shadow_behavior_is(behavior, bhvPurpleSwitchHiddenBoxes)
        || dynamic_shadow_behavior_is(behavior, bhvBookSwitch)
        || dynamic_shadow_behavior_is(behavior, bhvCapSwitchBase)
        || dynamic_shadow_behavior_is(behavior, bhvCapSwitch)
        || dynamic_shadow_behavior_is(behavior, bhvBlueCoinSwitch)
        || dynamic_shadow_behavior_is(behavior, bhvWoodenPost);
}

static u8 dynamic_shadow_behavior_is_platform_shadow_caster_allowed(
    const BehaviorScript *behavior) {
    if (behavior == NULL) {
        return FALSE;
    }
    if (gCurrLevelNum == LEVEL_BITDW || gCurrLevelNum == LEVEL_BITFS
        || gCurrLevelNum == LEVEL_BITS) {
        return behavior == segmented_to_virtual(bhvPushableMetalBox);
    }

    return behavior == segmented_to_virtual(bhvRrElevatorPlatform)
        || behavior == segmented_to_virtual(bhvHmcElevatorPlatform)
        || behavior == segmented_to_virtual(bhvRotatingPlatform)
        || behavior == segmented_to_virtual(bhvWfRotatingWoodenPlatform)
        || behavior == segmented_to_virtual(bhvTumblingBridgePlatform)
        || behavior == segmented_to_virtual(bhvWfTumblingBridge)
        || behavior == segmented_to_virtual(bhvBbhTumblingBridge)
        || behavior == segmented_to_virtual(bhvLllTumblingBridge)
        || behavior == segmented_to_virtual(bhvTowerPlatformGroup)
        || behavior == segmented_to_virtual(bhvWfSlidingTowerPlatform)
        || behavior == segmented_to_virtual(bhvWfElevatorTowerPlatform)
        || behavior == segmented_to_virtual(bhvWfSolidTowerPlatform)
        || behavior == segmented_to_virtual(bhvLargeBomp)
        || behavior == segmented_to_virtual(bhvPushableMetalBox)
        || behavior == segmented_to_virtual(bhvJrbSlidingBox)
        || behavior == segmented_to_virtual(bhvJrbFloatingBox)
        || behavior == segmented_to_virtual(bhvClockMinuteHand)
        || behavior == segmented_to_virtual(bhvClockHourHand)
        || behavior == segmented_to_virtual(bhvCheckerboardElevatorGroup)
        || behavior == segmented_to_virtual(bhvCheckerboardPlatformSub)
        || behavior == segmented_to_virtual(bhvHiddenStaircaseStep)
        || behavior == segmented_to_virtual(bhvBooBossSpawnedBridge)
        || behavior == segmented_to_virtual(bhvWdwExpressElevator)
        || behavior == segmented_to_virtual(bhvWdwExpressElevatorPlatform)
        || behavior == segmented_to_virtual(bhvMeshElevator)
        || behavior == segmented_to_virtual(bhvBbhTiltingTrapPlatform)
        || behavior == segmented_to_virtual(bhvStaticCheckeredPlatform)
        || behavior == segmented_to_virtual(bhvLllSinkingRockBlock)
        || behavior == segmented_to_virtual(bhvLllRotatingBlockWithFireBars)
        || behavior == segmented_to_virtual(bhvLllWoodPiece)
        || behavior == segmented_to_virtual(bhvLllFloatingWoodBridge)
        || behavior == segmented_to_virtual(bhvLllRotatingHexagonalRing)
        || behavior == segmented_to_virtual(bhvLllHexagonalMesh)
        || behavior == segmented_to_virtual(bhvLllDrawbridge)
        || behavior == segmented_to_virtual(bhvWfSlidingPlatform)
        || behavior == segmented_to_virtual(bhvRrCruiserWing)
        || behavior == segmented_to_virtual(bhvSslMovingPyramidWall)
        || behavior == segmented_to_virtual(bhvPyramidElevator)
        || behavior == segmented_to_virtual(bhvPyramidTop)
        || behavior == segmented_to_virtual(bhvTtmRollingLog)
        || behavior == segmented_to_virtual(bhvLllVolcanoFallingTrap)
        || behavior == segmented_to_virtual(bhvLllRollingLog)
        || behavior == segmented_to_virtual(bhvControllablePlatform)
        || behavior == segmented_to_virtual(bhvSeesawPlatform)
        || behavior == segmented_to_virtual(bhvPlatformOnTrack)
        || behavior == segmented_to_virtual(bhvFerrisWheelAxle)
        || behavior == segmented_to_virtual(bhvFerrisWheelPlatform)
        || behavior == segmented_to_virtual(bhvArrowLift)
        || behavior == segmented_to_virtual(bhvWdwSquareFloatingPlatform)
        || behavior == segmented_to_virtual(bhvWdwRectangularFloatingPlatform)
        || behavior == segmented_to_virtual(bhvJrbFloatingPlatform)
        || behavior == segmented_to_virtual(bhvRrElevatorPlatform)
        || behavior == segmented_to_virtual(bhvRrRotatingBridgePlatform)
        || behavior == segmented_to_virtual(bhvSlidingPlatform2)
        || behavior == segmented_to_virtual(bhvOctagonalPlatformRotating)
        || behavior == segmented_to_virtual(bhvActivatedBackAndForthPlatform)
        || behavior == segmented_to_virtual(bhvLllRotatingHexagonalPlatform)
        || behavior == segmented_to_virtual(bhvLllMovingOctagonalMeshPlatform)
        || behavior == segmented_to_virtual(bhvLllSinkingRectangularPlatform)
        || behavior == segmented_to_virtual(bhvLllSinkingSquarePlatforms)
        || behavior == segmented_to_virtual(bhvLllTiltingInvertedPyramid)
        || behavior == segmented_to_virtual(bhvBitfsSinkingPlatforms)
        || behavior == segmented_to_virtual(bhvBitfsSinkingCagePlatform)
        || behavior == segmented_to_virtual(bhvBitfsTiltingInvertedPyramid)
        || behavior == segmented_to_virtual(bhvSquishablePlatform)
        || behavior == segmented_to_virtual(bhvAnotherTiltingPlatform)
        || behavior == segmented_to_virtual(bhvTiltingBowserLavaPlatform)
        || behavior == segmented_to_virtual(bhvFallingBowserPlatform)
        || behavior == segmented_to_virtual(bhvSwingPlatform)
        || behavior == segmented_to_virtual(bhvDonutPlatform)
        || behavior == segmented_to_virtual(bhvTTCRotatingSolid)
        || behavior == segmented_to_virtual(bhvTTCPendulum)
        || behavior == segmented_to_virtual(bhvTTCTreadmill)
        || behavior == segmented_to_virtual(bhvTTCMovingBar)
        || behavior == segmented_to_virtual(bhvTTCCog)
        || behavior == segmented_to_virtual(bhvTTCPitBlock)
        || behavior == segmented_to_virtual(bhvTTC2DRotator)
        || behavior == segmented_to_virtual(bhvTTCElevator)
        || behavior == segmented_to_virtual(bhvTTCSpinner);
}

static u8 dynamic_shadow_is_platform_shadow_caster_allowed(void) {
    return dynamic_shadow_behavior_is_platform_shadow_caster_allowed(
        dynamic_shadow_current_behavior());
}

static u8 dynamic_shadow_is_large_platform_shadow_caster(void) {
    const BehaviorScript *behavior = dynamic_shadow_current_behavior();

    if (behavior == NULL) {
        return FALSE;
    }

    return behavior == segmented_to_virtual(bhvTowerPlatformGroup)
        || behavior == segmented_to_virtual(bhvBooBossSpawnedBridge)
        || behavior == segmented_to_virtual(bhvLllFloatingWoodBridge)
        || behavior == segmented_to_virtual(bhvRrCruiserWing)
        || behavior == segmented_to_virtual(bhvSslMovingPyramidWall)
        || behavior == segmented_to_virtual(bhvPyramidTop)
        || behavior == segmented_to_virtual(bhvOctagonalPlatformRotating)
        || behavior == segmented_to_virtual(bhvLllRotatingHexagonalPlatform)
        || behavior == segmented_to_virtual(bhvLllMovingOctagonalMeshPlatform)
        || behavior == segmented_to_virtual(bhvLllRotatingHexagonalRing);
}

static u8 dynamic_shadow_is_receiver_only_platform(void) {
    const BehaviorScript *behavior;

    if (gCurGraphNodeObject == NULL) {
        return FALSE;
    }

    behavior = ((struct Object *) gCurGraphNodeObject)->behavior;

    if (dynamic_shadow_is_platform_shadow_caster_allowed()) {
        return FALSE;
    }

    return behavior == segmented_to_virtual(bhvRotatingPlatform)
        || behavior == segmented_to_virtual(bhvWfRotatingWoodenPlatform)
        || behavior == segmented_to_virtual(bhvTumblingBridgePlatform)
        || behavior == segmented_to_virtual(bhvWfTumblingBridge)
        || behavior == segmented_to_virtual(bhvBbhTumblingBridge)
        || behavior == segmented_to_virtual(bhvLllTumblingBridge)
        || behavior == segmented_to_virtual(bhvTowerPlatformGroup)
        || behavior == segmented_to_virtual(bhvCheckerboardElevatorGroup)
        || behavior == segmented_to_virtual(bhvLargeBomp)
        || behavior == segmented_to_virtual(bhvBitfsSinkingPlatforms)
        || behavior == segmented_to_virtual(bhvBitfsSinkingCagePlatform)
        || behavior == segmented_to_virtual(bhvBitfsTiltingInvertedPyramid)
        || behavior == segmented_to_virtual(bhvSquishablePlatform)
        || behavior == segmented_to_virtual(bhvRrRotatingBridgePlatform)
        || behavior == segmented_to_virtual(bhvWfSolidTowerPlatform)
        || behavior == segmented_to_virtual(bhvAnotherTiltingPlatform)
        || behavior == segmented_to_virtual(bhvTiltingBowserLavaPlatform)
        || behavior == segmented_to_virtual(bhvFallingBowserPlatform)
        || behavior == segmented_to_virtual(bhvCheckerboardPlatformSub)
        || behavior == segmented_to_virtual(bhvLllRotatingHexagonalPlatform)
        || behavior == segmented_to_virtual(bhvLllMovingOctagonalMeshPlatform)
        || behavior == segmented_to_virtual(bhvLllSinkingRectangularPlatform)
        || behavior == segmented_to_virtual(bhvLllSinkingSquarePlatforms)
        || behavior == segmented_to_virtual(bhvLllTiltingInvertedPyramid)
        || behavior == segmented_to_virtual(bhvBbhTiltingTrapPlatform)
        || behavior == segmented_to_virtual(bhvStaticCheckeredPlatform)
        || behavior == segmented_to_virtual(bhvWdwSquareFloatingPlatform)
        || behavior == segmented_to_virtual(bhvWdwRectangularFloatingPlatform)
        || behavior == segmented_to_virtual(bhvJrbFloatingPlatform)
        || behavior == segmented_to_virtual(bhvFerrisWheelAxle)
        || behavior == segmented_to_virtual(bhvArrowLift)
        || behavior == segmented_to_virtual(bhvPlatformOnTrack)
        || behavior == segmented_to_virtual(bhvFerrisWheelPlatform)
        || behavior == segmented_to_virtual(bhvSlidingPlatform2)
        || behavior == segmented_to_virtual(bhvOctagonalPlatformRotating)
        || behavior == segmented_to_virtual(bhvActivatedBackAndForthPlatform)
        || behavior == segmented_to_virtual(bhvSwingPlatform)
        || behavior == segmented_to_virtual(bhvDonutPlatform);
}

static u8 dynamic_shadow_is_terrain_receiver_object(void) {
    return dynamic_shadow_is_wf_terrain_object()
        || dynamic_shadow_is_jrb_terrain_object()
        || dynamic_shadow_is_receiver_only_platform()
        || dynamic_shadow_is_platform_shadow_caster_allowed();
}

static u8 dynamic_shadow_behavior_is_platform_interaction_caster(
    const BehaviorScript *behavior) {
    if (behavior == NULL) {
        return FALSE;
    }

    return dynamic_shadow_behavior_is_crushing_platform_like(behavior)
        || dynamic_shadow_behavior_is_platform_shadow_caster_allowed(behavior)
        || dynamic_shadow_behavior_is(behavior, bhvMessagePanel)
        || dynamic_shadow_behavior_is(behavior, bhvSignOnWall)
        || dynamic_shadow_behavior_is(behavior, bhvBreakableBox)
        || dynamic_shadow_behavior_is(behavior, bhvBreakableBoxSmall)
        || dynamic_shadow_behavior_is(behavior, bhvExclamationBox);
}

static u8 dynamic_shadow_is_platform_interaction_caster(void) {
    if (sCurrentObjectPlatformInteractionCaster) {
        return TRUE;
    }

    return dynamic_shadow_behavior_is_platform_interaction_caster(
        dynamic_shadow_current_behavior());
}

static u8 dynamic_shadow_should_use_original_hitbox_blob(void) {
    const BehaviorScript *behavior = dynamic_shadow_current_behavior();

    return dynamic_shadow_behavior_is(behavior, bhvExclamationBox);

}

static u8 dynamic_shadow_requires_static_floor_receiver(void) {
    const BehaviorScript *behavior = dynamic_shadow_current_behavior();

    if (behavior == NULL) {
        return FALSE;
    }

    return dynamic_shadow_behavior_is(behavior, bhvFallingPillar)
        || dynamic_shadow_behavior_is(behavior, bhvCapSwitchBase)
        || dynamic_shadow_behavior_is(behavior, bhvCapSwitch)
        || dynamic_shadow_behavior_is(behavior, bhvBlueCoinSwitch);
}

static u8 dynamic_shadow_never_keep_original_shadow(void) {
    const BehaviorScript *behavior = dynamic_shadow_current_behavior();

    if (dynamic_shadow_is_ending_peach_cutscene_actor()
        && !dynamic_shadow_should_disable_ending_actor_dynamic_shadow()) {
        return TRUE;
    }

    if (behavior == NULL) {
        return FALSE;
    }

    return dynamic_shadow_behavior_is(behavior, bhvCapSwitchBase)
        || dynamic_shadow_behavior_is(behavior, bhvCapSwitch)
        || dynamic_shadow_behavior_is(behavior, bhvBlueCoinSwitch)
        || dynamic_shadow_behavior_is(behavior, bhvWoodenPost);
}

static u8 dynamic_shadow_should_suppress_self_receiver(void) {
    return !dynamic_shadow_is_terrain_receiver_object();
}

static u8 dynamic_shadow_is_model_object_far(void) {
    Vec3f *cameraPos;
    f32 dx, dy, dz;
    f32 marioDistSq;
    f32 maxMarioDist;
    f32 maxCameraDist;
    u8 isLargePlatform;

    if (gCurGraphNodeCamera == NULL || gCurGraphNodeObject == NULL || gMarioObject == NULL) {
        return TRUE;
    }

    // Primary: distance from Mario in XZ plane. The model-projection path is
    // most convincing near the player; far objects fall back to their vanilla
    // sprite shadows.
    isLargePlatform = dynamic_shadow_is_platform_interaction_caster();
    dx = gCurGraphNodeObject->pos[0] - gMarioObject->header.gfx.pos[0];
    dz = gCurGraphNodeObject->pos[2] - gMarioObject->header.gfx.pos[2];
    maxMarioDist = dynamic_shadow_is_mario_object() || isLargePlatform ? 5200.0f : 3600.0f;
    marioDistSq = dx * dx + dz * dz;
    if (marioDistSq > maxMarioDist * maxMarioDist) {
        return TRUE;
    }

    // Close interaction objects can briefly report awkward camera-space
    // bounds while Mario is inside their action radius. Keep their dynamic
    // caster path alive nearby, and only apply strict screen-size culling
    // once the object is far enough that the fallback blob is acceptable.
    if (!isLargePlatform && marioDistSq > 1600.0f * 1600.0f
        && !dynamic_shadow_object_is_clear_on_screen()) {
        return TRUE;
    }

    // Vertical offset from Mario (asymmetric):
    //   above Mario: tight threshold 600 (flying enemies, high platforms)
    //   below Mario: generous threshold 2000 (valleys, pits)
    // The light is ~4700 above Mario projecting downward; objects high above
    // are too close to the light and their shadows would be distorted.
    dy = gCurGraphNodeObject->pos[1] - gMarioObject->header.gfx.pos[1];
    if (isLargePlatform) {
        if (dy > 2600.0f || dy < -2600.0f) {
            return TRUE;
        }
    } else if (dynamic_shadow_behavior_is_crushing_platform_like(dynamic_shadow_current_behavior())) {
        if (dy > 2200.0f || dy < -2200.0f) {
            return TRUE;
        }
    } else if (!dynamic_shadow_is_mario_object() && (dy > 1400.0f || dy < -1800.0f)) {
        return TRUE;
    }
    if (dynamic_shadow_is_mario_object() && (dy > 1400.0f || dy < -2400.0f)) {
        return TRUE;
    }

    // Secondary: distance from camera. This catches large off-route scenes such
    // as castle grounds without imposing a visible-object count cap.
    dx = gCurGraphNodeObject->pos[0] - gCurGraphNodeCamera->pos[0];
    dy = gCurGraphNodeObject->pos[1] - gCurGraphNodeCamera->pos[1];
    dz = gCurGraphNodeObject->pos[2] - gCurGraphNodeCamera->pos[2];
    maxCameraDist = dynamic_shadow_is_mario_object() || isLargePlatform ? 9000.0f : 6400.0f;
    if (dx * dx + dy * dy + dz * dz > maxCameraDist * maxCameraDist) {
        return TRUE;
    }

    // Secondary: is the object behind the camera?
    // Use dot product with camera forward direction as a fallback for nodes
    // whose camera-space transform is near the frustum edge.
    cameraPos = &gCurGraphNodeCamera->pos;
    dx = gCurGraphNodeCamera->focus[0] - (*cameraPos)[0]; // forward X
    dz = gCurGraphNodeCamera->focus[2] - (*cameraPos)[2]; // forward Z
    // Negate camera->object to get object->camera dot forward
    f32 toCamX = (*cameraPos)[0] - gCurGraphNodeObject->pos[0];
    f32 toCamZ = (*cameraPos)[2] - gCurGraphNodeObject->pos[2];
    if (!isLargePlatform && toCamX * dx + toCamZ * dz > 0.0f) {
        return TRUE;
    }

    return FALSE;
}

static u8 dynamic_shadow_surface_type_can_receive(struct Surface *surface) {
    struct Object *caster = (struct Object *) gCurGraphNodeObject;

    if (surface == NULL) {
        return FALSE;
    }
    if (surface->object != NULL && surface->object == caster) {
        return FALSE;
    }
    if (surface->object != NULL && dynamic_shadow_requires_static_floor_receiver()) {
        return FALSE;
    }

    switch (surface->type) {
        case SURFACE_DEATH_PLANE:
        case SURFACE_BURNING:
        case SURFACE_INTANGIBLE:
        case SURFACE_CAMERA_BOUNDARY:
        case SURFACE_SHALLOW_QUICKSAND:
        case SURFACE_DEEP_QUICKSAND:
        case SURFACE_INSTANT_QUICKSAND:
        case SURFACE_DEEP_MOVING_QUICKSAND:
        case SURFACE_SHALLOW_MOVING_QUICKSAND:
        case SURFACE_QUICKSAND:
        case SURFACE_MOVING_QUICKSAND:
        case SURFACE_INSTANT_MOVING_QUICKSAND:
            return FALSE;
        case SURFACE_VANISH_CAP_WALLS:
            return gCurrLevelNum == LEVEL_VCUTM;
        case SURFACE_WATER:
            // Once the moat is drained, its collision keeps the water surface
            // type even though Mario is walking on the exposed channel.
            return gCurrLevelNum == LEVEL_CASTLE_GROUNDS
                && (save_file_get_flags() & SAVE_FLAG_MOAT_DRAINED)
                && surface->normal.y > 0.80f;
        case SURFACE_FLOWING_WATER:
            // The drained moat can still be tagged as flowing water; allow
            // only flat castle-ground riverbed planes so waterfall curtains
            // stay out of the receiver set.
            return gCurrLevelNum == LEVEL_CASTLE_GROUNDS
                && (save_file_get_flags() & SAVE_FLAG_MOAT_DRAINED)
                && surface->normal.y > 0.80f;
    }

    return TRUE;
}

static u8 dynamic_shadow_surface_can_receive(struct Surface *floor) {
    if (!dynamic_shadow_surface_type_can_receive(floor)) {
        return FALSE;
    }
    if (gCurrLevelNum == LEVEL_BITS && floor != NULL && floor->object == NULL
        && dynamic_shadow_current_caster_is_bowser_course_platform()) {
        return FALSE;
    }
    if (floor->normal.y < 0.25f) {
        return FALSE;
    }

    return TRUE;
}

static u8 dynamic_shadow_floor_contains_xz(struct Surface *surface, Vec3f point);

static f32 dynamic_shadow_surface_floor_height_at(struct Surface *surface, f32 x, f32 z) {
    if (surface == NULL || surface->normal.y == 0.0f) {
        return -11000.0f;
    }

    return -(surface->normal.x * x + surface->normal.z * z + surface->originOffset)
        / surface->normal.y;
}

static f32 dynamic_shadow_find_receivable_floor_in_list(struct SurfaceNode *node,
                                                        f32 x, f32 y, f32 z,
                                                        struct Surface **floor) {
    struct Surface *surface;
    struct Surface *bestFloor = NULL;
    Vec3f point;
    f32 height;
    f32 bestHeight = -11000.0f;

    point[0] = x;
    point[1] = y;
    point[2] = z;

    while (node != NULL) {
        surface = node->surface;
        node = node->next;
        if (!dynamic_shadow_surface_can_receive(surface)) {
            continue;
        }
        if (!dynamic_shadow_floor_contains_xz(surface, point)) {
            continue;
        }

        height = dynamic_shadow_surface_floor_height_at(surface, x, z);
        if (height < -10000.0f || height > y + 78.0f) {
            continue;
        }
        if (height > bestHeight) {
            bestHeight = height;
            bestFloor = surface;
        }
    }

    if (bestFloor == NULL) {
        return -11000.0f;
    }

    *floor = bestFloor;
    return bestHeight;
}

static f32 dynamic_shadow_find_receivable_floor_in_partitions(f32 x, f32 y, f32 z,
                                                              struct Surface **floor) {
    struct Surface *candidate;
    f32 height;
    f32 bestHeight = -11000.0f;
    s32 cellX;
    s32 cellZ;

    *floor = NULL;
    if (x <= -LEVEL_BOUNDARY_MAX || x >= LEVEL_BOUNDARY_MAX
        || z <= -LEVEL_BOUNDARY_MAX || z >= LEVEL_BOUNDARY_MAX) {
        return -11000.0f;
    }

    cellX = ((s16) x + LEVEL_BOUNDARY_MAX) / CELL_SIZE;
    cellZ = ((s16) z + LEVEL_BOUNDARY_MAX) / CELL_SIZE;
    cellX &= 0x0F;
    cellZ &= 0x0F;

    candidate = NULL;
    height = dynamic_shadow_find_receivable_floor_in_list(
        gDynamicSurfacePartition[cellZ][cellX][SPATIAL_PARTITION_FLOORS].next,
        x, y, z, &candidate);
    if (height > bestHeight && candidate != NULL) {
        bestHeight = height;
        *floor = candidate;
    }

    candidate = NULL;
    height = dynamic_shadow_find_receivable_floor_in_list(
        gStaticSurfacePartition[cellZ][cellX][SPATIAL_PARTITION_FLOORS].next,
        x, y, z, &candidate);
    if (height > bestHeight && candidate != NULL) {
        bestHeight = height;
        *floor = candidate;
    }

    return bestHeight;
}

static f32 dynamic_shadow_find_receivable_floor_in_neighbor_partitions(
    f32 x, f32 y, f32 z, struct Surface **floor) {
    struct Surface *candidate;
    f32 height;
    f32 bestHeight = -11000.0f;
    s32 centerCellX;
    s32 centerCellZ;
    s32 cellX;
    s32 cellZ;

    *floor = NULL;
    if (x <= -LEVEL_BOUNDARY_MAX || x >= LEVEL_BOUNDARY_MAX
        || z <= -LEVEL_BOUNDARY_MAX || z >= LEVEL_BOUNDARY_MAX) {
        return -11000.0f;
    }

    centerCellX = ((s16) x + LEVEL_BOUNDARY_MAX) / CELL_SIZE;
    centerCellZ = ((s16) z + LEVEL_BOUNDARY_MAX) / CELL_SIZE;
    centerCellX &= 0x0F;
    centerCellZ &= 0x0F;

    for (cellZ = centerCellZ - 1; cellZ <= centerCellZ + 1; cellZ++) {
        if (cellZ < 0 || cellZ > 15) {
            continue;
        }
        for (cellX = centerCellX - 1; cellX <= centerCellX + 1; cellX++) {
            if (cellX < 0 || cellX > 15) {
                continue;
            }

            candidate = NULL;
            height = dynamic_shadow_find_receivable_floor_in_list(
                gDynamicSurfacePartition[cellZ][cellX][SPATIAL_PARTITION_FLOORS].next,
                x, y, z, &candidate);
            if (height > bestHeight && candidate != NULL) {
                bestHeight = height;
                *floor = candidate;
            }

            candidate = NULL;
            height = dynamic_shadow_find_receivable_floor_in_list(
                gStaticSurfacePartition[cellZ][cellX][SPATIAL_PARTITION_FLOORS].next,
                x, y, z, &candidate);
            if (height > bestHeight && candidate != NULL) {
                bestHeight = height;
                *floor = candidate;
            }
        }
    }

    return bestHeight;
}

static f32 dynamic_shadow_find_receivable_floor_at(f32 x, f32 y, f32 z,
                                                   struct Surface **floor) {
    struct Object *caster = (struct Object *) gCurGraphNodeObject;
    struct Surface *candidate;
    f32 floorHeight;
    f32 skippedHeight;
    f32 queryY = y;
    s32 i;

    *floor = NULL;
    for (i = 0; i < 12; i++) {
        candidate = NULL;
        floorHeight = find_floor(x, queryY, z, &candidate);
        if (floorHeight < -10000.0f || candidate == NULL) {
            return -11000.0f;
        }
        if (dynamic_shadow_surface_can_receive(candidate)) {
            *floor = candidate;
            return floorHeight;
        }

        if (caster != NULL && candidate->object == caster) {
            skippedHeight = floorHeight;
            if (dynamic_shadow_is_whomp_like()) {
                floorHeight = dynamic_shadow_find_receivable_floor_in_neighbor_partitions(
                    x, queryY, z, floor);
            } else {
                floorHeight = dynamic_shadow_find_receivable_floor_in_partitions(
                    x, queryY, z, floor);
            }
            if (floorHeight > -10000.0f && *floor != NULL) {
                return floorHeight;
            }
            queryY = skippedHeight - 8.0f;
        } else {
            queryY = floorHeight - 96.0f;
        }
    }

    return -11000.0f;
}

static f32 dynamic_shadow_find_receivable_floor_near(Vec3f shadowPos, f32 queryHeight,
                                                     f32 minHeightFromFloor,
                                                     f32 maxHeightFromFloor,
                                                     struct Surface **floor) {
    struct Surface *candidate;
    struct Surface *bestFloor = NULL;
    f32 floorHeight;
    f32 heightDelta;
    f32 bestHeight = -11000.0f;
    f32 bestScore = 1000000000.0f;
    f32 sampleRadius;
    f32 dx;
    f32 dz;
    s16 objectRadius;
    s32 i;
    static const s8 sSampleDirs[9][2] = {
        { 0, 0 },
        { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
        { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 },
    };

    *floor = NULL;
    objectRadius = dynamic_shadow_get_object_culling_radius();
    if (dynamic_shadow_is_platform_interaction_caster()) {
        // Surface/collision objects can hide the real floor under their own
        // top faces when Mario gets close. Sample just outside their footprint
        // so Whomps, pushable boxes, and JRB stone boxes do not fall back to
        // their vanilla shadows at interaction range.
        sampleRadius = objectRadius + 96.0f;
        if (sampleRadius < 260.0f) sampleRadius = 260.0f;
        if (dynamic_shadow_is_whomp_like()) {
            if (sampleRadius < 960.0f) sampleRadius = 960.0f;
            if (sampleRadius > 1280.0f) sampleRadius = 1280.0f;
        } else if (sampleRadius > 820.0f) {
            sampleRadius = 820.0f;
        }
    } else {
        sampleRadius = objectRadius * 0.38f;
        if (sampleRadius < 140.0f) sampleRadius = 140.0f;
        if (sampleRadius > 560.0f) sampleRadius = 560.0f;
    }

    for (i = 0; i < 9; i++) {
        f32 radiusScale = 1.0f;
        dx = sSampleDirs[i][0] * sampleRadius;
        dz = sSampleDirs[i][1] * sampleRadius;
        if (dynamic_shadow_is_whomp_like() && i != 0) {
            /*
             * Real WF Whomps contribute a narrow collision prism and can hide
             * the supporting floor from find_floor exactly when Mario enters
             * their interaction radius. Try a half-ring before the outer ring:
             * it stays on the same ledge more often while still escaping self
             * collision when the Whomp lies flat.
             */
            radiusScale = 0.55f;
            dx *= radiusScale;
            dz *= radiusScale;
        }
        floorHeight = dynamic_shadow_find_receivable_floor_at(
            shadowPos[0] + dx, shadowPos[1] + queryHeight, shadowPos[2] + dz, &candidate);
        if (floorHeight < -10000.0f || candidate == NULL) {
            continue;
        }
        if (dynamic_shadow_is_platform_interaction_caster()
            && floorHeight > shadowPos[1] + 32.0f) {
            continue;
        }

        heightDelta = shadowPos[1] - floorHeight;
        if (heightDelta < minHeightFromFloor || heightDelta >= maxHeightFromFloor) {
            continue;
        }

        // Prefer the authoritative center hit, but for large interactive
        // objects fall back to nearby support points when their center is over
        // self-collision, a ledge, or the interaction volume.
        if (dx * dx + dz * dz < bestScore) {
            bestScore = dx * dx + dz * dz;
            bestFloor = candidate;
            bestHeight = floorHeight;
        }
    }

    if (bestFloor == NULL && dynamic_shadow_is_whomp_like()) {
        for (i = 1; i < 9; i++) {
            dx = sSampleDirs[i][0] * sampleRadius;
            dz = sSampleDirs[i][1] * sampleRadius;
            floorHeight = dynamic_shadow_find_receivable_floor_at(
                shadowPos[0] + dx, shadowPos[1] + queryHeight, shadowPos[2] + dz, &candidate);
            if (floorHeight < -10000.0f || candidate == NULL) {
                continue;
            }
            if (dynamic_shadow_is_platform_interaction_caster()
                && floorHeight > shadowPos[1] + 32.0f) {
                continue;
            }

            heightDelta = shadowPos[1] - floorHeight;
            if (heightDelta < minHeightFromFloor || heightDelta >= maxHeightFromFloor) {
                continue;
            }

            if (dx * dx + dz * dz < bestScore) {
                bestScore = dx * dx + dz * dz;
                bestFloor = candidate;
                bestHeight = floorHeight;
            }
        }
    }

    if (bestFloor == NULL) {
        return -11000.0f;
    }

    *floor = bestFloor;
    return bestHeight;
}

static u8 dynamic_shadow_surface_is_underwater(struct Surface *surface, Vec3f point) {
    f32 waterLevel;

    if (surface == NULL) {
        return FALSE;
    }
    if (gCurrLevelNum == LEVEL_CASTLE_GROUNDS
        && (save_file_get_flags() & SAVE_FLAG_MOAT_DRAINED)
        && (surface->type == SURFACE_WATER || surface->type == SURFACE_FLOWING_WATER)) {
        return FALSE;
    }
    if (surface->type == SURFACE_WATER || surface->type == SURFACE_FLOWING_WATER) {
        return TRUE;
    }

    waterLevel = find_water_level(point[0], point[2]);
    return waterLevel > -10000.0f && point[1] < waterLevel - 10.0f;
}

static u8 dynamic_shadow_point_is_underwater(Vec3f point) {
    f32 waterLevel = find_water_level(point[0], point[2]);
    return waterLevel > -10000.0f && point[1] < waterLevel - 28.0f;
}

static u8 dynamic_shadow_footprint_is_underwater(struct Surface *surface,
                                                 Vec3f anchorPos,
                                                 Vec3f projectedPoint);

static u8 dynamic_shadow_camera_is_underwater(void) {
    f32 waterLevel = find_water_level(gLakituState.pos[0], gLakituState.pos[2]);
    return waterLevel > -10000.0f && gLakituState.pos[1] < waterLevel - 10.0f;
}

static u8 dynamic_shadow_get_footprint_tint(struct Surface *surface,
                                            Vec3f anchorPos,
                                            Vec3f projectedPoint) {
    if (!dynamic_shadow_footprint_is_underwater(surface, anchorPos, projectedPoint)) {
        return 0;
    }

    return dynamic_shadow_camera_is_underwater() ? 2 : 1;
}

static f32 dynamic_shadow_surface_height_at(struct Surface *surface, f32 x, f32 z,
                                            const struct DynamicShadowLight *light);

static u8 dynamic_shadow_receiver_sample_is_underwater(struct Surface *surface,
                                                       const struct DynamicShadowLight *light,
                                                       Vec3f point) {
    f32 receiverY;
    f32 waterLevel;

    if (surface == NULL) {
        return FALSE;
    }
    if (dynamic_shadow_surface_is_underwater(surface, point)) {
        return TRUE;
    }

    receiverY = dynamic_shadow_surface_height_at(surface, point[0], point[2], light);
    if (receiverY < -10000.0f) {
        receiverY = point[1];
    }
    waterLevel = find_water_level(point[0], point[2]);
    return waterLevel > -10000.0f && receiverY < waterLevel - 2.0f;
}

static u8 dynamic_shadow_footprint_is_underwater(struct Surface *surface,
                                                 Vec3f anchorPos,
                                                 Vec3f projectedPoint) {
    const struct DynamicShadowLight *light = dynamic_shadows_get_light();
    (void) anchorPos;

    if (dynamic_shadow_receiver_sample_is_underwater(surface, light, projectedPoint)
        || dynamic_shadow_point_is_underwater(projectedPoint)) {
        return TRUE;
    }

    return FALSE;
}

static u8 dynamic_shadow_wall_can_receive(struct Surface *wall, const struct DynamicShadowLight *light) {
    f32 dirX;
    f32 dirY;
    f32 dirZ;
    f32 normalDotDir;

    if (!dynamic_shadow_surface_type_can_receive(wall)) {
        return FALSE;
    }
    if (wall->normal.y > 0.45f) {
        return FALSE;
    }
    if (wall->normal.y < -0.20f) {
        return FALSE;
    }

    dirX = -sins(light->yaw) * light->length;
    dirY = 1.0f;
    dirZ = -coss(light->yaw) * light->length;
    normalDotDir = wall->normal.x * dirX + wall->normal.y * dirY + wall->normal.z * dirZ;

    return normalDotDir < -0.05f || normalDotDir > 0.05f;
}

static u8 dynamic_shadow_object_is_held(struct Object *obj) {
    if (obj == NULL) {
        return FALSE;
    }

    return obj->oHeldState != HELD_FREE
        || (gMarioState != NULL && gMarioState->heldObj == obj);
}

static u8 dynamic_shadow_held_object_can_cast_dynamic_shadow(struct Object *obj) {
    return obj != NULL
        && dynamic_shadow_behavior_is(obj->behavior, bhvBowser);
}

static f32 dynamic_shadow_get_held_object_blob_scale(void) {
    const BehaviorScript *behavior;

    if (gCurGraphNodeHeldObject == NULL || gCurGraphNodeHeldObject->objNode == NULL) {
        return 80.0f;
    }

    behavior = gCurGraphNodeHeldObject->objNode->behavior;
    if (dynamic_shadow_behavior_is(behavior, bhvBreakableBoxSmall)
        || dynamic_shadow_behavior_is(behavior, bhvJumpingBox)) {
        return 90.0f;
    }

    return 80.0f;
}

static u8 dynamic_shadow_held_object_needs_blob_fallback(void) {
    const BehaviorScript *behavior;

    if (gCurGraphNodeHeldObject == NULL || gCurGraphNodeHeldObject->objNode == NULL) {
        return FALSE;
    }

    behavior = gCurGraphNodeHeldObject->objNode->behavior;
    return dynamic_shadow_behavior_is(behavior, bhvBreakableBoxSmall)
        || dynamic_shadow_behavior_is(behavior, bhvJumpingBox);
}

static void dynamic_shadow_surface_to_geometry(struct FloorGeometry *floorGeo, struct Surface *surface) {
    floorGeo->normalX = surface->normal.x;
    floorGeo->normalY = surface->normal.y;
    floorGeo->normalZ = surface->normal.z;
    floorGeo->originOffset = surface->originOffset + DYNAMIC_SHADOW_SURFACE_SINK;
}

static void dynamic_shadow_project_point_to_surface(Vec3f dest, Vec3f pos, struct Surface *surface,
                                                    const struct DynamicShadowLight *light) {
    f32 lift = light->lift > 0.12f ? 0.12f : light->lift;
    f32 dirX = -sins(light->yaw) * light->length;
    f32 dirY = 1.0f;
    f32 dirZ = -coss(light->yaw) * light->length;
    f32 normalDotDir = surface->normal.x * dirX + surface->normal.y * dirY + surface->normal.z * dirZ;
    f32 planeDist;

    if (normalDotDir > -0.05f && normalDotDir < 0.05f) {
        normalDotDir = normalDotDir < 0.0f ? -0.05f : 0.05f;
    }

    planeDist = (surface->normal.x * pos[0] + surface->normal.y * pos[1]
                 + surface->normal.z * pos[2]
                 + surface->originOffset + DYNAMIC_SHADOW_SURFACE_SINK - lift) / normalDotDir;
    dest[0] = pos[0] - dirX * planeDist;
    dest[1] = pos[1] - dirY * planeDist;
    dest[2] = pos[2] - dirZ * planeDist;
}

static u8 dynamic_shadow_floor_contains_xz(struct Surface *surface, Vec3f point) {
    f32 x = point[0];
    f32 z = point[2];
    f32 x1;
    f32 z1;
    f32 x2;
    f32 z2;
    f32 x3;
    f32 z3;

    if (surface == NULL) {
        return FALSE;
    }

    x1 = surface->vertex1[0];
    z1 = surface->vertex1[2];
    x2 = surface->vertex2[0];
    z2 = surface->vertex2[2];
    x3 = surface->vertex3[0];
    z3 = surface->vertex3[2];

    if ((z1 - z) * (x2 - x1) - (x1 - x) * (z2 - z1) < -12.0f) {
        return FALSE;
    }
    if ((z2 - z) * (x3 - x2) - (x2 - x) * (z3 - z2) < -12.0f) {
        return FALSE;
    }
    if ((z3 - z) * (x1 - x3) - (x3 - x) * (z1 - z3) < -12.0f) {
        return FALSE;
    }

    return TRUE;
}

static s16 dynamic_shadow_min_3s(s16 a, s16 b, s16 c) {
    if (b < a) {
        a = b;
    }
    if (c < a) {
        a = c;
    }
    return a;
}

static s16 dynamic_shadow_max_3s(s16 a, s16 b, s16 c) {
    if (b > a) {
        a = b;
    }
    if (c > a) {
        a = c;
    }
    return a;
}

static u8 dynamic_shadow_wall_contains_point(struct Surface *surface, Vec3f point) {
    f32 minX;
    f32 maxX;
    f32 minY;
    f32 maxY;
    f32 minZ;
    f32 maxZ;

    if (surface == NULL) {
        return FALSE;
    }

    minX = dynamic_shadow_min_3s(surface->vertex1[0], surface->vertex2[0], surface->vertex3[0]) - 24.0f;
    maxX = dynamic_shadow_max_3s(surface->vertex1[0], surface->vertex2[0], surface->vertex3[0]) + 24.0f;
    minY = surface->lowerY - 48.0f;
    maxY = surface->upperY + 48.0f;
    minZ = dynamic_shadow_min_3s(surface->vertex1[2], surface->vertex2[2], surface->vertex3[2]) - 24.0f;
    maxZ = dynamic_shadow_max_3s(surface->vertex1[2], surface->vertex2[2], surface->vertex3[2]) + 24.0f;

    return point[0] >= minX && point[0] <= maxX
        && point[1] >= minY && point[1] <= maxY
        && point[2] >= minZ && point[2] <= maxZ;
}

static f32 dynamic_shadow_surface_height_at(struct Surface *surface, f32 x, f32 z,
                                            const struct DynamicShadowLight *light) {
    f32 lift = light->lift > 0.12f ? 0.12f : light->lift;

    if (surface == NULL || (surface->normal.y > -0.001f && surface->normal.y < 0.001f)) {
        return -11000.0f;
    }

    return -(surface->normal.x * x + surface->normal.z * z
             + surface->originOffset + DYNAMIC_SHADOW_SURFACE_SINK - lift)
        / surface->normal.y;
}

#define DYNAMIC_SHADOW_MAX_RECEIVER_SURFACES 36
#define DYNAMIC_SHADOW_STEP_HEIGHT 40.0f
#define DYNAMIC_SHADOW_EDGE_GAP 28.0f

static f32 dynamic_shadow_absf(f32 value) {
    return value < 0.0f ? -value : value;
}

static u8 dynamic_shadow_surface_in_list(struct Surface **surfaces, s32 count,
                                         struct Surface *surface) {
    s32 i;

    for (i = 0; i < count; i++) {
        if (surfaces[i] == surface) {
            return TRUE;
        }
    }
    return FALSE;
}

static f32 dynamic_shadow_range_gap(f32 minA, f32 maxA, f32 minB, f32 maxB) {
    if (maxA < minB) {
        return minB - maxA;
    }
    if (maxB < minA) {
        return minA - maxB;
    }
    return 0.0f;
}

static void dynamic_shadow_surface_bounds(struct Surface *surface, Vec3f min, Vec3f max) {
    s32 axis;

    for (axis = 0; axis < 3; axis++) {
        min[axis] = surface->vertex1[axis];
        max[axis] = surface->vertex1[axis];
        if (surface->vertex2[axis] < min[axis]) min[axis] = surface->vertex2[axis];
        if (surface->vertex2[axis] > max[axis]) max[axis] = surface->vertex2[axis];
        if (surface->vertex3[axis] < min[axis]) min[axis] = surface->vertex3[axis];
        if (surface->vertex3[axis] > max[axis]) max[axis] = surface->vertex3[axis];
    }
}

static u8 dynamic_shadow_surfaces_are_continuous(struct Surface *a, struct Surface *b) {
    Vec3f minA;
    Vec3f maxA;
    Vec3f minB;
    Vec3f maxB;
    f32 gapX;
    f32 gapY;
    f32 gapZ;
    f32 normalDot;
    s32 matchingVertices = 0;
    s32 i;
    s32 j;
    Vec3s *aVertices[3] = { &a->vertex1, &a->vertex2, &a->vertex3 };
    Vec3s *bVertices[3] = { &b->vertex1, &b->vertex2, &b->vertex3 };

    if (a == b) {
        return TRUE;
    }
    if (!dynamic_shadow_surface_can_receive(b)) {
        return FALSE;
    }
    if (a->room != 0 && b->room != 0 && a->room != b->room) {
        return FALSE;
    }

    normalDot = a->normal.x * b->normal.x + a->normal.y * b->normal.y
        + a->normal.z * b->normal.z;
    if (normalDot < 0.35f) {
        return FALSE;
    }

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            f32 dx = (*aVertices[i])[0] - (*bVertices[j])[0];
            f32 dy = (*aVertices[i])[1] - (*bVertices[j])[1];
            f32 dz = (*aVertices[i])[2] - (*bVertices[j])[2];
            if (dx * dx + dz * dz <= 24.0f * 24.0f
                && dynamic_shadow_absf(dy) <= DYNAMIC_SHADOW_STEP_HEIGHT) {
                matchingVertices++;
                break;
            }
        }
    }
    if (matchingVertices >= 2) {
        return TRUE;
    }

    dynamic_shadow_surface_bounds(a, minA, maxA);
    dynamic_shadow_surface_bounds(b, minB, maxB);
    gapX = dynamic_shadow_range_gap(minA[0], maxA[0], minB[0], maxB[0]);
    gapY = dynamic_shadow_range_gap(minA[1], maxA[1], minB[1], maxB[1]);
    gapZ = dynamic_shadow_range_gap(minA[2], maxA[2], minB[2], maxB[2]);

    // Small stair steps may not share literal vertices. Permit one short gap,
    // but never bridge a cliff, stacked floor, or separated platform.
    if (gapY > DYNAMIC_SHADOW_STEP_HEIGHT || gapX > DYNAMIC_SHADOW_EDGE_GAP
        || gapZ > DYNAMIC_SHADOW_EDGE_GAP) {
        return FALSE;
    }
    if (gapX > 0.0f && gapZ > 0.0f
        && gapX * gapX + gapZ * gapZ > DYNAMIC_SHADOW_EDGE_GAP * DYNAMIC_SHADOW_EDGE_GAP) {
        return FALSE;
    }
    if (a->object != b->object && (a->object != NULL || b->object != NULL)
        && (gapY > 80.0f || gapX > 32.0f || gapZ > 32.0f)) {
        return FALSE;
    }

    return TRUE;
}

static u8 dynamic_shadow_surface_near_mask(struct Surface *surface, Vec3f center,
                                           f32 radius, f32 anchorHeight) {
    Vec3f min;
    Vec3f max;
    f32 dx = 0.0f;
    f32 dz = 0.0f;

    if (!dynamic_shadow_surface_can_receive(surface)) {
        return FALSE;
    }
    if (dynamic_shadow_surface_requires_blob_fallback(surface)) {
        return FALSE;
    }
    dynamic_shadow_surface_bounds(surface, min, max);
    if (max[1] < anchorHeight - DYNAMIC_SHADOW_STEP_HEIGHT
        || min[1] > anchorHeight + DYNAMIC_SHADOW_STEP_HEIGHT) {
        return FALSE;
    }
    if (center[0] < min[0]) dx = min[0] - center[0];
    else if (center[0] > max[0]) dx = center[0] - max[0];
    if (center[2] < min[2]) dz = min[2] - center[2];
    else if (center[2] > max[2]) dz = center[2] - max[2];
    return dx * dx + dz * dz <= radius * radius;
}

static void dynamic_shadow_collect_partition_surfaces(struct Surface **candidates, s32 *count,
                                                      struct SurfaceNode *node, Vec3f center,
                                                      f32 radius, f32 anchorHeight) {
    while (node != NULL && *count < DYNAMIC_SHADOW_MAX_RECEIVER_SURFACES) {
        struct Surface *surface = node->surface;
        if (!dynamic_shadow_surface_in_list(candidates, *count, surface)
            && dynamic_shadow_surface_near_mask(surface, center, radius, anchorHeight)) {
            candidates[(*count)++] = surface;
        }
        node = node->next;
    }
}

static s32 dynamic_shadow_collect_connected_surfaces(struct Surface *anchor, Vec3f center,
                                                     f32 radius, struct Surface **connected) {
    struct Surface *candidates[DYNAMIC_SHADOW_MAX_RECEIVER_SURFACES];
    s32 candidateCount = 0;
    s32 connectedCount = 0;
    s32 queueIndex = 0;
    s32 minCellX;
    s32 maxCellX;
    s32 minCellZ;
    s32 maxCellZ;
    s32 cellX;
    s32 cellZ;
    s32 i;
    f32 anchorHeight;

    if (anchor == NULL) {
        return 0;
    }
    anchorHeight = dynamic_shadow_surface_height_at(anchor, center[0], center[2],
                                                    dynamic_shadows_get_light());
    if (anchorHeight < -10000.0f) {
        anchorHeight = anchor->vertex1[1];
    }

    minCellX = (s32) ((center[0] - radius + LEVEL_BOUNDARY_MAX) / CELL_SIZE);
    maxCellX = (s32) ((center[0] + radius + LEVEL_BOUNDARY_MAX) / CELL_SIZE);
    minCellZ = (s32) ((center[2] - radius + LEVEL_BOUNDARY_MAX) / CELL_SIZE);
    maxCellZ = (s32) ((center[2] + radius + LEVEL_BOUNDARY_MAX) / CELL_SIZE);
    if (minCellX < 0) minCellX = 0;
    if (minCellZ < 0) minCellZ = 0;
    if (maxCellX > 15) maxCellX = 15;
    if (maxCellZ > 15) maxCellZ = 15;

    for (cellZ = minCellZ; cellZ <= maxCellZ; cellZ++) {
        for (cellX = minCellX; cellX <= maxCellX; cellX++) {
            dynamic_shadow_collect_partition_surfaces(
                candidates, &candidateCount,
                gStaticSurfacePartition[cellZ][cellX][SPATIAL_PARTITION_FLOORS].next,
                center, radius, anchorHeight);
            dynamic_shadow_collect_partition_surfaces(
                candidates, &candidateCount,
                gDynamicSurfacePartition[cellZ][cellX][SPATIAL_PARTITION_FLOORS].next,
                center, radius, anchorHeight);
        }
    }

    connected[connectedCount++] = anchor;
    while (queueIndex < connectedCount
           && connectedCount < DYNAMIC_SHADOW_MAX_RECEIVER_SURFACES) {
        struct Surface *from = connected[queueIndex++];
        for (i = 0; i < candidateCount; i++) {
            struct Surface *candidate = candidates[i];
            if (!dynamic_shadow_surface_in_list(connected, connectedCount, candidate)
                && dynamic_shadow_surfaces_are_continuous(from, candidate)) {
                connected[connectedCount++] = candidate;
                if (connectedCount >= DYNAMIC_SHADOW_MAX_RECEIVER_SURFACES) {
                    break;
                }
            }
        }
    }

    return connectedCount;
}

static u8 dynamic_shadow_surface_requires_blob_fallback(struct Surface *surface) {
    Vec3f min;
    Vec3f max;

    if (surface == NULL) {
        return FALSE;
    }

    if (gCurrLevelNum != LEVEL_BITS) {
        return FALSE;
    }

    dynamic_shadow_surface_bounds(surface, min, max);

    // BITS area 1 is drawn on the alpha layer, and a few large static chunks can
    // leak the receiver mask into the level display list state. Keep fallback
    // local to the high platforms that showed the black-face issue so the rest
    // of the course still receives dynamic shadows.
    return (max[0] >= -5600.0f && min[0] <= -900.0f
            && max[1] >= 3400.0f && min[1] <= 4100.0f
            && max[2] >= -1800.0f && min[2] <= -200.0f)
        || (max[0] >= -1800.0f && min[0] <= 900.0f
            && max[1] >= 2700.0f && min[1] <= 3450.0f
            && max[2] >= -1550.0f && min[2] <= -450.0f)
        || (max[0] >= -7900.0f && min[0] <= -4100.0f
            && max[1] >= 1800.0f && min[1] <= 2700.0f
            && max[2] >= -1900.0f && min[2] <= 150.0f)
        || (max[0] >= 650.0f && min[0] <= 2700.0f
            && max[1] >= 4200.0f && min[1] <= 4850.0f
            && max[2] >= -2700.0f && min[2] <= -150.0f)
        || (max[0] >= 6000.0f && min[0] <= 7900.0f
            && max[1] >= 3800.0f && min[1] <= 4700.0f
            && max[2] >= -3600.0f && min[2] <= -400.0f)
        || (max[0] >= -900.0f && min[0] <= 1600.0f
            && max[1] >= 5850.0f && min[1] <= 6600.0f
            && max[2] >= -4700.0f && min[2] <= -1900.0f);
}

static u8 dynamic_shadow_is_bits_allowed_enemy(void) {
    const BehaviorScript *behavior = dynamic_shadow_current_behavior();
    if (behavior == NULL) return FALSE;
    // Whitelist: only these enemies get dynamic shadows in BITS — everything
    // else falls back to vanilla blob shadow to avoid mask geometry leaking
    // stencil onto the problem platform below the octagonal rotating platform.
    return dynamic_shadow_behavior_is(behavior, bhvGoomba)
        || dynamic_shadow_behavior_is(behavior, bhvBobomb)
        || dynamic_shadow_behavior_is(behavior, bhvBobombBuddy)
        || dynamic_shadow_behavior_is(behavior, bhvSmallWhomp)
        || dynamic_shadow_behavior_is(behavior, bhvChuckya);
}

static u8 dynamic_shadow_is_bits_allowed_item(void) {
    const BehaviorScript *behavior = dynamic_shadow_current_behavior();
    if (behavior == NULL) return FALSE;
    // Whitelist: only these items get dynamic shadows in BITS — everything
    // else falls back to vanilla blob shadow to avoid mask geometry leaking
    // stencil onto the problem platform below the octagonal rotating platform.
    return dynamic_shadow_behavior_is(behavior, bhvPushableMetalBox)
        || dynamic_shadow_behavior_is(behavior, bhvExclamationBox)
        || dynamic_shadow_behavior_is(behavior, bhvPoleGrabbing)
        || dynamic_shadow_behavior_is(behavior, bhvCirclingAmp)
        || dynamic_shadow_behavior_is(behavior, bhvHomingAmp)
        || dynamic_shadow_behavior_is(behavior, bhvPiranhaPlant)
        || dynamic_shadow_behavior_is(behavior, bhvFirePiranhaPlant);
}

static u8 dynamic_shadow_is_bits_whitelisted(void) {
    return dynamic_shadow_is_mario_object()
        || dynamic_shadow_is_bits_allowed_enemy()
        || dynamic_shadow_is_bits_allowed_item();
}

static u8 dynamic_shadow_point_requires_blob_fallback(Vec3f point) {
    if (gCurrLevelNum != LEVEL_BITS) {
        return FALSE;
    }

    if (point[1] < 1200.0f || point[1] > 7200.0f) {
        return FALSE;
    }

    // The BITS object-platform chunks can leak dynamic receiver state by whole
    // coarse triangles. Use broad XZ zones here so casters standing on those
    // chunks fall back before their projected point gets pushed onto a neighbor.
    return (point[0] >= -5600.0f && point[0] <= -900.0f
            && point[2] >= -1800.0f && point[2] <= -200.0f)
        || (point[0] >= -1800.0f && point[0] <= 900.0f
            && point[2] >= -1550.0f && point[2] <= -450.0f)
        || (point[0] >= -7900.0f && point[0] <= -4100.0f
            && point[2] >= -1900.0f && point[2] <= 150.0f)
        || (point[0] >= 650.0f && point[0] <= 2700.0f
            && point[2] >= -2700.0f && point[2] <= -150.0f)
        || (point[0] >= 3300.0f && point[0] <= 7900.0f
            && point[2] >= -3600.0f && point[2] <= -400.0f)
        || (point[0] >= -900.0f && point[0] <= 1600.0f
            && point[2] >= -4700.0f && point[2] <= -1900.0f);
}

static u8 dynamic_shadow_projection_requires_blob_fallback(struct Surface *surface,
                                                           Vec3f projectedPoint) {
    return dynamic_shadow_surface_requires_blob_fallback(surface)
        || dynamic_shadow_point_requires_blob_fallback(projectedPoint);
}

static Gfx *dynamic_shadow_build_receiver_mask(struct Surface *anchor, Vec3f shadowPos,
                                               Vec3f projectedPoint, Mtx **maskMtx,
                                               Mtx **maskMtxInterpolated) {
    struct Surface *surfaces[DYNAMIC_SHADOW_MAX_RECEIVER_SURFACES];
    Vec3f center;
    Vtx *vertices;
    Gfx *displayList;
    Gfx *gfx;
    f32 dx;
    f32 dz;
    f32 radius;
    s16 objectRadius;
    s32 surfaceCount;
    s32 i;

    center[0] = (shadowPos[0] + projectedPoint[0]) * 0.5f;
    center[1] = (shadowPos[1] + projectedPoint[1]) * 0.5f;
    center[2] = (shadowPos[2] + projectedPoint[2]) * 0.5f;
    dx = projectedPoint[0] - shadowPos[0];
    dz = projectedPoint[2] - shadowPos[2];
    objectRadius = dynamic_shadow_get_object_culling_radius();
    radius = sqrtf(dx * dx + dz * dz) * 0.5f + objectRadius * 0.8f + 220.0f;
    if (radius < 360.0f) radius = 360.0f;
    if (radius > 1100.0f) radius = 1100.0f;

    surfaceCount = dynamic_shadow_collect_connected_surfaces(anchor, center, radius, surfaces);
    if (surfaceCount <= 0) {
        return NULL;
    }

    vertices = alloc_display_list(surfaceCount * 3 * sizeof(*vertices));
    displayList = alloc_display_list((surfaceCount * 2 + 1) * sizeof(*displayList));
    *maskMtx = alloc_display_list(sizeof(**maskMtx));
    *maskMtxInterpolated = alloc_display_list(sizeof(**maskMtxInterpolated));
    if (vertices == NULL || displayList == NULL || *maskMtx == NULL
        || *maskMtxInterpolated == NULL) {
        return NULL;
    }

    gfx = displayList;
    for (i = 0; i < surfaceCount; i++) {
        struct Surface *surface = surfaces[i];
        s32 vertexBase = i * 3;
        make_vertex(vertices, vertexBase + 0, surface->vertex1[0], surface->vertex1[1],
                    surface->vertex1[2], 0, 0, 255, 255, 255, 255);
        make_vertex(vertices, vertexBase + 1, surface->vertex2[0], surface->vertex2[1],
                    surface->vertex2[2], 0, 0, 255, 255, 255, 255);
        make_vertex(vertices, vertexBase + 2, surface->vertex3[0], surface->vertex3[1],
                    surface->vertex3[2], 0, 0, 255, 255, 255, 255);
        gSPVertex(gfx++, VIRTUAL_TO_PHYSICAL(vertices + vertexBase), 3, 0);
        gSP1Triangle(gfx++, 0, 1, 2, 0);
    }
    gSPEndDisplayList(gfx++);

    mtxf_to_mtx(*maskMtx, *gCurGraphNodeCamera->matrixPtr);
    mtxf_to_mtx(*maskMtxInterpolated, *gCurGraphNodeCamera->matrixPtrInterpolated);
    return (Gfx *) VIRTUAL_TO_PHYSICAL(displayList);
}

static Gfx *dynamic_shadow_get_receiver_mask(struct Surface *anchor, Vec3f shadowPos,
                                             Vec3f projectedPoint, Mtx **maskMtx,
                                             Mtx **maskMtxInterpolated, u16 *group) {
    struct DynamicShadowMaskCacheEntry *entry;
    Gfx *displayList;
    s32 i;

    for (i = 0; i < sDynamicShadowMaskCacheCount; i++) {
        entry = &sDynamicShadowMaskCache[i];
        if (entry->anchor == anchor) {
            *maskMtx = entry->transform;
            *maskMtxInterpolated = entry->transformInterpolated;
            *group = entry->group;
            return entry->displayList;
        }
    }

    displayList = dynamic_shadow_build_receiver_mask(anchor, shadowPos, projectedPoint,
                                                     maskMtx, maskMtxInterpolated);
    if (displayList == NULL) {
        return NULL;
    }

    *group = sNextDynamicShadowGroup++;
    if (sNextDynamicShadowGroup == 0 || sNextDynamicShadowGroup > 0xFF) {
        sNextDynamicShadowGroup = 1;
    }
    if (sDynamicShadowMaskCacheCount < DYNAMIC_SHADOW_MAX_MASK_CACHE) {
        entry = &sDynamicShadowMaskCache[sDynamicShadowMaskCacheCount++];
        entry->anchor = anchor;
        entry->displayList = displayList;
        entry->transform = *maskMtx;
        entry->transformInterpolated = *maskMtxInterpolated;
        entry->group = *group;
    }
    return displayList;
}

static u8 dynamic_shadow_floor_footprint_is_supported(Vec3f projectedPoint, Vec3f shadowPos,
                                                      struct Surface *surface,
                                                      const struct DynamicShadowLight *light) {
    struct Surface *checkFloor;
    Vec3f samplePoint;
    f32 sampleRadius;
    f32 expectedHeight;
    f32 checkHeight;
    f32 heightDelta;
    f32 sampleX;
    f32 sampleZ;
    s16 objectRadius;
    s32 i;
    static const s8 sSampleDirs[8][2] = {
        { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
        { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 },
    };

    objectRadius = dynamic_shadow_get_object_culling_radius();
    if (dynamic_shadow_is_mario_object()) {
        return TRUE;
    }

    sampleRadius = objectRadius * 0.42f;
    if (sampleRadius < 72.0f) {
        sampleRadius = 72.0f;
    }
    if (sampleRadius > 420.0f) {
        sampleRadius = 420.0f;
    }

    for (i = 0; i < 8; i++) {
        sampleX = projectedPoint[0] + sSampleDirs[i][0] * sampleRadius;
        sampleZ = projectedPoint[2] + sSampleDirs[i][1] * sampleRadius;
        expectedHeight = dynamic_shadow_surface_height_at(surface, sampleX, sampleZ, light);
        if (expectedHeight < -10000.0f) {
            return FALSE;
        }
        vec3f_set(samplePoint, sampleX, expectedHeight, sampleZ);
        if (!dynamic_shadow_floor_contains_xz(surface, samplePoint)) {
            return FALSE;
        }

        if (dynamic_shadow_is_platform_interaction_caster()) {
            checkHeight = dynamic_shadow_find_receivable_floor_at(
                sampleX, expectedHeight + 900.0f, sampleZ, &checkFloor);
        } else {
            checkHeight = find_floor(sampleX, expectedHeight + 900.0f, sampleZ, &checkFloor);
        }
        if (checkHeight < -10000.0f || !dynamic_shadow_surface_can_receive(checkFloor)) {
            return FALSE;
        }
        if (checkFloor != surface && checkFloor->object != surface->object) {
            return FALSE;
        }

        heightDelta = checkHeight - expectedHeight;
        if (heightDelta < 0.0f) {
            heightDelta = -heightDelta;
        }
        if (heightDelta > 8.0f) {
            return FALSE;
        }
    }

    return TRUE;
}

static u8 dynamic_shadow_floor_projection_is_physical(Vec3f shadowPos, struct Surface **floor,
                                                      const struct DynamicShadowLight *light,
                                                      Vec3f projectedPoint) {
    struct Surface *candidate;
    struct Surface *checkFloor;
    f32 checkHeight;
    f32 heightDelta;
    f32 maxProjectDist;
    s32 step;

    if (floor == NULL || *floor == NULL || light == NULL) {
        return FALSE;
    }

    candidate = *floor;
    for (step = 0; step < 3; step++) {
        dynamic_shadow_project_point_to_surface(projectedPoint, shadowPos, candidate, light);

        maxProjectDist = dynamic_shadow_is_mario_object() ? 2100.0f : 1700.0f;
        if (dynamic_shadow_is_platform_interaction_caster()) {
            maxProjectDist = 2600.0f;
        }
        if (dynamic_shadow_dist_sq_3f(shadowPos, projectedPoint) > maxProjectDist * maxProjectDist) {
            return FALSE;
        }
        if (dynamic_shadow_is_platform_interaction_caster()
            && projectedPoint[1] > shadowPos[1] + 32.0f) {
            return FALSE;
        }

        if (dynamic_shadow_is_platform_interaction_caster()) {
            checkHeight = dynamic_shadow_find_receivable_floor_at(
                projectedPoint[0], shadowPos[1] + 1800.0f, projectedPoint[2], &checkFloor);
        } else {
            checkHeight = find_floor(projectedPoint[0], shadowPos[1] + 1200.0f, projectedPoint[2], &checkFloor);
        }
        if (checkHeight < -10000.0f || !dynamic_shadow_surface_can_receive(checkFloor)) {
            return FALSE;
        }
        if (dynamic_shadow_is_platform_interaction_caster()
            && checkHeight > shadowPos[1] + 32.0f) {
            return FALSE;
        }

        heightDelta = checkHeight - projectedPoint[1];
        if (heightDelta < 0.0f) {
            heightDelta = -heightDelta;
        }

        if (heightDelta >= 48.0f) {
            return FALSE;
        }

        if (checkFloor == candidate
            || dynamic_shadow_floor_contains_xz(checkFloor, projectedPoint)
            || (candidate->object != NULL && checkFloor->object == candidate->object)) {
            *floor = checkFloor;
            return dynamic_shadow_floor_footprint_is_supported(projectedPoint, shadowPos, *floor, light);
        }

        // The seed surface may be Mario's current floor, but the projected
        // landing point often crosses onto a neighboring triangle. Re-project
        // once on the actual supporting floor so slopes and BOB-style terrain
        // do not lose shadows at triangle borders.
        candidate = checkFloor;
    }

    return FALSE;
}

static f32 dynamic_shadow_find_receiver_floor(Vec3f shadowPos, struct Surface **floor) {
    struct Object *caster = (struct Object *) gCurGraphNodeObject;
    f32 floorHeight;
    f32 maxHeightFromFloor = 1200.0f;
    f32 minHeightFromFloor = -80.0f;
    f32 queryHeight = 1200.0f;
    u8 isPlatformInteractionCaster;

    *floor = NULL;
    isPlatformInteractionCaster = dynamic_shadow_is_platform_interaction_caster();
    if (isPlatformInteractionCaster) {
        maxHeightFromFloor = 2600.0f;
    }
    if (dynamic_shadow_is_platform_shadow_caster_allowed()) {
        maxHeightFromFloor = 3400.0f;
        minHeightFromFloor = -900.0f;
        queryHeight = 2600.0f;
    } else if (dynamic_shadow_is_whomp_like()) {
        minHeightFromFloor = -300.0f;
    }

    if (dynamic_shadow_is_ending_peach_cutscene_actor()) {
        floorHeight = find_floor(shadowPos[0], shadowPos[1] + queryHeight, shadowPos[2], floor);
        if (floorHeight > -10000.0f && *floor != NULL
            && dynamic_shadow_surface_can_receive(*floor)
            && shadowPos[1] - floorHeight >= minHeightFromFloor
            && shadowPos[1] - floorHeight < maxHeightFromFloor) {
            return floorHeight;
        }
        *floor = NULL;
        return -11000.0f;
    }

    if (dynamic_shadow_is_whomp_like() && caster != NULL && caster->oFloor != NULL
        && caster->oFloor->object != caster) {
        floorHeight = caster->oFloorHeight;
        if (floorHeight > -10000.0f
            && shadowPos[1] - floorHeight >= minHeightFromFloor
            && shadowPos[1] - floorHeight < maxHeightFromFloor) {
            *floor = caster->oFloor;
            if (dynamic_shadow_surface_can_receive(*floor)) {
                return floorHeight;
            }
        }
    }

    if (isPlatformInteractionCaster && caster != NULL) {
        // Collision-contributing objects can report their own contact/top
        // surface as oFloor exactly when Mario gets close enough to interact.
        // Resolve the receiver from nearby world collision first so Whomps,
        // pushable boxes, signs, and large platforms do not snap back to the
        // vanilla blob/shadowless path at interaction range.
        floorHeight = dynamic_shadow_find_receivable_floor_near(
            shadowPos, queryHeight, minHeightFromFloor, maxHeightFromFloor, floor);
        if (floorHeight > -10000.0f && *floor != NULL) {
            return floorHeight;
        }
    }

    if (caster == gMarioObject && gMarioState != NULL && gMarioState->floor != NULL) {
        floorHeight = gMarioState->floorHeight;
        if (floorHeight > -10000.0f
            && shadowPos[1] - floorHeight >= minHeightFromFloor
            && shadowPos[1] - floorHeight < maxHeightFromFloor) {
            *floor = gMarioState->floor;
            if (dynamic_shadow_surface_can_receive(*floor)) {
                return floorHeight;
            }
        }
    } else if (caster != NULL && caster->oFloor != NULL
        && (!isPlatformInteractionCaster || caster->oFloor->object != caster)) {
        floorHeight = caster->oFloorHeight;
        if (floorHeight > -10000.0f
            && shadowPos[1] - floorHeight >= minHeightFromFloor
            && shadowPos[1] - floorHeight < maxHeightFromFloor) {
            *floor = caster->oFloor;
            if (dynamic_shadow_surface_can_receive(*floor)) {
                return floorHeight;
            }
        }
    }

    if (isPlatformInteractionCaster && caster != NULL) {
        floorHeight = dynamic_shadow_find_receivable_floor_near(
            shadowPos, queryHeight, minHeightFromFloor, maxHeightFromFloor, floor);
    } else {
        floorHeight = find_floor(shadowPos[0], shadowPos[1] + queryHeight, shadowPos[2], floor);
    }
    if (floorHeight < -10000.0f || *floor == NULL || !dynamic_shadow_surface_can_receive(*floor)) {
        *floor = NULL;
        return -11000.0f;
    }
    if (shadowPos[1] - floorHeight < minHeightFromFloor
        || shadowPos[1] - floorHeight >= maxHeightFromFloor) {
        *floor = NULL;
        return -11000.0f;
    }

    return floorHeight;
}

static struct Surface *dynamic_shadow_find_receiver_wall(Vec3f shadowPos, struct Surface *floor,
                                                         const struct DynamicShadowLight *light) {
    Vec3f floorTarget;
    Vec3f wallTarget;
    struct WallCollisionData collision;
    struct Surface *wall;
    f32 dx;
    f32 dy;
    f32 dz;
    f32 t;
    s32 step;
    s32 i;

    if (floor == NULL || light == NULL) {
        return NULL;
    }

    dynamic_shadow_project_point_to_surface(floorTarget, shadowPos, floor, light);
    dx = floorTarget[0] - shadowPos[0];
    dy = floorTarget[1] - shadowPos[1];
    dz = floorTarget[2] - shadowPos[2];
    if (dx * dx + dy * dy + dz * dz < 100.0f) {
        return NULL;
    }

    collision.offsetY = 0.0f;
    collision.radius = 40.0f;

    for (step = 1; step <= 3; step++) {
        t = step / 3.0f;
        collision.x = shadowPos[0] + dx * t;
        collision.y = shadowPos[1] + dy * t;
        collision.z = shadowPos[2] + dz * t;
        collision.numWalls = 0;

        if (find_wall_collisions(&collision) == 0) {
            continue;
        }

        for (i = 0; i < collision.numWalls; i++) {
            wall = collision.walls[i];
            if (dynamic_shadow_wall_can_receive(wall, light)) {
                dynamic_shadow_project_point_to_surface(wallTarget, shadowPos, wall, light);
                if (!dynamic_shadow_wall_contains_point(wall, wallTarget)) {
                    continue;
                }
                if (dynamic_shadow_dist_sq_3f(shadowPos, wallTarget) > 1700.0f * 1700.0f) {
                    continue;
                }
                return wall;
            }
        }
    }

    return NULL;
}

static u8 dynamic_shadow_find_projection_surface(Vec3f shadowPos, struct Surface **surface,
                                                 const struct DynamicShadowLight *light) {
    f32 floorHeight;
    Vec3f floorTarget;
    f32 maxProjectDist;
    struct Surface *floor;

    *surface = NULL;
    floorHeight = dynamic_shadow_find_receiver_floor(shadowPos, &floor);
    if (floorHeight < -10000.0f || floor == NULL) {
        return FALSE;
    }

    // The original blob floor is the authoritative receiver plane. The
    // connected receiver mask clips the projected model at stairs and cliffs;
    // never retarget the projection to an arbitrary floor under its far edge.
    dynamic_shadow_project_point_to_surface(floorTarget, shadowPos, floor, light);
    maxProjectDist = dynamic_shadow_is_mario_object() ? 2100.0f : 1700.0f;
    if (dynamic_shadow_behavior_is_crushing_platform_like(dynamic_shadow_current_behavior())
        || dynamic_shadow_is_platform_shadow_caster_allowed()) {
        maxProjectDist = 2600.0f;
    }
    if (dynamic_shadow_is_platform_shadow_caster_allowed()) {
        maxProjectDist = 3400.0f;
    }
    if (dynamic_shadow_dist_sq_3f(shadowPos, floorTarget) > maxProjectDist * maxProjectDist) {
        return FALSE;
    }

    *surface = floor;
    return TRUE;
}

static u8 dynamic_shadow_can_render_model_object(struct GraphNode *children) {
    struct Object *obj;

    if (!dynamic_shadows_should_render() || dynamic_shadows_is_rendering_mode()) {
        gDynamicShadowDebugRejectMode++;
        return FALSE;
    }
    if (gCurGraphNodeCamera == NULL || gCurGraphNodeObject == NULL) {
        gDynamicShadowDebugRejectContext++;
        return FALSE;
    }
    if (children == NULL) {
        gDynamicShadowDebugRejectChildren++;
        return FALSE;
    }
    // In BITS, only Mario and select enemies/items get dynamic shadows — all
    // other objects fall back to vanilla blob shadow.  This avoids mask geometry
    // leaking stencil onto the problem platform below the octagonal rotating
    // platform.  Enemy whitelist: Goomba, Bob-omb/Buddy, SmallWhomp, Chuckya.
    // Item whitelist: PushableMetalBox, ExclamationBox, PoleGrabbing, Amp, PiranhaPlant.
    if (gCurrLevelNum == LEVEL_BITS
        && !dynamic_shadow_is_mario_object()
        && !dynamic_shadow_is_bits_allowed_enemy()
        && !dynamic_shadow_is_bits_allowed_item()) {
        gDynamicShadowDebugRejectPlatform++;
        return FALSE;
    }
    obj = (struct Object *) gCurGraphNodeObject;
    if (dynamic_shadow_object_is_held(obj)
        && !dynamic_shadow_held_object_can_cast_dynamic_shadow(obj)) {
        gDynamicShadowDebugRejectContext++;
        return FALSE;
    }

    // Spatial checks first — only objects near Mario and visible to the
    // camera are candidates for dynamic shadows, regardless of budget.
    if (dynamic_shadow_is_model_object_far()) {
        gDynamicShadowDebugRejectFar++;
        return FALSE;
    }

    // Object-type exclusions
    if (dynamic_shadow_is_bobomb_like()) {
        gDynamicShadowDebugRejectBobomb++;
        return FALSE;
    }
    if (dynamic_shadow_is_water_surface_effect()) {
        gDynamicShadowDebugRejectBillboard++;
        return FALSE;
    }
    if (dynamic_shadow_is_coin()) {
        gDynamicShadowDebugRejectBillboard++;
        return FALSE;
    }
    if (dynamic_shadow_is_tree_like()) {
        gDynamicShadowDebugRejectBillboard++;
        return FALSE;
    }
    if (dynamic_shadow_is_fixed_interaction_object()
        && !dynamic_shadow_is_platform_interaction_caster()) {
        gDynamicShadowDebugRejectPlatform++;
        return FALSE;
    }
    if (dynamic_shadow_is_wf_terrain_object()) {
        gDynamicShadowDebugRejectPlatform++;
        return FALSE;
    }
    if (dynamic_shadow_is_jrb_terrain_object()) {
        gDynamicShadowDebugRejectPlatform++;
        return FALSE;
    }
    if (dynamic_shadow_is_excluded_platform()) {
        gDynamicShadowDebugRejectPlatform++;
        return FALSE;
    }
    if (dynamic_shadow_is_ghost_platform_caster()) {
        gDynamicShadowDebugRejectPlatform++;
        return FALSE;
    }
    if (dynamic_shadow_is_large_platform_shadow_caster()) {
        gDynamicShadowDebugRejectPlatform++;
        return FALSE;
    }
    if (dynamic_shadow_is_receiver_only_platform()
        && !dynamic_shadow_is_platform_interaction_caster()) {
        gDynamicShadowDebugRejectPlatform++;
        return FALSE;
    }
    if ((gCurGraphNodeObject->node.flags & GRAPH_RENDER_BILLBOARD) != 0
        && !dynamic_shadow_allows_shadow_alpha_layer()) {
        gDynamicShadowDebugRejectBillboard++;
        return FALSE;
    }

    // Safety cap: once the budget is exhausted, fall back to original shadows
    // for any remaining spatially-valid objects.
    if (!dynamic_shadow_is_platform_interaction_caster()
        && gDynamicShadowDebugAppendLists >= DYNAMIC_SHADOW_MAX_APPENDED_LISTS) {
        gDynamicShadowDebugRejectFar++;
        return FALSE;
    }

    return TRUE;
}

static u8 dynamic_shadow_should_keep_original_shadow(struct GraphNodeShadow *node, Vec3f shadowPos) {
    Vec3f anchorPos;
    Vec3f projectedPoint;
    f32 floorHeight;
    struct Surface *floor;
    struct Surface *projectionSurface;
    const struct DynamicShadowLight *light;

    if (dynamic_shadows_should_render()
        && node->shadowType == 0x01 + SHADOW_RECTANGLE_HARDCODED_OFFSET
        && dynamic_shadow_is_whomp_like()) {
        return !sCurrentObjectDynamicShadowGenerated;
    }
    if (dynamic_shadow_should_hide_ending_peach_blob()) {
        return FALSE;
    }
    if (dynamic_shadow_should_use_original_hitbox_blob()) {
        return TRUE;
    }
    if (dynamic_shadow_should_disable_ending_actor_dynamic_shadow()) {
        return TRUE;
    }
    if (dynamic_shadow_never_keep_original_shadow()) {
        return FALSE;
    }

    if ((struct Object *) gCurGraphNodeObject == gMarioObject || node->shadowType == SHADOW_CIRCLE_PLAYER) {
        if (!dynamic_shadow_can_render_model_object(node->node.children)) {
            return TRUE;
        }

        light = dynamic_shadows_get_light();
        dynamic_shadow_get_model_anchor(anchorPos, shadowPos);
        if (!dynamic_shadow_is_bits_whitelisted()) {
            if (dynamic_shadow_point_requires_blob_fallback(anchorPos)) {
                return TRUE;
            }
        }
        if (!dynamic_shadow_find_projection_surface(anchorPos, &projectionSurface, light)) {
            return TRUE;
        }
        dynamic_shadow_project_point_to_surface(projectedPoint, anchorPos, projectionSurface, light);
        if (!dynamic_shadow_is_bits_whitelisted()
            && dynamic_shadow_projection_requires_blob_fallback(projectionSurface, projectedPoint)) {
            return TRUE;
        }

        floorHeight = dynamic_shadow_find_receiver_floor(anchorPos, &floor);
        if (floorHeight < -10000.0f || floor == NULL) {
            return TRUE;
        }

        return shadowPos[1] - floorHeight > 60.0f;
    }

    if (sCurrentObjectDynamicShadowGenerated) {
        return FALSE;
    }
    if (dynamic_shadow_is_platform_interaction_caster()) {
        return TRUE;
    }
    if (!dynamic_shadow_can_render_model_object(node->node.children)) {
        return TRUE;
    }

    light = dynamic_shadows_get_light();
    dynamic_shadow_get_model_anchor(anchorPos, shadowPos);
    if (!dynamic_shadow_is_bits_whitelisted()) {
        if (dynamic_shadow_point_requires_blob_fallback(anchorPos)) {
            return TRUE;
        }
    }
    if (!dynamic_shadow_find_projection_surface(anchorPos, &projectionSurface, light)) {
        return TRUE;
    }
    dynamic_shadow_project_point_to_surface(projectedPoint, anchorPos, projectionSurface, light);
    if (!dynamic_shadow_is_bits_whitelisted()
        && dynamic_shadow_projection_requires_blob_fallback(projectionSurface, projectedPoint)) {
        return TRUE;
    }

    floorHeight = dynamic_shadow_find_receiver_floor(anchorPos, &floor);
    if (floorHeight < -10000.0f || floor == NULL) {
        return TRUE;
    }

    return FALSE;
}

static void dynamic_shadow_get_model_anchor(Vec3f dst, Vec3f shadowPos) {
    struct Object *obj;
    struct Object *parent;

    vec3f_copy(dst, shadowPos);

    if (!dynamic_shadow_is_mario_object() && gCurGraphNodeObject != NULL
        && (dynamic_shadow_is_platform_interaction_caster() || !sUseShadowNodeAnchor)) {
        vec3f_copy(dst, gCurGraphNodeObject->pos);
    }
    if (dynamic_shadow_is_cannon_base_object()) {
        obj = (struct Object *) gCurGraphNodeObject;
        if (obj != NULL && obj->oFloorHeight > -10000.0f) {
            dst[1] = obj->oFloorHeight + 180.0f;
        } else if (obj != NULL) {
            dst[1] = obj->oHomeY + 180.0f;
        } else {
            dst[1] += 340.0f;
        }
    } else if (dynamic_shadow_is_cannon_barrel_object()) {
        obj = (struct Object *) gCurGraphNodeObject;
        parent = obj != NULL ? obj->parentObj : NULL;
        if (parent != NULL && parent->oFloorHeight > -10000.0f) {
            dst[1] = parent->oFloorHeight + 220.0f;
        } else if (parent != NULL) {
            dst[1] = parent->oHomeY + 220.0f;
        } else if (obj != NULL && obj->oFloorHeight > -10000.0f) {
            dst[1] = obj->oFloorHeight + 220.0f;
        }
    }
}

static u8 dynamic_shadow_get_original_solidity(struct GraphNodeShadow *node, Vec3f shadowPos) {
    f32 floorHeight;
    f32 heightAboveFloor;
    f32 fadeStart = 60.0f;
    f32 fadeEnd = 140.0f;
    f32 fade;
    struct Surface *floor;

    if ((struct Object *) gCurGraphNodeObject != gMarioObject && node->shadowType != SHADOW_CIRCLE_PLAYER) {
        return node->shadowSolidity;
    }

    floorHeight = dynamic_shadow_find_receiver_floor(shadowPos, &floor);
    if (floorHeight < -10000.0f || floor == NULL) {
        return node->shadowSolidity;
    }
    if (dynamic_shadow_surface_requires_blob_fallback(floor)
        || dynamic_shadow_point_requires_blob_fallback(shadowPos)) {
        return node->shadowSolidity;
    }

    heightAboveFloor = shadowPos[1] - floorHeight;
    if (heightAboveFloor <= fadeStart) {
        return 0;
    }
    if (heightAboveFloor >= fadeEnd) {
        return node->shadowSolidity;
    }

    fade = (heightAboveFloor - fadeStart) / (fadeEnd - fadeStart);
    return (u8) (node->shadowSolidity * fade);
}

static u8 geo_process_dynamic_model_shadow(struct GraphNode *children, Vec3f shadowPos) {
    Mat4 objectMtx;
    Mat4 projectionMtx;
    Mat4 projectedMtx;
    Mat4 shadowMtx;
    Mat4 shadowMtxInterpolated;
    Mtx *mtx;
    Mtx *mtxInterpolated;
    Mtx *maskMtx;
    Mtx *maskMtxInterpolated;
    Gfx *receiverMask;
    u16 receiverGroup;
    Vec3f anchorPos;
    Vec3f projectedPoint;
    struct GeoAnimState savedAnimState;
    struct Surface *surface;
    struct FloorGeometry floorGeo;
    const struct DynamicShadowLight *light;
    u8 savedUnderwaterTint;
    f32 savedBillboardGeoScale;
    s32 appendedListsBefore;

    gDynamicShadowDebugShadowNodes++;
    if (dynamic_shadow_should_disable_ending_actor_dynamic_shadow()) {
        return FALSE;
    }
    if (dynamic_shadow_should_use_original_hitbox_blob()) {
        return FALSE;
    }
    if (!dynamic_shadow_can_render_model_object(children)) {
        return FALSE;
    }

    light = dynamic_shadows_get_light();
    dynamic_shadow_get_model_anchor(anchorPos, shadowPos);
    if (!dynamic_shadow_is_bits_whitelisted()) {
        if (dynamic_shadow_point_requires_blob_fallback(anchorPos)) {
            gDynamicShadowDebugRejectPlatform++;
            return FALSE;
        }
    }
    if (!dynamic_shadow_find_projection_surface(anchorPos, &surface, light)) {
        gDynamicShadowDebugRejectLight++;
        return FALSE;
    }
    dynamic_shadow_project_point_to_surface(projectedPoint, anchorPos, surface, light);
    if (!dynamic_shadow_is_bits_whitelisted()
        && dynamic_shadow_projection_requires_blob_fallback(surface, projectedPoint)) {
        gDynamicShadowDebugRejectPlatform++;
        return FALSE;
    }
    gDynamicShadowDebugCandidateObjects++;

    receiverMask = dynamic_shadow_get_receiver_mask(surface, anchorPos, projectedPoint,
                                                    &maskMtx, &maskMtxInterpolated,
                                                    &receiverGroup);
    if (receiverMask == NULL) {
        gDynamicShadowDebugRejectLight++;
        return FALSE;
    }

    dynamic_shadow_surface_to_geometry(&floorGeo, surface);
    dynamic_shadow_make_projection_mtx(projectionMtx, &floorGeo, light);

    if (sDynamicShadowModelMtxOverride != NULL) {
        mtxf_copy(objectMtx, *sDynamicShadowModelMtxOverride);
    } else {
        mtxf_rotate_zxy_and_translate(objectMtx, gCurGraphNodeObject->pos, gCurGraphNodeObject->angle);
        mtxf_scale_vec3f(objectMtx, objectMtx, gCurGraphNodeObject->scale);
    }
    mtxf_mul(projectedMtx, objectMtx, projectionMtx);
    mtxf_mul(shadowMtx, projectedMtx, *gCurGraphNodeCamera->matrixPtr);
    if (sDynamicShadowModelMtxInterpolatedOverride != NULL) {
        mtxf_mul(projectedMtx, *sDynamicShadowModelMtxInterpolatedOverride, projectionMtx);
    }
    mtxf_mul(shadowMtxInterpolated, projectedMtx, *gCurGraphNodeCamera->matrixPtrInterpolated);

    // Nudge the projected shadow slightly above the ground surface to avoid
    // z-fighting flicker.  The projection matrix flattens the model onto the
    // ground plane, so without this offset the shadow polygon sits at exactly
    // the same depth as the floor.
    shadowMtx[3][1] += 1.5f;
    shadowMtxInterpolated[3][1] += 1.5f;

    savedAnimState.type = gCurAnimType;
    savedAnimState.enabled = gCurAnimEnabled;
    savedAnimState.frame = gCurrAnimFrame;
    savedAnimState.prevFrame = gPrevAnimFrame;
    savedAnimState.translationMultiplier = gCurAnimTranslationMultiplier;
    savedAnimState.attribute = gCurrAnimAttribute;
    savedAnimState.data = gCurAnimData;

    mtx = alloc_display_list(sizeof(*mtx));
    mtxInterpolated = alloc_display_list(sizeof(*mtxInterpolated));
    if (mtx == NULL || mtxInterpolated == NULL) {
        return FALSE;
    }

    gMatStackIndex++;
    mtxf_copy(gMatStack[gMatStackIndex], shadowMtx);
    mtxf_copy(gMatStackInterpolated[gMatStackIndex], shadowMtxInterpolated);
    mtxf_to_mtx(mtx, gMatStack[gMatStackIndex]);
    mtxf_to_mtx(mtxInterpolated, gMatStackInterpolated[gMatStackIndex]);
    gMatStackFixed[gMatStackIndex] = mtx;
    gMatStackInterpolatedFixed[gMatStackIndex] = mtxInterpolated;

    dynamic_shadows_set_rendering_mode(TRUE);
    sAppendingDynamicShadowMask = receiverMask;
    sAppendingDynamicShadowMaskTransform = maskMtx;
    sAppendingDynamicShadowMaskTransformInterpolated = maskMtxInterpolated;
    sAppendingDynamicShadowSurface = surface;
    sAppendingDynamicShadowGroup = receiverGroup;
    savedUnderwaterTint = gDynamicShadowUnderwaterTint;
    savedBillboardGeoScale = sDynamicShadowBillboardGeoScale;
    gDynamicShadowUnderwaterTint =
        dynamic_shadow_get_footprint_tint(surface, anchorPos, projectedPoint);
    sDynamicShadowBillboardGeoScale = 1.0f;
    sAppendingDynamicShadow = TRUE;
    appendedListsBefore = gDynamicShadowDebugAppendLists;
    geo_process_node_and_siblings(children);
    sAppendingDynamicShadow = FALSE;
    sDynamicShadowBillboardGeoScale = savedBillboardGeoScale;
    gDynamicShadowUnderwaterTint = savedUnderwaterTint;
    sAppendingDynamicShadowMask = NULL;
    sAppendingDynamicShadowMaskTransform = NULL;
    sAppendingDynamicShadowMaskTransformInterpolated = NULL;
    sAppendingDynamicShadowSurface = NULL;
    sAppendingDynamicShadowGroup = 0;
    dynamic_shadows_set_rendering_mode(FALSE);

    gMatStackIndex--;

    gCurAnimType = savedAnimState.type;
    gCurAnimEnabled = savedAnimState.enabled;
    gCurrAnimFrame = savedAnimState.frame;
    gPrevAnimFrame = savedAnimState.prevFrame;
    gCurAnimTranslationMultiplier = savedAnimState.translationMultiplier;
    gCurrAnimAttribute = savedAnimState.attribute;
    gCurAnimData = savedAnimState.data;

    return gDynamicShadowDebugAppendLists > appendedListsBefore;
}
#endif

/**
 * Initialize the animation-related global variables for the currently drawn
 * object's animation.
 */
void geo_set_animation_globals(struct GraphNodeObject_sub *node, s32 hasAnimation) {
    struct Animation *anim = node->curAnim;

    if (hasAnimation != 0) {
        node->animFrame = geo_update_animation_frame(node, &node->animFrameAccelAssist);
    }
    node->animTimer = gAreaUpdateCounter;
    if (anim->flags & ANIM_FLAG_HOR_TRANS) {
        gCurAnimType = ANIM_TYPE_VERTICAL_TRANSLATION;
    } else if (anim->flags & ANIM_FLAG_VERT_TRANS) {
        gCurAnimType = ANIM_TYPE_LATERAL_TRANSLATION;
    } else if (anim->flags & ANIM_FLAG_6) {
        gCurAnimType = ANIM_TYPE_NO_TRANSLATION;
    } else {
        gCurAnimType = ANIM_TYPE_TRANSLATION;
    }

    gCurrAnimFrame = node->animFrame;
    if (node->prevAnimPtr == anim && node->prevAnimID == node->animID &&
        gGlobalTimer == node->prevAnimFrameTimestamp + 1) {
        gPrevAnimFrame = node->prevAnimFrame;
    } else {
        gPrevAnimFrame = node->animFrame;
    }
    node->prevAnimPtr = anim;
    node->prevAnimID = node->animID;
    node->prevAnimFrame = node->animFrame;
    node->prevAnimFrameTimestamp = gGlobalTimer;

    gCurAnimEnabled = (anim->flags & ANIM_FLAG_5) == 0;
    gCurrAnimAttribute = segmented_to_virtual((void *) anim->index);
    gCurAnimData = segmented_to_virtual((void *) anim->values);

    if (anim->unk02 == 0) {
        gCurAnimTranslationMultiplier = 1.0f;
    } else {
        gCurAnimTranslationMultiplier = (f32) node->animYTrans / (f32) anim->unk02;
    }
}

/**
 * Process a shadow node. Renders a shadow under an object offset by the
 * translation of the first animated component and rotated according to
 * the floor below it.
 */
static void geo_process_shadow(struct GraphNodeShadow *node) {
    Gfx *shadowList;
    Gfx *shadowListInterpolated;
    Mat4 mtxf;
    Vec3f shadowPos;
    Vec3f shadowPosInterpolated;
    Vec3f animOffset;
    f32 objScale;
    f32 shadowScale;
    f32 sinAng;
    f32 cosAng;
    struct GraphNode *geo;
    Mtx *mtx;
    Mtx *mtxInterpolated;

#ifdef TARGET_N3DS
    if (dynamic_shadows_is_rendering_mode()) {
        if (node->node.children != NULL) {
            geo_process_node_and_siblings(node->node.children);
        }
        return;
    }
#endif

    if (gCurGraphNodeCamera != NULL && gCurGraphNodeObject != NULL) {
        if (gCurGraphNodeHeldObject != NULL) {
            get_pos_from_transform_mtx(shadowPos, gMatStack[gMatStackIndex],
                                       *gCurGraphNodeCamera->matrixPtr);
            shadowScale = node->shadowScale;
        } else {
#ifdef TARGET_N3DS
            // Ending/staff actors can have oPos driven directly by cutscene
            // code, bypassing the usual gfx position refresh.
            dynamic_shadow_sync_ending_cutscene_object_pos((struct Object *) gCurGraphNodeObject);
#endif
            vec3f_copy(shadowPos, gCurGraphNodeObject->pos);
            shadowScale = node->shadowScale * gCurGraphNodeObject->scale[0];
        }

        objScale = 1.0f;
        if (gCurAnimEnabled != 0) {
            if (gCurAnimType == ANIM_TYPE_TRANSLATION
                || gCurAnimType == ANIM_TYPE_LATERAL_TRANSLATION) {
                geo = node->node.children;
                if (geo != NULL && geo->type == GRAPH_NODE_TYPE_SCALE) {
                    objScale = ((struct GraphNodeScale *) geo)->scale;
                }
                animOffset[0] =
                    gCurAnimData[retrieve_animation_index(gCurrAnimFrame, &gCurrAnimAttribute)]
                    * gCurAnimTranslationMultiplier * objScale;
                animOffset[1] = 0.0f;
                gCurrAnimAttribute += 2;
                animOffset[2] =
                    gCurAnimData[retrieve_animation_index(gCurrAnimFrame, &gCurrAnimAttribute)]
                    * gCurAnimTranslationMultiplier * objScale;
                gCurrAnimAttribute -= 6;

                // simple matrix rotation so the shadow offset rotates along with the object
                sinAng = sins(gCurGraphNodeObject->angle[1]);
                cosAng = coss(gCurGraphNodeObject->angle[1]);

                shadowPos[0] += animOffset[0] * cosAng + animOffset[2] * sinAng;
                shadowPos[2] += -animOffset[0] * sinAng + animOffset[2] * cosAng;
            }
        }

        if (gCurGraphNodeHeldObject != NULL) {
            if (gGlobalTimer == gCurGraphNodeHeldObject->prevShadowPosTimestamp + 1) {
                interpolate_vectors(shadowPosInterpolated, gCurGraphNodeHeldObject->prevShadowPos, shadowPos);
            } else {
                vec3f_copy(shadowPosInterpolated, shadowPos);
            }
            vec3f_copy(gCurGraphNodeHeldObject->prevShadowPos, shadowPos);
            gCurGraphNodeHeldObject->prevShadowPosTimestamp = gGlobalTimer;
        } else {
            if (gGlobalTimer == gCurGraphNodeObject->prevShadowPosTimestamp + 1 &&
                gGlobalTimer != gCurGraphNodeObject->skipInterpolationTimestamp) {
                interpolate_vectors(shadowPosInterpolated, gCurGraphNodeObject->prevShadowPos, shadowPos);
            } else {
                vec3f_copy(shadowPosInterpolated, shadowPos);
            }
            vec3f_copy(gCurGraphNodeObject->prevShadowPos, shadowPos);
            gCurGraphNodeObject->prevShadowPosTimestamp = gGlobalTimer;
        }

#ifdef TARGET_N3DS
        u8 keepOriginalShadow;
        u8 originalShadowType = node->shadowType;
        u8 shouldCheckOriginalShadow =
            !sCurrentObjectDynamicShadowGenerated
            || (gCurGraphNodeObject != NULL
                && ((struct Object *) gCurGraphNodeObject == gMarioObject
                    || node->shadowType == SHADOW_CIRCLE_PLAYER));
        if (gCurGraphNodeHeldObject == NULL
            && !sCurrentObjectDynamicShadowGenerated && node->node.children != NULL) {
            u8 savedUseShadowNodeAnchor = sUseShadowNodeAnchor;
            sUseShadowNodeAnchor = TRUE;
            sDynamicShadowBillboardScale = shadowScale;
            sCurrentObjectDynamicShadowGenerated =
                geo_process_dynamic_model_shadow(node->node.children, shadowPos);
            sDynamicShadowBillboardScale = 0.0f;
            sUseShadowNodeAnchor = savedUseShadowNodeAnchor;
        }
        if (gCurGraphNodeHeldObject != NULL) {
            keepOriginalShadow = !sCurrentObjectDynamicShadowGenerated;
        } else {
            keepOriginalShadow =
                shouldCheckOriginalShadow && dynamic_shadow_should_keep_original_shadow(node, shadowPos);
        }
        if (gCurGraphNodeHeldObject != NULL) {
            originalShadowType = SHADOW_CIRCLE_4_VERTS;
            sHeldObjectShadowRendered = TRUE;
        }
#else
        u8 keepOriginalShadow = TRUE;
        u8 originalShadowType = node->shadowType;
#endif
        if (keepOriginalShadow) {
            extern u8 gInterpolatingSurfaces;
#ifdef TARGET_N3DS
            u8 shadowSolidity = dynamic_shadow_get_original_solidity(node, shadowPos);
#else
            u8 shadowSolidity = node->shadowSolidity;
#endif
            gInterpolatingSurfaces = TRUE;
            shadowListInterpolated = create_shadow_below_xyz(shadowPosInterpolated[0], shadowPosInterpolated[1],
                                                             shadowPosInterpolated[2], shadowScale,
                                                             shadowSolidity, originalShadowType);
            gInterpolatingSurfaces = FALSE;
            shadowList = create_shadow_below_xyz(shadowPos[0], shadowPos[1], shadowPos[2], shadowScale,
                                                 shadowSolidity, originalShadowType);
            if (shadowListInterpolated != NULL && shadowList != NULL) {
                mtx = alloc_display_list(sizeof(*mtx));
                mtxInterpolated = alloc_display_list(sizeof(*mtxInterpolated));
                gMatStackIndex++;

                // Nudge the translation matrix up by a small amount so the shadow
                // polygon sits slightly above the floor surface and avoids
                // z-fighting flicker.  This must happen AFTER create_shadow_below_xyz
                // (which bakes vertex positions relative to the original shadowPos)
                // and BEFORE the matrix is built, so the extra Y only affects the
                // final world-space placement, not the vertex generation.
                shadowPos[1] += 1.5f;
                shadowPosInterpolated[1] += 1.5f;

                mtxf_translate(mtxf, shadowPos);
                mtxf_mul(gMatStack[gMatStackIndex], mtxf, *gCurGraphNodeCamera->matrixPtr);
                mtxf_to_mtx(mtx, gMatStack[gMatStackIndex]);
                gMatStackFixed[gMatStackIndex] = mtx;

                mtxf_translate(mtxf, shadowPosInterpolated);
                mtxf_mul(gMatStackInterpolated[gMatStackIndex], mtxf, *gCurGraphNodeCamera->matrixPtrInterpolated);
                mtxf_to_mtx(mtxInterpolated, gMatStackInterpolated[gMatStackIndex]);
                gMatStackInterpolatedFixed[gMatStackIndex] = mtxInterpolated;

                if (gShadowAboveWaterOrLava == 1) {
                    geo_append_display_list2((void *) VIRTUAL_TO_PHYSICAL(shadowList),
                                             (void *) VIRTUAL_TO_PHYSICAL(shadowListInterpolated), 4);
                } else if (gMarioOnIceOrCarpet == 1) {
                    geo_append_display_list2((void *) VIRTUAL_TO_PHYSICAL(shadowList),
                                             (void *) VIRTUAL_TO_PHYSICAL(shadowListInterpolated), 5);
                } else {
                    geo_append_display_list2((void *) VIRTUAL_TO_PHYSICAL(shadowList),
                                             (void *) VIRTUAL_TO_PHYSICAL(shadowListInterpolated), 6);
                }
                gMatStackIndex--;
            }
        }
    }
    if (node->node.children != NULL) {
        geo_process_node_and_siblings(node->node.children);
    }
}

/**
 * Check whether an object is in view to determine whether it should be drawn.
 * This is known as frustum culling.
 * It checks whether the object is far away, very close / behind the camera,
 * or horizontally out of view. It does not check whether it is vertically
 * out of view. It assumes a sphere of 300 units around the object's position
 * unless the object has a culling radius node that specifies otherwise.
 *
 * The matrix parameter should be the top of the matrix stack, which is the
 * object's transformation matrix times the camera 'look-at' matrix. The math
 * is counter-intuitive, but it checks column 3 (translation vector) of this
 * matrix to determine where the origin (0,0,0) in object space will be once
 * transformed to camera space (x+ = right, y+ = up, z = 'coming out the screen').
 * In 3D graphics, you typically model the world as being moved in front of a
 * static camera instead of a moving camera through a static world, which in
 * this case simplifies calculations. Note that the perspective matrix is not
 * on the matrix stack, so there are still calculations with the fov to compute
 * the slope of the lines of the frustum.
 *
 *        z-
 *
 *  \     |     /
 *   \    |    /
 *    \   |   /
 *     \  |  /
 *      \ | /
 *       \|/
 *        C       x+
 *
 * Since (0,0,0) is unaffected by rotation, columns 0, 1 and 2 are ignored.
 */
static int obj_is_in_view(struct GraphNodeObject *node, Mat4 matrix) {
    s16 cullingRadius;
    s16 halfFov; // half of the fov in in-game angle units instead of degrees
    struct GraphNode *geo;
    f32 hScreenEdge;

    if (node->node.flags & GRAPH_RENDER_INVISIBLE) {
        return FALSE;
    }

    geo = node->sharedChild;

    // ! @bug The aspect ratio is not accounted for. When the fov value is 45,
    // the horizontal effective fov is actually 60 degrees, so you can see objects
    // visibly pop in or out at the edge of the screen.
    halfFov = (gCurGraphNodeCamFrustum->fov / 2.0f + 1.0f) * 32768.0f / 180.0f + 0.5f;

    hScreenEdge = -matrix[3][2] * sins(halfFov) / coss(halfFov);
    // -matrix[3][2] is the depth, which gets multiplied by tan(halfFov) to get
    // the amount of units between the center of the screen and the horizontal edge
    // given the distance from the object to the camera.

#ifdef WIDESCREEN
    // This multiplication should really be performed on 4:3 as well,
    // but the issue will be more apparent on widescreen.
    hScreenEdge *= GFX_DIMENSIONS_ASPECT_RATIO;
#endif

    if (geo != NULL && geo->type == GRAPH_NODE_TYPE_CULLING_RADIUS) {
        cullingRadius =
            (f32)((struct GraphNodeCullingRadius *) geo)->cullingRadius; //! Why is there a f32 cast?
    } else {
        cullingRadius = 300;
    }

    // Don't render if the object is close to or behind the camera
    if (matrix[3][2] > -100.0f + cullingRadius) {
        return FALSE;
    }

    //! This makes the HOLP not update when the camera is far away, and it
    //  makes PU travel safe when the camera is locked on the main map.
    //  If Mario were rendered with a depth over 65536 it would cause overflow
    //  when converting the transformation matrix to a fixed point matrix.
    if (matrix[3][2] < -20000.0f - cullingRadius) {
        return FALSE;
    }

    // Check whether the object is horizontally in view
    if (matrix[3][0] > hScreenEdge + cullingRadius) {
        return FALSE;
    }
    if (matrix[3][0] < -hScreenEdge - cullingRadius) {
        return FALSE;
    }
    return TRUE;
}

static void interpolate_matrix(Mat4 result, Mat4 a, Mat4 b) {
    s32 i, j;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            result[i][j] = (a[i][j] + b[i][j]) / 2.0f;
        }
    }
}

/**
 * Process an object node.
 */
static void geo_process_object(struct Object *node) {
    Mat4 mtxf;
    s32 hasAnimation = (node->header.gfx.node.flags & GRAPH_RENDER_HAS_ANIMATION) != 0;
    Vec3f scaleInterpolated;
#ifdef TARGET_N3DS
    u8 savedCurrentObjectDynamicShadowGenerated = sCurrentObjectDynamicShadowGenerated;
    u8 savedCurrentObjectPlatformInteractionCaster = sCurrentObjectPlatformInteractionCaster;
    sCurrentObjectDynamicShadowGenerated = FALSE;
    sCurrentObjectPlatformInteractionCaster =
        dynamic_shadow_behavior_is_platform_interaction_caster(node->behavior);
    dynamic_shadow_sync_ending_cutscene_object_pos(node);
#endif

    if (node->header.gfx.unk18 == gCurGraphNodeRoot->areaIndex) {
        if (node->header.gfx.throwMatrix != NULL) {
            mtxf_mul(gMatStack[gMatStackIndex + 1], *node->header.gfx.throwMatrix,
                     gMatStack[gMatStackIndex]);
            if (gGlobalTimer == node->header.gfx.prevThrowMatrixTimestamp + 1 &&
                gGlobalTimer != node->header.gfx.skipInterpolationTimestamp) {
                interpolate_matrix(mtxf, *node->header.gfx.throwMatrix, node->header.gfx.prevThrowMatrix);
                mtxf_mul(gMatStackInterpolated[gMatStackIndex + 1], mtxf,
                     gMatStackInterpolated[gMatStackIndex]);
            } else {
                mtxf_mul(gMatStackInterpolated[gMatStackIndex + 1], (void *) node->header.gfx.throwMatrix,
                         gMatStackInterpolated[gMatStackIndex]);
            }
            mtxf_copy(node->header.gfx.prevThrowMatrix, *node->header.gfx.throwMatrix);
            node->header.gfx.prevThrowMatrixTimestamp = gGlobalTimer;
        } else if (node->header.gfx.node.flags & GRAPH_RENDER_BILLBOARD) {
            Vec3f posInterpolated;
            if (gGlobalTimer == node->header.gfx.prevTimestamp + 1 &&
                gGlobalTimer != node->header.gfx.skipInterpolationTimestamp) {
                interpolate_vectors(posInterpolated, node->header.gfx.prevPos, node->header.gfx.pos);
            } else {
                vec3f_copy(posInterpolated, node->header.gfx.pos);
            }
            vec3f_copy(node->header.gfx.prevPos, node->header.gfx.pos);
            node->header.gfx.prevTimestamp = gGlobalTimer;
            mtxf_billboard(gMatStack[gMatStackIndex + 1], gMatStack[gMatStackIndex],
                           node->header.gfx.pos, gCurGraphNodeCamera->roll);
            mtxf_billboard(gMatStackInterpolated[gMatStackIndex + 1], gMatStackInterpolated[gMatStackIndex],
                           posInterpolated, gCurGraphNodeCamera->roll);
        } else {
            Vec3f posInterpolated;
            Vec3s angleInterpolated;
            if (gGlobalTimer == node->header.gfx.prevTimestamp + 1 &&
                gGlobalTimer != node->header.gfx.skipInterpolationTimestamp) {
                interpolate_vectors(posInterpolated, node->header.gfx.prevPos, node->header.gfx.pos);
                interpolate_angles(angleInterpolated, node->header.gfx.prevAngle, node->header.gfx.angle);
            } else {
                vec3f_copy(posInterpolated, node->header.gfx.pos);
                vec3s_copy(angleInterpolated, node->header.gfx.angle);
            }
            vec3f_copy(node->header.gfx.prevPos, node->header.gfx.pos);
            vec3s_copy(node->header.gfx.prevAngle, node->header.gfx.angle);
            node->header.gfx.prevTimestamp = gGlobalTimer;
            mtxf_rotate_zxy_and_translate(mtxf, node->header.gfx.pos, node->header.gfx.angle);
            mtxf_mul(gMatStack[gMatStackIndex + 1], mtxf, gMatStack[gMatStackIndex]);
            mtxf_rotate_zxy_and_translate(mtxf, posInterpolated, angleInterpolated);
            mtxf_mul(gMatStackInterpolated[gMatStackIndex + 1], mtxf, gMatStackInterpolated[gMatStackIndex]);
        }

        if (gGlobalTimer == node->header.gfx.prevScaleTimestamp + 1 &&
            gGlobalTimer != node->header.gfx.skipInterpolationTimestamp) {
            interpolate_vectors(scaleInterpolated, node->header.gfx.prevScale, node->header.gfx.scale);
        } else {
            vec3f_copy(scaleInterpolated, node->header.gfx.scale);
        }
        vec3f_copy(node->header.gfx.prevScale, node->header.gfx.scale);
        node->header.gfx.prevScaleTimestamp = gGlobalTimer;

        mtxf_scale_vec3f(gMatStack[gMatStackIndex + 1], gMatStack[gMatStackIndex + 1],
                         node->header.gfx.scale);
        mtxf_scale_vec3f(gMatStackInterpolated[gMatStackIndex + 1], gMatStackInterpolated[gMatStackIndex + 1],
                         scaleInterpolated);
        node->header.gfx.throwMatrix = &gMatStack[++gMatStackIndex];
        node->header.gfx.throwMatrixInterpolated = &gMatStackInterpolated[gMatStackIndex];
        node->header.gfx.cameraToObject[0] = gMatStack[gMatStackIndex][3][0];
        node->header.gfx.cameraToObject[1] = gMatStack[gMatStackIndex][3][1];
        node->header.gfx.cameraToObject[2] = gMatStack[gMatStackIndex][3][2];

        // FIXME: correct types
        if (node->header.gfx.unk38.curAnim != NULL) {
            geo_set_animation_globals(&node->header.gfx.unk38, hasAnimation);
        }
        if (obj_is_in_view(&node->header.gfx, gMatStack[gMatStackIndex])) {
            Mtx *mtx = alloc_display_list(sizeof(*mtx));
            Mtx *mtxInterpolated = alloc_display_list(sizeof(*mtxInterpolated));
#ifdef TARGET_N3DS
            u8 objectDynamicShadowGenerated = FALSE;
            u8 objectCanGenerateDynamicShadow =
                (!dynamic_shadow_object_is_held(node)
                 || dynamic_shadow_held_object_can_cast_dynamic_shadow(node))
                && (gMarioState == NULL || gMarioState->action != ACT_END_PEACH_CUTSCENE
                    || node == gMarioObject
                    || (!dynamic_shadow_behavior_is(node->behavior, bhvEndPeach)
                        && !dynamic_shadow_behavior_is(node->behavior, bhvEndToad)));
#endif

            mtxf_to_mtx(mtx, gMatStack[gMatStackIndex]);
            gMatStackFixed[gMatStackIndex] = mtx;
            mtxf_to_mtx(mtxInterpolated, gMatStackInterpolated[gMatStackIndex]);
            gMatStackInterpolatedFixed[gMatStackIndex] = mtxInterpolated;
            if (node->header.gfx.sharedChild != NULL) {
#ifdef TARGET_N3DS
                u8 savedSuppressDynamicShadowReceiver;
                u8 generatedDynamicShadow;
#endif

                gCurGraphNodeObject = (struct GraphNodeObject *) node;
                node->header.gfx.sharedChild->parent = &node->header.gfx.node;
#ifdef TARGET_N3DS
                if (objectCanGenerateDynamicShadow) {
                    objectDynamicShadowGenerated =
                        geo_process_dynamic_model_shadow(node->header.gfx.sharedChild, node->header.gfx.pos);
                }
                sCurrentObjectDynamicShadowGenerated = objectDynamicShadowGenerated;
                generatedDynamicShadow = objectDynamicShadowGenerated;
                savedSuppressDynamicShadowReceiver = sSuppressDynamicShadowReceiver;
                sSuppressDynamicShadowReceiver = generatedDynamicShadow && dynamic_shadow_should_suppress_self_receiver();
#endif
                geo_process_node_and_siblings(node->header.gfx.sharedChild);
#ifdef TARGET_N3DS
                sSuppressDynamicShadowReceiver = savedSuppressDynamicShadowReceiver;
#endif
                node->header.gfx.sharedChild->parent = NULL;
                gCurGraphNodeObject = NULL;
            }
            if (node->header.gfx.node.children != NULL) {
#ifdef TARGET_N3DS
                u8 savedSuppressDynamicShadowReceiver;
                u8 generatedDynamicShadow;

                gCurGraphNodeObject = (struct GraphNodeObject *) node;
                generatedDynamicShadow = objectDynamicShadowGenerated;
                if (!generatedDynamicShadow && objectCanGenerateDynamicShadow) {
                    generatedDynamicShadow =
                        geo_process_dynamic_model_shadow(node->header.gfx.node.children, node->header.gfx.pos);
                    objectDynamicShadowGenerated = generatedDynamicShadow;
                    sCurrentObjectDynamicShadowGenerated = objectDynamicShadowGenerated;
                }
                savedSuppressDynamicShadowReceiver = sSuppressDynamicShadowReceiver;
                sSuppressDynamicShadowReceiver = generatedDynamicShadow && dynamic_shadow_should_suppress_self_receiver();
#endif
                geo_process_node_and_siblings(node->header.gfx.node.children);
#ifdef TARGET_N3DS
                sSuppressDynamicShadowReceiver = savedSuppressDynamicShadowReceiver;
                gCurGraphNodeObject = NULL;
#endif
            }
        } else {
            node->header.gfx.prevThrowMatrixTimestamp = 0;
            node->header.gfx.prevTimestamp = 0;
            node->header.gfx.prevScaleTimestamp = 0;
        }

        gMatStackIndex--;
        gCurAnimType = ANIM_TYPE_NONE;
        node->header.gfx.throwMatrix = NULL;
        node->header.gfx.throwMatrixInterpolated = NULL;
    }
#ifdef TARGET_N3DS
    sCurrentObjectDynamicShadowGenerated = savedCurrentObjectDynamicShadowGenerated;
    sCurrentObjectPlatformInteractionCaster = savedCurrentObjectPlatformInteractionCaster;
#endif
}

/**
 * Process an object parent node. Temporarily assigns itself as the parent of
 * the subtree rooted at 'sharedChild' and processes the subtree, after which the
 * actual children are be processed. (in practice they are null though)
 */
static void geo_process_object_parent(struct GraphNodeObjectParent *node) {
    if (node->sharedChild != NULL) {
        node->sharedChild->parent = (struct GraphNode *) node;
        geo_process_node_and_siblings(node->sharedChild);
        node->sharedChild->parent = NULL;
    }
    if (node->node.children != NULL) {
        geo_process_node_and_siblings(node->node.children);
    }
}

/**
 * Process a held object node.
 */
void geo_process_held_object(struct GraphNodeHeldObject *node) {
    Mat4 mat;
    Vec3f translation;
    Mtx *mtx = alloc_display_list(sizeof(*mtx));
    Mtx *mtxInterpolated = alloc_display_list(sizeof(*mtxInterpolated));
    Vec3f scaleInterpolated;
#ifdef TARGET_N3DS
    u8 savedHeldObjectShadowRendered = sHeldObjectShadowRendered;
    struct GraphNodeObject *savedGraphNodeObject;
    u8 savedObjectDynamicShadowGenerated;
#endif

#ifdef TARGET_N3DS
    if (sAppendingDynamicShadow) {
        return;
    }
#endif

#ifdef F3DEX_GBI_2
    gSPLookAt(gDisplayListHead++, &lookAt);
#endif

    if (node->fnNode.func != NULL) {
        node->fnNode.func(GEO_CONTEXT_RENDER, &node->fnNode.node, gMatStack[gMatStackIndex]);
    }
    if (node->objNode != NULL && node->objNode->header.gfx.sharedChild != NULL) {
        s32 hasAnimation = (node->objNode->header.gfx.node.flags & GRAPH_RENDER_HAS_ANIMATION) != 0;

        translation[0] = node->translation[0] / 4.0f;
        translation[1] = node->translation[1] / 4.0f;
        translation[2] = node->translation[2] / 4.0f;

        if (gGlobalTimer == node->objNode->header.gfx.prevScaleTimestamp + 1) {
            interpolate_vectors(scaleInterpolated, node->objNode->header.gfx.prevScale, node->objNode->header.gfx.scale);
        } else {
            vec3f_copy(scaleInterpolated, node->objNode->header.gfx.scale);
        }
        vec3f_copy(node->objNode->header.gfx.prevScale, node->objNode->header.gfx.scale);
        node->objNode->header.gfx.prevScaleTimestamp = gGlobalTimer;

        mtxf_translate(mat, translation);
        mtxf_copy(gMatStack[gMatStackIndex + 1], *gCurGraphNodeObject->throwMatrix);
        gMatStack[gMatStackIndex + 1][3][0] = gMatStack[gMatStackIndex][3][0];
        gMatStack[gMatStackIndex + 1][3][1] = gMatStack[gMatStackIndex][3][1];
        gMatStack[gMatStackIndex + 1][3][2] = gMatStack[gMatStackIndex][3][2];
        mtxf_mul(gMatStack[gMatStackIndex + 1], mat, gMatStack[gMatStackIndex + 1]);
        mtxf_scale_vec3f(gMatStack[gMatStackIndex + 1], gMatStack[gMatStackIndex + 1],
                         node->objNode->header.gfx.scale);
        mtxf_copy(gMatStackInterpolated[gMatStackIndex + 1], (void *) gCurGraphNodeObject->throwMatrixInterpolated);
        gMatStackInterpolated[gMatStackIndex + 1][3][0] = gMatStackInterpolated[gMatStackIndex][3][0];
        gMatStackInterpolated[gMatStackIndex + 1][3][1] = gMatStackInterpolated[gMatStackIndex][3][1];
        gMatStackInterpolated[gMatStackIndex + 1][3][2] = gMatStackInterpolated[gMatStackIndex][3][2];
        mtxf_mul(gMatStackInterpolated[gMatStackIndex + 1], mat, gMatStackInterpolated[gMatStackIndex + 1]);
        mtxf_scale_vec3f(gMatStackInterpolated[gMatStackIndex + 1], gMatStackInterpolated[gMatStackIndex + 1],
                         scaleInterpolated);
        if (node->fnNode.func != NULL) {
            node->fnNode.func(GEO_CONTEXT_HELD_OBJ, &node->fnNode.node,
                              (struct AllocOnlyPool *) gMatStack[gMatStackIndex + 1]);
        }
        gMatStackIndex++;
        mtxf_to_mtx(mtx, gMatStack[gMatStackIndex]);
        gMatStackFixed[gMatStackIndex] = mtx;
        mtxf_to_mtx(mtxInterpolated, gMatStackInterpolated[gMatStackIndex]);
        gMatStackInterpolatedFixed[gMatStackIndex] = mtxInterpolated;
        gGeoTempState.type = gCurAnimType;
        gGeoTempState.enabled = gCurAnimEnabled;
        gGeoTempState.frame = gCurrAnimFrame;
        gGeoTempState.translationMultiplier = gCurAnimTranslationMultiplier;
        gGeoTempState.attribute = gCurrAnimAttribute;
        gGeoTempState.data = gCurAnimData;
        gGeoTempState.prevFrame = gPrevAnimFrame;
        gCurAnimType = 0;
        gCurGraphNodeHeldObject = (void *) node;
#ifdef TARGET_N3DS
        savedGraphNodeObject = gCurGraphNodeObject;
        savedObjectDynamicShadowGenerated = sCurrentObjectDynamicShadowGenerated;
        gCurGraphNodeObject = (struct GraphNodeObject *) node->objNode;
        node->objNode->header.gfx.cameraToObject[0] = gMatStack[gMatStackIndex][3][0];
        node->objNode->header.gfx.cameraToObject[1] = gMatStack[gMatStackIndex][3][1];
        node->objNode->header.gfx.cameraToObject[2] = gMatStack[gMatStackIndex][3][2];
        sHeldObjectShadowRendered = FALSE;
        sCurrentObjectDynamicShadowGenerated = FALSE;
#endif
        if (node->objNode->header.gfx.unk38.curAnim != NULL) {
            geo_set_animation_globals(&node->objNode->header.gfx.unk38, hasAnimation);
        }

#ifdef TARGET_N3DS
        if (dynamic_shadow_held_object_can_cast_dynamic_shadow(node->objNode)) {
            Mat4 cameraInvMtx;
            Mat4 cameraInvMtxInterpolated;
            Mat4 heldModelMtx;
            Mat4 heldModelMtxInterpolated;
            Vec3f heldShadowPos;
            u8 savedUseShadowNodeAnchor = sUseShadowNodeAnchor;
            sUseShadowNodeAnchor = TRUE;
            dynamic_shadow_make_camera_inv_mtx(cameraInvMtx, *gCurGraphNodeCamera->matrixPtr,
                                               gCurGraphNodeCamera->pos);
            dynamic_shadow_make_camera_inv_mtx(cameraInvMtxInterpolated,
                                               *gCurGraphNodeCamera->matrixPtrInterpolated,
                                               gCurGraphNodeCamera->posInterpolated);
            mtxf_mul(heldModelMtx, gMatStack[gMatStackIndex], cameraInvMtx);
            mtxf_mul(heldModelMtxInterpolated, gMatStackInterpolated[gMatStackIndex],
                     cameraInvMtxInterpolated);
            get_pos_from_transform_mtx(heldShadowPos, gMatStack[gMatStackIndex],
                                       *gCurGraphNodeCamera->matrixPtr);
            sDynamicShadowModelMtxOverride = &heldModelMtx;
            sDynamicShadowModelMtxInterpolatedOverride = &heldModelMtxInterpolated;
            sCurrentObjectDynamicShadowGenerated =
                geo_process_dynamic_model_shadow(node->objNode->header.gfx.sharedChild, heldShadowPos);
            sDynamicShadowModelMtxOverride = NULL;
            sDynamicShadowModelMtxInterpolatedOverride = NULL;
            sUseShadowNodeAnchor = savedUseShadowNodeAnchor;
        }
#endif
        geo_process_node_and_siblings(node->objNode->header.gfx.sharedChild);
#ifdef TARGET_N3DS
        if (!sHeldObjectShadowRendered && dynamic_shadow_held_object_needs_blob_fallback()) {
            Vec3f shadowPos;
            Vec3f shadowPosInterpolated;
            Gfx *shadowList;
            Gfx *shadowListInterpolated;
            Mtx *shadowMtx;
            Mtx *shadowMtxInterpolated;
            Mat4 shadowTransform;
            extern u8 gInterpolatingSurfaces;
            f32 shadowScale = dynamic_shadow_get_held_object_blob_scale();

            get_pos_from_transform_mtx(shadowPos, gMatStack[gMatStackIndex],
                                       *gCurGraphNodeCamera->matrixPtr);
            get_pos_from_transform_mtx(shadowPosInterpolated, gMatStackInterpolated[gMatStackIndex],
                                       *gCurGraphNodeCamera->matrixPtrInterpolated);
            gInterpolatingSurfaces = TRUE;
            shadowListInterpolated = create_shadow_below_xyz(
                shadowPosInterpolated[0], shadowPosInterpolated[1], shadowPosInterpolated[2],
                shadowScale, 0xB4, SHADOW_CIRCLE_4_VERTS);
            gInterpolatingSurfaces = FALSE;
            shadowList = create_shadow_below_xyz(shadowPos[0], shadowPos[1], shadowPos[2],
                                                 shadowScale, 0xB4, SHADOW_CIRCLE_4_VERTS);
            if (shadowListInterpolated != NULL && shadowList != NULL) {
                shadowMtx = alloc_display_list(sizeof(*shadowMtx));
                shadowMtxInterpolated = alloc_display_list(sizeof(*shadowMtxInterpolated));
                gMatStackIndex++;

                mtxf_translate(shadowTransform, shadowPos);
                mtxf_mul(gMatStack[gMatStackIndex], shadowTransform,
                         *gCurGraphNodeCamera->matrixPtr);
                mtxf_to_mtx(shadowMtx, gMatStack[gMatStackIndex]);
                gMatStackFixed[gMatStackIndex] = shadowMtx;

                mtxf_translate(shadowTransform, shadowPosInterpolated);
                mtxf_mul(gMatStackInterpolated[gMatStackIndex], shadowTransform,
                         *gCurGraphNodeCamera->matrixPtrInterpolated);
                mtxf_to_mtx(shadowMtxInterpolated, gMatStackInterpolated[gMatStackIndex]);
                gMatStackInterpolatedFixed[gMatStackIndex] = shadowMtxInterpolated;

                if (gShadowAboveWaterOrLava == 1) {
                    geo_append_display_list2((void *) VIRTUAL_TO_PHYSICAL(shadowList),
                                             (void *) VIRTUAL_TO_PHYSICAL(shadowListInterpolated), 4);
                } else if (gMarioOnIceOrCarpet == 1) {
                    geo_append_display_list2((void *) VIRTUAL_TO_PHYSICAL(shadowList),
                                             (void *) VIRTUAL_TO_PHYSICAL(shadowListInterpolated), 5);
                } else {
                    geo_append_display_list2((void *) VIRTUAL_TO_PHYSICAL(shadowList),
                                             (void *) VIRTUAL_TO_PHYSICAL(shadowListInterpolated), 6);
                }
                gMatStackIndex--;
                sHeldObjectShadowRendered = TRUE;
            }
        }
#endif
        gCurGraphNodeHeldObject = NULL;
#ifdef TARGET_N3DS
        gCurGraphNodeObject = savedGraphNodeObject;
        sCurrentObjectDynamicShadowGenerated = savedObjectDynamicShadowGenerated;
        sHeldObjectShadowRendered = savedHeldObjectShadowRendered;
#endif
        gCurAnimType = gGeoTempState.type;
        gCurAnimEnabled = gGeoTempState.enabled;
        gCurrAnimFrame = gGeoTempState.frame;
        gCurAnimTranslationMultiplier = gGeoTempState.translationMultiplier;
        gCurrAnimAttribute = gGeoTempState.attribute;
        gCurAnimData = gGeoTempState.data;
        gPrevAnimFrame = gGeoTempState.prevFrame;
        gMatStackIndex--;
    }

    if (node->fnNode.node.children != NULL) {
        geo_process_node_and_siblings(node->fnNode.node.children);
    }
}

/**
 * Processes the children of the given GraphNode if it has any
 */
void geo_try_process_children(struct GraphNode *node) {
    if (node->children != NULL) {
        geo_process_node_and_siblings(node->children);
    }
}

/**
 * Process a generic geo node and its siblings.
 * The first argument is the start node, and all its siblings will
 * be iterated over.
 */
void geo_process_node_and_siblings(struct GraphNode *firstNode) {
    s16 iterateChildren = TRUE;
    struct GraphNode *curGraphNode = firstNode;
    struct GraphNode *parent = curGraphNode->parent;

    // In the case of a switch node, exactly one of the children of the node is
    // processed instead of all children like usual
    if (parent != NULL) {
        iterateChildren = (parent->type != GRAPH_NODE_TYPE_SWITCH_CASE);
    }

    do {
        if (curGraphNode->flags & GRAPH_RENDER_ACTIVE) {
            if (curGraphNode->flags & GRAPH_RENDER_CHILDREN_FIRST) {
                geo_try_process_children(curGraphNode);
            } else {
                switch (curGraphNode->type) {
                    case GRAPH_NODE_TYPE_ORTHO_PROJECTION:
                        geo_process_ortho_projection((struct GraphNodeOrthoProjection *) curGraphNode);
                        break;
                    case GRAPH_NODE_TYPE_PERSPECTIVE:
                        geo_process_perspective((struct GraphNodePerspective *) curGraphNode);
                        break;
                    case GRAPH_NODE_TYPE_MASTER_LIST:
                        geo_process_master_list((struct GraphNodeMasterList *) curGraphNode);
                        break;
                    case GRAPH_NODE_TYPE_LEVEL_OF_DETAIL:
                        geo_process_level_of_detail((struct GraphNodeLevelOfDetail *) curGraphNode);
                        break;
                    case GRAPH_NODE_TYPE_SWITCH_CASE:
                        geo_process_switch((struct GraphNodeSwitchCase *) curGraphNode);
                        break;
                    case GRAPH_NODE_TYPE_CAMERA:
                        geo_process_camera((struct GraphNodeCamera *) curGraphNode);
                        break;
                    case GRAPH_NODE_TYPE_TRANSLATION_ROTATION:
                        geo_process_translation_rotation(
                            (struct GraphNodeTranslationRotation *) curGraphNode);
                        break;
                    case GRAPH_NODE_TYPE_TRANSLATION:
                        geo_process_translation((struct GraphNodeTranslation *) curGraphNode);
                        break;
                    case GRAPH_NODE_TYPE_ROTATION:
                        geo_process_rotation((struct GraphNodeRotation *) curGraphNode);
                        break;
                    case GRAPH_NODE_TYPE_OBJECT:
                        geo_process_object((struct Object *) curGraphNode);
                        break;
                    case GRAPH_NODE_TYPE_ANIMATED_PART:
                        geo_process_animated_part((struct GraphNodeAnimatedPart *) curGraphNode);
                        break;
                    case GRAPH_NODE_TYPE_BILLBOARD:
                        geo_process_billboard((struct GraphNodeBillboard *) curGraphNode);
                        break;
                    case GRAPH_NODE_TYPE_DISPLAY_LIST:
                        geo_process_display_list((struct GraphNodeDisplayList *) curGraphNode);
                        break;
                    case GRAPH_NODE_TYPE_SCALE:
                        geo_process_scale((struct GraphNodeScale *) curGraphNode);
                        break;
                    case GRAPH_NODE_TYPE_SHADOW:
                        geo_process_shadow((struct GraphNodeShadow *) curGraphNode);
                        break;
                    case GRAPH_NODE_TYPE_OBJECT_PARENT:
                        geo_process_object_parent((struct GraphNodeObjectParent *) curGraphNode);
                        break;
                    case GRAPH_NODE_TYPE_GENERATED_LIST:
                        geo_process_generated_list((struct GraphNodeGenerated *) curGraphNode);
                        break;
                    case GRAPH_NODE_TYPE_BACKGROUND:
                        geo_process_background((struct GraphNodeBackground *) curGraphNode);
                        break;
                    case GRAPH_NODE_TYPE_HELD_OBJ:
                        geo_process_held_object((struct GraphNodeHeldObject *) curGraphNode);
                        break;
                    default:
                        geo_try_process_children((struct GraphNode *) curGraphNode);
                        break;
                }
            }
        } else {
            if (curGraphNode->type == GRAPH_NODE_TYPE_OBJECT) {
                ((struct GraphNodeObject *) curGraphNode)->throwMatrix = NULL;
            }
        }
    } while (iterateChildren && (curGraphNode = curGraphNode->next) != firstNode);
}

/**
 * Process a root node. This is the entry point for processing the scene graph.
 * The root node itself sets up the viewport, then all its children are processed
 * to set up the projection and draw display lists.
 */
void geo_process_root(struct GraphNodeRoot *node, Vp *b, Vp *c, s32 clearColor) {
    UNUSED s32 unused;

    if (node->node.flags & GRAPH_RENDER_ACTIVE) {
        Mtx *initialMatrix;
        Vp *viewport = alloc_display_list(sizeof(*viewport));
        Vp *viewportInterpolated = viewport;

        gDisplayListHeap = alloc_only_pool_init(main_pool_available() - sizeof(struct AllocOnlyPool),
                                                MEMORY_POOL_LEFT);
        initialMatrix = alloc_display_list(sizeof(*initialMatrix));
        gMatStackIndex = 0;
        gCurAnimType = 0;
        vec3s_set(viewport->vp.vtrans, node->x * 4, node->y * 4, 511);
        vec3s_set(viewport->vp.vscale, node->width * 4, node->height * 4, 511);
        if (b != NULL) {
            clear_frame_buffer(clearColor);
            viewportInterpolated = alloc_display_list(sizeof(*viewportInterpolated));
            interpolate_vectors_s16(viewportInterpolated->vp.vtrans, sPrevViewport.vp.vtrans, b->vp.vtrans);
            interpolate_vectors_s16(viewportInterpolated->vp.vscale, sPrevViewport.vp.vscale, b->vp.vscale);

            sViewportPos = gDisplayListHead;
            make_viewport_clip_rect(viewportInterpolated);
            *viewport = *b;
        }

        else if (c != NULL) {
            clear_frame_buffer(clearColor);
            make_viewport_clip_rect(c);
        }
        sPrevViewport = *viewport;

        mtxf_identity(gMatStack[gMatStackIndex]);
        mtxf_to_mtx(initialMatrix, gMatStack[gMatStackIndex]);
        gMatStackFixed[gMatStackIndex] = initialMatrix;

        mtxf_identity(gMatStackInterpolated[gMatStackIndex]);
        gMatStackInterpolatedFixed[gMatStackIndex] = initialMatrix;

        gSPViewport(gDisplayListHead++, VIRTUAL_TO_PHYSICAL(viewportInterpolated));
        gSPMatrix(gDisplayListHead++, VIRTUAL_TO_PHYSICAL(gMatStackFixed[gMatStackIndex]),
                  G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
        gCurGraphNodeRoot = node;
#ifdef TARGET_N3DS
        gDynamicShadowUnderwaterTint = FALSE;
        gDynamicShadowLightMtxReady = FALSE;
        sNextDynamicShadowGroup = 1;
        sDynamicShadowMaskCacheCount = 0;
        gDynamicShadowDebugLightReady = 0;
        gDynamicShadowDebugShadowNodes = 0;
        gDynamicShadowDebugCandidateObjects = 0;
        gDynamicShadowDebugAppendLists = 0;
        gDynamicShadowDebugCasterLists = 0;
        gDynamicShadowDebugDepthTris = 0;
        gDynamicShadowDebugReceiverTris = 0;
        gDynamicShadowDebugRejectMode = 0;
        gDynamicShadowDebugRejectContext = 0;
        gDynamicShadowDebugRejectChildren = 0;
        gDynamicShadowDebugRejectBobomb = 0;
        gDynamicShadowDebugRejectPlatform = 0;
        gDynamicShadowDebugRejectBillboard = 0;
        gDynamicShadowDebugRejectFar = 0;
        gDynamicShadowDebugRejectLight = 0;
#endif
        if (node->node.children != NULL) {
            geo_process_node_and_siblings(node->node.children);
        }
        gCurGraphNodeRoot = NULL;
#if defined(TARGET_N3DS) && defined(DYNAMIC_SHADOW_DEBUG_OVERLAY)
        print_text_fmt_int(8, 188, "DS C %d", gDynamicShadowDebugCasterLists);
        print_text_fmt_int(8, 172, "DS D %d", gDynamicShadowDebugDepthTris);
        print_text_fmt_int(8, 156, "DS R %d", gDynamicShadowDebugReceiverTris);
        print_text_fmt_int(8, 140, "DS N %d", gDynamicShadowDebugCandidateObjects);
        print_text_fmt_int(8, 124, "DS A %d", gDynamicShadowDebugAppendLists);
#endif
        if (gShowDebugText) {
            print_text_fmt_int(180, 36, "MEM %d",
                               gDisplayListHeap->totalSpace - gDisplayListHeap->usedSpace);
        }
        main_pool_free(gDisplayListHeap);
    }
}
