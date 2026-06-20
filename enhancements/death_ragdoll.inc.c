#include "sm64.h"
#include "audio/external.h"
#include "behavior_data.h"
#include "engine/behavior_script.h"
#include "game/camera.h"
#include "engine/math_util.h"
#include "engine/graph_node.h"
#include "engine/surface_collision.h"
#include "game/game_init.h"
#include "game/level_update.h"
#include "game/interaction.h"
#include "game/mario.h"
#include "game/mario_step.h"
#include "game/memory.h"
#include "object_fields.h"
#include "actors/group0.h"
#include "enhancements/death_ragdoll.h"
#ifdef TARGET_N3DS
#include "enhancements/dynamic_shadows.h"
#endif

#define DEATH_RAGDOLL_DURATION 60
#define DEATH_RAGDOLL_MAX_HORIZONTAL_SPEED 42.0f
#define DEATH_RAGDOLL_MAX_UPWARD_SPEED 62.0f
#define DEATH_RAGDOLL_DEBUG_IMPULSE_MULTIPLIER 10.0f
#define DEATH_RAGDOLL_MAX_FALL_SPEED -75.0f
#define DEATH_RAGDOLL_FADE_HOLD_FRAMES 42
#define DEATH_RAGDOLL_CENTER_Y 78.0f
#define DEATH_RAGDOLL_GRAVITY 4.0f
#define DEATH_RAGDOLL_SOFT_LIMIT_REBOUND -0.18f
#define DEATH_RAGDOLL_BODY_MAX_TILT 0x7C00
#define DEATH_RAGDOLL_LIMB_MAX_ANGULAR_SPEED 1150.0f
#define DEATH_RAGDOLL_NODE_MAX_OFFSET 72.0f
#define DEATH_RAGDOLL_SOLVER_GRAVITY_SCALE 0.22f
#define DEATH_RAGDOLL_AIR_GRAVITY_SCALE 1.0f
#define DEATH_RAGDOLL_CONTACT_GRAVITY_SCALE 0.30f
#define DEATH_RAGDOLL_DEBUG_ZR_DOUBLE_TAP_WINDOW 20
#define DEATH_RAGDOLL_BODY_CONTACT_COUNT 6
#define DEATH_RAGDOLL_CONSTRAINT_ITERATIONS 18
#define DEATH_RAGDOLL_COLLISION_ITERATION_STRIDE 6
#define DEATH_RAGDOLL_BONE_COLLISION_SAMPLES 4
#define DEATH_RAGDOLL_VISUAL_SCALE 0.25f
#define DEATH_RAGDOLL_DEBUG_SKELETON 1
#define DEATH_RAGDOLL_HARD_HUMANOID_DEBUG 1
#define DEATH_RAGDOLL_ENABLE_WALL_RESOLVER 0
#define DEATH_RAGDOLL_COLLISION_MARGIN 180.0f
#define DEATH_RAGDOLL_MAX_NODE_WORLD_OFFSET 420.0f
#define DEATH_RAGDOLL_MAX_BODY_WORLD_OFFSET 260.0f
#define DEATH_RAGDOLL_WALL_BRAKE_RADIUS 116.0f
#define DEATH_RAGDOLL_WALL_BRAKE_SPEED 24.0f
#define DEATH_RAGDOLL_WALL_BRAKE_HOLD_FRAMES 8
#define DEATH_RAGDOLL_SQUISH_VISUAL_XZ_SCALE 1.55f
#define DEATH_RAGDOLL_SQUISH_VISUAL_Y_SCALE 0.32f
#define DEATH_RAGDOLL_SQUISH_ESCAPE_DISTANCE 92.0f
#define DEATH_RAGDOLL_SQUISH_ESCAPE_Y 28.0f
#define DEATH_RAGDOLL_DEBUG_HOLD_FRAMES 24
#define DEATH_RAGDOLL_DEBUG_BLJ_START_FORWARD_VEL -16.0f
#define DEATH_RAGDOLL_DEBUG_BLJ_REPEAT_FRAMES 1
#define DEATH_RAGDOLL_DEBUG_BLJ_COAST_FRAMES 90
#define DEATH_RAGDOLL_DEBUG_FULL_HEALTH 0x880

enum DeathRagdollBodyNode {
    DEATH_RAGDOLL_BODY_NODE_CHEST,
    DEATH_RAGDOLL_BODY_NODE_PELVIS,
    DEATH_RAGDOLL_BODY_NODE_BACK,
    DEATH_RAGDOLL_BODY_NODE_COUNT,
};

static s16 sDeathRagdollSeed;
u8 gDeathRagdollDebugEnabled;
static enum DeathRagdollSource sDeathRagdollSource;
static enum DeathRagdollProfile sDeathRagdollProfile;
static u8 sDeathRagdollWarpStarted;
static u8 sDeathRagdollSkipDeathExitSpawn;
static u8 sDeathRagdollHasBlastPos;
static s16 sDeathRagdollDebugZrTapTimer;
static s16 sDeathRagdollDebugZlTapTimer;
static s16 sDeathRagdollDebugZrHoldTimer;
static s16 sDeathRagdollDebugZlHoldTimer;
static s16 sDeathRagdollDebugBljRepeatTimer;
static s16 sDeathRagdollDebugBljCoastTimer;
static u8 sDeathRagdollDebugBljActive;
static s16 sDeathRagdollWallBrakeTimer;
static s16 sDeathRagdollLooseTimer;
static s32 sDeathRagdollGroundContacts;
static s32 sDeathRagdollCenterContacts;
static f32 sDeathRagdollMaxPenetration;
static f32 sDeathRagdollGroundImpactSpeed;
static Vec3f sDeathRagdollCenter;
static Vec3f sDeathRagdollDebugBljLockPos;
static Vec3f sDeathRagdollBlastPos;
static Vec3f sDeathRagdollAngularVel;
static Vec3s sDeathRagdollEntryGfxAngle;
static Vec3f sDeathRagdollLimbAngle[DEATH_RAGDOLL_LIMB_COUNT];
static Vec3f sDeathRagdollLimbVel[DEATH_RAGDOLL_LIMB_COUNT];
static Vec3f sDeathRagdollRenderAngle[DEATH_RAGDOLL_LIMB_COUNT];
static s16 sDeathRagdollRenderAngleTimer[DEATH_RAGDOLL_LIMB_COUNT];
static Vec3f sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_COUNT];
static Vec3f sDeathRagdollNodeVel[DEATH_RAGDOLL_LIMB_COUNT];
static Vec3f sDeathRagdollNodePrevPos[DEATH_RAGDOLL_LIMB_COUNT];
static Vec3f sDeathRagdollBodyNodePos[DEATH_RAGDOLL_BODY_NODE_COUNT];
static Vec3f sDeathRagdollBodyNodeVel[DEATH_RAGDOLL_BODY_NODE_COUNT];
static Vec3f sDeathRagdollBodyNodePrevPos[DEATH_RAGDOLL_BODY_NODE_COUNT];
static s16 sDeathRagdollNodeHitTimer[DEATH_RAGDOLL_LIMB_COUNT];
static u8 sDeathRagdollNodeContact[DEATH_RAGDOLL_LIMB_COUNT];
static u8 sDeathRagdollBodyNodeContact[DEATH_RAGDOLL_BODY_NODE_COUNT];

#ifdef TARGET_N3DS
extern u8 gDeathRagdollDebugZrPressed;
extern u8 gDeathRagdollDebugZlPressed;
extern u8 gDeathRagdollDebugZrHeld;
extern u8 gDeathRagdollDebugZlHeld;
#endif

struct DeathRagdollVisualPart {
    const Gfx *displayList;
};

static const struct DeathRagdollVisualPart sDeathRagdollVisualParts[DEATH_RAGDOLL_LIMB_COUNT] = {
    [DEATH_RAGDOLL_LIMB_TORSO]           = { mario_torso },
    [DEATH_RAGDOLL_LIMB_HEAD]            = { mario_cap_on_eyes_dead },
    [DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM]  = { mario_left_arm },
    [DEATH_RAGDOLL_LIMB_LEFT_FOREARM]    = { mario_left_forearm_shared_dl },
    [DEATH_RAGDOLL_LIMB_LEFT_HAND]       = { mario_left_hand_closed },
    [DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM] = { mario_right_arm },
    [DEATH_RAGDOLL_LIMB_RIGHT_FOREARM]   = { mario_right_forearm_shared_dl },
    [DEATH_RAGDOLL_LIMB_RIGHT_HAND]      = { mario_right_hand_closed },
    [DEATH_RAGDOLL_LIMB_LEFT_THIGH]      = { mario_left_thigh },
    [DEATH_RAGDOLL_LIMB_LEFT_LEG]        = { mario_left_leg_shared_dl },
    [DEATH_RAGDOLL_LIMB_LEFT_FOOT]       = { mario_left_foot },
    [DEATH_RAGDOLL_LIMB_RIGHT_THIGH]     = { mario_right_thigh },
    [DEATH_RAGDOLL_LIMB_RIGHT_LEG]       = { mario_right_leg_shared_dl },
    [DEATH_RAGDOLL_LIMB_RIGHT_FOOT]      = { mario_right_foot },
};

static f32 death_ragdoll_clampf(f32 value, f32 min, f32 max) {
    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
    }
    return value;
}

static f32 death_ragdoll_absf(f32 value) {
    return value < 0.0f ? -value : value;
}

static u8 death_ragdoll_f32_is_safe(f32 value) {
    return value == value && value > -1000000.0f && value < 1000000.0f;
}

static u8 death_ragdoll_vec3f_is_safe(Vec3f pos) {
    return death_ragdoll_f32_is_safe(pos[0])
        && death_ragdoll_f32_is_safe(pos[1])
        && death_ragdoll_f32_is_safe(pos[2]);
}

static u8 death_ragdoll_collision_query_is_safe(Vec3f pos, f32 radius) {
    f32 limit = LEVEL_BOUNDARY_MAX - DEATH_RAGDOLL_COLLISION_MARGIN - radius;

    if (!death_ragdoll_vec3f_is_safe(pos)) {
        return FALSE;
    }
    if (pos[0] <= -limit || pos[0] >= limit || pos[2] <= -limit || pos[2] >= limit) {
        return FALSE;
    }
    return pos[1] > -20000.0f && pos[1] < 30000.0f;
}

static u8 death_ragdoll_center_out_of_bounds(void) {
    Vec3f query;

    if (!death_ragdoll_vec3f_is_safe(sDeathRagdollCenter)) {
        return TRUE;
    }
    vec3f_copy(query, sDeathRagdollCenter);
    return !death_ragdoll_collision_query_is_safe(query, 96.0f);
}

static u8 death_ragdoll_wall_brake_probe(Vec3f pos, f32 radius, Vec3f resolvedPos) {
    struct WallCollisionData collision;

    if (!death_ragdoll_collision_query_is_safe(pos, radius)) {
        return FALSE;
    }

    collision.x = pos[0];
    collision.y = pos[1];
    collision.z = pos[2];
    collision.offsetY = 0.0f;
    collision.radius = radius;
    collision.numWalls = 0;

    if (find_wall_collisions(&collision) == 0) {
        return FALSE;
    }

    resolvedPos[0] = collision.x;
    resolvedPos[1] = pos[1];
    resolvedPos[2] = collision.z;
    return death_ragdoll_vec3f_is_safe(resolvedPos);
}

static u8 death_ragdoll_apply_wall_brake(struct MarioState *m) {
    Vec3f nextCenter;
    Vec3f resolvedPos;
    f32 horizontalSpeed = sqrtf(m->vel[0] * m->vel[0] + m->vel[2] * m->vel[2]);
    f32 verticalSpeed = death_ragdoll_absf(m->vel[1]);
    u8 brakeLatched = sDeathRagdollWallBrakeTimer > 0;
    s32 limb;
    s32 bodyNode;

    if (sDeathRagdollWallBrakeTimer > 0) {
        sDeathRagdollWallBrakeTimer--;
    }

    if (horizontalSpeed < DEATH_RAGDOLL_WALL_BRAKE_SPEED
        && verticalSpeed < DEATH_RAGDOLL_WALL_BRAKE_SPEED
        && !brakeLatched) {
        return FALSE;
    }

    nextCenter[0] = sDeathRagdollCenter[0] + m->vel[0] * 0.75f;
    nextCenter[1] = sDeathRagdollCenter[1] + m->vel[1] * 0.35f;
    nextCenter[2] = sDeathRagdollCenter[2] + m->vel[2] * 0.75f;

    if (!death_ragdoll_wall_brake_probe(sDeathRagdollCenter, DEATH_RAGDOLL_WALL_BRAKE_RADIUS, resolvedPos)
        && !death_ragdoll_wall_brake_probe(nextCenter, DEATH_RAGDOLL_WALL_BRAKE_RADIUS, resolvedPos)) {
        if (!brakeLatched || horizontalSpeed <= 0.01f) {
            return FALSE;
        }
        vec3f_copy(resolvedPos, sDeathRagdollCenter);
    }

    sDeathRagdollWallBrakeTimer = DEATH_RAGDOLL_WALL_BRAKE_HOLD_FRAMES;
    sDeathRagdollCenter[0] = resolvedPos[0];
    sDeathRagdollCenter[2] = resolvedPos[2];
    m->vel[0] = 0.0f;
    m->vel[2] = 0.0f;
    m->forwardVel = 0.0f;

    if (m->vel[1] > 0.0f) {
        m->vel[1] *= 0.18f;
    }

    sDeathRagdollAngularVel[0] *= 0.42f;
    sDeathRagdollAngularVel[1] *= 0.35f;
    sDeathRagdollAngularVel[2] *= 0.42f;

    for (limb = 0; limb < DEATH_RAGDOLL_LIMB_COUNT; limb++) {
        sDeathRagdollNodeVel[limb][0] = 0.0f;
        sDeathRagdollNodeVel[limb][2] = 0.0f;
    }

    for (bodyNode = 0; bodyNode < DEATH_RAGDOLL_BODY_NODE_COUNT; bodyNode++) {
        sDeathRagdollBodyNodeVel[bodyNode][0] = 0.0f;
        sDeathRagdollBodyNodeVel[bodyNode][2] = 0.0f;
    }

    return TRUE;
}

static void death_ragdoll_clamp_vec3f_speed(Vec3f vel, f32 maxSpeed) {
    f32 speed;
    f32 scale;

    if (!death_ragdoll_vec3f_is_safe(vel)) {
        vec3f_set(vel, 0.0f, 0.0f, 0.0f);
        return;
    }

    speed = sqrtf(vel[0] * vel[0] + vel[1] * vel[1] + vel[2] * vel[2]);
    if (speed <= maxSpeed || speed <= 0.1f) {
        return;
    }

    scale = maxSpeed / speed;
    vel[0] *= scale;
    vel[1] *= scale;
    vel[2] *= scale;
}

static void death_ragdoll_clamp_point_around_center(Vec3f pos, Vec3f vel, f32 maxOffset) {
    Vec3f offset;
    f32 dist;
    f32 scale;

    if (!death_ragdoll_vec3f_is_safe(pos)) {
        vec3f_copy(pos, sDeathRagdollCenter);
        vec3f_set(vel, 0.0f, 0.0f, 0.0f);
        return;
    }

    vec3f_dif(offset, pos, sDeathRagdollCenter);
    dist = sqrtf(offset[0] * offset[0] + offset[1] * offset[1] + offset[2] * offset[2]);
    if (dist <= maxOffset || dist <= 1.0f) {
        return;
    }

    scale = maxOffset / dist;
    pos[0] = sDeathRagdollCenter[0] + offset[0] * scale;
    pos[1] = sDeathRagdollCenter[1] + offset[1] * scale;
    pos[2] = sDeathRagdollCenter[2] + offset[2] * scale;
    vel[0] *= 0.58f;
    vel[1] *= 0.58f;
    vel[2] *= 0.58f;
}

static void death_ragdoll_clamp_angular_velocity(void) {
    sDeathRagdollAngularVel[0] = death_ragdoll_clampf(sDeathRagdollAngularVel[0], -7200.0f, 7200.0f);
    sDeathRagdollAngularVel[1] = death_ragdoll_clampf(sDeathRagdollAngularVel[1], -7200.0f, 7200.0f);
    sDeathRagdollAngularVel[2] = death_ragdoll_clampf(sDeathRagdollAngularVel[2], -7200.0f, 7200.0f);
}

u8 death_ragdoll_debug_is_enabled(void) {
    return gDeathRagdollDebugEnabled;
}

void death_ragdoll_debug_set_enabled(u8 enabled) {
    gDeathRagdollDebugEnabled = enabled != FALSE;
    if (!gDeathRagdollDebugEnabled) {
        sDeathRagdollDebugZrTapTimer = 0;
        sDeathRagdollDebugZlTapTimer = 0;
        sDeathRagdollDebugZrHoldTimer = 0;
        sDeathRagdollDebugZlHoldTimer = 0;
        sDeathRagdollDebugBljRepeatTimer = 0;
        sDeathRagdollDebugBljActive = FALSE;
    }
}

u8 death_ragdoll_debug_blj_is_active(void) {
    return sDeathRagdollDebugBljActive;
}

u8 death_ragdoll_debug_blj_is_coasting(void) {
    return sDeathRagdollDebugBljCoastTimer > 0;
}

struct DeathRagdollProfileImpulse {
    f32 horizontal;
    f32 upward;
    f32 rotation;
};

static struct DeathRagdollProfileImpulse death_ragdoll_profile_impulse(void) {
    struct DeathRagdollProfileImpulse impulse;

    if (death_ragdoll_debug_is_enabled()) {
        impulse.horizontal = DEATH_RAGDOLL_DEBUG_IMPULSE_MULTIPLIER;
        impulse.upward = DEATH_RAGDOLL_DEBUG_IMPULSE_MULTIPLIER;
        impulse.rotation = DEATH_RAGDOLL_DEBUG_IMPULSE_MULTIPLIER;
        return impulse;
    }

    switch (sDeathRagdollProfile) {
        case DEATH_RAGDOLL_PROFILE_LIGHT_ENEMY:
        case DEATH_RAGDOLL_PROFILE_DIRECT_DAMAGE:
            impulse.horizontal = 5.0f;
            impulse.upward = 3.0f;
            impulse.rotation = 1.0f;
            break;

        case DEATH_RAGDOLL_PROFILE_CHAIN_CHOMP:
            impulse.horizontal = 6.0f;
            impulse.upward = 6.0f;
            impulse.rotation = 6.0f;
            break;

        case DEATH_RAGDOLL_PROFILE_BOWSER:
            impulse.horizontal = 9.0f;
            impulse.upward = 9.0f;
            impulse.rotation = 9.0f;
            break;

        case DEATH_RAGDOLL_PROFILE_SHOCK:
            impulse.horizontal = 5.0f;
            impulse.upward = 2.0f;
            impulse.rotation = 1.0f;
            break;

        case DEATH_RAGDOLL_PROFILE_PROJECTILE:
            impulse.horizontal = 4.0f;
            impulse.upward = 1.0f;
            impulse.rotation = 1.0f;
            break;

        case DEATH_RAGDOLL_PROFILE_EXPLOSION:
            impulse.horizontal = 10.0f;
            impulse.upward = 10.0f;
            impulse.rotation = 3.0f;
            break;

        case DEATH_RAGDOLL_PROFILE_FALL_DAMAGE:
            impulse.horizontal = 2.0f;
            impulse.upward = 7.0f;
            impulse.rotation = 2.0f;
            break;

        case DEATH_RAGDOLL_PROFILE_SQUISH:
            impulse.horizontal = 10.0f;
            impulse.upward = 10.0f;
            impulse.rotation = 6.0f;
            break;

        case DEATH_RAGDOLL_PROFILE_DEFAULT:
        case DEATH_RAGDOLL_PROFILE_DISABLED:
        default:
            impulse.horizontal = 1.0f;
            impulse.upward = 1.0f;
            impulse.rotation = 1.0f;
            break;
    }

    return impulse;
}

static f32 death_ragdoll_horizontal_impulse_multiplier(void) {
    return death_ragdoll_profile_impulse().horizontal;
}

static f32 death_ragdoll_upward_impulse_multiplier(void) {
    return death_ragdoll_profile_impulse().upward;
}

static f32 death_ragdoll_rotation_impulse_multiplier(void) {
    return death_ragdoll_profile_impulse().rotation;
}

static f32 death_ragdoll_rotation_debug_scale(void) {
    return death_ragdoll_rotation_impulse_multiplier() / 10.0f;
}

static void death_ragdoll_visual_axis_scale(Vec3f scale) {
    vec3f_set(scale, DEATH_RAGDOLL_VISUAL_SCALE, DEATH_RAGDOLL_VISUAL_SCALE,
              DEATH_RAGDOLL_VISUAL_SCALE);
}

static void death_ragdoll_reset_mario_object_scale(struct MarioState *m) {
    Vec3f scale;

    if (m->marioObj == NULL) {
        return;
    }

    m->squishTimer = 0;
    if (sDeathRagdollProfile == DEATH_RAGDOLL_PROFILE_SQUISH) {
        vec3f_set(scale, DEATH_RAGDOLL_SQUISH_VISUAL_XZ_SCALE,
                  DEATH_RAGDOLL_SQUISH_VISUAL_Y_SCALE,
                  DEATH_RAGDOLL_SQUISH_VISUAL_XZ_SCALE);
    } else {
        vec3f_set(scale, 1.0f, 1.0f, 1.0f);
    }

    vec3f_copy(m->marioObj->header.gfx.scale, scale);
    vec3f_copy(m->marioObj->header.gfx.prevScale, scale);
    m->marioObj->header.gfx.prevScaleTimestamp = gGlobalTimer;
    m->marioObj->header.gfx.skipInterpolationTimestamp = gGlobalTimer;
}

static void death_ragdoll_apply_squish_escape_offset(struct MarioState *m) {
    f32 dx;
    f32 dz;
    f32 dist;
    s16 yaw;

    if (sDeathRagdollProfile != DEATH_RAGDOLL_PROFILE_SQUISH) {
        return;
    }

    if (sDeathRagdollHasBlastPos) {
        dx = m->pos[0] - sDeathRagdollBlastPos[0];
        dz = m->pos[2] - sDeathRagdollBlastPos[2];
        dist = sqrtf(dx * dx + dz * dz);
        if (dist > 8.0f) {
            sDeathRagdollCenter[0] += dx / dist * DEATH_RAGDOLL_SQUISH_ESCAPE_DISTANCE;
            sDeathRagdollCenter[2] += dz / dist * DEATH_RAGDOLL_SQUISH_ESCAPE_DISTANCE;
        } else {
            yaw = m->faceAngle[1] + 0x8000;
            sDeathRagdollCenter[0] += sins(yaw) * DEATH_RAGDOLL_SQUISH_ESCAPE_DISTANCE;
            sDeathRagdollCenter[2] += coss(yaw) * DEATH_RAGDOLL_SQUISH_ESCAPE_DISTANCE;
        }
    } else {
        yaw = m->faceAngle[1] + 0x8000;
        sDeathRagdollCenter[0] += sins(yaw) * (DEATH_RAGDOLL_SQUISH_ESCAPE_DISTANCE * 0.65f);
        sDeathRagdollCenter[2] += coss(yaw) * (DEATH_RAGDOLL_SQUISH_ESCAPE_DISTANCE * 0.65f);
    }

    sDeathRagdollCenter[1] += DEATH_RAGDOLL_SQUISH_ESCAPE_Y;
}

static f32 death_ragdoll_solver_gravity(void) {
#if DEATH_RAGDOLL_HARD_HUMANOID_DEBUG
    return DEATH_RAGDOLL_GRAVITY * DEATH_RAGDOLL_SOLVER_GRAVITY_SCALE;
#else
    return DEATH_RAGDOLL_GRAVITY;
#endif
}

static f32 death_ragdoll_fall_gravity(UNUSED struct MarioState *m) {
#if DEATH_RAGDOLL_HARD_HUMANOID_DEBUG
    if (sDeathRagdollGroundContacts > 0) {
        return DEATH_RAGDOLL_GRAVITY * DEATH_RAGDOLL_CONTACT_GRAVITY_SCALE;
    }
    return DEATH_RAGDOLL_GRAVITY * DEATH_RAGDOLL_AIR_GRAVITY_SCALE;
#else
    return DEATH_RAGDOLL_GRAVITY;
#endif
}

static f32 death_ragdoll_angle_delta(s16 target, f32 current) {
    return (s16) (target - (s16) current);
}

static f32 death_ragdoll_pose_blend_weight(UNUSED s32 timer) {
    return 1.0f;
}

static s16 death_ragdoll_blend_angle(s16 from, s16 to, f32 weight) {
    return from + (s16) (death_ragdoll_angle_delta(to, from) * weight);
}

static s16 death_ragdoll_approach_angle(f32 current, s16 target, f32 maxDelta, f32 blend) {
    f32 delta = death_ragdoll_angle_delta(target, current);

    delta = death_ragdoll_clampf(delta * blend, -maxDelta, maxDelta);
    return (s16) (current + delta);
}

static s16 death_ragdoll_limb_decay(s16 value, UNUSED s32 timer) {
    return value;
}

static void death_ragdoll_freeze_current_animation(struct MarioState *m) {
    struct GraphNodeObject_sub *animInfo = &m->marioObj->header.gfx.unk38;

    if (animInfo->curAnim == NULL) {
        return;
    }

    animInfo->animFrameAccelAssist = animInfo->animFrame << 16;
    animInfo->animAccel = 0;
    animInfo->animTimer = gAreaUpdateCounter;
    animInfo->prevAnimFrame = animInfo->animFrame;
    animInfo->prevAnimID = animInfo->animID;
    animInfo->prevAnimFrameTimestamp = gGlobalTimer;
    animInfo->prevAnimPtr = animInfo->curAnim;
}

static s16 death_ragdoll_seeded_wave(s32 timer, s32 limb, s32 axis, s32 speed) {
    return sins(timer * speed + limb * 0x19A1 + axis * 0x2E31 + sDeathRagdollSeed);
}

static void death_ragdoll_add_limb_impulse(s32 limb, s16 pitch, s16 yaw, s16 roll) {
    sDeathRagdollLimbVel[limb][0] += pitch * 0.34f;
    sDeathRagdollLimbVel[limb][1] += yaw * 0.34f;
    sDeathRagdollLimbVel[limb][2] += roll * 0.34f;
    sDeathRagdollNodeHitTimer[limb] = 10;
}

static void death_ragdoll_add_limb_velocity(s32 limb, f32 pitch, f32 yaw, f32 roll) {
    sDeathRagdollLimbVel[limb][0] += pitch;
    sDeathRagdollLimbVel[limb][1] += yaw;
    sDeathRagdollLimbVel[limb][2] += roll;
    sDeathRagdollLimbVel[limb][0] = death_ragdoll_clampf(sDeathRagdollLimbVel[limb][0],
                                                          -DEATH_RAGDOLL_LIMB_MAX_ANGULAR_SPEED,
                                                          DEATH_RAGDOLL_LIMB_MAX_ANGULAR_SPEED);
    sDeathRagdollLimbVel[limb][1] = death_ragdoll_clampf(sDeathRagdollLimbVel[limb][1],
                                                          -DEATH_RAGDOLL_LIMB_MAX_ANGULAR_SPEED,
                                                          DEATH_RAGDOLL_LIMB_MAX_ANGULAR_SPEED);
    sDeathRagdollLimbVel[limb][2] = death_ragdoll_clampf(sDeathRagdollLimbVel[limb][2],
                                                          -DEATH_RAGDOLL_LIMB_MAX_ANGULAR_SPEED,
                                                          DEATH_RAGDOLL_LIMB_MAX_ANGULAR_SPEED);
    sDeathRagdollNodeHitTimer[limb] = 10;
}

static void death_ragdoll_add_chained_contact_impulse(s32 limb, f32 pitch, f32 yaw, f32 roll) {
    death_ragdoll_add_limb_velocity(limb, pitch, yaw, roll);

    switch (limb) {
        case DEATH_RAGDOLL_LIMB_HEAD:
            death_ragdoll_add_limb_velocity(DEATH_RAGDOLL_LIMB_TORSO, pitch * 0.35f, yaw * 0.20f, roll * 0.35f);
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_HAND:
            death_ragdoll_add_limb_velocity(DEATH_RAGDOLL_LIMB_LEFT_FOREARM, pitch * 0.55f, yaw * 0.45f, roll * 0.55f);
            death_ragdoll_add_limb_velocity(DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM, pitch * 0.25f, yaw * 0.20f, roll * 0.25f);
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_HAND:
            death_ragdoll_add_limb_velocity(DEATH_RAGDOLL_LIMB_RIGHT_FOREARM, pitch * 0.55f, yaw * 0.45f, roll * 0.55f);
            death_ragdoll_add_limb_velocity(DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM, pitch * 0.25f, yaw * 0.20f, roll * 0.25f);
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_FOREARM:
            death_ragdoll_add_limb_velocity(DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM, pitch * 0.45f, yaw * 0.35f, roll * 0.45f);
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_FOREARM:
            death_ragdoll_add_limb_velocity(DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM, pitch * 0.45f, yaw * 0.35f, roll * 0.45f);
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_FOOT:
            death_ragdoll_add_limb_velocity(DEATH_RAGDOLL_LIMB_LEFT_LEG, pitch * 0.60f, yaw * 0.40f, roll * 0.60f);
            death_ragdoll_add_limb_velocity(DEATH_RAGDOLL_LIMB_LEFT_THIGH, pitch * 0.30f, yaw * 0.20f, roll * 0.30f);
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_FOOT:
            death_ragdoll_add_limb_velocity(DEATH_RAGDOLL_LIMB_RIGHT_LEG, pitch * 0.60f, yaw * 0.40f, roll * 0.60f);
            death_ragdoll_add_limb_velocity(DEATH_RAGDOLL_LIMB_RIGHT_THIGH, pitch * 0.30f, yaw * 0.20f, roll * 0.30f);
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_LEG:
            death_ragdoll_add_limb_velocity(DEATH_RAGDOLL_LIMB_LEFT_THIGH, pitch * 0.50f, yaw * 0.30f, roll * 0.50f);
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_LEG:
            death_ragdoll_add_limb_velocity(DEATH_RAGDOLL_LIMB_RIGHT_THIGH, pitch * 0.50f, yaw * 0.30f, roll * 0.50f);
            break;
    }
}

static f32 death_ragdoll_joint_spring(s32 limb) {
    switch (limb) {
        case DEATH_RAGDOLL_LIMB_TORSO:
            return 0.0020f;
        case DEATH_RAGDOLL_LIMB_HEAD:
        case DEATH_RAGDOLL_LIMB_LEFT_HAND:
        case DEATH_RAGDOLL_LIMB_RIGHT_HAND:
        case DEATH_RAGDOLL_LIMB_LEFT_FOOT:
        case DEATH_RAGDOLL_LIMB_RIGHT_FOOT:
            return 0.0014f;
        default:
            return 0.0026f;
    }
}

static f32 death_ragdoll_contact_scale(s32 limb) {
    switch (limb) {
        case DEATH_RAGDOLL_LIMB_TORSO:
            return 0.45f;
        case DEATH_RAGDOLL_LIMB_HEAD:
            return 0.30f;
        case DEATH_RAGDOLL_LIMB_LEFT_HAND:
        case DEATH_RAGDOLL_LIMB_RIGHT_HAND:
        case DEATH_RAGDOLL_LIMB_LEFT_FOOT:
        case DEATH_RAGDOLL_LIMB_RIGHT_FOOT:
            return 0.72f;
        default:
            return 0.62f;
    }
}

static f32 death_ragdoll_settle_gravity_scale(s32 limb) {
    switch (limb) {
        case DEATH_RAGDOLL_LIMB_TORSO:
            return 0.20f;
        case DEATH_RAGDOLL_LIMB_HEAD:
            return 0.72f;
        case DEATH_RAGDOLL_LIMB_LEFT_HAND:
        case DEATH_RAGDOLL_LIMB_RIGHT_HAND:
        case DEATH_RAGDOLL_LIMB_LEFT_FOOT:
        case DEATH_RAGDOLL_LIMB_RIGHT_FOOT:
            return 1.35f;
        case DEATH_RAGDOLL_LIMB_LEFT_FOREARM:
        case DEATH_RAGDOLL_LIMB_RIGHT_FOREARM:
        case DEATH_RAGDOLL_LIMB_LEFT_LEG:
        case DEATH_RAGDOLL_LIMB_RIGHT_LEG:
            return 1.12f;
        default:
            return 0.88f;
    }
}

