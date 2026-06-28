#ifndef CONTROLLER_3DS_H
#define CONTROLLER_3DS_H

#include "controller_api.h"

extern struct ControllerAPI controller_3ds;
extern s16 rightstick[2];
extern u8 camera_mode_button_pressed;
extern u8 camera_recenter_button_pressed;
extern u8 level_select_button_pressed;
extern u8 gN3dsPhysicalStartPressed;
extern u8 gN3dsPhysicalStartHeld;
extern u8 gDeathRagdollDebugZrHeld;
extern u8 gDeathRagdollDebugZlHeld;

#endif
