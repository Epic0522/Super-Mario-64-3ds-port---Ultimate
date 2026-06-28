#ifdef TARGET_N3DS

#include "macros.h"

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#include "gfx_3ds.h"
#include "gfx_3ds_minimap.h"
#include "gfx_3ds_menu.h"

#include "gfx_cc.h"
#include "gfx_rendering_api.h"

#include "gfx_citro3d.h"
#include "color_conversion.h"
#include "enhancements/dynamic_shadows.h"
#include "audio/external.h"
#include "game/area.h"
#include "game/level_update.h"
#include "seq_ids.h"

#define TEXTURE_POOL_SIZE 4096
#define FOG_LUT_SIZE 32
#define DYNAMIC_SHADOWMAP_SIZE 256
#define DYNAMIC_SHADOW_RECEIVER_STENCIL 0x01
#define DYNAMIC_SHADOW_CLEAR_DEPTH_STENCIL 0xFFFFFFFF
#define DYNAMIC_SHADOW_RECEIVER_VERTEX_SIZE 11

#define NTSC_FRAMERATE(fps) ((float) fps * (1000.0f / 1001.0f))

static Gfx3DSMode sCurrentGfx3DSMode = GFX_3DS_MODE_NORMAL;

static DVLB_s* sVShaderDvlb;
static shaderProgram_s sShaderProgram;
static DVLB_s* sShadowShaderDvlb;
static shaderProgram_s sShadowShaderProgram;
static int uLoc_shadowProjection, uLoc_shadowModelView;
static float* sVboBuffer;
static float* sBottomVboBuffer;
static float* sShadowReceiverVboBuffer;

extern const u8 shader_shbin[];
extern const u32 shader_shbin_size;
extern const u8 shadow_shbin[];
extern const u32 shadow_shbin_size;

struct ShaderProgram {
    uint32_t shader_id;
    uint32_t program_id;
    uint8_t num_floats;
    struct CCFeatures cc_features;
    bool swap_input;
    C3D_TexEnv texenv0;
    C3D_TexEnv texenv1;
};

static struct ShaderProgram sShaderProgramPool[32];
static uint8_t sShaderProgramPoolSize;

struct FogLut {
    uint32_t id;
    C3D_FogLut lut;
};

static struct FogLut fog_lut[FOG_LUT_SIZE];
static uint8_t fog_lut_size;
static uint8_t current_fog_idx;
static uint32_t fog_color;

static u32 sTexBuf[16 * 1024] __attribute__((aligned(32)));
static C3D_Tex sTexturePool[TEXTURE_POOL_SIZE];
static float sTexturePoolScaleS[TEXTURE_POOL_SIZE];
static float sTexturePoolScaleT[TEXTURE_POOL_SIZE];
static u32 sTextureIndex;
static int sTexUnits[2];

static int sCurTex = 0;
static int sCurShader = 0;

static int sVtxUnitSize = 0;
static int sBufIdx = 0;

static bool sDepthTestOn = false;
static bool sDepthUpdateOn = true;
static bool sDepthDecal = false;
static bool sUseBlend = false;
static bool sDynamicShadowStencil = false;
static bool sReceiverStencil = false;
static bool sReceiverStencilTest = false;
static bool sDynamicShadowReceiver = false;
static uint8_t sDynamicShadowMaskMode = 0;
static uint8_t sDynamicShadowStencilRef = DYNAMIC_SHADOW_RECEIVER_STENCIL;
static bool sDynamicShadowDepthPass = false;
static bool sDynamicShadowReceiverPass = false;
static bool sDynamicShadowMapReady = false;
static C3D_Tex sDynamicShadowMapTex;
static C3D_RenderTarget *sDynamicShadowMapTarget;

// calling FrameDrawOn resets viewport
static int viewport_x, viewport_y;
static int viewport_width, viewport_height;
// calling SetViewport resets scissor!
static int scissor_x, scissor_y;
static int scissor_width, scissor_height;
static bool scissor;

static C3D_Mtx modelView, projection;

static bool sIsHud;

static int sOrigBufIdx;
static int s2DMode;
float iodZ = 8.0f;
float iodW = 16.0f;

static void applyBlend(void);
static void gfx_citro3d_configure_normal_vertex_layout(void);
static void gfx_citro3d_configure_shadow_receiver_vertex_layout(void);
static void gfx_citro3d_apply_screen_modelview(void);
static void gfx_citro3d_restore_normal_pipeline(void);

// Data storage type for the screen clear buf configs
union ScreenClearBufConfig3ds {
    struct {
        enum ViewportClearBuffer top;
        enum ViewportClearBuffer bottom;
    } struc;
    enum ViewportClearBuffer array[3];
};

// Determines the clear mode for the viewports.
static union ScreenClearBufConfig3ds screen_clear_bufs = {{
    VIEW_CLEAR_BUFFER_NONE,      // top
    VIEW_CLEAR_BUFFER_NONE       // bottom
}};

// Determines the clear colors for the viewports
static union {
    struct {
        u32 top;
        u32 bottom;
    } struc;
    u32 array[3];
} screen_clear_colors = {{
    COLOR_RGBA_PARAMS_TO_RGBA32(0, 0, 0, 255),    // top: 0x000000FF
    COLOR_RGBA_PARAMS_TO_RGBA32(0, 0, 0, 255),    // bottom: 0x000000FF
}};

// Handles 3DS screen clearing
static void clear_buffers()
{
    enum ViewportClearBuffer clear_top = screen_clear_bufs.struc.top;
    enum ViewportClearBuffer clear_bottom = screen_clear_bufs.struc.bottom;

    // Clear top screen
    if (clear_top)
        C3D_RenderTargetClear(gTarget, (C3D_ClearBits) clear_top, screen_clear_colors.struc.top,
                              DYNAMIC_SHADOW_CLEAR_DEPTH_STENCIL);
        
    // Clear right-eye view
    // We check gGfx3DSMode because clearing in 800px modes causes a crash.
    if (clear_top && (gGfx3DSMode == GFX_3DS_MODE_NORMAL || gGfx3DSMode == GFX_3DS_MODE_AA_22))
        C3D_RenderTargetClear(gTargetRight, (C3D_ClearBits) clear_top, screen_clear_colors.struc.top,
                              DYNAMIC_SHADOW_CLEAR_DEPTH_STENCIL);

    // Clear bottom screen only if it needs re-rendering.
    if (clear_bottom)
        C3D_RenderTargetClear(gTargetBottom, (C3D_ClearBits) clear_bottom, screen_clear_colors.struc.bottom,
                              DYNAMIC_SHADOW_CLEAR_DEPTH_STENCIL);
}

static bool gfx_citro3d_should_clear_staff_roll_top(void)
{
    u16 seqArgs = get_current_background_music();
    u8 seqId = seqArgs & 0x7F;

    return gCurrCreditsEntry != NULL
        || (seqArgs != 0xFFFF && seqId == SEQ_EVENT_CUTSCENE_CREDITS);
}

