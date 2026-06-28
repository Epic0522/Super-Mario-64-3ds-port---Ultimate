#include "sm64.h"
#include "pc/configfile.h"
#include "enhancements/death_ragdoll.h"
#include "enhancements/hit_ragdoll.h"

#define HIT_RAGDOLL_RECOVER_BACK_START_FRAME    0x2B
#define HIT_RAGDOLL_RECOVER_STOMACH_START_FRAME 0x15

static u8 hit_ragdoll_supports_knockback_action(u32 knockbackAction) {
    switch (knockbackAction) {
        case ACT_HARD_BACKWARD_GROUND_KB:
        case ACT_HARD_FORWARD_GROUND_KB:
        case ACT_BACKWARD_GROUND_KB:
        case ACT_FORWARD_GROUND_KB:
        case ACT_SOFT_BACKWARD_GROUND_KB:
        case ACT_SOFT_FORWARD_GROUND_KB:
        case ACT_HARD_BACKWARD_AIR_KB:
        case ACT_HARD_FORWARD_AIR_KB:
        case ACT_BACKWARD_AIR_KB:
        case ACT_FORWARD_AIR_KB:
            return TRUE;
    }

    return FALSE;
}

static u32 hit_ragdoll_recovery_action_from_knockback(u32 knockbackAction) {
    switch (knockbackAction) {
        case ACT_HARD_FORWARD_GROUND_KB:
        case ACT_FORWARD_GROUND_KB:
        case ACT_SOFT_FORWARD_GROUND_KB:
        case ACT_HARD_FORWARD_AIR_KB:
        case ACT_FORWARD_AIR_KB:
            return ACT_HARD_FORWARD_GROUND_KB;
    }

    return ACT_HARD_BACKWARD_GROUND_KB;
}

static u8 hit_ragdoll_recovery_is_stomach(u32 recoverAction) {
    return recoverAction == ACT_HARD_FORWARD_GROUND_KB;
}

static s16 hit_ragdoll_recovery_start_frame(u32 recoverAction) {
    return hit_ragdoll_recovery_is_stomach(recoverAction) ? HIT_RAGDOLL_RECOVER_STOMACH_START_FRAME
                                                          : HIT_RAGDOLL_RECOVER_BACK_START_FRAME;
}

static s16 hit_ragdoll_recovery_animation(u32 recoverAction) {
    return hit_ragdoll_recovery_is_stomach(recoverAction) ? MARIO_ANIM_LAND_ON_STOMACH
                                                          : MARIO_ANIM_FALL_OVER_BACKWARDS;
}

static u32 hit_ragdoll_random_getup_sound(void) {
    switch (gAudioRandom % 3U) {
        case 0:
            return SOUND_MARIO_EEUH;
        case 1:
            return SOUND_MARIO_UH2;
        default:
            return SOUND_MARIO_WHOA;
    }
}

static s16 hit_ragdoll_getup_sound_delay(u32 recoverAction) {
    return hit_ragdoll_recovery_is_stomach(recoverAction) ? 15 : 0;
}

u32 hit_ragdoll_try_start(struct MarioState *m, struct Object *o, u32 knockbackAction) {
    enum DeathRagdollProfile profile;

    if (!death_ragdoll_enabled || !hit_ragdoll_enabled
        || o == NULL || !hit_ragdoll_supports_knockback_action(knockbackAction)) {
        return FALSE;
    }

    if (m->action & (ACT_FLAG_SWIMMING | ACT_FLAG_METAL_WATER | ACT_FLAG_INTANGIBLE)) {
        return FALSE;
    }

    if (m->floor == NULL || m->floor->type == SURFACE_DEATH_PLANE) {
        return FALSE;
    }

    profile = death_ragdoll_profile_from_object(o);
    if (profile == DEATH_RAGDOLL_PROFILE_DISABLED) {
        return FALSE;
    }

    return death_ragdoll_start_recoverable(
        m,
        death_ragdoll_is_explosion_source(o) ? DEATH_RAGDOLL_SOURCE_EXPLOSION
                                             : DEATH_RAGDOLL_SOURCE_KNOCKBACK,
        profile,
        hit_ragdoll_recovery_action_from_knockback(knockbackAction));
}

s32 act_hit_ragdoll(struct MarioState *m) {
    return death_ragdoll_step_recoverable(m);
}

s32 act_hit_ragdoll_recover(struct MarioState *m) {
    s16 getupSoundDelay = hit_ragdoll_getup_sound_delay(m->actionArg);

    if (m->input & INPUT_OFF_FLOOR) {
        return set_mario_action(m, ACT_FREEFALL, 0);
    }

    if (m->input & INPUT_ABOVE_SLIDE) {
        return set_mario_action(m, ACT_BEGIN_SLIDING, 0);
    }

    stationary_ground_step(m);
    m->marioBodyState->eyeState = MARIO_EYES_CLOSED;
    set_mario_animation(m, hit_ragdoll_recovery_animation(m->actionArg));

    if (m->actionState == 0) {
        set_anim_to_frame(m, hit_ragdoll_recovery_start_frame(m->actionArg));
        m->actionState = 1;
    }

    if (m->actionTimer == getupSoundDelay) {
        play_sound_if_no_flag(m, hit_ragdoll_random_getup_sound(), MARIO_MARIO_SOUND_PLAYED);
    }

    m->actionTimer++;

    if (is_anim_at_end(m)) {
        return set_mario_action(m, ACT_IDLE, 0);
    }

    return FALSE;
}
