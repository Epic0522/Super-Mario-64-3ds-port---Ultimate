#ifndef HIT_RAGDOLL_H
#define HIT_RAGDOLL_H

#include <PR/ultratypes.h>

#include "types.h"

u32 hit_ragdoll_try_start(struct MarioState *m, struct Object *o, u32 knockbackAction);
s32 act_hit_ragdoll(struct MarioState *m);
s32 act_hit_ragdoll_recover(struct MarioState *m);

#endif
