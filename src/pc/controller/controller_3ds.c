#ifdef TARGET_N3DS

// hack for redefinition of types in libctru
// All 3DS includes must be done inside of an equivalent
// #define/undef block to avoid type redefinition issues.
#define u64 __3ds_u64
#define s64 __3ds_s64
#define u32 __3ds_u32
#define vu32 __3ds_vu32
#define vs32 __3ds_vs32
#define s32 __3ds_s32
#define u16 __3ds_u16
#define s16 __3ds_s16
#define u8 __3ds_u8
#define s8 __3ds_s8
#include <3ds/types.h>
#include <3ds.h>
#undef u64
#undef s64
#undef u32
#undef vu32
#undef vs32
#undef s32
#undef u16
#undef s16
#undef u8
#undef s8

#include <ultra64.h>

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "controller_api.h"

#include "../configfile.h"

#include <include/types.h>
#include <enhancements/puppycam.h>
#include <enhancements/death_ragdoll.h>

s16 rightstick[2];
u8 camera_mode_button_pressed;
u8 camera_recenter_button_pressed;
u8 level_select_button_pressed;
u8 gDeathRagdollDebugZrPressed;
u8 gDeathRagdollDebugZlPressed;
u8 gDeathRagdollDebugZrHeld;
u8 gDeathRagdollDebugZlHeld;
u8 gN3dsPhysicalStartPressed;
u8 gN3dsPhysicalStartHeld;

static int button_mapping[10][2];
static u32 camera_control_keys_prev;
static const s16 CSTICK_DEADZONE = 6;

static s16 apply_cstick_deadzone(s16 value)
{
    if (value > -CSTICK_DEADZONE && value < CSTICK_DEADZONE) {
        return 0;
    }

    return value;
}

static void set_button_mapping(int index, int mask_n64, int mask_3ds)
{
    button_mapping[index][0] = mask_3ds;
    button_mapping[index][1] = mask_n64;
}

static u32 controller_3ds_get_held(void)
{
    u32 res = 0;
    u32 kHeld = keysHeld();
    u32 kDown = kHeld & ~(KEY_X | KEY_Y);
    u32 kPressed = kHeld & ~camera_control_keys_prev;
    const u32 levelSelectCombo = KEY_SELECT | KEY_ZL | KEY_ZR;
    bool debugEnabled = death_ragdoll_debug_is_enabled();
    bool levelSelectRequested = debugEnabled && (kHeld & levelSelectCombo) == levelSelectCombo;

    gDeathRagdollDebugZrPressed = (kPressed & KEY_ZR) != 0;
    gDeathRagdollDebugZlPressed = (kPressed & KEY_ZL) != 0;
    gDeathRagdollDebugZrHeld = (kHeld & KEY_ZR) != 0;
    gDeathRagdollDebugZlHeld = (kHeld & KEY_ZL) != 0;
    gN3dsPhysicalStartPressed = (kPressed & KEY_START) != 0;
    gN3dsPhysicalStartHeld = (kHeld & KEY_START) != 0;
    camera_control_keys_prev = kHeld;
    if (levelSelectRequested) {
        level_select_button_pressed |= (kPressed & (KEY_ZL | KEY_ZR | KEY_SELECT)) != 0;
        kDown &= ~(KEY_SELECT | KEY_ZL | KEY_ZR);
    } else {
        camera_mode_button_pressed |= (kPressed & KEY_X) != 0;
        camera_recenter_button_pressed |= (kPressed & KEY_Y) != 0;
    }
    for (size_t i = 0; i < sizeof(button_mapping) / sizeof(button_mapping[0]); i++)
    {
        if (button_mapping[i][0] & kDown) {
            res |= button_mapping[i][1];
        }
    }

    if (!newcam_active) {
        if (rightstick[0] < -20)
            res |= L_CBUTTONS;
        if (rightstick[0] > 20)
            res |= R_CBUTTONS;
        if (rightstick[1] > 20)
            res |= U_CBUTTONS;
        if (rightstick[1] < -20)
            res |= D_CBUTTONS;
    }

    return res;
}

static void controller_3ds_init(void)
{
    u32 i = 0;
    set_button_mapping(i++, A_BUTTON,     configKeyA); // n64 button => configured button
    set_button_mapping(i++, B_BUTTON,     configKeyB);
    set_button_mapping(i++, START_BUTTON, configKeyStart);
    set_button_mapping(i++, L_TRIG,       configKeyL);
    set_button_mapping(i++, R_TRIG,       configKeyR);
    set_button_mapping(i++, Z_TRIG,       configKeyZ);
    set_button_mapping(i++, U_CBUTTONS,   configKeyCUp);
    set_button_mapping(i++, D_CBUTTONS,   configKeyCDown);
    set_button_mapping(i++, L_CBUTTONS,   configKeyCLeft);
    set_button_mapping(i++, R_CBUTTONS,   configKeyCRight);
}

static void controller_3ds_read(OSContPad *pad)
{
    hidScanInput();

    circlePosition circlePad;
    hidCircleRead(&circlePad);
    pad->stick_x = circlePad.dx / 2;
    pad->stick_y = circlePad.dy / 2;

    circlePosition cStick;
    hidCstickRead(&cStick);
    rightstick[0] = apply_cstick_deadzone(cStick.dx);
    rightstick[1] = apply_cstick_deadzone(cStick.dy);

    pad->button = controller_3ds_get_held();

    if (rightstick[0] == 0 && rightstick[1] == 0) {
        newcam_analogue = 0;
    } else {
        newcam_analogue = 1;
    }
}

struct ControllerAPI controller_3ds = {
    controller_3ds_init,
    controller_3ds_read
};

#endif