static f32 death_ragdoll_node_mass(s32 limb) {
    switch (limb) {
        case DEATH_RAGDOLL_LIMB_TORSO:
            return 2.6f;
        case DEATH_RAGDOLL_LIMB_HEAD:
            return 1.25f;
        case DEATH_RAGDOLL_LIMB_LEFT_THIGH:
        case DEATH_RAGDOLL_LIMB_RIGHT_THIGH:
        case DEATH_RAGDOLL_LIMB_LEFT_LEG:
        case DEATH_RAGDOLL_LIMB_RIGHT_LEG:
        case DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM:
        case DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM:
        case DEATH_RAGDOLL_LIMB_LEFT_FOREARM:
        case DEATH_RAGDOLL_LIMB_RIGHT_FOREARM:
        case DEATH_RAGDOLL_LIMB_LEFT_HAND:
        case DEATH_RAGDOLL_LIMB_RIGHT_HAND:
        case DEATH_RAGDOLL_LIMB_LEFT_FOOT:
        case DEATH_RAGDOLL_LIMB_RIGHT_FOOT:
            return 5.4f;
    }

    return 1.0f;
}

static f32 death_ragdoll_body_node_mass(s32 node) {
    switch (node) {
        case DEATH_RAGDOLL_BODY_NODE_CHEST:
            return 1.55f;
        case DEATH_RAGDOLL_BODY_NODE_PELVIS:
            return 1.70f;
        case DEATH_RAGDOLL_BODY_NODE_BACK:
            return 1.15f;
    }

    return 1.0f;
}

static f32 death_ragdoll_body_node_radius(s32 node) {
    switch (node) {
        case DEATH_RAGDOLL_BODY_NODE_CHEST:
            return 18.0f;
        case DEATH_RAGDOLL_BODY_NODE_PELVIS:
            return 18.0f;
        case DEATH_RAGDOLL_BODY_NODE_BACK:
            return 16.0f;
    }

    return 16.0f;
}

static f32 death_ragdoll_node_radius(s32 limb) {
    f32 radius;

    switch (limb) {
        case DEATH_RAGDOLL_LIMB_TORSO:
            radius = 34.0f;
            break;
        case DEATH_RAGDOLL_LIMB_HEAD:
            radius = 24.0f;
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_THIGH:
        case DEATH_RAGDOLL_LIMB_RIGHT_THIGH:
            radius = 19.0f;
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_LEG:
        case DEATH_RAGDOLL_LIMB_RIGHT_LEG:
            radius = 17.0f;
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_FOOT:
        case DEATH_RAGDOLL_LIMB_RIGHT_FOOT:
            radius = 16.0f;
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_FOREARM:
        case DEATH_RAGDOLL_LIMB_RIGHT_FOREARM:
            radius = 15.0f;
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_HAND:
        case DEATH_RAGDOLL_LIMB_RIGHT_HAND:
            radius = 14.0f;
            break;
        default:
            radius = 17.0f;
            break;
    }

    switch (limb) {
        case DEATH_RAGDOLL_LIMB_TORSO:
            return max(radius * DEATH_RAGDOLL_VISUAL_SCALE, 15.0f);
        case DEATH_RAGDOLL_LIMB_HEAD:
            return max(radius * DEATH_RAGDOLL_VISUAL_SCALE, 14.0f);
        case DEATH_RAGDOLL_LIMB_LEFT_HAND:
        case DEATH_RAGDOLL_LIMB_RIGHT_HAND:
        case DEATH_RAGDOLL_LIMB_LEFT_FOOT:
        case DEATH_RAGDOLL_LIMB_RIGHT_FOOT:
            return max(radius * DEATH_RAGDOLL_VISUAL_SCALE, 8.0f);
        default:
            return max(radius * DEATH_RAGDOLL_VISUAL_SCALE, 9.0f);
    }
}

static u8 death_ragdoll_limb_can_lift_center(s32 limb) {
    return limb == DEATH_RAGDOLL_LIMB_TORSO;
}

static f32 death_ragdoll_node_follow_strength(s32 limb) {
    switch (limb) {
        case DEATH_RAGDOLL_LIMB_TORSO:
            return 1.0f;
        case DEATH_RAGDOLL_LIMB_HEAD:
            return 0.032f;
        case DEATH_RAGDOLL_LIMB_LEFT_HAND:
        case DEATH_RAGDOLL_LIMB_RIGHT_HAND:
        case DEATH_RAGDOLL_LIMB_LEFT_FOOT:
        case DEATH_RAGDOLL_LIMB_RIGHT_FOOT:
            return 0.020f;
        default:
            return 0.024f;
    }
}

static f32 death_ragdoll_node_max_offset(s32 limb) {
    f32 maxOffset;

    switch (limb) {
        case DEATH_RAGDOLL_LIMB_HEAD:
            maxOffset = 64.0f;
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_HAND:
        case DEATH_RAGDOLL_LIMB_RIGHT_HAND:
        case DEATH_RAGDOLL_LIMB_LEFT_FOOT:
        case DEATH_RAGDOLL_LIMB_RIGHT_FOOT:
            maxOffset = DEATH_RAGDOLL_NODE_MAX_OFFSET;
            break;
        default:
            maxOffset = 60.0f;
            break;
    }

    return maxOffset * DEATH_RAGDOLL_VISUAL_SCALE;
}

static s32 death_ragdoll_parent_limb(s32 limb) {
    switch (limb) {
        case DEATH_RAGDOLL_LIMB_HEAD:
        case DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM:
        case DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM:
        case DEATH_RAGDOLL_LIMB_LEFT_THIGH:
        case DEATH_RAGDOLL_LIMB_RIGHT_THIGH:
            return DEATH_RAGDOLL_LIMB_TORSO;
        case DEATH_RAGDOLL_LIMB_LEFT_FOREARM:
            return DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM;
        case DEATH_RAGDOLL_LIMB_LEFT_HAND:
            return DEATH_RAGDOLL_LIMB_LEFT_FOREARM;
        case DEATH_RAGDOLL_LIMB_RIGHT_FOREARM:
            return DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM;
        case DEATH_RAGDOLL_LIMB_RIGHT_HAND:
            return DEATH_RAGDOLL_LIMB_RIGHT_FOREARM;
        case DEATH_RAGDOLL_LIMB_LEFT_LEG:
            return DEATH_RAGDOLL_LIMB_LEFT_THIGH;
        case DEATH_RAGDOLL_LIMB_LEFT_FOOT:
            return DEATH_RAGDOLL_LIMB_LEFT_LEG;
        case DEATH_RAGDOLL_LIMB_RIGHT_LEG:
            return DEATH_RAGDOLL_LIMB_RIGHT_THIGH;
        case DEATH_RAGDOLL_LIMB_RIGHT_FOOT:
            return DEATH_RAGDOLL_LIMB_RIGHT_LEG;
    }

    return DEATH_RAGDOLL_LIMB_TORSO;
}

static f32 death_ragdoll_segment_radius(f32 radius) {
    return death_ragdoll_clampf(radius * 0.72f, 9.0f, radius);
}

static void death_ragdoll_add_rotated_offset(Vec3f pos, f32 x, f32 y, f32 z, Vec3s angle);
static void death_ragdoll_get_component_probe(struct MarioState *m, s32 limb, Vec3f probe, f32 *radius);
static s32 death_ragdoll_render_angle_target_limb(s32 limb);
static void death_ragdoll_apply_limb_position_correction(s32 limb, f32 x, f32 y, f32 z);
static void death_ragdoll_apply_body_node_position_correction(s32 bodyNode, f32 x, f32 y, f32 z);
static void death_ragdoll_add_launch_angular_velocity(struct MarioState *m, enum DeathRagdollSource source);
static void death_ragdoll_apply_angular_velocity_field(struct MarioState *m);
static void death_ragdoll_clamp_angular_velocity(void);
s32 launch_mario_until_land(struct MarioState *m, s32 endAction, s32 animation, f32 forwardVel);

static f32 death_ragdoll_constraint_length(s32 limb) {
    f32 length;

    switch (limb) {
        case DEATH_RAGDOLL_LIMB_HEAD:
            length = 82.0f;
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM:
        case DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM:
            length = 75.0f;
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_FOREARM:
        case DEATH_RAGDOLL_LIMB_RIGHT_FOREARM:
            length = 64.0f;
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_HAND:
        case DEATH_RAGDOLL_LIMB_RIGHT_HAND:
            length = 58.0f;
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_THIGH:
        case DEATH_RAGDOLL_LIMB_RIGHT_THIGH:
            length = 66.0f;
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_LEG:
        case DEATH_RAGDOLL_LIMB_RIGHT_LEG:
            length = 82.0f;
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_FOOT:
        case DEATH_RAGDOLL_LIMB_RIGHT_FOOT:
            length = 62.0f;
            break;
        default:
            length = 42.0f;
            break;
    }

    return length * DEATH_RAGDOLL_VISUAL_SCALE;
}

static u8 death_ragdoll_is_body_root_limb(s32 limb) {
    switch (limb) {
        case DEATH_RAGDOLL_LIMB_HEAD:
        case DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM:
        case DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM:
        case DEATH_RAGDOLL_LIMB_LEFT_THIGH:
        case DEATH_RAGDOLL_LIMB_RIGHT_THIGH:
            return TRUE;
    }

    return FALSE;
}

static void death_ragdoll_get_body_root_slot(s32 limb, Vec3f slot, f32 *slack) {
    switch (limb) {
        case DEATH_RAGDOLL_LIMB_HEAD:
            vec3f_set(slot, 0.0f, 80.0f, -4.0f);
            *slack = 8.0f;
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM:
            vec3f_set(slot, 68.0f, 18.0f, 14.0f);
            *slack = 7.0f;
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM:
            vec3f_set(slot, -68.0f, 18.0f, 14.0f);
            *slack = 7.0f;
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_THIGH:
            vec3f_set(slot, 18.0f, -54.0f, 4.0f);
            *slack = 7.0f;
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_THIGH:
            vec3f_set(slot, -18.0f, -54.0f, 4.0f);
            *slack = 7.0f;
            break;
        default:
            vec3f_set(slot, 0.0f, 0.0f, 0.0f);
            *slack = 0.0f;
            break;
    }

    slot[0] *= DEATH_RAGDOLL_VISUAL_SCALE;
    slot[1] *= DEATH_RAGDOLL_VISUAL_SCALE;
    slot[2] *= DEATH_RAGDOLL_VISUAL_SCALE;
    *slack *= DEATH_RAGDOLL_VISUAL_SCALE;
}

static void death_ragdoll_world_delta_to_body_local(struct MarioState *m, Vec3f delta, Vec3f local) {
    f32 yawSin = sins(-m->marioObj->header.gfx.angle[1]);
    f32 yawCos = coss(-m->marioObj->header.gfx.angle[1]);

    local[0] = delta[0] * yawCos + delta[2] * yawSin;
    local[1] = delta[1];
    local[2] = -delta[0] * yawSin + delta[2] * yawCos;
}

static void death_ragdoll_body_local_to_world_delta(struct MarioState *m, Vec3f local, Vec3f delta) {
    f32 yawSin = sins(m->marioObj->header.gfx.angle[1]);
    f32 yawCos = coss(m->marioObj->header.gfx.angle[1]);

    delta[0] = local[0] * yawCos + local[2] * yawSin;
    delta[1] = local[1];
    delta[2] = -local[0] * yawSin + local[2] * yawCos;
}

static s16 death_ragdoll_clamp_angle(f32 angle, s16 min, s16 max) {
    angle = death_ragdoll_clampf(angle, min, max);
    return (s16) angle;
}

static void death_ragdoll_clamp_limb_angles(s32 limb) {
    switch (limb) {
        case DEATH_RAGDOLL_LIMB_TORSO:
            sDeathRagdollLimbAngle[limb][0] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][0], -0x1800, 0x1800);
            sDeathRagdollLimbAngle[limb][1] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][1], -0x1000, 0x1000);
            sDeathRagdollLimbAngle[limb][2] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][2], -0x1800, 0x1800);
            break;
        case DEATH_RAGDOLL_LIMB_HEAD:
            sDeathRagdollLimbAngle[limb][0] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][0], -0x2800, 0x2800);
            sDeathRagdollLimbAngle[limb][1] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][1], -0x3000, 0x3000);
            sDeathRagdollLimbAngle[limb][2] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][2], -0x2400, 0x2400);
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM:
            sDeathRagdollLimbAngle[limb][0] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][0], -0x5800, 0x2600);
            sDeathRagdollLimbAngle[limb][1] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][1], -0x2400, 0x4200);
            sDeathRagdollLimbAngle[limb][2] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][2], -0x1000, 0x5200);
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM:
            sDeathRagdollLimbAngle[limb][0] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][0], -0x5800, 0x2600);
            sDeathRagdollLimbAngle[limb][1] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][1], -0x4200, 0x2400);
            sDeathRagdollLimbAngle[limb][2] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][2], -0x5200, 0x1000);
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_FOREARM:
            sDeathRagdollLimbAngle[limb][0] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][0], -0x0800, 0x6800);
            sDeathRagdollLimbAngle[limb][1] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][1], -0x2200, 0x2400);
            sDeathRagdollLimbAngle[limb][2] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][2], -0x1000, 0x3600);
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_HAND:
            sDeathRagdollLimbAngle[limb][0] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][0], -0x3000, 0x3000);
            sDeathRagdollLimbAngle[limb][1] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][1], -0x2800, 0x2800);
            sDeathRagdollLimbAngle[limb][2] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][2], -0x3000, 0x3000);
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_FOREARM:
            sDeathRagdollLimbAngle[limb][0] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][0], -0x0800, 0x6800);
            sDeathRagdollLimbAngle[limb][1] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][1], -0x2400, 0x2200);
            sDeathRagdollLimbAngle[limb][2] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][2], -0x3600, 0x1000);
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_HAND:
            sDeathRagdollLimbAngle[limb][0] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][0], -0x3000, 0x3000);
            sDeathRagdollLimbAngle[limb][1] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][1], -0x2800, 0x2800);
            sDeathRagdollLimbAngle[limb][2] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][2], -0x3000, 0x3000);
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_THIGH:
            sDeathRagdollLimbAngle[limb][0] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][0], -0x3000, 0x5000);
            sDeathRagdollLimbAngle[limb][1] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][1], -0x1800, 0x2800);
            sDeathRagdollLimbAngle[limb][2] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][2], -0x1600, 0x3000);
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_THIGH:
            sDeathRagdollLimbAngle[limb][0] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][0], -0x3000, 0x5000);
            sDeathRagdollLimbAngle[limb][1] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][1], -0x2800, 0x1800);
            sDeathRagdollLimbAngle[limb][2] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][2], -0x3000, 0x1600);
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_LEG:
            sDeathRagdollLimbAngle[limb][0] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][0], -0x5000, 0x0800);
            sDeathRagdollLimbAngle[limb][1] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][1], -0x1600, 0x1600);
            sDeathRagdollLimbAngle[limb][2] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][2], -0x1400, 0x2400);
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_FOOT:
            sDeathRagdollLimbAngle[limb][0] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][0], -0x3000, 0x3000);
            sDeathRagdollLimbAngle[limb][1] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][1], -0x1800, 0x1800);
            sDeathRagdollLimbAngle[limb][2] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][2], -0x2400, 0x2400);
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_LEG:
            sDeathRagdollLimbAngle[limb][0] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][0], -0x5000, 0x0800);
            sDeathRagdollLimbAngle[limb][1] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][1], -0x1600, 0x1600);
            sDeathRagdollLimbAngle[limb][2] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][2], -0x2400, 0x1400);
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_FOOT:
            sDeathRagdollLimbAngle[limb][0] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][0], -0x3000, 0x3000);
            sDeathRagdollLimbAngle[limb][1] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][1], -0x1800, 0x1800);
            sDeathRagdollLimbAngle[limb][2] = death_ragdoll_clamp_angle(sDeathRagdollLimbAngle[limb][2], -0x2400, 0x2400);
            break;
    }
}

static void death_ragdoll_clamp_body_angles(struct MarioState *m) {
    death_ragdoll_clamp_angular_velocity();

    if (m->marioObj->header.gfx.angle[0] < -DEATH_RAGDOLL_BODY_MAX_TILT) {
        m->marioObj->header.gfx.angle[0] = -DEATH_RAGDOLL_BODY_MAX_TILT;
        sDeathRagdollAngularVel[0] *= DEATH_RAGDOLL_SOFT_LIMIT_REBOUND;
    } else if (m->marioObj->header.gfx.angle[0] > DEATH_RAGDOLL_BODY_MAX_TILT) {
        m->marioObj->header.gfx.angle[0] = DEATH_RAGDOLL_BODY_MAX_TILT;
        sDeathRagdollAngularVel[0] *= DEATH_RAGDOLL_SOFT_LIMIT_REBOUND;
    }

    if (m->marioObj->header.gfx.angle[2] < -DEATH_RAGDOLL_BODY_MAX_TILT) {
        m->marioObj->header.gfx.angle[2] = -DEATH_RAGDOLL_BODY_MAX_TILT;
        sDeathRagdollAngularVel[2] *= DEATH_RAGDOLL_SOFT_LIMIT_REBOUND;
    } else if (m->marioObj->header.gfx.angle[2] > DEATH_RAGDOLL_BODY_MAX_TILT) {
        m->marioObj->header.gfx.angle[2] = DEATH_RAGDOLL_BODY_MAX_TILT;
        sDeathRagdollAngularVel[2] *= DEATH_RAGDOLL_SOFT_LIMIT_REBOUND;
    }
}

static void death_ragdoll_rotate_local_offset(Vec3f out, Vec3f local, Vec3s angle) {
    f32 sp = sins(angle[0]);
    f32 cp = coss(angle[0]);
    f32 sy = sins(angle[1]);
    f32 cy = coss(angle[1]);
    f32 sr = sins(angle[2]);
    f32 cr = coss(angle[2]);
    f32 x = local[0] * cr - local[1] * sr;
    f32 y = local[0] * sr + local[1] * cr;
    f32 z = local[2];
    f32 py = y * cp - z * sp;
    f32 pz = y * sp + z * cp;

    out[0] = x * cy + pz * sy;
    out[1] = py;
    out[2] = -x * sy + pz * cy;
}

static void death_ragdoll_add_rotated_offset(Vec3f pos, f32 x, f32 y, f32 z, Vec3s angle) {
    Vec3f local;
    Vec3f offset;

    vec3f_set(local, x * DEATH_RAGDOLL_VISUAL_SCALE, y * DEATH_RAGDOLL_VISUAL_SCALE,
              z * DEATH_RAGDOLL_VISUAL_SCALE);
    death_ragdoll_rotate_local_offset(offset, local, angle);
    pos[0] += offset[0];
    pos[1] += offset[1];
    pos[2] += offset[2];
}

static void death_ragdoll_sum_angles(Vec3s out, Vec3s base, s32 limbA, s32 limbB, s32 limbC) {
    vec3s_copy(out, base);

    if (limbA >= 0) {
        out[0] += (s16) sDeathRagdollLimbAngle[limbA][0];
        out[1] += (s16) sDeathRagdollLimbAngle[limbA][1];
        out[2] += (s16) sDeathRagdollLimbAngle[limbA][2];
    }
    if (limbB >= 0) {
        out[0] += (s16) sDeathRagdollLimbAngle[limbB][0];
        out[1] += (s16) sDeathRagdollLimbAngle[limbB][1];
        out[2] += (s16) sDeathRagdollLimbAngle[limbB][2];
    }
    if (limbC >= 0) {
        out[0] += (s16) sDeathRagdollLimbAngle[limbC][0];
        out[1] += (s16) sDeathRagdollLimbAngle[limbC][1];
        out[2] += (s16) sDeathRagdollLimbAngle[limbC][2];
    }
}

static void death_ragdoll_get_limb_rest_pose(s32 limb, Vec3f rest) {
    switch (limb) {
        case DEATH_RAGDOLL_LIMB_TORSO:
            vec3f_set(rest, 0, 0, 0);
            break;
        case DEATH_RAGDOLL_LIMB_HEAD:
            vec3f_set(rest, 0, 0, 0);
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM:
            vec3f_set(rest, -0x1800,  0x0800,  0x1600);
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_FOREARM:
            vec3f_set(rest,  0x2400, -0x0500,  0x0A00);
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_HAND:
            vec3f_set(rest, 0, 0, 0);
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM:
            vec3f_set(rest, -0x1800, -0x0800, -0x1600);
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_FOREARM:
            vec3f_set(rest,  0x2400,  0x0500, -0x0A00);
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_HAND:
            vec3f_set(rest, 0, 0, 0);
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_THIGH:
            vec3f_set(rest,  0x0C00,  0x0300,  0x0600);
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_LEG:
            vec3f_set(rest, -0x1800, -0x0200,  0x0400);
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_FOOT:
            vec3f_set(rest, 0, 0, 0);
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_THIGH:
            vec3f_set(rest,  0x0C00, -0x0300, -0x0600);
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_LEG:
            vec3f_set(rest, -0x1800,  0x0200, -0x0400);
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_FOOT:
            vec3f_set(rest, 0, 0, 0);
            break;
    }
}

static void death_ragdoll_update_limb_physics(struct MarioState *m) {
    s32 limb;
    Vec3f rest;
    f32 speed = death_ragdoll_clampf(death_ragdoll_absf(m->forwardVel) / 42.0f, 0.0f, 1.0f);
    f32 fall = death_ragdoll_clampf(-m->vel[1] / 58.0f, -0.4f, 1.0f);
    f32 noise = 1.0f;

    for (limb = 0; limb < DEATH_RAGDOLL_LIMB_COUNT; limb++) {
        Vec3f unclamped;

        death_ragdoll_get_limb_rest_pose(limb, rest);

        if (limb == DEATH_RAGDOLL_LIMB_TORSO) {
            rest[0] += fall * 0x0800;
            rest[2] += death_ragdoll_seeded_wave(m->actionTimer, limb, 3, 0x240) * 0x0800;
        } else if (limb == DEATH_RAGDOLL_LIMB_HEAD) {
            rest[0] += fall * 0x1000;
            rest[2] += death_ragdoll_seeded_wave(m->actionTimer, limb, 3, 0x300) * 0x1000;
        } else if (limb < DEATH_RAGDOLL_LIMB_LEFT_THIGH) {
            rest[0] += fall * 0x1600;
        } else {
            rest[0] += fall * -0x0C00;
        }
        rest[1] += death_ragdoll_seeded_wave(m->actionTimer, limb, 2, 0x260) * 0x0500 * speed;
        rest[2] += death_ragdoll_seeded_wave(m->actionTimer, limb, 3, 0x2F0) * 0x0400 * noise;

        sDeathRagdollLimbVel[limb][0] += (rest[0] - sDeathRagdollLimbAngle[limb][0]) * death_ragdoll_joint_spring(limb);
        sDeathRagdollLimbVel[limb][1] += (rest[1] - sDeathRagdollLimbAngle[limb][1]) * death_ragdoll_joint_spring(limb);
        sDeathRagdollLimbVel[limb][2] += (rest[2] - sDeathRagdollLimbAngle[limb][2]) * death_ragdoll_joint_spring(limb);

        sDeathRagdollLimbVel[limb][0] += death_ragdoll_seeded_wave(m->actionTimer, limb, 0, 0x510) * 18.0f * noise;
        sDeathRagdollLimbVel[limb][1] += death_ragdoll_seeded_wave(m->actionTimer, limb, 1, 0x470) * 12.0f * noise;
        sDeathRagdollLimbVel[limb][2] += death_ragdoll_seeded_wave(m->actionTimer, limb, 4, 0x590) * 16.0f * noise;

        sDeathRagdollLimbVel[limb][0] *= 0.955f;
        sDeathRagdollLimbVel[limb][1] *= 0.960f;
        sDeathRagdollLimbVel[limb][2] *= 0.955f;

        sDeathRagdollLimbAngle[limb][0] += sDeathRagdollLimbVel[limb][0];
        sDeathRagdollLimbAngle[limb][1] += sDeathRagdollLimbVel[limb][1];
        sDeathRagdollLimbAngle[limb][2] += sDeathRagdollLimbVel[limb][2];

        vec3f_copy(unclamped, sDeathRagdollLimbAngle[limb]);
        death_ragdoll_clamp_limb_angles(limb);
        if (sDeathRagdollLimbAngle[limb][0] != unclamped[0]) {
            sDeathRagdollLimbVel[limb][0] *= DEATH_RAGDOLL_SOFT_LIMIT_REBOUND;
        }
        if (sDeathRagdollLimbAngle[limb][1] != unclamped[1]) {
            sDeathRagdollLimbVel[limb][1] *= DEATH_RAGDOLL_SOFT_LIMIT_REBOUND;
        }
        if (sDeathRagdollLimbAngle[limb][2] != unclamped[2]) {
            sDeathRagdollLimbVel[limb][2] *= DEATH_RAGDOLL_SOFT_LIMIT_REBOUND;
        }

        if (sDeathRagdollNodeHitTimer[limb] > 0) {
            sDeathRagdollNodeHitTimer[limb]--;
        }
    }
}

static void death_ragdoll_set_center_from_mario(struct MarioState *m) {
    Vec3f localCenterOffset;
    Vec3f worldCenterOffset;

    vec3f_set(localCenterOffset, 0.0f, DEATH_RAGDOLL_CENTER_Y, 0.0f);
    death_ragdoll_rotate_local_offset(worldCenterOffset, localCenterOffset,
                                      m->marioObj->header.gfx.angle);

    sDeathRagdollCenter[0] = m->pos[0] + worldCenterOffset[0];
    sDeathRagdollCenter[1] = m->pos[1] + worldCenterOffset[1];
    sDeathRagdollCenter[2] = m->pos[2] + worldCenterOffset[2];
}

static f32 death_ragdoll_max_horizontal_speed(void) {
    return DEATH_RAGDOLL_MAX_HORIZONTAL_SPEED * death_ragdoll_horizontal_impulse_multiplier();
}

static f32 death_ragdoll_max_upward_speed(void) {
    return DEATH_RAGDOLL_MAX_UPWARD_SPEED * death_ragdoll_upward_impulse_multiplier();
}

static void death_ragdoll_sync_mario_to_center(struct MarioState *m) {
    Vec3f localCenterOffset;
    Vec3f worldCenterOffset;

    vec3f_set(localCenterOffset, 0.0f, DEATH_RAGDOLL_CENTER_Y, 0.0f);
    death_ragdoll_rotate_local_offset(worldCenterOffset, localCenterOffset,
                                      m->marioObj->header.gfx.angle);

    m->pos[0] = sDeathRagdollCenter[0] - worldCenterOffset[0];
    m->pos[1] = sDeathRagdollCenter[1] - worldCenterOffset[1];
    m->pos[2] = sDeathRagdollCenter[2] - worldCenterOffset[2];
    m->marioObj->oPosX = m->pos[0];
    m->marioObj->oPosY = m->pos[1];
    m->marioObj->oPosZ = m->pos[2];
    vec3f_copy(m->marioObj->header.gfx.pos, m->pos);
}

static void death_ragdoll_cap_velocity(struct MarioState *m) {
    f32 speed;
    f32 maxHorizontal = death_ragdoll_max_horizontal_speed();

    if (!death_ragdoll_vec3f_is_safe(m->vel)) {
        vec3f_set(m->vel, 0.0f, 0.0f, 0.0f);
        m->forwardVel = 0.0f;
        return;
    }

    speed = sqrtf(m->vel[0] * m->vel[0] + m->vel[2] * m->vel[2]);
    if (speed > maxHorizontal) {
        f32 scale = maxHorizontal / speed;
        m->vel[0] *= scale;
        m->vel[2] *= scale;
        m->forwardVel *= scale;
    }

    m->vel[1] = death_ragdoll_clampf(m->vel[1], DEATH_RAGDOLL_MAX_FALL_SPEED,
                                     death_ragdoll_max_upward_speed());
}

static s32 death_ragdoll_begin_death_warp(struct MarioState *m) {
    if (!sDeathRagdollWarpStarted) {
        level_trigger_warp(m, WARP_OP_DEATH);
        sDeathRagdollWarpStarted = TRUE;
        sDeathRagdollSkipDeathExitSpawn = TRUE;
        if (m->actionTimer < DEATH_RAGDOLL_DURATION) {
            m->actionTimer = DEATH_RAGDOLL_DURATION;
        }
    }
    return FALSE;
}

u8 death_ragdoll_consume_death_warp_spawn(void) {
    if (!sDeathRagdollSkipDeathExitSpawn) {
        return FALSE;
    }

    sDeathRagdollSkipDeathExitSpawn = FALSE;
    return TRUE;
}

Gfx *death_ragdoll_geo_switch_visual(s32 callContext, struct GraphNode *node, UNUSED Mat4 *mtx) {
    struct GraphNodeSwitchCase *switchCase = (struct GraphNodeSwitchCase *) node;

    if (callContext == GEO_CONTEXT_RENDER) {
        switchCase->selectedCase = (gMarioState != NULL && gMarioState->action == ACT_DEATH_RAGDOLL) ? 1 : 0;
    }

    return NULL;
}

static void death_ragdoll_get_render_local_pos(struct MarioState *m, s32 limb, Vec3f localPos) {
    Vec3f delta;
    f32 yawSin;
    f32 yawCos;

    vec3f_dif(delta, sDeathRagdollNodePos[limb], m->marioObj->header.gfx.pos);
    yawSin = sins(-m->marioObj->header.gfx.angle[1]);
    yawCos = coss(-m->marioObj->header.gfx.angle[1]);

    localPos[0] = delta[0] * yawCos + delta[2] * yawSin;
    localPos[1] = delta[1];
    localPos[2] = -delta[0] * yawSin + delta[2] * yawCos;
}

static void death_ragdoll_get_visual_part_world_pos(s32 limb, Vec3f pos) {
    s32 target = death_ragdoll_render_angle_target_limb(limb);

    if (target != limb) {
        pos[0] = (sDeathRagdollNodePos[limb][0] + sDeathRagdollNodePos[target][0]) * 0.5f;
        pos[1] = (sDeathRagdollNodePos[limb][1] + sDeathRagdollNodePos[target][1]) * 0.5f;
        pos[2] = (sDeathRagdollNodePos[limb][2] + sDeathRagdollNodePos[target][2]) * 0.5f;
        return;
    }

    vec3f_copy(pos, sDeathRagdollNodePos[limb]);
}

static void death_ragdoll_get_visual_part_local_pos(struct MarioState *m, s32 limb, Vec3f localPos) {
    Vec3f worldPos;
    Vec3f delta;
    f32 yawSin;
    f32 yawCos;

    death_ragdoll_get_visual_part_world_pos(limb, worldPos);
    vec3f_dif(delta, worldPos, m->marioObj->header.gfx.pos);
    yawSin = sins(-m->marioObj->header.gfx.angle[1]);
    yawCos = coss(-m->marioObj->header.gfx.angle[1]);

    localPos[0] = delta[0] * yawCos + delta[2] * yawSin;
    localPos[1] = delta[1];
    localPos[2] = -delta[0] * yawSin + delta[2] * yawCos;
}

static s32 death_ragdoll_render_angle_target_limb(s32 limb) {
    switch (limb) {
        case DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM:
            return DEATH_RAGDOLL_LIMB_LEFT_FOREARM;
        case DEATH_RAGDOLL_LIMB_LEFT_FOREARM:
            return DEATH_RAGDOLL_LIMB_LEFT_HAND;
        case DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM:
            return DEATH_RAGDOLL_LIMB_RIGHT_FOREARM;
        case DEATH_RAGDOLL_LIMB_RIGHT_FOREARM:
            return DEATH_RAGDOLL_LIMB_RIGHT_HAND;
        case DEATH_RAGDOLL_LIMB_LEFT_THIGH:
            return DEATH_RAGDOLL_LIMB_LEFT_LEG;
        case DEATH_RAGDOLL_LIMB_LEFT_LEG:
            return DEATH_RAGDOLL_LIMB_LEFT_FOOT;
        case DEATH_RAGDOLL_LIMB_RIGHT_THIGH:
            return DEATH_RAGDOLL_LIMB_RIGHT_LEG;
        case DEATH_RAGDOLL_LIMB_RIGHT_LEG:
            return DEATH_RAGDOLL_LIMB_RIGHT_FOOT;
        default:
            return limb;
    }
}

static void death_ragdoll_world_to_render_delta(struct MarioState *m, Vec3f from, Vec3f to, Vec3f delta) {
    Vec3f worldDelta;
    f32 yawSin = sins(-m->marioObj->header.gfx.angle[1]);
    f32 yawCos = coss(-m->marioObj->header.gfx.angle[1]);

    vec3f_dif(worldDelta, to, from);
    delta[0] = worldDelta[0] * yawCos + worldDelta[2] * yawSin;
    delta[1] = worldDelta[1];
    delta[2] = -worldDelta[0] * yawSin + worldDelta[2] * yawCos;
}

