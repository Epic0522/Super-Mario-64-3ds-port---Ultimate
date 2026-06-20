#ifdef TARGET_N3DS

#include <stdlib.h>

#include "gfx_3ds_minimap.h"
#include "gfx_3ds.h"
#include "audio/external.h"
#include "game/game_init.h"
#include "game/ingame_menu.h"
#include "game/level_update.h"
#include "menu/intro_geo.h"
#include "menu/level_select_menu.h"
#include "seq_ids.h"

static C3D_Mtx modelView, projBottom;

static uint8_t *current_texture;
static size_t current_texture_size;

static uint32_t minimap_color;      // flood-fill background color
static C3D_Tex minimap_mario_tex;   // texture for mario location
static C3D_Tex minimap_arrow_tex;   // texture for heading arrow
static C3D_Tex minimap_tex;         // texture for map background
static bool minimap_tex_loaded = false;
static C3D_Tex hud_coin_tex;
static C3D_Tex hud_star_tex;
static C3D_Tex hud_x_tex;
static C3D_Tex hud_health_tex[8];
static C3D_Tex hud_red_coin_tex[4];
static C3D_Tex hud_digit_tex[10];
static C3D_Tex title_1996_nintendo_tex;
static C3D_Tex title_press_start_tex;
static C3D_Tex music_title_tex[SEQ_COUNT];
static u16 music_title_width[SEQ_COUNT];

static float mario_x, mario_y, mario_direction = 0.0f;
// 320 - 240 = 80, so add 40 to shift map into middle of the screen
static float x_offset = 40.0f;
// FIXME: just chop bottom 16px of texture for now
static float y_offset = 0.0f; //-16.0f;

static u32 buffer_offset = 0;

static uint32_t rgb_to_abgr(uint32_t rgb)
{
    // 0xRRGGBB to 0xFFBBGGRR
    return (0x0000ff & rgb >> 16)
        | (0x00ff00 & rgb)
        | (0xff0000 & rgb << 16)
        | 0xff000000;
}

static void minimap_load_new_minimap_texture()
{
    if (minimap_tex_loaded)
    {
        C3D_TexDelete(&minimap_tex);
        minimap_tex_loaded = false;
    }

    minimap_get_current_texture(&current_texture, &current_texture_size, &minimap_color);
    minimap_tex_loaded = load_t3x_texture(&minimap_tex, NULL, current_texture, current_texture_size);
    if (minimap_tex_loaded)
        C3D_TexSetFilter(&minimap_tex, GPU_LINEAR, GPU_NEAREST);
}

static void minimap_unload_current_minimap_texture()
{
    if (minimap_tex_loaded)
    {
        C3D_TexDelete(&minimap_tex);
        minimap_tex_loaded = false;
    }
}

static void gfx_3ds_minimap_draw_background_color_rgb(float *vbo_buffer, uint32_t color)
{
    Mtx_Identity(&modelView);
    Mtx_OrthoTilt(&projBottom, 0.0, 320.0, 0.0, 240.0, 0.0, 1.0, true);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, &modelView);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projBottom);

    memcpy(vbo_buffer + buffer_offset * VERTEX_SHADER_SIZE,
           vertex_list_color,
           sizeof(vertex_list_color));

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvColor(env, rgb_to_abgr(color));
    C3D_TexEnvSrc(env, C3D_Both, GPU_CONSTANT, 0, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

    C3D_DrawArrays(GPU_TRIANGLES, buffer_offset, 6); // 2 triangles
    buffer_offset += 6;
}

static void gfx_3ds_minimap_draw_background_color(float *vbo_buffer)
{
    gfx_3ds_minimap_draw_background_color_rgb(vbo_buffer, minimap_color);
}

