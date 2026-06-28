#ifdef TARGET_N3DS

#include "gfx_3ds.h"
#include "gfx_3ds_menu.h"
#include "gfx_citro3d.h"
#include "pc/configfile.h"
#include "enhancements/death_ragdoll.h"

struct gfx_configuration gfx_config = {true, true}; // AA on, 800px on

struct gfx_3ds_menu_texture {
    C3D_Tex tex;
    bool loaded;
};

enum gfx_3ds_menu_page {
    GFX_3DS_MENU_PAGE_ROOT,
    GFX_3DS_MENU_PAGE_ENH,
};

static C3D_Mtx modelView, projection;
static int buffer_offset;
static enum gfx_3ds_menu_page sMenuPage = GFX_3DS_MENU_PAGE_ROOT;

static struct gfx_3ds_menu_texture mode_400_tex, mode_800_tex;
static struct gfx_3ds_menu_texture aa_off_tex, aa_on_tex;
static struct gfx_3ds_menu_texture debug_off_tex, debug_on_tex;
static struct gfx_3ds_menu_texture enh_tex;
static struct gfx_3ds_menu_texture ds_off_tex, ds_on_tex;
static struct gfx_3ds_menu_texture drd_off_tex, drd_on_tex;
static struct gfx_3ds_menu_texture hrd_off_tex, hrd_on_tex;
static struct gfx_3ds_menu_texture resume_tex, exit_tex;
static struct gfx_3ds_menu_texture menu_cleft_tex, menu_cright_tex, menu_cdown_tex, menu_cup_tex;

#define MENU_BUTTON_SIZE 64
#define MENU_TOP_Y 32
#define MENU_DRAW_TOP_Y 96
#define MENU_BOTTOM_Y 144
#define MENU_DRAW_BOTTOM_Y 208
#define MENU_LEFT_X 48
#define MENU_CENTER_X 128
#define MENU_RIGHT_X 208
#define MENU_SINGLE_CENTER_X 128

#define CONFIG_FILE "sm64config.txt"

static bool is_inside_box(int pos_x, int pos_y, int x, int y, int width, int height);

static void gfx_3ds_menu_reset_state(void)
{
    sMenuPage = GFX_3DS_MENU_PAGE_ROOT;
}

static bool gfx_3ds_menu_load_texture(struct gfx_3ds_menu_texture *texture,
                                      const void *data, size_t size)
{
    texture->loaded = load_t3x_texture(&texture->tex, NULL, data, size);
    if (texture->loaded) {
        C3D_TexSetFilter(&texture->tex, GPU_LINEAR, GPU_NEAREST);
    }
    return texture->loaded;
}

static void gfx_3ds_menu_apply_config_state(void)
{
    gfx_config.useAA = config3dsAntiAliasing != 0;
    gfx_config.useWide = config3dsWideMode != 0;
    if (!death_ragdoll_enabled) {
        hit_ragdoll_enabled = 0;
    }
}

static void gfx_3ds_menu_reload_config_state(void)
{
    configfile_load(CONFIG_FILE);
    gfx_3ds_menu_apply_config_state();
}

static bool gfx_3ds_menu_hit_ragdoll_is_enabled(void)
{
    return death_ragdoll_enabled != 0 && hit_ragdoll_enabled != 0;
}

static void gfx_3ds_menu_save_and_reload_config(void)
{
    if (!death_ragdoll_enabled) {
        hit_ragdoll_enabled = 0;
    }
    configfile_save(CONFIG_FILE);
    gfx_3ds_menu_reload_config_state();
}

static menu_action gfx_3ds_menu_close(void)
{
    gfx_3ds_menu_reset_state();
    return EXIT_MENU;
}

