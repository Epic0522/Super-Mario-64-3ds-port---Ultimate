#include "sm64.h"
#include "game/camera.h"
#include "game/level_update.h"
#include "game/print.h"
#include "engine/math_util.h"
#include "game/segment2.h"
#include "game/save_file.h"
#include "puppycam.h"
#include "include/text_strings.h"
#include "puppycam_collision.inc.c"
#include "gfx_dimensions.h"
#include "pc/controller/controller_3ds.h"
#include "pc/configfile.h"

/**
Quick explanation of the camera modes

NC_MODE_NORMAL: Standard mode, allows dualaxial movement and free control of the camera.
NC_MODE_FIXED: Disables control of camera, and the actual position of the camera doesn't update.
NC_MODE_2D: Disables horizontal control of the camera and locks Mario's direction to the X axis. NYI though.
NC_MODE_8D: 8 directional movement. Similar to standard, except the camera direction snaps to 8 directions.
NC_MODE_FIXED_NOMOVE: Disables control and movement of the camera.
NC_MODE_NOTURN: Disables horizontal and vertical control of the camera. Perfect for when running scripts alongside the camera angle.
**/

//!A bunch of developer intended options, to cover every base, really.
//#define NEWCAM_DEBUG //Some print values for puppycam. Not useful anymore, but never hurts to keep em around.
//#define nosound //If for some reason you hate the concept of audio, you can disable it.
//#define noaccel //Disables smooth movement of the camera with the C buttons.
//#define EXT_BOUNDS

#ifdef EXT_BOUNDS
    #define MULTI 4.0f
#endif // EXT_BOUNDS


//!Hardcoded camera angle stuff. They're essentially area boxes that when Mario is inside, will trigger some view changes.
///Don't touch this btw, unless you know what you're doing, this has to be above for religious reasons.
struct newcam_hardpos
{
    u8 newcam_hard_levelID;
    u8 newcam_hard_areaID;
    u8 newcam_hard_permaswap;
    u16 newcam_hard_modeset;
    void (*newcam_hard_script)(void);
    s16 newcam_hard_X1;
    s16 newcam_hard_Y1;
    s16 newcam_hard_Z1;
    s16 newcam_hard_X2;
    s16 newcam_hard_Y2;
    s16 newcam_hard_Z2;
    s16 newcam_hard_camX;
    s16 newcam_hard_camY;
    s16 newcam_hard_camZ;
    s16 newcam_hard_lookX;
    s16 newcam_hard_lookY;
    s16 newcam_hard_lookZ;
};

#include "puppycam_scripts.inc.c"
#include "puppycam_angles.inc.c"

#ifdef noaccel
    u8 accel = 255;
    #else
    u8 accel = 10;
#endif // noaccel

#ifndef TARGET_N64
u8 mousemode = 0;
s16 mousepos[2]; //The current position of the mouse.
s16 mouselock[2]; //Where the mouse was when it got locked by controller. Moving the mouse 4 pixels away will unlock the mouse again.
#endif // TARGET_N64

s16 newcam_yaw; //Z axis rotation
f32 newcam_yaw_acc;
s16 newcam_tilt = 1500; //Y axis rotation
f32 newcam_tilt_acc;
u16 newcam_distance = 750; //The distance the camera stays from the player
u16 newcam_distance_target = 750; //The distance the player camera tries to reach.
f32 newcam_pos_target[3]; //The position the camera is basing calculations off. *usually* Mario.
f32 newcam_pos[3]; //Position the camera is in the world
f32 newcam_lookat[3]; //Position the camera is looking at
f32 newcam_framessincec[2];
f32 newcam_extheight = 125;
u8 newcam_centering = 0; // The flag that depicts whether the camera's going to try centreing.
s16 newcam_yaw_target; // The yaw value the camera tries to set itself to when the centre flag is active. Is set to Mario's face angle.
f32 newcam_turnwait; // The amount of time to wait after landing before allowing the camera to turn again
f32 newcam_pan_x;
f32 newcam_pan_z;
u8 newcam_cstick_down = 0; //Just a value that triggers true when the player 2 stick is moved in 8 direction move to prevent holding it down.
u8 newcam_target;
s32 newcam_sintimer = 0;
s16 newcam_coldist;
u8 newcam_xlu = 255;
s8 newcam_stick2[2];
s16 newcam_yaw_offset = 0;
s16 newcam_pitch_offset = 0;

s16 newcam_sensitivityX; //How quick the camera works.
s16 newcam_sensitivityY;
s16 newcam_invertX; //Reverses movement of the camera axis.
s16 newcam_invertY;
s16 newcam_panlevel; //How much the camera sticks out a bit in the direction you're looking.
s16 newcam_aggression ; //How much the camera tries to centre itself to Mario's facing and movement.
s16 newcam_degrade ;
s16 newcam_analogue = 0; //Whether to accept inputs from a player 2 joystick, and then disables C button input.
s16 newcam_distance_values[] = {750,1250,2000};
u8 newcam_active = 0; // Free camera mode. When disabled, the native SM64 camera owns the view.
u16 newcam_mode;
u16 newcam_intendedmode = 0; // which camera mode the camera's going to try to be in when not forced into another.
u16 newcam_modeflags;

#define NEWCAM_HINT_NONE 0
#define NEWCAM_HINT_SOFT 1
#define NEWCAM_HINT_HARD 2
#define NEWCAM_ANALOG_DEADZONE 8

u8 newcam_official_hint_active = 0;
Vec3f newcam_official_pos;
Vec3f newcam_official_lookat;
u8 newcam_recenter_behind_mario = 0;
u8 newcam_base_angle_locked = 0;
f32 newcam_base_distance = 0;
s16 newcam_base_pitch = 0;
s16 newcam_base_yaw = 0;

u8 newcam_override = 0;
u8 newcam_override_timer = 0;
static const u8 NEWCAM_OVERRIDE_HOLDTIME = 10;

u8 newcam_option_open = 0;
s8 newcam_option_selection = 0;
f32 newcam_option_timer = 0;
u8 newcam_option_index = 0;
u8 newcam_option_scroll = 0;
u8 newcam_option_scroll_last = 0;

