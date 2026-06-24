#ifndef GFX_3DS_MINIMAP_H
#define GFX_3DS_MINIMAP_H

#include "gfx_3ds_common.h"

#include "src/minimap/minimap.h"

/* mario head */
#include "src/minimap/textures/mario_t3x.h"
/* heading */
#include "src/minimap/textures/arrow_t3x.h"
/* bottom screen HUD */
#include "src/minimap/textures/hud_coin_t3x.h"
#include "src/minimap/textures/hud_digit_0_t3x.h"
#include "src/minimap/textures/hud_digit_1_t3x.h"
#include "src/minimap/textures/hud_digit_2_t3x.h"
#include "src/minimap/textures/hud_digit_3_t3x.h"
#include "src/minimap/textures/hud_digit_4_t3x.h"
#include "src/minimap/textures/hud_digit_5_t3x.h"
#include "src/minimap/textures/hud_digit_6_t3x.h"
#include "src/minimap/textures/hud_digit_7_t3x.h"
#include "src/minimap/textures/hud_digit_8_t3x.h"
#include "src/minimap/textures/hud_digit_9_t3x.h"
#include "src/minimap/textures/hud_health_1_t3x.h"
#include "src/minimap/textures/hud_health_2_t3x.h"
#include "src/minimap/textures/hud_health_3_t3x.h"
#include "src/minimap/textures/hud_health_4_t3x.h"
#include "src/minimap/textures/hud_health_5_t3x.h"
#include "src/minimap/textures/hud_health_6_t3x.h"
#include "src/minimap/textures/hud_health_7_t3x.h"
#include "src/minimap/textures/hud_health_8_t3x.h"
#include "src/minimap/textures/hud_red_coin_front_t3x.h"
#include "src/minimap/textures/hud_red_coin_side_t3x.h"
#include "src/minimap/textures/hud_red_coin_tilt_left_t3x.h"
#include "src/minimap/textures/hud_red_coin_tilt_right_t3x.h"
#include "src/minimap/textures/hud_star_t3x.h"
#include "src/minimap/textures/hud_x_t3x.h"
#include "src/minimap/textures/title_1996_nintendo_t3x.h"
#include "src/minimap/textures/title_press_start_t3x.h"
#include "src/minimap/textures/title_press_home_t3x.h"
#include "src/minimap/textures/music_boss_battle_t3x.h"
#include "src/minimap/textures/music_bowser_battle_t3x.h"
#include "src/minimap/textures/music_dire_dire_docks_t3x.h"
#include "src/minimap/textures/music_ending_t3x.h"
#include "src/minimap/textures/music_endless_stairs_t3x.h"
#include "src/minimap/textures/music_file_select_t3x.h"
#include "src/minimap/textures/music_final_bowser_t3x.h"
#include "src/minimap/textures/music_haunted_house_t3x.h"
#include "src/minimap/textures/music_hazy_maze_cave_t3x.h"
#include "src/minimap/textures/music_high_score_t3x.h"
#include "src/minimap/textures/music_inside_castle_t3x.h"
#include "src/minimap/textures/music_key_clear_t3x.h"
#include "src/minimap/textures/music_koopa_message_t3x.h"
#include "src/minimap/textures/music_koopas_road_t3x.h"
#include "src/minimap/textures/music_lakitu_t3x.h"
#include "src/minimap/textures/music_lethal_lava_land_t3x.h"
#include "src/minimap/textures/music_main_theme_t3x.h"
#include "src/minimap/textures/music_merry_go_round_t3x.h"
#include "src/minimap/textures/music_metal_mario_t3x.h"
#include "src/minimap/textures/music_opening_t3x.h"
#include "src/minimap/textures/music_peach_message_t3x.h"
#include "src/minimap/textures/music_piranha_lullaby_t3x.h"
#include "src/minimap/textures/music_powerful_mario_t3x.h"
#include "src/minimap/textures/music_puzzle_solved_t3x.h"
#include "src/minimap/textures/music_race_fanfare_t3x.h"
#include "src/minimap/textures/music_silence_t3x.h"
#include "src/minimap/textures/music_slider_t3x.h"
#include "src/minimap/textures/music_snow_mountain_t3x.h"
#include "src/minimap/textures/music_staff_roll_t3x.h"
#include "src/minimap/textures/music_star_select_t3x.h"
#include "src/minimap/textures/music_star_spawn_t3x.h"
#include "src/minimap/textures/music_title_theme_t3x.h"
#include "src/minimap/textures/music_toad_message_t3x.h"
#include "src/minimap/textures/music_victory_t3x.h"

static const float mario_sprite_size = 16.0f / 2.0f; // 16px but 8px left/right of 0
static const float arrow_sprite_size = 8.0f / 2.0f;  // 8px but 4px left/right of 0

static const vertex vertex_list_mario[] =
{
    { { -mario_sprite_size, -mario_sprite_size, 0.5f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { {  mario_sprite_size,  mario_sprite_size, 0.5f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { {  mario_sprite_size, -mario_sprite_size, 0.5f, 1.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },

    { { -mario_sprite_size, -mario_sprite_size, 0.5f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { { -mario_sprite_size,  mario_sprite_size, 0.5f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { {  mario_sprite_size,  mario_sprite_size, 0.5f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } }
};

static const vertex vertex_list_arrow[] =
{
    { { -arrow_sprite_size, -arrow_sprite_size, 0.5f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { {  arrow_sprite_size,  arrow_sprite_size, 0.5f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { {  arrow_sprite_size, -arrow_sprite_size, 0.5f, 1.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },

    { { -arrow_sprite_size, -arrow_sprite_size, 0.5f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { { -arrow_sprite_size,  arrow_sprite_size, 0.5f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { {  arrow_sprite_size,  arrow_sprite_size, 0.5f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } }
};

static const vertex vertex_list_hud_icon[] =
{
    { {  0.0f,  0.0f, 0.5f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { { 16.0f, 16.0f, 0.5f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { { 16.0f,  0.0f, 0.5f, 1.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },

    { {  0.0f,  0.0f, 0.5f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { {  0.0f, 16.0f, 0.5f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { { 16.0f, 16.0f, 0.5f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } }
};

static const vertex vertex_list_hud_digit[] =
{
    { {  0.0f,  0.0f, 0.5f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { { 16.0f, 16.0f, 0.5f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { { 16.0f,  0.0f, 0.5f, 1.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },

    { {  0.0f,  0.0f, 0.5f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { {  0.0f, 16.0f, 0.5f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { { 16.0f, 16.0f, 0.5f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } }
};

static const vertex vertex_list_hud_health[] =
{
    { {  0.0f,  0.0f, 0.5f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { { 64.0f, 64.0f, 0.5f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { { 64.0f,  0.0f, 0.5f, 1.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },

    { {  0.0f,  0.0f, 0.5f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { {  0.0f, 64.0f, 0.5f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { { 64.0f, 64.0f, 0.5f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } }
};

static const vertex vertex_list_music_title[] =
{
    { {   0.0f,  0.0f, 0.5f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { { 160.0f, 16.0f, 0.5f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { { 160.0f,  0.0f, 0.5f, 1.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },

    { {   0.0f,  0.0f, 0.5f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { {   0.0f, 16.0f, 0.5f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { { 160.0f, 16.0f, 0.5f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } }
};

void gfx_3ds_init_minimap();
uint32_t gfx_3ds_draw_minimap(float *vertex_buffer, int vertex_offset);

#endif