// Unused. We clear the screen elsewhere with a proper clear function.
/*static void gfx_3ds_menu_draw_background(float *vbo_buffer)
{
    Mtx_Identity(&modelView);
    Mtx_OrthoTilt(&projection, 0.0, 320.0, 0.0, 240.0, 0.0, 1.0, true);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, &modelView);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);

    memcpy(vbo_buffer + buffer_offset * VERTEX_SHADER_SIZE,
           vertex_list_color,
           sizeof(vertex_list_color));

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvColor(env, 0x66000000);
    C3D_TexEnvSrc(env, C3D_Both, GPU_CONSTANT, 0, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

    C3D_DrawArrays(GPU_TRIANGLES, buffer_offset, 6); // 2 triangles

    buffer_offset += 6;
}*/

static void gfx_3ds_menu_draw_button(float *vbo_buffer, int x, int y,
                                     struct gfx_3ds_menu_texture *texture, bool thin)
{
    if (!texture->loaded) {
        return;
    }

    Mtx_Identity(&modelView);
    Mtx_Translate(&modelView, x, 240 - y, 0.0f, false);

    Mtx_OrthoTilt(&projection, 0.0, 320.0, 0.0, 240.0, 0.0, 1.0, true);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, &modelView);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);

    const vertex *vertex_list = thin ? vertex_list_button_thin : vertex_list_button;
    size_t vertex_list_size = thin ? sizeof(vertex_list_button_thin) : sizeof(vertex_list_button);

    memcpy(vbo_buffer + buffer_offset * VERTEX_SHADER_SIZE,
           vertex_list,
           vertex_list_size);

    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
    C3D_StencilTest(false, GPU_ALWAYS, 0, 0xFF, 0xFF);
    C3D_StencilOp(GPU_STENCIL_KEEP, GPU_STENCIL_KEEP, GPU_STENCIL_KEEP);
    C3D_AlphaTest(true, GPU_GREATER, 0x00);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
                   GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
    C3D_TexBind(0, &texture->tex);
    C3D_TexFlush(&texture->tex);

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvColor(env, 0);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, 0, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

    C3D_DrawArrays(GPU_TRIANGLES, buffer_offset, 6); // 2 triangles
    buffer_offset += 6;
}

static bool gfx_3ds_menu_is_inside_button(int pos_x, int pos_y, int x, int y)
{
    return is_inside_box(pos_x, pos_y, x, y, MENU_BUTTON_SIZE, MENU_BUTTON_SIZE);
}

static void gfx_3ds_menu_draw_root_buttons(float *vertex_buffer)
{
    gfx_3ds_menu_draw_button(vertex_buffer, MENU_LEFT_X, MENU_DRAW_TOP_Y,
                             gfx_config.useAA ? &aa_on_tex : &aa_off_tex, false);
    gfx_3ds_menu_draw_button(vertex_buffer, MENU_CENTER_X, MENU_DRAW_TOP_Y,
                             gfx_config.useWide ? &mode_800_tex : &mode_400_tex, false);
    gfx_3ds_menu_draw_button(vertex_buffer, MENU_RIGHT_X, MENU_DRAW_TOP_Y,
                             death_ragdoll_debug_is_enabled() ? &debug_on_tex : &debug_off_tex, false);
    gfx_3ds_menu_draw_button(vertex_buffer, MENU_LEFT_X, MENU_DRAW_BOTTOM_Y, &enh_tex, false);
    gfx_3ds_menu_draw_button(vertex_buffer, MENU_CENTER_X, MENU_DRAW_BOTTOM_Y, &resume_tex, false);
    gfx_3ds_menu_draw_button(vertex_buffer, MENU_RIGHT_X, MENU_DRAW_BOTTOM_Y, &exit_tex, false);
}

