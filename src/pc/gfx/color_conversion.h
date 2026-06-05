#ifndef GFX_COLOR_CONVERSION_H
#define GFX_COLOR_CONVERSION_H

// Conversion macros for colors
// Author: Wyatt-James
// Please keep the formatting tidy!


// ------------------------- TYPE DESCRIPTIONS -------------------------


// RGBA16 is a 16-bit number with bits: RRRRR GGGGG BBBBB A
// RGBA_S32_N64 is a 32-bit number containing two copies of an RGBA16
// RGBA32 is a 32-bit number with bits: RRRRRRRR GGGGGGGG BBBBBBBB AAAAAAAA

// u8 RGB parameters are parameter lists with the format: (u8) R, (u8) G, (u8) B
// u8 RGBA parameters are parameter lists with the format: (u8) R, (u8) G, (u8) B, (u8) A

// When converting from RGB24 to RGB16, the 3 least significant bits are dropped (div. by 8)


// ------------------------- CONSTANTS -------------------------


#define RGB5_TO_8_SCALE 1.02822580645f


// ------------------------- COLOR CHANNEL ACCESSORS -------------------------

// ---------- RGBA32 ----------


// Gets the red color channel of an RGBA32 color, in bits  0-7 (example: 0b10001111).
#define COLOR_RGBA32_GET_RED(COL)   ((COL >> 24) & 0xFF)

// Gets the green color channel of an RGBA32 color, in bits  0-7 (example: 0b10001111).
#define COLOR_RGBA32_GET_GREEN(COL) ((COL >> 16) & 0xFF)

// Gets the blue color channel of an RGBA32 color, in bits  0-7 (example: 0b10001111).
#define COLOR_RGBA32_GET_BLUE(COL)  ((COL >>  8) & 0xFF)

// Gets the alpha channel of an RGBA32 color, in bits 0-7 (example: 0b10001111).
#define COLOR_RGBA32_GET_ALPHA(COL)  (COL  &  0xFF)


// ---------- RGBA16 ----------

// Gets the red color channel of an RGBA16 color, in bits 0-4 (example: 0b10001).
#define COLOR_RGBA16_GET_RED(COL)   ((COL >> 11) & 0b11111)

// Gets the green color channel of an RGBA16 color, in bits 0-4 (example: 0b10001).
#define COLOR_RGBA16_GET_GREEN(COL) ((COL >>  6) & 0b11111)

// Gets the blue color channel of an RGBA16 color, in bits 0-4 (example: 0b10001).
#define COLOR_RGBA16_GET_BLUE(COL)  ((COL >>  1) & 0b11111)

// Gets the alpha channel of an RGBA16 color, in bit 0 (example: 0b10001).
#define COLOR_RGBA16_GET_ALPHA(COL)  (COL  &  1)

// ---------- RGBA_S32_N64 ----------

// Gets the red color channel of an RGBA_S32_N64 color, in bits 0-4 (example: 0b10001).
#define COLOR_RGBA_S32_N64_GET_RED(COL)   ((COL >> 11) & 0b11111)

// Gets the red color channel of an RGBA_S32_N64 color, in bits 0-4 (example: 0b10001).
#define COLOR_RGBA_S32_N64_GET_GREEN(COL) ((COL >>  6) & 0b11111)

// Gets the red color channel of an RGBA_S32_N64 color, in bits 0-4 (example: 0b10001).
#define COLOR_RGBA_S32_N64_GET_BLUE(COL)  ((COL >>  1) & 0b11111)

// Gets the alpha channel of an RGBA_S32_N64 color, in bit 0
#define COLOR_RGBA_S32_N64_GET_ALPHA(COL)  (COL  &  1)

// Gets the red color channel of an RGBA_S32_N64 color, shifted into bits 3-7 (example: 0b10001 is 0b10001000).
#define COLOR_RGBA_S32_N64_GET_RED_S(COL)   ((COL >>  8) & 0b11111000)

// Gets the green color channel of an RGBA_S32_N64 color, shifted into bits 3-7 (example: 0b10001 is 0b10001000).
#define COLOR_RGBA_S32_N64_GET_GREEN_S(COL) ((COL >>  3) & 0b11111000)