static void death_ragdoll_get_upper_body_axes(struct MarioState *m, Vec3f xAxis, Vec3f yAxis, Vec3f zAxis) {
    Vec3f backAxis;

    death_ragdoll_world_to_render_delta(m,
                                        sDeathRagdollBodyNodePos[DEATH_RAGDOLL_BODY_NODE_PELVIS],
                                        sDeathRagdollBodyNodePos[DEATH_RAGDOLL_BODY_NODE_CHEST],
                                        xAxis);
    if (sqrtf(xAxis[0] * xAxis[0] + xAxis[1] * xAxis[1] + xAxis[2] * xAxis[2]) < 1.0f) {
        vec3f_set(xAxis, 1.0f, 0.0f, 0.0f);
    }
    vec3f_normalize(xAxis);

    death_ragdoll_world_to_render_delta(m,
                                        sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_TORSO],
                                        sDeathRagdollBodyNodePos[DEATH_RAGDOLL_BODY_NODE_BACK],
                                        backAxis);
    if (sqrtf(backAxis[0] * backAxis[0] + backAxis[1] * backAxis[1] + backAxis[2] * backAxis[2]) < 1.0f) {
        vec3f_set(backAxis, 0.0f, 0.0f, 1.0f);
    }
    vec3f_normalize(backAxis);

    vec3f_cross(yAxis, backAxis, xAxis);
    if (sqrtf(yAxis[0] * yAxis[0] + yAxis[1] * yAxis[1] + yAxis[2] * yAxis[2]) < 0.1f) {
        vec3f_set(yAxis, 0.0f, 1.0f, 0.0f);
    }
    vec3f_normalize(yAxis);
    vec3f_cross(zAxis, xAxis, yAxis);
    vec3f_normalize(zAxis);
}

static void death_ragdoll_make_upper_body_matrix(struct MarioState *m, Vec3f localPos,
                                                 Mat4 matrix) {
    Vec3f xAxis;
    Vec3f yAxis;
    Vec3f zAxis;
    Vec3f scale;

    death_ragdoll_get_upper_body_axes(m, xAxis, yAxis, zAxis);
    death_ragdoll_visual_axis_scale(scale);

    matrix[0][0] = xAxis[0] * scale[0];
    matrix[0][1] = xAxis[1] * scale[0];
    matrix[0][2] = xAxis[2] * scale[0];
    matrix[0][3] = 0.0f;
    matrix[1][0] = yAxis[0] * scale[1];
    matrix[1][1] = yAxis[1] * scale[1];
    matrix[1][2] = yAxis[2] * scale[1];
    matrix[1][3] = 0.0f;
    matrix[2][0] = zAxis[0] * scale[2];
    matrix[2][1] = zAxis[1] * scale[2];
    matrix[2][2] = zAxis[2] * scale[2];
    matrix[2][3] = 0.0f;
    matrix[3][0] = localPos[0];
    matrix[3][1] = localPos[1];
    matrix[3][2] = localPos[2];
    matrix[3][3] = 1.0f;
}

static void death_ragdoll_make_head_matrix(struct MarioState *m, Vec3f localPos, Mat4 matrix) {
    Vec3f xAxis;
    Vec3f yAxis;
    Vec3f zAxis;
    Vec3f torsoX;
    Vec3f torsoY;
    Vec3f torsoZ;
    Vec3f refAxis;
    Vec3f scale;

    death_ragdoll_world_to_render_delta(m,
                                        sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_TORSO],
                                        sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_HEAD],
                                        xAxis);
    if (sqrtf(xAxis[0] * xAxis[0] + xAxis[1] * xAxis[1] + xAxis[2] * xAxis[2]) < 0.5f) {
        death_ragdoll_get_upper_body_axes(m, xAxis, torsoY, torsoZ);
    }
    vec3f_normalize(xAxis);

    death_ragdoll_get_upper_body_axes(m, torsoX, torsoY, torsoZ);
    vec3f_copy(refAxis, torsoZ);
    if (death_ragdoll_absf(xAxis[0] * refAxis[0] + xAxis[1] * refAxis[1] + xAxis[2] * refAxis[2]) > 0.88f) {
        vec3f_copy(refAxis, torsoY);
    }

    vec3f_cross(yAxis, refAxis, xAxis);
    if (sqrtf(yAxis[0] * yAxis[0] + yAxis[1] * yAxis[1] + yAxis[2] * yAxis[2]) < 0.1f) {
        vec3f_copy(yAxis, torsoY);
    }
    vec3f_normalize(yAxis);
    vec3f_cross(zAxis, xAxis, yAxis);
    vec3f_normalize(zAxis);
    death_ragdoll_visual_axis_scale(scale);

    matrix[0][0] = xAxis[0] * scale[0];
    matrix[0][1] = xAxis[1] * scale[0];
    matrix[0][2] = xAxis[2] * scale[0];
    matrix[0][3] = 0.0f;
    matrix[1][0] = yAxis[0] * scale[1];
    matrix[1][1] = yAxis[1] * scale[1];
    matrix[1][2] = yAxis[2] * scale[1];
    matrix[1][3] = 0.0f;
    matrix[2][0] = zAxis[0] * scale[2];
    matrix[2][1] = zAxis[1] * scale[2];
    matrix[2][2] = zAxis[2] * scale[2];
    matrix[2][3] = 0.0f;
    matrix[3][0] = localPos[0];
    matrix[3][1] = localPos[1];
    matrix[3][2] = localPos[2];
    matrix[3][3] = 1.0f;
}

static void death_ragdoll_make_limb_segment_matrix(struct MarioState *m, s32 limb,
                                                   Vec3f localPos, Mat4 matrix) {
    Vec3f from;
    Vec3f to;
    Vec3f xAxis;
    Vec3f yAxis;
    Vec3f zAxis;
    Vec3f torsoX;
    Vec3f torsoY;
    Vec3f torsoZ;
    Vec3f refAxis;
    s32 target = death_ragdoll_render_angle_target_limb(limb);
    Vec3f scale;

    if (target == limb) {
        vec3f_copy(from, sDeathRagdollNodePos[death_ragdoll_parent_limb(limb)]);
        vec3f_copy(to, sDeathRagdollNodePos[limb]);
    } else {
        vec3f_copy(from, sDeathRagdollNodePos[limb]);
        vec3f_copy(to, sDeathRagdollNodePos[target]);
    }

    death_ragdoll_world_to_render_delta(m, from, to, xAxis);
    if (sqrtf(xAxis[0] * xAxis[0] + xAxis[1] * xAxis[1] + xAxis[2] * xAxis[2]) < 0.5f) {
        vec3f_set(xAxis, 1.0f, 0.0f, 0.0f);
    }
    vec3f_normalize(xAxis);

    death_ragdoll_get_upper_body_axes(m, torsoX, torsoY, torsoZ);
    vec3f_copy(refAxis, torsoZ);
    if (death_ragdoll_absf(xAxis[0] * refAxis[0] + xAxis[1] * refAxis[1] + xAxis[2] * refAxis[2]) > 0.86f) {
        vec3f_copy(refAxis, torsoY);
    }

    vec3f_cross(yAxis, refAxis, xAxis);
    if (sqrtf(yAxis[0] * yAxis[0] + yAxis[1] * yAxis[1] + yAxis[2] * yAxis[2]) < 0.1f) {
        vec3f_set(yAxis, 0.0f, 1.0f, 0.0f);
    }
    vec3f_normalize(yAxis);
    vec3f_cross(zAxis, xAxis, yAxis);
    vec3f_normalize(zAxis);
    death_ragdoll_visual_axis_scale(scale);

    matrix[0][0] = xAxis[0] * scale[0];
    matrix[0][1] = xAxis[1] * scale[0];
    matrix[0][2] = xAxis[2] * scale[0];
    matrix[0][3] = 0.0f;
    matrix[1][0] = yAxis[0] * scale[1];
    matrix[1][1] = yAxis[1] * scale[1];
    matrix[1][2] = yAxis[2] * scale[1];
    matrix[1][3] = 0.0f;
    matrix[2][0] = zAxis[0] * scale[2];
    matrix[2][1] = zAxis[1] * scale[2];
    matrix[2][2] = zAxis[2] * scale[2];
    matrix[2][3] = 0.0f;
    matrix[3][0] = localPos[0];
    matrix[3][1] = localPos[1];
    matrix[3][2] = localPos[2];
    matrix[3][3] = 1.0f;
}

static s16 death_ragdoll_limb_side_sign(s32 limb) {
    switch (limb) {
        case DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM:
        case DEATH_RAGDOLL_LIMB_LEFT_FOREARM:
        case DEATH_RAGDOLL_LIMB_LEFT_HAND:
        case DEATH_RAGDOLL_LIMB_LEFT_THIGH:
        case DEATH_RAGDOLL_LIMB_LEFT_LEG:
        case DEATH_RAGDOLL_LIMB_LEFT_FOOT:
            return 1;
        case DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM:
        case DEATH_RAGDOLL_LIMB_RIGHT_FOREARM:
        case DEATH_RAGDOLL_LIMB_RIGHT_HAND:
        case DEATH_RAGDOLL_LIMB_RIGHT_THIGH:
        case DEATH_RAGDOLL_LIMB_RIGHT_LEG:
        case DEATH_RAGDOLL_LIMB_RIGHT_FOOT:
            return -1;
    }

    return 0;
}

static void death_ragdoll_get_torso_render_angle(struct MarioState *m, Vec3s angle) {
    Vec3f up;
    Vec3f back;
    Vec3f side;
    Vec3f renderUp;
    Vec3f renderSide;
    f32 upLen;
    f32 sideLen;

    vec3f_dif(up,
              sDeathRagdollBodyNodePos[DEATH_RAGDOLL_BODY_NODE_CHEST],
              sDeathRagdollBodyNodePos[DEATH_RAGDOLL_BODY_NODE_PELVIS]);
    vec3f_dif(back,
              sDeathRagdollBodyNodePos[DEATH_RAGDOLL_BODY_NODE_BACK],
              sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_TORSO]);

    side[0] = up[1] * back[2] - up[2] * back[1];
    side[1] = up[2] * back[0] - up[0] * back[2];
    side[2] = up[0] * back[1] - up[1] * back[0];
    upLen = sqrtf(up[0] * up[0] + up[1] * up[1] + up[2] * up[2]);
    sideLen = sqrtf(side[0] * side[0] + side[1] * side[1] + side[2] * side[2]);
    if (upLen < 1.0f || sideLen < 1.0f) {
        angle[0] = (s16) sDeathRagdollLimbAngle[DEATH_RAGDOLL_LIMB_TORSO][0];
        angle[1] = (s16) sDeathRagdollLimbAngle[DEATH_RAGDOLL_LIMB_TORSO][1];
        angle[2] = (s16) sDeathRagdollLimbAngle[DEATH_RAGDOLL_LIMB_TORSO][2];
        return;
    }

    death_ragdoll_world_to_render_delta(m,
                                        sDeathRagdollBodyNodePos[DEATH_RAGDOLL_BODY_NODE_PELVIS],
                                        sDeathRagdollBodyNodePos[DEATH_RAGDOLL_BODY_NODE_CHEST],
                                        renderUp);
    {
        Vec3f sideTarget;

        sideTarget[0] = sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_TORSO][0] + side[0] / sideLen * 40.0f;
        sideTarget[1] = sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_TORSO][1] + side[1] / sideLen * 40.0f;
        sideTarget[2] = sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_TORSO][2] + side[2] / sideLen * 40.0f;
        death_ragdoll_world_to_render_delta(m, sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_TORSO],
                                            sideTarget, renderSide);
    }

    angle[0] = atan2s(renderUp[2], renderUp[1]);
    angle[1] = atan2s(-renderUp[0], renderUp[1]);
    angle[2] = atan2s(renderSide[1], sqrtf(renderSide[0] * renderSide[0]
                                         + renderSide[2] * renderSide[2]));
}

static void death_ragdoll_get_raw_render_angle(struct MarioState *m, s32 limb, Vec3s angle) {
    Vec3f delta;
    f32 lateral;

    if (limb == DEATH_RAGDOLL_LIMB_TORSO) {
        death_ragdoll_get_torso_render_angle(m, angle);
        return;
    }

    if (limb == DEATH_RAGDOLL_LIMB_HEAD) {
        Vec3s torsoAngle;

        death_ragdoll_get_torso_render_angle(m, torsoAngle);
        angle[0] = torsoAngle[0];
        angle[1] = torsoAngle[1];
        angle[2] = torsoAngle[2];
        return;
    }

    if (death_ragdoll_render_angle_target_limb(limb) == limb) {
        death_ragdoll_world_to_render_delta(m, sDeathRagdollNodePos[death_ragdoll_parent_limb(limb)],
                                            sDeathRagdollNodePos[limb], delta);
    } else {
        death_ragdoll_world_to_render_delta(m, sDeathRagdollNodePos[limb],
                                            sDeathRagdollNodePos[death_ragdoll_render_angle_target_limb(limb)],
                                            delta);
    }
    lateral = sqrtf(delta[0] * delta[0] + delta[1] * delta[1]);
    angle[0] = atan2s(delta[1], lateral) * 0.35f;
    angle[1] = atan2s(-delta[2], lateral);
    angle[2] = 0;
}

static f32 death_ragdoll_render_angle_speed_limit(s32 limb) {
    switch (limb) {
        case DEATH_RAGDOLL_LIMB_TORSO:
            return 0x4000;
        case DEATH_RAGDOLL_LIMB_HEAD:
            return 0x3000;
        case DEATH_RAGDOLL_LIMB_LEFT_HAND:
        case DEATH_RAGDOLL_LIMB_RIGHT_HAND:
        case DEATH_RAGDOLL_LIMB_LEFT_FOOT:
        case DEATH_RAGDOLL_LIMB_RIGHT_FOOT:
            return 0x0900;
        default:
            return 0x0A00;
    }
}

static void death_ragdoll_get_render_angle(struct MarioState *m, s32 limb, Vec3s angle) {
    Vec3s raw;
    f32 maxDelta;
    f32 blend;
    s32 axis;

    death_ragdoll_get_raw_render_angle(m, limb, raw);
    if (sDeathRagdollRenderAngleTimer[limb] != m->actionTimer) {
        maxDelta = death_ragdoll_render_angle_speed_limit(limb);
        blend = (limb == DEATH_RAGDOLL_LIMB_TORSO || limb == DEATH_RAGDOLL_LIMB_HEAD) ? 1.0f : 0.34f;
        if (sDeathRagdollRenderAngleTimer[limb] < 0) {
            for (axis = 0; axis < 3; axis++) {
                sDeathRagdollRenderAngle[limb][axis] = raw[axis];
            }
        } else {
            for (axis = 0; axis < 3; axis++) {
                sDeathRagdollRenderAngle[limb][axis] =
                    death_ragdoll_approach_angle(sDeathRagdollRenderAngle[limb][axis],
                                                 raw[axis], maxDelta, blend);
            }
        }
        sDeathRagdollRenderAngleTimer[limb] = m->actionTimer;
    }

    angle[0] = (s16) sDeathRagdollRenderAngle[limb][0];
    angle[1] = (s16) sDeathRagdollRenderAngle[limb][1];
    angle[2] = (s16) sDeathRagdollRenderAngle[limb][2];
}

static s32 death_ragdoll_parent_segment_limb(s32 limb) {
    switch (limb) {
        case DEATH_RAGDOLL_LIMB_LEFT_FOREARM:
            return DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM;
        case DEATH_RAGDOLL_LIMB_LEFT_HAND:
            return DEATH_RAGDOLL_LIMB_LEFT_FOREARM;
        case DEATH_RAGDOLL_LIMB_RIGHT_FOREARM:
            return DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM;
        case DEATH_RAGDOLL_LIMB_RIGHT_HAND:
            return DEATH_RAGDOLL_LIMB_RIGHT_FOREARM;
        case DEATH_RAGDOLL_LIMB_LEFT_LEG:
            return DEATH_RAGDOLL_LIMB_LEFT_THIGH;
        case DEATH_RAGDOLL_LIMB_LEFT_FOOT:
            return DEATH_RAGDOLL_LIMB_LEFT_LEG;
        case DEATH_RAGDOLL_LIMB_RIGHT_LEG:
            return DEATH_RAGDOLL_LIMB_RIGHT_THIGH;
        case DEATH_RAGDOLL_LIMB_RIGHT_FOOT:
            return DEATH_RAGDOLL_LIMB_RIGHT_LEG;
    }

    return DEATH_RAGDOLL_LIMB_TORSO;
}

#if DEATH_RAGDOLL_DEBUG_SKELETON
static void death_ragdoll_set_debug_vertex(Vtx *vertex, Vec3f pos) {
    vertex->v.ob[0] = (s16) pos[0];
    vertex->v.ob[1] = (s16) pos[1];
    vertex->v.ob[2] = (s16) pos[2];
    vertex->v.flag = 0;
    vertex->v.tc[0] = 0;
    vertex->v.tc[1] = 0;
    vertex->v.cn[0] = 255;
    vertex->v.cn[1] = 255;
    vertex->v.cn[2] = 0;
    vertex->v.cn[3] = 255;
}

static void death_ragdoll_set_debug_vertex_xyz(Vtx *vertex, f32 x, f32 y, f32 z) {
    vertex->v.ob[0] = (s16) x;
    vertex->v.ob[1] = (s16) y;
    vertex->v.ob[2] = (s16) z;
    vertex->v.flag = 0;
    vertex->v.tc[0] = 0;
    vertex->v.tc[1] = 0;
    vertex->v.cn[0] = 255;
    vertex->v.cn[1] = 255;
    vertex->v.cn[2] = 0;
    vertex->v.cn[3] = 255;
}

static Gfx *death_ragdoll_draw_debug_segment(Gfx *gfx, Vec3f a, Vec3f b, f32 width) {
    Vtx *quad = alloc_display_list(4 * sizeof(*quad));
    f32 dx = b[0] - a[0];
    f32 dy = b[1] - a[1];
    f32 dz = b[2] - a[2];
    f32 sideX = -dz;
    f32 sideY = 0.0f;
    f32 sideZ = dx;
    f32 sideLen = sqrtf(sideX * sideX + sideZ * sideZ);

    if (quad == NULL) {
        return gfx;
    }

    if (sideLen < 0.5f) {
        sideX = width;
        sideY = 0.0f;
        sideZ = 0.0f;
    } else {
        sideX = sideX / sideLen * width;
        sideZ = sideZ / sideLen * width;
    }

    if (death_ragdoll_absf(dy) > death_ragdoll_absf(dx) + death_ragdoll_absf(dz)) {
        sideY = width;
        sideX *= 0.35f;
        sideZ *= 0.35f;
    }

    death_ragdoll_set_debug_vertex_xyz(&quad[0], a[0] - sideX, a[1] - sideY, a[2] - sideZ);
    death_ragdoll_set_debug_vertex_xyz(&quad[1], a[0] + sideX, a[1] + sideY, a[2] + sideZ);
    death_ragdoll_set_debug_vertex_xyz(&quad[2], b[0] + sideX, b[1] + sideY, b[2] + sideZ);
    death_ragdoll_set_debug_vertex_xyz(&quad[3], b[0] - sideX, b[1] - sideY, b[2] - sideZ);

    gSPVertex(gfx++, VIRTUAL_TO_PHYSICAL(quad), 4, 0);
    gSP1Triangle(gfx++, 0, 1, 2, 0);
    gSP1Triangle(gfx++, 0, 2, 3, 0);
    return gfx;
}

static Gfx *death_ragdoll_draw_debug_edge(Gfx *gfx, Vec3f localPos[], s32 a, s32 b) {
    return death_ragdoll_draw_debug_segment(gfx, localPos[a], localPos[b], 5.0f);
}

static Gfx *death_ragdoll_draw_debug_node(Gfx *gfx, Vec3f localPos[], s32 limb) {
    Vec3f a;
    Vec3f b;
    f32 size = (limb == DEATH_RAGDOLL_LIMB_TORSO || limb == DEATH_RAGDOLL_LIMB_HEAD) ? 8.0f : 6.0f;

    vec3f_copy(a, localPos[limb]);
    vec3f_copy(b, localPos[limb]);
    a[0] -= size;
    b[0] += size;
    gfx = death_ragdoll_draw_debug_segment(gfx, a, b, 3.5f);

    vec3f_copy(a, localPos[limb]);
    vec3f_copy(b, localPos[limb]);
    a[1] -= size;
    b[1] += size;
    gfx = death_ragdoll_draw_debug_segment(gfx, a, b, 3.5f);

    vec3f_copy(a, localPos[limb]);
    vec3f_copy(b, localPos[limb]);
    a[2] -= size;
    b[2] += size;
    return death_ragdoll_draw_debug_segment(gfx, a, b, 3.5f);
}
#endif

static void death_ragdoll_get_geo_limb_angle(struct MarioState *m, s32 limb, Vec3s angle) {
    Vec3s worldAngle;
    Vec3s parentAngle;
    s32 parentSegment = death_ragdoll_parent_segment_limb(limb);

    death_ragdoll_get_render_angle(m, limb, worldAngle);
    if (parentSegment == DEATH_RAGDOLL_LIMB_TORSO) {
        angle[0] = (s16) death_ragdoll_angle_delta(worldAngle[0],
                                                   sDeathRagdollLimbAngle[DEATH_RAGDOLL_LIMB_TORSO][0]);
        angle[1] = worldAngle[1];
        angle[2] = (s16) death_ragdoll_angle_delta(worldAngle[2],
                                                   sDeathRagdollLimbAngle[DEATH_RAGDOLL_LIMB_TORSO][2]);
        return;
    }

    death_ragdoll_get_render_angle(m, parentSegment, parentAngle);
    angle[0] = (s16) death_ragdoll_angle_delta(worldAngle[0], parentAngle[0]);
    angle[1] = (s16) death_ragdoll_angle_delta(worldAngle[1], parentAngle[1]);
    angle[2] = 0;
}