#ifndef NC_CODE_NOMENU
#if defined(VERSION_EU)
static u8 newcam_options_fr[][64] = {{NC_ANALOGUE_FR}, {NC_CAMX_FR}, {NC_CAMY_FR}, {NC_INVERTX_FR}, {NC_INVERTY_FR}, {NC_CAMC_FR}, {NC_CAMP_FR}, {NC_CAMD_FR}};
static u8 newcam_options_de[][64] = {{NC_ANALOGUE_DE}, {NC_CAMX_DE}, {NC_CAMY_DE}, {NC_INVERTX}, {NC_INVERTY_DE}, {NC_CAMC_DE}, {NC_CAMP_DE}, {NC_CAMD_DE}};
static u8 newcam_flags_fr[][64] = {{NC_DISABLED_FR}, {NC_ENABLED_FR}};
static u8 newcam_flags_de[][64] = {{NC_DISABLED_DE}, {NC_ENABLED_DE}};
static u8 newcam_strings_fr[][64] = {{NC_BUTTON_FR}, {NC_BUTTON2_FR}, {NC_OPTION_FR}, {NC_HIGHLIGHT_L_FR}, {NC_HIGHLIGHT_R_FR}};
static u8 newcam_strings_de[][64] = {{NC_BUTTON_DE}, {NC_BUTTON2_DE}, {NC_OPTION_DE}, {NC_HIGHLIGHT_L_DE}, {NC_HIGHLIGHT_R_DE}};
#endif
static u8 newcam_options[][64] = {{NC_ANALOGUE}, {NC_CAMX}, {NC_CAMY}, {NC_INVERTX}, {NC_INVERTY}, {NC_CAMC}, {NC_CAMP}, {NC_CAMD}};
static u8 newcam_flags[][64] = {{NC_DISABLED}, {NC_ENABLED}};
static u8 newcam_strings[][64] = {{NC_BUTTON}, {NC_BUTTON2}, {NC_OPTION}, {NC_HIGHLIGHT_L}, {NC_HIGHLIGHT_R}};
#endif

static u8 (*newcam_options_ptr)[64] = newcam_options;
static u8 (*newcam_flags_ptr)[64] = newcam_flags;
static u8 (*newcam_strings_ptr)[64] = newcam_strings;


struct newcam_optionstruct
{
    u8 newcam_op_name; //This is the position in the newcam_options text array. It doesn't have to directly correlate with its position in the struct
    s16 *newcam_op_var; //This is the value that the option is going to directly affect.
    u8 newcam_op_option_start; //This is where the text array will start. Set it to 255 to have it be ignored.
    s32 newcam_op_min; //The minimum value of the option.
    s32 newcam_op_max; //The maximum value of the option.
};

static struct newcam_optionstruct newcam_optionvar[]=
{ //If the min and max are 0 and 1, then the value text is used, otherwise it's ignored.
    //No point this existing on hardware you natively get double analogue sticks.
    #ifdef TARGET_N64
    {/*Option Name*/ 0, /*Option Variable*/ &newcam_analogue, /*Option Value Text Start*/ 0, /*Option Minimum*/ FALSE, /*Option Maximum*/ TRUE},
    #endif
    {/*Option Name*/ 1, /*Option Variable*/ &newcam_sensitivityX, /*Option Value Text Start*/ 255, /*Option Minimum*/ 10, /*Option Maximum*/ 500},
    {/*Option Name*/ 2, /*Option Variable*/ &newcam_sensitivityY, /*Option Value Text Start*/ 255, /*Option Minimum*/ 10, /*Option Maximum*/ 500},
    {/*Option Name*/ 3, /*Option Variable*/ &newcam_invertX, /*Option Value Text Start*/ 0, /*Option Minimum*/ FALSE, /*Option Maximum*/ TRUE},
    {/*Option Name*/ 4, /*Option Variable*/ &newcam_invertY, /*Option Value Text Start*/ 0, /*Option Minimum*/ FALSE, /*Option Maximum*/ TRUE},
    {/*Option Name*/ 7, /*Option Variable*/ &newcam_degrade, /*Option Value Text Start*/ 255, /*Option Minimum*/ 5, /*Option Maximum*/ 100},
    {/*Option Name*/ 5, /*Option Variable*/ &newcam_aggression, /*Option Value Text Start*/ 255, /*Option Minimum*/ 0, /*Option Maximum*/ 100},
    {/*Option Name*/ 6, /*Option Variable*/ &newcam_panlevel, /*Option Value Text Start*/ 255, /*Option Minimum*/ 0, /*Option Maximum*/ 100},
};

//You no longer need to edit this anymore lol
u8 newcam_total = sizeof(newcam_optionvar) / sizeof(struct newcam_optionstruct); //How many options there are in newcam_uptions.

#if defined(VERSION_EU)
static void newcam_set_language(void)
{
    switch (eu_get_language())
    {
    case 0:
        newcam_options_ptr = newcam_options;
        newcam_flags_ptr = newcam_flags;
        newcam_strings_ptr = newcam_strings;
        break;
    case 1:
        newcam_options_ptr = newcam_options_fr;
        newcam_flags_ptr = newcam_flags_fr;
        newcam_strings_ptr = newcam_strings_fr;
        break;
    case 2:
        newcam_options_ptr = newcam_options_de;
        newcam_flags_ptr = newcam_flags_de;
        newcam_strings_ptr = newcam_strings_de;
        break;
    }
}
#endif

///This is called at every level initialisation.
void newcam_init(struct Camera *c, u8 dv)
{
    #if defined(VERSION_EU)
    newcam_set_language();
    #endif
    newcam_tilt = 1500;
    newcam_distance_target = newcam_distance_values[dv];
    newcam_yaw = -c->yaw+0x4000; //Mario and the camera's yaw have this offset between them.
    newcam_mode = NC_MODE_NORMAL;
    ///This here will dictate what modes the camera will start in at the beginning of a level. Below are some examples.
    switch (gCurrLevelNum)
    {
        case LEVEL_BITDW: newcam_yaw = 0x4000; newcam_mode = NC_MODE_8D; newcam_tilt = 4000; newcam_distance_target = newcam_distance_values[2]; break;
        case LEVEL_BITFS: newcam_yaw = 0x4000; newcam_mode = NC_MODE_8D; newcam_tilt = 4000; newcam_distance_target = newcam_distance_values[2]; break;
        case LEVEL_BITS: newcam_yaw = 0x4000; newcam_mode = NC_MODE_8D; newcam_tilt = 4000; newcam_distance_target = newcam_distance_values[2]; break;
        case LEVEL_WF: newcam_yaw = 0x4000; newcam_tilt = 2000; newcam_distance_target = newcam_distance_values[1]; break;
        case LEVEL_RR: newcam_yaw = 0x6000; newcam_tilt = 2000; newcam_distance_target = newcam_distance_values[2]; break;
        case LEVEL_CCM: if (gCurrAreaIndex == 1) {newcam_yaw = -0x4000; newcam_tilt = 2000; newcam_distance_target = newcam_distance_values[1];} else newcam_mode = NC_MODE_SLIDE; break;
        case LEVEL_WDW: newcam_yaw = 0x2000; newcam_tilt = 3000; newcam_distance_target = newcam_distance_values[1]; break;
        case 27: newcam_mode = NC_MODE_SLIDE; break;
        case LEVEL_TTM: if (gCurrAreaIndex == 2) newcam_mode = NC_MODE_SLIDE; break;
    }

    newcam_distance = newcam_distance_target;
    newcam_intendedmode = newcam_mode;
    newcam_modeflags = newcam_mode;
    newcam_yaw_offset = 0;
    newcam_pitch_offset = 0;
    newcam_override_timer = 0;
}

static s16 newcam_clamp(s16 value, s16 max, s16 min)
{
    if (value >= max)
        return max;
    else
    if (value <= min)
        return min;
    else
        return value;
}

