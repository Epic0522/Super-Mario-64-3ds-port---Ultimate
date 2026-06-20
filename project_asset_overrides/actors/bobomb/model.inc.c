// Bobomb

// 0x0801DA60
ALIGNED8 static const u8 bobomb_seg8_texture_0801DA60[] = {
#include "actors/bobomb/bob-omb_left_side.rgba16.inc.c"
};

// 0x0801EA60
ALIGNED8 static const u8 bobomb_seg8_texture_0801EA60[] = {
#include "actors/bobomb/bob-omb_right_side.rgba16.inc.c"
};

// 0x0801FA60
ALIGNED8 static const u8 bobomb_seg8_texture_0801FA60[] = {
#include "actors/bobomb/bob-omb_buddy_left_side.rgba16.inc.c"
};

// 0x08020A60
ALIGNED8 static const u8 bobomb_seg8_texture_08020A60[] = {
#include "actors/bobomb/bob-omb_buddy_right_side.rgba16.inc.c"
};

// 0x08021A60
ALIGNED8 static const u8 bobomb_seg8_texture_08021A60[] = {
#include "actors/bobomb/bob-omb_eyes.rgba16.inc.c"
};

// 0x08022260
ALIGNED8 static const u8 bobomb_seg8_texture_08022260[] = {
#include "actors/bobomb/bob-omb_eyes_blink.rgba16.inc.c"
};

// 0x08022A60
static const Vtx bobomb_seg8_vertex_08022A60[] = {
    {{{   133,    -47,      0}, 0, {   480,      0}, {0xff, 0xff, 0xff, 0xff}}},
    {{{   133,     32,      0}, 0, {   480,    990}, {0xff, 0xff, 0xff, 0xff}}},
    {{{   128,     32,     50}, 0, {   990,    990}, {0xff, 0xff, 0xff, 0xff}}},
    {{{   128,    -47,    -49}, 0, {     0,      0}, {0xff, 0xff, 0xff, 0xff}}},
    {{{   128,    -47,     50}, 0, {   990,      0}, {0xff, 0xff, 0xff, 0xff}}},
    {{{   128,     32,    -49}, 0, {     0,    990}, {0xff, 0xff, 0xff, 0xff}}},
};

// 0x08022AC0 - 0x08022B08
const Gfx bobomb_seg8_dl_08022AC0[] = {
    gsDPPipeSync(),
    gsDPSetCombineMode(G_CC_DECALRGBA, G_CC_DECALRGBA),
    gsSPClearGeometryMode(G_LIGHTING),
    gsDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOLOD, G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOLOD),
    gsSPTexture(0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON),
    gsDPTileSync(),
    gsDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 8, 0, G_TX_RENDERTILE, 0, G_TX_WRAP | G_TX_NOMIRROR, 5, G_TX_NOLOD, G_TX_WRAP | G_TX_NOMIRROR, 5, G_TX_NOLOD),
    gsDPSetTileSize(0, 0, 0, (32 - 1) << G_TEXTURE_IMAGE_FRAC, (32 - 1) << G_TEXTURE_IMAGE_FRAC),
    gsSPEndDisplayList(),
};

// 0x08022B08 - 0x08022B58
const Gfx bobomb_seg8_dl_08022B08[] = {
    gsSPVertex(bobomb_seg8_vertex_08022A60, 6, 0),
    gsSP2Triangles( 0,  1,  2, 0x0,  3,  1,  0, 0x0),
    gsSP2Triangles( 0,  2,  4, 0x0,  3,  5,  1, 0x0),
    gsSPTexture(0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF),
    gsDPPipeSync(),
    gsDPSetCombineMode(G_CC_SHADE, G_CC_SHADE),
    gsSPSetGeometryMode(G_LIGHTING),
    gsSPEndDisplayList(),
};

// 0x08022B58 - 0x08022B88
const Gfx bobomb_seg8_dl_08022B58[] = {
    gsSPDisplayList(bobomb_seg8_dl_08022AC0),
    gsDPSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, bobomb_seg8_texture_08021A60),
    gsDPLoadSync(),
    gsDPLoadBlock(G_TX_LOADTILE, 0, 0, 32 * 32 - 1, CALC_DXT(32, G_IM_SIZ_16b_BYTES)),
    gsSPDisplayList(bobomb_seg8_dl_08022B08),
    gsSPEndDisplayList(),
};

// 0x08022B88 - 0x08022BB8
const Gfx bobomb_seg8_dl_08022B88[] = {
    gsSPDisplayList(bobomb_seg8_dl_08022AC0),
    gsDPSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, bobomb_seg8_texture_08022260),
    gsDPLoadSync(),
    gsDPLoadBlock(G_TX_LOADTILE, 0, 0, 32 * 32 - 1, CALC_DXT(32, G_IM_SIZ_16b_BYTES)),
    gsSPDisplayList(bobomb_seg8_dl_08022B08),
    gsSPEndDisplayList(),
};

// 0x08022BB8
static const Vtx bobomb_seg8_vertex_08022BB8[] = {
    {{{     0,     49,      0}, 0, {   990,      0}, {0xff, 0xff, 0xff, 0xff}}},
    {{{   -49,    -49,      0}, 0, {     0,   2012}, {0xff, 0xff, 0xff, 0xff}}},
    {{{     0,    -49,      0}, 0, {   990,   2012}, {0xff, 0xff, 0xff, 0xff}}},
    {{{   -49,     49,      0}, 0, {     0,      0}, {0xff, 0xff, 0xff, 0xff}}},
};

// 0x08022BF8
static const Vtx bobomb_seg8_vertex_08022BF8[] = {
    {{{    49,     49,      0}, 0, {   990,      0}, {0xff, 0xff, 0xff, 0xff}}},
    {{{     0,    -49,      0}, 0, {     0,   2012}, {0xff, 0xff, 0xff, 0xff}}},
    {{{    49,    -49,      0}, 0, {   990,   2012}, {0xff, 0xff, 0xff, 0xff}}},
    {{{     0,     49,      0}, 0, {     0,      0}, {0xff, 0xff, 0xff, 0xff}}},
};