Gfx *death_ragdoll_geo_render(s32 callContext, struct GraphNode *node, UNUSED Mat4 *mtx) {
    struct GraphNodeGenerated *asGenerated = (struct GraphNodeGenerated *) node;
    struct MarioState *m = gMarioState;
    Gfx *gfxHead;
    Gfx *gfx;
    Mtx *matrix;
    s32 limb;

    if (callContext != GEO_CONTEXT_RENDER || m == NULL || m->action != ACT_DEATH_RAGDOLL) {
        return NULL;
    }

    asGenerated->fnNode.node.flags = (asGenerated->fnNode.node.flags & 0xFF) | (LAYER_OPAQUE << 8);
    gfxHead = alloc_display_list(((DEATH_RAGDOLL_LIMB_COUNT + 1) * 3 + 260) * sizeof(*gfxHead));
    matrix = alloc_display_list((DEATH_RAGDOLL_LIMB_COUNT + 1) * sizeof(*matrix));
    if (gfxHead == NULL || matrix == NULL) {
        return NULL;
    }

    gfx = gfxHead;
    for (limb = 0; limb < DEATH_RAGDOLL_LIMB_COUNT; limb++) {
        Vec3f localPos;
        Mat4 scaledMtx;

        if (sDeathRagdollVisualParts[limb].displayList == NULL) {
            continue;
        }

        death_ragdoll_get_visual_part_local_pos(m, limb, localPos);
        if (limb == DEATH_RAGDOLL_LIMB_TORSO) {
            death_ragdoll_make_upper_body_matrix(m, localPos, scaledMtx);
        } else if (limb == DEATH_RAGDOLL_LIMB_HEAD) {
            death_ragdoll_make_head_matrix(m, localPos, scaledMtx);
        } else {
            death_ragdoll_make_limb_segment_matrix(m, limb, localPos, scaledMtx);
        }
        mtxf_to_mtx(&matrix[limb], scaledMtx);

        if (limb == DEATH_RAGDOLL_LIMB_TORSO) {
            matrix[DEATH_RAGDOLL_LIMB_COUNT] = matrix[limb];
            gSPMatrix(gfx++, VIRTUAL_TO_PHYSICAL(&matrix[DEATH_RAGDOLL_LIMB_COUNT]),
                      G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);
            gSPDisplayList(gfx++, VIRTUAL_TO_PHYSICAL(mario_butt));
            gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
        }

        gSPMatrix(gfx++, VIRTUAL_TO_PHYSICAL(&matrix[limb]), G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);
        gSPDisplayList(gfx++, VIRTUAL_TO_PHYSICAL(sDeathRagdollVisualParts[limb].displayList));
        gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
    }

#if DEATH_RAGDOLL_DEBUG_SKELETON
    if (death_ragdoll_debug_is_enabled()
#ifdef TARGET_N3DS
        && !dynamic_shadows_is_rendering_mode()
#endif
    ) {
        Vec3f debugLocalPos[DEATH_RAGDOLL_LIMB_COUNT];

        for (limb = 0; limb < DEATH_RAGDOLL_LIMB_COUNT; limb++) {
            death_ragdoll_get_render_local_pos(m, limb, debugLocalPos[limb]);
        }

        gDPPipeSync(gfx++);
        gSPClearGeometryMode(gfx++, G_LIGHTING);
        gSPClearGeometryMode(gfx++, G_ZBUFFER | G_CULL_BACK);
        gDPSetCombineMode(gfx++, G_CC_SHADE, G_CC_SHADE);
        gDPSetRenderMode(gfx++, G_RM_AA_OPA_SURF, G_RM_AA_OPA_SURF2);
        gfx = death_ragdoll_draw_debug_edge(gfx, debugLocalPos, DEATH_RAGDOLL_LIMB_TORSO, DEATH_RAGDOLL_LIMB_HEAD);
        gfx = death_ragdoll_draw_debug_edge(gfx, debugLocalPos, DEATH_RAGDOLL_LIMB_HEAD, DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM);
        gfx = death_ragdoll_draw_debug_edge(gfx, debugLocalPos, DEATH_RAGDOLL_LIMB_HEAD, DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM);
        gfx = death_ragdoll_draw_debug_edge(gfx, debugLocalPos, DEATH_RAGDOLL_LIMB_TORSO, DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM);
        gfx = death_ragdoll_draw_debug_edge(gfx, debugLocalPos, DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM, DEATH_RAGDOLL_LIMB_LEFT_FOREARM);
        gfx = death_ragdoll_draw_debug_edge(gfx, debugLocalPos, DEATH_RAGDOLL_LIMB_LEFT_FOREARM, DEATH_RAGDOLL_LIMB_LEFT_HAND);
        gfx = death_ragdoll_draw_debug_edge(gfx, debugLocalPos, DEATH_RAGDOLL_LIMB_TORSO, DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM);
        gfx = death_ragdoll_draw_debug_edge(gfx, debugLocalPos, DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM, DEATH_RAGDOLL_LIMB_RIGHT_FOREARM);
        gfx = death_ragdoll_draw_debug_edge(gfx, debugLocalPos, DEATH_RAGDOLL_LIMB_RIGHT_FOREARM, DEATH_RAGDOLL_LIMB_RIGHT_HAND);
        gfx = death_ragdoll_draw_debug_edge(gfx, debugLocalPos, DEATH_RAGDOLL_LIMB_TORSO, DEATH_RAGDOLL_LIMB_LEFT_THIGH);
        gfx = death_ragdoll_draw_debug_edge(gfx, debugLocalPos, DEATH_RAGDOLL_LIMB_LEFT_THIGH, DEATH_RAGDOLL_LIMB_LEFT_LEG);
        gfx = death_ragdoll_draw_debug_edge(gfx, debugLocalPos, DEATH_RAGDOLL_LIMB_LEFT_LEG, DEATH_RAGDOLL_LIMB_LEFT_FOOT);
        gfx = death_ragdoll_draw_debug_edge(gfx, debugLocalPos, DEATH_RAGDOLL_LIMB_TORSO, DEATH_RAGDOLL_LIMB_RIGHT_THIGH);
        gfx = death_ragdoll_draw_debug_edge(gfx, debugLocalPos, DEATH_RAGDOLL_LIMB_RIGHT_THIGH, DEATH_RAGDOLL_LIMB_RIGHT_LEG);
        gfx = death_ragdoll_draw_debug_edge(gfx, debugLocalPos, DEATH_RAGDOLL_LIMB_RIGHT_LEG, DEATH_RAGDOLL_LIMB_RIGHT_FOOT);
        gfx = death_ragdoll_draw_debug_edge(gfx, debugLocalPos, DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM, DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM);
        gfx = death_ragdoll_draw_debug_edge(gfx, debugLocalPos, DEATH_RAGDOLL_LIMB_LEFT_THIGH, DEATH_RAGDOLL_LIMB_RIGHT_THIGH);
        for (limb = 0; limb < DEATH_RAGDOLL_LIMB_COUNT; limb++) {
            gfx = death_ragdoll_draw_debug_node(gfx, debugLocalPos, limb);
        }
        gDPPipeSync(gfx++);
        gSPSetGeometryMode(gfx++, G_ZBUFFER | G_LIGHTING | G_CULL_BACK);
        gDPSetRenderMode(gfx++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
        gDPPipeSync(gfx++);
    }
#endif
    gSPEndDisplayList(gfx);

    return gfxHead;
}

Gfx *death_ragdoll_geo_rotate_limb(s32 callContext, struct GraphNode *node, UNUSED Mat4 *mtx) {
    struct GraphNodeGenerated *asGenerated = (struct GraphNodeGenerated *) node;
    struct GraphNodeRotation *rotNode;
    struct MarioState *m = gMarioState;
    s32 limb;

    if (callContext != GEO_CONTEXT_RENDER || node->next == NULL) {
        return NULL;
    }

    rotNode = (struct GraphNodeRotation *) node->next;
    vec3s_set(rotNode->rotation, 0, 0, 0);

    if (m == NULL || m->action != ACT_DEATH_RAGDOLL) {
        return NULL;
    }

    limb = asGenerated->parameter;
    death_ragdoll_get_geo_limb_angle(m, limb, rotNode->rotation);
    {
        f32 blendWeight = death_ragdoll_pose_blend_weight(m->actionTimer);
        rotNode->rotation[0] = (s16) (rotNode->rotation[0] * blendWeight);
        rotNode->rotation[1] = (s16) (rotNode->rotation[1] * blendWeight);
        rotNode->rotation[2] = (s16) (rotNode->rotation[2] * blendWeight);
    }
    rotNode->rotation[0] = death_ragdoll_limb_decay(rotNode->rotation[0], m->actionTimer);
    rotNode->rotation[1] = death_ragdoll_limb_decay(rotNode->rotation[1], m->actionTimer);
    rotNode->rotation[2] = death_ragdoll_limb_decay(rotNode->rotation[2], m->actionTimer);
    return NULL;
}

static void death_ragdoll_get_component_probe(struct MarioState *m, s32 limb, Vec3f probe, f32 *radius) {
    Vec3s angle;

    vec3f_copy(probe, sDeathRagdollCenter);
    *radius = 18.0f;

    switch (limb) {
        case DEATH_RAGDOLL_LIMB_TORSO:
            *radius = 34.0f;
            break;
        case DEATH_RAGDOLL_LIMB_HEAD:
            death_ragdoll_sum_angles(angle, m->marioObj->header.gfx.angle, DEATH_RAGDOLL_LIMB_TORSO,
                                     DEATH_RAGDOLL_LIMB_HEAD, -1);
            death_ragdoll_add_rotated_offset(probe, 0.0f, 82.0f, -4.0f, angle);
            *radius = 24.0f;
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM:
            death_ragdoll_sum_angles(angle, m->marioObj->header.gfx.angle, DEATH_RAGDOLL_LIMB_TORSO,
                                     DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM, -1);
            death_ragdoll_add_rotated_offset(probe, 58.0f, 22.0f, 16.0f, m->marioObj->header.gfx.angle);
            death_ragdoll_add_rotated_offset(probe, 28.0f, 0.0f, 0.0f, angle);
            *radius = 17.0f;
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_FOREARM:
            death_ragdoll_sum_angles(angle, m->marioObj->header.gfx.angle, DEATH_RAGDOLL_LIMB_TORSO,
                                     DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM, -1);
            death_ragdoll_add_rotated_offset(probe, 58.0f, 22.0f, 16.0f, m->marioObj->header.gfx.angle);
            death_ragdoll_add_rotated_offset(probe, 58.0f, 0.0f, 0.0f, angle);
            death_ragdoll_sum_angles(angle, angle, DEATH_RAGDOLL_LIMB_LEFT_FOREARM, -1, -1);
            death_ragdoll_add_rotated_offset(probe, 30.0f, 0.0f, 0.0f, angle);
            *radius = 15.0f;
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_HAND:
            death_ragdoll_sum_angles(angle, m->marioObj->header.gfx.angle, DEATH_RAGDOLL_LIMB_TORSO,
                                     DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM, -1);
            death_ragdoll_add_rotated_offset(probe, 58.0f, 22.0f, 16.0f, m->marioObj->header.gfx.angle);
            death_ragdoll_add_rotated_offset(probe, 58.0f, 0.0f, 0.0f, angle);
            death_ragdoll_sum_angles(angle, angle, DEATH_RAGDOLL_LIMB_LEFT_FOREARM, -1, -1);
            death_ragdoll_add_rotated_offset(probe, 58.0f, 0.0f, 0.0f, angle);
            death_ragdoll_sum_angles(angle, angle, DEATH_RAGDOLL_LIMB_LEFT_HAND, -1, -1);
            death_ragdoll_add_rotated_offset(probe, 16.0f, 0.0f, 0.0f, angle);
            *radius = 14.0f;
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM:
            death_ragdoll_sum_angles(angle, m->marioObj->header.gfx.angle, DEATH_RAGDOLL_LIMB_TORSO,
                                     DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM, -1);
            death_ragdoll_add_rotated_offset(probe, -58.0f, 22.0f, 16.0f, m->marioObj->header.gfx.angle);
            death_ragdoll_add_rotated_offset(probe, -28.0f, 0.0f, 0.0f, angle);
            *radius = 17.0f;
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_FOREARM:
            death_ragdoll_sum_angles(angle, m->marioObj->header.gfx.angle, DEATH_RAGDOLL_LIMB_TORSO,
                                     DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM, -1);
            death_ragdoll_add_rotated_offset(probe, -58.0f, 22.0f, 16.0f, m->marioObj->header.gfx.angle);
            death_ragdoll_add_rotated_offset(probe, -58.0f, 0.0f, 0.0f, angle);
            death_ragdoll_sum_angles(angle, angle, DEATH_RAGDOLL_LIMB_RIGHT_FOREARM, -1, -1);
            death_ragdoll_add_rotated_offset(probe, -30.0f, 0.0f, 0.0f, angle);
            *radius = 15.0f;
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_HAND:
            death_ragdoll_sum_angles(angle, m->marioObj->header.gfx.angle, DEATH_RAGDOLL_LIMB_TORSO,
                                     DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM, -1);
            death_ragdoll_add_rotated_offset(probe, -58.0f, 22.0f, 16.0f, m->marioObj->header.gfx.angle);
            death_ragdoll_add_rotated_offset(probe, -58.0f, 0.0f, 0.0f, angle);
            death_ragdoll_sum_angles(angle, angle, DEATH_RAGDOLL_LIMB_RIGHT_FOREARM, -1, -1);
            death_ragdoll_add_rotated_offset(probe, -58.0f, 0.0f, 0.0f, angle);
            death_ragdoll_sum_angles(angle, angle, DEATH_RAGDOLL_LIMB_RIGHT_HAND, -1, -1);
            death_ragdoll_add_rotated_offset(probe, -16.0f, 0.0f, 0.0f, angle);
            *radius = 14.0f;
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_THIGH:
            death_ragdoll_sum_angles(angle, m->marioObj->header.gfx.angle, DEATH_RAGDOLL_LIMB_TORSO,
                                     DEATH_RAGDOLL_LIMB_LEFT_THIGH, -1);
            death_ragdoll_add_rotated_offset(probe, 24.0f, -50.0f, 4.0f, m->marioObj->header.gfx.angle);
            death_ragdoll_add_rotated_offset(probe, 34.0f, 0.0f, 0.0f, angle);
            *radius = 19.0f;
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_LEG:
            death_ragdoll_sum_angles(angle, m->marioObj->header.gfx.angle, DEATH_RAGDOLL_LIMB_TORSO,
                                     DEATH_RAGDOLL_LIMB_LEFT_THIGH, -1);
            death_ragdoll_add_rotated_offset(probe, 24.0f, -50.0f, 4.0f, m->marioObj->header.gfx.angle);
            death_ragdoll_add_rotated_offset(probe, 72.0f, 0.0f, 0.0f, angle);
            death_ragdoll_sum_angles(angle, angle, DEATH_RAGDOLL_LIMB_LEFT_LEG, -1, -1);
            death_ragdoll_add_rotated_offset(probe, 36.0f, 0.0f, 0.0f, angle);
            *radius = 17.0f;
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_FOOT:
            death_ragdoll_sum_angles(angle, m->marioObj->header.gfx.angle, DEATH_RAGDOLL_LIMB_TORSO,
                                     DEATH_RAGDOLL_LIMB_LEFT_THIGH, -1);
            death_ragdoll_add_rotated_offset(probe, 24.0f, -50.0f, 4.0f, m->marioObj->header.gfx.angle);
            death_ragdoll_add_rotated_offset(probe, 72.0f, 0.0f, 0.0f, angle);
            death_ragdoll_sum_angles(angle, angle, DEATH_RAGDOLL_LIMB_LEFT_LEG, -1, -1);
            death_ragdoll_add_rotated_offset(probe, 58.0f, 0.0f, 0.0f, angle);
            death_ragdoll_sum_angles(angle, angle, DEATH_RAGDOLL_LIMB_LEFT_FOOT, -1, -1);
            death_ragdoll_add_rotated_offset(probe, 20.0f, 0.0f, 10.0f, angle);
            *radius = 16.0f;
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_THIGH:
            death_ragdoll_sum_angles(angle, m->marioObj->header.gfx.angle, DEATH_RAGDOLL_LIMB_TORSO,
                                     DEATH_RAGDOLL_LIMB_RIGHT_THIGH, -1);
            death_ragdoll_add_rotated_offset(probe, -24.0f, -50.0f, 4.0f, m->marioObj->header.gfx.angle);
            death_ragdoll_add_rotated_offset(probe, -34.0f, 0.0f, 0.0f, angle);
            *radius = 19.0f;
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_LEG:
            death_ragdoll_sum_angles(angle, m->marioObj->header.gfx.angle, DEATH_RAGDOLL_LIMB_TORSO,
                                     DEATH_RAGDOLL_LIMB_RIGHT_THIGH, -1);
            death_ragdoll_add_rotated_offset(probe, -24.0f, -50.0f, 4.0f, m->marioObj->header.gfx.angle);
            death_ragdoll_add_rotated_offset(probe, -72.0f, 0.0f, 0.0f, angle);
            death_ragdoll_sum_angles(angle, angle, DEATH_RAGDOLL_LIMB_RIGHT_LEG, -1, -1);
            death_ragdoll_add_rotated_offset(probe, -36.0f, 0.0f, 0.0f, angle);
            *radius = 17.0f;
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_FOOT:
            death_ragdoll_sum_angles(angle, m->marioObj->header.gfx.angle, DEATH_RAGDOLL_LIMB_TORSO,
                                     DEATH_RAGDOLL_LIMB_RIGHT_THIGH, -1);
            death_ragdoll_add_rotated_offset(probe, -24.0f, -50.0f, 4.0f, m->marioObj->header.gfx.angle);
            death_ragdoll_add_rotated_offset(probe, -72.0f, 0.0f, 0.0f, angle);
            death_ragdoll_sum_angles(angle, angle, DEATH_RAGDOLL_LIMB_RIGHT_LEG, -1, -1);
            death_ragdoll_add_rotated_offset(probe, -58.0f, 0.0f, 0.0f, angle);
            death_ragdoll_sum_angles(angle, angle, DEATH_RAGDOLL_LIMB_RIGHT_FOOT, -1, -1);
            death_ragdoll_add_rotated_offset(probe, -20.0f, 0.0f, 10.0f, angle);
            *radius = 16.0f;
            break;
    }
}

static void death_ragdoll_get_component_support_point(Vec3f support, Vec3f center, f32 radius) {
    support[0] = center[0];
    support[1] = center[1] - radius;
    support[2] = center[2];
}

static void death_ragdoll_get_body_contact_probe(struct MarioState *m, s32 point, Vec3f probe, f32 *radius) {
    Vec3f local;
    Vec3f offset;

    switch (point) {
        case 0:
            vec3f_set(local, 0.0f, 36.0f, 16.0f);
            *radius = 25.0f;
            break;
        case 1:
            vec3f_set(local, 0.0f, -34.0f, 8.0f);
            *radius = 24.0f;
            break;
        case 2:
            vec3f_set(local, 34.0f, 20.0f, 12.0f);
            *radius = 17.0f;
            break;
        case 3:
            vec3f_set(local, -34.0f, 20.0f, 12.0f);
            *radius = 17.0f;
            break;
        case 4:
            vec3f_set(local, 23.0f, -42.0f, 4.0f);
            *radius = 16.0f;
            break;
        default:
            vec3f_set(local, -23.0f, -42.0f, 4.0f);
            *radius = 16.0f;
            break;
    }

    vec3f_copy(probe, sDeathRagdollCenter);
    death_ragdoll_rotate_local_offset(offset, local, m->marioObj->header.gfx.angle);
    probe[0] += offset[0];
    probe[1] += offset[1];
    probe[2] += offset[2];
}

static u8 death_ragdoll_entry_is_airborne(struct MarioState *m) {
    return (m->action & ACT_FLAG_AIR) != 0;
}

static u8 death_ragdoll_entry_is_crawl_or_slide(struct MarioState *m) {
    return m->action == ACT_CRAWLING || m->action == ACT_START_CRAWLING
        || m->action == ACT_STOP_CRAWLING || (m->action & ACT_FLAG_BUTT_OR_STOMACH_SLIDE);
}

static u8 death_ragdoll_entry_is_burning(struct MarioState *m) {
    return m->action == ACT_BURNING_GROUND || m->action == ACT_BURNING_JUMP
        || m->action == ACT_BURNING_FALL || m->action == ACT_LAVA_BOOST
        || m->action == ACT_LAVA_BOOST_LAND;
}

static void death_ragdoll_apply_entry_action_pose(struct MarioState *m, s32 limb, Vec3f offset) {
    f32 air = death_ragdoll_entry_is_airborne(m) ? 1.0f : 0.0f;
    f32 crawlOrSlide = death_ragdoll_entry_is_crawl_or_slide(m) ? 1.0f : 0.0f;
    f32 burning = death_ragdoll_entry_is_burning(m) ? 1.0f : 0.0f;
    f32 jumpTuck = death_ragdoll_clampf((death_ragdoll_absf(m->vel[1]) + death_ragdoll_absf(m->forwardVel))
                                      / 54.0f, 0.0f, 1.0f) * air;
    f32 side = 0.0f;

    if (limb == DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM || limb == DEATH_RAGDOLL_LIMB_LEFT_FOREARM
        || limb == DEATH_RAGDOLL_LIMB_LEFT_HAND || limb == DEATH_RAGDOLL_LIMB_LEFT_THIGH
        || limb == DEATH_RAGDOLL_LIMB_LEFT_LEG || limb == DEATH_RAGDOLL_LIMB_LEFT_FOOT) {
        side = 1.0f;
    } else if (limb != DEATH_RAGDOLL_LIMB_TORSO && limb != DEATH_RAGDOLL_LIMB_HEAD) {
        side = -1.0f;
    }

    if (air > 0.0f) {
        switch (limb) {
            case DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM:
            case DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM:
                offset[0] = side * (62.0f + 12.0f * (1.0f - jumpTuck));
                offset[1] -= 8.0f * jumpTuck;
                offset[2] += 18.0f + 12.0f * jumpTuck;
                break;
            case DEATH_RAGDOLL_LIMB_LEFT_FOREARM:
            case DEATH_RAGDOLL_LIMB_RIGHT_FOREARM:
                offset[0] = side * (96.0f + 20.0f * (1.0f - jumpTuck));
                offset[1] -= 18.0f * jumpTuck;
                offset[2] += 28.0f + 18.0f * jumpTuck;
                break;
            case DEATH_RAGDOLL_LIMB_LEFT_HAND:
            case DEATH_RAGDOLL_LIMB_RIGHT_HAND:
                offset[0] = side * (112.0f + 26.0f * (1.0f - jumpTuck));
                offset[1] -= 28.0f * jumpTuck;
                offset[2] += 34.0f + 24.0f * jumpTuck;
                break;
            case DEATH_RAGDOLL_LIMB_LEFT_THIGH:
            case DEATH_RAGDOLL_LIMB_RIGHT_THIGH:
                offset[0] = side * 30.0f;
                offset[1] += 10.0f * jumpTuck;
                offset[2] += 18.0f + 16.0f * jumpTuck;
                break;
            case DEATH_RAGDOLL_LIMB_LEFT_LEG:
            case DEATH_RAGDOLL_LIMB_RIGHT_LEG:
                offset[0] = side * 46.0f;
                offset[1] += 22.0f * jumpTuck;
                offset[2] += 34.0f + 26.0f * jumpTuck;
                break;
            case DEATH_RAGDOLL_LIMB_LEFT_FOOT:
            case DEATH_RAGDOLL_LIMB_RIGHT_FOOT:
                offset[0] = side * 66.0f;
                offset[1] += 20.0f * jumpTuck;
                offset[2] += 48.0f + 28.0f * jumpTuck;
                break;
        }
    }

    if (crawlOrSlide > 0.0f) {
        switch (limb) {
            case DEATH_RAGDOLL_LIMB_HEAD:
                offset[1] -= 28.0f;
                offset[2] += 28.0f;
                break;
            case DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM:
            case DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM:
                offset[1] -= 20.0f;
                offset[2] += 30.0f;
                break;
            case DEATH_RAGDOLL_LIMB_LEFT_FOREARM:
            case DEATH_RAGDOLL_LIMB_RIGHT_FOREARM:
            case DEATH_RAGDOLL_LIMB_LEFT_HAND:
            case DEATH_RAGDOLL_LIMB_RIGHT_HAND:
                offset[1] -= 34.0f;
                offset[2] += 46.0f;
                break;
            case DEATH_RAGDOLL_LIMB_LEFT_THIGH:
            case DEATH_RAGDOLL_LIMB_RIGHT_THIGH:
            case DEATH_RAGDOLL_LIMB_LEFT_LEG:
            case DEATH_RAGDOLL_LIMB_RIGHT_LEG:
            case DEATH_RAGDOLL_LIMB_LEFT_FOOT:
            case DEATH_RAGDOLL_LIMB_RIGHT_FOOT:
                offset[1] += 24.0f;
                offset[2] -= 22.0f;
                break;
        }
    }

    if (burning > 0.0f) {
        switch (limb) {
            case DEATH_RAGDOLL_LIMB_LEFT_LEG:
            case DEATH_RAGDOLL_LIMB_RIGHT_LEG:
            case DEATH_RAGDOLL_LIMB_LEFT_FOOT:
            case DEATH_RAGDOLL_LIMB_RIGHT_FOOT:
                offset[1] += 18.0f;
                offset[2] += 26.0f;
                break;
            case DEATH_RAGDOLL_LIMB_LEFT_HAND:
            case DEATH_RAGDOLL_LIMB_RIGHT_HAND:
                offset[1] -= 12.0f;
                offset[2] += 18.0f;
                break;
        }
    }
}

static void death_ragdoll_get_initial_node_offset(struct MarioState *m, s32 limb, Vec3f offset) {
    s16 animFrame = m->marioObj->header.gfx.unk38.animFrame;
    f32 speed = death_ragdoll_clampf(death_ragdoll_absf(m->forwardVel) / 32.0f, 0.0f, 1.0f);
    f32 stride = sins(animFrame * 0x1200) * 14.0f * speed;
    f32 armStride = sins(animFrame * 0x1200 + 0x8000) * 16.0f * speed;

    switch (limb) {
        case DEATH_RAGDOLL_LIMB_TORSO:
            vec3f_set(offset, 0.0f, 0.0f, 0.0f);
            break;
        case DEATH_RAGDOLL_LIMB_HEAD:
            vec3f_set(offset, 0.0f, 82.0f, -4.0f);
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM:
            vec3f_set(offset, 82.0f, 16.0f, 12.0f + armStride);
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_FOREARM:
            vec3f_set(offset, 148.0f, 0.0f, 10.0f + armStride);
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_HAND:
            vec3f_set(offset, 178.0f, -8.0f, 10.0f + armStride);
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM:
            vec3f_set(offset, -82.0f, 16.0f, 12.0f - armStride);
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_FOREARM:
            vec3f_set(offset, -148.0f, 0.0f, 10.0f - armStride);
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_HAND:
            vec3f_set(offset, -178.0f, -8.0f, 10.0f - armStride);
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_THIGH:
            vec3f_set(offset, 20.0f, -56.0f, 6.0f + stride * 0.55f);
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_LEG:
            vec3f_set(offset, 30.0f, -118.0f, 2.0f + stride * 0.55f);
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_FOOT:
            vec3f_set(offset, 42.0f, -144.0f, 24.0f + stride * 0.55f);
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_THIGH:
            vec3f_set(offset, -20.0f, -56.0f, 6.0f - stride * 0.55f);
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_LEG:
            vec3f_set(offset, -30.0f, -118.0f, 2.0f - stride * 0.55f);
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_FOOT:
            vec3f_set(offset, -42.0f, -144.0f, 24.0f - stride * 0.55f);
            break;
    }

    death_ragdoll_apply_entry_action_pose(m, limb, offset);
}

static void death_ragdoll_init_node_masses(struct MarioState *m) {
    s32 limb;
    s32 bodyNode;
    Vec3s angle;

    vec3s_copy(angle, sDeathRagdollEntryGfxAngle);
    for (limb = 0; limb < DEATH_RAGDOLL_LIMB_COUNT; limb++) {
        Vec3f offset;

        death_ragdoll_get_initial_node_offset(m, limb, offset);
        vec3f_copy(sDeathRagdollNodePos[limb], sDeathRagdollCenter);
        death_ragdoll_add_rotated_offset(sDeathRagdollNodePos[limb], offset[0], offset[1], offset[2], angle);
        sDeathRagdollNodeVel[limb][0] = m->vel[0] + (random_u16() - 0x8000) / 9000.0f;
        sDeathRagdollNodeVel[limb][1] = m->vel[1] + (random_u16() - 0x8000) / 12000.0f;
        sDeathRagdollNodeVel[limb][2] = m->vel[2] + (random_u16() - 0x8000) / 9000.0f;
        vec3f_copy(sDeathRagdollNodePrevPos[limb], sDeathRagdollNodePos[limb]);
    }

    for (bodyNode = 0; bodyNode < DEATH_RAGDOLL_BODY_NODE_COUNT; bodyNode++) {
        Vec3f offset;

        switch (bodyNode) {
            case DEATH_RAGDOLL_BODY_NODE_CHEST:
                vec3f_set(offset, 0.0f, 34.0f, 8.0f);
                break;
            case DEATH_RAGDOLL_BODY_NODE_PELVIS:
                vec3f_set(offset, 0.0f, -36.0f, 4.0f);
                break;
            default:
                vec3f_set(offset, 0.0f, 4.0f, -34.0f);
                break;
        }

        vec3f_copy(sDeathRagdollBodyNodePos[bodyNode], sDeathRagdollCenter);
        death_ragdoll_add_rotated_offset(sDeathRagdollBodyNodePos[bodyNode],
                                         offset[0], offset[1], offset[2], angle);
        sDeathRagdollBodyNodeVel[bodyNode][0] = m->vel[0] + (random_u16() - 0x8000) / 10000.0f;
        sDeathRagdollBodyNodeVel[bodyNode][1] = m->vel[1] + (random_u16() - 0x8000) / 14000.0f;
        sDeathRagdollBodyNodeVel[bodyNode][2] = m->vel[2] + (random_u16() - 0x8000) / 10000.0f;
        vec3f_copy(sDeathRagdollBodyNodePrevPos[bodyNode], sDeathRagdollBodyNodePos[bodyNode]);
        sDeathRagdollBodyNodeContact[bodyNode] = FALSE;
    }
}

static void death_ragdoll_apply_entry_node_impulse(UNUSED struct MarioState *m,
                                                   enum DeathRagdollSource source) {
    s32 limb;
    f32 tumble = (source == DEATH_RAGDOLL_SOURCE_EXPLOSION) ? 0.34f : 0.22f;
    f32 lift = (source == DEATH_RAGDOLL_SOURCE_EXPLOSION) ? 0.30f : 0.16f;

    if (source == DEATH_RAGDOLL_SOURCE_DEFAULT) {
        tumble = 0.14f;
        lift = 0.10f;
    }

    for (limb = 0; limb < DEATH_RAGDOLL_LIMB_COUNT; limb++) {
        Vec3f rel;
        f32 lightness = 1.0f / death_ragdoll_node_mass(limb);
        f32 spread = (limb == DEATH_RAGDOLL_LIMB_TORSO) ? 0.18f : 1.0f;
        f32 randomSide = (random_u16() - 0x8000) / 32768.0f;

        vec3f_dif(rel, sDeathRagdollNodePos[limb], sDeathRagdollCenter);
        if (source != DEATH_RAGDOLL_SOURCE_DEFAULT && sDeathRagdollHasBlastPos) {
            Vec3f blastDir;
            f32 blastDist;
            f32 blastStrength;

            vec3f_dif(blastDir, sDeathRagdollNodePos[limb], sDeathRagdollBlastPos);
            blastDir[1] += 26.0f;
            blastDist = sqrtf(blastDir[0] * blastDir[0] + blastDir[1] * blastDir[1]
                            + blastDir[2] * blastDir[2]);
            if (blastDist > 1.0f) {
                if (source == DEATH_RAGDOLL_SOURCE_EXPLOSION) {
                    blastStrength = death_ragdoll_clampf(38.0f - blastDist * 0.10f, 12.0f, 34.0f);
                } else {
                    blastStrength = death_ragdoll_clampf(18.0f - blastDist * 0.06f, 5.0f, 14.0f);
                }
                blastStrength *= lightness;
                sDeathRagdollNodeVel[limb][0] += blastDir[0] / blastDist * blastStrength * spread;
                sDeathRagdollNodeVel[limb][1] += blastDir[1] / blastDist * blastStrength * spread + 8.0f * lightness;
                sDeathRagdollNodeVel[limb][2] += blastDir[2] / blastDist * blastStrength * spread;
            }
        }
        sDeathRagdollNodeVel[limb][0] += (rel[1] * 0.10f + rel[2] * 0.05f) * tumble * lightness * spread;
        sDeathRagdollNodeVel[limb][1] += death_ragdoll_absf(rel[0]) * lift * lightness * spread
                                        + death_ragdoll_absf(rel[2]) * lift * 0.35f * lightness * spread;
        sDeathRagdollNodeVel[limb][2] -= rel[0] * 0.08f * tumble * lightness * spread;
        sDeathRagdollNodeVel[limb][0] += randomSide * 1.6f * lightness * spread;
        sDeathRagdollNodeVel[limb][2] -= randomSide * 1.2f * lightness * spread;
    }
}

static void death_ragdoll_reapply_entry_velocity_after_warm_start(struct MarioState *m,
                                                                  enum DeathRagdollSource source) {
    s32 limb;
    s32 bodyNode;
    f32 scale;

    if (source == DEATH_RAGDOLL_SOURCE_DEFAULT) {
        return;
    }

    scale = (source == DEATH_RAGDOLL_SOURCE_EXPLOSION) ? 0.62f : 0.44f;

    for (limb = 0; limb < DEATH_RAGDOLL_LIMB_COUNT; limb++) {
        f32 lightness = (limb == DEATH_RAGDOLL_LIMB_TORSO)
                      ? 0.80f
                      : death_ragdoll_clampf(1.0f / death_ragdoll_node_mass(limb), 0.70f, 1.20f);

        sDeathRagdollNodeVel[limb][0] += m->vel[0] * scale * lightness;
        sDeathRagdollNodeVel[limb][1] += m->vel[1] * scale * 0.34f * lightness;
        sDeathRagdollNodeVel[limb][2] += m->vel[2] * scale * lightness;
    }

    for (bodyNode = 0; bodyNode < DEATH_RAGDOLL_BODY_NODE_COUNT; bodyNode++) {
        sDeathRagdollBodyNodeVel[bodyNode][0] += m->vel[0] * scale * 0.82f;
        sDeathRagdollBodyNodeVel[bodyNode][1] += m->vel[1] * scale * 0.28f;
        sDeathRagdollBodyNodeVel[bodyNode][2] += m->vel[2] * scale * 0.82f;
    }
}

static void death_ragdoll_add_angular_velocity_to_point(Vec3f pos, Vec3f vel,
                                                        f32 omegaX, f32 omegaY, f32 omegaZ,
                                                        f32 scale) {
    Vec3f rel;

    vec3f_dif(rel, pos, sDeathRagdollCenter);
    vel[0] += (omegaY * rel[2] - omegaZ * rel[1]) * scale;
    vel[1] += (omegaZ * rel[0] - omegaX * rel[2]) * scale;
    vel[2] += (omegaX * rel[1] - omegaY * rel[0]) * scale;
}

static void death_ragdoll_add_launch_angular_velocity(struct MarioState *m, enum DeathRagdollSource source) {
    static const s32 sDriverLimbs[] = {
        DEATH_RAGDOLL_LIMB_HEAD,
        DEATH_RAGDOLL_LIMB_LEFT_FOREARM,
        DEATH_RAGDOLL_LIMB_RIGHT_FOREARM,
        DEATH_RAGDOLL_LIMB_LEFT_HAND,
        DEATH_RAGDOLL_LIMB_RIGHT_HAND,
        DEATH_RAGDOLL_LIMB_LEFT_LEG,
        DEATH_RAGDOLL_LIMB_RIGHT_LEG,
        DEATH_RAGDOLL_LIMB_LEFT_FOOT,
        DEATH_RAGDOLL_LIMB_RIGHT_FOOT,
    };
    u8 selected[sizeof(sDriverLimbs) / sizeof(sDriverLimbs[0])] = { 0 };
    s32 driverLimb;
    s32 driverIndex;
    s32 driverCount;
    s32 selectedCount;
    s32 attempts;
    f32 speed;
    f32 forwardX;
    f32 forwardZ;
    f32 sideX;
    f32 sideZ;
    f32 tumble;
    f32 yawTumble;
    f32 speedScale;
    f32 limbJitter;
    f32 yawSign;
    f32 driverForce;
    f32 driverShare;
    Vec3f rel;

    if (source == DEATH_RAGDOLL_SOURCE_DEFAULT) {
        tumble = 0.40f;
        yawTumble = 0.12f;
    } else if (source == DEATH_RAGDOLL_SOURCE_EXPLOSION) {
        tumble = 4.20f;
        yawTumble = 0.82f;
    } else {
        tumble = 7.00f;
        yawTumble = 1.05f;
    }

    speed = sqrtf(m->vel[0] * m->vel[0] + m->vel[2] * m->vel[2]);
    if (speed > 1.0f) {
        forwardX = m->vel[0] / speed;
        forwardZ = m->vel[2] / speed;
    } else {
        forwardX = sins(m->faceAngle[1]);
        forwardZ = coss(m->faceAngle[1]);
    }
    sideX = forwardZ;
    sideZ = -forwardX;
    speedScale = death_ragdoll_clampf(speed / 120.0f, 0.65f, 3.25f);
    tumble *= speedScale;
    yawTumble *= speedScale;
    tumble *= death_ragdoll_rotation_debug_scale();
    yawTumble *= death_ragdoll_rotation_debug_scale();

    driverForce = (random_u16() & 1) ? death_ragdoll_horizontal_impulse_multiplier()
                                     : death_ragdoll_upward_impulse_multiplier();
    driverCount = (s32) (driverForce * 0.5f + 0.5f);
    if (driverCount < 2) {
        driverCount = 2;
    }
    if (driverCount > (s32) (sizeof(sDriverLimbs) / sizeof(sDriverLimbs[0]))) {
        driverCount = sizeof(sDriverLimbs) / sizeof(sDriverLimbs[0]);
    }
    driverShare = 1.0f / sqrtf(driverCount);

    selectedCount = 0;
    attempts = 0;
    while (selectedCount < driverCount && attempts < driverCount * 8) {
        attempts++;
        driverIndex = random_u16() % (sizeof(sDriverLimbs) / sizeof(sDriverLimbs[0]));
        if (selected[driverIndex]) {
            continue;
        }
        selected[driverIndex] = TRUE;
        selectedCount++;
        driverLimb = sDriverLimbs[driverIndex];
        yawSign = (random_u16() & 1) ? 1.0f : -1.0f;
        limbJitter = ((random_u16() - 0x8000) / 32768.0f) * 0.35f;

        death_ragdoll_add_angular_velocity_to_point(sDeathRagdollNodePos[driverLimb],
                                                    sDeathRagdollNodeVel[driverLimb],
                                                    sideX * (tumble + limbJitter),
                                                    yawTumble * yawSign,
                                                    sideZ * (tumble + limbJitter),
                                                    2.75f * driverShare);

        vec3f_dif(rel, sDeathRagdollNodePos[driverLimb], sDeathRagdollCenter);
        sDeathRagdollNodeVel[driverLimb][0] += rel[0] * tumble * 0.020f * driverShare;
        sDeathRagdollNodeVel[driverLimb][1] += (death_ragdoll_absf(rel[0]) + death_ragdoll_absf(rel[2]))
                                             * tumble * 0.012f * driverShare;
        sDeathRagdollNodeVel[driverLimb][2] += rel[2] * tumble * 0.020f * driverShare;

        if (source != DEATH_RAGDOLL_SOURCE_DEFAULT) {
            death_ragdoll_add_chained_contact_impulse(driverLimb,
                                                      sideZ * tumble * 95.0f * driverShare,
                                                      yawTumble * yawSign * 90.0f * driverShare,
                                                      -sideX * tumble * 95.0f * driverShare);
        }
    }
}

static void death_ragdoll_apply_angular_velocity_field(struct MarioState *m) {
    s32 limb;
    s32 bodyNode;
    f32 horizontalSpeed;
    f32 scale;
    f32 rollDamp;

    death_ragdoll_clamp_angular_velocity();

    if (death_ragdoll_absf(sDeathRagdollAngularVel[0]) < 0.5f
        && death_ragdoll_absf(sDeathRagdollAngularVel[1]) < 0.5f
        && death_ragdoll_absf(sDeathRagdollAngularVel[2]) < 0.5f) {
        vec3f_set(sDeathRagdollAngularVel, 0.0f, 0.0f, 0.0f);
        return;
    }

    scale = (m->actionState == 0) ? (1.0f / 8500.0f) : (1.0f / 14000.0f);
    for (bodyNode = 0; bodyNode < DEATH_RAGDOLL_BODY_NODE_COUNT; bodyNode++) {
        death_ragdoll_add_angular_velocity_to_point(sDeathRagdollBodyNodePos[bodyNode],
                                                    sDeathRagdollBodyNodeVel[bodyNode],
                                                    sDeathRagdollAngularVel[0],
                                                    sDeathRagdollAngularVel[1],
                                                    sDeathRagdollAngularVel[2],
                                                    scale);
    }
    for (limb = 0; limb < DEATH_RAGDOLL_LIMB_COUNT; limb++) {
        death_ragdoll_add_angular_velocity_to_point(sDeathRagdollNodePos[limb],
                                                    sDeathRagdollNodeVel[limb],
                                                    sDeathRagdollAngularVel[0],
                                                    sDeathRagdollAngularVel[1],
                                                    sDeathRagdollAngularVel[2],
                                                    scale);
    }

    horizontalSpeed = sqrtf(m->vel[0] * m->vel[0] + m->vel[2] * m->vel[2]);
    if (m->actionState == 0) {
        sDeathRagdollAngularVel[0] *= 0.988f;
        sDeathRagdollAngularVel[1] *= 0.940f;
        sDeathRagdollAngularVel[2] *= 0.988f;
    } else {
        rollDamp = 0.78f + death_ragdoll_clampf(horizontalSpeed / 24.0f, 0.0f, 1.0f) * 0.16f;
        sDeathRagdollAngularVel[0] *= rollDamp;
        sDeathRagdollAngularVel[1] *= 0.64f;
        sDeathRagdollAngularVel[2] *= rollDamp;
    }
}

static void death_ragdoll_apply_node_feedback(s32 limb, Vec3f target) {
    f32 side = sDeathRagdollNodePos[limb][0] - target[0];
    f32 up = sDeathRagdollNodePos[limb][1] - target[1];
    f32 forward = sDeathRagdollNodePos[limb][2] - target[2];
    f32 scale = death_ragdoll_contact_scale(limb);
    f32 displacement = death_ragdoll_clampf((death_ragdoll_absf(up) + death_ragdoll_absf(side)
                                           + death_ragdoll_absf(forward)) / 90.0f, 0.0f, 1.0f);

    if (limb == DEATH_RAGDOLL_LIMB_TORSO) {
        return;
    }
    if (death_ragdoll_is_body_root_limb(limb)) {
        return;
    }

    death_ragdoll_add_chained_contact_impulse(limb,
                                              (up * 0.34f - forward * 0.62f) * scale * displacement,
                                              side * -0.30f * scale * displacement,
                                              side * 0.58f * scale * displacement);
}

static void death_ragdoll_limit_node_offset(s32 limb, Vec3f target) {
    Vec3f offset;
    f32 dist;
    f32 maxDist = death_ragdoll_node_max_offset(limb);
    f32 scale;
    Vec3f limitedPos;

    vec3f_dif(offset, sDeathRagdollNodePos[limb], target);
    dist = sqrtf(offset[0] * offset[0] + offset[1] * offset[1] + offset[2] * offset[2]);
    if (dist <= maxDist || dist < 1.0f) {
        return;
    }

    scale = maxDist / dist;
    limitedPos[0] = target[0] + offset[0] * scale;
    limitedPos[1] = target[1] + offset[1] * scale;
    limitedPos[2] = target[2] + offset[2] * scale;
    death_ragdoll_apply_limb_position_correction(limb,
                                                 limitedPos[0] - sDeathRagdollNodePos[limb][0],
                                                 limitedPos[1] - sDeathRagdollNodePos[limb][1],
                                                 limitedPos[2] - sDeathRagdollNodePos[limb][2]);
    sDeathRagdollNodeVel[limb][0] *= 0.72f;
    sDeathRagdollNodeVel[limb][1] *= 0.72f;
    sDeathRagdollNodeVel[limb][2] *= 0.72f;
}

static void death_ragdoll_collide_node_sample(s32 limb, Vec3f sample, f32 radius, f32 weight) {
    Vec3f support;
    struct Surface *floor;
    f32 floorHeight;
    f32 penetration;

    death_ragdoll_get_component_support_point(support, sample, radius);
    if (!death_ragdoll_collision_query_is_safe(support, radius)) {
        return;
    }
    floorHeight = find_floor(support[0], sample[1] + radius + 120.0f, support[2], &floor);
    if (floor == NULL || floor->type == SURFACE_DEATH_PLANE || support[1] >= floorHeight) {
        return;
    }

    penetration = floorHeight - support[1];
    sDeathRagdollGroundContacts++;
    if (limb == DEATH_RAGDOLL_LIMB_TORSO) {
        sDeathRagdollCenterContacts++;
    }
    if (penetration > sDeathRagdollMaxPenetration) {
        sDeathRagdollMaxPenetration = penetration;
    }
    if (-sDeathRagdollNodeVel[limb][1] > sDeathRagdollGroundImpactSpeed) {
        sDeathRagdollGroundImpactSpeed = -sDeathRagdollNodeVel[limb][1];
    }
    sDeathRagdollNodeContact[limb] = TRUE;
    death_ragdoll_apply_limb_position_correction(limb, 0.0f, penetration * weight, 0.0f);
    if (sDeathRagdollNodeVel[limb][1] < 0.0f) {
        if (sDeathRagdollSource == DEATH_RAGDOLL_SOURCE_KNOCKBACK && gMarioState != NULL
            && gMarioState->actionTimer < 42) {
            sDeathRagdollNodeVel[limb][1] *= -0.22f;
            if (death_ragdoll_absf(sDeathRagdollNodeVel[limb][1]) < 1.2f) {
                sDeathRagdollNodeVel[limb][1] = 0.0f;
            }
        } else if (limb == DEATH_RAGDOLL_LIMB_HEAD) {
            sDeathRagdollNodeVel[limb][1] = 0.0f;
        } else {
            sDeathRagdollNodeVel[limb][1] *= -0.04f;
            if (death_ragdoll_absf(sDeathRagdollNodeVel[limb][1]) < 2.0f) {
                sDeathRagdollNodeVel[limb][1] = 0.0f;
            }
        }
    }
    if (sDeathRagdollSource == DEATH_RAGDOLL_SOURCE_KNOCKBACK && gMarioState != NULL
               && gMarioState->actionTimer < 34) {
        sDeathRagdollNodeVel[limb][0] *= 0.94f;
        sDeathRagdollNodeVel[limb][2] *= 0.94f;
    } else if (limb == DEATH_RAGDOLL_LIMB_HEAD) {
        sDeathRagdollNodeVel[limb][0] *= 0.18f;
        sDeathRagdollNodeVel[limb][2] *= 0.18f;
    } else {
        sDeathRagdollNodeVel[limb][0] *= 0.34f;
        sDeathRagdollNodeVel[limb][2] *= 0.34f;
    }
    sDeathRagdollNodeHitTimer[limb] = 8;
    death_ragdoll_clamp_angular_velocity();
#if !DEATH_RAGDOLL_HARD_HUMANOID_DEBUG
    death_ragdoll_add_chained_contact_impulse(limb,
                                              penetration * 4.0f,
                                              (sample[2] - sDeathRagdollCenter[2]) * 0.42f,
                                              (sample[0] - sDeathRagdollCenter[0]) * 0.90f);

    switch (limb) {
        case DEATH_RAGDOLL_LIMB_LEFT_LEG:
        case DEATH_RAGDOLL_LIMB_RIGHT_LEG:
            sDeathRagdollLimbVel[limb][0] -= penetration * 18.0f;
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_FOOT:
        case DEATH_RAGDOLL_LIMB_RIGHT_FOOT:
            sDeathRagdollLimbVel[limb][0] += penetration * 12.0f;
            sDeathRagdollLimbVel[death_ragdoll_parent_limb(limb)][0] -= penetration * 10.0f;
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_FOREARM:
        case DEATH_RAGDOLL_LIMB_RIGHT_FOREARM:
            sDeathRagdollLimbVel[limb][0] += penetration * 12.0f;
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_HAND:
        case DEATH_RAGDOLL_LIMB_RIGHT_HAND:
            sDeathRagdollLimbVel[limb][0] += penetration * 8.0f;
            sDeathRagdollLimbVel[death_ragdoll_parent_limb(limb)][0] += penetration * 8.0f;
            break;
    }
#endif
}

static void death_ragdoll_apply_contact_to_node(s32 limb, f32 penetration, f32 weight) {
    if (weight <= 0.0f) {
        return;
    }

    sDeathRagdollNodeContact[limb] = TRUE;
    death_ragdoll_apply_limb_position_correction(limb, 0.0f, penetration * weight, 0.0f);
    if (sDeathRagdollNodeVel[limb][1] < 0.0f) {
        if (sDeathRagdollSource == DEATH_RAGDOLL_SOURCE_KNOCKBACK && gMarioState != NULL
            && gMarioState->actionTimer < 42) {
            sDeathRagdollNodeVel[limb][1] *= -0.20f;
            if (death_ragdoll_absf(sDeathRagdollNodeVel[limb][1]) < 1.1f) {
                sDeathRagdollNodeVel[limb][1] = 0.0f;
            }
        } else if (limb == DEATH_RAGDOLL_LIMB_HEAD) {
            sDeathRagdollNodeVel[limb][1] = 0.0f;
        } else {
            sDeathRagdollNodeVel[limb][1] *= -0.03f;
            if (death_ragdoll_absf(sDeathRagdollNodeVel[limb][1]) < 1.5f) {
                sDeathRagdollNodeVel[limb][1] = 0.0f;
            }
        }
    }
    if (sDeathRagdollSource == DEATH_RAGDOLL_SOURCE_KNOCKBACK && gMarioState != NULL
               && gMarioState->actionTimer < 34) {
        sDeathRagdollNodeVel[limb][0] *= 0.94f;
        sDeathRagdollNodeVel[limb][2] *= 0.94f;
    } else if (limb == DEATH_RAGDOLL_LIMB_HEAD) {
        sDeathRagdollNodeVel[limb][0] *= 0.20f;
        sDeathRagdollNodeVel[limb][2] *= 0.20f;
    } else {
        sDeathRagdollNodeVel[limb][0] *= 0.42f;
        sDeathRagdollNodeVel[limb][2] *= 0.42f;
    }
    sDeathRagdollNodeHitTimer[limb] = 5;
}

static void death_ragdoll_collide_bone_sample(s32 a, s32 b, f32 t, f32 radius) {
    Vec3f sample;
    Vec3f support;
    struct Surface *floor;
    f32 floorHeight;
    f32 penetration;
    f32 weightA;
    f32 weightB;

    sample[0] = sDeathRagdollNodePos[a][0]
              + (sDeathRagdollNodePos[b][0] - sDeathRagdollNodePos[a][0]) * t;
    sample[1] = sDeathRagdollNodePos[a][1]
              + (sDeathRagdollNodePos[b][1] - sDeathRagdollNodePos[a][1]) * t;
    sample[2] = sDeathRagdollNodePos[a][2]
              + (sDeathRagdollNodePos[b][2] - sDeathRagdollNodePos[a][2]) * t;

    death_ragdoll_get_component_support_point(support, sample, radius);
    if (!death_ragdoll_collision_query_is_safe(support, radius)) {
        return;
    }
    floorHeight = find_floor(support[0], sample[1] + radius + 120.0f, support[2], &floor);
    if (floor == NULL || floor->type == SURFACE_DEATH_PLANE || support[1] >= floorHeight) {
        return;
    }

    penetration = floorHeight - support[1];
    sDeathRagdollGroundContacts++;
    if (penetration > sDeathRagdollMaxPenetration) {
        sDeathRagdollMaxPenetration = penetration;
    }
    if (-sDeathRagdollNodeVel[a][1] > sDeathRagdollGroundImpactSpeed) {
        sDeathRagdollGroundImpactSpeed = -sDeathRagdollNodeVel[a][1];
    }
    if (-sDeathRagdollNodeVel[b][1] > sDeathRagdollGroundImpactSpeed) {
        sDeathRagdollGroundImpactSpeed = -sDeathRagdollNodeVel[b][1];
    }

    weightA = (1.0f - t) * 0.90f;
    weightB = t * 0.90f;
    death_ragdoll_apply_contact_to_node(a, penetration, weightA);
    death_ragdoll_apply_contact_to_node(b, penetration, weightB);
}

static void death_ragdoll_collide_bone_segment(s32 a, s32 b, f32 radius) {
    s32 i;

    for (i = 1; i < DEATH_RAGDOLL_BONE_COLLISION_SAMPLES; i++) {
        death_ragdoll_collide_bone_sample(a, b, i * (1.0f / DEATH_RAGDOLL_BONE_COLLISION_SAMPLES), radius);
    }
}

static void death_ragdoll_collide_node_segment(s32 limb, f32 radius) {
    death_ragdoll_collide_node_sample(limb, sDeathRagdollNodePos[limb], radius, 1.0f);
}

static void death_ragdoll_apply_contact_to_body_node(s32 bodyNode, f32 penetration, f32 weight) {
    if (weight <= 0.0f) {
        return;
    }

    sDeathRagdollBodyNodeContact[bodyNode] = TRUE;
    death_ragdoll_apply_body_node_position_correction(bodyNode, 0.0f, penetration * weight, 0.0f);
    if (sDeathRagdollBodyNodeVel[bodyNode][1] < 0.0f) {
        if (sDeathRagdollSource == DEATH_RAGDOLL_SOURCE_KNOCKBACK && gMarioState != NULL
            && gMarioState->actionTimer < 42) {
            sDeathRagdollBodyNodeVel[bodyNode][1] *= -0.18f;
        } else {
            sDeathRagdollBodyNodeVel[bodyNode][1] *= -0.04f;
        }
        if (death_ragdoll_absf(sDeathRagdollBodyNodeVel[bodyNode][1]) < 1.5f) {
            sDeathRagdollBodyNodeVel[bodyNode][1] = 0.0f;
        }
    }
    if (sDeathRagdollSource == DEATH_RAGDOLL_SOURCE_KNOCKBACK && gMarioState != NULL
        && gMarioState->actionTimer < 34) {
        sDeathRagdollBodyNodeVel[bodyNode][0] *= 0.94f;
        sDeathRagdollBodyNodeVel[bodyNode][2] *= 0.94f;
    } else {
        sDeathRagdollBodyNodeVel[bodyNode][0] *= 0.40f;
        sDeathRagdollBodyNodeVel[bodyNode][2] *= 0.40f;
    }
    sDeathRagdollCenterContacts++;
}

static void death_ragdoll_collide_body_node(s32 bodyNode) {
    Vec3f support;
    struct Surface *floor;
    f32 radius = death_ragdoll_body_node_radius(bodyNode);
    f32 floorHeight;
    f32 penetration;

    death_ragdoll_get_component_support_point(support, sDeathRagdollBodyNodePos[bodyNode], radius);
    if (!death_ragdoll_collision_query_is_safe(support, radius)) {
        return;
    }
    floorHeight = find_floor(support[0], sDeathRagdollBodyNodePos[bodyNode][1] + radius + 120.0f,
                             support[2], &floor);
    if (floor == NULL || floor->type == SURFACE_DEATH_PLANE || support[1] >= floorHeight) {
        return;
    }

    penetration = floorHeight - support[1];
    sDeathRagdollGroundContacts++;
    if (penetration > sDeathRagdollMaxPenetration) {
        sDeathRagdollMaxPenetration = penetration;
    }
    if (-sDeathRagdollBodyNodeVel[bodyNode][1] > sDeathRagdollGroundImpactSpeed) {
        sDeathRagdollGroundImpactSpeed = -sDeathRagdollBodyNodeVel[bodyNode][1];
    }
    death_ragdoll_apply_contact_to_body_node(bodyNode, penetration, 1.0f);
}

static void death_ragdoll_collide_volume_segment(s32 a, s32 b, f32 radiusScale) {
    f32 radius = min(death_ragdoll_node_radius(a), death_ragdoll_node_radius(b));

    death_ragdoll_collide_bone_segment(a, b, death_ragdoll_segment_radius(radius) * radiusScale);
}

static void death_ragdoll_collide_full_volume(void) {
    death_ragdoll_collide_volume_segment(DEATH_RAGDOLL_LIMB_TORSO,
                                         DEATH_RAGDOLL_LIMB_HEAD, 1.10f);
    death_ragdoll_collide_volume_segment(DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM,
                                         DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM, 0.88f);
    death_ragdoll_collide_volume_segment(DEATH_RAGDOLL_LIMB_LEFT_THIGH,
                                         DEATH_RAGDOLL_LIMB_RIGHT_THIGH, 0.92f);
    death_ragdoll_collide_volume_segment(DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM,
                                         DEATH_RAGDOLL_LIMB_LEFT_FOREARM, 1.05f);
    death_ragdoll_collide_volume_segment(DEATH_RAGDOLL_LIMB_LEFT_FOREARM,
                                         DEATH_RAGDOLL_LIMB_LEFT_HAND, 1.05f);
    death_ragdoll_collide_volume_segment(DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM,
                                         DEATH_RAGDOLL_LIMB_RIGHT_FOREARM, 1.05f);
    death_ragdoll_collide_volume_segment(DEATH_RAGDOLL_LIMB_RIGHT_FOREARM,
                                         DEATH_RAGDOLL_LIMB_RIGHT_HAND, 1.05f);
    death_ragdoll_collide_volume_segment(DEATH_RAGDOLL_LIMB_LEFT_THIGH,
                                         DEATH_RAGDOLL_LIMB_LEFT_LEG, 1.10f);
    death_ragdoll_collide_volume_segment(DEATH_RAGDOLL_LIMB_LEFT_LEG,
                                         DEATH_RAGDOLL_LIMB_LEFT_FOOT, 1.10f);
    death_ragdoll_collide_volume_segment(DEATH_RAGDOLL_LIMB_RIGHT_THIGH,
                                         DEATH_RAGDOLL_LIMB_RIGHT_LEG, 1.10f);
    death_ragdoll_collide_volume_segment(DEATH_RAGDOLL_LIMB_RIGHT_LEG,
                                         DEATH_RAGDOLL_LIMB_RIGHT_FOOT, 1.10f);
}

static void death_ragdoll_collide_node_wall(UNUSED s32 limb, UNUSED f32 radius) {
#if DEATH_RAGDOLL_ENABLE_WALL_RESOLVER
    Vec3f resolvedPos;
    struct Surface *wall;

    vec3f_copy(resolvedPos, sDeathRagdollNodePos[limb]);
    if (!death_ragdoll_collision_query_is_safe(resolvedPos, radius)) {
        return;
    }
    wall = resolve_and_return_wall_collisions(resolvedPos, 0.0f, radius);
    if (wall == NULL) {
        return;
    }

    death_ragdoll_apply_limb_position_correction(limb,
                                                 resolvedPos[0] - sDeathRagdollNodePos[limb][0],
                                                 0.0f,
                                                 resolvedPos[2] - sDeathRagdollNodePos[limb][2]);
    {
        f32 intoWall = sDeathRagdollNodeVel[limb][0] * wall->normal.x
                     + sDeathRagdollNodeVel[limb][2] * wall->normal.z;

        if (intoWall < 0.0f) {
            sDeathRagdollNodeVel[limb][0] -= wall->normal.x * intoWall * 1.18f;
            sDeathRagdollNodeVel[limb][2] -= wall->normal.z * intoWall * 1.18f;
        }
        sDeathRagdollNodeVel[limb][0] *= 0.72f;
        sDeathRagdollNodeVel[limb][2] *= 0.72f;
    }
    sDeathRagdollNodeHitTimer[limb] = 8;
#if !DEATH_RAGDOLL_HARD_HUMANOID_DEBUG
    death_ragdoll_add_chained_contact_impulse(limb, 0.0f,
                                              (random_u16() & 1) ? 360.0f : -360.0f,
                                              0.0f);
#endif
#endif
}

static void death_ragdoll_solve_joint_constraint(s32 limb) {
    s32 parent = death_ragdoll_parent_limb(limb);
    Vec3f delta;
    f32 dist;
    f32 targetDist;
    f32 invParent;
    f32 invChild;
    f32 invTotal;
    f32 correction;
    f32 parentShare;
    f32 childShare;

    if (limb == DEATH_RAGDOLL_LIMB_TORSO) {
        return;
    }

    vec3f_dif(delta, sDeathRagdollNodePos[limb], sDeathRagdollNodePos[parent]);
    dist = sqrtf(delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2]);
    if (dist < 1.0f) {
        return;
    }

    targetDist = death_ragdoll_constraint_length(limb);
    invParent = 1.0f / death_ragdoll_node_mass(parent);
    invChild = 1.0f / death_ragdoll_node_mass(limb);
    invTotal = invParent + invChild;
    correction = (dist - targetDist) / dist * 0.72f;
    if (parent == DEATH_RAGDOLL_LIMB_TORSO) {
        parentShare = 0.0f;
        childShare = 1.0f;
    } else {
        parentShare = invParent / invTotal;
        childShare = invChild / invTotal;
    }

    death_ragdoll_apply_limb_position_correction(parent,
                                                 delta[0] * correction * parentShare,
                                                 delta[1] * correction * parentShare,
                                                 delta[2] * correction * parentShare);
    death_ragdoll_apply_limb_position_correction(limb,
                                                 -delta[0] * correction * childShare,
                                                 -delta[1] * correction * childShare,
                                                 -delta[2] * correction * childShare);
}