static void gfx_3ds_menu_draw_enh_buttons(float *vertex_buffer)
{
    gfx_3ds_menu_draw_button(vertex_buffer, MENU_LEFT_X, MENU_DRAW_TOP_Y,
                             dynamic_shadows_enabled ? &ds_on_tex : &ds_off_tex, false);
    gfx_3ds_menu_draw_button(vertex_buffer, MENU_CENTER_X, MENU_DRAW_TOP_Y,
                             death_ragdoll_enabled ? &drd_on_tex : &drd_off_tex, false);
    gfx_3ds_menu_draw_button(vertex_buffer, MENU_RIGHT_X, MENU_DRAW_TOP_Y,
                             gfx_3ds_menu_hit_ragdoll_is_enabled() ? &hrd_on_tex : &hrd_off_tex, false);
    gfx_3ds_menu_draw_button(vertex_buffer, MENU_SINGLE_CENTER_X, MENU_DRAW_BOTTOM_Y, &resume_tex, false);
}

static bool is_inside_box(int pos_x, int pos_y, int x, int y, int width, int height)
{
    return pos_x >= x && pos_x <= (x+width) && pos_y >= y && pos_y <= (y+height);
}

menu_action gfx_3ds_menu_on_touch(int touch_x, int touch_y)
{
    if (!gShowConfigMenu) {
        gfx_3ds_menu_reset_state();
        gfx_3ds_menu_reload_config_state();
        return SHOW_MENU;
    }

    if (sMenuPage == GFX_3DS_MENU_PAGE_ENH) {
        if (gfx_3ds_menu_is_inside_button(touch_x, touch_y, MENU_LEFT_X, MENU_TOP_Y)) {
            dynamic_shadows_enabled = !dynamic_shadows_enabled;
            gfx_3ds_menu_save_and_reload_config();
            return MENU_CHANGED;
        }

        if (gfx_3ds_menu_is_inside_button(touch_x, touch_y, MENU_CENTER_X, MENU_TOP_Y)) {
            death_ragdoll_enabled = !death_ragdoll_enabled;
            if (!death_ragdoll_enabled) {
                hit_ragdoll_enabled = 0;
            }
            gfx_3ds_menu_save_and_reload_config();
            return MENU_CHANGED;
        }

        if (gfx_3ds_menu_is_inside_button(touch_x, touch_y, MENU_RIGHT_X, MENU_TOP_Y)) {
            if (!death_ragdoll_enabled) {
                if (hit_ragdoll_enabled != 0) {
                    hit_ragdoll_enabled = 0;
                    gfx_3ds_menu_save_and_reload_config();
                    return MENU_CHANGED;
                }
                return DO_NOTHING;
            }

            hit_ragdoll_enabled = !hit_ragdoll_enabled;
            gfx_3ds_menu_save_and_reload_config();
            return MENU_CHANGED;
        }

        if (gfx_3ds_menu_is_inside_button(touch_x, touch_y, MENU_SINGLE_CENTER_X, MENU_BOTTOM_Y)) {
            return gfx_3ds_menu_close();
        }

        return DO_NOTHING;
    }

    if (!gGfx3DEnabled && gfx_3ds_menu_is_inside_button(touch_x, touch_y, MENU_LEFT_X, MENU_TOP_Y)) {
        if (config3dsWideMode != 0) {
            config3dsAntiAliasing = (config3dsAntiAliasing == 0);
            gfx_3ds_menu_save_and_reload_config();
            return CONFIG_CHANGED;
        }
        return DO_NOTHING;
    }

    if (!gGfx3DEnabled && gfx_3ds_menu_is_inside_button(touch_x, touch_y, MENU_CENTER_X, MENU_TOP_Y)) {
        config3dsWideMode = (config3dsWideMode == 0);

        if (config3dsWideMode == 0 && config3dsAntiAliasing != 0)
            config3dsAntiAliasing = 0;
        gfx_3ds_menu_save_and_reload_config();

        return CONFIG_CHANGED;
    }

    if (gfx_3ds_menu_is_inside_button(touch_x, touch_y, MENU_RIGHT_X, MENU_TOP_Y)) {
        death_ragdoll_debug_set_enabled(!death_ragdoll_debug_is_enabled());
        return MENU_CHANGED;
    }

    if (gfx_3ds_menu_is_inside_button(touch_x, touch_y, MENU_LEFT_X, MENU_BOTTOM_Y)) {
        sMenuPage = GFX_3DS_MENU_PAGE_ENH;
        return MENU_CHANGED;
    }

    if (gfx_3ds_menu_is_inside_button(touch_x, touch_y, MENU_CENTER_X, MENU_BOTTOM_Y)) {
        return gfx_3ds_menu_close();
    }

    if (gfx_3ds_menu_is_inside_button(touch_x, touch_y, MENU_RIGHT_X, MENU_BOTTOM_Y)) {
        gfx_3ds_menu_reset_state();
        gShouldRun = false;
    }

    return DO_NOTHING;
}