void stereoTilt(C3D_Mtx* mtx, float z, float w)
{
    /** ********************** Default L/R stereo perspective function with x/y tilt removed **********************

        Preserving this to show what the proper function *should* look like.
        TODO: move to gfx_pc before RDP's mv*p happens, for proper and portable stereoscopic support

    float fovy_tan = tanf(fovy * 0.5f * M_PI / 180.0f); // equals 1.0 when FOV is 90
    float fovy_tan_aspect = fovy_tan * aspect; // equals 1.0 because we are being passed an existing mv*p matrix
    float shift = iod / (2.0f*screen);

    Mtx_Zeros(mtx); // most values revert to identity matrix anyway, including several that are necessary

    mtx->r[0].x = 1.0f / fovy_tan_aspect; // equals 1.0
    mtx->r[1].y = 1.0f / fovy_tan; // equals 1.0
    mtx->r[1].z = -mtx->r[3].z * shift / fovx_tan_invaspect; // equivalent in value to r[1].w at focallen = 1.0
    mtx->r[1].w = iod / 2.0f; // equivalent in value to r[1].z at focallen = 1.0
    mtx->r[2].z = -mtx->r[3].z * near / (near - far); // kills zbuffer
    mtx->r[2].w = near * far / (near - far); // kills clipping plane
    mtx->r[3].z = isLeftHanded ? 1.0f : -1.0f; // kills fog (viewplane data?)
    ************************************************************************************************************ */

    Mtx_Identity(mtx);

    switch (s2DMode) {
        case 0 : // 3D
            break;
        case 1 : // pure 2D
            C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, mtx);
            return;
        case 2 : // goddard hand and press start text
            z = (z < 0) ? -32.0f : 32.0f;
            w = (w < 0) ? -32.0f : 32.0f;
            break;
        case 3 : // credits
            z = (z < 0) ? -64.0f : 64.0f;
            w = (w < 0) ? -64.0f : 64.0f;
            break;
        case 4 : // the goddamn score menu
            return;
    }

    mtx->r[1].z = (z == 0) ? 0 : gSliderLevel / z; // view frustum separation? (+ = deep)
    mtx->r[1].w = (w == 0) ? 0 : gSliderLevel / w; // camera-to-viewport separation? (+ = pop)
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, mtx);
}

static void gfx_citro3d_set_2d(int mode_2d)
{
    s2DMode = mode_2d;
}

void gfx_citro3d_set_iod(float z, float w)
{
    iodZ = z;
    iodW = w;
}

static void gfx_citro3d_is_hud(bool is_hud)
{
    sIsHud = is_hud;
}

static bool gfx_citro3d_z_is_from_0_to_1(void)
{
    return true;
}

static void gfx_citro3d_vertex_array_set_attribs(struct ShaderProgram *prg)
{
    sVtxUnitSize = prg->num_floats;
}

static void gfx_citro3d_configure_normal_vertex_layout(void)
{
    C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
    AttrInfo_Init(attrInfo);
    AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 4); // v0=position
    AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2); // v1=texcoord
    AttrInfo_AddLoader(attrInfo, 2, GPU_FLOAT, 4); // v2=color

    C3D_BufInfo* bufInfo = C3D_GetBufInfo();
    BufInfo_Init(bufInfo);
    BufInfo_Add(bufInfo, sVboBuffer, VERTEX_SHADER_SIZE * 4, 3, 0x210);
}

static void gfx_citro3d_configure_bottom_vertex_layout(void)
{
    C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
    AttrInfo_Init(attrInfo);
    AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 4); // v0=position
    AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2); // v1=texcoord
    AttrInfo_AddLoader(attrInfo, 2, GPU_FLOAT, 4); // v2=color

    C3D_BufInfo* bufInfo = C3D_GetBufInfo();
    BufInfo_Init(bufInfo);
    BufInfo_Add(bufInfo, sBottomVboBuffer, VERTEX_SHADER_SIZE * 4, 3, 0x210);
}

static void gfx_citro3d_configure_shadow_receiver_vertex_layout(void)
{
    C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
    AttrInfo_Init(attrInfo);
    AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 4); // v0=position
    AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 3); // v1=shadow uv + compare depth
    AttrInfo_AddLoader(attrInfo, 2, GPU_FLOAT, 4); // v2=color

    C3D_BufInfo* bufInfo = C3D_GetBufInfo();
    BufInfo_Init(bufInfo);
    BufInfo_Add(bufInfo, sShadowReceiverVboBuffer, DYNAMIC_SHADOW_RECEIVER_VERTEX_SIZE * 4, 3, 0x210);
}

static void gfx_citro3d_unload_shader(UNUSED struct ShaderProgram *old_prg)
{
}

static GPU_TEVSRC getTevSrc(int input, bool swapInput01)
{
    switch (input)
    {
        case SHADER_0:
            return GPU_CONSTANT;
        case SHADER_INPUT_1:
            return swapInput01 ? GPU_PREVIOUS : GPU_PRIMARY_COLOR;
        case SHADER_INPUT_2:
            return swapInput01 ? GPU_PRIMARY_COLOR : GPU_PREVIOUS;
        case SHADER_INPUT_3:
            return GPU_CONSTANT;
        case SHADER_INPUT_4:
            return GPU_CONSTANT;
        case SHADER_TEXEL0:
        case SHADER_TEXEL0A:
            return GPU_TEXTURE0;
        case SHADER_TEXEL1:
            return GPU_TEXTURE1;
    }
    return GPU_CONSTANT;
}