static void death_ragdoll_solve_distance_pair(s32 a, s32 b, f32 targetDist, f32 stiffness) {
    Vec3f delta;
    f32 dist;
    f32 invA;
    f32 invB;
    f32 invTotal;
    f32 correction;

    vec3f_dif(delta, sDeathRagdollNodePos[b], sDeathRagdollNodePos[a]);
    dist = sqrtf(delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2]);
    if (dist < 0.5f) {
        return;
    }

    invA = 1.0f / death_ragdoll_node_mass(a);
    invB = 1.0f / death_ragdoll_node_mass(b);
    invTotal = invA + invB;
    correction = (dist - targetDist) / dist * stiffness;

    death_ragdoll_apply_limb_position_correction(a,
                                                 delta[0] * correction * (invA / invTotal),
                                                 delta[1] * correction * (invA / invTotal),
                                                 delta[2] * correction * (invA / invTotal));
    death_ragdoll_apply_limb_position_correction(b,
                                                 -delta[0] * correction * (invB / invTotal),
                                                 -delta[1] * correction * (invB / invTotal),
                                                 -delta[2] * correction * (invB / invTotal));
}

static void death_ragdoll_solve_body_node_distance(s32 a, s32 b, f32 targetDist, f32 stiffness) {
    Vec3f delta;
    f32 dist;
    f32 invA;
    f32 invB;
    f32 invTotal;
    f32 correction;

    vec3f_dif(delta, sDeathRagdollBodyNodePos[b], sDeathRagdollBodyNodePos[a]);
    dist = sqrtf(delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2]);
    if (dist < 0.5f) {
        return;
    }

    invA = 1.0f / death_ragdoll_body_node_mass(a);
    invB = 1.0f / death_ragdoll_body_node_mass(b);
    invTotal = invA + invB;
    correction = (dist - targetDist) / dist * stiffness;

    death_ragdoll_apply_body_node_position_correction(a,
                                                      delta[0] * correction * (invA / invTotal),
                                                      delta[1] * correction * (invA / invTotal),
                                                      delta[2] * correction * (invA / invTotal));
    death_ragdoll_apply_body_node_position_correction(b,
                                                      -delta[0] * correction * (invB / invTotal),
                                                      -delta[1] * correction * (invB / invTotal),
                                                      -delta[2] * correction * (invB / invTotal));
}

static u8 death_ragdoll_airborne_limb_feedback_enabled(void) {
    if (gMarioState == NULL || gMarioState->actionState != 0 || sDeathRagdollGroundContacts > 0) {
        return FALSE;
    }

    return sDeathRagdollSource == DEATH_RAGDOLL_SOURCE_KNOCKBACK
        || sDeathRagdollSource == DEATH_RAGDOLL_SOURCE_EXPLOSION;
}

static void death_ragdoll_solve_body_node_to_limb(s32 bodyNode, s32 limb, f32 targetDist, f32 stiffness) {
    Vec3f delta;
    f32 dist;
    f32 correction;
    f32 limbShare = 1.0f;
    f32 bodyShare = 0.0f;

    vec3f_dif(delta, sDeathRagdollNodePos[limb], sDeathRagdollBodyNodePos[bodyNode]);
    dist = sqrtf(delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2]);
    if (dist < 0.5f) {
        return;
    }

    correction = (dist - targetDist) / dist * stiffness;

    if (death_ragdoll_airborne_limb_feedback_enabled()) {
        limbShare = 0.62f;
        bodyShare = 0.38f;
    }

    death_ragdoll_apply_limb_position_correction(limb,
                                                 -delta[0] * correction * limbShare,
                                                 -delta[1] * correction * limbShare,
                                                 -delta[2] * correction * limbShare);
    death_ragdoll_apply_body_node_position_correction(bodyNode,
                                                      delta[0] * correction * bodyShare,
                                                      delta[1] * correction * bodyShare,
                                                      delta[2] * correction * bodyShare);
}

static void death_ragdoll_sync_torso_limb_to_body_nodes(void) {
    sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_TORSO][0] =
        (sDeathRagdollBodyNodePos[DEATH_RAGDOLL_BODY_NODE_CHEST][0]
       + sDeathRagdollBodyNodePos[DEATH_RAGDOLL_BODY_NODE_PELVIS][0]
       + sDeathRagdollBodyNodePos[DEATH_RAGDOLL_BODY_NODE_BACK][0]) / 3.0f;
    sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_TORSO][1] =
        (sDeathRagdollBodyNodePos[DEATH_RAGDOLL_BODY_NODE_CHEST][1]
       + sDeathRagdollBodyNodePos[DEATH_RAGDOLL_BODY_NODE_PELVIS][1]
       + sDeathRagdollBodyNodePos[DEATH_RAGDOLL_BODY_NODE_BACK][1]) / 3.0f;
    sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_TORSO][2] =
        (sDeathRagdollBodyNodePos[DEATH_RAGDOLL_BODY_NODE_CHEST][2]
       + sDeathRagdollBodyNodePos[DEATH_RAGDOLL_BODY_NODE_PELVIS][2]
       + sDeathRagdollBodyNodePos[DEATH_RAGDOLL_BODY_NODE_BACK][2]) / 3.0f;
}

static void death_ragdoll_apply_limb_position_correction(s32 limb, f32 x, f32 y, f32 z) {
    sDeathRagdollNodePos[limb][0] += x;
    sDeathRagdollNodePos[limb][1] += y;
    sDeathRagdollNodePos[limb][2] += z;
    if (sDeathRagdollGroundContacts > 0) {
        sDeathRagdollNodePrevPos[limb][0] += x;
        sDeathRagdollNodePrevPos[limb][1] += y;
        sDeathRagdollNodePrevPos[limb][2] += z;
    }
}

static void death_ragdoll_apply_body_node_position_correction(s32 bodyNode, f32 x, f32 y, f32 z) {
    sDeathRagdollBodyNodePos[bodyNode][0] += x;
    sDeathRagdollBodyNodePos[bodyNode][1] += y;
    sDeathRagdollBodyNodePos[bodyNode][2] += z;
    if (sDeathRagdollGroundContacts > 0) {
        sDeathRagdollBodyNodePrevPos[bodyNode][0] += x;
        sDeathRagdollBodyNodePrevPos[bodyNode][1] += y;
        sDeathRagdollBodyNodePrevPos[bodyNode][2] += z;
    }
}

static void death_ragdoll_get_upper_body_world_axes(Vec3f xAxis, Vec3f yAxis, Vec3f zAxis) {
    Vec3f backAxis;

    vec3f_dif(xAxis,
              sDeathRagdollBodyNodePos[DEATH_RAGDOLL_BODY_NODE_CHEST],
              sDeathRagdollBodyNodePos[DEATH_RAGDOLL_BODY_NODE_PELVIS]);
    if (sqrtf(xAxis[0] * xAxis[0] + xAxis[1] * xAxis[1] + xAxis[2] * xAxis[2]) < 1.0f) {
        vec3f_set(xAxis, 1.0f, 0.0f, 0.0f);
    }
    vec3f_normalize(xAxis);

    vec3f_dif(backAxis,
              sDeathRagdollBodyNodePos[DEATH_RAGDOLL_BODY_NODE_BACK],
              sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_TORSO]);
    if (sqrtf(backAxis[0] * backAxis[0] + backAxis[1] * backAxis[1] + backAxis[2] * backAxis[2]) < 1.0f) {
        vec3f_set(backAxis, 0.0f, 0.0f, 1.0f);
    }
    vec3f_normalize(backAxis);

    vec3f_cross(yAxis, backAxis, xAxis);
    if (sqrtf(yAxis[0] * yAxis[0] + yAxis[1] * yAxis[1] + yAxis[2] * yAxis[2]) < 0.1f) {
        vec3f_set(yAxis, 0.0f, 1.0f, 0.0f);
    }
    vec3f_normalize(yAxis);
    vec3f_cross(zAxis, xAxis, yAxis);
    vec3f_normalize(zAxis);
}

static void death_ragdoll_solve_body_anchor(s32 limb, s32 bodyNode, f32 localX, f32 localY,
                                            f32 localZ, f32 slack, f32 stiffness) {
    Vec3f xAxis;
    Vec3f yAxis;
    Vec3f zAxis;
    Vec3f target;
    Vec3f delta;
    f32 dist;
    f32 correction;
    f32 limbShare = 1.0f;
    f32 bodyShare = 0.0f;

    death_ragdoll_get_upper_body_world_axes(xAxis, yAxis, zAxis);
    target[0] = sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_TORSO][0]
              + xAxis[0] * localX + yAxis[0] * localY + zAxis[0] * localZ;
    target[1] = sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_TORSO][1]
              + xAxis[1] * localX + yAxis[1] * localY + zAxis[1] * localZ;
    target[2] = sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_TORSO][2]
              + xAxis[2] * localX + yAxis[2] * localY + zAxis[2] * localZ;

    vec3f_dif(delta, sDeathRagdollNodePos[limb], target);
    dist = sqrtf(delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2]);
    if (dist <= slack || dist < 0.5f) {
        return;
    }

    correction = (dist - slack) / dist * stiffness;
    if (death_ragdoll_airborne_limb_feedback_enabled()) {
        limbShare = 0.68f;
        bodyShare = 0.32f;
    }
    death_ragdoll_apply_limb_position_correction(limb,
                                                 -delta[0] * correction * limbShare,
                                                 -delta[1] * correction * limbShare,
                                                 -delta[2] * correction * limbShare);
    death_ragdoll_apply_body_node_position_correction(bodyNode,
                                                      delta[0] * correction * bodyShare,
                                                      delta[1] * correction * bodyShare,
                                                      delta[2] * correction * bodyShare);
}

static void death_ragdoll_solve_min_distance_pair(s32 a, s32 b, f32 minDist, f32 stiffness) {
    Vec3f delta;
    f32 dist;
    f32 invA;
    f32 invB;
    f32 invTotal;
    f32 correction;

    vec3f_dif(delta, sDeathRagdollNodePos[b], sDeathRagdollNodePos[a]);
    dist = sqrtf(delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2]);
    if (dist >= minDist || dist < 0.5f) {
        return;
    }

    invA = 1.0f / death_ragdoll_node_mass(a);
    invB = 1.0f / death_ragdoll_node_mass(b);
    invTotal = invA + invB;
    correction = (minDist - dist) / dist * stiffness;

    death_ragdoll_apply_limb_position_correction(a,
                                                 -delta[0] * correction * (invA / invTotal),
                                                 -delta[1] * correction * (invA / invTotal),
                                                 -delta[2] * correction * (invA / invTotal));
    death_ragdoll_apply_limb_position_correction(b,
                                                 delta[0] * correction * (invB / invTotal),
                                                 delta[1] * correction * (invB / invTotal),
                                                 delta[2] * correction * (invB / invTotal));
}

static f32 death_ragdoll_shape_stiffness(f32 stiffness) {
#if DEATH_RAGDOLL_HARD_HUMANOID_DEBUG
    return min(stiffness * 1.05f, 0.72f);
#else
    if (sDeathRagdollLooseTimer <= 0) {
        return stiffness;
    }

    if (sDeathRagdollSource == DEATH_RAGDOLL_SOURCE_EXPLOSION) {
        return stiffness * 0.78f;
    }

    return stiffness * 0.86f;
#endif
}

static void death_ragdoll_get_humanoid_slot(s32 limb, Vec3f slot, f32 *slack, f32 *stiffness) {
    switch (limb) {
        case DEATH_RAGDOLL_LIMB_HEAD:
            vec3f_set(slot, 0.0f, 82.0f, -4.0f);
            *slack = 8.0f;
            *stiffness = 0.72f;
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM:
            vec3f_set(slot, 82.0f, 16.0f, 12.0f);
            *slack = 8.0f;
            *stiffness = 0.76f;
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_FOREARM:
            vec3f_set(slot, 148.0f, 0.0f, 10.0f);
            *slack = 12.0f;
            *stiffness = 0.66f;
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_HAND:
            vec3f_set(slot, 178.0f, -8.0f, 10.0f);
            *slack = 16.0f;
            *stiffness = 0.56f;
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM:
            vec3f_set(slot, -82.0f, 16.0f, 12.0f);
            *slack = 8.0f;
            *stiffness = 0.76f;
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_FOREARM:
            vec3f_set(slot, -148.0f, 0.0f, 10.0f);
            *slack = 12.0f;
            *stiffness = 0.66f;
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_HAND:
            vec3f_set(slot, -178.0f, -8.0f, 10.0f);
            *slack = 16.0f;
            *stiffness = 0.56f;
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_THIGH:
            vec3f_set(slot, 24.0f, -56.0f, 8.0f);
            *slack = 11.0f;
            *stiffness = 0.70f;
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_LEG:
            vec3f_set(slot, 36.0f, -118.0f, 8.0f);
            *slack = 17.0f;
            *stiffness = 0.58f;
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_FOOT:
            vec3f_set(slot, 54.0f, -144.0f, 26.0f);
            *slack = 22.0f;
            *stiffness = 0.48f;
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_THIGH:
            vec3f_set(slot, -24.0f, -56.0f, 8.0f);
            *slack = 12.0f;
            *stiffness = 0.70f;
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_LEG:
            vec3f_set(slot, -36.0f, -118.0f, 8.0f);
            *slack = 18.0f;
            *stiffness = 0.58f;
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_FOOT:
            vec3f_set(slot, -54.0f, -144.0f, 26.0f);
            *slack = 23.0f;
            *stiffness = 0.48f;
            break;
        default:
            vec3f_set(slot, 0.0f, 0.0f, 0.0f);
            *slack = 0.0f;
            *stiffness = 0.0f;
            break;
    }

    slot[0] *= DEATH_RAGDOLL_VISUAL_SCALE;
    slot[1] *= DEATH_RAGDOLL_VISUAL_SCALE;
    slot[2] *= DEATH_RAGDOLL_VISUAL_SCALE;
    *slack *= DEATH_RAGDOLL_VISUAL_SCALE;
#if DEATH_RAGDOLL_HARD_HUMANOID_DEBUG
    *slack *= 0.38f;
    *stiffness = min(*stiffness * 1.45f, 0.98f);
#endif
    *stiffness = death_ragdoll_shape_stiffness(*stiffness);
}

static void death_ragdoll_solve_humanoid_slot(struct MarioState *m, s32 limb) {
    Vec3f slot;
    Vec3f targetOffset;
    Vec3f target;
    Vec3f delta;
    Vec3s bodyAngle;
    f32 slack;
    f32 stiffness;
    f32 dist;
    f32 correction;

#if DEATH_RAGDOLL_HARD_HUMANOID_DEBUG
    return;
#endif

    if (limb == DEATH_RAGDOLL_LIMB_TORSO) {
        return;
    }

    death_ragdoll_get_humanoid_slot(limb, slot, &slack, &stiffness);
    bodyAngle[0] = (s16) sDeathRagdollLimbAngle[DEATH_RAGDOLL_LIMB_TORSO][0];
    bodyAngle[1] = m->marioObj->header.gfx.angle[1] + (s16) sDeathRagdollLimbAngle[DEATH_RAGDOLL_LIMB_TORSO][1];
    bodyAngle[2] = (s16) sDeathRagdollLimbAngle[DEATH_RAGDOLL_LIMB_TORSO][2];
    death_ragdoll_rotate_local_offset(targetOffset, slot, bodyAngle);

    target[0] = sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_TORSO][0] + targetOffset[0];
    target[1] = sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_TORSO][1] + targetOffset[1];
    target[2] = sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_TORSO][2] + targetOffset[2];

    vec3f_dif(delta, sDeathRagdollNodePos[limb], target);
    dist = sqrtf(delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2]);
    if (dist <= slack || dist < 0.5f) {
        return;
    }

    correction = (dist - slack) / dist * stiffness;
    death_ragdoll_apply_limb_position_correction(limb,
                                                 -delta[0] * correction,
                                                 -delta[1] * correction,
                                                 -delta[2] * correction);

