#ifndef LEVEL_SELECT_MENU_H
#define LEVEL_SELECT_MENU_H

#include <PR/ultratypes.h>

#include "macros.h"

s32 lvl_intro_update(s16 arg1, UNUSED s32 arg2);

#ifdef TARGET_N3DS
extern s16 gN3dsIntroScreenMode;
extern s16 gN3dsIntroScreenExiting;
extern u32 gN3dsIntroScreenFadeStart;
#endif

#endif // LEVEL_SELECT_MENU_H