// 0x08022C38 - 0x08022CA0
const Gfx bobomb_seg8_dl_08022C38[] = {
    gsDPSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, bobomb_seg8_texture_0801DA60),
    gsDPLoadSync(),
    gsDPLoadBlock(G_TX_LOADTILE, 0, 0, 32 * 64 - 1, CALC_DXT(32, G_IM_SIZ_16b_BYTES)),
    gsSPVertex(bobomb_seg8_vertex_08022BB8, 4, 0),
    gsSP2Triangles( 0,  1,  2, 0x0,  0,  3,  1, 0x0),
    gsDPSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, bobomb_seg8_texture_0801EA60),
    gsDPLoadSync(),
    gsDPLoadBlock(G_TX_LOADTILE, 0, 0, 32 * 64 - 1, CALC_DXT(32, G_IM_SIZ_16b_BYTES)),
    gsSPVertex(bobomb_seg8_vertex_08022BF8, 4, 0),
    gsSP2Triangles( 0,  1,  2, 0x0,  0,  3,  1, 0x0),
    gsSPEndDisplayList(),
};

// 0x08022CA0 - 0x08022D08
const Gfx bobomb_seg8_dl_08022CA0[] = {
    gsDPSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, bobomb_seg8_texture_0801FA60),
    gsDPLoadSync(),
    gsDPLoadBlock(G_TX_LOADTILE, 0, 0, 32 * 64 - 1, CALC_DXT(32, G_IM_SIZ_16b_BYTES)),
    gsSPVertex(bobomb_seg8_vertex_08022BB8, 4, 0),
    gsSP2Triangles( 0,  1,  2, 0x0,  0,  3,  1, 0x0),
    gsDPSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, bobomb_seg8_texture_08020A60),
    gsDPLoadSync(),
    gsDPLoadBlock(G_TX_LOADTILE, 0, 0, 32 * 64 - 1, CALC_DXT(32, G_IM_SIZ_16b_BYTES)),
    gsSPVertex(bobomb_seg8_vertex_08022BF8, 4, 0),
    gsSP2Triangles( 0,  1,  2, 0x0,  0,  3,  1, 0x0),
    gsSPEndDisplayList(),
};

// 0x08022D08 - 0x08022D78
const Gfx bobomb_seg8_dl_08022D08[] = {
    gsDPPipeSync(),
    gsDPSetCombineMode(G_CC_DECALRGBA, G_CC_DECALRGBA),
    gsSPClearGeometryMode(G_LIGHTING),
    gsDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOLOD, G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOLOD),
    gsSPTexture(0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON),
    gsDPTileSync(),
    gsDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 8, 0, G_TX_RENDERTILE, 0, G_TX_CLAMP, 6, G_TX_NOLOD, G_TX_CLAMP, 5, G_TX_NOLOD),
    gsDPSetTileSize(0, 0, 0, (32 - 1) << G_TEXTURE_IMAGE_FRAC, (64 - 1) << G_TEXTURE_IMAGE_FRAC),
    gsSPDisplayList(bobomb_seg8_dl_08022C38),
    gsSPTexture(0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF),
    gsDPPipeSync(),
    gsDPSetCombineMode(G_CC_SHADE, G_CC_SHADE),
    gsSPSetGeometryMode(G_LIGHTING),
    gsSPEndDisplayList(),
};

// 0x08022D78 - 0x08022DE8
const Gfx bobomb_seg8_dl_08022D78[] = {
    gsDPPipeSync(),
    gsDPSetCombineMode(G_CC_DECALRGBA, G_CC_DECALRGBA),
    gsSPClearGeometryMode(G_LIGHTING),
    gsDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOLOD, G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOLOD),
    gsSPTexture(0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON),
    gsDPTileSync(),
    gsDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 8, 0, G_TX_RENDERTILE, 0, G_TX_CLAMP, 6, G_TX_NOLOD, G_TX_CLAMP, 5, G_TX_NOLOD),
    gsDPSetTileSize(0, 0, 0, (32 - 1) << G_TEXTURE_IMAGE_FRAC, (64 - 1) << G_TEXTURE_IMAGE_FRAC),
    gsSPDisplayList(bobomb_seg8_dl_08022CA0),
    gsSPTexture(0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF),
    gsDPPipeSync(),
    gsDPSetCombineMode(G_CC_SHADE, G_CC_SHADE),
    gsSPSetGeometryMode(G_LIGHTING),
    gsSPEndDisplayList(),
};

// 0x08022DE8
static const Lights1 bobomb_seg8_lights_08022DE8 = gdSPDefLights1(
    0x3f, 0x26, 0x04,
    0xff, 0x99, 0x12, 0x28, 0x28, 0x28
);

// 0x08022E00
static const Lights1 bobomb_seg8_lights_08022E00 = gdSPDefLights1(
    0x2c, 0x2c, 0x2c,
    0xb2, 0xb2, 0xb2, 0x28, 0x28, 0x28
);

// Unreferenced light group
static const Lights1 bobomb_lights_unused = gdSPDefLights1(
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x28, 0x28, 0x28
);