///These are the default settings for Puppycam. You may change them to change how they'll be set for first timers.
void newcam_init_settings()
{
#ifdef TARGET_N64
#ifndef NC_CODE_NOSAVE
    if (save_check_firsttime())
    {
        save_file_get_setting();
        newcam_sensitivityX = newcam_clamp(newcam_sensitivityX, 500, 10);
        newcam_sensitivityY = newcam_clamp(newcam_sensitivityY, 500, 10);
        newcam_aggression = newcam_clamp(newcam_aggression, 100, 0);
        newcam_panlevel = newcam_clamp(newcam_panlevel, 100, 0);
        newcam_invertX = newcam_clamp(newcam_invertX, 1, 0);
        newcam_invertY = newcam_clamp(newcam_invertY, 1, 0);
        newcam_degrade = newcam_clamp(newcam_degrade, 100, 10);
    }
    else
    {
#endif
        newcam_sensitivityX = 75;
        newcam_sensitivityY = 75;
        newcam_aggression = 35;
        newcam_panlevel = 75;
        newcam_invertX = 0;
        newcam_invertY = 0;
        newcam_degrade = 10;
        save_set_firsttime();
#ifndef NC_CODE_NOSAVE
    }
    #endif
    #else
    newcam_sensitivityX = puppycam_sensitivityX;
    newcam_sensitivityY = puppycam_sensitivityY;
    newcam_aggression = puppycam_aggression;
    newcam_panlevel = puppycam_panlevel;
    newcam_invertX = puppycam_invertX;
    newcam_invertY = puppycam_invertY;
    newcam_degrade = puppycam_degrade;

    newcam_analogue = 1;
    #endif // TARGET_N64
}

/** Mathematic calculations. This stuffs so basic even *I* understand it lol
Basically, it just returns a position based on angle */
static s16 lengthdir_x(f32 length, s16 dir)
{
    return (s16) (length * coss(dir));
}
static s16 lengthdir_y(f32 length, s16 dir)
{
    return (s16) (length * sins(dir));
}

void newcam_diagnostics(void)
{
    print_text_fmt_int(32,192,"L %d",gCurrLevelNum);
    print_text_fmt_int(32,176,"Area %d",gCurrAreaIndex);
    print_text_fmt_int(32,160,"1 %d",gMarioState->pos[0]);
    print_text_fmt_int(32,144,"2 %d",gMarioState->pos[1]);
    print_text_fmt_int(32,128,"3 %d",gMarioState->pos[2]);
    print_text_fmt_int(32,112,"FLAGS %d",newcam_modeflags);
    print_text_fmt_int(180,112,"INTM %d",newcam_intendedmode);
    print_text_fmt_int(32,96,"TILT UP %d",newcam_tilt_acc);
    print_text_fmt_int(32,80,"YAW UP %d",newcam_yaw_acc);
    print_text_fmt_int(32,64,"YAW %d",newcam_yaw);
    print_text_fmt_int(32,48,"TILT  %d",newcam_tilt);
    print_text_fmt_int(32,32,"DISTANCE %d",newcam_distance);
}

static void newcam_stick_input(void)
{
    #ifdef TARGET_N64
    newcam_stick2[0] = gPlayer2Controller->rawStickX;
    newcam_stick2[1] = gPlayer2Controller->rawStickY;
    #else
    newcam_stick2[0] = MIN(MAX(rightstick[0], -100), 100);
    newcam_stick2[1] = MIN(MAX(rightstick[1], -100), 100);
    #endif // TARGET_N64
}

static s16 newcam_adjust_value(s16 var, s16 val, s8 max)
{
    if (val > 0)
    {
        var += val;
        if (var > max)
            var = max;
    }
    else
    if (val < 0)
    {
        var += val;
        if (var < max)
            var = max;
    }

    return var;
}

static f32 newcam_approach_float(f32 var, f32 val, f32 inc)
{
    if (var < val)
        return min(var + inc, val);
        else
        return max(var - inc, val);
}

static s16 newcam_approach_s16(s16 var, s16 val, s16 inc)
{
    if (var < val)
        return max(var + inc, val);
        else
        return min(var - inc, val);
}

static f32 ivrt(u8 axis)
{
    if (axis == 0)
        return 1;
    else
        return -1;
}

