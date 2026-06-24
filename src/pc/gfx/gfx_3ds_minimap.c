#ifdef TARGET_N3DS

#include <stdlib.h>

#include "gfx_3ds_minimap.h"
#include "gfx_3ds.h"
#include "audio/external.h"
#include "game/area.h"
#include "game/game_init.h"
#include "game/geo_misc.h"
#include "game/ingame_menu.h"
#include "game/memory.h"
#include "game/screen_transition.h"
#include "game/segment2.h"
#include "game/segment7.h"
#include "game/level_update.h"
#include "menu/file_select.h"
#include "menu/intro_geo.h"
#include "menu/level_select_menu.h"
#include "menu/star_select.h"
#include "level_table.h"
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
static C3D_Tex title_press_home_tex;
static C3D_Tex transition_tex[4];
static C3D_Tex music_title_tex[SEQ_COUNT];
static u16 music_title_width[SEQ_COUNT];
static C3D_Tex credits_font_tex[256];
static bool credits_font_tex_loaded[256];

static float mario_x, mario_y, mario_direction = 0.0f;
static bool cached_minimap_frame_valid = false;
static float cached_mario_x, cached_mario_y, cached_mario_direction = 0.0f;
static struct HudDisplay cached_hud_display;
static s8 cached_red_coins = 0;
static u8 cached_red_coin_frame = 0;
// 320 - 240 = 80, so add 40 to shift map into middle of the screen
static float x_offset = 40.0f;
// FIXME: just chop bottom 16px of texture for now
static float y_offset = 0.0f; //-16.0f;

static u32 buffer_offset = 0;
static bool show_minimap = false;
static u8 sBottomTransitionRevealHold = 0;
static u8 sBottomTransitionLastType = 0xFF;
static u8 sBottomTransitionStepGate = 0;
static u16 sStarSelectPressStartTimer = 0;
static bool sTransitionTexLoaded = false;
static struct CreditsEntry *sBottomCreditsLastEntry = NULL;
static u8 sBottomCreditsFadeTimer = 0;
static u16 sCakeEndPressHomeTimer = 0;

static bool gfx_3ds_minimap_is_star_select(void);
static bool gfx_3ds_minimap_is_staff_roll(void);
static void gfx_3ds_minimap_draw_star_select(float *vbo_buffer);
static void gfx_3ds_minimap_draw_staff_roll(float *vbo_buffer);
static void gfx_3ds_minimap_draw_cake_end_screen(float *vbo_buffer);
static void gfx_3ds_minimap_draw_music_title(float *vbo_buffer);

static uint32_t rgb_to_abgr(uint32_t rgb)
{
    // 0xRRGGBB to 0xFFBBGGRR
    return (0x0000ff & rgb >> 16)
        | (0x00ff00 & rgb)
        | (0xff0000 & rgb << 16)
        | 0xff000000;
}

static uint32_t rgba_to_abgr(u8 red, u8 green, u8 blue, u8 alpha)
{
    return ((uint32_t) alpha << 24) | ((uint32_t) blue << 16)
        | ((uint32_t) green << 8) | red;
}

static const int sMinimapTexTileOrder[] =
{
    0,  1,   4,  5,
    2,  3,   6,  7,

    8,  9,  12, 13,
    10, 11, 14, 15
};

static void gfx_3ds_minimap_swizzle_rgba8(const u8 *src, u32 *dst, u32 width, u32 height)
{
    int offs = 0;
    u32 y;

    for (y = 0; y < height; y += 8) {
        u32 x;
        for (x = 0; x < width; x += 8) {
            int i;
            for (i = 0; i < 64; i++) {
                int x2 = i & 7;
                int y2 = i >> 3;
                int pos = sMinimapTexTileOrder[(x2 & 3) + ((y2 & 3) << 2)]
                        + ((x2 >> 2) << 4) + ((y2 >> 2) << 5);
                u32 c = ((const u32 *) src)[(y + y2) * width + x + x2];

                dst[offs + pos] = ((c & 0xFF) << 24)
                    | (((c >> 8) & 0xFF) << 16)
                    | (((c >> 16) & 0xFF) << 8)
                    | (c >> 24);
            }
            offs += 64;
        }
    }
}

