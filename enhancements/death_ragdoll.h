#ifndef DEATH_RAGDOLL_H
#define DEATH_RAGDOLL_H

#include <PR/ultratypes.h>

#include "types.h"

struct GraphNode;

enum DeathRagdollSource {
    DEATH_RAGDOLL_SOURCE_DEFAULT,
    DEATH_RAGDOLL_SOURCE_KNOCKBACK,
    DEATH_RAGDOLL_SOURCE_EXPLOSION,
};

enum DeathRagdollLimb {
    DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM,
    DEATH_RAGDOLL_LIMB_LEFT_FOREARM,
    DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM,
    DEATH_RAGDOLL_LIMB_RIGHT_FOREARM,
    DEATH_RAGDOLL_LIMB_LEFT_THIGH,
    DEATH_RAGDOLL_LIMB_LEFT_LEG,
    DEATH_RAGDOLL_LIMB_RIGHT_THIGH,
    DEATH_RAGDOLL_LIMB_RIGHT_LEG,
};

u8 death_ragdoll_damage_would_kill(struct MarioState *m);
u8 death_ragdoll_is_explosion_source(struct Object *o);
u32 death_ragdoll_start(struct MarioState *m, enum DeathRagdollSource source);
u32 death_ragdoll_try_start_from_health_depleted(struct MarioState *m);
s32 act_death_ragdoll(struct MarioState *m);
Gfx *death_ragdoll_geo_rotate_limb(s32 callContext, struct GraphNode *node, UNUSED Mat4 *mtx);

#endif