// Gets the blue color channel of an RGBA_S32_N64 color, shifted into bits 3-7 (example: 0b10001 is 0b10001000).
#define COLOR_RGBA_S32_N64_GET_BLUE_S(COL)  ((COL <<  2) & 0b11111000)


// ------------------------- RGBA32 Macros -------------------------


// Converts an RGBA32 color to u8 RGB parameters.
#define COLOR_RGBA32_TO_RGB_PARAMS(COL) (u8) COLOR_RGBA32_GET_RED(COL), \
                                        (u8) COLOR_RGBA32_GET_GREEN(COL), \
                                        (u8) COLOR_RGBA32_GET_BLUE(COL)

// Converts an RGBA32 color to u8 RGBA parameters.
#define COLOR_RGBA32_TO_RGBA_PARAMS(COL) COLOR_RGBA32_TO_RGB_PARAMS(COL), \
                                         (u8) (COLOR_RGBA32_GET_ALPHA(COL))

// Converts RGBA parameters to an RGBA32 value.
#define COLOR_RGBA_PARAMS_TO_RGBA32(R, G, B, A) ((R << 24) | (G << 16) | (B << 8) | A)

// Converts RGB parameters to an RGBA32 value.
#define COLOR_RGB_PARAMS_TO_RGBA32(R, G, B) COLOR_RGBA_PARAMS_TO_RGBA32(R, G, B, 0xFF)


// ------------------------- RGBA16 Macros -------------------------


// Converts an RGBA16 color to an RGBA32 color.
// Values are not scaled, so they occupy the range 0-248.
#define COLOR_RGBA16_TO_RGBA32_UNSCALED(COL) (u32) ((COLOR_RGBA16_GET_RED(COL)   << 27  ) | \
                                                    (COLOR_RGBA16_GET_GREEN(COL) << 19  ) | \
                                                    (COLOR_RGBA16_GET_BLUE(COL)  << 11  ) | \
                                                    (COLOR_RGBA16_GET_ALPHA(COL) * 0xFF))

// Converts an RGBA16 color to an RGBA32 color.
// Values are scaled, so they occupy the range 0-255.
#define COLOR_RGBA16_TO_RGBA32_SCALED(COL) (u32) ((((u32) (COLOR_RGBA16_GET_RED(COL)   * RGB5_TO_8_SCALE)) << 24  ) | \
                                                  (((u32) (COLOR_RGBA16_GET_GREEN(COL) * RGB5_TO_8_SCALE)) << 16  ) | \
                                                  (((u32) (COLOR_RGBA16_GET_BLUE(COL)  * RGB5_TO_8_SCALE)) <<  8  ) | \
                                                  (COLOR_RGBA16_GET_ALPHA(COL) * 0xFF))

// Converts an RGBA16 color to u8 RGB parameters.
// Values are unscaled, so they occupy the range 0-248.
#define COLOR_RGBA16_N64_TO_RGB_PARAMS_UNSCALED(COL) (u8) COLOR_RGBA16_GET_RED(COL), \
                                                     (u8) COLOR_RGBA16_GET_GREEN(COL), \
                                                     (u8) COLOR_RGBA16_GET_BLUE(COL)

// Converts an RGBA16 color to u8 RGB parameters.
// Values are scaled, so they occupy the range 0-255.
#define COLOR_RGBA16_N64_TO_RGB_PARAMS_SCALED(COL) (u8) (COLOR_RGBA16_GET_RED(COL)   * RGB5_TO_8_SCALE), \
                                                   (u8) (COLOR_RGBA16_GET_GREEN(COL) * RGB5_TO_8_SCALE), \
                                                   (u8) (COLOR_RGBA16_GET_BLUE(COL)  * RGB5_TO_8_SCALE)

// Converts an RGBA16 color to u8 RGBA parameters.
// Values are unscaled, so they occupy the range 0-248.
#define COLOR_RGBA16_N64_TO_RGBA_PARAMS_UNSCALED(COL) COLOR_RGBA16_N64_TO_RGB_PARAMS_UNSCALED(COL), \
                                                      (u8) (COLOR_RGBA16_GET_ALPHA(COL) * 0xFF)