#if !DEATH_RAGDOLL_HARD_HUMANOID_DEBUG
    sDeathRagdollNodeVel[limb][0] -= delta[0] * correction * 0.08f;
    sDeathRagdollNodeVel[limb][1] -= delta[1] * correction * 0.08f;
    sDeathRagdollNodeVel[limb][2] -= delta[2] * correction * 0.08f;
#endif
}

static void death_ragdoll_solve_chain_midpoint(s32 root, s32 mid, s32 end, f32 t, f32 stiffness) {
    Vec3f target;
    Vec3f correction;
    f32 invRoot;
    f32 invMid;
    f32 invEnd;
    f32 invTotal;

    target[0] = sDeathRagdollNodePos[root][0]
              + (sDeathRagdollNodePos[end][0] - sDeathRagdollNodePos[root][0]) * t;
    target[1] = sDeathRagdollNodePos[root][1]
              + (sDeathRagdollNodePos[end][1] - sDeathRagdollNodePos[root][1]) * t;
    target[2] = sDeathRagdollNodePos[root][2]
              + (sDeathRagdollNodePos[end][2] - sDeathRagdollNodePos[root][2]) * t;

    vec3f_dif(correction, sDeathRagdollNodePos[mid], target);
    invRoot = 1.0f / death_ragdoll_node_mass(root);
    invMid = 1.0f / death_ragdoll_node_mass(mid);
    invEnd = 1.0f / death_ragdoll_node_mass(end);
    invTotal = invRoot + invMid + invEnd;

    death_ragdoll_apply_limb_position_correction(mid,
                                                 -correction[0] * stiffness * (invMid / invTotal) * 2.0f,
                                                 -correction[1] * stiffness * (invMid / invTotal) * 2.0f,
                                                 -correction[2] * stiffness * (invMid / invTotal) * 2.0f);
    death_ragdoll_apply_limb_position_correction(root,
                                                 correction[0] * stiffness * (invRoot / invTotal) * 0.45f,
                                                 correction[1] * stiffness * (invRoot / invTotal) * 0.45f,
                                                 correction[2] * stiffness * (invRoot / invTotal) * 0.45f);
    death_ragdoll_apply_limb_position_correction(end,
                                                 correction[0] * stiffness * (invEnd / invTotal) * 0.45f,
                                                 correction[1] * stiffness * (invEnd / invTotal) * 0.45f,
                                                 correction[2] * stiffness * (invEnd / invTotal) * 0.45f);
}

static void death_ragdoll_solve_short_limb_constraints(void) {
#define RG_DIST(units) ((units) * DEATH_RAGDOLL_VISUAL_SCALE)
    death_ragdoll_solve_distance_pair(DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM,
                                      DEATH_RAGDOLL_LIMB_LEFT_HAND,
                                      RG_DIST(122.0f), 0.52f);
    death_ragdoll_solve_distance_pair(DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM,
                                      DEATH_RAGDOLL_LIMB_RIGHT_HAND,
                                      RG_DIST(122.0f), 0.52f);
    death_ragdoll_solve_distance_pair(DEATH_RAGDOLL_LIMB_LEFT_THIGH,
                                      DEATH_RAGDOLL_LIMB_LEFT_FOOT,
                                      RG_DIST(132.0f), 0.42f);
    death_ragdoll_solve_distance_pair(DEATH_RAGDOLL_LIMB_RIGHT_THIGH,
                                      DEATH_RAGDOLL_LIMB_RIGHT_FOOT,
                                      RG_DIST(132.0f), 0.42f);

    death_ragdoll_solve_chain_midpoint(DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM,
                                       DEATH_RAGDOLL_LIMB_LEFT_FOREARM,
                                       DEATH_RAGDOLL_LIMB_LEFT_HAND,
                                       0.52f, 0.66f);
    death_ragdoll_solve_chain_midpoint(DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM,
                                       DEATH_RAGDOLL_LIMB_RIGHT_FOREARM,
                                       DEATH_RAGDOLL_LIMB_RIGHT_HAND,
                                       0.52f, 0.66f);
    death_ragdoll_solve_chain_midpoint(DEATH_RAGDOLL_LIMB_LEFT_THIGH,
                                       DEATH_RAGDOLL_LIMB_LEFT_LEG,
                                       DEATH_RAGDOLL_LIMB_LEFT_FOOT,
                                       0.52f, 0.54f);
    death_ragdoll_solve_chain_midpoint(DEATH_RAGDOLL_LIMB_RIGHT_THIGH,
                                       DEATH_RAGDOLL_LIMB_RIGHT_LEG,
                                       DEATH_RAGDOLL_LIMB_RIGHT_FOOT,
                                       0.52f, 0.54f);
#undef RG_DIST
}

static void death_ragdoll_solve_torso_shape_constraints(void) {
#define RG_DIST(units) ((units) * DEATH_RAGDOLL_VISUAL_SCALE)
#define RG_STIFF(value) death_ragdoll_shape_stiffness(value)
    death_ragdoll_solve_distance_pair(DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM,
                                      DEATH_RAGDOLL_LIMB_LEFT_THIGH,
                                      RG_DIST(85.0f), RG_STIFF(0.18f));
    death_ragdoll_solve_distance_pair(DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM,
                                      DEATH_RAGDOLL_LIMB_RIGHT_THIGH,
                                      RG_DIST(85.0f), RG_STIFF(0.18f));
    death_ragdoll_solve_min_distance_pair(DEATH_RAGDOLL_LIMB_HEAD,
                                          DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM,
                                          RG_DIST(76.0f), RG_STIFF(0.12f));
    death_ragdoll_solve_min_distance_pair(DEATH_RAGDOLL_LIMB_HEAD,
                                          DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM,
                                          RG_DIST(76.0f), RG_STIFF(0.12f));
    death_ragdoll_solve_distance_pair(DEATH_RAGDOLL_LIMB_HEAD,
                                      DEATH_RAGDOLL_LIMB_TORSO,
                                      RG_DIST(82.0f), RG_STIFF(0.30f));
    if (gMarioState != NULL && sDeathRagdollGroundContacts > 0 && gMarioState->actionTimer > 30) {
        return;
    }

    death_ragdoll_solve_min_distance_pair(DEATH_RAGDOLL_LIMB_LEFT_HAND,
                                          DEATH_RAGDOLL_LIMB_LEFT_THIGH,
                                          RG_DIST(72.0f), RG_STIFF(0.06f));
    death_ragdoll_solve_min_distance_pair(DEATH_RAGDOLL_LIMB_RIGHT_HAND,
                                          DEATH_RAGDOLL_LIMB_RIGHT_THIGH,
                                          RG_DIST(72.0f), RG_STIFF(0.06f));
    death_ragdoll_solve_min_distance_pair(DEATH_RAGDOLL_LIMB_LEFT_FOOT,
                                          DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM,
                                          RG_DIST(88.0f), RG_STIFF(0.06f));
    death_ragdoll_solve_min_distance_pair(DEATH_RAGDOLL_LIMB_RIGHT_FOOT,
                                          DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM,
                                          RG_DIST(88.0f), RG_STIFF(0.06f));
    death_ragdoll_solve_min_distance_pair(DEATH_RAGDOLL_LIMB_HEAD,
                                          DEATH_RAGDOLL_LIMB_LEFT_FOOT,
                                          RG_DIST(98.0f), RG_STIFF(0.08f));
    death_ragdoll_solve_min_distance_pair(DEATH_RAGDOLL_LIMB_HEAD,
                                          DEATH_RAGDOLL_LIMB_RIGHT_FOOT,
                                          RG_DIST(98.0f), RG_STIFF(0.08f));
    death_ragdoll_solve_min_distance_pair(DEATH_RAGDOLL_LIMB_HEAD,
                                          DEATH_RAGDOLL_LIMB_LEFT_HAND,
                                          RG_DIST(62.0f), RG_STIFF(0.06f));
    death_ragdoll_solve_min_distance_pair(DEATH_RAGDOLL_LIMB_HEAD,
                                          DEATH_RAGDOLL_LIMB_RIGHT_HAND,
                                          RG_DIST(62.0f), RG_STIFF(0.06f));
#undef RG_STIFF
#undef RG_DIST
}

static void death_ragdoll_solve_body_anchor_constraints(void) {
#define RG_DIST(units) ((units) * DEATH_RAGDOLL_VISUAL_SCALE)
    death_ragdoll_solve_body_anchor(DEATH_RAGDOLL_LIMB_HEAD,
                                    DEATH_RAGDOLL_BODY_NODE_CHEST,
                                    RG_DIST(64.0f), RG_DIST(0.0f), RG_DIST(-4.0f),
                                    RG_DIST(14.0f), 0.42f);
    death_ragdoll_solve_body_anchor(DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM,
                                    DEATH_RAGDOLL_BODY_NODE_CHEST,
                                    RG_DIST(28.0f), RG_DIST(56.0f), RG_DIST(8.0f),
                                    RG_DIST(9.0f), 0.62f);
    death_ragdoll_solve_body_anchor(DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM,
                                    DEATH_RAGDOLL_BODY_NODE_CHEST,
                                    RG_DIST(28.0f), RG_DIST(-56.0f), RG_DIST(8.0f),
                                    RG_DIST(9.0f), 0.62f);
    death_ragdoll_solve_body_anchor(DEATH_RAGDOLL_LIMB_LEFT_THIGH,
                                    DEATH_RAGDOLL_BODY_NODE_PELVIS,
                                    RG_DIST(-34.0f), RG_DIST(16.0f), RG_DIST(0.0f),
                                    RG_DIST(10.0f), 0.56f);
    death_ragdoll_solve_body_anchor(DEATH_RAGDOLL_LIMB_RIGHT_THIGH,
                                    DEATH_RAGDOLL_BODY_NODE_PELVIS,
                                    RG_DIST(-34.0f), RG_DIST(-16.0f), RG_DIST(0.0f),
                                    RG_DIST(10.0f), 0.56f);
#undef RG_DIST
}

static void death_ragdoll_solve_body_tripod_constraints(void) {
#define RG_DIST(units) ((units) * DEATH_RAGDOLL_VISUAL_SCALE)
    death_ragdoll_solve_body_node_distance(DEATH_RAGDOLL_BODY_NODE_CHEST,
                                           DEATH_RAGDOLL_BODY_NODE_PELVIS,
                                           RG_DIST(78.0f), 0.72f);
    death_ragdoll_solve_body_node_distance(DEATH_RAGDOLL_BODY_NODE_CHEST,
                                           DEATH_RAGDOLL_BODY_NODE_BACK,
                                           RG_DIST(54.0f), 0.66f);
    death_ragdoll_solve_body_node_distance(DEATH_RAGDOLL_BODY_NODE_PELVIS,
                                           DEATH_RAGDOLL_BODY_NODE_BACK,
                                           RG_DIST(62.0f), 0.66f);

    death_ragdoll_solve_body_node_to_limb(DEATH_RAGDOLL_BODY_NODE_CHEST,
                                          DEATH_RAGDOLL_LIMB_HEAD,
                                          RG_DIST(56.0f), 0.40f);
    death_ragdoll_solve_body_node_to_limb(DEATH_RAGDOLL_BODY_NODE_CHEST,
                                          DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM,
                                          RG_DIST(76.0f), 0.18f);
    death_ragdoll_solve_body_node_to_limb(DEATH_RAGDOLL_BODY_NODE_CHEST,
                                          DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM,
                                          RG_DIST(76.0f), 0.18f);
    death_ragdoll_solve_body_node_to_limb(DEATH_RAGDOLL_BODY_NODE_PELVIS,
                                          DEATH_RAGDOLL_LIMB_LEFT_THIGH,
                                          RG_DIST(42.0f), 0.18f);
    death_ragdoll_solve_body_node_to_limb(DEATH_RAGDOLL_BODY_NODE_PELVIS,
                                          DEATH_RAGDOLL_LIMB_RIGHT_THIGH,
                                          RG_DIST(42.0f), 0.18f);
    death_ragdoll_solve_body_node_to_limb(DEATH_RAGDOLL_BODY_NODE_BACK,
                                          DEATH_RAGDOLL_LIMB_TORSO,
                                          RG_DIST(34.0f), 0.44f);
    death_ragdoll_sync_torso_limb_to_body_nodes();
#undef RG_DIST
}

static void death_ragdoll_push_limb_out_of_torso(s32 limb, f32 minDist, f32 stiffness) {
    Vec3f delta;
    Vec3f sideAnchor;
    s16 sideSign;
    f32 dist;
    f32 correction;
    f32 settleScale;

    vec3f_dif(delta, sDeathRagdollNodePos[limb], sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_TORSO]);
    sideSign = death_ragdoll_limb_side_sign(limb);
    if (sideSign != 0) {
        sideAnchor[0] = sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_TORSO][0]
                      + (sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM][0]
                       - sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM][0]) * 0.18f * sideSign;
        sideAnchor[1] = sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_TORSO][1];
        sideAnchor[2] = sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_TORSO][2]
                      + (sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM][2]
                       - sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM][2]) * 0.18f * sideSign;
        vec3f_dif(delta, sDeathRagdollNodePos[limb], sideAnchor);
    }
    dist = sqrtf(delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2]);
    if (dist >= minDist || dist < 0.5f) {
        return;
    }

    settleScale = (sDeathRagdollGroundContacts > 0)
        ? death_ragdoll_clampf((18.0f - gMarioState->actionTimer) / 18.0f, 0.02f, 1.0f)
        : 1.0f;
    correction = (minDist - dist) / dist * stiffness * settleScale;
    death_ragdoll_apply_limb_position_correction(limb,
                                                 delta[0] * correction,
                                                 delta[1] * correction,
                                                 delta[2] * correction);
}

static void death_ragdoll_limit_limb_backward_fold(s32 root, s32 mid, s32 end, f32 minForward, f32 stiffness) {
    Vec3f shoulderToHead;
    Vec3f rootToEnd;
    Vec3f correction;
    f32 forwardLen;
    f32 dot;
    f32 amount;
    f32 settleScale;

    shoulderToHead[0] = sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_HEAD][0]
                      - sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_TORSO][0];
    shoulderToHead[1] = 0.0f;
    shoulderToHead[2] = sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_HEAD][2]
                      - sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_TORSO][2];
    forwardLen = sqrtf(shoulderToHead[0] * shoulderToHead[0] + shoulderToHead[2] * shoulderToHead[2]);
    if (forwardLen < 1.0f) {
        return;
    }

    shoulderToHead[0] /= forwardLen;
    shoulderToHead[2] /= forwardLen;
    vec3f_dif(rootToEnd, sDeathRagdollNodePos[end], sDeathRagdollNodePos[root]);
    dot = rootToEnd[0] * shoulderToHead[0] + rootToEnd[2] * shoulderToHead[2];
    if (dot >= minForward) {
        return;
    }

    settleScale = (sDeathRagdollGroundContacts > 0)
        ? death_ragdoll_clampf((18.0f - gMarioState->actionTimer) / 18.0f, 0.03f, 1.0f)
        : 1.0f;
    amount = (minForward - dot) * stiffness * settleScale;
    correction[0] = shoulderToHead[0] * amount;
    correction[1] = 0.0f;
    correction[2] = shoulderToHead[2] * amount;
    death_ragdoll_apply_limb_position_correction(mid, correction[0] * 0.45f, 0.0f,
                                                 correction[2] * 0.45f);
    death_ragdoll_apply_limb_position_correction(end, correction[0], 0.0f, correction[2]);
}

static void death_ragdoll_solve_anatomy_limits(void) {
#define RG_DIST(units) ((units) * DEATH_RAGDOLL_VISUAL_SCALE)
    death_ragdoll_push_limb_out_of_torso(DEATH_RAGDOLL_LIMB_LEFT_FOREARM, RG_DIST(66.0f), 0.16f);
    death_ragdoll_push_limb_out_of_torso(DEATH_RAGDOLL_LIMB_RIGHT_FOREARM, RG_DIST(66.0f), 0.16f);
    death_ragdoll_push_limb_out_of_torso(DEATH_RAGDOLL_LIMB_LEFT_HAND, RG_DIST(72.0f), 0.18f);
    death_ragdoll_push_limb_out_of_torso(DEATH_RAGDOLL_LIMB_RIGHT_HAND, RG_DIST(72.0f), 0.18f);
    death_ragdoll_push_limb_out_of_torso(DEATH_RAGDOLL_LIMB_LEFT_LEG, RG_DIST(34.0f), 0.025f);
    death_ragdoll_push_limb_out_of_torso(DEATH_RAGDOLL_LIMB_RIGHT_LEG, RG_DIST(34.0f), 0.025f);
    death_ragdoll_push_limb_out_of_torso(DEATH_RAGDOLL_LIMB_LEFT_FOOT, RG_DIST(38.0f), 0.025f);
    death_ragdoll_push_limb_out_of_torso(DEATH_RAGDOLL_LIMB_RIGHT_FOOT, RG_DIST(38.0f), 0.025f);

    death_ragdoll_limit_limb_backward_fold(DEATH_RAGDOLL_LIMB_LEFT_THIGH,
                                           DEATH_RAGDOLL_LIMB_LEFT_LEG,
                                           DEATH_RAGDOLL_LIMB_LEFT_FOOT,
                                           RG_DIST(-42.0f), 0.11f);
    death_ragdoll_limit_limb_backward_fold(DEATH_RAGDOLL_LIMB_RIGHT_THIGH,
                                           DEATH_RAGDOLL_LIMB_RIGHT_LEG,
                                           DEATH_RAGDOLL_LIMB_RIGHT_FOOT,
                                           RG_DIST(-42.0f), 0.11f);
    death_ragdoll_limit_limb_backward_fold(DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM,
                                           DEATH_RAGDOLL_LIMB_LEFT_FOREARM,
                                           DEATH_RAGDOLL_LIMB_LEFT_HAND,
                                           RG_DIST(-48.0f), 0.10f);
    death_ragdoll_limit_limb_backward_fold(DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM,
                                           DEATH_RAGDOLL_LIMB_RIGHT_FOREARM,
                                           DEATH_RAGDOLL_LIMB_RIGHT_HAND,
                                           RG_DIST(-48.0f), 0.10f);
#undef RG_DIST
}

static void death_ragdoll_solve_body_root_slot(struct MarioState *m, s32 limb) {
    Vec3f slot;
    Vec3f targetOffset;
    Vec3f target;
    Vec3f worldCorrection;
    Vec3s bodyAngle;
    f32 slack;
    f32 dx;
    f32 dy;
    f32 dz;
    f32 dist;

#if DEATH_RAGDOLL_HARD_HUMANOID_DEBUG
    return;
#endif

    if (!death_ragdoll_is_body_root_limb(limb)) {
        return;
    }

    death_ragdoll_get_body_root_slot(limb, slot, &slack);
    bodyAngle[0] = (s16) sDeathRagdollLimbAngle[DEATH_RAGDOLL_LIMB_TORSO][0];
    bodyAngle[1] = m->marioObj->header.gfx.angle[1] + (s16) sDeathRagdollLimbAngle[DEATH_RAGDOLL_LIMB_TORSO][1];
    bodyAngle[2] = (s16) sDeathRagdollLimbAngle[DEATH_RAGDOLL_LIMB_TORSO][2];
    death_ragdoll_rotate_local_offset(targetOffset, slot, bodyAngle);

    target[0] = sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_TORSO][0] + targetOffset[0];
    target[1] = sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_TORSO][1] + targetOffset[1];
    target[2] = sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_TORSO][2] + targetOffset[2];

    dx = sDeathRagdollNodePos[limb][0] - target[0];
    dy = sDeathRagdollNodePos[limb][1] - target[1];
    dz = sDeathRagdollNodePos[limb][2] - target[2];
    dist = sqrtf(dx * dx + dy * dy + dz * dz);
    if (dist <= slack || dist < 0.1f) {
        return;
    }

    worldCorrection[0] = dx / dist * (dist - slack) * 0.68f;
    worldCorrection[1] = dy / dist * (dist - slack) * 0.68f;
    worldCorrection[2] = dz / dist * (dist - slack) * 0.68f;

    death_ragdoll_apply_limb_position_correction(limb,
                                                 -worldCorrection[0],
                                                 -worldCorrection[1],
                                                 -worldCorrection[2]);
    death_ragdoll_apply_limb_position_correction(DEATH_RAGDOLL_LIMB_TORSO,
                                                 worldCorrection[0] * 0.05f,
                                                 worldCorrection[1] * 0.05f,
                                                 worldCorrection[2] * 0.05f);
}

static u8 death_ragdoll_nodes_are_connected(s32 a, s32 b) {
    return death_ragdoll_parent_limb(a) == b || death_ragdoll_parent_limb(b) == a;
}

static f32 death_ragdoll_connected_min_dist_scale(s32 a, s32 b) {
    if ((a == DEATH_RAGDOLL_LIMB_TORSO && b == DEATH_RAGDOLL_LIMB_HEAD)
        || (a == DEATH_RAGDOLL_LIMB_HEAD && b == DEATH_RAGDOLL_LIMB_TORSO)) {
        return 0.74f;
    }

    if ((a == DEATH_RAGDOLL_LIMB_TORSO && (b == DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM
        || b == DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM || b == DEATH_RAGDOLL_LIMB_LEFT_THIGH
        || b == DEATH_RAGDOLL_LIMB_RIGHT_THIGH))
        || (b == DEATH_RAGDOLL_LIMB_TORSO && (a == DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM
        || a == DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM || a == DEATH_RAGDOLL_LIMB_LEFT_THIGH
        || a == DEATH_RAGDOLL_LIMB_RIGHT_THIGH))) {
        return 0.0f;
    }

    return 0.0f;
}

static f32 death_ragdoll_pair_min_dist_scale(s32 a, s32 b) {
    if ((a == DEATH_RAGDOLL_LIMB_LEFT_THIGH && b == DEATH_RAGDOLL_LIMB_RIGHT_THIGH)
        || (a == DEATH_RAGDOLL_LIMB_RIGHT_THIGH && b == DEATH_RAGDOLL_LIMB_LEFT_THIGH)
        || (a == DEATH_RAGDOLL_LIMB_LEFT_LEG && b == DEATH_RAGDOLL_LIMB_RIGHT_LEG)
        || (a == DEATH_RAGDOLL_LIMB_RIGHT_LEG && b == DEATH_RAGDOLL_LIMB_LEFT_LEG)
        || (a == DEATH_RAGDOLL_LIMB_LEFT_FOOT && b == DEATH_RAGDOLL_LIMB_RIGHT_FOOT)
        || (a == DEATH_RAGDOLL_LIMB_RIGHT_FOOT && b == DEATH_RAGDOLL_LIMB_LEFT_FOOT)) {
        return 0.42f;
    }

    if ((a == DEATH_RAGDOLL_LIMB_LEFT_FOOT || a == DEATH_RAGDOLL_LIMB_RIGHT_FOOT
         || a == DEATH_RAGDOLL_LIMB_LEFT_LEG || a == DEATH_RAGDOLL_LIMB_RIGHT_LEG)
        && (b == DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM || b == DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM
            || b == DEATH_RAGDOLL_LIMB_LEFT_FOREARM || b == DEATH_RAGDOLL_LIMB_RIGHT_FOREARM
            || b == DEATH_RAGDOLL_LIMB_LEFT_HAND || b == DEATH_RAGDOLL_LIMB_RIGHT_HAND)) {
        return 0.56f;
    }

    if ((b == DEATH_RAGDOLL_LIMB_LEFT_FOOT || b == DEATH_RAGDOLL_LIMB_RIGHT_FOOT
         || b == DEATH_RAGDOLL_LIMB_LEFT_LEG || b == DEATH_RAGDOLL_LIMB_RIGHT_LEG)
        && (a == DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM || a == DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM
            || a == DEATH_RAGDOLL_LIMB_LEFT_FOREARM || a == DEATH_RAGDOLL_LIMB_RIGHT_FOREARM
            || a == DEATH_RAGDOLL_LIMB_LEFT_HAND || a == DEATH_RAGDOLL_LIMB_RIGHT_HAND)) {
        return 0.56f;
    }

    return 1.0f;
}

static void death_ragdoll_solve_node_self_collision(s32 a, s32 b) {
    Vec3f delta;
    f32 dist;
    f32 minDist;
    f32 invA;
    f32 invB;
    f32 invTotal;
    f32 correction;

    vec3f_dif(delta, sDeathRagdollNodePos[b], sDeathRagdollNodePos[a]);
    dist = sqrtf(delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2]);
    if (death_ragdoll_nodes_are_connected(a, b)) {
        minDist = (death_ragdoll_node_radius(a) + death_ragdoll_node_radius(b))
                * death_ragdoll_connected_min_dist_scale(a, b);
        if (minDist <= 0.0f) {
            return;
        }
    } else {
        minDist = (death_ragdoll_node_radius(a) + death_ragdoll_node_radius(b))
                * 1.08f * death_ragdoll_pair_min_dist_scale(a, b);
    }
    if (dist >= minDist || dist < 0.5f) {
        return;
    }

    invA = 1.0f / death_ragdoll_node_mass(a);
    invB = 1.0f / death_ragdoll_node_mass(b);
    invTotal = invA + invB;
    correction = (minDist - dist) / dist * 0.78f;

    death_ragdoll_apply_limb_position_correction(a,
                                                 -delta[0] * correction * (invA / invTotal),
                                                 -delta[1] * correction * (invA / invTotal),
                                                 -delta[2] * correction * (invA / invTotal));
    death_ragdoll_apply_limb_position_correction(b,
                                                 delta[0] * correction * (invB / invTotal),
                                                 delta[1] * correction * (invB / invTotal),
                                                 delta[2] * correction * (invB / invTotal));
}

static void death_ragdoll_solve_self_collisions(void) {
    s32 a;
    s32 b;

    for (a = 0; a < DEATH_RAGDOLL_LIMB_COUNT; a++) {
        for (b = a + 1; b < DEATH_RAGDOLL_LIMB_COUNT; b++) {
            death_ragdoll_solve_node_self_collision(a, b);
        }
    }
}

static void death_ragdoll_damp_after_contact(void) {
    s32 limb;
    f32 torsoDamp;

    if (sDeathRagdollGroundContacts == 0) {
        return;
    }

    for (limb = 0; limb < DEATH_RAGDOLL_LIMB_COUNT; limb++) {
        f32 horizontalDamp = (sDeathRagdollNodeHitTimer[limb] > 0) ? 0.46f : 0.68f;
        f32 verticalDamp = (sDeathRagdollNodeHitTimer[limb] > 0) ? 0.70f : 0.86f;
        f32 angleDamp = (sDeathRagdollNodeHitTimer[limb] > 0) ? 0.64f : 0.84f;

        if (sDeathRagdollSource == DEATH_RAGDOLL_SOURCE_KNOCKBACK && gMarioState != NULL
            && gMarioState->actionTimer < 34) {
            horizontalDamp = max(horizontalDamp, 0.92f);
            angleDamp = max(angleDamp, 0.76f);
        }

        if (limb == DEATH_RAGDOLL_LIMB_LEFT_HAND || limb == DEATH_RAGDOLL_LIMB_RIGHT_HAND
            || limb == DEATH_RAGDOLL_LIMB_LEFT_FOOT || limb == DEATH_RAGDOLL_LIMB_RIGHT_FOOT) {
            horizontalDamp -= 0.08f;
            angleDamp -= 0.08f;
        }

        sDeathRagdollNodeVel[limb][0] *= horizontalDamp;
        sDeathRagdollNodeVel[limb][1] *= verticalDamp;
        sDeathRagdollNodeVel[limb][2] *= horizontalDamp;
        if (death_ragdoll_absf(sDeathRagdollNodeVel[limb][0]) < 0.18f) {
            sDeathRagdollNodeVel[limb][0] = 0.0f;
        }
        if (death_ragdoll_absf(sDeathRagdollNodeVel[limb][2]) < 0.18f) {
            sDeathRagdollNodeVel[limb][2] = 0.0f;
        }
        sDeathRagdollLimbVel[limb][0] *= angleDamp;
        sDeathRagdollLimbVel[limb][1] *= angleDamp;
        sDeathRagdollLimbVel[limb][2] *= angleDamp;
    }

    torsoDamp = (sDeathRagdollCenterContacts > 0) ? 0.28f : 0.42f;
    if (sDeathRagdollSource == DEATH_RAGDOLL_SOURCE_KNOCKBACK && gMarioState != NULL
        && gMarioState->actionTimer < 34) {
        torsoDamp = max(torsoDamp, 0.94f);
    }
    sDeathRagdollNodeVel[DEATH_RAGDOLL_LIMB_TORSO][0] *= torsoDamp;
    sDeathRagdollNodeVel[DEATH_RAGDOLL_LIMB_TORSO][2] *= torsoDamp;
    if (death_ragdoll_absf(sDeathRagdollNodeVel[DEATH_RAGDOLL_LIMB_TORSO][0]) < 0.55f) {
        sDeathRagdollNodeVel[DEATH_RAGDOLL_LIMB_TORSO][0] = 0.0f;
    }
    if (death_ragdoll_absf(sDeathRagdollNodeVel[DEATH_RAGDOLL_LIMB_TORSO][2]) < 0.55f) {
        sDeathRagdollNodeVel[DEATH_RAGDOLL_LIMB_TORSO][2] = 0.0f;
    }
}

static void death_ragdoll_apply_settle_gravity(void) {
    s32 limb;

    for (limb = DEATH_RAGDOLL_LIMB_HEAD; limb < DEATH_RAGDOLL_LIMB_COUNT; limb++) {
        f32 gravity = death_ragdoll_solver_gravity() * death_ragdoll_settle_gravity_scale(limb);

        if (sDeathRagdollNodeContact[limb]) {
            continue;
        }
        death_ragdoll_apply_limb_position_correction(limb, 0.0f, -gravity * 0.18f, 0.0f);
    }
}

static void death_ragdoll_collide_skeleton_nodes(void) {
    s32 limb;
    s32 bodyNode;
    f32 radius;

    if (death_ragdoll_center_out_of_bounds()) {
        return;
    }

    for (bodyNode = 0; bodyNode < DEATH_RAGDOLL_BODY_NODE_COUNT; bodyNode++) {
        death_ragdoll_collide_body_node(bodyNode);
    }

    for (limb = 0; limb < DEATH_RAGDOLL_LIMB_COUNT; limb++) {
        radius = death_ragdoll_node_radius(limb);
        death_ragdoll_collide_node_segment(limb, radius);
        death_ragdoll_collide_node_wall(limb, radius);
    }
    if (sDeathRagdollGroundContacts > 0) {
        death_ragdoll_collide_full_volume();
    }
}