static void gfx_3ds_minimap_draw_background(float *vbo_buffer)
{
    Mtx_Identity(&modelView);
    Mtx_Translate(&modelView, x_offset, y_offset, 0.0f, false);

    Mtx_OrthoTilt(&projBottom, 0.0, 320.0, 0.0, 240.0, 0.0, 1.0, true);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, &modelView);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projBottom);

    memcpy(vbo_buffer + buffer_offset * VERTEX_SHADER_SIZE,
           vertex_list_background,
           sizeof(vertex_list_background));

    C3D_TexBind(0, &minimap_tex);
    C3D_TexFlush(&minimap_tex);

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvColor(env, 0);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, 0, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

    C3D_DrawArrays(GPU_TRIANGLES, buffer_offset, 6); // 2 triangles
    buffer_offset += 6;
}

static void gfx_3ds_minimap_draw_mario(float *vbo_buffer)
{
    memcpy(vbo_buffer + buffer_offset * VERTEX_SHADER_SIZE,
           vertex_list_mario,
           sizeof(vertex_list_mario));

    Mtx_Identity(&modelView);
    // subtract y from 240 to flip y-axis
    Mtx_Translate(&modelView, mario_x + x_offset, 240.0f - mario_y + y_offset, 0.0f, false);

    Mtx_OrthoTilt(&projBottom, 0.0, 320.0, 0.0, 240.0, 0.0, 1.0, true);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, &modelView);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projBottom);

    C3D_TexBind(0, &minimap_mario_tex);
    C3D_TexFlush(&minimap_mario_tex);

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvColor(env, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, 0, 0);

    C3D_DrawArrays(GPU_TRIANGLES, buffer_offset, 6); // 2 triangles
    buffer_offset += 6;
}

static void gfx_3ds_minimap_draw_heading(float *vbo_buffer)
{
    memcpy(vbo_buffer + buffer_offset * VERTEX_SHADER_SIZE,
           vertex_list_arrow,
           sizeof(vertex_list_arrow));

    float angle = C3D_Angle(mario_direction);

    Mtx_Identity(&modelView);
    Mtx_RotateZ(&modelView, angle, false);

    float arrow_x_offset = 16.0f * sin(angle);
    float arrow_y_offset = -16.0f * cos(angle);

    Mtx_Translate(&modelView, mario_x + x_offset + arrow_x_offset,
                  240.0f - mario_y + y_offset + arrow_y_offset, 0.0f, false);

    Mtx_OrthoTilt(&projBottom, 0.0, 320.0, 0.0, 240.0, 0.0, 1.0, true);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, &modelView);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projBottom);

    C3D_TexBind(0, &minimap_arrow_tex);
    C3D_TexFlush(&minimap_arrow_tex);

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvColor(env, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, 0, 0);

    C3D_DrawArrays(GPU_TRIANGLES, buffer_offset, 6); // 2 triangles
    buffer_offset += 6;
}

static void gfx_3ds_minimap_draw_tex(float *vbo_buffer, C3D_Tex *tex, const vertex *verts, size_t verts_size,
                                     float x, float y)
{
    memcpy(vbo_buffer + buffer_offset * VERTEX_SHADER_SIZE, verts, verts_size);

    Mtx_Identity(&modelView);
    Mtx_Translate(&modelView, x, y, 0.0f, false);
    Mtx_OrthoTilt(&projBottom, 0.0, 320.0, 0.0, 240.0, 0.0, 1.0, true);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, &modelView);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projBottom);

    C3D_TexBind(0, tex);
    C3D_TexFlush(tex);

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvColor(env, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, 0, 0);

    C3D_DrawArrays(GPU_TRIANGLES, buffer_offset, 6);
    buffer_offset += 6;
}