// 0x08022E30
static const Vtx bobomb_seg8_vertex_08022E30[] = {
    {{{    27,    -26,    -31}, 0, {     0,      0}, {0xeb, 0x96, 0xbf, 0x00}}},
    {{{   -36,      0,    -20}, 0, {     0,      0}, {0xb1, 0xd0, 0xaa, 0x00}}},
    {{{    32,      0,    -41}, 0, {     0,      0}, {0xfc, 0xfb, 0x82, 0x00}}},
    {{{    85,      0,    -32}, 0, {     0,      0}, {0x50, 0xf1, 0xa0, 0xff}}},
    {{{    79,     28,    -20}, 0, {     0,      0}, {0x34, 0x62, 0xc4, 0xff}}},
    {{{    79,     28,     15}, 0, {     0,      0}, {0x34, 0x62, 0x3c, 0xff}}},
    {{{    85,      0,     27}, 0, {     0,      0}, {0x50, 0xf1, 0x60, 0xff}}},
    {{{    33,     28,     29}, 0, {     0,      0}, {0xff, 0x69, 0x47, 0xff}}},
    {{{   -29,     28,     12}, 0, {     0,      0}, {0xc8, 0x60, 0x3c, 0xff}}},
    {{{   -36,      0,     16}, 0, {     0,      0}, {0xb1, 0xd0, 0x56, 0xff}}},
    {{{    32,      0,     37}, 0, {     0,      0}, {0xfc, 0xfb, 0x7e, 0xff}}},
    {{{    33,     28,    -34}, 0, {     0,      0}, {0xff, 0x69, 0xb9, 0xff}}},
    {{{   -29,     28,    -16}, 0, {     0,      0}, {0xc8, 0x60, 0xc4, 0xff}}},
    {{{    68,    -30,     16}, 0, {     0,      0}, {0x26, 0x95, 0x36, 0xff}}},
    {{{    68,    -30,    -21}, 0, {     0,      0}, {0x26, 0x95, 0xca, 0xff}}},
    {{{    27,    -26,     27}, 0, {     0,      0}, {0xeb, 0x96, 0x41, 0xff}}},
};

// 0x08022F30
static const Vtx bobomb_seg8_vertex_08022F30[] = {
    {{{    27,    -26,    -31}, 0, {     0,      0}, {0xed, 0x90, 0xc8, 0xff}}},
    {{{    27,    -26,     27}, 0, {     0,      0}, {0xf5, 0xa2, 0x53, 0x00}}},
    {{{   -36,      0,     16}, 0, {     0,      0}, {0xa2, 0xf8, 0x54, 0x00}}},
    {{{   -36,      0,    -20}, 0, {     0,      0}, {0xa9, 0xdd, 0xac, 0xff}}},
};

// 0x08022F70
static const Vtx bobomb_seg8_vertex_08022F70[] = {
    {{{    32,      0,     41}, 0, {     0,      0}, {0xfc, 0xfb, 0x7e, 0x00}}},
    {{{   -36,      0,     20}, 0, {     0,      0}, {0xb1, 0xd0, 0x56, 0x00}}},
    {{{    27,    -26,     31}, 0, {     0,      0}, {0xeb, 0x96, 0x41, 0x00}}},
    {{{    84,      0,    -27}, 0, {     0,      0}, {0x50, 0xf1, 0xa0, 0xff}}},
    {{{    79,     28,    -15}, 0, {     0,      0}, {0x34, 0x62, 0xc4, 0xff}}},
    {{{    79,     28,     20}, 0, {     0,      0}, {0x34, 0x62, 0x3c, 0xff}}},
    {{{    84,      0,     32}, 0, {     0,      0}, {0x50, 0xf1, 0x60, 0xff}}},
    {{{    32,      0,    -37}, 0, {     0,      0}, {0xfc, 0xfb, 0x82, 0xff}}},
    {{{   -36,      0,    -16}, 0, {     0,      0}, {0xb1, 0xd0, 0xaa, 0xff}}},
    {{{   -28,     28,    -12}, 0, {     0,      0}, {0xc8, 0x60, 0xc4, 0xff}}},
    {{{    33,     28,    -29}, 0, {     0,      0}, {0xff, 0x69, 0xb9, 0xff}}},
    {{{   -28,     28,     16}, 0, {     0,      0}, {0xc8, 0x60, 0x3c, 0xff}}},
    {{{    33,     28,     33}, 0, {     0,      0}, {0xff, 0x69, 0x47, 0xff}}},
    {{{    68,    -29,     21}, 0, {     0,      0}, {0x26, 0x95, 0x36, 0xff}}},
    {{{    68,    -29,    -16}, 0, {     0,      0}, {0x26, 0x95, 0xca, 0xff}}},
    {{{    27,    -26,    -27}, 0, {     0,      0}, {0xeb, 0x96, 0xbf, 0xff}}},
};

// 0x08023070
static const Vtx bobomb_seg8_vertex_08023070[] = {
    {{{    27,    -26,    -27}, 0, {     0,      0}, {0xed, 0x90, 0xc8, 0xff}}},
    {{{    27,    -26,     31}, 0, {     0,      0}, {0xf5, 0xa2, 0x53, 0x00}}},
    {{{   -36,      0,     20}, 0, {     0,      0}, {0xa2, 0xf8, 0x54, 0x00}}},
    {{{   -36,      0,    -16}, 0, {     0,      0}, {0xa9, 0xdd, 0xac, 0xff}}},
};

// 0x080230B0
static const Vtx bobomb_seg8_vertex_080230B0[] = {
    {{{     0,   -100,     59}, 0, {     0,      0}, {0x00, 0xfe, 0x7f, 0x00}}},
    {{{   -53,    -99,     28}, 0, {     0,      0}, {0xc1, 0xfe, 0x6d, 0x00}}},
    {{{   -53,   -140,     27}, 0, {     0,      0}, {0xc1, 0xfe, 0x6d, 0x00}}},
    {{{     0,   -141,     58}, 0, {     0,      0}, {0x00, 0xfe, 0x7f, 0xff}}},
    {{{    53,    -99,     28}, 0, {     0,      0}, {0x3f, 0xfe, 0x6d, 0xff}}},
    {{{    53,   -140,     27}, 0, {     0,      0}, {0x3f, 0xfe, 0x6d, 0xff}}},
    {{{   -53,    -99,     28}, 0, {     0,      0}, {0x81, 0x00, 0x00, 0xff}}},
    {{{   -53,    -98,    -32}, 0, {     0,      0}, {0x81, 0x00, 0x00, 0xff}}},
    {{{   -53,   -139,    -33}, 0, {     0,      0}, {0x81, 0x00, 0x00, 0xff}}},
    {{{   -53,   -140,     27}, 0, {     0,      0}, {0x81, 0x00, 0x00, 0xff}}},
    {{{   -53,    -98,    -32}, 0, {     0,      0}, {0xc1, 0x02, 0x93, 0xff}}},
    {{{     0,    -97,    -63}, 0, {     0,      0}, {0xc1, 0x02, 0x93, 0xff}}},
    {{{     0,   -138,    -64}, 0, {     0,      0}, {0xc1, 0x02, 0x93, 0xff}}},
    {{{   -53,   -139,    -33}, 0, {     0,      0}, {0xc1, 0x02, 0x93, 0xff}}},
};