static void newcam_rotate_button(void)
{
    f32 intendedXMag;
    f32 intendedYMag;
    if ((newcam_modeflags & NC_FLAG_8D || newcam_modeflags & NC_FLAG_4D) && newcam_modeflags & NC_FLAG_XTURN) //8 directional camera rotation input for buttons.
    {
        if ((gPlayer1Controller->buttonPressed & L_CBUTTONS) && newcam_analogue == 0)
        {
            #ifndef nosound
            play_sound(SOUND_MENU_CAMERA_ZOOM_IN, gDefaultSoundArgs);
            #endif
            if (newcam_modeflags & NC_FLAG_8D)
                newcam_yaw_target = newcam_yaw_target+(ivrt(newcam_invertX)*0x2000);
            else
                newcam_yaw_target = newcam_yaw_target+(ivrt(newcam_invertX)*0x4000);
            newcam_centering = 1;
        }
        else
        if ((gPlayer1Controller->buttonPressed & R_CBUTTONS) && newcam_analogue == 0)
        {
            #ifndef nosound
            play_sound(SOUND_MENU_CAMERA_ZOOM_IN, gDefaultSoundArgs);
            #endif
            if (newcam_modeflags & NC_FLAG_8D)
                newcam_yaw_target = newcam_yaw_target-(ivrt(newcam_invertX)*0x2000);
            else
                newcam_yaw_target = newcam_yaw_target-(ivrt(newcam_invertX)*0x4000);
            newcam_centering = 1;
        }
    }
    else //Standard camera movement
    if (newcam_modeflags & NC_FLAG_XTURN)
    {
        if ((gPlayer1Controller->buttonDown & L_CBUTTONS) && newcam_analogue == 0)
            newcam_yaw_acc = newcam_adjust_value(newcam_yaw_acc,-accel, -100);
        else if ((gPlayer1Controller->buttonDown & R_CBUTTONS) && newcam_analogue == 0)
            newcam_yaw_acc = newcam_adjust_value(newcam_yaw_acc,accel, 100);
        else
        if (!newcam_analogue)
        {
            #ifdef noaccel
            newcam_yaw_acc = 0;
            #else
            newcam_yaw_acc -= (newcam_yaw_acc*((f32)newcam_degrade/100));
            #endif
        }
    }

    if (gPlayer1Controller->buttonDown & U_CBUTTONS && newcam_modeflags & NC_FLAG_YTURN && newcam_analogue == 0)
        newcam_tilt_acc = newcam_adjust_value(newcam_tilt_acc,accel, 100);
    else if (gPlayer1Controller->buttonDown & D_CBUTTONS && newcam_modeflags & NC_FLAG_YTURN && newcam_analogue == 0)
        newcam_tilt_acc = newcam_adjust_value(newcam_tilt_acc,-accel, -100);
    else
    if (!newcam_analogue)
    {
        #ifdef noaccel
        newcam_tilt_acc = 0;
        #else
        newcam_tilt_acc -= (newcam_tilt_acc*((f32)newcam_degrade/100));
        #endif
    }

    newcam_framessincec[0] ++;
    newcam_framessincec[1] ++;
    if ((gPlayer1Controller->buttonPressed & L_CBUTTONS) && newcam_modeflags & NC_FLAG_XTURN && !(newcam_modeflags & NC_FLAG_8D) && newcam_analogue == 0)
    {
        if (newcam_framessincec[0] < 6)
        {
            newcam_yaw_target = newcam_yaw+(ivrt(newcam_invertX)*0x3000);
            newcam_centering = 1;
            #ifndef nosound
            play_sound(SOUND_MENU_CAMERA_ZOOM_IN, gDefaultSoundArgs);
            #endif
        }
        newcam_framessincec[0] = 0;
    }
    if ((gPlayer1Controller->buttonPressed & R_CBUTTONS) && newcam_modeflags & NC_FLAG_XTURN && !(newcam_modeflags & NC_FLAG_8D) && newcam_analogue == 0)
    {
        if (newcam_framessincec[1] < 6)
            {
            newcam_yaw_target = newcam_yaw-(ivrt(newcam_invertX)*0x3000);
            newcam_centering = 1;
            #ifndef nosound
            play_sound(SOUND_MENU_CAMERA_ZOOM_IN, gDefaultSoundArgs);
            #endif
        }
        newcam_framessincec[1] = 0;
    }


    if (newcam_analogue == 1) //There's not much point in keeping this behind a check, but it wouldn't hurt, just incase any 2player shenanigans ever happen, it makes it easy to disable.
    { //The joystick values cap at 80, so divide by 8 to get the same net result at maximum turn as the button
        intendedXMag = newcam_stick2[0]*1.25;
        intendedYMag = newcam_stick2[1]*1.25;

        if (ABS(newcam_stick2[0]) > NEWCAM_ANALOG_DEADZONE && newcam_modeflags & NC_FLAG_XTURN)
        {
            if (newcam_modeflags & NC_FLAG_8D)
            {
                if (newcam_cstick_down == 0)
                    {
                    newcam_cstick_down = 1;
                    newcam_centering = 1;
                    #ifndef nosound
                    play_sound(SOUND_MENU_CAMERA_ZOOM_IN, gDefaultSoundArgs);
                    #endif
                    if (newcam_stick2[0] > NEWCAM_ANALOG_DEADZONE)
                    {
                        if (newcam_modeflags & NC_FLAG_8D)
                            newcam_yaw_target = newcam_yaw_target+(ivrt(newcam_invertX)*0x2000);
                        else
                            newcam_yaw_target = newcam_yaw_target+(ivrt(newcam_invertX)*0x4000);
                    }
                    else
                    {
                        if (newcam_modeflags & NC_FLAG_8D)
                            newcam_yaw_target = newcam_yaw_target-(ivrt(newcam_invertX)*0x2000);
                        else
                            newcam_yaw_target = newcam_yaw_target-(ivrt(newcam_invertX)*0x4000);
                    }
                }
            }
            else
            {
                newcam_yaw_acc = newcam_adjust_value(newcam_yaw_acc,newcam_stick2[0]*0.125, intendedXMag);
            }
        }
        else
        if (newcam_analogue)
        {
            newcam_cstick_down = 0;
            newcam_yaw_acc -= (newcam_yaw_acc*((f32)newcam_degrade/100));
        }

        if (ABS(newcam_stick2[1]) > NEWCAM_ANALOG_DEADZONE && newcam_modeflags & NC_FLAG_YTURN)
            newcam_tilt_acc = newcam_adjust_value(newcam_tilt_acc,newcam_stick2[1]*0.125, intendedYMag);
        else
        if (newcam_analogue)
        {
            newcam_tilt_acc -= (newcam_tilt_acc*((f32)newcam_degrade/100));
        }
    }
}

static u8 newcam_has_input(void)
{
    if (newcam_analogue) {
        if (ABS(newcam_stick2[0]) > NEWCAM_ANALOG_DEADZONE || ABS(newcam_stick2[1]) > NEWCAM_ANALOG_DEADZONE) return 1;
    } else {
        if (gPlayer1Controller->buttonDown & (L_CBUTTONS | R_CBUTTONS | U_CBUTTONS | D_CBUTTONS)) return 1;
    }
    return 0;
}

void newcam_reset_player_offset(void)
{
    newcam_yaw_offset = 0;
    newcam_pitch_offset = 0;
    newcam_yaw_acc = 0;
    newcam_tilt_acc = 0;
    newcam_centering = 0;
    newcam_override_timer = 0;
    newcam_recenter_behind_mario = 0;
    newcam_base_angle_locked = 0;
}

void newcam_request_recenter_behind_mario(void)
{
    newcam_recenter_behind_mario = 1;
}

static void newcam_apply_recenter_behind_mario(s16 baseYaw)
{
    newcam_yaw_offset = -gMarioState->faceAngle[1] - 0x4000 - baseYaw;
    newcam_pitch_offset = 0;
    newcam_yaw_acc = 0;
    newcam_tilt_acc = 0;
    newcam_centering = 0;
    newcam_override_timer = 0;
    newcam_recenter_behind_mario = 0;
}

static void newcam_update_player_offset(void)
{
    if (newcam_analogue) {
        if (ABS(newcam_stick2[0]) > NEWCAM_ANALOG_DEADZONE) {
            newcam_yaw_acc = newcam_adjust_value(newcam_yaw_acc, newcam_stick2[0] * 0.125f, newcam_stick2[0] * 0.75f);
            newcam_yaw_offset -= (newcam_yaw_acc * MAX(1, newcam_sensitivityX / 35)) * ivrt(newcam_invertX);
        } else {
            newcam_yaw_acc = 0;
        }
        if (ABS(newcam_stick2[1]) > NEWCAM_ANALOG_DEADZONE) {
            newcam_tilt_acc = newcam_adjust_value(newcam_tilt_acc, newcam_stick2[1] * 0.125f, newcam_stick2[1] * 0.75f);
            newcam_pitch_offset += (newcam_tilt_acc * MAX(1, newcam_sensitivityY / 35)) * ivrt(newcam_invertY);
        } else {
            newcam_tilt_acc = 0;
        }
    } else {
        newcam_yaw_acc = 0;
        newcam_tilt_acc = 0;
        if (gPlayer1Controller->buttonDown & L_CBUTTONS)
            newcam_yaw_offset -= 0x200;
        if (gPlayer1Controller->buttonDown & R_CBUTTONS)
            newcam_yaw_offset += 0x200;
        if (gPlayer1Controller->buttonDown & U_CBUTTONS)
            newcam_pitch_offset += 0x180;
        if (gPlayer1Controller->buttonDown & D_CBUTTONS)
            newcam_pitch_offset -= 0x180;
    }

    newcam_pitch_offset = newcam_clamp(newcam_pitch_offset, 0x2C00, -0x2C00);
}

static void newcam_recenter_offset(void)
{
    if (newcam_override_timer != 0)
        return;

    newcam_yaw_offset = approach_s16_symmetric(newcam_yaw_offset, 0, MAX(0x40, newcam_aggression * 0x10));
    newcam_pitch_offset = approach_s16_symmetric(newcam_pitch_offset, 0, MAX(0x20, newcam_aggression * 0x08));
}