void gfx_3ds_menu_init()
{
    gfx_3ds_menu_apply_config_state();
    death_ragdoll_debug_set_enabled(false);
    gfx_3ds_menu_reset_state();

    gfx_3ds_menu_load_texture(&mode_400_tex, mode_400_t3x, mode_400_t3x_size);
    gfx_3ds_menu_load_texture(&mode_800_tex, mode_800_t3x, mode_800_t3x_size);
    gfx_3ds_menu_load_texture(&aa_on_tex, aa_on_t3x, aa_on_t3x_size);
    gfx_3ds_menu_load_texture(&aa_off_tex, aa_off_t3x, aa_off_t3x_size);
    gfx_3ds_menu_load_texture(&debug_on_tex, debug_on_t3x, debug_on_t3x_size);
    gfx_3ds_menu_load_texture(&debug_off_tex, debug_off_t3x, debug_off_t3x_size);
    gfx_3ds_menu_load_texture(&enh_tex, enh_t3x, enh_t3x_size);
    gfx_3ds_menu_load_texture(&ds_on_tex, ds_on_t3x, ds_on_t3x_size);
    gfx_3ds_menu_load_texture(&ds_off_tex, ds_off_t3x, ds_off_t3x_size);
    gfx_3ds_menu_load_texture(&drd_on_tex, drd_on_t3x, drd_on_t3x_size);
    gfx_3ds_menu_load_texture(&drd_off_tex, drd_off_t3x, drd_off_t3x_size);
    gfx_3ds_menu_load_texture(&hrd_on_tex, hrd_on_t3x, hrd_on_t3x_size);
    gfx_3ds_menu_load_texture(&hrd_off_tex, hrd_off_t3x, hrd_off_t3x_size);
    gfx_3ds_menu_load_texture(&resume_tex, resume_t3x, resume_t3x_size);
    gfx_3ds_menu_load_texture(&exit_tex, exit_t3x, exit_t3x_size);
    gfx_3ds_menu_load_texture(&menu_cleft_tex, menu_cleft_t3x, menu_cleft_t3x_size);
    gfx_3ds_menu_load_texture(&menu_cright_tex, menu_cright_t3x, menu_cright_t3x_size);
    gfx_3ds_menu_load_texture(&menu_cdown_tex, menu_cdown_t3x, menu_cdown_t3x_size);
    gfx_3ds_menu_load_texture(&menu_cup_tex, menu_cup_t3x, menu_cup_t3x_size);

    gBottomScreenNeedsRender = true;
}

uint32_t gfx_3ds_menu_draw(float *vertex_buffer, int vertex_offset, bool configButtonsEnabled)
{
    gBottomScreenNeedsRender = false;

    C3D_FrameDrawOn(gTargetBottom);

    buffer_offset = vertex_offset;

    // WYATT_TODO this should proooobably be re-enabled once the bottom screen is reworked.
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_GREEN | GPU_WRITE_RED | GPU_WRITE_BLUE);

    if (configButtonsEnabled) {
        if (sMenuPage == GFX_3DS_MENU_PAGE_ENH) {
            gfx_3ds_menu_draw_enh_buttons(vertex_buffer);
        } else {
            gfx_3ds_menu_draw_root_buttons(vertex_buffer);
        }
    }

    return buffer_offset - vertex_offset;
}

#endif
