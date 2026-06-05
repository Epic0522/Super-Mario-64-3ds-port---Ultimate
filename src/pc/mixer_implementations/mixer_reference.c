#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <ultra64.h>

/*
 * Reference mixer.c software implementation, originally written for the PC port.
 * Enhanced RSPA emulation is not supported.
 */

#pragma GCC optimize ("unroll-loops")

#define ROUND_UP_32(v) (((v) + 31) & ~31)
#define ROUND_UP_16(v) (((v) + 15) & ~15)
#define ROUND_UP_8(v)  (((v) +  7) &  ~7)

static struct {
    uint16_t in;
    uint16_t out;
    uint16_t nbytes;

    int16_t vol[2];

    uint16_t dry_right;
    uint16_t wet_left;
    uint16_t wet_right;

    int16_t target[2];
    int32_t rate[2];

    int16_t vol_dry;
    int16_t vol_wet;

    ADPCM_STATE *adpcm_loop_state;

    int16_t adpcm_table[8][2][8];
    union {
        int16_t as_s16[2512 / sizeof(int16_t)];
        uint8_t as_u8[2512];
    } buf;
} rspa;

static int16_t resample_table[64][4] = {
    {0x0c39, 0x66ad, 0x0d46, 0xffdf}, {0x0b39, 0x6696, 0x0e5f, 0xffd8},
    {0x0a44, 0x6669, 0x0f83, 0xffd0}, {0x095a, 0x6626, 0x10b4, 0xffc8},
    {0x087d, 0x65cd, 0x11f0, 0xffbf}, {0x07ab, 0x655e, 0x1338, 0xffb6},
    {0x06e4, 0x64d9, 0x148c, 0xffac}, {0x0628, 0x643f, 0x15eb, 0xffa1},
    {0x0577, 0x638f, 0x1756, 0xff96}, {0x04d1, 0x62cb, 0x18cb, 0xff8a},
    {0x0435, 0x61f3, 0x1a4c, 0xff7e}, {0x03a4, 0x6106, 0x1bd7, 0xff71},
    {0x031c, 0x6007, 0x1d6c, 0xff64}, {0x029f, 0x5ef5, 0x1f0b, 0xff56},
    {0x022a, 0x5dd0, 0x20b3, 0xff48}, {0x01be, 0x5c9a, 0x2264, 0xff3a},
    {0x015b, 0x5b53, 0x241e, 0xff2c}, {0x0101, 0x59fc, 0x25e0, 0xff1e},
    {0x00ae, 0x5896, 0x27a9, 0xff10}, {0x0063, 0x5720, 0x297a, 0xff02},
    {0x001f, 0x559d, 0x2b50, 0xfef4}, {0xffe2, 0x540d, 0x2d2c, 0xfee8},
    {0xffac, 0x5270, 0x2f0d, 0xfedb}, {0xff7c, 0x50c7, 0x30f3, 0xfed0},
    {0xff53, 0x4f14, 0x32dc, 0xfec6}, {0xff2e, 0x4d57, 0x34c8, 0xfebd},
    {0xff0f, 0x4b91, 0x36b6, 0xfeb6}, {0xfef5, 0x49c2, 0x38a5, 0xfeb0},
    {0xfedf, 0x47ed, 0x3a95, 0xfeac}, {0xfece, 0x4611, 0x3c85, 0xfeab},
    {0xfec0, 0x4430, 0x3e74, 0xfeac}, {0xfeb6, 0x424a, 0x4060, 0xfeaf},
    {0xfeaf, 0x4060, 0x424a, 0xfeb6}, {0xfeac, 0x3e74, 0x4430, 0xfec0},
    {0xfeab, 0x3c85, 0x4611, 0xfece}, {0xfeac, 0x3a95, 0x47ed, 0xfedf},
    {0xfeb0, 0x38a5, 0x49c2, 0xfef5}, {0xfeb6, 0x36b6, 0x4b91, 0xff0f},
    {0xfebd, 0x34c8, 0x4d57, 0xff2e}, {0xfec6, 0x32dc, 0x4f14, 0xff53},
    {0xfed0, 0x30f3, 0x50c7, 0xff7c}, {0xfedb, 0x2f0d, 0x5270, 0xffac},
    {0xfee8, 0x2d2c, 0x540d, 0xffe2}, {0xfef4, 0x2b50, 0x559d, 0x001f},
    {0xff02, 0x297a, 0x5720, 0x0063}, {0xff10, 0x27a9, 0x5896, 0x00ae},
    {0xff1e, 0x25e0, 0x59fc, 0x0101}, {0xff2c, 0x241e, 0x5b53, 0x015b},
    {0xff3a, 0x2264, 0x5c9a, 0x01be}, {0xff48, 0x20b3, 0x5dd0, 0x022a},
    {0xff56, 0x1f0b, 0x5ef5, 0x029f}, {0xff64, 0x1d6c, 0x6007, 0x031c},
    {0xff71, 0x1bd7, 0x6106, 0x03a4}, {0xff7e, 0x1a4c, 0x61f3, 0x0435},
    {0xff8a, 0x18cb, 0x62cb, 0x04d1}, {0xff96, 0x1756, 0x638f, 0x0577},
    {0xffa1, 0x15eb, 0x643f, 0x0628}, {0xffac, 0x148c, 0x64d9, 0x06e4},
    {0xffb6, 0x1338, 0x655e, 0x07ab}, {0xffbf, 0x11f0, 0x65cd, 0x087d},
    {0xffc8, 0x10b4, 0x6626, 0x095a}, {0xffd0, 0x0f83, 0x6669, 0x0a44},
    {0xffd8, 0x0e5f, 0x6696, 0x0b39}, {0xffdf, 0x0d46, 0x66ad, 0x0c39}
};