static void newcam_zoom_button(void)
{
    //Smoothly move the camera to the new spot.
    if (newcam_distance > newcam_distance_target)
    {
        newcam_distance -= 250;
        if (newcam_distance < newcam_distance_target)
            newcam_distance = newcam_distance_target;
    }
    if (newcam_distance < newcam_distance_target)
    {
        newcam_distance += 250;
        if (newcam_distance > newcam_distance_target)
            newcam_distance = newcam_distance_target;
    }

    //When you press L and R together, set the flag for centering the camera. Afterwards, start setting the yaw to the Player's yaw at the time.
    if (gPlayer1Controller->buttonDown & L_TRIG && gPlayer1Controller->buttonDown & R_TRIG && newcam_modeflags & NC_FLAG_ZOOM)
    {
        newcam_yaw_target = -gMarioState->faceAngle[1]-0x4000;
        newcam_centering = 1;
    }
    else //Each time the player presses R, but NOT L the camera zooms out more, until it hits the limit and resets back to close view.
    if (gPlayer1Controller->buttonPressed & R_TRIG && newcam_modeflags & NC_FLAG_XTURN)
    {
        #ifndef nosound
        play_sound(SOUND_MENU_CLICK_CHANGE_VIEW, gDefaultSoundArgs);
        #endif

        if (newcam_distance_target == newcam_distance_values[0])
            newcam_distance_target = newcam_distance_values[1];
        else
        if (newcam_distance_target == newcam_distance_values[1])
            newcam_distance_target = newcam_distance_values[2];
        else
            newcam_distance_target = newcam_distance_values[0];

    }
    if (newcam_centering && newcam_modeflags & NC_FLAG_XTURN)
    {
        newcam_yaw = approach_s16_symmetric(newcam_yaw,newcam_yaw_target,0x800);
        if (newcam_yaw == newcam_yaw_target)
            newcam_centering = 0;
    }
    else
        newcam_yaw_target = newcam_yaw;
}

static void newcam_update_values(void)
{//For tilt, this just limits it so it doesn't go further than 90 degrees either way. 90 degrees is actually 16384, but can sometimes lead to issues, so I just leave it shy of 90.
    u8 waterflag = 0;

    if (newcam_modeflags & NC_FLAG_XTURN)
        newcam_yaw -= ((newcam_yaw_acc*(newcam_sensitivityX/10))*ivrt(newcam_invertX));
    if (((newcam_tilt <= 12000) && (newcam_tilt >= -12000)) && newcam_modeflags & NC_FLAG_YTURN)
        newcam_tilt += ((newcam_tilt_acc*ivrt(newcam_invertY))*(newcam_sensitivityY/10));

    if (newcam_tilt > 12000)
        newcam_tilt = 12000;
    if (newcam_tilt < -12000)
        newcam_tilt = -12000;

        if (newcam_turnwait > 0 && gMarioState->vel[1] == 0)
        {
            newcam_turnwait -= 1;
            if (newcam_turnwait < 0)
                newcam_turnwait = 0;
        }
        else
        {
        if (gMarioState->intendedMag > 0 && gMarioState->vel[1] == 0 && newcam_modeflags & NC_FLAG_XTURN && !(newcam_modeflags & NC_FLAG_8D) && !(newcam_modeflags & NC_FLAG_4D))
        {
            s16 stickFactor = MAX(ABS(gPlayer1Controller->rawStickX / 10), 4);
            s16 speedFactor = MAX(ABS((s32)gMarioState->forwardVel) / 32, 1);
            newcam_yaw = approach_s16_symmetric(newcam_yaw, -gMarioState->faceAngle[1] - 0x4000,
                                                newcam_aggression * stickFactor * speedFactor);
        }
        else
            newcam_turnwait = 10;
        }

        if (newcam_modeflags & NC_FLAG_SLIDECORRECT)
        {
            switch (gMarioState->action)
            {
                case ACT_BUTT_SLIDE: if (gMarioState->forwardVel > 8) waterflag = 1; break;
                case ACT_STOMACH_SLIDE: if (gMarioState->forwardVel > 8) waterflag = 1; break;
                case ACT_HOLD_BUTT_SLIDE: if (gMarioState->forwardVel > 8) waterflag = 1; break;
                case ACT_HOLD_STOMACH_SLIDE: if (gMarioState->forwardVel > 8) waterflag = 1; break;
            }
        }
        switch (gMarioState->action)
        {
            case ACT_SHOT_FROM_CANNON: waterflag = 1; break;
            case ACT_FLYING: waterflag = 1; break;
        }

        if (gMarioState->action & ACT_FLAG_SWIMMING)
        {
            if (gMarioState->forwardVel > 2)
            waterflag = 1;
        }

        if (waterflag && newcam_modeflags & NC_FLAG_XTURN)
        {
            newcam_yaw = (approach_s16_symmetric(newcam_yaw,-gMarioState->faceAngle[1]-0x4000,(gMarioState->forwardVel*128)));
            if ((signed)gMarioState->forwardVel > 1)
                newcam_tilt = (approach_s16_symmetric(newcam_tilt,(-gMarioState->faceAngle[0]*0.8)+3000,(gMarioState->forwardVel*32)));
            else
                newcam_tilt = (approach_s16_symmetric(newcam_tilt,3000,32));
        }
}

static void newcam_collision(void)
{
    struct Surface *surf;
    Vec3f camdir;
    Vec3f hitpos;
    const f32 wallMargin = 70.0f;

    camdir[0] = newcam_pos[0]-newcam_lookat[0];
    camdir[1] = newcam_pos[1]-newcam_lookat[1];
    camdir[2] = newcam_pos[2]-newcam_lookat[2];

    find_surface_on_ray(newcam_pos_target, camdir, &surf, hitpos);
    newcam_coldist = sqrtf((newcam_pos_target[0] - hitpos[0]) * (newcam_pos_target[0] - hitpos[0]) + (newcam_pos_target[1] - hitpos[1]) * (newcam_pos_target[1] - hitpos[1]) + (newcam_pos_target[2] - hitpos[2]) * (newcam_pos_target[2] - hitpos[2]));


    if (surf)
    {
        if (newcam_coldist > wallMargin) {
            f32 scale = (newcam_coldist - wallMargin) / newcam_coldist;
            newcam_pos[0] = newcam_pos_target[0] + (hitpos[0] - newcam_pos_target[0]) * scale;
            newcam_pos[1] = approach_f32(newcam_pos_target[1] + (hitpos[1] - newcam_pos_target[1]) * scale,
                                         newcam_pos[1], 25, -25);
            newcam_pos[2] = newcam_pos_target[2] + (hitpos[2] - newcam_pos_target[2]) * scale;
        } else {
            newcam_pos[0] = newcam_pos_target[0];
            newcam_pos[1] = approach_f32(newcam_pos_target[1], newcam_pos[1], 25, -25);
            newcam_pos[2] = newcam_pos_target[2];
        }
        newcam_pan_x = 0;
        newcam_pan_z = 0;
    }
}