static void update_tex_env(struct ShaderProgram *prg, bool swap_input)
{
    if (prg->cc_features.num_inputs == 2)
    {
        C3D_TexEnvInit(&prg->texenv1);
        C3D_TexEnvColor(&prg->texenv1, 0);
        C3D_TexEnvFunc(&prg->texenv1, C3D_Both, GPU_REPLACE);
        C3D_TexEnvSrc(&prg->texenv1, C3D_Both, GPU_CONSTANT, 0, 0);
        C3D_TexEnvOpRgb(&prg->texenv1, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
        C3D_TexEnvOpAlpha(&prg->texenv1, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA);
    }

    C3D_TexEnvInit(&prg->texenv0);
    C3D_TexEnvColor(&prg->texenv0, 0);
    if (prg->cc_features.opt_alpha && !prg->cc_features.color_alpha_same)
    {
        // RGB first
        if (prg->cc_features.do_single[0])
        {
            C3D_TexEnvFunc(&prg->texenv0, C3D_RGB, GPU_REPLACE);
            C3D_TexEnvSrc(&prg->texenv0, C3D_RGB, getTevSrc(prg->cc_features.c[0][3], swap_input), 0, 0);
            if (prg->cc_features.c[0][3] == SHADER_TEXEL0A)
                C3D_TexEnvOpRgb(&prg->texenv0, GPU_TEVOP_RGB_SRC_ALPHA, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
            else
                C3D_TexEnvOpRgb(&prg->texenv0, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
        }
        else if (prg->cc_features.do_multiply[0])
        {
            C3D_TexEnvFunc(&prg->texenv0, C3D_RGB, GPU_MODULATE);
            C3D_TexEnvSrc(&prg->texenv0, C3D_RGB, getTevSrc(prg->cc_features.c[0][0], swap_input),
                                        getTevSrc(prg->cc_features.c[0][2], swap_input), 0);
            C3D_TexEnvOpRgb(&prg->texenv0,
                prg->cc_features.c[0][0] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
                prg->cc_features.c[0][2] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
                GPU_TEVOP_RGB_SRC_COLOR);
        }
        else if (prg->cc_features.do_mix[0])
        {
            C3D_TexEnvFunc(&prg->texenv0, C3D_RGB, GPU_INTERPOLATE);
            C3D_TexEnvSrc(&prg->texenv0, C3D_RGB, getTevSrc(prg->cc_features.c[0][0], swap_input),
                                        getTevSrc(prg->cc_features.c[0][1], swap_input),
                                        getTevSrc(prg->cc_features.c[0][2], swap_input));
            C3D_TexEnvOpRgb(&prg->texenv0,
                prg->cc_features.c[0][0] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
                prg->cc_features.c[0][1] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
                prg->cc_features.c[0][2] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR);
        }
        // now Alpha
        C3D_TexEnvOpAlpha(&prg->texenv0,  GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA);
        if (prg->cc_features.do_single[1])
        {
            C3D_TexEnvFunc(&prg->texenv0, C3D_Alpha, GPU_REPLACE);
            C3D_TexEnvSrc(&prg->texenv0, C3D_Alpha, getTevSrc(prg->cc_features.c[1][3], swap_input), 0, 0);
        }
        else if (prg->cc_features.do_multiply[1])
        {
            C3D_TexEnvFunc(&prg->texenv0, C3D_Alpha, GPU_MODULATE);
            C3D_TexEnvSrc(&prg->texenv0, C3D_Alpha, getTevSrc(prg->cc_features.c[1][0], swap_input),
                                          getTevSrc(prg->cc_features.c[1][2], swap_input), 0);
        }
        else if (prg->cc_features.do_mix[1])
        {
            C3D_TexEnvFunc(&prg->texenv0, C3D_Alpha, GPU_INTERPOLATE);
            C3D_TexEnvSrc(&prg->texenv0, C3D_Alpha, getTevSrc(prg->cc_features.c[1][0], swap_input),
                                          getTevSrc(prg->cc_features.c[1][1], swap_input),
                                          getTevSrc(prg->cc_features.c[1][2], swap_input));
        }
    }
    else
    {
        // RBGA
        C3D_TexEnvOpAlpha(&prg->texenv0, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA);
        if (prg->cc_features.do_single[0])
        {
            C3D_TexEnvFunc(&prg->texenv0, C3D_Both, GPU_REPLACE);
            C3D_TexEnvSrc(&prg->texenv0, C3D_Both, getTevSrc(prg->cc_features.c[0][3], swap_input), 0, 0);
            if (prg->cc_features.c[0][3] == SHADER_TEXEL0A)
                C3D_TexEnvOpRgb(&prg->texenv0, GPU_TEVOP_RGB_SRC_ALPHA, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
            else
                C3D_TexEnvOpRgb(&prg->texenv0, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
        }
        else if (prg->cc_features.do_multiply[0])
        {
            C3D_TexEnvFunc(&prg->texenv0, C3D_Both, GPU_MODULATE);
            C3D_TexEnvSrc(&prg->texenv0, C3D_Both, getTevSrc(prg->cc_features.c[0][0], swap_input),
                                         getTevSrc(prg->cc_features.c[0][2], swap_input), 0);
            C3D_TexEnvOpRgb(&prg->texenv0,
                prg->cc_features.c[0][0] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
                prg->cc_features.c[0][2] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
                GPU_TEVOP_RGB_SRC_COLOR);
        }
        else if (prg->cc_features.do_mix[0])
        {
            C3D_TexEnvFunc(&prg->texenv0, C3D_Both, GPU_INTERPOLATE);
            C3D_TexEnvSrc(&prg->texenv0, C3D_Both, getTevSrc(prg->cc_features.c[0][0], swap_input),
                                         getTevSrc(prg->cc_features.c[0][1], swap_input),
                                         getTevSrc(prg->cc_features.c[0][2], swap_input));
            C3D_TexEnvOpRgb(&prg->texenv0,
                prg->cc_features.c[0][0] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
                prg->cc_features.c[0][1] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
                prg->cc_features.c[0][2] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR);
        }
    }
    if (!prg->cc_features.opt_alpha)
    {
        C3D_TexEnvColor(&prg->texenv0, 0xFF000000);
        C3D_TexEnvFunc(&prg->texenv0, C3D_Alpha, GPU_REPLACE);
        C3D_TexEnvSrc(&prg->texenv0, C3D_Alpha, GPU_CONSTANT, 0, 0);
    }

    prg->swap_input = swap_input;
}

static void update_shader(bool swap_input)
{
    struct ShaderProgram *prg = &sShaderProgramPool[sCurShader];

    // only Goddard
    if (prg->swap_input != swap_input)
    {
        update_tex_env(prg, swap_input);
    }

    if (prg->cc_features.num_inputs == 2)
    {
        C3D_SetTexEnv(0, &prg->texenv1);
        C3D_SetTexEnv(1, &prg->texenv0);
    } else {
        C3D_SetTexEnv(0, &prg->texenv0);
        C3D_TexEnvInit(C3D_GetTexEnv(1));
    }

    if (prg->cc_features.opt_fog)
    {
        C3D_FogGasMode(GPU_FOG, GPU_PLAIN_DENSITY, true);
        C3D_FogColor(fog_color);
        C3D_FogLutBind(&fog_lut[current_fog_idx].lut);
    } else {
        C3D_FogGasMode(GPU_NO_FOG, GPU_PLAIN_DENSITY, false);
    }

    if (prg->cc_features.opt_texture_edge && prg->cc_features.opt_alpha)
        C3D_AlphaTest(true, GPU_GREATER, 77);
    else
        C3D_AlphaTest(true, GPU_GREATER, 0);
}

static void gfx_citro3d_load_shader(struct ShaderProgram *new_prg)
{
    sCurShader = new_prg->program_id;
    gfx_citro3d_vertex_array_set_attribs(new_prg);

    update_shader(false);
}

static struct ShaderProgram *gfx_citro3d_create_and_load_new_shader(uint32_t shader_id)
{
    int id = sShaderProgramPoolSize;
    struct ShaderProgram *prg = &sShaderProgramPool[sShaderProgramPoolSize++];

    prg->program_id = id;

    prg->shader_id = shader_id;
    gfx_cc_get_features(shader_id, &prg->cc_features);

    update_tex_env(prg, false);

    prg->num_floats = 4; // vertex position (xyzw)

    if (prg->cc_features.used_textures[0] || prg->cc_features.used_textures[1])
    {
        prg->num_floats += 2;
    }
    prg->num_floats += prg->cc_features.num_inputs * (prg->cc_features.opt_alpha ? 4 : 3);

    gfx_citro3d_load_shader(prg);

    return prg;
}

static struct ShaderProgram *gfx_citro3d_lookup_shader(uint32_t shader_id)
{
    for (size_t i = 0; i < sShaderProgramPoolSize; i++)
    {
        if (sShaderProgramPool[i].shader_id == shader_id)
        {
            return &sShaderProgramPool[i];
        }
    }
    return NULL;
}

static void gfx_citro3d_shader_get_info(struct ShaderProgram *prg, uint8_t *num_inputs, bool used_textures[2])
{
    *num_inputs = prg->cc_features.num_inputs;
    used_textures[0] = prg->cc_features.used_textures[0];
    used_textures[1] = prg->cc_features.used_textures[1];
}

static uint32_t gfx_citro3d_new_texture(void)
{
    if (sTextureIndex == TEXTURE_POOL_SIZE)
    {
        printf("Out of textures!\n");
        return 0;
    }
    return sTextureIndex++;
}

static void gfx_citro3d_select_texture(int tile, uint32_t texture_id)
{
    C3D_TexBind(tile, &sTexturePool[texture_id]);
    sCurTex = texture_id;
    sTexUnits[tile] = texture_id;
}

static int sTileOrder[] =
{
    0,  1,   4,  5,
    2,  3,   6,  7,

    8,  9,  12, 13,
    10, 11, 14, 15
};

static void performTexSwizzle(const u8* src, u32* dst, u32 w, u32 h)
{
    int offs = 0;
    for (u32 y = 0; y < h; y += 8)
    {
        for (u32 x = 0; x < w; x += 8)
        {
            for (int i = 0; i < 64; i++)
            {
                int x2 = i & 7;
                int y2 = i >> 3;
                int pos = sTileOrder[(x2 & 3) + ((y2 & 3) << 2)] + ((x2 >> 2) << 4) + ((y2 >> 2) << 5);
                u32 c = ((const u32*)src)[(y + y2) * w + x + x2];
                dst[offs + pos] = ((c & 0xFF) << 24) | (((c >> 8) & 0xFF) << 16) | (((c >> 16) & 0xFF) << 8) | (c >> 24);
            }
            dst += 64;
        }
    }
}

static void gfx_citro3d_upload_texture(const uint8_t *rgba32_buf, int width, int height)
{
    if (width < 8 || height < 8 || (width & (width - 1)) || (height & (height - 1)))
    {
        u32 newWidth = width < 8 ? 8 : (1 << (32 - __builtin_clz(width - 1)));
        u32 newHeight = height < 8 ? 8 : (1 << (32 - __builtin_clz(height - 1)));
        if (newWidth * newHeight * 4 > sizeof(sTexBuf))
        {
            printf("Tex buffer overflow!\n");
            return;
        }
        int offs = 0;
        for (u32 y = 0; y < newHeight; y += 8)
        {
            for (u32 x = 0; x < newWidth; x += 8)
            {
                for (int i = 0; i < 64; i++)
                {
                    int x2 = i % 8;
                    int y2 = i / 8;

                    int realX = x + x2;
                    if (realX >= width)
                        realX -= width;

                    int realY = y + y2;
                    if (realY >= height)
                        realY -= height;

                    int pos = sTileOrder[x2 % 4 + y2 % 4 * 4] + 16 * (x2 / 4) + 32 * (y2 / 4);
                    u32 c = ((u32*)rgba32_buf)[realY * width + realX];
                    ((u32*)sTexBuf)[offs + pos] = ((c & 0xFF) << 24) | (((c >> 8) & 0xFF) << 16) | (((c >> 16) & 0xFF) << 8) | (c >> 24);
                }
                offs += 64;
            }
        }
        sTexturePoolScaleS[sCurTex] = width / (float)newWidth;
        sTexturePoolScaleT[sCurTex] = height / (float)newHeight;
        width = newWidth;
        height = newHeight;
    }
    else
    {
        sTexturePoolScaleS[sCurTex] = 1.f;
        sTexturePoolScaleT[sCurTex] = 1.f;
        performTexSwizzle(rgba32_buf, sTexBuf, width, height);
    }
    C3D_TexInit(&sTexturePool[sCurTex], width, height, GPU_RGBA8);
    C3D_TexUpload(&sTexturePool[sCurTex], sTexBuf);
    C3D_TexFlush(&sTexturePool[sCurTex]);
}

static uint32_t gfx_cm_to_opengl(uint32_t val)
{
    if (val & G_TX_CLAMP)
        return GPU_CLAMP_TO_EDGE;
    return (val & G_TX_MIRROR) ? GPU_MIRRORED_REPEAT : GPU_REPEAT;
}

static void gfx_citro3d_set_sampler_parameters(int tile, bool linear_filter, uint32_t cms, uint32_t cmt)
{
    C3D_TexSetFilter(&sTexturePool[sTexUnits[tile]], linear_filter ? GPU_LINEAR : GPU_NEAREST, linear_filter ? GPU_LINEAR : GPU_NEAREST);
    C3D_TexSetWrap(&sTexturePool[sTexUnits[tile]], gfx_cm_to_opengl(cms), gfx_cm_to_opengl(cmt));
}

static void update_depth()
{
    if (sDynamicShadowMaskMode != 0) {
        /*
         * Receiver collision triangles are coplanar with the visual floor.
         * Comparing them against the already-drawn depth buffer makes their
         * stencil coverage alternate with tiny rasterization differences.
         * Connectivity and height filtering already reject unrelated floors;
         * the projected shadow itself still uses the normal depth test.
         */
        C3D_DepthTest(false, GPU_ALWAYS, (GPU_WRITEMASK) 0);
        C3D_DepthMap(true, -1.0f, 0);
        return;
    }

    C3D_DepthTest(sDepthTestOn, GPU_LEQUAL, sDepthUpdateOn ? GPU_WRITE_ALL : GPU_WRITE_COLOR);
    C3D_DepthMap(true, -1.0f, sDepthDecal ? -0.001f : 0);
}

static void gfx_citro3d_init_dynamic_shadowmap(void)
{
    if (sDynamicShadowMapReady)
        return;

    if (!C3D_TexInitShadow(&sDynamicShadowMapTex, DYNAMIC_SHADOWMAP_SIZE, DYNAMIC_SHADOWMAP_SIZE)) {
        printf("Dynamic shadowmap texture init failed\n");
        return;
    }

    C3D_TexSetFilter(&sDynamicShadowMapTex, GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(&sDynamicShadowMapTex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    sDynamicShadowMapTex.param |= GPU_TEXTURE_SHADOW_PARAM;
    C3D_TexShadowParams(false, 0.0125f);

    sDynamicShadowMapTarget =
        C3D_RenderTargetCreateFromTex(&sDynamicShadowMapTex, GPU_TEXFACE_2D, 0, GPU_RB_DEPTH24);
    if (sDynamicShadowMapTarget == NULL) {
        C3D_TexDelete(&sDynamicShadowMapTex);
        printf("Dynamic shadowmap render target init failed\n");
        return;
    }

    sDynamicShadowMapReady = true;
}

bool gfx_citro3d_dynamic_shadowmap_available(void)
{
    return sDynamicShadowMapReady && sDynamicShadowMapTarget != NULL;
}

static void gfx_citro3d_apply_dynamic_shadow_stencil(void)
{
    if (sDynamicShadowMaskMode != 0) {
        C3D_StencilTest(true, GPU_ALWAYS, sDynamicShadowMaskMode, 0xFF, 0xFF);
        C3D_StencilOp(GPU_STENCIL_KEEP, GPU_STENCIL_KEEP, GPU_STENCIL_REPLACE);
    } else if (sDynamicShadowStencil) {
        C3D_StencilTest(true, GPU_EQUAL, sDynamicShadowStencilRef, 0xFF, 0xFF);
        // Consume an accepted receiver pixel once. This prevents overlapping
        // model triangles from darkening it repeatedly.
        C3D_StencilOp(GPU_STENCIL_KEEP, GPU_STENCIL_KEEP, GPU_STENCIL_ZERO);
    } else if (sReceiverStencilTest) {
        C3D_StencilTest(true, GPU_EQUAL, DYNAMIC_SHADOW_RECEIVER_STENCIL, 0xFF, 0xFF);
        C3D_StencilOp(GPU_STENCIL_KEEP, GPU_STENCIL_KEEP, GPU_STENCIL_KEEP);
    } else if (sReceiverStencil) {
        C3D_StencilTest(true, GPU_ALWAYS, DYNAMIC_SHADOW_RECEIVER_STENCIL, 0xFF, 0xFF);
        C3D_StencilOp(GPU_STENCIL_KEEP, GPU_STENCIL_KEEP, GPU_STENCIL_REPLACE);
    } else {
        C3D_StencilTest(false, GPU_ALWAYS, 0, 0xFF, 0xFF);
        C3D_StencilOp(GPU_STENCIL_KEEP, GPU_STENCIL_KEEP, GPU_STENCIL_KEEP);
    }
}

void gfx_citro3d_set_dynamic_shadow_stencil(bool enabled)
{
    if (sDynamicShadowStencil == enabled)
        return;

    sDynamicShadowStencil = enabled;
    gfx_citro3d_apply_dynamic_shadow_stencil();
}

static void gfx_citro3d_set_dynamic_shadow_receiver(bool enabled)
{
    sDynamicShadowReceiver = enabled;
}

static void gfx_citro3d_set_dynamic_shadow_mask(uint8_t mode)
{
    if (sDynamicShadowMaskMode == mode)
        return;

    sDynamicShadowMaskMode = mode;
    if (mode != 0) {
        sDynamicShadowStencilRef = mode;
        C3D_AlphaTest(false, GPU_ALWAYS, 0x00);
        C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ZERO, GPU_ONE,
                       GPU_ZERO, GPU_ONE);
        C3D_DepthTest(false, GPU_ALWAYS, (GPU_WRITEMASK) 0);
    } else {
        C3D_AlphaTest(true, GPU_GREATER, 0x00);
        update_depth();
        applyBlend();
    }
    gfx_citro3d_apply_dynamic_shadow_stencil();
}

static void gfx_citro3d_restore_normal_pipeline(void)
{
    sDynamicShadowDepthPass = false;
    sDynamicShadowReceiverPass = false;
    sDynamicShadowStencil = false;
    sReceiverStencil = false;
    sReceiverStencilTest = false;
    sDynamicShadowReceiver = false;
    sDynamicShadowMaskMode = 0;
    sDynamicShadowStencilRef = DYNAMIC_SHADOW_RECEIVER_STENCIL;

    C3D_FragOpMode(GPU_FRAGOPMODE_GL);
    C3D_BindProgram(&sShaderProgram);
    gfx_citro3d_configure_normal_vertex_layout();
    C3D_AlphaTest(true, GPU_GREATER, 0x00);
    update_depth();
    applyBlend();
    gfx_citro3d_apply_dynamic_shadow_stencil();
    update_shader(false);
}

static void gfx_citro3d_begin_bottom_screen_pass(void)
{
    gfx_citro3d_restore_normal_pipeline();
    gfx_citro3d_configure_bottom_vertex_layout();

    /*
     * The bottom screen is redrawn every frame.  It must not inherit depth,
     * stencil, scissor, or viewport state from the previous top-screen pass:
     * title/credits 2D modes otherwise leave old fragments behind when the
     * physical 3D slider is at zero.
     */
    C3D_RenderTargetClear(gTargetBottom, C3D_CLEAR_ALL, 0x000000FF,
                          DYNAMIC_SHADOW_CLEAR_DEPTH_STENCIL);
    C3D_FrameDrawOn(gTargetBottom);
    C3D_SetViewport(0, 0, 240, 320);
    C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
    C3D_StencilTest(false, GPU_ALWAYS, 0, 0xFF, 0xFF);
    C3D_StencilOp(GPU_STENCIL_KEEP, GPU_STENCIL_KEEP, GPU_STENCIL_KEEP);
    C3D_AlphaTest(true, GPU_GREATER, 0x00);
    C3D_FragOpMode(GPU_FRAGOPMODE_GL);
    applyBlend();
}

static void gfx_citro3d_set_receiver_stencil(bool enabled)
{
    if (sReceiverStencil == enabled)
        return;

    sReceiverStencil = enabled;
    gfx_citro3d_apply_dynamic_shadow_stencil();
}

static void gfx_citro3d_set_receiver_stencil_test(bool enabled)
{
    if (sReceiverStencilTest == enabled)
        return;

    sReceiverStencilTest = enabled;
    gfx_citro3d_apply_dynamic_shadow_stencil();
}

static void gfx_citro3d_set_dynamic_shadow_pass(uint32_t pass)
{
    C3D_Mtx identity;

    sDynamicShadowDepthPass = false;
    sDynamicShadowReceiverPass = false;
    if (pass != DYNAMIC_SHADOW_GFX_PASS_RECEIVER) {
        sReceiverStencilTest = false;
    }
    C3D_FragOpMode(GPU_FRAGOPMODE_GL);
    C3D_AlphaTest(true, GPU_GREATER, 0x00);
    applyBlend();
    update_depth();
    gfx_citro3d_apply_dynamic_shadow_stencil();
    gfx_citro3d_apply_screen_modelview();

    if ((pass & 0xFFFFFF00u) != DYNAMIC_SHADOW_GFX_PASS_MAGIC) {
        return;
    }

    if (pass == DYNAMIC_SHADOW_GFX_PASS_DEPTH && gfx_citro3d_dynamic_shadowmap_available()) {
        sDynamicShadowDepthPass = true;
        sDynamicShadowStencil = false;
        sReceiverStencil = false;
        sReceiverStencilTest = false;
        gfx_citro3d_apply_dynamic_shadow_stencil();
        C3D_AlphaTest(false, GPU_ALWAYS, 0x00);
        C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
        C3D_FragOpMode(GPU_FRAGOPMODE_SHADOW);
        C3D_FragOpShadow(1.0f, 0.0f);
        C3D_DepthTest(true, GPU_LEQUAL, GPU_WRITE_ALL);
        C3D_FrameDrawOn(sDynamicShadowMapTarget);
        C3D_RenderTargetClear(sDynamicShadowMapTarget,
                              C3D_CLEAR_COLOR | C3D_CLEAR_DEPTH,
                              0xFFFFFFFF,
                              DYNAMIC_SHADOW_CLEAR_DEPTH_STENCIL);
        C3D_SetViewport(0, 0, DYNAMIC_SHADOWMAP_SIZE, DYNAMIC_SHADOWMAP_SIZE);
        Mtx_Identity(&identity);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, &identity);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &identity);
    } else if (pass == DYNAMIC_SHADOW_GFX_PASS_RECEIVER && gfx_citro3d_dynamic_shadowmap_available()) {
        sDynamicShadowReceiverPass = true;
        sReceiverStencil = false;
        sReceiverStencilTest = true;
        gfx_citro3d_apply_dynamic_shadow_stencil();
        C3D_FragOpMode(GPU_FRAGOPMODE_GL);
        C3D_FrameDrawOn(gTarget);
        C3D_SetViewport(viewport_y, viewport_x, viewport_height, viewport_width);
        C3D_DepthTest(false, GPU_LEQUAL, GPU_WRITE_COLOR);
        C3D_AlphaTest(true, GPU_GREATER, 0x00);
        applyBlend();
    }
}

static void gfx_citro3d_apply_screen_modelview(void)
{
    Mtx_Identity(&modelView);
    Mtx_RotateZ(&modelView, 0.75f*M_TAU, false);
    Mtx_Identity(&projection);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, &modelView);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);
}

static void gfx_citro3d_set_depth_test(bool depth_test)
{
    sDepthTestOn = depth_test;
    update_depth();
}

static void gfx_citro3d_set_depth_mask(bool z_upd)
{
    sDepthUpdateOn = z_upd;
    update_depth();
}

static void gfx_citro3d_set_zmode_decal(bool zmode_decal)
{
    sDepthDecal = zmode_decal;
    update_depth();
}

static void gfx_citro3d_set_viewport(int x, int y, int width, int height)
{
    if (gGfx3DSMode == GFX_3DS_MODE_AA_22 || gGfx3DSMode == GFX_3DS_MODE_WIDE_AA_12)
    {
        viewport_x = x * 2;
        viewport_y = y * 2;
        viewport_width = width * 2;
        viewport_height = height * 2;
    }
    else if (gGfx3DSMode == GFX_3DS_MODE_WIDE)
    {
        viewport_x = x * 2;
        viewport_y = y;
        viewport_width = width * 2;
        viewport_height = height;
    }
    else // gGfx3DSMode == GFX_3DS_MODE_NORMAL
    {
        viewport_x = x;
        viewport_y = y;
        viewport_width = width;
        viewport_height = height;
    }
}

static void gfx_citro3d_set_scissor(int x, int y, int width, int height)
{
    scissor = true;
    if (gGfx3DSMode == GFX_3DS_MODE_AA_22 || gGfx3DSMode == GFX_3DS_MODE_WIDE_AA_12)
    {
        scissor_x = x * 2;
        scissor_y = y * 2;
        scissor_width = (x + width) * 2;
        scissor_height = (y + height) * 2;
    }
    else if (gGfx3DSMode == GFX_3DS_MODE_WIDE)
    {
        scissor_x = x * 2;
        scissor_y = y;
        scissor_width = (x + width) * 2;
        scissor_height = y + height;
    }
    else // gGfx3DSMode == GFX_3DS_MODE_NORMAL
    {
        scissor_x = x;
        scissor_y = y;
        scissor_width = x + width;
        scissor_height = y + height;
    }
}

static void applyBlend()
{
    if (sDynamicShadowMaskMode != 0) {
        // Receiver masks are stencil-only. Display-list material commands
        // may toggle alpha blending while the mask is being drawn; never let
        // those commands turn the collision mesh into visible gray geometry.
        C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ZERO, GPU_ONE,
                       GPU_ZERO, GPU_ONE);
    } else if (sUseBlend) {
        C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
    } else {
        C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
    }
}

static void gfx_citro3d_set_use_alpha(bool use_alpha)
{
    sUseBlend = use_alpha;
    applyBlend();
}

static u32 vec4ToU32Color(float r, float g, float b, float a)
{
    u8 r2 = MAX(0, MIN(255, r * 255));
    u8 g2 = MAX(0, MIN(255, g * 255));
    u8 b2 = MAX(0, MIN(255, b * 255));
    u8 a2 = MAX(0, MIN(255, a * 255));
    return (a2 << 24) | (b2 << 16) | (g2 << 8) | r2;
}

static void renderTwoColorTris(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris)
{
    int offset = 0;
    float* dst = &((float*)sVboBuffer)[sBufIdx * VERTEX_SHADER_SIZE];
    bool hasTex = sShaderProgramPool[sCurShader].cc_features.used_textures[0] || sShaderProgramPool[sCurShader].cc_features.used_textures[1];
    bool hasColor = sShaderProgramPool[sCurShader].cc_features.num_inputs > 0;
    bool hasAlpha = sShaderProgramPool[sCurShader].cc_features.opt_alpha;
    if (sShaderProgramPool[sCurShader].cc_features.opt_fog)
        C3D_TexEnvColor(C3D_GetTexEnv(2), vec4ToU32Color(buf_vbo[hasTex ? 6 : 4], buf_vbo[hasTex ? 7 : 5], buf_vbo[hasTex ? 8 : 6], buf_vbo[hasTex ? 9 : 7]));
    u32 firstColor0, firstColor1;
    bool color0Constant = true;
    bool color1Constant = true;
    //determine which color is constant over all vertices
    for (u32 i = 0; i < buf_vbo_num_tris * 3 && color0Constant && color1Constant; i++)
    {
        int vtxOffs = 4;
        if (hasTex)
            vtxOffs += 2;
        u32 color0 = vec4ToU32Color(
            buf_vbo[offset + vtxOffs],
            buf_vbo[offset + vtxOffs + 1],
            buf_vbo[offset + vtxOffs + 2],
            hasAlpha ? buf_vbo[offset + vtxOffs + 3] : 1.0f);
        vtxOffs += hasAlpha ? 4 : 3;
        u32 color1 = vec4ToU32Color(
            buf_vbo[offset + vtxOffs],
            buf_vbo[offset + vtxOffs + 1],
            buf_vbo[offset + vtxOffs + 2],
            hasAlpha ? buf_vbo[offset + vtxOffs + 3] : 1.0f);
        if (i == 0)
        {
            firstColor0 = color0;
            firstColor1 = color1;
        }
        else
        {
            if (firstColor0 != color0)
                color0Constant = false;
            if (firstColor1 != color1)
                color1Constant = false;
        }
        offset += sVtxUnitSize;
    }
    offset = 0;
    update_shader(!color1Constant);
    C3D_TexEnvColor(C3D_GetTexEnv(0), color1Constant ? firstColor1 : firstColor0);
    for (u32 i = 0; i < 3 * buf_vbo_num_tris; i++)
    {
        *dst++ = buf_vbo[offset + 0];
        *dst++ = buf_vbo[offset + 1];
        *dst++ = buf_vbo[offset + 2];
        *dst++ = buf_vbo[offset + 3];
        int vtxOffs = 4;
        if (hasTex)
        {
            *dst++ = buf_vbo[offset + vtxOffs++] * sTexturePoolScaleS[sCurTex];
            *dst++ = 1 - (buf_vbo[offset + vtxOffs++] * sTexturePoolScaleT[sCurTex]);
        }
        else
        {
            *dst++ = 0;
            *dst++ = 0;
        }
        if (color0Constant)
            vtxOffs += hasAlpha ? 4 : 3;
        if (hasColor)
        {
            *dst++ = buf_vbo[offset + vtxOffs++];
            *dst++ = buf_vbo[offset + vtxOffs++];
            *dst++ = buf_vbo[offset + vtxOffs++];
            *dst++ = hasAlpha ? buf_vbo[offset + vtxOffs++] : 1.0f;
        }
        else
        {
            *dst++ = 1.0f;
            *dst++ = 1.0f;
            *dst++ = 1.0f;
            *dst++ = 1.0f;
        }

        offset += sVtxUnitSize;
    }

    C3D_DrawArrays(GPU_TRIANGLES, sBufIdx, buf_vbo_num_tris * 3);
    sBufIdx += buf_vbo_num_tris * 3;
}

static void gfx_citro3d_draw_triangles(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris)
{
    bool shadowReceiverPass = sDynamicShadowReceiverPass && sShadowShaderDvlb != NULL;
    if (sDynamicShadowDepthPass) {
        gDynamicShadowDebugDepthTris += buf_vbo_num_tris;
    } else if (shadowReceiverPass) {
        gDynamicShadowDebugReceiverTris += buf_vbo_num_tris;
    }

    if (!shadowReceiverPass && sBufIdx * VERTEX_SHADER_SIZE > 1 * 1024 * 1024 / 4)
    {
        printf("Vertex buffer full!\n");
        return;
    }

    if (!shadowReceiverPass && sShaderProgramPool[sCurShader].cc_features.num_inputs > 1)
    {
        renderTwoColorTris(buf_vbo, buf_vbo_len, buf_vbo_num_tris);
        return;
    }

    int offset = 0;
    float* dst = shadowReceiverPass ? sShadowReceiverVboBuffer : &((float*)sVboBuffer)[sBufIdx * VERTEX_SHADER_SIZE];
    bool hasTex = sShaderProgramPool[sCurShader].cc_features.used_textures[0] || sShaderProgramPool[sCurShader].cc_features.used_textures[1];
    bool hasColor = sShaderProgramPool[sCurShader].cc_features.num_inputs > 0;
    bool hasAlpha = sShaderProgramPool[sCurShader].cc_features.opt_alpha;

    if (shadowReceiverPass) {
        if (buf_vbo_num_tris * 3 * DYNAMIC_SHADOW_RECEIVER_VERTEX_SIZE > 1 * 1024 * 1024 / 4) {
            printf("Shadow receiver vertex buffer full!\n");
            return;
        }
        hasTex = true;
        hasColor = true;
        hasAlpha = true;
    }
    for (u32 i = 0; i < 3 * buf_vbo_num_tris; i++)
    {
        *dst++ = buf_vbo[offset + 0];
        *dst++ = buf_vbo[offset + 1];
        *dst++ = buf_vbo[offset + 2];
        *dst++ = buf_vbo[offset + 3];
        int vtxOffs = 4;
        if (hasTex)
        {
            if (shadowReceiverPass) {
                *dst++ = buf_vbo[offset + vtxOffs++];
                *dst++ = buf_vbo[offset + vtxOffs++];
                *dst++ = buf_vbo[offset + vtxOffs++];
            } else {
                *dst++ = buf_vbo[offset + vtxOffs++] * sTexturePoolScaleS[sCurTex];
                *dst++ = 1 - (buf_vbo[offset + vtxOffs++] * sTexturePoolScaleT[sCurTex]);
            }
        }
        else
        {
            *dst++ = 0;
            *dst++ = 0;
        }
        if (hasColor)
        {
            *dst++ = buf_vbo[offset + vtxOffs++];
            *dst++ = buf_vbo[offset + vtxOffs++];
            *dst++ = buf_vbo[offset + vtxOffs++];
            *dst++ = hasAlpha ? buf_vbo[offset + vtxOffs++] : 1.0f;
        }
        else
        {
            *dst++ = 1.0f;
            *dst++ = 1.0f;
            *dst++ = 1.0f;
            *dst++ = 1.0f;
        }

        offset += shadowReceiverPass ? DYNAMIC_SHADOW_RECEIVER_VERTEX_SIZE : sVtxUnitSize;
    }

    if (shadowReceiverPass) {
        gfx_citro3d_configure_shadow_receiver_vertex_layout();
        C3D_BindProgram(&sShadowShaderProgram);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_shadowModelView, &modelView);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_shadowProjection, &projection);
        C3D_TexBind(0, &sDynamicShadowMapTex);
        C3D_TexShadowParams(false, 0.0125f);
        C3D_FragOpMode(GPU_FRAGOPMODE_GL);

        C3D_TexEnv* env = C3D_GetTexEnv(0);
        C3D_TexEnvInit(env);
        C3D_TexEnvColor(env, 0x88000000);
        C3D_TexEnvFunc(env, C3D_RGB, GPU_REPLACE);
        C3D_TexEnvSrc(env, C3D_RGB, GPU_CONSTANT, 0, 0);
        C3D_TexEnvOpRgb(env, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
        C3D_TexEnvFunc(env, C3D_Alpha, GPU_MODULATE);
        C3D_TexEnvSrc(env, C3D_Alpha, GPU_CONSTANT, GPU_TEXTURE0, 0);
        C3D_TexEnvOpAlpha(env, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_ONE_MINUS_SRC_R, GPU_TEVOP_A_SRC_ALPHA);
    }

    C3D_DrawArrays(GPU_TRIANGLES, shadowReceiverPass ? 0 : sBufIdx, buf_vbo_num_tris * 3);

    if (shadowReceiverPass) {
        C3D_FragOpMode(GPU_FRAGOPMODE_GL);
        C3D_BindProgram(&sShaderProgram);
        gfx_citro3d_configure_normal_vertex_layout();
        update_shader(false);
    } else {
        sBufIdx += buf_vbo_num_tris * 3;
    }
}

