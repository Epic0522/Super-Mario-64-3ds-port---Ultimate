#include "game/level_update.h"
#include "game/area.h"
#include "game/save_file.h"
#include "level_table.h"

#include "minimap_levels.h"
#include "minimap.h"

// SM64 map area is 16384x16384 (-8192 to +8192 in x and y)
static const float MAP_OFFSET = 8192.0f;
// vertical height of 3DS bottom screen is 240px
static const float MAP_SIZE = 240.0f;
// translate 0->16384 into 0->240
static const float MAP_SCALE = MAP_SIZE / (2 * MAP_OFFSET);

static u8 level_index = 0; // map gCurrLevelNum into level_info[X][]
static u8 area_index = 0;  // map gCurrAreaIndex into level_info[][X]

static s16 current_level = -1;
static s16 current_area = -1;
static s16 current_play_mode = -1;

static bool level_select_preview_active = false;
static s16 level_select_preview_level = -1;
static s8 level_select_preview_area = 1;

static s16 minimap_get_effective_level(void)
{
    return level_select_preview_active ? level_select_preview_level : gCurrLevelNum;
}

static s16 minimap_get_effective_area(void)
{
    return level_select_preview_active ? level_select_preview_area : gCurrAreaIndex;
}

static s16 minimap_get_effective_play_mode(void)
{
    return level_select_preview_active ? 0 : sCurrPlayMode;
}

void minimap_set_level_select_preview(bool active, s16 level, s8 area)
{
    level_select_preview_active = active;
    level_select_preview_level = level;
    level_select_preview_area = area > 0 ? area : 1;
}

bool minimap_is_level_select_preview_active(void)
{
    return level_select_preview_active;
}

bool minimap_has_level_or_area_changed()
{
    return (current_level != minimap_get_effective_level()
            || current_area != minimap_get_effective_area()
            || current_play_mode != minimap_get_effective_play_mode());
}

bool minimap_load_level_and_area()
{
    bool has_minimap_level = true;
    s16 effective_level = minimap_get_effective_level();
    s16 effective_area = minimap_get_effective_area();
    s16 effective_play_mode = minimap_get_effective_play_mode();

    // gCurrAreaIndex is 1-indexed
    area_index = effective_area > 0 ? effective_area - 1 : 0;

    switch (effective_level)
    {
        case LEVEL_BBH:
            level_index = 1;
            break;
        case LEVEL_CCM:
            level_index = 2;
            break;
        case LEVEL_CASTLE:
            level_index = 3;
            break;
        case LEVEL_HMC:
            level_index = 4;
            break;
        case LEVEL_SSL:
            level_index = 5;
            break;
        case LEVEL_BOB:
            level_index = 6;
            break;
        case LEVEL_SL:
            level_index = 7;
            break;
        case LEVEL_WDW:
            level_index = 8;
            break;
        case LEVEL_JRB:
            level_index = 9;
            break;
        case LEVEL_THI:
            level_index = 10;
            break;
        case LEVEL_TTC:
            level_index = 11;
            break;
        case LEVEL_RR:
            level_index = 12;
            break;
        case LEVEL_CASTLE_GROUNDS:
            level_index = 13;
            // special case for drained moat
            if (save_file_get_flags() & SAVE_FLAG_MOAT_DRAINED)
                area_index = 1;
            break;
        case LEVEL_BITDW:
            level_index = 14;
            break;
        case LEVEL_VCUTM:
            level_index = 15;
            break;
        case LEVEL_BITFS:
            level_index = 16;
            break;
        case LEVEL_SA:
            level_index = 17;
            break;
        case LEVEL_BITS:
            level_index = 18;
            break;
        case LEVEL_LLL:
            level_index = 19;
            break;
        case LEVEL_DDD:
            level_index = 20;
            break;
        case LEVEL_WF:
            level_index = 21;
            break;
        case LEVEL_CASTLE_COURTYARD:
            level_index = 22;
            break;
        case LEVEL_PSS:
            level_index = 23;
            break;
        case LEVEL_COTMC:
            level_index = 24;
            break;
        case LEVEL_TOTWC:
            level_index = 25;
            break;
        case LEVEL_BOWSER_1:
            level_index = 26;
            break;
        case LEVEL_WMOTR:
            level_index = 27;
            break;
        case LEVEL_BOWSER_2:
            level_index = 28;
            break;
        case LEVEL_BOWSER_3:
            level_index = 29;
            break;
        case LEVEL_TTM:
            level_index = 30;
            break;
        case LEVEL_UNKNOWN_1:
        case LEVEL_UNKNOWN_2:
        case LEVEL_UNKNOWN_3:
        case LEVEL_ENDING:
        case LEVEL_UNKNOWN_32:
        case LEVEL_UNKNOWN_35:
        case LEVEL_UNKNOWN_37:
        case LEVEL_UNKNOWN_38:
        default:
            level_index = 0; // blank
            has_minimap_level = false;
    }

    current_level = effective_level;
    current_area = effective_area;
    current_play_mode = effective_play_mode;

    if (!has_minimap_level)
        return false;

    // if not play or pause modes, then dont display minimap
    if (effective_play_mode != 0 && effective_play_mode != 2)
        return false;

    if (area_index >= level_info[level_index].area_count)
    {
        level_index = 0;
        area_index = 0;
    }
    else if (level_info[level_index].areas[area_index].is_placeholder)
    {
        level_index = 0;
        area_index = 0;
    }

    return true;
}

void minimap_get_current_texture(uint8_t **texture, size_t *texture_size, uint32_t *color)
{
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
    *texture = level_info[level_index].areas[area_index].texture;
#pragma GCC diagnostic pop
    *texture_size = level_info[level_index].areas[area_index].size;
    *color = level_info[level_index].areas[area_index].color;
}

bool minimap_get_mario_position(float *mario_x, float *mario_y, float *mario_direction)
{
    if (gMarioState == NULL)
    {
        return false;
    }

    // get mario's current position
    float cur_mario_x = gMarioState->pos[0]; // x
    float cur_mario_y = gMarioState->pos[2]; // y

    float cur_mario_direction = gMarioState->faceAngle[1];

    // scale for the mini map and clamp
    float mario_x_scaled_and_offset = MIN(MAP_SIZE, MAX(0.0f, (cur_mario_x + MAP_OFFSET) * MAP_SCALE));
    float mario_y_scaled_and_offset = MIN(MAP_SIZE, MAX(0.0f, (cur_mario_y + MAP_OFFSET) * MAP_SCALE));

    // range of rotation is +- 32768
    float mario_direction_scaled = cur_mario_direction / 65536.0f;

    *mario_x = mario_x_scaled_and_offset;
    *mario_y = mario_y_scaled_and_offset;
    *mario_direction = mario_direction_scaled;

    return true;
}