void newcam_set_official_hint(u8 active, Vec3f pos, Vec3f focus)
{
    newcam_official_hint_active = active;
    if (active != NEWCAM_HINT_NONE) {
        vec3f_copy(newcam_official_pos, pos);
        vec3f_copy(newcam_official_lookat, focus);
    }
}

static void newcam_set_pan(void)
{
    //Apply panning values based on Mario's direction.
    if (gMarioState->action != ACT_HOLDING_BOWSER && gMarioState->action != ACT_SLEEPING && gMarioState->action != ACT_START_SLEEPING)
    {
        approach_f32_asymptotic_bool(&newcam_pan_x, lengthdir_x((160*newcam_panlevel)/100, -gMarioState->faceAngle[1]-0x4000), 0.05);
        approach_f32_asymptotic_bool(&newcam_pan_z, lengthdir_y((160*newcam_panlevel)/100, -gMarioState->faceAngle[1]-0x4000), 0.05);
    }
    else
    {
        approach_f32_asymptotic_bool(&newcam_pan_x, 0, 0.05);
        approach_f32_asymptotic_bool(&newcam_pan_z, 0, 0.05);
    }

    newcam_pan_x = newcam_pan_x*(min(newcam_distance/newcam_distance_target,1));
    newcam_pan_z = newcam_pan_z*(min(newcam_distance/newcam_distance_target,1));
}

static void newcam_position_cam(void)
{
    f32 floorY = 0;
    f32 floorY2 = 0;
    s16 shakeX;
    s16 shakeY;

    if (!(gMarioState->action & ACT_FLAG_SWIMMING) && newcam_modeflags & NC_FLAG_FOCUSY && newcam_modeflags & NC_FLAG_POSY)
        calc_y_to_curr_floor(&floorY, 1.f, 200.f, &floorY2, 0.9f, 200.f);

    newcam_update_values();
    shakeX = gLakituState.shakeMagnitude[1];
    shakeY = gLakituState.shakeMagnitude[0];
    //Fetch Mario's current position. Not hardcoded just for the sake of flexibility, though this specific bit is temp, because it won't always want to be focusing on Mario.
    newcam_pos_target[0] = gMarioState->pos[0];
    newcam_pos_target[1] = gMarioState->pos[1]+newcam_extheight;
    newcam_pos_target[2] = gMarioState->pos[2];
    //These will set the position of the camera to where Mario is supposed to be, minus adjustments for where the camera should be, on top of.
    if (newcam_modeflags & NC_FLAG_POSX)
        newcam_pos[0] = newcam_pos_target[0]+lengthdir_x(lengthdir_x(newcam_distance,newcam_tilt+shakeX),newcam_yaw+shakeY);
    if (newcam_modeflags & NC_FLAG_POSZ)
        newcam_pos[2] = newcam_pos_target[2]+lengthdir_y(lengthdir_x(newcam_distance,newcam_tilt+shakeX),newcam_yaw+shakeY);
    if (newcam_modeflags & NC_FLAG_POSY)
        newcam_pos[1] = newcam_pos_target[1]+lengthdir_y(newcam_distance,newcam_tilt+gLakituState.shakeMagnitude[0])+floorY;
    if ((newcam_modeflags & NC_FLAG_FOCUSX) && (newcam_modeflags & NC_FLAG_FOCUSY) && (newcam_modeflags & NC_FLAG_FOCUSZ))
        newcam_set_pan();
    //Set where the camera wants to be looking at. This is almost always the place it's based off, too.
    if (newcam_modeflags & NC_FLAG_FOCUSX)
        newcam_lookat[0] = newcam_pos_target[0]-newcam_pan_x;
    if (newcam_modeflags & NC_FLAG_FOCUSY)
        newcam_lookat[1] = newcam_pos_target[1]+floorY2;
    if (newcam_modeflags & NC_FLAG_FOCUSZ)
        newcam_lookat[2] = newcam_pos_target[2]-newcam_pan_z;

    if (newcam_modeflags & NC_FLAG_COLLISION)
    newcam_collision();

}

//Nested if's baybeeeee
static void newcam_find_fixed(void)
{
    u8 i = 0;
    void (*func)();
    newcam_mode = newcam_intendedmode;
    newcam_modeflags = newcam_mode;

    for (i = 0; i < sizeof(newcam_fixedcam) / sizeof(struct newcam_hardpos); i++)
    {
        if (newcam_fixedcam[i].newcam_hard_levelID == gCurrLevelNum && newcam_fixedcam[i].newcam_hard_areaID == gCurrAreaIndex)
        {//I didn't wanna just obliterate the horizontal plane of the IDE with a beefy if statement, besides, I think this runs slightly better anyway?
            if (newcam_pos_target[0] > newcam_fixedcam[i].newcam_hard_X1)
            if (newcam_pos_target[0] < newcam_fixedcam[i].newcam_hard_X2)
            if (newcam_pos_target[1] > newcam_fixedcam[i].newcam_hard_Y1)
            if (newcam_pos_target[1] < newcam_fixedcam[i].newcam_hard_Y2)
            if (newcam_pos_target[2] > newcam_fixedcam[i].newcam_hard_Z1)
            if (newcam_pos_target[2] < newcam_fixedcam[i].newcam_hard_Z2)
            {
                if (newcam_fixedcam[i].newcam_hard_permaswap)
                    newcam_intendedmode = newcam_fixedcam[i].newcam_hard_modeset;
                newcam_mode = newcam_fixedcam[i].newcam_hard_modeset;
                newcam_modeflags = newcam_mode;

                if (newcam_fixedcam[i].newcam_hard_camX != 32767 && !(newcam_modeflags & NC_FLAG_POSX))
                    newcam_pos[0] = newcam_fixedcam[i].newcam_hard_camX;
                if (newcam_fixedcam[i].newcam_hard_camY != 32767 && !(newcam_modeflags & NC_FLAG_POSY))
                    newcam_pos[1] = newcam_fixedcam[i].newcam_hard_camY;
                if (newcam_fixedcam[i].newcam_hard_camZ != 32767 && !(newcam_modeflags & NC_FLAG_POSZ))
                    newcam_pos[2] = newcam_fixedcam[i].newcam_hard_camZ;

                if (newcam_fixedcam[i].newcam_hard_lookX != 32767 && !(newcam_modeflags & NC_FLAG_FOCUSX))
                    newcam_lookat[0] = newcam_fixedcam[i].newcam_hard_lookX;
                if (newcam_fixedcam[i].newcam_hard_lookY != 32767 && !(newcam_modeflags & NC_FLAG_FOCUSY))
                    newcam_lookat[1] = newcam_fixedcam[i].newcam_hard_lookY;
                if (newcam_fixedcam[i].newcam_hard_lookZ != 32767 && !(newcam_modeflags & NC_FLAG_FOCUSZ))
                    newcam_lookat[2] = newcam_fixedcam[i].newcam_hard_lookZ;

                newcam_yaw = atan2s(newcam_pos[0]-newcam_pos_target[0],newcam_pos[2]-newcam_pos_target[2]);

                if (newcam_fixedcam[i].newcam_hard_script != 0)
                {
                    func = newcam_fixedcam[i].newcam_hard_script;
                    (func)();
                }
            }
        }
    }
}