static inline int16_t clamp16(int32_t v) {
    if (v < -0x8000) {
        return -0x8000;
    } else if (v > 0x7fff) {
        return 0x7fff;
    }
    return (int16_t)v;
}

static inline int32_t clamp32(int64_t v) {
    if (v < -0x7fffffff - 1) {
        return -0x7fffffff - 1;
    } else if (v > 0x7fffffff) {
        return 0x7fffffff;
    }
    return (int32_t)v;
}

void aClearBufferImpl(uint16_t addr, int nbytes) {
    nbytes = ROUND_UP_16(nbytes);
    memset(rspa.buf.as_u8 + addr, 0, nbytes);
}

void aLoadBufferImpl(const void *source_addr) {
    memcpy(rspa.buf.as_u8 + rspa.in, source_addr, ROUND_UP_8(rspa.nbytes));
}

void aSaveBufferImpl(int16_t *dest_addr) {
    memcpy(dest_addr, rspa.buf.as_s16 + rspa.out / sizeof(int16_t), ROUND_UP_8(rspa.nbytes));
}

void aLoadADPCMImpl(int num_entries_times_16, const int16_t *book_source_addr) {
    memcpy(rspa.adpcm_table, book_source_addr, num_entries_times_16);
}

void aSetBufferImpl(uint8_t flags, uint16_t in, uint16_t out, uint16_t nbytes) {
    if (flags & A_AUX) {
        rspa.dry_right = in;
        rspa.wet_left = out;
        rspa.wet_right = nbytes;
    } else {
        rspa.in = in;
        rspa.out = out;
        rspa.nbytes = nbytes;
    }
}

void aSetVolumeImpl(uint8_t flags, int16_t v, int16_t t, int16_t r) {
    if (flags & A_AUX) {
        rspa.vol_dry = v;
        rspa.vol_wet = r;
    } else if (flags & A_VOL) {
        if (flags & A_LEFT) {
            rspa.vol[0] = v;
        } else {
            rspa.vol[1] = v;
        }
    } else {
        if (flags & A_LEFT) {
            rspa.target[0] = v;
            rspa.rate[0] = (int32_t)((uint16_t)t << 16 | ((uint16_t)r));
        } else {
            rspa.target[1] = v;
            rspa.rate[1] = (int32_t)((uint16_t)t << 16 | ((uint16_t)r));
        }
    }
}

void aInterleaveImpl(uint16_t left, uint16_t right) {
    int count = ROUND_UP_16(rspa.nbytes) / sizeof(int16_t) / 8;
    int16_t *l = rspa.buf.as_s16 + left / sizeof(int16_t);
    int16_t *r = rspa.buf.as_s16 + right / sizeof(int16_t);
    int16_t *d = rspa.buf.as_s16 + rspa.out / sizeof(int16_t);
    while (count > 0) {
        int16_t l0 = *l++;
        int16_t l1 = *l++;
        int16_t l2 = *l++;
        int16_t l3 = *l++;
        int16_t l4 = *l++;
        int16_t l5 = *l++;
        int16_t l6 = *l++;
        int16_t l7 = *l++;
        int16_t r0 = *r++;
        int16_t r1 = *r++;
        int16_t r2 = *r++;
        int16_t r3 = *r++;
        int16_t r4 = *r++;
        int16_t r5 = *r++;
        int16_t r6 = *r++;
        int16_t r7 = *r++;
        *d++ = l0;
        *d++ = r0;
        *d++ = l1;
        *d++ = r1;
        *d++ = l2;
        *d++ = r2;
        *d++ = l3;
        *d++ = r3;
        *d++ = l4;
        *d++ = r4;
        *d++ = l5;
        *d++ = r5;
        *d++ = l6;
        *d++ = r6;
        *d++ = l7;
        *d++ = r7;
        --count;
    }
}