// 0x08023190
static const Vtx bobomb_seg8_vertex_08023190[] = {
    {{{    53,    -98,    -32}, 0, {     0,      0}, {0x7f, 0x00, 0x00, 0xff}}},
    {{{    53,    -99,     28}, 0, {     0,      0}, {0x7f, 0x00, 0x00, 0x00}}},
    {{{    53,   -140,     27}, 0, {     0,      0}, {0x7f, 0x00, 0x00, 0x00}}},
    {{{    53,   -139,    -33}, 0, {     0,      0}, {0x7f, 0x00, 0x00, 0xff}}},
    {{{     0,    -97,    -63}, 0, {     0,      0}, {0x3f, 0x02, 0x93, 0xff}}},
    {{{    53,    -98,    -32}, 0, {     0,      0}, {0x3f, 0x02, 0x93, 0xff}}},
    {{{    53,   -139,    -33}, 0, {     0,      0}, {0x3f, 0x02, 0x93, 0xff}}},
    {{{     0,   -138,    -64}, 0, {     0,      0}, {0x3f, 0x02, 0x93, 0xff}}},
    {{{     0,   -138,    -64}, 0, {     0,      0}, {0x00, 0x81, 0xfe, 0xff}}},
    {{{    53,   -139,    -33}, 0, {     0,      0}, {0x00, 0x81, 0xfe, 0xff}}},
    {{{    53,   -140,     27}, 0, {     0,      0}, {0x00, 0x81, 0xfe, 0xff}}},
    {{{     0,   -141,     58}, 0, {     0,      0}, {0x00, 0x81, 0xfe, 0xff}}},
    {{{   -53,   -140,     27}, 0, {     0,      0}, {0x00, 0x81, 0xfe, 0xff}}},
    {{{   -53,   -139,    -33}, 0, {     0,      0}, {0x00, 0x81, 0xfe, 0xff}}},
};

// 0x08023270 - 0x08023378
const Gfx bobomb_seg8_dl_08023270[] = {
    gsSPLight(&bobomb_seg8_lights_08022DE8.l, 1),
    gsSPLight(&bobomb_seg8_lights_08022DE8.a, 2),
    gsSPVertex(bobomb_seg8_vertex_08022E30, 16, 0),
    gsSP2Triangles( 0,  1,  2, 0x0,  3,  4,  5, 0x0),
    gsSP2Triangles( 3,  5,  6, 0x0,  7,  8,  9, 0x0),
    gsSP2Triangles( 7,  9, 10, 0x0, 11, 12,  8, 0x0),
    gsSP2Triangles(11,  8,  7, 0x0,  6, 13, 14, 0x0),
    gsSP2Triangles( 6, 14,  3, 0x0,  9,  8, 12, 0x0),
    gsSP2Triangles( 9, 12,  1, 0x0, 10,  9, 15, 0x0),
    gsSP2Triangles( 2,  1, 12, 0x0,  2, 12, 11, 0x0),
    gsSP2Triangles(10,  6,  5, 0x0, 10,  5,  7, 0x0),
    gsSP2Triangles( 0, 14, 13, 0x0,  0, 13, 15, 0x0),
    gsSP2Triangles(11,  4,  3, 0x0, 11,  3,  2, 0x0),
    gsSP2Triangles( 2,  3, 14, 0x0,  2, 14,  0, 0x0),
    gsSP2Triangles( 7,  5,  4, 0x0,  7,  4, 11, 0x0),
    gsSP2Triangles(15, 13,  6, 0x0, 15,  6, 10, 0x0),
    gsSPVertex(bobomb_seg8_vertex_08022F30, 4, 0),
    gsSP2Triangles( 0,  1,  2, 0x0,  0,  2,  3, 0x0),
    gsSPEndDisplayList(),
};

// 0x08023378 - 0x08023480
const Gfx bobomb_seg8_dl_08023378[] = {
    gsSPLight(&bobomb_seg8_lights_08022DE8.l, 1),
    gsSPLight(&bobomb_seg8_lights_08022DE8.a, 2),
    gsSPVertex(bobomb_seg8_vertex_08022F70, 16, 0),
    gsSP2Triangles( 0,  1,  2, 0x0,  3,  4,  5, 0x0),
    gsSP2Triangles( 3,  5,  6, 0x0,  7,  8,  9, 0x0),
    gsSP2Triangles( 7,  9, 10, 0x0, 10,  9, 11, 0x0),
    gsSP2Triangles(10, 11, 12, 0x0,  6, 13, 14, 0x0),
    gsSP2Triangles( 6, 14,  3, 0x0,  1, 11,  9, 0x0),
    gsSP2Triangles( 1,  9,  8, 0x0, 15,  8,  7, 0x0),
    gsSP2Triangles(12, 11,  1, 0x0, 12,  1,  0, 0x0),
    gsSP2Triangles(10,  4,  3, 0x0, 10,  3,  7, 0x0),
    gsSP2Triangles(15, 14, 13, 0x0, 15, 13,  2, 0x0),
    gsSP2Triangles( 0,  6,  5, 0x0,  0,  5, 12, 0x0),
    gsSP2Triangles( 2, 13,  6, 0x0,  2,  6,  0, 0x0),
    gsSP2Triangles(12,  5,  4, 0x0, 12,  4, 10, 0x0),
    gsSP2Triangles( 7,  3, 14, 0x0,  7, 14, 15, 0x0),
    gsSPVertex(bobomb_seg8_vertex_08023070, 4, 0),
    gsSP2Triangles( 0,  1,  2, 0x0,  0,  2,  3, 0x0),
    gsSPEndDisplayList(),
};