static void death_ragdoll_update_mass_center(struct MarioState *m) {
    s32 limb;
    s32 bodyNode;
    f32 mass;
    f32 totalMass = 0.0f;
    Vec3f center;
    Vec3f velocity;
    Vec3f oldVel;

    vec3f_set(center, 0.0f, 0.0f, 0.0f);
    vec3f_set(velocity, 0.0f, 0.0f, 0.0f);
    vec3f_copy(oldVel, m->vel);

    if (sDeathRagdollGroundContacts == 0) {
        for (limb = 0; limb < DEATH_RAGDOLL_LIMB_COUNT; limb++) {
            mass = death_ragdoll_node_mass(limb);
            totalMass += mass;
            center[0] += sDeathRagdollNodePos[limb][0] * mass;
            center[1] += sDeathRagdollNodePos[limb][1] * mass;
            center[2] += sDeathRagdollNodePos[limb][2] * mass;
            velocity[0] += sDeathRagdollNodeVel[limb][0] * mass;
            velocity[1] += sDeathRagdollNodeVel[limb][1] * mass;
            velocity[2] += sDeathRagdollNodeVel[limb][2] * mass;
        }
    }

    for (bodyNode = 0; bodyNode < DEATH_RAGDOLL_BODY_NODE_COUNT; bodyNode++) {
        mass = death_ragdoll_body_node_mass(bodyNode);
        totalMass += mass;
        center[0] += sDeathRagdollBodyNodePos[bodyNode][0] * mass;
        center[1] += sDeathRagdollBodyNodePos[bodyNode][1] * mass;
        center[2] += sDeathRagdollBodyNodePos[bodyNode][2] * mass;
        velocity[0] += sDeathRagdollBodyNodeVel[bodyNode][0] * mass;
        velocity[1] += sDeathRagdollBodyNodeVel[bodyNode][1] * mass;
        velocity[2] += sDeathRagdollBodyNodeVel[bodyNode][2] * mass;
    }

    if (totalMass <= 0.0f) {
        return;
    }

    sDeathRagdollCenter[0] = center[0] / totalMass;
    sDeathRagdollCenter[1] = center[1] / totalMass;
    sDeathRagdollCenter[2] = center[2] / totalMass;
    m->vel[0] = velocity[0] / totalMass;
    m->vel[1] = velocity[1] / totalMass;
    m->vel[2] = velocity[2] / totalMass;
    if (sDeathRagdollGroundContacts > 0) {
        f32 groundDamp = 0.72f;

        if (sDeathRagdollSource == DEATH_RAGDOLL_SOURCE_KNOCKBACK && m->actionTimer < 34) {
            groundDamp = 0.98f;
            m->vel[0] = oldVel[0] * 0.58f + m->vel[0] * 0.42f;
            m->vel[2] = oldVel[2] * 0.58f + m->vel[2] * 0.42f;
            if (oldVel[1] > 0.0f && m->vel[1] < oldVel[1] * 0.62f) {
                m->vel[1] = oldVel[1] * 0.62f;
            }
        }
        m->vel[0] *= groundDamp;
        m->vel[2] *= groundDamp;
    }
    if (death_ragdoll_absf(m->vel[0]) < 0.08f) {
        m->vel[0] = 0.0f;
    }
    if (death_ragdoll_absf(m->vel[2]) < 0.08f) {
        m->vel[2] = 0.0f;
    }
    m->forwardVel = sqrtf(m->vel[0] * m->vel[0] + m->vel[2] * m->vel[2]);
}

static void death_ragdoll_apply_slope_slide(struct MarioState *m) {
    s32 limb;
    s32 bodyNode;
    struct Surface *floor = m->floor;
    f32 steepness;
    f32 accel;
    f32 slideX;
    f32 slideZ;
    s16 slopeAngle;

    if (floor == NULL || sDeathRagdollGroundContacts == 0 || floor->normal.y >= 0.9998f) {
        return;
    }

    steepness = sqrtf(floor->normal.x * floor->normal.x + floor->normal.z * floor->normal.z);
    if (steepness <= 0.001f) {
        return;
    }

    slopeAngle = atan2s(floor->normal.z, floor->normal.x);
    accel = 6.0f * steepness;
    slideX = accel * sins(slopeAngle);
    slideZ = accel * coss(slopeAngle);

    m->vel[0] += slideX;
    m->vel[2] += slideZ;
    m->forwardVel = sqrtf(m->vel[0] * m->vel[0] + m->vel[2] * m->vel[2]);

    for (bodyNode = 0; bodyNode < DEATH_RAGDOLL_BODY_NODE_COUNT; bodyNode++) {
        sDeathRagdollBodyNodeVel[bodyNode][0] += slideX;
        sDeathRagdollBodyNodeVel[bodyNode][2] += slideZ;
    }
    for (limb = 0; limb < DEATH_RAGDOLL_LIMB_COUNT; limb++) {
        sDeathRagdollNodeVel[limb][0] += slideX;
        sDeathRagdollNodeVel[limb][2] += slideZ;
    }
}

static void death_ragdoll_share_center_inertia(struct MarioState *m) {
    s32 limb;
    s32 bodyNode;
    f32 bodyBlend;
    f32 limbBlend;

    if (sDeathRagdollGroundContacts == 0) {
        return;
    }

    if (sDeathRagdollSource == DEATH_RAGDOLL_SOURCE_KNOCKBACK && m->actionTimer < 48) {
        bodyBlend = 0.42f;
        limbBlend = 0.30f;
    } else {
        bodyBlend = 0.18f;
        limbBlend = 0.10f;
    }

    for (bodyNode = 0; bodyNode < DEATH_RAGDOLL_BODY_NODE_COUNT; bodyNode++) {
        sDeathRagdollBodyNodeVel[bodyNode][0] =
            sDeathRagdollBodyNodeVel[bodyNode][0] * (1.0f - bodyBlend) + m->vel[0] * bodyBlend;
        sDeathRagdollBodyNodeVel[bodyNode][2] =
            sDeathRagdollBodyNodeVel[bodyNode][2] * (1.0f - bodyBlend) + m->vel[2] * bodyBlend;
        if (m->vel[1] > 0.0f) {
            sDeathRagdollBodyNodeVel[bodyNode][1] =
                max(sDeathRagdollBodyNodeVel[bodyNode][1],
                    sDeathRagdollBodyNodeVel[bodyNode][1] * (1.0f - bodyBlend) + m->vel[1] * bodyBlend);
        }
    }
    for (limb = 0; limb < DEATH_RAGDOLL_LIMB_COUNT; limb++) {
        sDeathRagdollNodeVel[limb][0] =
            sDeathRagdollNodeVel[limb][0] * (1.0f - limbBlend) + m->vel[0] * limbBlend;
        sDeathRagdollNodeVel[limb][2] =
            sDeathRagdollNodeVel[limb][2] * (1.0f - limbBlend) + m->vel[2] * limbBlend;
        if (m->vel[1] > 0.0f) {
            sDeathRagdollNodeVel[limb][1] =
                max(sDeathRagdollNodeVel[limb][1],
                    sDeathRagdollNodeVel[limb][1] * (1.0f - limbBlend) + m->vel[1] * limbBlend);
        }
    }
}

static void death_ragdoll_clamp_skeleton_velocity(void) {
    s32 limb;
    s32 bodyNode;
    f32 maxSpeed = (sDeathRagdollGroundContacts > 0) ? 14.0f : 28.0f;

    if (sDeathRagdollGroundContacts == 0) {
        maxSpeed = max(maxSpeed, -DEATH_RAGDOLL_MAX_FALL_SPEED + 12.0f);
    }

    if (sDeathRagdollProfile != DEATH_RAGDOLL_PROFILE_DEFAULT
        && sDeathRagdollProfile != DEATH_RAGDOLL_PROFILE_DISABLED) {
        if (sDeathRagdollGroundContacts == 0) {
            maxSpeed *= max(death_ragdoll_horizontal_impulse_multiplier(),
                            death_ragdoll_upward_impulse_multiplier());
        } else if (gMarioState != NULL && gMarioState->actionTimer < 48) {
            maxSpeed = 46.0f;
        }
    }

    for (bodyNode = 0; bodyNode < DEATH_RAGDOLL_BODY_NODE_COUNT; bodyNode++) {
        death_ragdoll_clamp_vec3f_speed(sDeathRagdollBodyNodeVel[bodyNode], maxSpeed);
        death_ragdoll_clamp_point_around_center(sDeathRagdollBodyNodePos[bodyNode],
                                                sDeathRagdollBodyNodeVel[bodyNode],
                                                DEATH_RAGDOLL_MAX_BODY_WORLD_OFFSET);
    }

    for (limb = 0; limb < DEATH_RAGDOLL_LIMB_COUNT; limb++) {
        death_ragdoll_clamp_vec3f_speed(sDeathRagdollNodeVel[limb], maxSpeed);
        death_ragdoll_clamp_point_around_center(sDeathRagdollNodePos[limb],
                                                sDeathRagdollNodeVel[limb],
                                                DEATH_RAGDOLL_MAX_NODE_WORLD_OFFSET);

#if DEATH_RAGDOLL_HARD_HUMANOID_DEBUG
        if (sDeathRagdollGroundContacts > 0) {
            if (sDeathRagdollSource == DEATH_RAGDOLL_SOURCE_KNOCKBACK
                && gMarioState != NULL && gMarioState->actionTimer < 48) {
                sDeathRagdollNodeVel[limb][0] *= 0.94f;
                sDeathRagdollNodeVel[limb][1] *= 0.82f;
                sDeathRagdollNodeVel[limb][2] *= 0.94f;
            } else {
                sDeathRagdollNodeVel[limb][0] *= 0.72f;
                sDeathRagdollNodeVel[limb][1] *= 0.62f;
                sDeathRagdollNodeVel[limb][2] *= 0.72f;
            }
        }
#endif
    }
}

static void death_ragdoll_step_physics_skeleton(struct MarioState *m) {
    s32 limb;
    s32 bodyNode;
    s32 iteration;
    f32 airVelocityDamp;
    f32 airRebuildDamp;

    death_ragdoll_apply_angular_velocity_field(m);
    death_ragdoll_clamp_skeleton_velocity();
    airVelocityDamp = (sDeathRagdollGroundContacts == 0) ? 0.996f : 0.986f;
    airRebuildDamp = (sDeathRagdollGroundContacts == 0) ? 0.985f : 0.92f;

    for (bodyNode = 0; bodyNode < DEATH_RAGDOLL_BODY_NODE_COUNT; bodyNode++) {
        vec3f_copy(sDeathRagdollBodyNodePrevPos[bodyNode], sDeathRagdollBodyNodePos[bodyNode]);
        sDeathRagdollBodyNodeContact[bodyNode] = FALSE;

        sDeathRagdollBodyNodeVel[bodyNode][1] -= death_ragdoll_fall_gravity(m);
        sDeathRagdollBodyNodeVel[bodyNode][1] =
            death_ragdoll_clampf(sDeathRagdollBodyNodeVel[bodyNode][1],
                                 DEATH_RAGDOLL_MAX_FALL_SPEED,
                                 death_ragdoll_max_upward_speed());
        sDeathRagdollBodyNodeVel[bodyNode][0] *= airVelocityDamp;
        sDeathRagdollBodyNodeVel[bodyNode][1] *= airVelocityDamp;
        sDeathRagdollBodyNodeVel[bodyNode][2] *= airVelocityDamp;

        sDeathRagdollBodyNodePos[bodyNode][0] += sDeathRagdollBodyNodeVel[bodyNode][0];
        sDeathRagdollBodyNodePos[bodyNode][1] += sDeathRagdollBodyNodeVel[bodyNode][1];
        sDeathRagdollBodyNodePos[bodyNode][2] += sDeathRagdollBodyNodeVel[bodyNode][2];
    }

    for (limb = 0; limb < DEATH_RAGDOLL_LIMB_COUNT; limb++) {
        vec3f_copy(sDeathRagdollNodePrevPos[limb], sDeathRagdollNodePos[limb]);
        sDeathRagdollNodeContact[limb] = FALSE;

        sDeathRagdollNodeVel[limb][1] -= death_ragdoll_fall_gravity(m);
        sDeathRagdollNodeVel[limb][1] = death_ragdoll_clampf(sDeathRagdollNodeVel[limb][1],
                                                             DEATH_RAGDOLL_MAX_FALL_SPEED,
                                                             death_ragdoll_max_upward_speed());
        sDeathRagdollNodeVel[limb][0] *= airVelocityDamp;
        sDeathRagdollNodeVel[limb][1] *= airVelocityDamp;
        sDeathRagdollNodeVel[limb][2] *= airVelocityDamp;

        sDeathRagdollNodePos[limb][0] += sDeathRagdollNodeVel[limb][0];
        sDeathRagdollNodePos[limb][1] += sDeathRagdollNodeVel[limb][1];
        sDeathRagdollNodePos[limb][2] += sDeathRagdollNodeVel[limb][2];
    }

    for (iteration = 0; iteration < DEATH_RAGDOLL_CONSTRAINT_ITERATIONS; iteration++) {
        for (limb = DEATH_RAGDOLL_LIMB_HEAD; limb < DEATH_RAGDOLL_LIMB_COUNT; limb++) {
            death_ragdoll_solve_joint_constraint(limb);
            death_ragdoll_solve_body_root_slot(m, limb);
            death_ragdoll_solve_humanoid_slot(m, limb);
        }
        death_ragdoll_solve_body_tripod_constraints();
        death_ragdoll_solve_body_anchor_constraints();
        death_ragdoll_solve_short_limb_constraints();
        death_ragdoll_solve_torso_shape_constraints();
        death_ragdoll_solve_anatomy_limits();
        death_ragdoll_apply_settle_gravity();
        death_ragdoll_solve_self_collisions();
        if ((iteration % DEATH_RAGDOLL_COLLISION_ITERATION_STRIDE) == (DEATH_RAGDOLL_COLLISION_ITERATION_STRIDE - 1)
            || iteration == DEATH_RAGDOLL_CONSTRAINT_ITERATIONS - 1) {
            death_ragdoll_collide_skeleton_nodes();
        }
        for (limb = DEATH_RAGDOLL_LIMB_HEAD; limb < DEATH_RAGDOLL_LIMB_COUNT; limb++) {
            death_ragdoll_solve_humanoid_slot(m, limb);
        }
    }
    airRebuildDamp = (sDeathRagdollGroundContacts == 0) ? 0.985f : 0.92f;
    for (limb = 0; limb < DEATH_RAGDOLL_LIMB_COUNT; limb++) {
        sDeathRagdollNodeVel[limb][0] =
            (sDeathRagdollNodePos[limb][0] - sDeathRagdollNodePrevPos[limb][0]) * airRebuildDamp;
        sDeathRagdollNodeVel[limb][1] =
            (sDeathRagdollNodePos[limb][1] - sDeathRagdollNodePrevPos[limb][1]) * airRebuildDamp;
        sDeathRagdollNodeVel[limb][2] =
            (sDeathRagdollNodePos[limb][2] - sDeathRagdollNodePrevPos[limb][2]) * airRebuildDamp;

        if (limb == DEATH_RAGDOLL_LIMB_HEAD) {
            if (sDeathRagdollSource == DEATH_RAGDOLL_SOURCE_KNOCKBACK && m->actionTimer < 48) {
                sDeathRagdollNodeVel[limb][0] *= 0.88f;
                sDeathRagdollNodeVel[limb][1] *= sDeathRagdollNodeContact[limb] ? 0.52f : 0.84f;
                sDeathRagdollNodeVel[limb][2] *= 0.88f;
            } else {
                sDeathRagdollNodeVel[limb][0] *= 0.72f;
                sDeathRagdollNodeVel[limb][1] *= sDeathRagdollNodeContact[limb] ? 0.18f : 0.72f;
                sDeathRagdollNodeVel[limb][2] *= 0.72f;
            }
            if (sDeathRagdollNodeContact[limb]
                && death_ragdoll_absf(sDeathRagdollNodeVel[limb][1]) < 1.8f) {
                sDeathRagdollNodeVel[limb][1] = 0.0f;
            }
            sDeathRagdollLimbVel[limb][0] *= 0.52f;
            sDeathRagdollLimbVel[limb][1] *= 0.52f;
            sDeathRagdollLimbVel[limb][2] *= 0.52f;
        }

        if (sDeathRagdollNodeHitTimer[limb] > 0) {
            if (sDeathRagdollSource == DEATH_RAGDOLL_SOURCE_KNOCKBACK && m->actionTimer < 34) {
                sDeathRagdollNodeVel[limb][0] *= 0.90f;
                sDeathRagdollNodeVel[limb][1] *= 0.62f;
                sDeathRagdollNodeVel[limb][2] *= 0.90f;
            } else {
                sDeathRagdollNodeVel[limb][0] *= 0.58f;
                sDeathRagdollNodeVel[limb][1] *= 0.46f;
                sDeathRagdollNodeVel[limb][2] *= 0.58f;
            }
            if (death_ragdoll_absf(sDeathRagdollNodeVel[limb][1]) < 1.4f) {
                sDeathRagdollNodeVel[limb][1] = 0.0f;
            }
            sDeathRagdollNodeHitTimer[limb]--;
        }
    }
    death_ragdoll_damp_after_contact();
    death_ragdoll_clamp_skeleton_velocity();
    if (sDeathRagdollLooseTimer > 0) {
        sDeathRagdollLooseTimer--;
    }

    death_ragdoll_update_mass_center(m);
}

static void death_ragdoll_warm_start_skeleton(void) {
    s32 limb;
    s32 bodyNode;
    s32 iteration;

    for (iteration = 0; iteration < DEATH_RAGDOLL_CONSTRAINT_ITERATIONS * 2; iteration++) {
        for (limb = DEATH_RAGDOLL_LIMB_HEAD; limb < DEATH_RAGDOLL_LIMB_COUNT; limb++) {
            death_ragdoll_solve_joint_constraint(limb);
            death_ragdoll_solve_body_root_slot(gMarioState, limb);
            death_ragdoll_solve_humanoid_slot(gMarioState, limb);
        }
        death_ragdoll_solve_body_tripod_constraints();
        death_ragdoll_solve_body_anchor_constraints();
        death_ragdoll_solve_short_limb_constraints();
        death_ragdoll_solve_torso_shape_constraints();
        death_ragdoll_solve_anatomy_limits();
        death_ragdoll_solve_self_collisions();
        for (limb = DEATH_RAGDOLL_LIMB_HEAD; limb < DEATH_RAGDOLL_LIMB_COUNT; limb++) {
            death_ragdoll_solve_humanoid_slot(gMarioState, limb);
        }
    }

    for (bodyNode = 0; bodyNode < DEATH_RAGDOLL_BODY_NODE_COUNT; bodyNode++) {
        sDeathRagdollBodyNodeVel[bodyNode][0] =
            (sDeathRagdollBodyNodePos[bodyNode][0] - sDeathRagdollBodyNodePrevPos[bodyNode][0]) * 0.90f;
        sDeathRagdollBodyNodeVel[bodyNode][1] =
            (sDeathRagdollBodyNodePos[bodyNode][1] - sDeathRagdollBodyNodePrevPos[bodyNode][1]) * 0.90f;
        sDeathRagdollBodyNodeVel[bodyNode][2] =
            (sDeathRagdollBodyNodePos[bodyNode][2] - sDeathRagdollBodyNodePrevPos[bodyNode][2]) * 0.90f;
        if (sDeathRagdollBodyNodeContact[bodyNode]) {
            sDeathRagdollBodyNodeVel[bodyNode][0] *= 0.68f;
            sDeathRagdollBodyNodeVel[bodyNode][1] *= 0.58f;
            sDeathRagdollBodyNodeVel[bodyNode][2] *= 0.68f;
        }
    }

    for (limb = 0; limb < DEATH_RAGDOLL_LIMB_COUNT; limb++) {
        vec3f_copy(sDeathRagdollNodePrevPos[limb], sDeathRagdollNodePos[limb]);
    }
    for (bodyNode = 0; bodyNode < DEATH_RAGDOLL_BODY_NODE_COUNT; bodyNode++) {
        vec3f_copy(sDeathRagdollBodyNodePrevPos[bodyNode], sDeathRagdollBodyNodePos[bodyNode]);
    }
}

static void death_ragdoll_pre_step_skeleton(void) {
    s32 limb;
    s32 bodyNode;
    s32 iteration;

    for (iteration = 0; iteration < 4; iteration++) {
        for (limb = DEATH_RAGDOLL_LIMB_HEAD; limb < DEATH_RAGDOLL_LIMB_COUNT; limb++) {
            death_ragdoll_solve_joint_constraint(limb);
            death_ragdoll_solve_body_root_slot(gMarioState, limb);
            death_ragdoll_solve_humanoid_slot(gMarioState, limb);
        }
        death_ragdoll_solve_body_tripod_constraints();
        death_ragdoll_solve_body_anchor_constraints();
        death_ragdoll_solve_short_limb_constraints();
        death_ragdoll_solve_torso_shape_constraints();
        death_ragdoll_solve_anatomy_limits();
        death_ragdoll_solve_self_collisions();
        death_ragdoll_collide_skeleton_nodes();
        for (limb = DEATH_RAGDOLL_LIMB_HEAD; limb < DEATH_RAGDOLL_LIMB_COUNT; limb++) {
            death_ragdoll_solve_humanoid_slot(gMarioState, limb);
        }
    }

    for (limb = 0; limb < DEATH_RAGDOLL_LIMB_COUNT; limb++) {
        vec3f_copy(sDeathRagdollNodePrevPos[limb], sDeathRagdollNodePos[limb]);
    }
    for (bodyNode = 0; bodyNode < DEATH_RAGDOLL_BODY_NODE_COUNT; bodyNode++) {
        vec3f_copy(sDeathRagdollBodyNodePrevPos[bodyNode], sDeathRagdollBodyNodePos[bodyNode]);
    }
}

static void death_ragdoll_apply_distributed_mass_to_body(struct MarioState *m) {
    s32 limb;
    f32 mass;
    f32 totalMass = 0.0f;
    Vec3f averageVel;

    vec3f_set(averageVel, 0.0f, 0.0f, 0.0f);

    for (limb = 0; limb < DEATH_RAGDOLL_LIMB_COUNT; limb++) {
        mass = death_ragdoll_node_mass(limb);
        totalMass += mass;
        averageVel[0] += sDeathRagdollNodeVel[limb][0] * mass;
        averageVel[1] += sDeathRagdollNodeVel[limb][1] * mass;
        averageVel[2] += sDeathRagdollNodeVel[limb][2] * mass;

        sDeathRagdollAngularVel[0] += (sDeathRagdollNodePos[limb][2] - sDeathRagdollCenter[2])
                                    * (sDeathRagdollNodeVel[limb][1] - m->vel[1]) * mass * 0.004f;
        sDeathRagdollAngularVel[1] += ((sDeathRagdollNodePos[limb][0] - sDeathRagdollCenter[0])
                                    * (sDeathRagdollNodeVel[limb][2] - m->vel[2])
                                    - (sDeathRagdollNodePos[limb][2] - sDeathRagdollCenter[2])
                                    * (sDeathRagdollNodeVel[limb][0] - m->vel[0])) * mass * 0.002f;
        sDeathRagdollAngularVel[2] += -(sDeathRagdollNodePos[limb][0] - sDeathRagdollCenter[0])
                                    * (sDeathRagdollNodeVel[limb][1] - m->vel[1]) * mass * 0.004f;
    }

    if (totalMass <= 0.0f) {
        return;
    }

    averageVel[0] /= totalMass;
    averageVel[1] /= totalMass;
    averageVel[2] /= totalMass;

    m->vel[0] = m->vel[0] * 0.84f + averageVel[0] * 0.16f;
    m->vel[1] = m->vel[1] * 0.86f + averageVel[1] * 0.14f;
    m->vel[2] = m->vel[2] * 0.84f + averageVel[2] * 0.16f;
    m->forwardVel = sqrtf(m->vel[0] * m->vel[0] + m->vel[2] * m->vel[2]);
}

static s16 death_ragdoll_vector_pitch(Vec3f from, Vec3f to) {
    f32 dx = to[0] - from[0];
    f32 dy = to[1] - from[1];
    f32 dz = to[2] - from[2];
    f32 lateral = sqrtf(dx * dx + dz * dz);

    return atan2s(lateral, dy) - 0x4000;
}

static s16 death_ragdoll_vector_yaw(Vec3f from, Vec3f to) {
    return atan2s(to[2] - from[2], to[0] - from[0]);
}

static void death_ragdoll_update_body_angle_from_skeleton(struct MarioState *m) {
    Vec3f sideA;
    Vec3f sideB;
    Vec3f upper;
    Vec3f lower;
    Vec3f bodyDelta;
    Vec3f xAxis;
    Vec3f yAxis;
    Vec3f zAxis;
    f32 sideDx;
    f32 sideDy;
    f32 sideDz;
    f32 sideDist;
    f32 bodyForward;
    f32 bodyUp;
    s16 targetPitch;
    s16 targetRoll;
    s16 targetYaw;

    sideA[0] = (sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM][0]
              + sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_LEFT_THIGH][0]) * 0.5f;
    sideA[1] = (sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM][1]
              + sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_LEFT_THIGH][1]) * 0.5f;
    sideA[2] = (sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM][2]
              + sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_LEFT_THIGH][2]) * 0.5f;
    sideB[0] = (sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM][0]
              + sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_RIGHT_THIGH][0]) * 0.5f;
    sideB[1] = (sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM][1]
              + sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_RIGHT_THIGH][1]) * 0.5f;
    sideB[2] = (sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM][2]
              + sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_RIGHT_THIGH][2]) * 0.5f;

    sideDx = sideB[0] - sideA[0];
    sideDy = sideB[1] - sideA[1];
    sideDz = sideB[2] - sideA[2];
    sideDist = sqrtf(sideDx * sideDx + sideDz * sideDz);

    upper[0] = (sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM][0]
              + sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM][0]) * 0.5f;
    upper[1] = (sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM][1]
              + sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM][1]) * 0.5f;
    upper[2] = (sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM][2]
              + sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM][2]) * 0.5f;
    lower[0] = (sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_LEFT_THIGH][0]
              + sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_RIGHT_THIGH][0]) * 0.5f;
    lower[1] = (sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_LEFT_THIGH][1]
              + sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_RIGHT_THIGH][1]) * 0.5f;
    lower[2] = (sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_LEFT_THIGH][2]
              + sDeathRagdollNodePos[DEATH_RAGDOLL_LIMB_RIGHT_THIGH][2]) * 0.5f;

    death_ragdoll_get_upper_body_world_axes(xAxis, yAxis, zAxis);
    targetYaw = atan2s(zAxis[0], zAxis[2]);
    m->marioObj->header.gfx.angle[1] =
        m->marioObj->header.gfx.angle[1]
        + (s16) (death_ragdoll_angle_delta(targetYaw, m->marioObj->header.gfx.angle[1]) * 0.42f);
    m->faceAngle[1] = m->marioObj->header.gfx.angle[1];

    death_ragdoll_world_to_render_delta(m, lower, upper, bodyDelta);
    bodyUp = max(bodyDelta[1], 1.0f);
    bodyForward = bodyDelta[2];

    targetPitch = atan2s(bodyForward, bodyUp) * 0.75f;
    targetRoll = atan2s(sideDy, max(sideDist, 1.0f)) * 0.75f;

    sDeathRagdollLimbAngle[DEATH_RAGDOLL_LIMB_TORSO][0] = targetPitch;
    sDeathRagdollLimbAngle[DEATH_RAGDOLL_LIMB_TORSO][1] = 0;
    sDeathRagdollLimbAngle[DEATH_RAGDOLL_LIMB_TORSO][2] = targetRoll;
}

static void death_ragdoll_update_limb_angles_from_skeleton(struct MarioState *m) {
    s32 limb;

    for (limb = DEATH_RAGDOLL_LIMB_HEAD; limb < DEATH_RAGDOLL_LIMB_COUNT; limb++) {
        s32 parent = death_ragdoll_parent_limb(limb);
        s16 targetPitch = death_ragdoll_vector_pitch(sDeathRagdollNodePos[parent], sDeathRagdollNodePos[limb]);
        s16 targetYaw = death_ragdoll_vector_yaw(sDeathRagdollNodePos[parent], sDeathRagdollNodePos[limb])
                      - m->marioObj->header.gfx.angle[1];
        s16 targetRoll = 0;

        sDeathRagdollLimbVel[limb][0] = death_ragdoll_angle_delta(targetPitch, sDeathRagdollLimbAngle[limb][0]);
        sDeathRagdollLimbVel[limb][1] = death_ragdoll_angle_delta(targetYaw, sDeathRagdollLimbAngle[limb][1]);
        sDeathRagdollLimbVel[limb][2] = death_ragdoll_angle_delta(targetRoll, sDeathRagdollLimbAngle[limb][2]);
        sDeathRagdollLimbAngle[limb][0] = targetPitch;
        sDeathRagdollLimbAngle[limb][1] = targetYaw;
        sDeathRagdollLimbAngle[limb][2] = targetRoll;
        death_ragdoll_clamp_limb_angles(limb);
    }
}

static void death_ragdoll_update_pose_from_skeleton(struct MarioState *m) {
    death_ragdoll_update_body_angle_from_skeleton(m);
    death_ragdoll_update_limb_angles_from_skeleton(m);
}

static void death_ragdoll_probe_collision(struct MarioState *m, s32 limb) {
    Vec3f probe;
    Vec3f support;
#if DEATH_RAGDOLL_ENABLE_WALL_RESOLVER
    Vec3f wallProbe;
#endif
    struct Surface *floor;
#if DEATH_RAGDOLL_ENABLE_WALL_RESOLVER
    struct Surface *wall;
#endif
    f32 floorHeight;
    f32 radius;
    f32 side;
    f32 up;
    f32 forward;

    death_ragdoll_get_component_probe(m, limb, probe, &radius);
    if (limb != DEATH_RAGDOLL_LIMB_TORSO) {
        vec3f_copy(probe, sDeathRagdollNodePos[limb]);
    }
    death_ragdoll_get_component_support_point(support, probe, radius);
    side = probe[0] - sDeathRagdollCenter[0];
    up = probe[1] - sDeathRagdollCenter[1];
    forward = probe[2] - sDeathRagdollCenter[2];

    if (!death_ragdoll_collision_query_is_safe(support, radius)) {
        return;
    }
    floorHeight = find_floor(support[0], probe[1] + radius + 120.0f, support[2], &floor);
    if (floor == NULL || floor->type == SURFACE_DEATH_PLANE) {
        return;
    }

    if (support[1] < floorHeight) {
        f32 penetration = floorHeight - support[1];
        f32 lever = death_ragdoll_clampf((death_ragdoll_absf(side) + death_ragdoll_absf(up) * 0.35f) / 86.0f,
                                         0.35f, 1.35f);
        f32 contactForce = death_ragdoll_clampf(penetration / 42.0f, 0.10f, 1.05f)
                         * death_ragdoll_contact_scale(limb);
        f32 limbPitch = (-forward * 7.0f + up * 1.0f) * contactForce;
        f32 limbYaw = (side > 0.0f ? -72.0f : 72.0f) * contactForce;
        f32 limbRoll = side * 9.0f * contactForce;

        sDeathRagdollGroundContacts++;
        if (death_ragdoll_limb_can_lift_center(limb) && penetration > sDeathRagdollMaxPenetration) {
            sDeathRagdollMaxPenetration = penetration;
        }
        if (death_ragdoll_limb_can_lift_center(limb)) {
            sDeathRagdollCenterContacts++;
        }
        if (-m->vel[1] > sDeathRagdollGroundImpactSpeed) {
            sDeathRagdollGroundImpactSpeed = -m->vel[1];
        }
        sDeathRagdollAngularVel[0] += -forward * 2.8f * lever * contactForce;
        sDeathRagdollAngularVel[1] += side * forward * 0.04f * lever * contactForce;
        sDeathRagdollAngularVel[2] += side * 2.8f * lever * contactForce;
        death_ragdoll_add_chained_contact_impulse(limb, limbPitch, limbYaw, limbRoll);
    }

#if DEATH_RAGDOLL_ENABLE_WALL_RESOLVER
    vec3f_copy(wallProbe, probe);
    if (!death_ragdoll_collision_query_is_safe(wallProbe, radius)) {
        return;
    }
    wall = resolve_and_return_wall_collisions(wallProbe, 0.0f, radius);
    if (wall != NULL) {
        sDeathRagdollCenter[0] += (wallProbe[0] - probe[0]) * 0.36f;
        sDeathRagdollCenter[2] += (wallProbe[2] - probe[2]) * 0.36f;
        m->vel[0] *= -0.32f;
        m->vel[2] *= -0.32f;
        m->forwardVel *= -0.32f;
        sDeathRagdollAngularVel[0] += up * 8.0f;
        sDeathRagdollAngularVel[1] += (side > 0.0f ? -900.0f : 900.0f);
        sDeathRagdollAngularVel[2] += side * 24.0f;
        death_ragdoll_add_limb_impulse(limb, 0x1800, (side > 0.0f ? -0x2800 : 0x2800), -0x2000);
    }
#endif
}

static void death_ragdoll_probe_body_shape_collision(struct MarioState *m, s32 point) {
    Vec3f probe;
    Vec3f support;
    struct Surface *floor;
    f32 floorHeight;
    f32 radius;
    f32 side;
    f32 up;
    f32 forward;

    death_ragdoll_get_body_contact_probe(m, point, probe, &radius);
    death_ragdoll_get_component_support_point(support, probe, radius);

    side = probe[0] - sDeathRagdollCenter[0];
    up = probe[1] - sDeathRagdollCenter[1];
    forward = probe[2] - sDeathRagdollCenter[2];

    if (!death_ragdoll_collision_query_is_safe(support, radius)) {
        return;
    }
    floorHeight = find_floor(support[0], probe[1] + radius + 120.0f, support[2], &floor);
    if (floor == NULL || floor->type == SURFACE_DEATH_PLANE || support[1] >= floorHeight) {
        return;
    }

    {
        f32 penetration = floorHeight - support[1];
        f32 lever = death_ragdoll_clampf((death_ragdoll_absf(side) + death_ragdoll_absf(forward)
                                       + death_ragdoll_absf(up) * 0.25f) / 70.0f, 0.25f, 1.45f);
        f32 contactForce = death_ragdoll_clampf(penetration / 38.0f, 0.12f, 1.15f);

        sDeathRagdollGroundContacts++;
        sDeathRagdollCenterContacts++;
        if (penetration > sDeathRagdollMaxPenetration) {
            sDeathRagdollMaxPenetration = penetration;
        }
        if (-m->vel[1] > sDeathRagdollGroundImpactSpeed) {
            sDeathRagdollGroundImpactSpeed = -m->vel[1];
        }

        sDeathRagdollAngularVel[0] += -forward * 1.2f * lever * contactForce;
        sDeathRagdollAngularVel[1] += side * forward * 0.05f * lever * contactForce;
        sDeathRagdollAngularVel[2] += side * 1.2f * lever * contactForce;
        if (sDeathRagdollSource == DEATH_RAGDOLL_SOURCE_KNOCKBACK && m->actionTimer < 34) {
            m->vel[0] *= 0.98f;
            m->vel[2] *= 0.98f;
        } else {
            m->vel[0] *= 0.90f;
            m->vel[2] *= 0.90f;
        }
    }
}

