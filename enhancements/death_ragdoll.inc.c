#include "sm64.h"
#include "audio/external.h"
#include "behavior_data.h"
#include "game/camera.h"
#include "engine/math_util.h"
#include "engine/graph_node.h"
#include "engine/surface_collision.h"
#include "game/level_update.h"
#include "game/interaction.h"
#include "game/mario.h"
#include "game/mario_step.h"
#include "object_fields.h"
#include "enhancements/death_ragdoll.h"

#define DEATH_RAGDOLL_DURATION 60
#define DEATH_RAGDOLL_MAX_HORIZONTAL_SPEED 42.0f
#define DEATH_RAGDOLL_MAX_UPWARD_SPEED 46.0f
#define DEATH_RAGDOLL_MAX_FALL_SPEED -58.0f
#define DEATH_RAGDOLL_LIMB_DECAY_START 34

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

static s16 death_ragdoll_limb_decay(s16 value, s32 timer) {
    s32 remaining;

    if (timer <= DEATH_RAGDOLL_LIMB_DECAY_START) {
        return value;
    }

    remaining = DEATH_RAGDOLL_DURATION - timer;
    if (remaining < 0) {
        remaining = 0;
    }

    return value * remaining / (DEATH_RAGDOLL_DURATION - DEATH_RAGDOLL_LIMB_DECAY_START);
}

static void death_ragdoll_cap_velocity(struct MarioState *m) {
    f32 speed = sqrtf(m->vel[0] * m->vel[0] + m->vel[2] * m->vel[2]);

    if (speed > DEATH_RAGDOLL_MAX_HORIZONTAL_SPEED) {
        f32 scale = DEATH_RAGDOLL_MAX_HORIZONTAL_SPEED / speed;
        m->vel[0] *= scale;
        m->vel[2] *= scale;
        m->forwardVel *= scale;
    }

    m->vel[1] = death_ragdoll_clampf(m->vel[1], DEATH_RAGDOLL_MAX_FALL_SPEED,
                                     DEATH_RAGDOLL_MAX_UPWARD_SPEED);
}

Gfx *death_ragdoll_geo_rotate_limb(s32 callContext, struct GraphNode *node, UNUSED Mat4 *mtx) {
    struct GraphNodeGenerated *asGenerated = (struct GraphNodeGenerated *) node;
    struct GraphNodeRotation *rotNode;
    struct MarioState *m = gMarioState;
    s32 timer;
    s32 limb;
    f32 impact;
    s16 swing;
    s16 drag;

    if (callContext != GEO_CONTEXT_RENDER || node->next == NULL) {
        return NULL;
    }

    rotNode = (struct GraphNodeRotation *) node->next;
    vec3s_set(rotNode->rotation, 0, 0, 0);

    if (m == NULL || m->action != ACT_DEATH_RAGDOLL) {
        return NULL;
    }

    timer = m->actionTimer;
    limb = asGenerated->parameter;
    impact = death_ragdoll_clampf((death_ragdoll_absf(m->forwardVel) + death_ragdoll_absf(m->vel[1])) / 64.0f,
                                  0.35f, 1.0f);
    swing = sins(timer * 0x720 + limb * 0x1555) * 0x1550 * impact;
    drag = coss(timer * 0x560 + limb * 0x1111) * 0x0A00 * impact;

    switch (limb) {
        case DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM:
            rotNode->rotation[0] = death_ragdoll_limb_decay(-0x2800 + swing, timer);
            rotNode->rotation[1] = death_ragdoll_limb_decay( 0x0A00 + drag, timer);
            rotNode->rotation[2] = death_ragdoll_limb_decay( 0x1400 + swing / 2, timer);
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_FOREARM:
            rotNode->rotation[0] = death_ragdoll_limb_decay( 0x3000 - swing / 2, timer);
            rotNode->rotation[1] = death_ragdoll_limb_decay(-0x0800 + drag / 2, timer);
            rotNode->rotation[2] = death_ragdoll_limb_decay( 0x1000 + swing / 3, timer);
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM:
            rotNode->rotation[0] = death_ragdoll_limb_decay(-0x2600 - swing, timer);
            rotNode->rotation[1] = death_ragdoll_limb_decay(-0x0A00 + drag, timer);
            rotNode->rotation[2] = death_ragdoll_limb_decay(-0x1400 + swing / 2, timer);
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_FOREARM:
            rotNode->rotation[0] = death_ragdoll_limb_decay( 0x3000 + swing / 2, timer);
            rotNode->rotation[1] = death_ragdoll_limb_decay( 0x0800 + drag / 2, timer);
            rotNode->rotation[2] = death_ragdoll_limb_decay(-0x1000 + swing / 3, timer);
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_THIGH:
            rotNode->rotation[0] = death_ragdoll_limb_decay( 0x1800 - swing / 2, timer);
            rotNode->rotation[1] = death_ragdoll_limb_decay( 0x0500 + drag / 3, timer);
            rotNode->rotation[2] = death_ragdoll_limb_decay( 0x0800 + swing / 4, timer);
            break;
        case DEATH_RAGDOLL_LIMB_LEFT_LEG:
            rotNode->rotation[0] = death_ragdoll_limb_decay(-0x2200 + swing / 2, timer);
            rotNode->rotation[1] = death_ragdoll_limb_decay(-0x0400 + drag / 4, timer);
            rotNode->rotation[2] = death_ragdoll_limb_decay( 0x0600 - swing / 5, timer);
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_THIGH:
            rotNode->rotation[0] = death_ragdoll_limb_decay( 0x1600 + swing / 2, timer);
            rotNode->rotation[1] = death_ragdoll_limb_decay(-0x0500 + drag / 3, timer);
            rotNode->rotation[2] = death_ragdoll_limb_decay(-0x0800 + swing / 4, timer);
            break;
        case DEATH_RAGDOLL_LIMB_RIGHT_LEG:
            rotNode->rotation[0] = death_ragdoll_limb_decay(-0x2200 - swing / 2, timer);
            rotNode->rotation[1] = death_ragdoll_limb_decay( 0x0400 + drag / 4, timer);
            rotNode->rotation[2] = death_ragdoll_limb_decay(-0x0600 - swing / 5, timer);
            break;
    }

    return NULL;
}

