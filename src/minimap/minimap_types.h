#ifndef GFX_MINIMAP_TYPES_H
#define GFX_MINIMAP_TYPES_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

struct MiniMapInfo {
    const uint8_t *texture;
    const size_t size;
    const uint32_t color; // RRGGBB
    const bool is_placeholder;
};

struct MiniMapLevel {
    const struct MiniMapInfo *areas;
    const size_t area_count;
};

#endif