// 0x08023480 - 0x08023528
const Gfx bobomb_seg8_dl_08023480[] = {
    gsSPLight(&bobomb_seg8_lights_08022E00.l, 1),
    gsSPLight(&bobomb_seg8_lights_08022E00.a, 2),
    gsSPVertex(bobomb_seg8_vertex_080230B0, 14, 0),
    gsSP2Triangles( 0,  1,  2, 0x0,  0,  2,  3, 0x0),
    gsSP2Triangles( 4,  0,  3, 0x0,  4,  3,  5, 0x0),
    gsSP2Triangles( 6,  7,  8, 0x0,  6,  8,  9, 0x0),
    gsSP2Triangles(10, 11, 12, 0x0, 10, 12, 13, 0x0),
    gsSPVertex(bobomb_seg8_vertex_08023190, 14, 0),
    gsSP2Triangles( 0,  1,  2, 0x0,  0,  2,  3, 0x0),
    gsSP2Triangles( 4,  5,  6, 0x0,  4,  6,  7, 0x0),
    gsSP2Triangles( 8,  9, 10, 0x0,  8, 10, 11, 0x0),
    gsSP2Triangles( 8, 11, 12, 0x0,  8, 12, 13, 0x0),
    gsSPEndDisplayList(),
};

// --- Bob-omb 3D Body Sphere v2 (2.1x) ---
static const Vtx bobomb_body_sphere_vtx_0[] = {
    {{{  -33,  -106,   -54}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{    0,  -125,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -73,  -101,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -33,  -106,    56}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -91,   -64,    56}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{   35,  -106,    56}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -18,   -61,  -106}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -91,   -64,   -54}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -64,     0,  -106}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -112,     0,   -54}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{    0,     0,  -125}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{   92,   -64,   -54}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  120,   -38,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{   74,  -101,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{   92,   -64,    56}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  114,     0,    56}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
};

static const Vtx bobomb_body_sphere_vtx_1[] = {
    {{{   54,   -38,  -106}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{   92,   -64,   -54}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{   35,  -106,   -54}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -18,   -61,  -106}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -33,  -106,   -54}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{    0,     0,  -125}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{   74,   102,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{   92,    66,    56}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  120,    40,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  114,     0,    56}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -73,   102,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -33,   107,    56}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{    0,   127,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{   35,   107,    56}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{   92,    66,   -54}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
};

static const Vtx bobomb_body_sphere_vtx_2[] = {
    {{{   54,    40,  -106}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{   92,    66,   -54}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  114,     0,   -54}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{    0,     0,  -125}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{   54,   -38,  -106}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{   92,   -64,   -54}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{    0,   127,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -33,   107,    56}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{   35,   107,    56}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -33,   107,   -54}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -73,   102,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -91,    66,    56}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -18,    63,  -106}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{   35,   107,   -54}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
};

static const Vtx bobomb_body_sphere_vtx_3[] = {
    {{{ -119,   -38,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -112,     0,    56}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -119,    40,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -91,    66,    56}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -112,     0,   -54}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -91,   -64,    56}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -64,     0,  -106}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -91,    66,   -54}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{    0,     0,  -125}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -18,    63,  -106}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -33,   107,   -54}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{   35,  -106,    56}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{    0,  -125,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{   74,  -101,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{   35,  -106,   -54}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{   92,   -64,   -54}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
};

static const Vtx bobomb_body_sphere_vtx_4[] = {
    {{{    0,  -125,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -33,  -106,   -54}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{   35,  -106,   -54}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{   20,   -61,   107}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{   35,  -106,    56}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{   92,   -64,    56}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{   66,     0,   107}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  114,     0,    56}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{    0,     0,   127}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -91,   -64,    56}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -119,   -38,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -73,  -101,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -91,   -64,   -54}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -112,     0,   -54}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -53,   -38,   107}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -33,  -106,    56}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
};

static const Vtx bobomb_body_sphere_vtx_5[] = {
    {{{  -91,    66,    56}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -73,   102,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -119,    40,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -91,    66,   -54}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -112,     0,   -54}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -33,   107,   -54}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -53,    40,   107}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -112,     0,    56}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -53,   -38,   107}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -91,   -64,    56}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{    0,     0,   127}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{    0,   127,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{   35,   107,   -54}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{   74,   102,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{   35,   107,    56}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{   92,    66,   -54}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
};

static const Vtx bobomb_body_sphere_vtx_6[] = {
    {{{   20,    63,   107}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{   35,   107,    56}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -33,   107,    56}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{    0,     0,   127}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -53,    40,   107}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -91,    66,    56}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  120,   -38,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  114,     0,   -54}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  120,    40,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{   92,    66,   -54}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  114,     0,    56}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{   92,    66,    56}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{   92,   -64,   -54}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{   66,     0,   107}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
};

static const Lights1 bobomb_seg8_lights_body_red = gdSPDefLights1(
    0xdb, 0x3b, 0x6c,
    0x00, 0x00, 0x00, 0x28, 0x28, 0x28
);

static const Lights1 bobomb_seg8_lights_body_black = gdSPDefLights1(
    0x00, 0x00, 0x00,
    0x33, 0x33, 0x33, 0x28, 0x28, 0x28
);

