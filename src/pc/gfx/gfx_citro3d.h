#ifndef GFX_CITRO3D_H
#define GFX_CITRO3D_H

#include <stdbool.h>

#include "gfx_rendering_api.h"
#include "multi_viewport/multi_viewport.h"

enum ViewportId3DS {
    VIEW_MAIN_SCREEN   = 0,
    VIEW_BOTTOM_SCREEN = 1
};

extern struct GfxRenderingAPI gfx_citro3d_api;

// WYATT_TODO figure out how to get these functions into the GfxRenderingAPI. I'm done for tonight.

// Sets the clear color for a Viewport.
void gfx_citro3d_set_clear_color(enum ViewportId3DS viewport, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

// Sets the clear color for a Viewport.
void gfx_citro3d_set_clear_color_RGBA32(enum ViewportId3DS viewport, u32 color);

// Sets a buffer to be cleared for the given viewport on the next frame.
// All flags provided will be cleared on gfx_citro3d_start_frame().
void gfx_citro3d_set_viewport_clear_buffer(enum ViewportId3DS viewport, enum ViewportClearBuffer mode);

// Enables a one-write stencil mask for projected dynamic shadows so overlapping
// model parts do not repeatedly darken the same screen pixel.
void gfx_citro3d_set_dynamic_shadow_stencil(bool enabled);

// Returns true when the experimental dynamic-shadow shadowmap resources are
// available. The map is intentionally allocated independently from the current
// planar fallback so the feature can be wired in incrementally.
bool gfx_citro3d_dynamic_shadowmap_available(void);

#endif