void aDMEMMoveImpl(uint16_t in_addr, uint16_t out_addr, int nbytes) {
    nbytes = ROUND_UP_16(nbytes);
    memmove(rspa.buf.as_u8 + out_addr, rspa.buf.as_u8 + in_addr, nbytes);
}

void aSetLoopImpl(ADPCM_STATE *adpcm_loop_state) {
    rspa.adpcm_loop_state = adpcm_loop_state;
}

// Decompresses ADPCM data
void aADPCMdecImpl(uint8_t flags, ADPCM_STATE state) {
    uint8_t *in = rspa.buf.as_u8 + rspa.in;
    int16_t *out = rspa.buf.as_s16 + rspa.out / sizeof(int16_t);
    int nbytes = ROUND_UP_32(rspa.nbytes);

    if (flags & A_INIT) {
        memset(out, 0, 16 * sizeof(int16_t));
    } else if (flags & A_LOOP) {
        memcpy(out, rspa.adpcm_loop_state, 16 * sizeof(int16_t));
    } else {
        memcpy(out, state, 16 * sizeof(int16_t));
    }
    out += 16;
    
    while (nbytes > 0) {
        int shift = *in >> 4; // should be in 0..12
        int table_index = *in++ & 0xf; // should be in 0..7
        int16_t (*tbl)[8] = rspa.adpcm_table[table_index];
        int i;
        
        for (i = 0; i < 2; i++) {
            int16_t ins[8];
            int16_t prev1 = out[-1];
            int16_t prev2 = out[-2];
            int j, k;
            for (j = 0; j < 4; j++) {
                ins[j * 2] = (((*in >> 4) << 28) >> 28) << shift;
                ins[j * 2 + 1] = (((*in++ & 0xf) << 28) >> 28) << shift;
            }
            for (j = 0; j < 8; j++) {
                int32_t acc = tbl[0][j] * prev2 + tbl[1][j] * prev1 + (ins[j] << 11);
                for (k = 0; k < j; k++) {
                    acc += tbl[1][((j - k) - 1)] * ins[k];
                }
                acc >>= 11;
                *out++ = clamp16(acc);
            }
        }

        nbytes -= 16 * sizeof(int16_t);
    }
    memcpy(state, out - 16, 16 * sizeof(int16_t));
}

void aResampleImpl(uint8_t flags, uint16_t pitch, RESAMPLE_STATE state) {
    int16_t tmp[16];
    int16_t *in_initial = rspa.buf.as_s16 + rspa.in / sizeof(int16_t);
    int16_t *in = in_initial;
    int16_t *out = rspa.buf.as_s16 + rspa.out / sizeof(int16_t);
    int nbytes = ROUND_UP_16(rspa.nbytes);
    uint32_t pitch_accumulator;
    int i;
    int16_t *tbl;
    int32_t sample;

    if (flags & A_INIT) {
        memset(tmp, 0, 5 * sizeof(int16_t));
    } else {
        memcpy(tmp, state, 16 * sizeof(int16_t));
    }
    if (flags & 2) {
        memcpy(in - 8, tmp + 8, 8 * sizeof(int16_t));
        in -= tmp[5] / sizeof(int16_t);
    }

    in -= 4;
    pitch_accumulator = (uint16_t)tmp[4];
    memcpy(in, tmp, 4 * sizeof(int16_t));

    do {
        for (i = 0; i < 8; i++) {
            tbl = resample_table[pitch_accumulator * 64 >> 16];
            sample = ((in[0] * tbl[0] + 0x4000) >> 15) +
                     ((in[1] * tbl[1] + 0x4000) >> 15) +
                     ((in[2] * tbl[2] + 0x4000) >> 15) +
                     ((in[3] * tbl[3] + 0x4000) >> 15);
            *out++ = clamp16(sample);

            pitch_accumulator += (pitch << 1);
            in += pitch_accumulator >> 16;
            pitch_accumulator %= 0x10000;
        }
        nbytes -= 8 * sizeof(int16_t);
    } while (nbytes > 0);

    state[4] = (int16_t)pitch_accumulator;
    memcpy(state, in, 4 * sizeof(int16_t));
    i = (in - in_initial + 4) & 7;
    in -= i;
    if (i != 0) {
        i = -8 - i;
    }
    state[5] = i;
    memcpy(state + 8, in, 8 * sizeof(int16_t));
}