void gfx_citro3d_frame_draw_on(C3D_RenderTarget* target)
{
    target->used = true;
    C3D_SetFrameBuf(&target->frameBuf);
    C3D_SetViewport(viewport_y, viewport_x, viewport_height, viewport_width);
    if (scissor)
        C3D_SetScissor(GPU_SCISSOR_NORMAL, scissor_y, scissor_x, scissor_height, scissor_width);
}

static void gfx_citro3d_draw_triangles_helper(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris)
{
    bool canReceiveDynamicShadow;

    if (sDynamicShadowDepthPass) {
        gfx_citro3d_draw_triangles(buf_vbo, buf_vbo_len, buf_vbo_num_tris);
        return;
    }

    if (sIsHud)
    {
        gfx_citro3d_set_receiver_stencil(false);
        C3D_FrameDrawOn(gTargetBottom);

        gfx_citro3d_draw_triangles(buf_vbo, buf_vbo_len, buf_vbo_num_tris);
        return;
    }
    canReceiveDynamicShadow = sDynamicShadowMaskMode == 0
        && !sDynamicShadowStencil && sDynamicShadowReceiver;
    gfx_citro3d_set_receiver_stencil(canReceiveDynamicShadow);
    if (gGfx3DEnabled)
    {
        // left screen
        sOrigBufIdx = sBufIdx;
        stereoTilt(&projection, -iodZ, -iodW);
        gfx_citro3d_frame_draw_on(gTarget);
        gfx_citro3d_draw_triangles(buf_vbo, buf_vbo_len, buf_vbo_num_tris);

        // right screen
        sBufIdx = sOrigBufIdx;
        stereoTilt(&projection, iodZ, iodW);
        gfx_citro3d_frame_draw_on(gTargetRight);
        gfx_citro3d_draw_triangles(buf_vbo, buf_vbo_len, buf_vbo_num_tris);
        return;
    }
    gfx_citro3d_frame_draw_on(gTarget);
    gfx_citro3d_draw_triangles(buf_vbo, buf_vbo_len, buf_vbo_num_tris);
}