static void newcam_apply_values(struct Camera *c)
{

    c->pos[0] = newcam_pos[0];
    c->pos[1] = newcam_pos[1];
    c->pos[2] = newcam_pos[2];

    c->focus[0] = newcam_lookat[0];
    c->focus[1] = newcam_lookat[1];
    c->focus[2] = newcam_lookat[2];

    gLakituState.pos[0] = newcam_pos[0];
    gLakituState.pos[1] = newcam_pos[1];
    gLakituState.pos[2] = newcam_pos[2];

    gLakituState.focus[0] = newcam_lookat[0];
    gLakituState.focus[1] = newcam_lookat[1];
    gLakituState.focus[2] = newcam_lookat[2];

    c->yaw = -newcam_yaw+0x4000;
    gLakituState.yaw = -newcam_yaw+0x4000;

    //Adds support for wing mario tower
    if (gMarioState->floor != NULL)
    {
        if (gMarioState->floor->type == SURFACE_LOOK_UP_WARP) {
            if (save_file_get_total_star_count(gCurrSaveFileNum - 1, 0, 0x18) >= 10) {
                if (newcam_tilt < -8000 && gMarioState->forwardVel == 0) {
                    level_trigger_warp(gMarioState, 1);
                }
            }
        }
    }
}

//If puppycam gets too close to its target, start fading it out so you don't see the inside of it.
void newcam_fade_target_closeup(void)
{
    if (newcam_coldist <= 250 && (newcam_coldist-150)*2.55 < 255)
    {
        if ((newcam_coldist-150)*2.55 > 0)
            newcam_xlu = (newcam_coldist-150)*2.55;
        else
            newcam_xlu = 0;
    }
    else
        newcam_xlu = 255;
}

//The ingame cutscene system is such a spaghetti mess I actually have to resort to something as stupid as this to cover every base.
void newcam_apply_outside_values(struct Camera *c, u8 bit)
{
    if (bit)
        newcam_yaw = -gMarioState->faceAngle[1]-0x4000;
    else
        newcam_yaw = -c->yaw+0x4000;
}

void newcam_reset_after_cutscene(struct Camera *c)
{
    newcam_apply_outside_values(c, 1);
    newcam_yaw_target = newcam_yaw;
    newcam_yaw_acc = 0;
    newcam_tilt_acc = 0;
    newcam_centering = 0;
    newcam_cstick_down = 0;
    newcam_distance = newcam_distance_target;
    c->yaw = -newcam_yaw + 0x4000;
    gLakituState.yaw = c->yaw;
}

//Main loop.
void newcam_loop(struct Camera *c)
{
    newcam_stick_input();

    if (newcam_recenter_behind_mario) {
        newcam_yaw = -gMarioState->faceAngle[1] - 0x4000;
        newcam_yaw_target = newcam_yaw;
        newcam_yaw_acc = 0;
        newcam_tilt_acc = 0;
        newcam_centering = 0;
        newcam_recenter_behind_mario = 0;
    }

    newcam_find_fixed();
    newcam_zoom_button();
    newcam_rotate_button();
    newcam_position_cam();

    if (gMarioObject)
        newcam_apply_values(c);

    newcam_fade_target_closeup();

    //Just some visual information on the values of the camera. utilises ifdef because it's better at runtime.
    #ifdef NEWCAM_DEBUG
    newcam_diagnostics();
    #endif // NEWCAM_DEBUG

}


#ifndef NC_CODE_NOMENU
//Displays a box.
void newcam_display_box(s16 x1, s16 y1, s16 x2, s16 y2, u8 r, u8 g, u8 b)
{
    gDPPipeSync(gDisplayListHead++);
    gDPSetRenderMode(gDisplayListHead++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPSetCycleType(gDisplayListHead++, G_CYC_FILL);
    gDPSetFillColor(gDisplayListHead++, GPACK_RGBA5551(r, g, b, 255));
    gDPFillRectangle(gDisplayListHead++, x1, y1, x2 - 1, y2 - 1);
    gDPPipeSync(gDisplayListHead++);
    gDPSetCycleType(gDisplayListHead++, G_CYC_1CYCLE);
}

//I actually took the time to redo this, properly. Lmao. Please don't bully me over this anymore :(
void newcam_change_setting(s8 toggle)
{
    if (gPlayer1Controller->buttonDown & A_BUTTON)
        toggle*= 5;
    if (gPlayer1Controller->buttonDown & B_BUTTON)
        toggle*= 10;

    if (newcam_optionvar[newcam_option_selection].newcam_op_min == FALSE && newcam_optionvar[newcam_option_selection].newcam_op_max == TRUE)
    {
        *newcam_optionvar[newcam_option_selection].newcam_op_var ^= 1;
    }
    else
        *newcam_optionvar[newcam_option_selection].newcam_op_var += toggle;
    //Forgive me father, for I have sinned. I guess if you wanted a selling point for a 21:9 monitor though, "I can view this line in puppycam's code without scrolling!" can be added to it.
    *newcam_optionvar[newcam_option_selection].newcam_op_var = newcam_clamp(*newcam_optionvar[newcam_option_selection].newcam_op_var, newcam_optionvar[newcam_option_selection].newcam_op_max, newcam_optionvar[newcam_option_selection].newcam_op_min);
}

void newcam_text(s16 x, s16 y, u8 str[], u8 col)
{
    u8 textX;
    textX = get_str_x_pos_from_center(x,str,10.0f);
    gDPSetEnvColor(gDisplayListHead++, 0, 0, 0, 255);
    print_generic_string(textX+1,y-1,str);
    if (col != 0)
    {
        gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, 255);
    }
    else
    {
        gDPSetEnvColor(gDisplayListHead++, 255, 32, 32, 255);
    }
    print_generic_string(textX,y,str);
}

