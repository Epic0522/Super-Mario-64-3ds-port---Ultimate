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

enum DeathRagdollProfile {
    DEATH_RAGDOLL_PROFILE_DISABLED,
    DEATH_RAGDOLL_PROFILE_DEFAULT,
    DEATH_RAGDOLL_PROFILE_LIGHT_ENEMY,
    DEATH_RAGDOLL_PROFILE_DIRECT_DAMAGE,
    DEATH_RAGDOLL_PROFILE_CHAIN_CHOMP,
    DEATH_RAGDOLL_PROFILE_BOWSER,
    DEATH_RAGDOLL_PROFILE_SHOCK,
    DEATH_RAGDOLL_PROFILE_PROJECTILE,
    DEATH_RAGDOLL_PROFILE_EXPLOSION,
    DEATH_RAGDOLL_PROFILE_FALL_DAMAGE,
    DEATH_RAGDOLL_PROFILE_SQUISH,
};

enum DeathRagdollLimb {
    DEATH_RAGDOLL_LIMB_TORSO,
    DEATH_RAGDOLL_LIMB_HEAD,
    DEATH_RAGDOLL_LIMB_LEFT_UPPER_ARM,
    DEATH_RAGDOLL_LIMB_LEFT_FOREARM,
    DEATH_RAGDOLL_LIMB_LEFT_HAND,
    DEATH_RAGDOLL_LIMB_RIGHT_UPPER_ARM,
    DEATH_RAGDOLL_LIMB_RIGHT_FOREARM,
    DEATH_RAGDOLL_LIMB_RIGHT_HAND,
    DEATH_RAGDOLL_LIMB_LEFT_THIGH,
    DEATH_RAGDOLL_LIMB_LEFT_LEG,
    DEATH_RAGDOLL_LIMB_LEFT_FOOT,
    DEATH_RAGDOLL_LIMB_RIGHT_THIGH,
    DEATH_RAGDOLL_LIMB_RIGHT_LEG,
    DEATH_RAGDOLL_LIMB_RIGHT_FOOT,
    DEATH_RAGDOLL_LIMB_COUNT,
};

u8 death_ragdoll_damage_would_kill(struct MarioState *m);
u8 death_ragdoll_is_explosion_source(struct Object *o);
enum DeathRagdollProfile death_ragdoll_profile_from_object(struct Object *o);
u8 death_ragdoll_debug_is_enabled(void);
void death_ragdoll_debug_set_enabled(u8 enabled);
u8 death_ragdoll_debug_blj_is_active(void);
u8 death_ragdoll_debug_blj_is_coasting(void);
u32 death_ragdoll_start(struct MarioState *m, enum DeathRagdollSource source);
u32 death_ragdoll_start_recoverable(struct MarioState *m, enum DeathRagdollSource source,
                                    enum DeathRagdollProfile profile, u32 recoverAction);
u32 death_ragdoll_start_with_profile(struct MarioState *m, enum DeathRagdollSource source,
                                     enum DeathRagdollProfile profile);
u32 death_ragdoll_try_start_from_health_depleted(struct MarioState *m);
u32 death_ragdoll_debug_update_shortcut(struct MarioState *m);
u8 death_ragdoll_consume_death_warp_spawn(void);
s32 act_death_ragdoll(struct MarioState *m);
s32 death_ragdoll_step_recoverable(struct MarioState *m);
Gfx *death_ragdoll_geo_rotate_limb(s32 callContext, struct GraphNode *node, UNUSED Mat4 *mtx);
Gfx *death_ragdoll_geo_switch_visual(s32 callContext, struct GraphNode *node, UNUSED Mat4 *mtx);
Gfx *death_ragdoll_geo_render(s32 callContext, struct GraphNode *node, UNUSED Mat4 *mtx);

#endif