void aEnvMixerImpl(uint8_t flags, ENVMIX_STATE state) {
    int16_t *in = rspa.buf.as_s16 + rspa.in / sizeof(int16_t);
    int16_t *dry[2] = {rspa.buf.as_s16 + rspa.out / sizeof(int16_t), rspa.buf.as_s16 + rspa.dry_right / sizeof(int16_t)};
    int16_t *wet[2] = {rspa.buf.as_s16 + rspa.wet_left / sizeof(int16_t), rspa.buf.as_s16 + rspa.wet_right / sizeof(int16_t)};
    int nbytes = ROUND_UP_16(rspa.nbytes);

    int16_t target[2];
    int32_t rate[2];
    int16_t vol_dry, vol_wet;

    int32_t step_diff[2];
    int32_t vols[2][8];

    int c, i;

    if (flags & A_INIT) {
        target[0] = rspa.target[0];
        target[1] = rspa.target[1];
        rate[0] = rspa.rate[0];
        rate[1] = rspa.rate[1];
        vol_dry = rspa.vol_dry;
        vol_wet = rspa.vol_wet;
        step_diff[0] = rspa.vol[0] * (rate[0] - 0x10000) / 8;
        step_diff[1] = rspa.vol[0] * (rate[1] - 0x10000) / 8;

        for (i = 0; i < 8; i++) {
            vols[0][i] = clamp32((int64_t)(rspa.vol[0] << 16) + step_diff[0] * (i + 1));
            vols[1][i] = clamp32((int64_t)(rspa.vol[1] << 16) + step_diff[1] * (i + 1));
        }
    } else {
        memcpy(vols[0], state, 32);
        memcpy(vols[1], state + 16, 32);
        target[0] = state[32];
        target[1] = state[35];
        rate[0] = (state[33] << 16) | (uint16_t)state[34];
        rate[1] = (state[36] << 16) | (uint16_t)state[37];
        vol_dry = state[38];
        vol_wet = state[39];
    }

    do {
        for (c = 0; c < 2; c++) {
            for (i = 0; i < 8; i++) {
                if ((rate[c] >> 16) > 0) {
                    // Increasing volume
                    if ((vols[c][i] >> 16) > target[c]) {
                        vols[c][i] = target[c] << 16;
                    }
                } else {
                    // Decreasing volume
                    if ((vols[c][i] >> 16) < target[c]) {
                        vols[c][i] = target[c] << 16;
                    }
                }
                dry[c][i] = clamp16((dry[c][i] * 0x7fff + in[i] * (((vols[c][i] >> 16) * vol_dry + 0x4000) >> 15) + 0x4000) >> 15);
                if (flags & A_AUX) {
                    wet[c][i] = clamp16((wet[c][i] * 0x7fff + in[i] * (((vols[c][i] >> 16) * vol_wet + 0x4000) >> 15) + 0x4000) >> 15);
                }
                vols[c][i] = clamp32((int64_t)vols[c][i] * rate[c] >> 16);
            }

            dry[c] += 8;
            if (flags & A_AUX) {
                wet[c] += 8;
            }
        }

        nbytes -= 16;
        in += 8;
    } while (nbytes > 0);

    memcpy(state, vols[0], 32);
    memcpy(state + 16, vols[1], 32);
    state[32] = target[0];
    state[35] = target[1];
    state[33] = (int16_t)(rate[0] >> 16);
    state[34] = (int16_t)rate[0];
    state[36] = (int16_t)(rate[1] >> 16);
    state[37] = (int16_t)rate[1];
    state[38] = vol_dry;
    state[39] = vol_wet;
}

void aMixImpl(int16_t gain, uint16_t in_addr, uint16_t out_addr) {
    int nbytes = ROUND_UP_32(rspa.nbytes);
    int16_t *in = rspa.buf.as_s16 + in_addr / sizeof(int16_t);
    int16_t *out = rspa.buf.as_s16 + out_addr / sizeof(int16_t);
    int i;
    int32_t sample;

    if (gain == -0x8000) {
        while (nbytes > 0) {
            for (i = 0; i < 16; i++) {
                sample = *out - *in++;
                *out++ = clamp16(sample);
            }

            nbytes -= 16 * sizeof(int16_t);
        }
    }

    while (nbytes > 0) {
        for (i = 0; i < 16; i++) {
            sample = ((*out * 0x7fff + *in++ * gain) + 0x4000) >> 15;
            *out++ = clamp16(sample);
        }

        nbytes -= 16 * sizeof(int16_t);
    }
}
