#include "minimap_levels.h"

#define MINIMAP_LEVEL(areas) { areas, sizeof(areas) / sizeof((areas)[0]) }

const struct MiniMapInfo level_unknown[] = {
    {unknown_t3x, unknown_t3x_size, 0x000000, false},
};
const struct MiniMapInfo level_bbh[] = {
    {bbh_t3x, bbh_t3x_size, 0x252725, false},
};
const struct MiniMapInfo level_ccm[] = {
    {ccm_1_t3x, ccm_1_t3x_size, 0x848c94, false},
    {ccm_2_t3x, ccm_2_t3x_size, 0x181818, false},
};
const struct MiniMapInfo level_castle[] = {
    {castle_1_t3x, castle_1_t3x_size, 0x000000, false},
    {castle_2_t3x, castle_2_t3x_size, 0x000000, false},
    {castle_3_t3x, castle_3_t3x_size, 0x000000, false},
};
const struct MiniMapInfo level_hmc[] = {
    {hmc_t3x, hmc_t3x_size, 0xA06A85, false},
};
const struct MiniMapInfo level_ssl[] = {
    {ssl_1_t3x, ssl_1_t3x_size, 0xEEBF84, false},
    {ssl_2_t3x, ssl_2_t3x_size, 0x000000, false},
    {ssl_3_t3x, ssl_3_t3x_size, 0x000000, false},
};
const struct MiniMapInfo level_bob[] = {
    {bob_t3x, bob_t3x_size, 0x0cae1f, false},
};
const struct MiniMapInfo level_sl[] = {
    {sl_1_t3x, sl_1_t3x_size, 0xD4EEFD, false},
    {sl_2_t3x, sl_2_t3x_size, 0x000000, false},
};
const struct MiniMapInfo level_wdw[] = {
    {wdw_1_t3x, wdw_1_t3x_size, 0x545258, false},
    {wdw_2_t3x, wdw_2_t3x_size, 0x545258, false},
};
const struct MiniMapInfo level_jrb[] = {
    {jrb_1_t3x, jrb_1_t3x_size, 0x94cebd, false},
    {jrb_2_t3x, jrb_2_t3x_size, 0x000000, false},
};
const struct MiniMapInfo level_thi[] = {
    {thi_1_t3x, thi_1_t3x_size, 0x97A4B7, false},
    {thi_2_t3x, thi_2_t3x_size, 0x97A4B7, false},
    {thi_3_t3x, thi_3_t3x_size, 0x000000, false},
};
const struct MiniMapInfo level_ttc[] = {
    {ttc_t3x, ttc_t3x_size, 0x000000, false},
};
const struct MiniMapInfo level_rr[] = {
    {rr_t3x, rr_t3x_size, 0x3152ad, false},
};
const struct MiniMapInfo level_castle_grounds[] = {
    {castle_grounds_t3x, castle_grounds_t3x_size, 0x5CE728, false},
    /* drained castle grounds is not an area but a savegame flag */
    {castle_grounds_drained_t3x, castle_grounds_drained_t3x_size, 0x5CE728, false},
};
const struct MiniMapInfo level_bitdw[] = {
    {bitdw_t3x, bitdw_t3x_size, 0x000000, false},
};
const struct MiniMapInfo level_vcutm[] = {
    {vcutm_t3x, vcutm_t3x_size, 0x000000, false},
};
const struct MiniMapInfo level_bitfs[] = {
    {bitfs_t3x, bitfs_t3x_size, 0xD82A12, false},
};
const struct MiniMapInfo level_sa[] = {
    {sa_t3x, sa_t3x_size, 0x000000, false},
};
const struct MiniMapInfo level_bits[] = {
    {bits_t3x, bits_t3x_size, 0x000000, false},
};
const struct MiniMapInfo level_lll[] = {
    {lll_1_t3x, lll_1_t3x_size, 0xD82A12, false},
    {lll_2_t3x, lll_2_t3x_size, 0x000000, false},
};
const struct MiniMapInfo level_ddd[] = {
    {ddd_1_t3x, ddd_1_t3x_size, 0x94cebd, false},
    {ddd_2_t3x, ddd_2_t3x_size, 0x94cebd, false},
};
const struct MiniMapInfo level_wf[] = {
    {wf_t3x, wf_t3x_size, 0x8cbdf7, false},
};
const struct MiniMapInfo level_castle_courtyard[] = {
    {castle_courtyard_t3x, castle_courtyard_t3x_size, 0x4FC522, false},
};
const struct MiniMapInfo level_pss[] = {
    {pss_t3x, pss_t3x_size, 0x000000, false},
};
const struct MiniMapInfo level_cotmc[] = {
    {cotmc_t3x, cotmc_t3x_size, 0x000000, false},
};
const struct MiniMapInfo level_totwc[] = {
    {totwc_t3x, totwc_t3x_size, 0x000000, false},
};
const struct MiniMapInfo level_bowser_1[] = {
    {bowser_1_t3x, bowser_1_t3x_size, 0x00295a, false},
};
const struct MiniMapInfo level_wmotr[] = {
    {wmotr_t3x, wmotr_t3x_size, 0x579FF7, false},
};
const struct MiniMapInfo level_bowser_2[] = {
    {bowser_2_t3x, bowser_2_t3x_size, 0xD82A12, false},
};
const struct MiniMapInfo level_bowser_3[] = {
    {bowser_3_t3x, bowser_3_t3x_size, 0x000000, false},
};
const struct MiniMapInfo level_ttm[] = {
    {ttm_1_t3x, ttm_1_t3x_size, 0xACD5FB, false},
    {ttm_2_t3x, ttm_2_t3x_size, 0x000000, false},
    {ttm_3_t3x, ttm_3_t3x_size, 0x000000, false},
    {ttm_4_t3x, ttm_4_t3x_size, 0x000000, false},
};

const struct MiniMapLevel level_info[] = {
    MINIMAP_LEVEL(level_unknown),
    MINIMAP_LEVEL(level_bbh),
    MINIMAP_LEVEL(level_ccm),
    MINIMAP_LEVEL(level_castle),
    MINIMAP_LEVEL(level_hmc),
    MINIMAP_LEVEL(level_ssl),
    MINIMAP_LEVEL(level_bob),
    MINIMAP_LEVEL(level_sl),
    MINIMAP_LEVEL(level_wdw),
    MINIMAP_LEVEL(level_jrb),
    MINIMAP_LEVEL(level_thi),
    MINIMAP_LEVEL(level_ttc),
    MINIMAP_LEVEL(level_rr),
    MINIMAP_LEVEL(level_castle_grounds),
    MINIMAP_LEVEL(level_bitdw),
    MINIMAP_LEVEL(level_vcutm),
    MINIMAP_LEVEL(level_bitfs),
    MINIMAP_LEVEL(level_sa),
    MINIMAP_LEVEL(level_bits),
    MINIMAP_LEVEL(level_lll),
    MINIMAP_LEVEL(level_ddd),
    MINIMAP_LEVEL(level_wf),
    MINIMAP_LEVEL(level_castle_courtyard),
    MINIMAP_LEVEL(level_pss),
    MINIMAP_LEVEL(level_cotmc),
    MINIMAP_LEVEL(level_totwc),
    MINIMAP_LEVEL(level_bowser_1),
    MINIMAP_LEVEL(level_wmotr),
    MINIMAP_LEVEL(level_bowser_2),
    MINIMAP_LEVEL(level_bowser_3),
    MINIMAP_LEVEL(level_ttm),
};