const Gfx bobomb_body_sphere_dl[] = {
    gsDPPipeSync(),
    gsDPSetCombineMode(G_CC_SHADE, G_CC_SHADE),
    gsSPSetGeometryMode(G_LIGHTING),
    gsSPLight(&bobomb_seg8_lights_body_red.l, 1),
    gsSPLight(&bobomb_seg8_lights_body_red.a, 2),
    gsSPVertex(bobomb_body_sphere_vtx_0, 16, 0),
    gsSP2Triangles(  0,   1,   2, 0x0,   1,   3,   2, 0x0),
    gsSP2Triangles(  2,   3,   4, 0x0,   1,   5,   3, 0x0),
    gsSP2Triangles(  6,   0,   7, 0x0,   8,   7,   9, 0x0),
    gsSP2Triangles( 10,   6,   8, 0x0,   6,   7,   8, 0x0),
    gsSP2Triangles( 11,  12,  13, 0x0,  12,  14,  13, 0x0),
    gsSP2Triangles( 12,  15,  14, 0x0,  13,  14,   5, 0x0),
    gsSPVertex(bobomb_body_sphere_vtx_1, 15, 0),
    gsSP2Triangles(  0,   1,   2, 0x0,   0,   2,   3, 0x0),
    gsSP2Triangles(  3,   2,   4, 0x0,   5,   0,   3, 0x0),
    gsSP2Triangles(  6,   7,   8, 0x0,   8,   7,   9, 0x0),
    gsSP2Triangles( 10,  11,  12, 0x0,   6,  13,   7, 0x0),
    gsSP1Triangle( 14,   6,   8, 0x0),
    gsSPVertex(bobomb_body_sphere_vtx_2, 14, 0),
    gsSP2Triangles(  0,   1,   2, 0x0,   3,   0,   4, 0x0),
    gsSP2Triangles(  0,   2,   4, 0x0,   4,   2,   5, 0x0),
    gsSP2Triangles(  6,   7,   8, 0x0,   9,  10,   6, 0x0),
    gsSP2Triangles( 10,  11,   7, 0x0,  12,   9,  13, 0x0),
    gsSP2Triangles(  3,  12,   0, 0x0,  12,  13,   0, 0x0),
    gsSP1Triangle(  0,  13,   1, 0x0),
    gsSPVertex(bobomb_body_sphere_vtx_3, 16, 0),
    gsSP2Triangles(  0,   1,   2, 0x0,   2,   1,   3, 0x0),
    gsSP2Triangles(  4,   0,   2, 0x0,   0,   5,   1, 0x0),
    gsSP2Triangles(  6,   4,   7, 0x0,   8,   6,   9, 0x0),
    gsSP2Triangles(  6,   7,   9, 0x0,   9,   7,  10, 0x0),
    gsSP2Triangles( 11,  12,  13, 0x0,  12,  14,  13, 0x0),
    gsSP1Triangle( 13,  14,  15, 0x0),
    gsSPVertex(bobomb_body_sphere_vtx_4, 16, 0),
    gsSP2Triangles(  0,   1,   2, 0x0,   3,   4,   5, 0x0),
    gsSP2Triangles(  6,   5,   7, 0x0,   3,   5,   6, 0x0),
    gsSP2Triangles(  8,   3,   6, 0x0,   9,  10,  11, 0x0),
    gsSP2Triangles( 10,  12,  11, 0x0,  11,  12,   1, 0x0),
    gsSP2Triangles( 10,  13,  12, 0x0,  14,   9,  15, 0x0),
    gsSP2Triangles( 14,  15,   3, 0x0,   3,  15,   4, 0x0),
    gsSP1Triangle(  8,  14,   3, 0x0),
    gsSPVertex(bobomb_body_sphere_vtx_5, 16, 0),
    gsSP2Triangles(  0,   1,   2, 0x0,   1,   3,   2, 0x0),
    gsSP2Triangles(  2,   3,   4, 0x0,   1,   5,   3, 0x0),
    gsSP2Triangles(  6,   0,   7, 0x0,   8,   7,   9, 0x0),
    gsSP2Triangles( 10,   6,   8, 0x0,   6,   7,   8, 0x0),
    gsSP2Triangles( 11,  12,   5, 0x0,  13,  12,  11, 0x0),
    gsSP2Triangles( 14,  13,  11, 0x0,  13,  15,  12, 0x0),
    gsSPVertex(bobomb_body_sphere_vtx_6, 14, 0),
    gsSP2Triangles(  0,   1,   2, 0x0,   3,   0,   4, 0x0),
    gsSP2Triangles(  0,   2,   4, 0x0,   4,   2,   5, 0x0),
    gsSP2Triangles(  6,   7,   8, 0x0,   8,   7,   9, 0x0),
    gsSP2Triangles( 10,   6,   8, 0x0,   0,  11,   1, 0x0),
    gsSP2Triangles(  6,  12,   7, 0x0,  13,  10,  11, 0x0),
    gsSP2Triangles(  3,  13,   0, 0x0,  13,  11,   0, 0x0),
    gsSPEndDisplayList(),
};