static bool gfx_3ds_minimap_load_ia8_transition_tex(C3D_Tex *tex, const u8 *data, u32 width, u32 height,
                                                    bool mirrored)
{
    static u8 rgba[64 * 64 * 4];
    static u32 swizzled[64 * 64];
    u32 i;

    if (width > 64 || height > 64)
        return false;

    for (i = 0; i < width * height; i++) {
        u8 intensity = ((data[i] >> 4) & 0xF) * 17;
        u8 alpha = (data[i] & 0xF) * 17;

        rgba[i * 4 + 0] = intensity;
        rgba[i * 4 + 1] = intensity;
        rgba[i * 4 + 2] = intensity;
        rgba[i * 4 + 3] = alpha;
    }

    gfx_3ds_minimap_swizzle_rgba8(rgba, swizzled, width, height);

    if (!C3D_TexInit(tex, width, height, GPU_RGBA8))
        return false;

    C3D_TexUpload(tex, swizzled);
    C3D_TexFlush(tex);
    C3D_TexSetFilter(tex, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(tex, mirrored ? GPU_MIRRORED_REPEAT : GPU_CLAMP_TO_EDGE,
                   mirrored ? GPU_MIRRORED_REPEAT : GPU_CLAMP_TO_EDGE);
    return true;
}

static void gfx_3ds_minimap_load_transition_textures(void)
{
    if (sTransitionTexLoaded)
        return;

    sTransitionTexLoaded =
        gfx_3ds_minimap_load_ia8_transition_tex(&transition_tex[TEX_TRANS_STAR],
                                                texture_transition_star_half, 32, 64, true)
        && gfx_3ds_minimap_load_ia8_transition_tex(&transition_tex[TEX_TRANS_CIRCLE],
                                                   texture_transition_circle_half, 32, 64, true)
        && gfx_3ds_minimap_load_ia8_transition_tex(&transition_tex[TEX_TRANS_MARIO],
                                                   texture_transition_mario, 64, 64, false)
        && gfx_3ds_minimap_load_ia8_transition_tex(&transition_tex[TEX_TRANS_BOWSER],
                                                   texture_transition_bowser_half, 32, 64, true);
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

static void gfx_3ds_minimap_draw_color_overlay(float *vbo_buffer, u8 red, u8 green, u8 blue, u8 alpha)
{
    if (alpha == 0)
        return;

    Mtx_Identity(&modelView);
    Mtx_OrthoTilt(&projBottom, 0.0, 320.0, 0.0, 240.0, 0.0, 1.0, true);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, &modelView);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projBottom);

    memcpy(vbo_buffer + buffer_offset * VERTEX_SHADER_SIZE,
           vertex_list_color,
           sizeof(vertex_list_color));

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvColor(env, rgba_to_abgr(red, green, blue, alpha));
    C3D_TexEnvSrc(env, C3D_Both, GPU_CONSTANT, 0, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
                   GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
    C3D_DrawArrays(GPU_TRIANGLES, buffer_offset, 6);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
    buffer_offset += 6;
}

static void gfx_3ds_minimap_draw_rect_overlay(float *vbo_buffer, float x0, float y0, float x1, float y1,
                                              u8 red, u8 green, u8 blue, u8 alpha)
{
    vertex verts[6];
    float a = (float) alpha / 255.0f;

    if (alpha == 0 || x0 >= x1 || y0 >= y1)
        return;

    verts[0] = (vertex) { { x0, y0, 0.5f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, a } };
    verts[1] = (vertex) { { x1, y1, 0.5f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, a } };
    verts[2] = (vertex) { { x1, y0, 0.5f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, a } };
    verts[3] = (vertex) { { x0, y0, 0.5f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, a } };
    verts[4] = (vertex) { { x0, y1, 0.5f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, a } };
    verts[5] = (vertex) { { x1, y1, 0.5f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, a } };

    Mtx_Identity(&modelView);
    Mtx_OrthoTilt(&projBottom, 0.0, 320.0, 0.0, 240.0, 0.0, 1.0, true);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, &modelView);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projBottom);

    memcpy(vbo_buffer + buffer_offset * VERTEX_SHADER_SIZE, verts, sizeof(verts));

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvColor(env, rgba_to_abgr(red, green, blue, alpha));
    C3D_TexEnvSrc(env, C3D_Both, GPU_CONSTANT, 0, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
                   GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
    C3D_DrawArrays(GPU_TRIANGLES, buffer_offset, 6);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
    buffer_offset += 6;
}

static void gfx_3ds_minimap_draw_transition_fill(float *vbo_buffer, float radius,
                                                 u8 red, u8 green, u8 blue)
{
    const float centerX = 160.0f;
    const float centerY = 120.0f;
    float left = centerX - radius;
    float right = centerX + radius;
    float bottom = centerY - radius;
    float top = centerY + radius;

    if (radius <= 1.0f) {
        gfx_3ds_minimap_draw_color_overlay(vbo_buffer, red, green, blue, 255);
        return;
    }
    if (left <= 0.0f && right >= 320.0f && bottom <= 0.0f && top >= 240.0f)
        return;

    gfx_3ds_minimap_draw_rect_overlay(vbo_buffer, 0.0f, 0.0f, 320.0f, bottom, red, green, blue, 255);
    gfx_3ds_minimap_draw_rect_overlay(vbo_buffer, 0.0f, top, 320.0f, 240.0f, red, green, blue, 255);
    gfx_3ds_minimap_draw_rect_overlay(vbo_buffer, 0.0f, bottom, left, top, red, green, blue, 255);
    gfx_3ds_minimap_draw_rect_overlay(vbo_buffer, right, bottom, 320.0f, top, red, green, blue, 255);
}