static void death_ragdoll_resolve_ground_contact(struct MarioState *m) {
    s32 limb;
    s32 bodyNode;
    f32 bounce;
    f32 horizontalSpeed;
    f32 rollDamp;

    if (sDeathRagdollGroundContacts == 0) {
        return;
    }

    horizontalSpeed = sqrtf(m->vel[0] * m->vel[0] + m->vel[2] * m->vel[2]);

    if (sDeathRagdollCenterContacts == 0) {
        m->vel[0] *= 0.94f;
        m->vel[2] *= 0.94f;
        m->forwardVel *= 0.94f;
        sDeathRagdollAngularVel[0] *= 0.92f;
        sDeathRagdollAngularVel[1] *= 0.96f;
        sDeathRagdollAngularVel[2] *= 0.92f;
        return;
    }

    if (m->vel[1] < 0.0f) {
        if (sDeathRagdollGroundImpactSpeed > 48.0f) {
            bounce = death_ragdoll_clampf((sDeathRagdollGroundImpactSpeed - 28.0f) * 0.24f, 0.0f, 16.0f);
        } else if (sDeathRagdollGroundImpactSpeed > 12.0f) {
            bounce = death_ragdoll_clampf((sDeathRagdollGroundImpactSpeed - 10.0f) * 0.16f, 1.1f, 7.0f);
        } else {
            bounce = 0.0f;
        }
        m->vel[1] = bounce;
        if (bounce > 0.0f) {
            if (sDeathRagdollWallBrakeTimer > 0) {
                m->vel[0] = 0.0f;
                m->vel[2] = 0.0f;
                m->forwardVel = 0.0f;
                sDeathRagdollAngularVel[0] *= 0.62f;
                sDeathRagdollAngularVel[2] *= 0.62f;
            } else {
                sDeathRagdollAngularVel[0] += m->vel[2] * bounce * 14.0f;
                sDeathRagdollAngularVel[2] -= m->vel[0] * bounce * 14.0f;
            }
            for (bodyNode = 0; bodyNode < DEATH_RAGDOLL_BODY_NODE_COUNT; bodyNode++) {
                if (sDeathRagdollBodyNodeVel[bodyNode][1] < bounce) {
                    sDeathRagdollBodyNodeVel[bodyNode][1] =
                        sDeathRagdollBodyNodeVel[bodyNode][1] * 0.35f + bounce * 0.65f;
                }
            }
            for (limb = 0; limb < DEATH_RAGDOLL_LIMB_COUNT; limb++) {
                if (sDeathRagdollNodeVel[limb][1] < bounce) {
                    sDeathRagdollNodeVel[limb][1] =
                        sDeathRagdollNodeVel[limb][1] * 0.42f + bounce * 0.58f;
                }
            }
        }
    }

    {
        f32 groundDamp = 0.76f;

        if (sDeathRagdollWallBrakeTimer > 0) {
            groundDamp = 0.0f;
        } else if (sDeathRagdollSource == DEATH_RAGDOLL_SOURCE_KNOCKBACK && m->actionTimer < 34) {
            groundDamp = 0.98f;
        }
        m->vel[0] *= groundDamp;
        m->vel[2] *= groundDamp;
        m->forwardVel *= groundDamp;
    }
    if (death_ragdoll_absf(m->vel[0]) < 0.08f) {
        m->vel[0] = 0.0f;
    }
    if (death_ragdoll_absf(m->vel[2]) < 0.08f) {
        m->vel[2] = 0.0f;
    }
    rollDamp = 0.66f + death_ragdoll_clampf((horizontalSpeed + sDeathRagdollGroundImpactSpeed * 0.35f) / 34.0f,
                                             0.0f, 1.0f) * 0.28f;
    sDeathRagdollAngularVel[0] *= rollDamp;
    sDeathRagdollAngularVel[1] *= 0.58f;
    sDeathRagdollAngularVel[2] *= rollDamp;
}

static void death_ragdoll_apply_ledge_drop_spin(struct MarioState *m, s32 previousGroundContacts) {
    f32 horizontalSpeed;
    f32 fallSpeed;
    f32 spin;
    s16 yaw;

    if (previousGroundContacts <= 0 || sDeathRagdollGroundContacts > 0 || sDeathRagdollWallBrakeTimer > 0) {
        return;
    }

    horizontalSpeed = sqrtf(m->vel[0] * m->vel[0] + m->vel[2] * m->vel[2]);
    fallSpeed = max(-m->vel[1], 0.0f);
    if (horizontalSpeed < 1.2f && fallSpeed < 2.0f) {
        return;
    }

    spin = death_ragdoll_clampf(horizontalSpeed * 86.0f + fallSpeed * 24.0f,
                                260.0f, 1650.0f);
    if (horizontalSpeed > 1.2f) {
        sDeathRagdollAngularVel[0] += (m->vel[2] / horizontalSpeed) * spin;
        sDeathRagdollAngularVel[2] -= (m->vel[0] / horizontalSpeed) * spin;
    } else {
        yaw = random_u16();
        sDeathRagdollAngularVel[0] += sins(yaw) * spin;
        sDeathRagdollAngularVel[2] += coss(yaw) * spin;
    }
    sDeathRagdollAngularVel[1] += ((random_u16() & 0x3FF) - 0x200) * 0.55f;
    death_ragdoll_clamp_angular_velocity();
}

static void death_ragdoll_collide_body_nodes(struct MarioState *m) {
    s32 limb;
    s32 point;

    for (limb = 0; limb < DEATH_RAGDOLL_LIMB_COUNT; limb++) {
        death_ragdoll_probe_collision(m, limb);
    }

    for (point = 0; point < DEATH_RAGDOLL_BODY_CONTACT_COUNT; point++) {
        death_ragdoll_probe_body_shape_collision(m, point);
    }
}

static f32 death_ragdoll_find_lowest_body_point(UNUSED struct MarioState *m) {
    s32 limb;
    Vec3f probe;
    f32 lowest = 100000.0f;
    f32 radius;

    for (limb = 0; limb < DEATH_RAGDOLL_LIMB_COUNT; limb++) {
        radius = death_ragdoll_node_radius(limb);
        vec3f_copy(probe, sDeathRagdollNodePos[limb]);
        death_ragdoll_get_component_support_point(probe, probe, radius);
        if (probe[1] < lowest) {
            lowest = probe[1];
        }
    }

    return lowest;
}

static void death_ragdoll_integrate_center(struct MarioState *m) {
#if DEATH_RAGDOLL_ENABLE_WALL_RESOLVER
    Vec3f wallCenter;
#endif
    struct Surface *floor;
#if DEATH_RAGDOLL_ENABLE_WALL_RESOLVER
    struct Surface *wall;
#endif
    f32 floorHeight;

    sDeathRagdollCenter[0] += m->vel[0];
    sDeathRagdollCenter[1] += m->vel[1];
    sDeathRagdollCenter[2] += m->vel[2];
    m->vel[1] -= death_ragdoll_fall_gravity(m);

#if DEATH_RAGDOLL_ENABLE_WALL_RESOLVER
    vec3f_copy(wallCenter, sDeathRagdollCenter);
    wall = resolve_and_return_wall_collisions(wallCenter, -DEATH_RAGDOLL_CENTER_Y * 0.25f, 38.0f);
    if (wall != NULL) {
        sDeathRagdollCenter[0] = wallCenter[0];
        sDeathRagdollCenter[2] = wallCenter[2];
        {
            f32 intoWall = m->vel[0] * wall->normal.x + m->vel[2] * wall->normal.z;

            if (intoWall < 0.0f) {
                m->vel[0] -= wall->normal.x * intoWall * 1.24f;
                m->vel[2] -= wall->normal.z * intoWall * 1.24f;
            }
        }
        m->vel[0] *= 0.72f;
        m->vel[2] *= 0.72f;
        m->forwardVel = sqrtf(m->vel[0] * m->vel[0] + m->vel[2] * m->vel[2]);
        sDeathRagdollAngularVel[1] += 680.0f;
        sDeathRagdollAngularVel[2] += 360.0f;
        death_ragdoll_clamp_angular_velocity();
    }
#endif

    floorHeight = find_floor(sDeathRagdollCenter[0], sDeathRagdollCenter[1] + 80.0f,
                             sDeathRagdollCenter[2], &floor);
    m->floor = floor;
    m->floorHeight = floorHeight;
}

static void death_ragdoll_apply_default_impulse(struct MarioState *m) {
    s16 yaw = m->faceAngle[1] + 0x8000 + ((random_u16() & 0x1FFF) - 0x1000);

    mario_set_forward_vel(m, 5.0f + ((random_u16() & 0xFF) / 255.0f) * 4.0f);
    m->faceAngle[1] = yaw;
    m->vel[0] = sins(yaw) * m->forwardVel;
    m->vel[2] = coss(yaw) * m->forwardVel;
    m->vel[1] = 10.0f + ((random_u16() & 0xFF) / 255.0f) * 5.0f;
}

static void death_ragdoll_apply_source_velocity(struct MarioState *m, enum DeathRagdollSource source) {
    f32 speed;
    f32 currentSpeed;
    f32 horizontalMultiplier = death_ragdoll_horizontal_impulse_multiplier();
    f32 upwardMultiplier = death_ragdoll_upward_impulse_multiplier();
    s16 yaw;

    if (source != DEATH_RAGDOLL_SOURCE_DEFAULT && m->interactObj != NULL) {
        currentSpeed = sqrtf(m->vel[0] * m->vel[0] + m->vel[2] * m->vel[2]);
        if (death_ragdoll_absf(m->forwardVel) > 4.0f) {
            yaw = m->faceAngle[1];
            speed = m->forwardVel;
            if (speed < 0.0f) {
                yaw += 0x8000;
                speed = -speed;
            }
        } else if (currentSpeed > 4.0f) {
            yaw = atan2s(m->vel[0], m->vel[2]);
            speed = currentSpeed;
        } else {
            yaw = mario_obj_angle_to_object(m, m->interactObj) + 0x8000;
            speed = death_ragdoll_absf(m->forwardVel);
        }
        if (speed < 18.0f) {
            speed = 18.0f;
        }
        if (source == DEATH_RAGDOLL_SOURCE_EXPLOSION) {
            speed += 10.0f;
        }
        m->faceAngle[1] = yaw;
        m->forwardVel = speed;
        m->vel[0] = sins(yaw) * speed;
        m->vel[2] = coss(yaw) * speed;
    } else if (source != DEATH_RAGDOLL_SOURCE_DEFAULT
               && m->vel[0] == 0.0f && m->vel[2] == 0.0f && m->forwardVel != 0.0f) {
        m->vel[0] = sins(m->faceAngle[1]) * m->forwardVel;
        m->vel[2] = coss(m->faceAngle[1]) * m->forwardVel;
    }

    if (m->vel[0] == 0.0f && m->vel[2] == 0.0f) {
        death_ragdoll_apply_default_impulse(m);
    } else {
        m->forwardVel = sqrtf(m->vel[0] * m->vel[0] + m->vel[2] * m->vel[2]);
        if (m->vel[1] < 10.0f) {
            m->vel[1] = 10.0f + ((random_u16() & 0xFF) / 255.0f) * 8.0f;
        }
    }

    if (sDeathRagdollProfile != DEATH_RAGDOLL_PROFILE_DEFAULT
        && sDeathRagdollProfile != DEATH_RAGDOLL_PROFILE_DISABLED) {
        m->vel[0] *= horizontalMultiplier;
        m->vel[2] *= horizontalMultiplier;
        m->forwardVel *= horizontalMultiplier;
        m->vel[1] = max(m->vel[1] * upwardMultiplier, 52.0f * upwardMultiplier);
    }
}

u8 death_ragdoll_damage_would_kill(struct MarioState *m) {
    return m->health - (m->hurtCounter * 0x40) < 0x100;
}

u8 death_ragdoll_is_explosion_source(struct Object *o) {
    if (o == NULL) {
        return FALSE;
    }
    return o->behavior == segmented_to_virtual(bhvExplosion);
}

enum DeathRagdollProfile death_ragdoll_profile_from_object(struct Object *o) {
    if (o == NULL) {
        return DEATH_RAGDOLL_PROFILE_DEFAULT;
    }

    if (death_ragdoll_is_explosion_source(o)) {
        return DEATH_RAGDOLL_PROFILE_EXPLOSION;
    }

    if (o->behavior == segmented_to_virtual(bhvChainChomp)) {
        return DEATH_RAGDOLL_PROFILE_CHAIN_CHOMP;
    }

    if (o->behavior == segmented_to_virtual(bhvBowser)
        || (o->oInteractionSubtype & INT_SUBTYPE_BIG_KNOCKBACK)) {
        return DEATH_RAGDOLL_PROFILE_BOWSER;
    }

    switch (o->oInteractType) {
        case INTERACT_BOUNCE_TOP:
        case INTERACT_BOUNCE_TOP2:
        case INTERACT_HIT_FROM_BELOW:
        case INTERACT_KOOPA:
            return DEATH_RAGDOLL_PROFILE_LIGHT_ENEMY;

        case INTERACT_DAMAGE:
        case INTERACT_MR_BLIZZARD:
            return DEATH_RAGDOLL_PROFILE_DIRECT_DAMAGE;

        case INTERACT_SHOCK:
            return DEATH_RAGDOLL_PROFILE_SHOCK;

        case INTERACT_SNUFIT_BULLET:
            return DEATH_RAGDOLL_PROFILE_PROJECTILE;

        case INTERACT_BULLY:
        case INTERACT_FLAME:
        case INTERACT_CLAM_OR_BUBBA:
            return DEATH_RAGDOLL_PROFILE_DISABLED;

        default:
            return DEATH_RAGDOLL_PROFILE_DIRECT_DAMAGE;
    }
}

static enum DeathRagdollSource death_ragdoll_source_from_profile(enum DeathRagdollProfile profile) {
    if (profile == DEATH_RAGDOLL_PROFILE_EXPLOSION) {
        return DEATH_RAGDOLL_SOURCE_EXPLOSION;
    }

    if (profile != DEATH_RAGDOLL_PROFILE_DEFAULT && profile != DEATH_RAGDOLL_PROFILE_DISABLED) {
        return DEATH_RAGDOLL_SOURCE_KNOCKBACK;
    }

    return DEATH_RAGDOLL_SOURCE_DEFAULT;
}

static enum DeathRagdollProfile death_ragdoll_profile_from_depleted_health(struct MarioState *m) {
    if (m->action == ACT_LAVA_BOOST || m->action == ACT_LAVA_BOOST_LAND
        || m->action == ACT_BURNING_GROUND || m->action == ACT_BURNING_JUMP
        || m->action == ACT_BURNING_FALL || m->action == ACT_SQUISHED
        || (m->input & INPUT_IN_POISON_GAS)
        || (m->action & ACT_FLAG_SWIMMING)) {
        return DEATH_RAGDOLL_PROFILE_DISABLED;
    }

    if (m->interactObj != NULL) {
        return death_ragdoll_profile_from_object(m->interactObj);
    }

    if (m->hurtCounter > 0) {
        return DEATH_RAGDOLL_PROFILE_FALL_DAMAGE;
    }

    return DEATH_RAGDOLL_PROFILE_DEFAULT;
}

u32 death_ragdoll_start_with_profile(struct MarioState *m, enum DeathRagdollSource source,
                                     enum DeathRagdollProfile profile) {
    s32 limb;

    if (profile == DEATH_RAGDOLL_PROFILE_DISABLED) {
        return FALSE;
    }

    mario_stop_riding_and_holding(m);
    sDeathRagdollSource = source;
    sDeathRagdollProfile = profile;
    sDeathRagdollHasBlastPos = FALSE;
    sDeathRagdollLooseTimer = (profile == DEATH_RAGDOLL_PROFILE_EXPLOSION) ? 18 : 0;
    sDeathRagdollWallBrakeTimer = 0;
    sDeathRagdollGroundContacts = 0;
    sDeathRagdollCenterContacts = 0;
    sDeathRagdollMaxPenetration = 0.0f;
    sDeathRagdollGroundImpactSpeed = 0.0f;
    if (m->interactObj != NULL) {
        sDeathRagdollHasBlastPos = TRUE;
        sDeathRagdollBlastPos[0] = m->interactObj->oPosX;
        sDeathRagdollBlastPos[1] = m->interactObj->oPosY;
        sDeathRagdollBlastPos[2] = m->interactObj->oPosZ;
    }
    vec3s_copy(sDeathRagdollEntryGfxAngle, m->marioObj->header.gfx.angle);
    death_ragdoll_reset_mario_object_scale(m);
    death_ragdoll_set_center_from_mario(m);
    death_ragdoll_apply_squish_escape_offset(m);
    sDeathRagdollWarpStarted = FALSE;
    sDeathRagdollSeed = random_u16() ^ gGlobalTimer ^ (s16) m->pos[0] ^ ((s16) m->pos[2] << 1);
    sDeathRagdollAngularVel[0] = (random_u16() - 0x8000) / 16.0f * death_ragdoll_rotation_debug_scale();
    sDeathRagdollAngularVel[1] = 0.0f;
    sDeathRagdollAngularVel[2] = (random_u16() - 0x8000) / 16.0f * death_ragdoll_rotation_debug_scale();
    for (limb = 0; limb < DEATH_RAGDOLL_LIMB_COUNT; limb++) {
        death_ragdoll_get_limb_rest_pose(limb, sDeathRagdollLimbAngle[limb]);
        sDeathRagdollLimbAngle[limb][0] += (random_u16() - 0x8000) / 96.0f;
        sDeathRagdollLimbAngle[limb][1] += (random_u16() - 0x8000) / 128.0f;
        sDeathRagdollLimbAngle[limb][2] += (random_u16() - 0x8000) / 96.0f;
        sDeathRagdollRenderAngle[limb][0] = sDeathRagdollLimbAngle[limb][0];
        sDeathRagdollRenderAngle[limb][1] = sDeathRagdollLimbAngle[limb][1];
        sDeathRagdollRenderAngle[limb][2] = sDeathRagdollLimbAngle[limb][2];
        sDeathRagdollRenderAngleTimer[limb] = -1;
        sDeathRagdollLimbVel[limb][0] = (random_u16() - 0x8000) / 360.0f;
        sDeathRagdollLimbVel[limb][1] = (random_u16() - 0x8000) / 420.0f;
        sDeathRagdollLimbVel[limb][2] = (random_u16() - 0x8000) / 360.0f;
        sDeathRagdollNodeHitTimer[limb] = 0;
    }

    death_ragdoll_apply_source_velocity(m, source);

    if (source == DEATH_RAGDOLL_SOURCE_EXPLOSION) {
        m->vel[0] *= 1.25f;
        m->vel[2] *= 1.25f;
        m->forwardVel *= 1.25f;
        m->vel[1] += 18.0f;
    } else if (sDeathRagdollProfile != DEATH_RAGDOLL_PROFILE_DEFAULT
               && sDeathRagdollProfile != DEATH_RAGDOLL_PROFILE_DISABLED
               && m->vel[1] < 52.0f * death_ragdoll_upward_impulse_multiplier()) {
        m->vel[1] = 52.0f * death_ragdoll_upward_impulse_multiplier();
    }

    death_ragdoll_cap_velocity(m);
    death_ragdoll_init_node_masses(m);
    death_ragdoll_apply_entry_node_impulse(m, source);
    death_ragdoll_warm_start_skeleton();
    death_ragdoll_pre_step_skeleton();
    death_ragdoll_reapply_entry_velocity_after_warm_start(m, source);
    death_ragdoll_add_launch_angular_velocity(m, source);
    death_ragdoll_freeze_current_animation(m);
    m->health = 0xFF;
    m->hurtCounter = 0;
    m->healCounter = 0;
    m->invincTimer = 0;

    play_sound_if_no_flag(m, SOUND_MARIO_ATTACKED, MARIO_MARIO_SOUND_PLAYED);
    return set_mario_action(m, ACT_DEATH_RAGDOLL, source);
}

u32 death_ragdoll_start(struct MarioState *m, enum DeathRagdollSource source) {
    enum DeathRagdollProfile profile = DEATH_RAGDOLL_PROFILE_DEFAULT;

    if (source == DEATH_RAGDOLL_SOURCE_EXPLOSION) {
        profile = DEATH_RAGDOLL_PROFILE_EXPLOSION;
    } else if (source == DEATH_RAGDOLL_SOURCE_KNOCKBACK) {
        profile = DEATH_RAGDOLL_PROFILE_DIRECT_DAMAGE;
    }

    return death_ragdoll_start_with_profile(m, source, profile);
}

u32 death_ragdoll_try_start_from_health_depleted(struct MarioState *m) {
    enum DeathRagdollProfile profile;

    if (m->health >= 0x100 || m->action == ACT_DEATH_RAGDOLL || (m->action & ACT_FLAG_INTANGIBLE)) {
        return FALSE;
    }

    if (m->floor == NULL || (m->floor != NULL && m->floor->type == SURFACE_DEATH_PLANE)) {
        return FALSE;
    }

    profile = death_ragdoll_profile_from_depleted_health(m);
    if (profile == DEATH_RAGDOLL_PROFILE_DISABLED) {
        return FALSE;
    }

    return death_ragdoll_start_with_profile(m, death_ragdoll_source_from_profile(profile), profile);
}

static void death_ragdoll_debug_full_heal(struct MarioState *m) {
    m->health = DEATH_RAGDOLL_DEBUG_FULL_HEALTH;
    m->hurtCounter = 0;
    m->healCounter = 0;
}

static void death_ragdoll_debug_clear_blj(void) {
    sDeathRagdollDebugZlHoldTimer = 0;
    sDeathRagdollDebugBljRepeatTimer = 0;
    sDeathRagdollDebugBljCoastTimer = 0;
    sDeathRagdollDebugBljActive = FALSE;
}

static void death_ragdoll_debug_release_blj(struct MarioState *m) {
    sDeathRagdollDebugZlHoldTimer = 0;
    sDeathRagdollDebugBljRepeatTimer = 0;
    if (sDeathRagdollDebugBljActive && m->forwardVel < DEATH_RAGDOLL_DEBUG_BLJ_START_FORWARD_VEL) {
        sDeathRagdollDebugBljCoastTimer = DEATH_RAGDOLL_DEBUG_BLJ_COAST_FRAMES;
    }
    sDeathRagdollDebugBljActive = FALSE;
}

static void death_ragdoll_debug_sync_mario_pos(struct MarioState *m, Vec3f pos) {
    vec3f_copy(m->pos, pos);
    if (m->marioObj != NULL) {
        m->marioObj->oPosX = m->pos[0];
        m->marioObj->oPosY = m->pos[1];
        m->marioObj->oPosZ = m->pos[2];
        vec3f_copy(m->marioObj->header.gfx.pos, m->pos);
    }
}

static u8 death_ragdoll_debug_can_start_blj(struct MarioState *m) {
    if (m->action == ACT_DEATH_RAGDOLL || (m->action & (ACT_FLAG_INTANGIBLE | ACT_FLAG_SWIMMING))
        || m->floor == NULL || m->floor->type == SURFACE_DEATH_PLANE) {
        return FALSE;
    }
    return TRUE;
}

static u32 death_ragdoll_debug_update_blj(struct MarioState *m, u8 zlHeld) {
    if (!zlHeld) {
        death_ragdoll_debug_release_blj(m);
        if (sDeathRagdollDebugBljCoastTimer > 0) {
            sDeathRagdollDebugBljCoastTimer--;
        }
        return FALSE;
    }

    if (sDeathRagdollDebugZlHoldTimer < DEATH_RAGDOLL_DEBUG_HOLD_FRAMES) {
        sDeathRagdollDebugZlHoldTimer++;
    }
    if (sDeathRagdollDebugZlHoldTimer < DEATH_RAGDOLL_DEBUG_HOLD_FRAMES) {
        return FALSE;
    }

    if (!sDeathRagdollDebugBljActive) {
        if (!death_ragdoll_debug_can_start_blj(m)) {
            return FALSE;
        }
        vec3f_copy(sDeathRagdollDebugBljLockPos, m->pos);
        sDeathRagdollDebugBljActive = TRUE;
        sDeathRagdollDebugBljRepeatTimer = 0;
        sDeathRagdollDebugBljCoastTimer = 0;
        sDeathRagdollDebugZlTapTimer = 0;
        sDeathRagdollDebugZrTapTimer = 0;
    }

    death_ragdoll_debug_sync_mario_pos(m, sDeathRagdollDebugBljLockPos);
    m->input |= INPUT_Z_DOWN;

    if (sDeathRagdollDebugBljRepeatTimer > 0) {
        sDeathRagdollDebugBljRepeatTimer--;
        return TRUE;
    }

    if (m->forwardVel > DEATH_RAGDOLL_DEBUG_BLJ_START_FORWARD_VEL) {
        mario_set_forward_vel(m, DEATH_RAGDOLL_DEBUG_BLJ_START_FORWARD_VEL);
    }
    set_mario_action(m, ACT_LONG_JUMP, 0);

    death_ragdoll_debug_sync_mario_pos(m, sDeathRagdollDebugBljLockPos);
    sDeathRagdollDebugBljRepeatTimer = DEATH_RAGDOLL_DEBUG_BLJ_REPEAT_FRAMES;
    return TRUE;
}

u32 death_ragdoll_debug_update_shortcut(struct MarioState *m) {
    u8 zrPressed = FALSE;
    u8 zlPressed = FALSE;
    u8 zrHeld = FALSE;
    u8 zlHeld = FALSE;

    if (!death_ragdoll_debug_is_enabled()) {
        sDeathRagdollDebugZrTapTimer = 0;
        sDeathRagdollDebugZlTapTimer = 0;
        sDeathRagdollDebugZrHoldTimer = 0;
        death_ragdoll_debug_clear_blj();
        return FALSE;
    }

    m->numLives = 90;

#ifdef TARGET_N3DS
    zrPressed = gDeathRagdollDebugZrPressed;
    zlPressed = gDeathRagdollDebugZlPressed;
    zrHeld = gDeathRagdollDebugZrHeld;
    zlHeld = gDeathRagdollDebugZlHeld;
#endif
    if (m->controller != NULL) {
        zrPressed |= (m->controller->buttonPressed & R_TRIG) != 0;
        zlPressed |= (m->controller->buttonPressed & L_TRIG) != 0;
        zrHeld |= (m->controller->buttonDown & R_TRIG) != 0;
        zlHeld |= (m->controller->buttonDown & L_TRIG) != 0;
    }

    if (sDeathRagdollDebugZrTapTimer > 0) {
        sDeathRagdollDebugZrTapTimer--;
    }
    if (sDeathRagdollDebugZlTapTimer > 0) {
        sDeathRagdollDebugZlTapTimer--;
    }

    if (zrHeld) {
        if (sDeathRagdollDebugZrHoldTimer < DEATH_RAGDOLL_DEBUG_HOLD_FRAMES) {
            sDeathRagdollDebugZrHoldTimer++;
        }
        if (sDeathRagdollDebugZrHoldTimer >= DEATH_RAGDOLL_DEBUG_HOLD_FRAMES) {
            death_ragdoll_debug_full_heal(m);
        }
    } else {
        sDeathRagdollDebugZrHoldTimer = 0;
    }

    if (death_ragdoll_debug_update_blj(m, zlHeld)) {
        return TRUE;
    }

    if (!zrPressed && !zlPressed) {
        return FALSE;
    }

    if (m->action == ACT_DEATH_RAGDOLL || (m->action & ACT_FLAG_INTANGIBLE)
        || m->floor == NULL || m->floor->type == SURFACE_DEATH_PLANE) {
        sDeathRagdollDebugZrTapTimer = 0;
        sDeathRagdollDebugZlTapTimer = 0;
        return FALSE;
    }

    if (zlPressed) {
        if (sDeathRagdollDebugZlTapTimer > 0) {
            sDeathRagdollDebugZlTapTimer = 0;
            m->health = 0x100;
            m->hurtCounter = 0;
            m->healCounter = 0;
            return FALSE;
        }
        sDeathRagdollDebugZlTapTimer = DEATH_RAGDOLL_DEBUG_ZR_DOUBLE_TAP_WINDOW;
    }

    if (!zrPressed) {
        return FALSE;
    }

    if (sDeathRagdollDebugZrTapTimer > 0) {
        sDeathRagdollDebugZrTapTimer = 0;
        return death_ragdoll_start(m, DEATH_RAGDOLL_SOURCE_DEFAULT);
    }

    sDeathRagdollDebugZrTapTimer = DEATH_RAGDOLL_DEBUG_ZR_DOUBLE_TAP_WINDOW;
    return FALSE;
}

s32 act_death_ragdoll(struct MarioState *m) {
    struct Surface *floor;
    f32 floorHeight;
    f32 lowestBodyPoint;
    u8 debugEnabled = death_ragdoll_debug_is_enabled();
    u8 floorValid;
    s32 previousGroundContacts = sDeathRagdollGroundContacts;

    m->marioBodyState->eyeState = MARIO_EYES_DEAD;
    death_ragdoll_freeze_current_animation(m);

    death_ragdoll_cap_velocity(m);
    if (!sDeathRagdollWarpStarted && death_ragdoll_center_out_of_bounds()) {
        return death_ragdoll_begin_death_warp(m);
    }
    death_ragdoll_apply_wall_brake(m);
    sDeathRagdollGroundContacts = 0;
    sDeathRagdollCenterContacts = 0;
    sDeathRagdollMaxPenetration = 0.0f;
    sDeathRagdollGroundImpactSpeed = 0.0f;
    death_ragdoll_step_physics_skeleton(m);
    death_ragdoll_resolve_ground_contact(m);
    death_ragdoll_apply_ledge_drop_spin(m, previousGroundContacts);
    death_ragdoll_update_pose_from_skeleton(m);

    if (!sDeathRagdollWarpStarted && death_ragdoll_center_out_of_bounds()) {
        return death_ragdoll_begin_death_warp(m);
    }
    death_ragdoll_apply_wall_brake(m);
    floorHeight = find_floor(sDeathRagdollCenter[0], sDeathRagdollCenter[1] + 80.0f,
                             sDeathRagdollCenter[2], &floor);
    m->floor = floor;
    m->floorHeight = floorHeight;
    floorValid = floor != NULL && floor->type != SURFACE_DEATH_PLANE;
    if (!sDeathRagdollWarpStarted && !floorValid) {
        return death_ragdoll_begin_death_warp(m);
    }

    if (!sDeathRagdollWarpStarted && floorValid && sDeathRagdollCenter[1] < floorHeight - 240.0f) {
        return death_ragdoll_begin_death_warp(m);
    }

    lowestBodyPoint = death_ragdoll_find_lowest_body_point(m);
    if (floorValid && m->actionState == 0 && lowestBodyPoint <= floorHeight + 20.0f) {
        play_sound_if_no_flag(m, SOUND_MARIO_DYING, MARIO_ACTION_SOUND_PLAYED);
        play_mario_landing_sound_once(m, SOUND_ACTION_TERRAIN_BODY_HIT_GROUND);
        m->actionState = 1;
    }

    sDeathRagdollAngularVel[0] *= 0.985f;
    sDeathRagdollAngularVel[1] *= 0.90f;
    sDeathRagdollAngularVel[2] *= 0.985f;
    death_ragdoll_clamp_body_angles(m);
    death_ragdoll_apply_slope_slide(m);
    death_ragdoll_share_center_inertia(m);
    death_ragdoll_clamp_skeleton_velocity();
    death_ragdoll_sync_mario_to_center(m);

    if (!sDeathRagdollWarpStarted && !debugEnabled && m->actionTimer >= DEATH_RAGDOLL_DURATION) {
        return death_ragdoll_begin_death_warp(m);
    }

    if (!sDeathRagdollWarpStarted && debugEnabled && m->controller != NULL
        && (m->controller->buttonPressed & START_BUTTON)) {
        return death_ragdoll_begin_death_warp(m);
    }

    if (sDeathRagdollWarpStarted
        && m->actionTimer++ >= DEATH_RAGDOLL_DURATION + DEATH_RAGDOLL_FADE_HOLD_FRAMES) {
        return FALSE;
    }
    if (!sDeathRagdollWarpStarted && m->actionTimer < DEATH_RAGDOLL_DURATION) {
        m->actionTimer++;
    }

    return FALSE;
}