const Gfx bobomb_body_sphere_dl_black[] = {
    gsDPPipeSync(),
    gsDPSetCombineMode(G_CC_SHADE, G_CC_SHADE),
    gsSPSetGeometryMode(G_LIGHTING),
    gsSPLight(&bobomb_seg8_lights_body_black.l, 1),
    gsSPLight(&bobomb_seg8_lights_body_black.a, 2),
    gsSPVertex(bobomb_body_sphere_vtx_0, 16, 0),
    gsSP2Triangles(  0,   1,   2, 0x0,   1,   3,   2, 0x0),
    gsSP2Triangles(  2,   3,   4, 0x0,   1,   5,   3, 0x0),
    gsSP2Triangles(  6,   0,   7, 0x0,   8,   7,   9, 0x0),
    gsSP2Triangles( 10,   6,   8, 0x0,   6,   7,   8, 0x0),
    gsSP2Triangles( 11,  12,  13, 0x0,  12,  14,  13, 0x0),
    gsSP2Triangles( 12,  15,  14, 0x0,  13,  14,   5, 0x0),
    gsSPVertex(bobomb_body_sphere_vtx_1, 15, 0),
    gsSP2Triangles(  0,   1,   2, 0x0,   0,   2,   3, 0x0),
    gsSP2Triangles(  3,   2,   4, 0x0,   5,   0,   3, 0x0),
    gsSP2Triangles(  6,   7,   8, 0x0,   8,   7,   9, 0x0),
    gsSP2Triangles( 10,  11,  12, 0x0,   6,  13,   7, 0x0),
    gsSP1Triangle( 14,   6,   8, 0x0),
    gsSPVertex(bobomb_body_sphere_vtx_2, 14, 0),
    gsSP2Triangles(  0,   1,   2, 0x0,   3,   0,   4, 0x0),
    gsSP2Triangles(  0,   2,   4, 0x0,   4,   2,   5, 0x0),
    gsSP2Triangles(  6,   7,   8, 0x0,   9,  10,   6, 0x0),
    gsSP2Triangles( 10,  11,   7, 0x0,  12,   9,  13, 0x0),
    gsSP2Triangles(  3,  12,   0, 0x0,  12,  13,   0, 0x0),
    gsSP1Triangle(  0,  13,   1, 0x0),
    gsSPVertex(bobomb_body_sphere_vtx_3, 16, 0),
    gsSP2Triangles(  0,   1,   2, 0x0,   2,   1,   3, 0x0),
    gsSP2Triangles(  4,   0,   2, 0x0,   0,   5,   1, 0x0),
    gsSP2Triangles(  6,   4,   7, 0x0,   8,   6,   9, 0x0),
    gsSP2Triangles(  6,   7,   9, 0x0,   9,   7,  10, 0x0),
    gsSP2Triangles( 11,  12,  13, 0x0,  12,  14,  13, 0x0),
    gsSP1Triangle( 13,  14,  15, 0x0),
    gsSPVertex(bobomb_body_sphere_vtx_4, 16, 0),
    gsSP2Triangles(  0,   1,   2, 0x0,   3,   4,   5, 0x0),
    gsSP2Triangles(  6,   5,   7, 0x0,   3,   5,   6, 0x0),
    gsSP2Triangles(  8,   3,   6, 0x0,   9,  10,  11, 0x0),
    gsSP2Triangles( 10,  12,  11, 0x0,  11,  12,   1, 0x0),
    gsSP2Triangles( 10,  13,  12, 0x0,  14,   9,  15, 0x0),
    gsSP2Triangles( 14,  15,   3, 0x0,   3,  15,   4, 0x0),
    gsSP1Triangle(  8,  14,   3, 0x0),
    gsSPVertex(bobomb_body_sphere_vtx_5, 16, 0),
    gsSP2Triangles(  0,   1,   2, 0x0,   1,   3,   2, 0x0),
    gsSP2Triangles(  2,   3,   4, 0x0,   1,   5,   3, 0x0),
    gsSP2Triangles(  6,   0,   7, 0x0,   8,   7,   9, 0x0),
    gsSP2Triangles( 10,   6,   8, 0x0,   6,   7,   8, 0x0),
    gsSP2Triangles( 11,  12,   5, 0x0,  13,  12,  11, 0x0),
    gsSP2Triangles( 14,  13,  11, 0x0,  13,  15,  12, 0x0),
    gsSPVertex(bobomb_body_sphere_vtx_6, 14, 0),
    gsSP2Triangles(  0,   1,   2, 0x0,   3,   0,   4, 0x0),
    gsSP2Triangles(  0,   2,   4, 0x0,   4,   2,   5, 0x0),
    gsSP2Triangles(  6,   7,   8, 0x0,   8,   7,   9, 0x0),
    gsSP2Triangles( 10,   6,   8, 0x0,   0,  11,   1, 0x0),
    gsSP2Triangles(  6,  12,   7, 0x0,  13,  10,  11, 0x0),
    gsSP2Triangles(  3,  13,   0, 0x0,  13,  11,   0, 0x0),
    gsSPEndDisplayList(),
};

// --- King Bob-omb 3D Body Sphere v2 (9.0x) ---
static const Vtx bobomb_body_sphere_king_vtx_0[] = {
    {{{ -140,  -448,  -231}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{    0,  -532,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -308,  -427,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -140,  -448,   238}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -385,  -273,   238}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  147,  -448,   238}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -77,  -259,  -448}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -385,  -273,  -231}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -273,     0,  -448}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -476,     0,  -231}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{    0,     0,  -532}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  392,  -273,  -231}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  511,  -161,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  315,  -427,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  392,  -273,   238}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  483,     0,   238}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
};

static const Vtx bobomb_body_sphere_king_vtx_1[] = {
    {{{  231,  -161,  -448}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  392,  -273,  -231}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  147,  -448,  -231}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -77,  -259,  -448}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -140,  -448,  -231}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{    0,     0,  -532}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  315,   434,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  392,   280,   238}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  511,   168,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  483,     0,   238}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -308,   434,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -140,   455,   238}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{    0,   539,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  147,   455,   238}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  392,   280,  -231}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
};

static const Vtx bobomb_body_sphere_king_vtx_2[] = {
    {{{  231,   168,  -448}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  392,   280,  -231}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  483,     0,  -231}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{    0,     0,  -532}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  231,  -161,  -448}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  392,  -273,  -231}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{    0,   539,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -140,   455,   238}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  147,   455,   238}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -140,   455,  -231}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -308,   434,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -385,   280,   238}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -77,   266,  -448}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  147,   455,  -231}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
};

static const Vtx bobomb_body_sphere_king_vtx_3[] = {
    {{{ -504,  -161,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -476,     0,   238}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -504,   168,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -385,   280,   238}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -476,     0,  -231}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -385,  -273,   238}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -273,     0,  -448}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -385,   280,  -231}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{    0,     0,  -532}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  -77,   266,  -448}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -140,   455,  -231}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  147,  -448,   238}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{    0,  -532,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  315,  -427,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  147,  -448,  -231}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  392,  -273,  -231}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
};

static const Vtx bobomb_body_sphere_king_vtx_4[] = {
    {{{    0,  -532,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -140,  -448,  -231}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  147,  -448,  -231}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{   84,  -259,   455}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  147,  -448,   238}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  392,  -273,   238}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  280,     0,   455}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  483,     0,   238}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{    0,     0,   539}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -385,  -273,   238}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -504,  -161,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -308,  -427,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -385,  -273,  -231}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -476,     0,  -231}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -224,  -161,   455}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -140,  -448,   238}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
};

