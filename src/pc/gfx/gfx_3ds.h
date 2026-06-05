#ifdef TARGET_N3DS

#ifndef GFX_3DS_H
#define GFX_3DS_H

#include "gfx_window_manager_api.h"

#ifndef _LANGUAGE_C
#define _LANGUAGE_C
#endif

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

#include <PR/gbi.h>

#include <citro3d.h>
#include <tex3ds.h>

#define VERTEX_SHADER_SIZE 10

#define DISPLAY_TRANSFER_FLAGS \
	(GX_TRANSFER_FLIP_VERT(0) | \
     GX_TRANSFER_OUT_TILED(0) | \
     GX_TRANSFER_RAW_COPY(0) | \
	 GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | \
     GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8))

extern C3D_RenderTarget *gTarget;
extern C3D_RenderTarget *gTargetRight;
extern C3D_RenderTarget *gTargetBottom;

extern bool gBottomScreenNeedsRender;

extern int uLoc_projection, uLoc_modelView;

extern float gSliderLevel;

typedef enum
{
    GFX_3DS_MODE_NORMAL,     // 400px no AA AND 400px 3D | !useAA && !useWide
    GFX_3DS_MODE_AA_22,      // 400px +  AA (unused)     |  useAA && !useWide
    GFX_3DS_MODE_WIDE,       // 800px no AA              | !useAA &&  useWide
    GFX_3DS_MODE_WIDE_AA_12  // 800px +  AA              |  useAA &&  useWide
} Gfx3DSMode;

extern bool gShouldRun;
extern bool gShowConfigMenu;

extern struct GfxWindowManagerAPI gfx_3ds;
extern Gfx3DSMode gGfx3DSMode;
extern bool gGfx3DEnabled;

static bool load_t3x_texture(C3D_Tex* tex, C3D_TexCube* cube, const void* data, size_t size)
{
    Tex3DS_Texture t3x = Tex3DS_TextureImport(data, size, tex, cube, false);
    if (!t3x)
        return false;
    Tex3DS_TextureFree(t3x);
    return true;
}

#endif
#endif