static void gfx_3ds_minimap_draw_tex_sized(float *vbo_buffer, C3D_Tex *tex, float width, float height,
                                           float x, float y)
{
    vertex verts[] =
    {
        { {   0.0f,  0.0f, 0.5f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
        { {  width, height, 0.5f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
        { {  width,  0.0f, 0.5f, 1.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },

        { {   0.0f,  0.0f, 0.5f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
        { {   0.0f, height, 0.5f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
        { {  width, height, 0.5f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } }
    };

    gfx_3ds_minimap_draw_tex(vbo_buffer, tex, verts, sizeof(verts), x, y);
}

static void gfx_3ds_minimap_draw_tex_sized_alpha(float *vbo_buffer, C3D_Tex *tex, float width, float height,
                                                 float x, float y, u8 alpha)
{
    float a = (float) alpha / 255.0f;
    vertex verts[] =
    {
        { {   0.0f,  0.0f, 0.5f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, a } },
        { {  width, height, 0.5f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, a } },
        { {  width,  0.0f, 0.5f, 1.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, a } },

        { {   0.0f,  0.0f, 0.5f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, a } },
        { {   0.0f, height, 0.5f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, a } },
        { {  width, height, 0.5f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, a } }
    };

    memcpy(vbo_buffer + buffer_offset * VERTEX_SHADER_SIZE, verts, sizeof(verts));

    Mtx_Identity(&modelView);
    Mtx_Translate(&modelView, x, y, 0.0f, false);
    Mtx_OrthoTilt(&projBottom, 0.0, 320.0, 0.0, 240.0, 0.0, 1.0, true);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, &modelView);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projBottom);

    C3D_TexBind(0, tex);
    C3D_TexFlush(tex);

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvColor(env, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);

    C3D_DrawArrays(GPU_TRIANGLES, buffer_offset, 6);
    buffer_offset += 6;
}

static void gfx_3ds_minimap_draw_tex_centered_scaled_alpha(float *vbo_buffer, C3D_Tex *tex,
                                                           float width, float height, float centerX,
                                                           float centerY, float scaleX, float scaleY,
                                                           u8 alpha)
{
    float scaledWidth = width * scaleX;
    float scaledHeight = height * scaleY;

    gfx_3ds_minimap_draw_tex_sized_alpha(vbo_buffer, tex, scaledWidth, scaledHeight,
                                         centerX - scaledWidth * 0.5f,
                                         centerY - scaledHeight * 0.5f, alpha);
}

static void gfx_3ds_minimap_get_intro_logo_scale(float *scaleX, float *scaleY)
{
    if (gTitleZoomCounter < 8) {
        *scaleX = 0.55f + (0.57f * gTitleZoomCounter / 8.0f);
        *scaleY = 0.25f + (0.95f * gTitleZoomCounter / 8.0f);
    } else if (gTitleZoomCounter < 15) {
        *scaleX = 1.12f - (0.18f * (gTitleZoomCounter - 8) / 7.0f);
        *scaleY = 1.20f - (0.26f * (gTitleZoomCounter - 8) / 7.0f);
    } else if (gTitleZoomCounter < 22) {
        *scaleX = 0.94f + (0.06f * (gTitleZoomCounter - 15) / 7.0f);
        *scaleY = 0.94f + (0.06f * (gTitleZoomCounter - 15) / 7.0f);
    } else {
        *scaleX = 1.0f;
        *scaleY = 1.0f;
    }
}

static float gfx_3ds_minimap_draw_number_left(float *vbo_buffer, s16 value, float x, float y)
{
    s16 clamped = value;
    s16 digits[3];
    s16 digit_count = 0;
    s16 i;

    if (clamped < 0)
        clamped = 0;
    if (clamped > 999)
        clamped = 999;

    do {
        digits[digit_count++] = clamped % 10;
        clamped /= 10;
    } while (clamped > 0 && digit_count < 3);

    for (i = digit_count - 1; i >= 0; i--) {
        gfx_3ds_minimap_draw_tex(vbo_buffer, &hud_digit_tex[digits[i]], vertex_list_hud_digit,
                                 sizeof(vertex_list_hud_digit), x, y);
        x += 12.0f;
    }

    return x;
}

static s16 gfx_3ds_minimap_get_digit_count(s16 value)
{
    if (value < 0)
        value = 0;
    if (value > 999)
        value = 999;
    if (value >= 100)
        return 3;
    if (value >= 10)
        return 2;
    return 1;
}

static void gfx_3ds_minimap_draw_counter_right(float *vbo_buffer, C3D_Tex *icon, s16 value,
                                               float right, float y)
{
    s16 digit_count = gfx_3ds_minimap_get_digit_count(value);
    float digit_x = right - 16.0f - (digit_count - 1) * 12.0f;
    float x_x = digit_x - 14.0f;
    float icon_x = x_x - 16.0f;

    gfx_3ds_minimap_draw_tex(vbo_buffer, icon, vertex_list_hud_icon,
                             sizeof(vertex_list_hud_icon), icon_x, y);
    gfx_3ds_minimap_draw_tex(vbo_buffer, &hud_x_tex, vertex_list_hud_digit,
                             sizeof(vertex_list_hud_digit), x_x, y);
    gfx_3ds_minimap_draw_number_left(vbo_buffer, value, digit_x, y);
}

static void gfx_3ds_minimap_draw_health(float *vbo_buffer)
{
    s16 wedges = gHudDisplay.wedges;

    if (wedges < 0)
        wedges = 0;
    if (wedges > 8)
        wedges = 8;

    if (wedges == 0)
        return;

    gfx_3ds_minimap_draw_tex(vbo_buffer, &hud_health_tex[wedges - 1], vertex_list_hud_health,
                             sizeof(vertex_list_hud_health), 4.0f, 150.0f);
}

static void gfx_3ds_minimap_draw_red_coins(float *vbo_buffer)
{
    s8 redCoins = gRedCoinsCollected;
    u8 frame = (gGlobalTimer & 6) >> 1;
    s8 i;

    if (redCoins < 0)
        redCoins = 0;
    if (redCoins > 8)
        redCoins = 8;

    for (i = 0; i < redCoins; i++) {
        gfx_3ds_minimap_draw_tex_sized(vbo_buffer, &hud_red_coin_tex[frame],
                                       16.0f, 16.0f, 8.0f + i * 17.0f, 3.0f);
    }
}

static void gfx_3ds_minimap_draw_hud(float *vbo_buffer)
{
    s16 hudDisplayFlags = gHudDisplay.flags;

    if (hudDisplayFlags == HUD_DISPLAY_NONE)
        return;

    if (hudDisplayFlags & HUD_DISPLAY_FLAG_LIVES) {
        gfx_3ds_minimap_draw_tex(vbo_buffer, &minimap_mario_tex, vertex_list_hud_icon,
                                 sizeof(vertex_list_hud_icon), 8.0f, 218.0f);
        gfx_3ds_minimap_draw_tex(vbo_buffer, &hud_x_tex, vertex_list_hud_digit,
                                 sizeof(vertex_list_hud_digit), 24.0f, 218.0f);
        gfx_3ds_minimap_draw_number_left(vbo_buffer, gHudDisplay.lives, 40.0f, 218.0f);
    }

    gfx_3ds_minimap_draw_red_coins(vbo_buffer);

    if (hudDisplayFlags & HUD_DISPLAY_FLAG_CAMERA_AND_POWER) {
        gfx_3ds_minimap_draw_health(vbo_buffer);
    }

    if (hudDisplayFlags & HUD_DISPLAY_FLAG_STAR_COUNT) {
        gfx_3ds_minimap_draw_counter_right(vbo_buffer, &hud_star_tex, gHudDisplay.stars,
                                           316.0f, 218.0f);
    }

    if (hudDisplayFlags & HUD_DISPLAY_FLAG_COIN_COUNT) {
        gfx_3ds_minimap_draw_counter_right(vbo_buffer, &hud_coin_tex, gHudDisplay.coins,
                                           316.0f, 198.0f);
    }
}

static C3D_Tex *gfx_3ds_minimap_get_music_title_texture(u8 *seqIdOut)
{
    u16 seqArgs = get_current_background_music();
    u8 seqId = seqArgs & 0x7F;

    if (seqArgs == 0xFFFF || seqId >= SEQ_COUNT)
        return NULL;

    *seqIdOut = seqId;
    return &music_title_tex[seqId];
}

static void gfx_3ds_minimap_draw_music_title(float *vbo_buffer)
{
    u8 seqId = 0;
    C3D_Tex *tex = gfx_3ds_minimap_get_music_title_texture(&seqId);
    u16 width;

    if (tex == NULL)
        return;

    width = music_title_width[seqId];
    if (width == 0)
        width = 160;

    gfx_3ds_minimap_draw_tex_sized(vbo_buffer, tex, (float) width, 16.0f,
                                   316.0f - (float) width, 3.0f);
}

static void gfx_3ds_minimap_draw_music_title_alpha(float *vbo_buffer, u8 alpha)
{
    u8 seqId = 0;
    C3D_Tex *tex = gfx_3ds_minimap_get_music_title_texture(&seqId);
    u16 width;

    if (tex == NULL || alpha == 0)
        return;

    width = music_title_width[seqId];
    if (width == 0)
        width = 160;

    gfx_3ds_minimap_draw_tex_sized_alpha(vbo_buffer, tex, (float) width, 16.0f,
                                         316.0f - (float) width, 3.0f, alpha);
}

static void gfx_3ds_minimap_draw_intro_title(float *vbo_buffer)
{
    u8 seqId = 0;
    C3D_Tex *musicTex = gfx_3ds_minimap_get_music_title_texture(&seqId);
    u8 alpha = 255;
    u8 musicAlpha = 255;
    float scaleX = 1.0f;
    float scaleY = 1.0f;

    gfx_3ds_minimap_draw_background_color_rgb(vbo_buffer, 0x000000);

    if (gN3dsIntroScreenMode == 0 && seqId != SEQ_MENU_TITLE_SCREEN) {
        alpha = (gTitleFadeCounter < 0) ? 0 : (gTitleFadeCounter > 255 ? 255 : (u8) gTitleFadeCounter);
        if (gTitleZoomCounter >= 75 && gTitleZoomCounter < 91) {
            alpha = (u8) ((90 - gTitleZoomCounter) * 255 / 15);
        } else if (gTitleZoomCounter >= 91) {
            alpha = 0;
        }
        if (alpha != 0) {
            gfx_3ds_minimap_get_intro_logo_scale(&scaleX, &scaleY);
            gfx_3ds_minimap_draw_tex_centered_scaled_alpha(vbo_buffer, &title_1996_nintendo_tex,
                                                           128.0f, 16.0f, 160.0f, 120.0f,
                                                           scaleX, scaleY, alpha);
        }
    } else if (gN3dsIntroScreenMode == 1 || (musicTex != NULL && seqId == SEQ_MENU_TITLE_SCREEN)) {
        if ((gGlobalTimer & 0x1F) >= 20)
            alpha = 0;
        if (gN3dsIntroScreenExiting) {
            u32 elapsed = gGlobalTimer - gN3dsIntroScreenFadeStart;
            alpha = (elapsed >= 16) ? 0 : (u8) (alpha * (16 - elapsed) / 16);
            musicAlpha = (elapsed >= 16) ? 0 : (u8) (musicAlpha * (16 - elapsed) / 16);
        }
        if (alpha != 0) {
            gfx_3ds_minimap_draw_tex_sized_alpha(vbo_buffer, &title_press_start_tex,
                                                 256.0f, 16.0f, 32.0f, 112.0f, alpha);
        }
        gfx_3ds_minimap_draw_music_title_alpha(vbo_buffer, musicAlpha);
    }
}

static void gfx_3ds_minimap_load_hud_texture(C3D_Tex *tex, const void *data, size_t size)
{
    load_t3x_texture(tex, NULL, data, size);
    C3D_TexSetFilter(tex, GPU_NEAREST, GPU_NEAREST);
}

void gfx_3ds_init_minimap()
{
    load_t3x_texture(&minimap_mario_tex, NULL, mario_t3x, mario_t3x_size);
    C3D_TexSetFilter(&minimap_mario_tex, GPU_LINEAR, GPU_NEAREST);

    load_t3x_texture(&minimap_arrow_tex, NULL, arrow_t3x, arrow_t3x_size);
    C3D_TexSetFilter(&minimap_arrow_tex, GPU_LINEAR, GPU_NEAREST);

    gfx_3ds_minimap_load_hud_texture(&hud_coin_tex, hud_coin_t3x, hud_coin_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&hud_star_tex, hud_star_t3x, hud_star_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&hud_x_tex, hud_x_t3x, hud_x_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&hud_health_tex[0], hud_health_1_t3x, hud_health_1_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&hud_health_tex[1], hud_health_2_t3x, hud_health_2_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&hud_health_tex[2], hud_health_3_t3x, hud_health_3_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&hud_health_tex[3], hud_health_4_t3x, hud_health_4_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&hud_health_tex[4], hud_health_5_t3x, hud_health_5_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&hud_health_tex[5], hud_health_6_t3x, hud_health_6_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&hud_health_tex[6], hud_health_7_t3x, hud_health_7_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&hud_health_tex[7], hud_health_8_t3x, hud_health_8_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&hud_red_coin_tex[0], hud_red_coin_front_t3x, hud_red_coin_front_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&hud_red_coin_tex[1], hud_red_coin_tilt_right_t3x, hud_red_coin_tilt_right_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&hud_red_coin_tex[2], hud_red_coin_side_t3x, hud_red_coin_side_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&hud_red_coin_tex[3], hud_red_coin_tilt_left_t3x, hud_red_coin_tilt_left_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&hud_digit_tex[0], hud_digit_0_t3x, hud_digit_0_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&hud_digit_tex[1], hud_digit_1_t3x, hud_digit_1_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&hud_digit_tex[2], hud_digit_2_t3x, hud_digit_2_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&hud_digit_tex[3], hud_digit_3_t3x, hud_digit_3_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&hud_digit_tex[4], hud_digit_4_t3x, hud_digit_4_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&hud_digit_tex[5], hud_digit_5_t3x, hud_digit_5_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&hud_digit_tex[6], hud_digit_6_t3x, hud_digit_6_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&hud_digit_tex[7], hud_digit_7_t3x, hud_digit_7_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&hud_digit_tex[8], hud_digit_8_t3x, hud_digit_8_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&hud_digit_tex[9], hud_digit_9_t3x, hud_digit_9_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&title_1996_nintendo_tex, title_1996_nintendo_t3x, title_1996_nintendo_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&title_press_start_tex, title_press_start_t3x, title_press_start_t3x_size);

    gfx_3ds_minimap_load_hud_texture(&music_title_tex[SEQ_SOUND_PLAYER], music_silence_t3x, music_silence_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&music_title_tex[SEQ_EVENT_CUTSCENE_COLLECT_STAR], music_victory_t3x, music_victory_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&music_title_tex[SEQ_MENU_TITLE_SCREEN], music_title_theme_t3x, music_title_theme_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&music_title_tex[SEQ_LEVEL_GRASS], music_main_theme_t3x, music_main_theme_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&music_title_tex[SEQ_LEVEL_INSIDE_CASTLE], music_inside_castle_t3x, music_inside_castle_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&music_title_tex[SEQ_LEVEL_WATER], music_dire_dire_docks_t3x, music_dire_dire_docks_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&music_title_tex[SEQ_LEVEL_HOT], music_lethal_lava_land_t3x, music_lethal_lava_land_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&music_title_tex[SEQ_LEVEL_BOSS_KOOPA], music_bowser_battle_t3x, music_bowser_battle_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&music_title_tex[SEQ_LEVEL_SNOW], music_snow_mountain_t3x, music_snow_mountain_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&music_title_tex[SEQ_LEVEL_SLIDE], music_slider_t3x, music_slider_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&music_title_tex[SEQ_LEVEL_SPOOKY], music_haunted_house_t3x, music_haunted_house_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&music_title_tex[SEQ_EVENT_PIRANHA_PLANT], music_piranha_lullaby_t3x, music_piranha_lullaby_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&music_title_tex[SEQ_LEVEL_UNDERGROUND], music_hazy_maze_cave_t3x, music_hazy_maze_cave_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&music_title_tex[SEQ_MENU_STAR_SELECT], music_star_select_t3x, music_star_select_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&music_title_tex[SEQ_EVENT_POWERUP], music_powerful_mario_t3x, music_powerful_mario_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&music_title_tex[SEQ_EVENT_METAL_CAP], music_metal_mario_t3x, music_metal_mario_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&music_title_tex[SEQ_EVENT_KOOPA_MESSAGE], music_koopa_message_t3x, music_koopa_message_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&music_title_tex[SEQ_LEVEL_KOOPA_ROAD], music_koopas_road_t3x, music_koopas_road_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&music_title_tex[SEQ_EVENT_HIGH_SCORE], music_high_score_t3x, music_high_score_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&music_title_tex[SEQ_EVENT_MERRY_GO_ROUND], music_merry_go_round_t3x, music_merry_go_round_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&music_title_tex[SEQ_EVENT_RACE], music_race_fanfare_t3x, music_race_fanfare_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&music_title_tex[SEQ_EVENT_CUTSCENE_STAR_SPAWN], music_star_spawn_t3x, music_star_spawn_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&music_title_tex[SEQ_EVENT_BOSS], music_boss_battle_t3x, music_boss_battle_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&music_title_tex[SEQ_EVENT_CUTSCENE_COLLECT_KEY], music_key_clear_t3x, music_key_clear_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&music_title_tex[SEQ_EVENT_ENDLESS_STAIRS], music_endless_stairs_t3x, music_endless_stairs_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&music_title_tex[SEQ_LEVEL_BOSS_KOOPA_FINAL], music_final_bowser_t3x, music_final_bowser_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&music_title_tex[SEQ_EVENT_CUTSCENE_CREDITS], music_staff_roll_t3x, music_staff_roll_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&music_title_tex[SEQ_EVENT_SOLVE_PUZZLE], music_puzzle_solved_t3x, music_puzzle_solved_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&music_title_tex[SEQ_EVENT_TOAD_MESSAGE], music_toad_message_t3x, music_toad_message_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&music_title_tex[SEQ_EVENT_PEACH_MESSAGE], music_peach_message_t3x, music_peach_message_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&music_title_tex[SEQ_EVENT_CUTSCENE_INTRO], music_opening_t3x, music_opening_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&music_title_tex[SEQ_EVENT_CUTSCENE_VICTORY], music_victory_t3x, music_victory_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&music_title_tex[SEQ_EVENT_CUTSCENE_ENDING], music_ending_t3x, music_ending_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&music_title_tex[SEQ_MENU_FILE_SELECT], music_file_select_t3x, music_file_select_t3x_size);
    gfx_3ds_minimap_load_hud_texture(&music_title_tex[SEQ_EVENT_CUTSCENE_LAKITU], music_lakitu_t3x, music_lakitu_t3x_size);

    music_title_width[SEQ_SOUND_PLAYER] = 64;
    music_title_width[SEQ_EVENT_CUTSCENE_COLLECT_STAR] = 128;
    music_title_width[SEQ_MENU_TITLE_SCREEN] = 128;
    music_title_width[SEQ_LEVEL_GRASS] = 256;
    music_title_width[SEQ_LEVEL_INSIDE_CASTLE] = 256;
    music_title_width[SEQ_LEVEL_WATER] = 128;
    music_title_width[SEQ_LEVEL_HOT] = 128;
    music_title_width[SEQ_LEVEL_BOSS_KOOPA] = 128;
    music_title_width[SEQ_LEVEL_SNOW] = 128;
    music_title_width[SEQ_LEVEL_SLIDE] = 64;
    music_title_width[SEQ_LEVEL_SPOOKY] = 128;
    music_title_width[SEQ_EVENT_PIRANHA_PLANT] = 256;
    music_title_width[SEQ_LEVEL_UNDERGROUND] = 128;
    music_title_width[SEQ_MENU_STAR_SELECT] = 128;
    music_title_width[SEQ_EVENT_POWERUP] = 128;
    music_title_width[SEQ_EVENT_METAL_CAP] = 128;
    music_title_width[SEQ_EVENT_KOOPA_MESSAGE] = 128;
    music_title_width[SEQ_LEVEL_KOOPA_ROAD] = 128;
    music_title_width[SEQ_EVENT_HIGH_SCORE] = 128;
    music_title_width[SEQ_EVENT_MERRY_GO_ROUND] = 128;
    music_title_width[SEQ_EVENT_RACE] = 128;
    music_title_width[SEQ_EVENT_CUTSCENE_STAR_SPAWN] = 128;
    music_title_width[SEQ_EVENT_BOSS] = 128;
    music_title_width[SEQ_EVENT_CUTSCENE_COLLECT_KEY] = 128;
    music_title_width[SEQ_EVENT_ENDLESS_STAIRS] = 128;
    music_title_width[SEQ_LEVEL_BOSS_KOOPA_FINAL] = 128;
    music_title_width[SEQ_EVENT_CUTSCENE_CREDITS] = 128;
    music_title_width[SEQ_EVENT_SOLVE_PUZZLE] = 128;
    music_title_width[SEQ_EVENT_TOAD_MESSAGE] = 128;
    music_title_width[SEQ_EVENT_PEACH_MESSAGE] = 128;
    music_title_width[SEQ_EVENT_CUTSCENE_INTRO] = 64;
    music_title_width[SEQ_EVENT_CUTSCENE_VICTORY] = 128;
    music_title_width[SEQ_EVENT_CUTSCENE_ENDING] = 128;
    music_title_width[SEQ_MENU_FILE_SELECT] = 128;
    music_title_width[SEQ_EVENT_CUTSCENE_LAKITU] = 64;
}

bool show_minimap = false;

uint32_t gfx_3ds_draw_minimap(float *vertex_buffer, int vertex_offset)
{
    bool drew_minimap = false;
    bool preview_minimap;

    buffer_offset = vertex_offset;

    if (gN3dsIntroScreenMode >= 0) {
        gfx_3ds_minimap_draw_intro_title(vertex_buffer);
        return buffer_offset - vertex_offset;
    }

    if(minimap_has_level_or_area_changed())
    {
        show_minimap = minimap_load_level_and_area();
        if (show_minimap)
            minimap_load_new_minimap_texture();
        else
            minimap_unload_current_minimap_texture();
    }

    preview_minimap = minimap_is_level_select_preview_active();

    if (show_minimap && minimap_tex_loaded && preview_minimap)
    {
        gfx_3ds_minimap_draw_background_color(vertex_buffer);
        gfx_3ds_minimap_draw_background(vertex_buffer);
        drew_minimap = true;
    }
    else if (show_minimap && minimap_tex_loaded && minimap_get_mario_position(&mario_x, &mario_y, &mario_direction))
    {
        gfx_3ds_minimap_draw_background_color(vertex_buffer);
        gfx_3ds_minimap_draw_background(vertex_buffer);
        gfx_3ds_minimap_draw_mario(vertex_buffer);
        gfx_3ds_minimap_draw_heading(vertex_buffer);
        gfx_3ds_minimap_draw_hud(vertex_buffer);
        drew_minimap = true;
    }

    if (!drew_minimap)
        gfx_3ds_minimap_draw_background_color_rgb(vertex_buffer, 0x000000);

    if (!preview_minimap)
        gfx_3ds_minimap_draw_music_title(vertex_buffer);
    return buffer_offset - vertex_offset;
}

#endif