static const Vtx bobomb_body_sphere_king_vtx_5[] = {
    {{{ -385,   280,   238}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -308,   434,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -504,   168,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -385,   280,  -231}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -476,     0,  -231}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -140,   455,  -231}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -224,   168,   455}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -476,     0,   238}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -224,  -161,   455}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -385,  -273,   238}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{    0,     0,   539}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{    0,   539,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  147,   455,  -231}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  315,   434,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  147,   455,   238}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  392,   280,  -231}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
};

static const Vtx bobomb_body_sphere_king_vtx_6[] = {
    {{{   84,   266,   455}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  147,   455,   238}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -140,   455,   238}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{    0,     0,   539}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -224,   168,   455}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{ -385,   280,   238}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  511,  -161,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  483,     0,  -231}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  511,   168,     0}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  392,   280,  -231}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  483,     0,   238}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  392,   280,   238}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  392,  -273,  -231}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
    {{{  280,     0,   455}, 0, {     0,      0}, {0x00, 0x00, 0x00, 0xff}}},
};

const Gfx bobomb_body_sphere_dl_king_black[] = {
    gsDPPipeSync(),
    gsDPSetCombineMode(G_CC_SHADE, G_CC_SHADE),
    gsSPSetGeometryMode(G_LIGHTING),
    gsSPLight(&bobomb_seg8_lights_body_black.l, 1),
    gsSPLight(&bobomb_seg8_lights_body_black.a, 2),
    gsSPVertex(bobomb_body_sphere_king_vtx_0, 16, 0),
    gsSP2Triangles(  0,   1,   2, 0x0,   1,   3,   2, 0x0),
    gsSP2Triangles(  2,   3,   4, 0x0,   1,   5,   3, 0x0),
    gsSP2Triangles(  6,   0,   7, 0x0,   8,   7,   9, 0x0),
    gsSP2Triangles( 10,   6,   8, 0x0,   6,   7,   8, 0x0),
    gsSP2Triangles( 11,  12,  13, 0x0,  12,  14,  13, 0x0),
    gsSP2Triangles( 12,  15,  14, 0x0,  13,  14,   5, 0x0),
    gsSPVertex(bobomb_body_sphere_king_vtx_1, 15, 0),
    gsSP2Triangles(  0,   1,   2, 0x0,   0,   2,   3, 0x0),
    gsSP2Triangles(  3,   2,   4, 0x0,   5,   0,   3, 0x0),
    gsSP2Triangles(  6,   7,   8, 0x0,   8,   7,   9, 0x0),
    gsSP2Triangles( 10,  11,  12, 0x0,   6,  13,   7, 0x0),
    gsSP1Triangle( 14,   6,   8, 0x0),
    gsSPVertex(bobomb_body_sphere_king_vtx_2, 14, 0),
    gsSP2Triangles(  0,   1,   2, 0x0,   3,   0,   4, 0x0),
    gsSP2Triangles(  0,   2,   4, 0x0,   4,   2,   5, 0x0),
    gsSP2Triangles(  6,   7,   8, 0x0,   9,  10,   6, 0x0),
    gsSP2Triangles( 10,  11,   7, 0x0,  12,   9,  13, 0x0),
    gsSP2Triangles(  3,  12,   0, 0x0,  12,  13,   0, 0x0),
    gsSP1Triangle(  0,  13,   1, 0x0),
    gsSPVertex(bobomb_body_sphere_king_vtx_3, 16, 0),
    gsSP2Triangles(  0,   1,   2, 0x0,   2,   1,   3, 0x0),
    gsSP2Triangles(  4,   0,   2, 0x0,   0,   5,   1, 0x0),
    gsSP2Triangles(  6,   4,   7, 0x0,   8,   6,   9, 0x0),
    gsSP2Triangles(  6,   7,   9, 0x0,   9,   7,  10, 0x0),
    gsSP2Triangles( 11,  12,  13, 0x0,  12,  14,  13, 0x0),
    gsSP1Triangle( 13,  14,  15, 0x0),
    gsSPVertex(bobomb_body_sphere_king_vtx_4, 16, 0),
    gsSP2Triangles(  0,   1,   2, 0x0,   3,   4,   5, 0x0),
    gsSP2Triangles(  6,   5,   7, 0x0,   3,   5,   6, 0x0),
    gsSP2Triangles(  8,   3,   6, 0x0,   9,  10,  11, 0x0),
    gsSP2Triangles( 10,  12,  11, 0x0,  11,  12,   1, 0x0),
    gsSP2Triangles( 10,  13,  12, 0x0,  14,   9,  15, 0x0),
    gsSP2Triangles( 14,  15,   3, 0x0,   3,  15,   4, 0x0),
    gsSP1Triangle(  8,  14,   3, 0x0),
    gsSPVertex(bobomb_body_sphere_king_vtx_5, 16, 0),
    gsSP2Triangles(  0,   1,   2, 0x0,   1,   3,   2, 0x0),
    gsSP2Triangles(  2,   3,   4, 0x0,   1,   5,   3, 0x0),
    gsSP2Triangles(  6,   0,   7, 0x0,   8,   7,   9, 0x0),
    gsSP2Triangles( 10,   6,   8, 0x0,   6,   7,   8, 0x0),
    gsSP2Triangles( 11,  12,   5, 0x0,  13,  12,  11, 0x0),
    gsSP2Triangles( 14,  13,  11, 0x0,  13,  15,  12, 0x0),
    gsSPVertex(bobomb_body_sphere_king_vtx_6, 14, 0),
    gsSP2Triangles(  0,   1,   2, 0x0,   3,   0,   4, 0x0),
    gsSP2Triangles(  0,   2,   4, 0x0,   4,   2,   5, 0x0),
    gsSP2Triangles(  6,   7,   8, 0x0,   8,   7,   9, 0x0),
    gsSP2Triangles( 10,   6,   8, 0x0,   0,  11,   1, 0x0),
    gsSP2Triangles(  6,  12,   7, 0x0,  13,  10,  11, 0x0),
    gsSP2Triangles(  3,  13,   0, 0x0,  13,  11,   0, 0x0),
    gsSPEndDisplayList(),
};