static void gfx_citro3d_init(void)
{
    sVShaderDvlb = DVLB_ParseFile((__3ds_u32*)shader_shbin, shader_shbin_size);
    shaderProgramInit(&sShaderProgram);
    shaderProgramSetVsh(&sShaderProgram, &sVShaderDvlb->DVLE[0]);
    C3D_BindProgram(&sShaderProgram);

    uLoc_projection = shaderInstanceGetUniformLocation((&sShaderProgram)->vertexShader, "projection");
    uLoc_modelView = shaderInstanceGetUniformLocation((&sShaderProgram)->vertexShader, "modelView");

    sShadowShaderDvlb = DVLB_ParseFile((__3ds_u32*)shadow_shbin, shadow_shbin_size);
    shaderProgramInit(&sShadowShaderProgram);
    if (sShadowShaderDvlb != NULL) {
        shaderProgramSetVsh(&sShadowShaderProgram, &sShadowShaderDvlb->DVLE[0]);
        uLoc_shadowProjection = shaderInstanceGetUniformLocation((&sShadowShaderProgram)->vertexShader, "projection");
        uLoc_shadowModelView = shaderInstanceGetUniformLocation((&sShadowShaderProgram)->vertexShader, "modelView");
    }

    // Create 1MB VBO (vertex buffer object)
    sVboBuffer = linearAlloc(1 * 1024 * 1024);
    // Bottom-screen UI must not share the top-screen write cursor. Title,
    // Goddard and credits frames can nearly fill the main VBO.
    sBottomVboBuffer = linearAlloc(128 * 1024);
    sShadowReceiverVboBuffer = linearAlloc(1 * 1024 * 1024);

    // Configure buffers
    gfx_citro3d_configure_normal_vertex_layout();

    C3D_CullFace(GPU_CULL_NONE);
    C3D_DepthMap(true, -1.0f, 0);
    C3D_DepthTest(false, GPU_LEQUAL, GPU_WRITE_ALL);
    C3D_StencilTest(false, GPU_ALWAYS, 0, 0xFF, 0xFF);
    C3D_StencilOp(GPU_STENCIL_KEEP, GPU_STENCIL_KEEP, GPU_STENCIL_KEEP);
    C3D_AlphaTest(true, GPU_GREATER, 0x00);

    gfx_citro3d_init_dynamic_shadowmap();

#ifdef VERSION_EU
    C3D_FrameRate(25);
#else
    C3D_FrameRate(60);

    gfx_3ds_init_minimap();
#endif
}