//Options menu
void newcam_display_options()
{
    u8 i = 0;
    u8 newstring[32];
    s16 scroll;
    s16 scrollpos;
    s16 var;
    u16 maxvar;
    u16 minvar;
    f32 newcam_sinpos;
    gSPDisplayList(gDisplayListHead++, dl_rgba16_text_begin);
    gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, 255);
    print_hud_lut_string(HUD_LUT_GLOBAL, 64, 40, newcam_strings_ptr[2]);
    gSPDisplayList(gDisplayListHead++, dl_rgba16_text_end);

    if (newcam_total>4)
    {
        newcam_display_box(272,90,280,208,0x80,0x80,0x80);
        scrollpos = (54)*((f32)newcam_option_scroll/(newcam_total-4));
        newcam_display_box(272,90+scrollpos,280,154+scrollpos,0xFF,0xFF,0xFF);
    }


    gSPDisplayList(gDisplayListHead++, dl_ia_text_begin);
    gDPSetScissor(gDisplayListHead++, G_SC_NON_INTERLACE, 0, 80, SCREEN_WIDTH, SCREEN_HEIGHT);
    for (i = 0; i < newcam_total; i++)
    {
        scroll = 140-(32*i)+(newcam_option_scroll*32);
        if (scroll <= 140 && scroll > 32)
        {
            newcam_text(160,scroll,newcam_options_ptr[newcam_optionvar[i].newcam_op_name],newcam_option_selection-i);
            if (newcam_optionvar[i].newcam_op_option_start != 255)
            {
                var = *newcam_optionvar[i].newcam_op_var;
                newcam_text(160,scroll-12,newcam_flags_ptr[var+newcam_optionvar[i].newcam_op_option_start],newcam_option_selection-i);
            }
            else
            {
                int_to_str(*newcam_optionvar[i].newcam_op_var,newstring);
                newcam_text(160,scroll-12,newstring,newcam_option_selection-i);
                newcam_display_box(96,111+(32*i)-(newcam_option_scroll*32),224,117+(32*i)-(newcam_option_scroll*32),0x80,0x80,0x80);
                maxvar = newcam_optionvar[i].newcam_op_max - newcam_optionvar[i].newcam_op_min;
                minvar = *newcam_optionvar[i].newcam_op_var - newcam_optionvar[i].newcam_op_min;
                newcam_display_box(96,111+(32*i)-(newcam_option_scroll*32),96+(((f32)minvar/maxvar)*128),117+(32*i)-(newcam_option_scroll*32),0xFF,0xFF,0xFF);
                newcam_display_box(94+(((f32)minvar/maxvar)*128),109+(32*i)-(newcam_option_scroll*32),98+(((f32)minvar/maxvar)*128),119+(32*i)-(newcam_option_scroll*32),0xFF,0x0,0x0);
                gSPDisplayList(gDisplayListHead++, dl_ia_text_begin);
            }
        }
    }
    newcam_sinpos = sins(newcam_sintimer*5000)*4;
    gDPSetScissor(gDisplayListHead++, G_SC_NON_INTERLACE, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, 255);
    print_generic_string(80-newcam_sinpos, 132-(32*(newcam_option_selection-newcam_option_scroll)),  newcam_strings_ptr[3]);
    print_generic_string(232+newcam_sinpos, 132-(32*(newcam_option_selection-newcam_option_scroll)),  newcam_strings_ptr[4]);
    gSPDisplayList(gDisplayListHead++, dl_ia_text_end);
}

//This has been separated for interesting reasons. Don't question it.
void newcam_render_option_text(void)
{
    gSPDisplayList(gDisplayListHead++, dl_ia_text_begin);
    newcam_text(278,212,newcam_strings_ptr[newcam_option_open],1);
    gSPDisplayList(gDisplayListHead++, dl_ia_text_end);
}


#ifndef TARGET_N64
void newcam_mouse_handler(void)
{
    //If the cursor moves an amount from the locked position, that isn't just the mouse getting the slightest movement from time manipulation, then it enables mouse input.
    if ((mousepos[0] - mouselock[0] < 4 || mousepos[1] - mouselock[1] < 4) && !mousemode)
        mousemode = 1;

    //If the controller even dares to have an input, then the mouse backs the hell down and lets the controller take the wheel.
    if (gPlayer1Controller->buttonDown || gPlayer1Controller->stickMag|| gPlayer2Controller->stickMag)
    {
        mousemode = 0;
        mouselock[0] = mousepos[0];
        mouselock[1] = mousepos[1];
    }
}

void newcam_config_save(void)
{
    puppycam_sensitivityX = newcam_sensitivityX;
    puppycam_sensitivityY = newcam_sensitivityY;
    puppycam_aggression = newcam_aggression;
    puppycam_panlevel = newcam_panlevel;
    puppycam_invertX = newcam_invertX;
    puppycam_invertY = newcam_invertY;
    puppycam_degrade = newcam_degrade;

    configfile_save("sm64config.txt");
}
#endif

void newcam_check_pause_buttons()
{
    if (gPlayer1Controller->buttonPressed & R_TRIG)
    {
            #ifndef nosound
            play_sound(SOUND_MENU_CHANGE_SELECT, gDefaultSoundArgs);
            #endif
        if (newcam_option_open == 0)
        {
            newcam_option_open = 1;
            #if defined(VERSION_EU)
            newcam_set_language();
            #endif
        }

        else
        {
            newcam_option_open = 0;
            #ifdef TARGET_N64
            save_file_set_setting();
            #else
            newcam_config_save();
            #endif
        }
    }

    if (newcam_option_open)
    {
        newcam_sintimer++;
        #ifndef TARGET_N64
        if (mousemode)
        {
            //newcam_option_index = 140-(32*i)+(newcam_option_scroll*32);
        }
        #endif
        if (ABS(gPlayer1Controller->rawStickY) > 60 || gPlayer1Controller->buttonDown & U_JPAD || gPlayer1Controller->buttonDown & D_JPAD)
        {
            newcam_option_timer -= 1;
            if (newcam_option_timer <= 0)
            {
                switch (newcam_option_index)
                {
                    case 0: newcam_option_index++; newcam_option_timer += 10; break;
                    default: newcam_option_timer += 5; break;
                }
                #ifndef nosound
                play_sound(SOUND_MENU_CHANGE_SELECT, gDefaultSoundArgs);
                #endif
                if (gPlayer1Controller->rawStickY >= 60 || gPlayer1Controller->buttonDown & U_JPAD)
                {
                    newcam_option_selection--;
                    if (newcam_option_selection < 0)
                        newcam_option_selection = newcam_total-1;
                }
                else
                if (gPlayer1Controller->rawStickY <= -60 || gPlayer1Controller->buttonDown & D_JPAD)
                {
                    newcam_option_selection++;
                    if (newcam_option_selection >= newcam_total)
                        newcam_option_selection = 0;
                }
            }
        }
        else
        if (ABS(gPlayer1Controller->rawStickX) > 60 || gPlayer1Controller->buttonDown & L_JPAD || gPlayer1Controller->buttonDown & R_JPAD)
        {
            newcam_option_timer -= 1;
            if (newcam_option_timer <= 0)
            {
                switch (newcam_option_index)
                {
                    case 0: newcam_option_index++; newcam_option_timer += 10; break;
                    default: newcam_option_timer += 5; break;
                }
                #ifndef nosound
                play_sound(SOUND_MENU_CHANGE_SELECT, gDefaultSoundArgs);
                #endif
                if (gPlayer1Controller->rawStickX >= 60 || gPlayer1Controller->buttonDown & R_JPAD)
                    newcam_change_setting(1);
                else
                if (gPlayer1Controller->rawStickX <= -60 || gPlayer1Controller->buttonDown & L_JPAD)
                    newcam_change_setting(-1);
            }
        }
        else
        {
            newcam_option_timer = 0;
            newcam_option_index = 0;
        }

        while (newcam_option_scroll - newcam_option_selection < -3 && newcam_option_selection > newcam_option_scroll)
            newcam_option_scroll +=1;
        while (newcam_option_scroll + newcam_option_selection > 0 && newcam_option_selection < newcam_option_scroll)
            newcam_option_scroll -=1;
    }
}
#endif
