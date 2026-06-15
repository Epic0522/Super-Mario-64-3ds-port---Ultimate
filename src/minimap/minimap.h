#ifndef GFX_MINIMAP_H
#define GFX_MINIMAP_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "types.h"

bool minimap_has_level_or_area_changed();
bool minimap_load_level_and_area();
void minimap_get_current_texture(uint8_t **texture, size_t *texture_size, uint32_t *color);
bool minimap_get_mario_position(float *mario_x, float *mario_y, float *mario_direction);
void minimap_set_level_select_preview(bool active, s16 level, s8 area);
bool minimap_is_level_select_preview_active(void);

#endif