static void gfx_3ds_minimap_draw_transition_texture_quad(float *vbo_buffer, C3D_Tex *tex,
                                                         float radius, bool mirrored,
                                                         u8 red, u8 green, u8 blue)
{
    const float centerX = 160.0f;
    const float centerY = 120.0f;
    float left = centerX - radius;
    float right = centerX + radius;
    float bottom = centerY - radius;
    float top = centerY + radius;
    float u0 = mirrored ? (-31.0f / 32.0f) : 0.0f;
    float u1 = mirrored ? (31.0f / 32.0f) : 1.0f;
    vertex verts[6];

    if (radius <= 1.0f)
        return;

    verts[0] = (vertex) { { left, bottom, 0.5f, 1.0f }, { u0, 0.0f }, { red / 255.0f, green / 255.0f, blue / 255.0f, 1.0f } };
    verts[1] = (vertex) { { right, top, 0.5f, 1.0f }, { u1, 1.0f }, { red / 255.0f, green / 255.0f, blue / 255.0f, 1.0f } };
    verts[2] = (vertex) { { right, bottom, 0.5f, 1.0f }, { u1, 0.0f }, { red / 255.0f, green / 255.0f, blue / 255.0f, 1.0f } };
    verts[3] = (vertex) { { left, bottom, 0.5f, 1.0f }, { u0, 0.0f }, { red / 255.0f, green / 255.0f, blue / 255.0f, 1.0f } };
    verts[4] = (vertex) { { left, top, 0.5f, 1.0f }, { u0, 1.0f }, { red / 255.0f, green / 255.0f, blue / 255.0f, 1.0f } };
    verts[5] = (vertex) { { right, top, 0.5f, 1.0f }, { u1, 1.0f }, { red / 255.0f, green / 255.0f, blue / 255.0f, 1.0f } };

    memcpy(vbo_buffer + buffer_offset * VERTEX_SHADER_SIZE, verts, sizeof(verts));

    Mtx_Identity(&modelView);
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

    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
                   GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
    C3D_DrawArrays(GPU_TRIANGLES, buffer_offset, 6);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
    buffer_offset += 6;
}

static bool gfx_3ds_minimap_get_transition_texture(u8 type, C3D_Tex **tex, bool *mirrored)
{
    switch (type) {
        case WARP_TRANSITION_FADE_FROM_STAR:
        case WARP_TRANSITION_FADE_INTO_STAR:
            *tex = &transition_tex[TEX_TRANS_STAR];
            *mirrored = true;
            return true;
        case WARP_TRANSITION_FADE_FROM_CIRCLE:
        case WARP_TRANSITION_FADE_INTO_CIRCLE:
            *tex = &transition_tex[TEX_TRANS_CIRCLE];
            *mirrored = true;
            return true;
        case WARP_TRANSITION_FADE_FROM_MARIO:
        case WARP_TRANSITION_FADE_INTO_MARIO:
            *tex = &transition_tex[TEX_TRANS_MARIO];
            *mirrored = false;
            return true;
        case WARP_TRANSITION_FADE_FROM_BOWSER:
        case WARP_TRANSITION_FADE_INTO_BOWSER:
            *tex = &transition_tex[TEX_TRANS_BOWSER];
            *mirrored = true;
            return true;
    }

    return false;
}