static void gfx_citro3d_start_frame(void)
{
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    sBufIdx = 0;
    scissor = false;
    s2DMode = 0;
    sIsHud = false;
    gfx_citro3d_restore_normal_pipeline();
    // reset viewport if video mode changed
    if (gGfx3DSMode != sCurrentGfx3DSMode)
    {
        gfx_citro3d_set_viewport(0, 0, 400, 240);
        sCurrentGfx3DSMode = gGfx3DSMode;
    }

    // Due to hardware differences, the PC port always clears the depth buffer,
    // rather than just when the N64 would clear it.
    gfx_citro3d_set_viewport_clear_buffer(VIEW_MAIN_SCREEN, VIEW_CLEAR_BUFFER_DEPTH);
    if (gfx_citro3d_should_clear_staff_roll_top())
        gfx_citro3d_set_viewport_clear_buffer(VIEW_MAIN_SCREEN, VIEW_CLEAR_BUFFER_COLOR);

    clear_buffers();

    // Reset screen clear buffer flags
    screen_clear_bufs.struc.top = 
    screen_clear_bufs.struc.bottom = VIEW_CLEAR_BUFFER_NONE;

    // reset model view matrix
    Mtx_Identity(&modelView);
    // 3DS screen is rotated 90 degrees
    Mtx_RotateZ(&modelView, 0.75f*M_TAU, false);
    // reset projection
    Mtx_Identity(&projection);
    // set uniforms
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, &modelView);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);
}