// Converts an RGBA16 color to u8 RGBA parameters.
// Values are scaled, so they occupy the range 0-255.
#define COLOR_RGBA16_N64_TO_RGBA_PARAMS_SCALED(COL) COLOR_RGBA16_N64_TO_RGB_PARAMS_SCALED(COL), \
                                                    (u8) (COLOR_RGBA16_GET_ALPHA(COL)   * 0xFF)


// ------------------------- RGBA_S32_N64 Macros -------------------------


// RGBA32 is ((RGBA16 << 16) | RGBA16), so we can just drop the upper 16 bits.
#define COLOR_RGBA_S32_N64_TO_RGBA_U16(COL) (COL & 0xFFFF)


// Converts an RGBA_S32_N64 color to an RGBA32 color.
// Values are not scaled, so they occupy the range 0-248.
#define COLOR_RGBA_S32_N64_TO_RGBA32_UNSCALED(COL) ((COLOR_RGBA_S32_N64_GET_RED_S(COL)   << 24) | \
                                                    (COLOR_RGBA_S32_N64_GET_GREEN_S(COL) << 16) | \
                                                    (COLOR_RGBA_S32_N64_GET_BLUE_S(COL)  <<  8) | \
                                                    (COLOR_RGBA_S32_N64_GET_ALPHA(COL) * 0xFF))

// Converts an RGBA_S32_N64 color to an RGBA32 color.
// Values are scaled, so they occupy the range 0-255.
#define COLOR_RGBA_S32_N64_TO_RGBA32_SCALED(COL) ((((u32) (COLOR_RGBA_S32_N64_GET_RED_S(COL)   * RGB5_TO_8_SCALE)) << 24) | \
                                                  (((u32) (COLOR_RGBA_S32_N64_GET_GREEN_S(COL) * RGB5_TO_8_SCALE)) << 16) | \
                                                  (((u32) (COLOR_RGBA_S32_N64_GET_BLUE_S(COL)  * RGB5_TO_8_SCALE)) <<  8) | \
                                                  (COLOR_RGBA_S32_N64_GET_ALPHA(COL) * 0xFF))

// Converts an RGBA_S32_N64 color to u8 RGB parameters.
// Values are unscaled, so they occupy the range 0-248.
#define COLOR_RGBA_S32_N64_TO_RGB_PARAMS_UNSCALED(COL) (u8) COLOR_RGBA_S32_N64_GET_RED_S(COL), \
                                                       (u8) COLOR_RGBA_S32_N64_GET_GREEN_S(COL), \
                                                       (u8) COLOR_RGBA_S32_N64_GET_BLUE_S(COL)

// Converts an RGBA_S32_N64 color to u8 RGB parameters.
// Values are scaled, so they occupy the range 0-255.
#define COLOR_RGBA_S32_N64_TO_RGB_PARAMS_SCALED(COL) (u8) (COLOR_RGBA_S32_N64_GET_RED_S(COL)   * RGB5_TO_8_SCALE), \
                                                     (u8) (COLOR_RGBA_S32_N64_GET_GREEN_S(COL) * RGB5_TO_8_SCALE), \
                                                     (u8) (COLOR_RGBA_S32_N64_GET_BLUE_S(COL)  * RGB5_TO_8_SCALE)

// Converts an RGBA_S32_N64 color to u8 RGBA parameters.
// Values are unscaled, so they occupy the range 0-248.
#define COLOR_RGBA_S32_N64_TO_RGBA_PARAMS_UNSCALED(COL) COLOR_RGBA_S32_N64_TO_RGB_PARAMS_UNSCALED(COL), \
                                                        (u8) (COLOR_RGBA_S32_N64_GET_ALPHA(COL) * 0xFF)

// Converts an RGBA_S32_N64 color to u8 RGBA parameters.
// Values are scaled, so they occupy the range 0-255.
#define COLOR_RGBA_S32_N64_TO_RGBA_PARAMS_SCALED(COL) COLOR_RGBA_S32_N64_TO_RGB_PARAMS_SCALED(COL), \
                                                      (u8) (COLOR_RGBA_S32_N64_GET_ALPHA(COL)   * 0xFF)

#endif