static void gfx_3ds_minimap_draw_textured_transition(float *vbo_buffer, u8 type, u8 alpha)
{
    C3D_Tex *tex;
    bool mirrored;
    bool fadeInto = type & 1;
    float progress = fadeInto ? ((float) alpha / 255.0f) : (1.0f - ((float) alpha / 255.0f));
    float startRadius = fadeInto ? 320.0f : (type >= WARP_TRANSITION_FADE_FROM_MARIO ? 16.0f : 0.0f);
    float endRadius = fadeInto ? (type >= WARP_TRANSITION_FADE_FROM_MARIO ? 16.0f : 0.0f) : 320.0f;
    float radius = startRadius + (endRadius - startRadius) * progress;

    if (!sTransitionTexLoaded || !gfx_3ds_minimap_get_transition_texture(type, &tex, &mirrored)) {
        gfx_3ds_minimap_draw_color_overlay(vbo_buffer, gN3dsBottomTransitionRed, gN3dsBottomTransitionGreen,
                                           gN3dsBottomTransitionBlue, alpha);
        return;
    }

    if (fadeInto && alpha == 255) {
        gfx_3ds_minimap_draw_color_overlay(vbo_buffer, gN3dsBottomTransitionRed,
                                           gN3dsBottomTransitionGreen,
                                           gN3dsBottomTransitionBlue, 255);
        return;
    }

    gfx_3ds_minimap_draw_transition_fill(vbo_buffer, radius, gN3dsBottomTransitionRed,
                                         gN3dsBottomTransitionGreen, gN3dsBottomTransitionBlue);
    gfx_3ds_minimap_draw_transition_texture_quad(vbo_buffer, tex, radius, mirrored,
                                                 gN3dsBottomTransitionRed, gN3dsBottomTransitionGreen,
                                                 gN3dsBottomTransitionBlue);
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
    static float prevScaleX = 0.0f;
    static float prevScaleY = 0.0f;
    float targetScaleX;
    float targetScaleY;

    if (gTitleZoomCounter >= 0 && gTitleZoomCounter < 20) {
        targetScaleX = intro_seg7_table_0700C790[gTitleZoomCounter * 3];
        targetScaleY = intro_seg7_table_0700C790[gTitleZoomCounter * 3 + 1];
    } else if (gTitleZoomCounter >= 75 && gTitleZoomCounter < 91) {
        targetScaleX = intro_seg7_table_0700C880[(gTitleZoomCounter - 75) * 3];
        targetScaleY = intro_seg7_table_0700C880[(gTitleZoomCounter - 75) * 3 + 1];
    } else {
        targetScaleX = 1.0f;
        targetScaleY = 1.0f;
    }

    if (gTitleZoomCounter <= 0) {
        prevScaleX = 0.0f;
        prevScaleY = 0.0f;
    }

    *scaleX = (prevScaleX + targetScaleX) * 0.5f;
    *scaleY = (prevScaleY + targetScaleY) * 0.5f;
    prevScaleX = targetScaleX;
    prevScaleY = targetScaleY;
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

static void gfx_3ds_minimap_draw_health(float *vbo_buffer, s16 wedges)
{
    if (wedges < 0)
        wedges = 0;
    if (wedges > 8)
        wedges = 8;

    if (wedges == 0)
        return;

    gfx_3ds_minimap_draw_tex(vbo_buffer, &hud_health_tex[wedges - 1], vertex_list_hud_health,
                             sizeof(vertex_list_hud_health), 4.0f, 150.0f);
}

static void gfx_3ds_minimap_draw_red_coins(float *vbo_buffer, s8 redCoins, u8 frame)
{
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

static void gfx_3ds_minimap_draw_hud_snapshot(float *vbo_buffer, const struct HudDisplay *hud, s8 redCoins,
                                              u8 redCoinFrame)
{
    s16 hudDisplayFlags = hud->flags;

    if (hudDisplayFlags == HUD_DISPLAY_NONE)
        return;

    if (hudDisplayFlags & HUD_DISPLAY_FLAG_LIVES) {
        gfx_3ds_minimap_draw_tex(vbo_buffer, &minimap_mario_tex, vertex_list_hud_icon,
                                 sizeof(vertex_list_hud_icon), 8.0f, 218.0f);
        gfx_3ds_minimap_draw_tex(vbo_buffer, &hud_x_tex, vertex_list_hud_digit,
                                 sizeof(vertex_list_hud_digit), 24.0f, 218.0f);
        gfx_3ds_minimap_draw_number_left(vbo_buffer, hud->lives, 40.0f, 218.0f);
    }

    gfx_3ds_minimap_draw_red_coins(vbo_buffer, redCoins, redCoinFrame);

    if (hudDisplayFlags & HUD_DISPLAY_FLAG_CAMERA_AND_POWER) {
        gfx_3ds_minimap_draw_health(vbo_buffer, hud->wedges);
    }

    if (hudDisplayFlags & HUD_DISPLAY_FLAG_STAR_COUNT) {
        gfx_3ds_minimap_draw_counter_right(vbo_buffer, &hud_star_tex, hud->stars,
                                           316.0f, 218.0f);
    }

    if (hudDisplayFlags & HUD_DISPLAY_FLAG_COIN_COUNT) {
        gfx_3ds_minimap_draw_counter_right(vbo_buffer, &hud_coin_tex, hud->coins,
                                           316.0f, 198.0f);
    }
}

static void gfx_3ds_minimap_cache_current_frame(void)
{
    cached_minimap_frame_valid = true;
    cached_mario_x = mario_x;
    cached_mario_y = mario_y;
    cached_mario_direction = mario_direction;
    cached_hud_display = gHudDisplay;
    cached_red_coins = gRedCoinsCollected;
    cached_red_coin_frame = (gGlobalTimer & 6) >> 1;
}

static void gfx_3ds_minimap_draw_hud(float *vbo_buffer)
{
    gfx_3ds_minimap_draw_hud_snapshot(vbo_buffer, &gHudDisplay, gRedCoinsCollected,
                                      (gGlobalTimer & 6) >> 1);
}

static void gfx_3ds_minimap_draw_cached_frame(float *vbo_buffer)
{
    gfx_3ds_minimap_draw_background_color(vbo_buffer);
    gfx_3ds_minimap_draw_background(vbo_buffer);

    if (cached_minimap_frame_valid) {
        mario_x = cached_mario_x;
        mario_y = cached_mario_y;
        mario_direction = cached_mario_direction;
        gfx_3ds_minimap_draw_mario(vbo_buffer);
        gfx_3ds_minimap_draw_heading(vbo_buffer);
        gfx_3ds_minimap_draw_hud_snapshot(vbo_buffer, &cached_hud_display, cached_red_coins,
                                          cached_red_coin_frame);
    } else {
        gfx_3ds_minimap_draw_hud(vbo_buffer);
    }
}

static bool gfx_3ds_minimap_draw_live_frame_no_cache(float *vbo_buffer)
{
    if (!show_minimap || !minimap_tex_loaded)
        return false;

    gfx_3ds_minimap_draw_background_color(vbo_buffer);
    gfx_3ds_minimap_draw_background(vbo_buffer);

    if (minimap_get_mario_position(&mario_x, &mario_y, &mario_direction)) {
        gfx_3ds_minimap_draw_mario(vbo_buffer);
        gfx_3ds_minimap_draw_heading(vbo_buffer);
        gfx_3ds_minimap_cache_current_frame();
    }

    gfx_3ds_minimap_draw_hud(vbo_buffer);
    return true;
}

static u8 gfx_3ds_minimap_get_bottom_transition_alpha(void)
{
    u8 frame;
    u8 alpha;

    if (gWarpTransition.pauseRendering)
        return 255;

    if (!gN3dsBottomTransitionActive) {
        sBottomTransitionLastType = 0xFF;
        sBottomTransitionRevealHold = 0;
        sBottomTransitionStepGate = 0;
        return 0;
    }

    if (gN3dsBottomTransitionDelay > 0) {
        gN3dsBottomTransitionDelay--;
        return 0;
    }

    if (gN3dsBottomTransitionTime <= 1) {
        gN3dsBottomTransitionActive = FALSE;
        return 255;
    }

    if (sBottomTransitionLastType != gN3dsBottomTransitionType) {
        sBottomTransitionLastType = gN3dsBottomTransitionType;
        sBottomTransitionRevealHold = (gN3dsBottomTransitionType & 1) ? 0 : 1;
        sBottomTransitionStepGate = 0;
    }

    if (!(gN3dsBottomTransitionType & 1) && sBottomTransitionRevealHold > 0) {
        sBottomTransitionRevealHold--;
        return 255;
    }

    frame = gN3dsBottomTransitionFrame;
    if (frame >= gN3dsBottomTransitionTime)
        frame = gN3dsBottomTransitionTime - 1;

    sBottomTransitionStepGate ^= 1;
    if (sBottomTransitionStepGate && gN3dsBottomTransitionFrame < gN3dsBottomTransitionTime)
        gN3dsBottomTransitionFrame++;

    if (gN3dsBottomTransitionFrame > gN3dsBottomTransitionTime)
        gN3dsBottomTransitionFrame = gN3dsBottomTransitionTime;
    if (!(gN3dsBottomTransitionType & 1) && gN3dsBottomTransitionFrame >= gN3dsBottomTransitionTime)
        gN3dsBottomTransitionActive = FALSE;

    if (gN3dsBottomTransitionType & 1) {
        alpha = (u8) ((float) frame * 255.0f / (float) (gN3dsBottomTransitionTime - 1) + 0.5f);
    } else {
        alpha = (u8) ((1.0f - ((float) frame / (float) (gN3dsBottomTransitionTime - 1))) * 255.0f + 0.5f);
    }

    return alpha;
}

static void gfx_3ds_minimap_draw_bottom_transition(float *vbo_buffer)
{
    u8 alpha = gfx_3ds_minimap_get_bottom_transition_alpha();

    if (alpha == 0)
        return;

    if (gN3dsBottomTransitionType < WARP_TRANSITION_FADE_FROM_STAR) {
        gfx_3ds_minimap_draw_color_overlay(vbo_buffer, gN3dsBottomTransitionRed, gN3dsBottomTransitionGreen,
                                           gN3dsBottomTransitionBlue, alpha);
    } else {
        gfx_3ds_minimap_draw_textured_transition(vbo_buffer, gN3dsBottomTransitionType, alpha);
    }
}

static bool gfx_3ds_minimap_transition_holds_frame(void)
{
    return gWarpTransition.pauseRendering
        || (gN3dsBottomTransitionActive && gN3dsBottomTransitionDelay <= 0);
}

static bool gfx_3ds_minimap_transition_is_fully_black(void)
{
    if (gWarpTransition.pauseRendering)
        return true;

    if (!gN3dsBottomTransitionActive || gN3dsBottomTransitionDelay > 0)
        return false;

    if (!(gN3dsBottomTransitionType & 1))
        return false;

    return gN3dsBottomTransitionFrame + 1 >= gN3dsBottomTransitionTime;
}

static bool gfx_3ds_minimap_is_file_select_exit_transition(void)
{
    return gN3dsFileSelectExiting
        && gN3dsBottomTransitionActive
        && gN3dsBottomTransitionDelay <= 0
        && gN3dsBottomTransitionType == WARP_TRANSITION_FADE_INTO_COLOR
        && gN3dsBottomTransitionRed == 0xFF
        && gN3dsBottomTransitionGreen == 0xFF
        && gN3dsBottomTransitionBlue == 0xFF;
}

static void gfx_3ds_minimap_draw_transition_screen(float *vbo_buffer)
{
    if (gfx_3ds_minimap_is_star_select()) {
        gfx_3ds_minimap_draw_star_select(vbo_buffer);
        gfx_3ds_minimap_draw_bottom_transition(vbo_buffer);
        return;
    } else if (gfx_3ds_minimap_is_staff_roll()) {
        gfx_3ds_minimap_draw_staff_roll(vbo_buffer);
        gfx_3ds_minimap_draw_bottom_transition(vbo_buffer);
        gfx_3ds_minimap_draw_music_title(vbo_buffer);
        return;
    } else if ((gN3dsBottomTransitionType & 1) && show_minimap && minimap_tex_loaded) {
        sStarSelectPressStartTimer = 0;
        gfx_3ds_minimap_draw_cached_frame(vbo_buffer);
    } else if (!(gN3dsBottomTransitionType & 1) && gfx_3ds_minimap_draw_live_frame_no_cache(vbo_buffer)) {
        sStarSelectPressStartTimer = 0;
        // Fade-from transitions reveal the new map through the matching texture mask.
    } else {
        sStarSelectPressStartTimer = 0;
        gfx_3ds_minimap_draw_background_color_rgb(vbo_buffer, 0x000000);
        if (gfx_3ds_minimap_is_file_select_exit_transition()) {
            gfx_3ds_minimap_draw_music_title(vbo_buffer);
            gfx_3ds_minimap_draw_bottom_transition(vbo_buffer);
            return;
        }
        if (!minimap_is_level_select_preview_active())
            gfx_3ds_minimap_draw_music_title(vbo_buffer);
        return;
    }
    if (!minimap_is_level_select_preview_active())
        gfx_3ds_minimap_draw_music_title(vbo_buffer);
    gfx_3ds_minimap_draw_bottom_transition(vbo_buffer);
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

static bool gfx_3ds_minimap_load_credits_glyph(u8 glyph)
{
    void **fontLUT;
    const u8 *src;
    u8 rgba[8 * 8 * 4];
    u32 swizzled[8 * 8];
    u32 x;
    u32 y;

    if (credits_font_tex_loaded[glyph])
        return true;

    fontLUT = segmented_to_virtual(main_credits_font_lut);
    src = fontLUT[glyph];
    if (src == NULL)
        return false;

    for (y = 0; y < 8; y++) {
        for (x = 0; x < 8; x++) {
            u32 srcIndex = (y * 8 + x) * 2;
            u32 dstIndex = (y * 8 + x) * 4;
            u16 color = ((u16) src[srcIndex] << 8) | src[srcIndex + 1];
        u8 red = ((color >> 11) & 0x1F) * 255 / 31;
        u8 green = ((color >> 6) & 0x1F) * 255 / 31;
        u8 blue = ((color >> 1) & 0x1F) * 255 / 31;
        u8 alpha = (color & 1) ? 255 : 0;

            rgba[dstIndex + 0] = red;
            rgba[dstIndex + 1] = green;
            rgba[dstIndex + 2] = blue;
            rgba[dstIndex + 3] = alpha;
        }
    }

    gfx_3ds_minimap_swizzle_rgba8(rgba, swizzled, 8, 8);
    if (!C3D_TexInit(&credits_font_tex[glyph], 8, 8, GPU_RGBA8))
        return false;

    C3D_TexUpload(&credits_font_tex[glyph], swizzled);
    C3D_TexFlush(&credits_font_tex[glyph]);
    C3D_TexSetFilter(&credits_font_tex[glyph], GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(&credits_font_tex[glyph], GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    credits_font_tex_loaded[glyph] = true;
    return true;
}

static s16 gfx_3ds_minimap_get_credits_str_width(const char *str)
{
    s16 width = 0;

    while (*str != '\0') {
        width += (*str == ' ') ? 4 : 7;
        str++;
    }

    return width;
}

static s16 gfx_3ds_minimap_max_s16(s16 a, s16 b)
{
    return a > b ? a : b;
}

static s16 gfx_3ds_minimap_min_s16(s16 a, s16 b)
{
    return a < b ? a : b;
}

static void gfx_3ds_minimap_draw_credits_str(float *vbo_buffer, s16 x, s16 y,
                                             const char *str, u8 alpha)
{
    while (*str != '\0') {
        if (*str == ' ') {
            x += 4;
        } else {
            u8 glyph = ascii_to_credits_char(*str);
            if (gfx_3ds_minimap_load_credits_glyph(glyph)) {
                gfx_3ds_minimap_draw_tex_sized_alpha(vbo_buffer, &credits_font_tex[glyph],
                                                     8.0f, 8.0f, (float) x, (float) y, alpha);
            }
            x += 7;
        }
        str++;
    }
}

static void gfx_3ds_minimap_draw_cake_end_screen(float *vbo_buffer)
{
    enum {
        PRESS_HOME_DELAY_FRAMES = 720,
        PRESS_HOME_BLINK_PERIOD = 32,
        PRESS_HOME_BLINK_OFF_FRAME = 22,
    };
    u8 alpha = 255;
    u16 blinkTimer;

    gfx_3ds_minimap_draw_background_color_rgb(vbo_buffer, 0x000000);

    if (sCakeEndPressHomeTimer < PRESS_HOME_DELAY_FRAMES) {
        gfx_3ds_minimap_draw_music_title(vbo_buffer);
        sCakeEndPressHomeTimer++;
        return;
    }

    blinkTimer = sCakeEndPressHomeTimer - PRESS_HOME_DELAY_FRAMES;

    if ((blinkTimer % PRESS_HOME_BLINK_PERIOD) >= PRESS_HOME_BLINK_OFF_FRAME) {
        alpha = 0;
    }

    if (alpha != 0) {
        gfx_3ds_minimap_draw_tex_sized_alpha(vbo_buffer, &title_press_home_tex,
                                             256.0f, 16.0f, 32.0f, 112.0f, alpha);
    }

    if (sCakeEndPressHomeTimer < 0xFFFF) {
        sCakeEndPressHomeTimer++;
    }
}

static bool gfx_3ds_minimap_is_staff_roll(void)
{
    u16 seqArgs = get_current_background_music();
    u8 seqId = seqArgs & 0x7F;

    return gCurrCreditsEntry != NULL
        || gN3dsBottomCreditsEntry != NULL
        || (seqArgs != 0xFFFF && seqId == SEQ_EVENT_CUTSCENE_CREDITS);
}

static void gfx_3ds_minimap_draw_bottom_credits_entry(float *vbo_buffer)
{
    enum {
        CREDIT_BOTTOM_ORIGINAL_LEFT_X = 21,
        CREDIT_BOTTOM_ORIGINAL_RIGHT_X = 299,
        CREDIT_BOTTOM_CENTER_X = 160,
        CREDIT_BOTTOM_CENTER_Y = 126,
        CREDIT_BOTTOM_GLYPH_HEIGHT = 8,
    };
    struct CreditsEntry *entry = gN3dsBottomCreditsEntry;
    const char **currStrPtr;
    const char *titleStr;
    const char *lineStr;
    s16 numLines;
    s16 lineHeight = 16;
    s16 baseY;
    s16 strY;
    s16 minX;
    s16 maxX;
    s16 minY;
    s16 maxY;
    s16 offsetX;
    s16 offsetY;
    s16 width;
    u8 alpha;

    if (gfx_3ds_minimap_transition_is_fully_black()) {
        gN3dsBottomCreditsEntry = NULL;
        entry = NULL;
    }

    if (entry == NULL) {
        sBottomCreditsLastEntry = NULL;
        sBottomCreditsFadeTimer = 0;
        return;
    }

    if (entry != sBottomCreditsLastEntry) {
        sBottomCreditsLastEntry = entry;
        sBottomCreditsFadeTimer = 0;
    } else if (sBottomCreditsFadeTimer < 16) {
        sBottomCreditsFadeTimer++;
    }
    alpha = sBottomCreditsFadeTimer >= 16 ? 255 : (u8) (sBottomCreditsFadeTimer * 255 / 16);

    currStrPtr = entry->unk0C;
    titleStr = *currStrPtr++;
    numLines = *titleStr++ - '0';
    baseY = (numLines == 1) * 16;
    strY = baseY;

    minX = CREDIT_BOTTOM_ORIGINAL_LEFT_X;
    maxX = CREDIT_BOTTOM_ORIGINAL_LEFT_X + gfx_3ds_minimap_get_credits_str_width(titleStr);
    minY = strY;
    maxY = strY + CREDIT_BOTTOM_GLYPH_HEIGHT;

    switch (numLines) {
        case 4:
            lineStr = *currStrPtr++;
            width = gfx_3ds_minimap_get_credits_str_width(lineStr);
            maxX = gfx_3ds_minimap_max_s16(maxX, CREDIT_BOTTOM_ORIGINAL_LEFT_X + width);
            maxY = gfx_3ds_minimap_max_s16(maxY, strY + 24 + CREDIT_BOTTOM_GLYPH_HEIGHT);
            numLines = 2;
            lineHeight = 24;
            break;
        case 5:
            lineStr = *currStrPtr++;
            width = gfx_3ds_minimap_get_credits_str_width(lineStr);
            maxX = gfx_3ds_minimap_max_s16(maxX, CREDIT_BOTTOM_ORIGINAL_LEFT_X + width);
            maxY = gfx_3ds_minimap_max_s16(maxY, strY + 16 + CREDIT_BOTTOM_GLYPH_HEIGHT);
            numLines = 3;
            break;
    }

    while (numLines-- > 0) {
        const char *name = *currStrPtr++;
        width = gfx_3ds_minimap_get_credits_str_width(name);
        minX = gfx_3ds_minimap_min_s16(minX, CREDIT_BOTTOM_ORIGINAL_RIGHT_X - width);
        maxX = gfx_3ds_minimap_max_s16(maxX, CREDIT_BOTTOM_ORIGINAL_RIGHT_X);
        maxY = gfx_3ds_minimap_max_s16(maxY, strY + CREDIT_BOTTOM_GLYPH_HEIGHT);
        strY += lineHeight;
    }

    offsetX = CREDIT_BOTTOM_CENTER_X - (minX + maxX) / 2;
    offsetY = CREDIT_BOTTOM_CENTER_Y - (minY + maxY) / 2;

    currStrPtr = entry->unk0C;
    titleStr = *currStrPtr++;
    numLines = *titleStr++ - '0';
    lineHeight = 16;
    strY = baseY + offsetY;

    gfx_3ds_minimap_draw_credits_str(vbo_buffer,
                                     CREDIT_BOTTOM_ORIGINAL_LEFT_X + offsetX,
                                     strY, titleStr, alpha);

    switch (numLines) {
        case 4:
            lineStr = *currStrPtr++;
            gfx_3ds_minimap_draw_credits_str(vbo_buffer,
                                             CREDIT_BOTTOM_ORIGINAL_LEFT_X + offsetX,
                                             strY + 24, lineStr, alpha);
            numLines = 2;
            lineHeight = 24;
            break;
        case 5:
            lineStr = *currStrPtr++;
            gfx_3ds_minimap_draw_credits_str(vbo_buffer,
                                             CREDIT_BOTTOM_ORIGINAL_LEFT_X + offsetX,
                                             strY + 16, lineStr, alpha);
            numLines = 3;
            break;
    }

    while (numLines-- > 0) {
        const char *name = *currStrPtr++;
        gfx_3ds_minimap_draw_credits_str(vbo_buffer,
                                         CREDIT_BOTTOM_ORIGINAL_RIGHT_X + offsetX
                                             - gfx_3ds_minimap_get_credits_str_width(name),
                                         strY, name, alpha);
        strY += lineHeight;
    }
}

static bool gfx_3ds_minimap_is_star_select(void)
{
    u16 seqArgs = get_current_background_music();
    u8 seqId = seqArgs & 0x7F;

    return gN3dsStarSelectActive || (seqArgs != 0xFFFF && seqId == SEQ_MENU_STAR_SELECT);
}

static void gfx_3ds_minimap_draw_staff_roll(float *vbo_buffer)
{
    sStarSelectPressStartTimer = 0;
    gfx_3ds_minimap_draw_background_color_rgb(vbo_buffer, 0x000000);
    gfx_3ds_minimap_draw_bottom_credits_entry(vbo_buffer);
}

static void gfx_3ds_minimap_draw_star_select(float *vbo_buffer)
{
    enum { PRESS_START_DELAY_FRAMES = 24 };
    u8 alpha = 255;
    u16 blinkTimer;

    gfx_3ds_minimap_draw_background_color_rgb(vbo_buffer, 0x000000);

    if (sStarSelectPressStartTimer < PRESS_START_DELAY_FRAMES) {
        alpha = 0;
    } else {
        blinkTimer = sStarSelectPressStartTimer - PRESS_START_DELAY_FRAMES;
        if ((blinkTimer & 0x1F) >= 20)
            alpha = 0;
    }

    if (sStarSelectPressStartTimer < 0xFFFF)
        sStarSelectPressStartTimer++;

    if (alpha != 0) {
        gfx_3ds_minimap_draw_tex_sized_alpha(vbo_buffer, &title_press_start_tex,
                                             256.0f, 16.0f, 32.0f, 112.0f, alpha);
    }

    gfx_3ds_minimap_draw_music_title(vbo_buffer);
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
        alpha = (gTitleZoomCounter < 75) ? 255 :
                (gTitleFadeCounter < 0) ? 0 : (gTitleFadeCounter > 255 ? 255 : (u8) gTitleFadeCounter);
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
    gfx_3ds_minimap_load_hud_texture(&title_press_home_tex, title_press_home_t3x, title_press_home_t3x_size);
    gfx_3ds_minimap_load_transition_textures();

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

uint32_t gfx_3ds_draw_minimap(float *vertex_buffer, int vertex_offset)
{
    bool drew_minimap = false;
    bool preview_minimap;

    buffer_offset = vertex_offset;

    if (gN3dsIntroScreenMode >= 0) {
        if (gN3dsIntroScreenExiting && gfx_3ds_minimap_transition_holds_frame()) {
            gfx_3ds_minimap_draw_transition_screen(vertex_buffer);
            return buffer_offset - vertex_offset;
        }
        gfx_3ds_minimap_draw_intro_title(vertex_buffer);
        return buffer_offset - vertex_offset;
    }

    if (gN3dsCakeEndScreenActive) {
        sStarSelectPressStartTimer = 0;
        gfx_3ds_minimap_draw_cake_end_screen(vertex_buffer);
        return buffer_offset - vertex_offset;
    }
    gN3dsCakeEndScreenActive = FALSE;
    sCakeEndPressHomeTimer = 0;

    if (gfx_3ds_minimap_is_staff_roll()) {
        if (gfx_3ds_minimap_transition_holds_frame()) {
            gfx_3ds_minimap_draw_transition_screen(vertex_buffer);
        } else {
            gfx_3ds_minimap_draw_staff_roll(vertex_buffer);
            gfx_3ds_minimap_draw_bottom_transition(vertex_buffer);
        }
        gfx_3ds_minimap_draw_music_title(vertex_buffer);
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

    if (gfx_3ds_minimap_transition_holds_frame()) {
        gfx_3ds_minimap_draw_transition_screen(vertex_buffer);
        return buffer_offset - vertex_offset;
    }

    if (gfx_3ds_minimap_is_star_select()) {
        gfx_3ds_minimap_draw_star_select(vertex_buffer);
        gfx_3ds_minimap_draw_bottom_transition(vertex_buffer);
        return buffer_offset - vertex_offset;
    }
    sStarSelectPressStartTimer = 0;

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
        gfx_3ds_minimap_cache_current_frame();
        drew_minimap = true;
    }
    else if (show_minimap && minimap_tex_loaded)
    {
        gfx_3ds_minimap_draw_cached_frame(vertex_buffer);
        drew_minimap = true;
    }

    if (!drew_minimap)
        gfx_3ds_minimap_draw_background_color_rgb(vertex_buffer, 0x000000);

    if (!preview_minimap)
        gfx_3ds_minimap_draw_music_title(vertex_buffer);
    gfx_3ds_minimap_draw_bottom_transition(vertex_buffer);
    return buffer_offset - vertex_offset;
}

#endif