static void gfx_citro3d_on_resize(void)
{
}

static void gfx_citro3d_end_frame(void)
{
    /*
     * Draw the bottom screen last.  Title, Goddard, transitions and credits
     * all mutate the shared Citro3D pipeline in unusual ways; drawing the
     * bottom screen at frame start let those later commands corrupt its
     * texture/layout state in wide (3D-slider-zero) mode.
     */
    gfx_citro3d_begin_bottom_screen_pass();
    uint32_t tris = gfx_3ds_draw_minimap(sBottomVboBuffer, 0);
    if (gShowConfigMenu && !gfx_3ds_config_menu_can_open()) {
        gShowConfigMenu = false;
        gBottomScreenNeedsRender = true;
    }
    if (gShowConfigMenu)
        tris += gfx_3ds_menu_draw(sBottomVboBuffer, tris, true);

    gfx_citro3d_restore_normal_pipeline();
    s2DMode = 0;
    sIsHud = false;

    C3D_FrameEnd(0); // Swap is handled automatically within this function
}

static void gfx_citro3d_finish_render(void)
{
}

static void gfx_citro3d_set_fog(uint16_t from, uint16_t to)
{
    // dumb enough
    uint32_t id = (from << 16) | to;
    // current already loaded
    if (fog_lut[current_fog_idx].id == id)
        return;
    // lut already calculated
    for (int i = 0; i < fog_lut_size; i++)
    {
        if (fog_lut[i].id == id)
        {
            current_fog_idx = i;
            return;
        }
    }
    // new lut required
    if (fog_lut_size == FOG_LUT_SIZE)
    {
        printf("Fog exhausted!\n");
        return;
    }

    current_fog_idx = fog_lut_size++;
    (&fog_lut[current_fog_idx])->id = id;

    // FIXME: The near/far factors are personal preference
    // BOB:  6400, 59392 => 0.16, 116
    // JRB:  1280, 64512 => 0.80, 126
    FogLut_Exp(&fog_lut[current_fog_idx].lut, 0.05f, 1.5f, 1024 / (float)from, ((float)to) / 512);
}