static void death_ragdoll_apply_default_impulse(struct MarioState *m) {
    s16 yaw = m->faceAngle[1] + 0x8000;

    mario_set_forward_vel(m, 18.0f);
    m->faceAngle[1] = yaw;
    m->vel[0] = sins(yaw) * m->forwardVel;
    m->vel[2] = coss(yaw) * m->forwardVel;
    m->vel[1] = 22.0f;
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

u32 death_ragdoll_start(struct MarioState *m, enum DeathRagdollSource source) {
    mario_stop_riding_and_holding(m);

    if (m->forwardVel == 0.0f && m->vel[0] == 0.0f && m->vel[2] == 0.0f) {
        death_ragdoll_apply_default_impulse(m);
    } else {
        m->vel[0] = sins(m->faceAngle[1]) * m->forwardVel;
        m->vel[2] = coss(m->faceAngle[1]) * m->forwardVel;
        if (m->vel[1] < 16.0f) {
            m->vel[1] = 16.0f;
        }
    }

    if (source == DEATH_RAGDOLL_SOURCE_EXPLOSION) {
        m->vel[0] *= 1.25f;
        m->vel[2] *= 1.25f;
        m->forwardVel *= 1.25f;
        m->vel[1] += 18.0f;
    } else if (source == DEATH_RAGDOLL_SOURCE_KNOCKBACK && m->vel[1] < 24.0f) {
        m->vel[1] = 24.0f;
    }

    death_ragdoll_cap_velocity(m);
    m->health = 0xFF;
    m->hurtCounter = 0;
    m->healCounter = 0;
    m->invincTimer = 0;

    play_sound_if_no_flag(m, SOUND_MARIO_DYING, MARIO_ACTION_SOUND_PLAYED);
    return set_mario_action(m, ACT_DEATH_RAGDOLL, source);
}

u32 death_ragdoll_try_start_from_health_depleted(struct MarioState *m) {
    if (m->health >= 0x100 || m->action == ACT_DEATH_RAGDOLL || (m->action & ACT_FLAG_INTANGIBLE)) {
        return FALSE;
    }

    if (m->floor == NULL || (m->floor != NULL && m->floor->type == SURFACE_DEATH_PLANE)) {
        return FALSE;
    }

    return death_ragdoll_start(m, DEATH_RAGDOLL_SOURCE_DEFAULT);
}

s32 act_death_ragdoll(struct MarioState *m) {
    s32 stepResult;
    s16 tumble = m->actionTimer * 0x620;

    play_sound_if_no_flag(m, SOUND_MARIO_DYING, MARIO_ACTION_SOUND_PLAYED);
    set_mario_animation(m, MARIO_ANIM_A_POSE);
    m->marioBodyState->eyeState = MARIO_EYES_DEAD;

    death_ragdoll_cap_velocity(m);
    stepResult = perform_air_step(m, 0);

    if (stepResult == AIR_STEP_HIT_WALL) {
        mario_bonk_reflection(m, FALSE);
        m->vel[0] *= 0.45f;
        m->vel[2] *= 0.45f;
        m->forwardVel *= 0.45f;
    } else if (stepResult == AIR_STEP_HIT_LAVA_WALL || m->floor == NULL) {
        level_trigger_warp(m, WARP_OP_DEATH);
        return set_mario_action(m, ACT_DISAPPEARED, 0);
    } else if (stepResult == AIR_STEP_LANDED) {
        play_mario_landing_sound_once(m, SOUND_ACTION_TERRAIN_BODY_HIT_GROUND);
        if (m->actionState == 0) {
            m->vel[1] = 12.0f;
            m->actionState = 1;
        } else {
            m->vel[1] = 0.0f;
        }
        m->vel[0] *= 0.50f;
        m->vel[2] *= 0.50f;
        m->forwardVel *= 0.50f;
    }

    if (m->floor != NULL && m->floor->type == SURFACE_DEATH_PLANE) {
        level_trigger_warp(m, WARP_OP_DEATH);
        return set_mario_action(m, ACT_DISAPPEARED, 0);
    }

    m->marioObj->header.gfx.angle[0] = 0x1800 + sins(tumble) * 0x1800;
    m->marioObj->header.gfx.angle[1] = m->faceAngle[1] + tumble;
    m->marioObj->header.gfx.angle[2] = coss(tumble / 2) * 0x1200;

    if (m->actionTimer++ >= DEATH_RAGDOLL_DURATION) {
        level_trigger_warp(m, WARP_OP_DEATH);
        return set_mario_action(m, ACT_DISAPPEARED, 0);
    }

    return FALSE;
}
