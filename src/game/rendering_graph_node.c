#include <PR/ultratypes.h>

#include "area.h"
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

#ifdef TARGET_N3DS
#include "src/pc/gfx/gfx_citro3d.h"
#include "src/pc/gfx/color_conversion.h"
#include "enhancements/dynamic_shadows.h"
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
static void *sAppendingDynamicShadowMask;
static Mtx *sAppendingDynamicShadowMaskTransform;
static Mtx *sAppendingDynamicShadowMaskTransformInterpolated;
static u16 sAppendingDynamicShadowGroup;
static u16 sNextDynamicShadowGroup = 1;
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
#include "enhancements/dynamic_shadows.inc.c"

static void dynamic_shadow_update_light_matrices(void);

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
                    gSPMatrix(gDisplayListHead++, VIRTUAL_TO_PHYSICAL(currList->transformInterpolated),
                              G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
                    gSPDisplayList(gDisplayListHead++, currList->displayListInterpolated);
                }
            }
        }
    }
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
        // Per-display-list safety cap (redundant with the per-object cap in
        // can_render_model_object, but guards against objects with many parts)
        if (sAppendingDynamicShadow && gDynamicShadowDebugAppendLists >= DYNAMIC_SHADOW_MAX_APPENDED_LISTS) {
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
#ifdef TARGET_N3DS
        if (sAppendingDynamicShadow) {
            listNode->dynamicShadow = 1;
            listNode->dynamicShadowMaskTransform = sAppendingDynamicShadowMaskTransform;
            listNode->dynamicShadowMaskTransformInterpolated =
                sAppendingDynamicShadowMaskTransformInterpolated;
            listNode->dynamicShadowMask = sAppendingDynamicShadowMask;
            listNode->dynamicShadowGroup = sAppendingDynamicShadowGroup;
            layer = LAYER_OPAQUE;
        } else if (sSuppressDynamicShadowReceiver) {
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

    vec3f_set(scaleVec, node->scale, node->scale, node->scale);
    mtxf_scale_vec3f(gMatStack[gMatStackIndex + 1], gMatStack[gMatStackIndex], scaleVec);
    mtxf_scale_vec3f(gMatStackInterpolated[gMatStackIndex + 1], gMatStackInterpolated[gMatStackIndex], scaleVec);
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
    // Camera-facing accessory quads (eyes, hands, decorations) do not have a
    // meaningful world-space silhouette. Keep them out of projected shadows,
    // while allowing explicitly whitelisted billboard-bodied actors.
    if (sAppendingDynamicShadow && !dynamic_shadow_allows_billboard_caster()) {
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
    f32 range = 5200.0f;

    gDynamicShadowLightMtxReady = FALSE;
    if (gCurGraphNodeCamera == NULL || gMarioState == NULL || gMarioObject == NULL) {
        return;
    }

    vec3f_copy(center, gMarioObject->header.gfx.pos);
    center[1] += 500.0f;
    light = dynamic_shadows_get_light();

    lightPos[0] = center[0] + sins(light->yaw) * 3600.0f;
    lightPos[1] = center[1] + 4200.0f;
    lightPos[2] = center[2] + coss(light->yaw) * 3600.0f;
    vec3f_set(up, 0.0f, 1.0f, 0.0f);

    dynamic_shadow_make_camera_inv_mtx(gDynamicShadowCameraInvMtx,
                                       *gCurGraphNodeCamera->matrixPtrInterpolated,
                                       gCurGraphNodeCamera->pos);
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

    if (gCurGraphNodeObject == NULL) {
        return 300;
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
    struct Object *obj;
    const BehaviorScript *behavior;

    if (gCurGraphNodeObject == NULL) {
        return FALSE;
    }

    obj = (struct Object *) gCurGraphNodeObject;
    behavior = obj->behavior;
    return behavior == segmented_to_virtual(bhvKingBobomb);
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

static u8 dynamic_shadow_allows_billboard_caster(void) {
    const BehaviorScript *behavior = dynamic_shadow_current_behavior();

    if (behavior == NULL) {
        return FALSE;
    }

    // These actors use billboard geometry for their actual readable body.
    // Ordinary actors only lose billboard accessories such as Whomp hands.
    return dynamic_shadow_behavior_is(behavior, bhvButterfly)
        || dynamic_shadow_behavior_is(behavior, bhvTripletButterfly)
        || dynamic_shadow_behavior_is(behavior, bhvScuttlebug)
        || dynamic_shadow_behavior_is(behavior, bhvSkeeter);
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

static u8 dynamic_shadow_is_whomp_like(void) {
    struct Object *obj;
    const BehaviorScript *behavior;

    if (gCurGraphNodeObject == NULL) {
        return FALSE;
    }

    obj = (struct Object *) gCurGraphNodeObject;
    behavior = obj->behavior;
    return behavior == segmented_to_virtual(bhvWhompKingBoss)
        || behavior == segmented_to_virtual(bhvSmallWhomp);
}

static u8 dynamic_shadow_is_excluded_platform(void) {
    struct Object *obj;

    if (gCurGraphNodeObject == NULL) {
        return FALSE;
    }

    obj = (struct Object *) gCurGraphNodeObject;
    return gCurrLevelNum == LEVEL_WF
        && obj->behavior == segmented_to_virtual(bhvRotatingPlatform)
        && obj->header.gfx.sharedChild == gLoadedGraphNodes[MODEL_WF_ROTATING_PLATFORM];
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
        || dynamic_shadow_behavior_is(behavior, bhvWfRotatingWoodenPlatform)
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

static u8 dynamic_shadow_is_fixed_interaction_object(void) {
    const BehaviorScript *behavior = dynamic_shadow_current_behavior();

    if (behavior == NULL) {
        return FALSE;
    }

    // Doors and floor switches are level fixtures. Their projected models are
    // large enough to land on roofs or floors above them, while their baked
    // presentation already reads correctly without a dynamic caster.
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
        || dynamic_shadow_behavior_is(behavior, bhvCapSwitchBase)
        || dynamic_shadow_behavior_is(behavior, bhvCapSwitch)
        || dynamic_shadow_behavior_is(behavior, bhvFloorSwitchAnimatesObject)
        || dynamic_shadow_behavior_is(behavior, bhvFloorSwitchGrills)
        || dynamic_shadow_behavior_is(behavior, bhvFloorSwitchHardcodedModel)
        || dynamic_shadow_behavior_is(behavior, bhvFloorSwitchHiddenObjects)
        || dynamic_shadow_behavior_is(behavior, bhvPurpleSwitchHiddenBoxes)
        || dynamic_shadow_behavior_is(behavior, bhvBlueCoinSwitch)
        || dynamic_shadow_behavior_is(behavior, bhvBookSwitch);
}

static u8 dynamic_shadow_is_platform_shadow_caster_allowed(void) {
    const BehaviorScript *behavior;

    if (gCurGraphNodeObject == NULL) {
        return FALSE;
    }

    behavior = ((struct Object *) gCurGraphNodeObject)->behavior;

    return behavior == segmented_to_virtual(bhvRrElevatorPlatform)
        || behavior == segmented_to_virtual(bhvHmcElevatorPlatform)
        || behavior == segmented_to_virtual(bhvWdwExpressElevator)
        || behavior == segmented_to_virtual(bhvWdwExpressElevatorPlatform)
        || behavior == segmented_to_virtual(bhvMeshElevator)
        || behavior == segmented_to_virtual(bhvPyramidElevator)
        || behavior == segmented_to_virtual(bhvControllablePlatform)
        || behavior == segmented_to_virtual(bhvControllablePlatformSub)
        || behavior == segmented_to_virtual(bhvSeesawPlatform)
        || behavior == segmented_to_virtual(bhvTTCElevator);
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
        || dynamic_shadow_is_receiver_only_platform()
        || dynamic_shadow_is_platform_shadow_caster_allowed();
}

static u8 dynamic_shadow_should_suppress_self_receiver(void) {
    return !dynamic_shadow_is_terrain_receiver_object();
}

static u8 dynamic_shadow_is_model_object_far(void) {
    Vec3f *cameraPos;
    f32 dx, dy, dz;
    f32 maxMarioDist;
    f32 maxCameraDist;

    if (gCurGraphNodeCamera == NULL || gCurGraphNodeObject == NULL || gMarioObject == NULL) {
        return TRUE;
    }

    if (!dynamic_shadow_object_is_clear_on_screen()) {
        return TRUE;
    }

    // Primary: distance from Mario in XZ plane. The model-projection path is
    // most convincing near the player; far objects fall back to their vanilla
    // sprite shadows.
    dx = gCurGraphNodeObject->pos[0] - gMarioObject->header.gfx.pos[0];
    dz = gCurGraphNodeObject->pos[2] - gMarioObject->header.gfx.pos[2];
    maxMarioDist = dynamic_shadow_is_mario_object() ? 5200.0f : 3600.0f;
    if (dx * dx + dz * dz > maxMarioDist * maxMarioDist) {
        return TRUE;
    }

    // Vertical offset from Mario (asymmetric):
    //   above Mario: tight threshold 600 (flying enemies, high platforms)
    //   below Mario: generous threshold 2000 (valleys, pits)
    // The light is ~4700 above Mario projecting downward; objects high above
    // are too close to the light and their shadows would be distorted.
    dy = gCurGraphNodeObject->pos[1] - gMarioObject->header.gfx.pos[1];
    if (dynamic_shadow_is_whomp_like()) {
        if (dy > 2200.0f || dy < -2200.0f) {
            return TRUE;
        }
    } else if (!dynamic_shadow_is_mario_object() && (dy > 900.0f || dy < -1800.0f)) {
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
    maxCameraDist = dynamic_shadow_is_mario_object() ? 9000.0f : 6400.0f;
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
    if (toCamX * dx + toCamZ * dz > 0.0f) {
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

    switch (surface->type) {
        case SURFACE_DEATH_PLANE:
        case SURFACE_BURNING:
        case SURFACE_INTANGIBLE:
        case SURFACE_CAMERA_BOUNDARY:
        case SURFACE_VANISH_CAP_WALLS:
        case SURFACE_SHALLOW_QUICKSAND:
        case SURFACE_DEEP_QUICKSAND:
        case SURFACE_INSTANT_QUICKSAND:
        case SURFACE_DEEP_MOVING_QUICKSAND:
        case SURFACE_SHALLOW_MOVING_QUICKSAND:
        case SURFACE_QUICKSAND:
        case SURFACE_MOVING_QUICKSAND:
        case SURFACE_INSTANT_MOVING_QUICKSAND:
            return FALSE;
        case SURFACE_WATER:
        case SURFACE_FLOWING_WATER:
            // Once the moat is drained, its collision keeps the water surface
            // type even though Mario is walking on the exposed channel.
            return gCurrLevelNum == LEVEL_CASTLE_GROUNDS
                && (save_file_get_flags() & SAVE_FLAG_MOAT_DRAINED);
    }

    return TRUE;
}

static u8 dynamic_shadow_surface_can_receive(struct Surface *floor) {
    if (!dynamic_shadow_surface_type_can_receive(floor)) {
        return FALSE;
    }
    if (floor->normal.y < 0.25f) {
        return FALSE;
    }

    return TRUE;
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

static void dynamic_shadow_surface_to_geometry(struct FloorGeometry *floorGeo, struct Surface *surface) {
    floorGeo->normalX = surface->normal.x;
    floorGeo->normalY = surface->normal.y;
    floorGeo->normalZ = surface->normal.z;
    floorGeo->originOffset = surface->originOffset;
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
                 + surface->normal.z * pos[2] + surface->originOffset - lift) / normalDotDir;
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

    return -(surface->normal.x * x + surface->normal.z * z + surface->originOffset - lift)
        / surface->normal.y;
}

#define DYNAMIC_SHADOW_MAX_RECEIVER_SURFACES 36
#define DYNAMIC_SHADOW_STEP_HEIGHT 140.0f
#define DYNAMIC_SHADOW_EDGE_GAP 72.0f

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
    if (dynamic_shadow_is_mario_object() || dynamic_shadow_is_whomp_like()
        || objectRadius < 360) {
        return TRUE;
    }

    sampleRadius = objectRadius * 0.26f;
    if (sampleRadius < 120.0f) {
        sampleRadius = 120.0f;
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

        checkHeight = find_floor(sampleX, expectedHeight + 900.0f, sampleZ, &checkFloor);
        if (checkHeight < -10000.0f || !dynamic_shadow_surface_can_receive(checkFloor)) {
            return FALSE;
        }

        heightDelta = checkHeight - expectedHeight;
        if (heightDelta < 0.0f) {
            heightDelta = -heightDelta;
        }
        if (heightDelta > 140.0f && checkFloor != surface && checkFloor->object != surface->object) {
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
        if (dynamic_shadow_is_whomp_like() || dynamic_shadow_is_platform_shadow_caster_allowed()) {
            maxProjectDist = 2600.0f;
        }
        if (dynamic_shadow_dist_sq_3f(shadowPos, projectedPoint) > maxProjectDist * maxProjectDist) {
            return FALSE;
        }

        checkHeight = find_floor(projectedPoint[0], shadowPos[1] + 1200.0f, projectedPoint[2], &checkFloor);
        if (checkHeight < -10000.0f || !dynamic_shadow_surface_can_receive(checkFloor)) {
            return FALSE;
        }

        heightDelta = checkHeight - projectedPoint[1];
        if (heightDelta < 0.0f) {
            heightDelta = -heightDelta;
        }

        if (heightDelta >= 120.0f) {
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

    *floor = NULL;
    if (dynamic_shadow_is_whomp_like() || dynamic_shadow_is_platform_shadow_caster_allowed()) {
        maxHeightFromFloor = 2600.0f;
    }

    if (caster == gMarioObject && gMarioState != NULL && gMarioState->floor != NULL) {
        floorHeight = gMarioState->floorHeight;
        if (floorHeight > -10000.0f
            && shadowPos[1] - floorHeight >= -80.0f
            && shadowPos[1] - floorHeight < maxHeightFromFloor) {
            *floor = gMarioState->floor;
            if (dynamic_shadow_surface_can_receive(*floor)) {
                return floorHeight;
            }
        }
    } else if (caster != NULL && caster->oFloor != NULL) {
        floorHeight = caster->oFloorHeight;
        if (floorHeight > -10000.0f
            && shadowPos[1] - floorHeight >= -80.0f
            && shadowPos[1] - floorHeight < maxHeightFromFloor) {
            *floor = caster->oFloor;
            if (dynamic_shadow_surface_can_receive(*floor)) {
                return floorHeight;
            }
        }
    }

    floorHeight = find_floor(shadowPos[0], shadowPos[1] + 1200.0f, shadowPos[2], floor);
    if (floorHeight < -10000.0f || !dynamic_shadow_surface_can_receive(*floor)) {
        *floor = NULL;
        return -11000.0f;
    }
    if (shadowPos[1] - floorHeight < -80.0f
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
    if (dynamic_shadow_is_whomp_like() || dynamic_shadow_is_platform_shadow_caster_allowed()) {
        maxProjectDist = 2600.0f;
    }
    if (dynamic_shadow_dist_sq_3f(shadowPos, floorTarget) > maxProjectDist * maxProjectDist) {
        return FALSE;
    }

    *surface = floor;
    return TRUE;
}

static u8 dynamic_shadow_can_render_model_object(struct GraphNode *children) {
    if (!dynamic_shadows_should_render() || dynamic_shadows_is_rendering_mode()) {
        gDynamicShadowDebugRejectMode++;
        return FALSE;
    }
    if (gCurGraphNodeHeldObject != NULL || gCurGraphNodeCamera == NULL || gCurGraphNodeObject == NULL) {
        gDynamicShadowDebugRejectContext++;
        return FALSE;
    }
    if (children == NULL) {
        gDynamicShadowDebugRejectChildren++;
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
    if (dynamic_shadow_is_fixed_interaction_object()) {
        gDynamicShadowDebugRejectPlatform++;
        return FALSE;
    }
    if (dynamic_shadow_is_wf_terrain_object()) {
        gDynamicShadowDebugRejectPlatform++;
        return FALSE;
    }
    if (dynamic_shadow_is_excluded_platform()) {
        gDynamicShadowDebugRejectPlatform++;
        return FALSE;
    }
    if (dynamic_shadow_is_receiver_only_platform()) {
        gDynamicShadowDebugRejectPlatform++;
        return FALSE;
    }
    if ((gCurGraphNodeObject->node.flags & GRAPH_RENDER_BILLBOARD) != 0
        && !dynamic_shadow_allows_billboard_caster()) {
        gDynamicShadowDebugRejectBillboard++;
        return FALSE;
    }

    // Safety cap: once the budget is exhausted, fall back to original shadows
    // for any remaining spatially-valid objects.
    if (gDynamicShadowDebugAppendLists >= DYNAMIC_SHADOW_MAX_APPENDED_LISTS) {
        gDynamicShadowDebugRejectFar++;
        return FALSE;
    }

    return TRUE;
}

static u8 dynamic_shadow_should_keep_original_shadow(struct GraphNodeShadow *node, Vec3f shadowPos) {
    f32 floorHeight;
    struct Surface *floor;
    struct Surface *projectionSurface;
    const struct DynamicShadowLight *light;

    if (!dynamic_shadow_can_render_model_object(node->node.children)) {
        return TRUE;
    }

    light = dynamic_shadows_get_light();
    if (!dynamic_shadow_find_projection_surface(shadowPos, &projectionSurface, light)) {
        return TRUE;
    }

    floorHeight = dynamic_shadow_find_receiver_floor(shadowPos, &floor);
    if (floorHeight < -10000.0f || floor == NULL) {
        return TRUE;
    }

    if ((struct Object *) gCurGraphNodeObject == gMarioObject || node->shadowType == SHADOW_CIRCLE_PLAYER) {
        return shadowPos[1] - floorHeight > 60.0f;
    }

    return FALSE;
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
    Vec3f projectedPoint;
    struct GeoAnimState savedAnimState;
    struct Surface *surface;
    struct FloorGeometry floorGeo;
    const struct DynamicShadowLight *light;

    gDynamicShadowDebugShadowNodes++;
    if (!dynamic_shadow_can_render_model_object(children)) {
        return FALSE;
    }

    light = dynamic_shadows_get_light();
    if (!dynamic_shadow_find_projection_surface(shadowPos, &surface, light)) {
        gDynamicShadowDebugRejectLight++;
        return FALSE;
    }
    gDynamicShadowDebugCandidateObjects++;

    dynamic_shadow_project_point_to_surface(projectedPoint, shadowPos, surface, light);
    receiverMask = dynamic_shadow_get_receiver_mask(surface, shadowPos, projectedPoint,
                                                    &maskMtx, &maskMtxInterpolated,
                                                    &receiverGroup);
    if (receiverMask == NULL) {
        gDynamicShadowDebugRejectLight++;
        return FALSE;
    }

    dynamic_shadow_surface_to_geometry(&floorGeo, surface);
    dynamic_shadow_make_projection_mtx(projectionMtx, &floorGeo, light);

    mtxf_rotate_zxy_and_translate(objectMtx, gCurGraphNodeObject->pos, gCurGraphNodeObject->angle);
    mtxf_scale_vec3f(objectMtx, objectMtx, gCurGraphNodeObject->scale);
    mtxf_mul(projectedMtx, objectMtx, projectionMtx);
    mtxf_mul(shadowMtx, projectedMtx, *gCurGraphNodeCamera->matrixPtr);
    mtxf_mul(shadowMtxInterpolated, projectedMtx, *gCurGraphNodeCamera->matrixPtrInterpolated);

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
    sAppendingDynamicShadowGroup = receiverGroup;
    sAppendingDynamicShadow = TRUE;
    geo_process_node_and_siblings(children);
    sAppendingDynamicShadow = FALSE;
    sAppendingDynamicShadowMask = NULL;
    sAppendingDynamicShadowMaskTransform = NULL;
    sAppendingDynamicShadowMaskTransformInterpolated = NULL;
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

    return TRUE;
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
        u8 keepOriginalShadow = dynamic_shadow_should_keep_original_shadow(node, shadowPos);
#else
        u8 keepOriginalShadow = TRUE;
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
                                                             shadowSolidity, node->shadowType);
            gInterpolatingSurfaces = FALSE;
            shadowList = create_shadow_below_xyz(shadowPos[0], shadowPos[1], shadowPos[2], shadowScale,
                                                 shadowSolidity, node->shadowType);
            if (shadowListInterpolated != NULL && shadowList != NULL) {
                mtx = alloc_display_list(sizeof(*mtx));
                mtxInterpolated = alloc_display_list(sizeof(*mtxInterpolated));
                gMatStackIndex++;

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
                objectDynamicShadowGenerated =
                    geo_process_dynamic_model_shadow(node->header.gfx.sharedChild, node->header.gfx.pos);
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
                if (!generatedDynamicShadow) {
                    generatedDynamicShadow =
                        geo_process_dynamic_model_shadow(node->header.gfx.node.children, node->header.gfx.pos);
                    objectDynamicShadowGenerated = generatedDynamicShadow;
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
        if (node->objNode->header.gfx.unk38.curAnim != NULL) {
            geo_set_animation_globals(&node->objNode->header.gfx.unk38, hasAnimation);
        }

        geo_process_node_and_siblings(node->objNode->header.gfx.sharedChild);
        gCurGraphNodeHeldObject = NULL;
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