static void gfx_citro3d_set_fog_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    fog_color = (a << 24) | (b << 16) | (g << 8) | r; // Why is this reversed? Weird endianness?
}

void gfx_citro3d_set_clear_color(enum ViewportId3DS viewport, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    screen_clear_colors.array[viewport] = COLOR_RGBA_PARAMS_TO_RGBA32(r, g, b, a);
}

void gfx_citro3d_set_clear_color_RGBA32(enum ViewportId3DS viewport, u32 color)
{
    screen_clear_colors.array[viewport] = color;
}

void gfx_citro3d_set_viewport_clear_buffer(enum ViewportId3DS viewport, enum ViewportClearBuffer mode)
{
    screen_clear_bufs.array[viewport] |= mode;
}

struct GfxRenderingAPI gfx_citro3d_api = {
    gfx_citro3d_z_is_from_0_to_1,
    gfx_citro3d_unload_shader,
    gfx_citro3d_load_shader,
    gfx_citro3d_create_and_load_new_shader,
    gfx_citro3d_lookup_shader,
    gfx_citro3d_shader_get_info,
    gfx_citro3d_new_texture,
    gfx_citro3d_select_texture,
    gfx_citro3d_upload_texture,
    gfx_citro3d_set_sampler_parameters,
    gfx_citro3d_set_depth_test,
    gfx_citro3d_set_depth_mask,
    gfx_citro3d_set_zmode_decal,
    gfx_citro3d_set_viewport,
    gfx_citro3d_set_scissor,
    gfx_citro3d_set_use_alpha,
    gfx_citro3d_draw_triangles_helper,
    gfx_citro3d_init,
    gfx_citro3d_on_resize,
    gfx_citro3d_start_frame,
    gfx_citro3d_end_frame,
    gfx_citro3d_finish_render,
    gfx_citro3d_set_fog,
    gfx_citro3d_set_fog_color,
    gfx_citro3d_is_hud,
    gfx_citro3d_set_2d,
    gfx_citro3d_set_iod,
    gfx_citro3d_set_dynamic_shadow_receiver,
    gfx_citro3d_set_dynamic_shadow_mask,
    gfx_citro3d_set_dynamic_shadow_pass
};

#endif
