/* stuntracefx's cost profile: written by tools/nshvc-profile 1.0 (docs/cost-profile.md) */
#include "nshvc/nshvc.h"

static const uint32_t w0[4] = { 0, 0, 0, 0 };
static const uint32_t w1[4] = { 0, 0, 0, 0 };
static const uint32_t w2[4] = { 0, 0, 0, 0 };
static const uint32_t w3[4] = { 0, 0, 0, 0 };
static const uint32_t w4[4] = { 114265, 0, 30494, 2377440 };
static const uint32_t w5[4] = { 19113, 41137, 0, 0 };
static const uint32_t w6[4] = { 79845, 55422, 0, 0 };
static const uint32_t w7[4] = { 0, 238771, 0, 0 };
static const uint32_t w8[4] = { 120320, 120320, 120320, 120320 };
static const uint32_t w9[4] = { 51351, 51351, 51351, 51351 };
static const uint32_t w10[4] = { 15057, 0, 0, 0 };
static const uint32_t w11[4] = { 14314, 4153, 0, 0 };
static const uint32_t w12[4] = { 0, 77778, 0, 0 };
static const uint32_t w13[4] = { 18379, 184753, 0, 0 };
static const uint32_t w14[4] = { 104155, 104155, 104155, 104155 };
static const uint32_t w15[4] = { 0, 0, 0, 0 };
static const uint32_t w16[4] = { 0, 0, 0, 0 };
static const uint32_t w17[4] = { 118432, 0, 16600, 0 };
static const uint32_t w18[4] = { 104464, 0, 21450, 0 };
static const uint32_t w19[4] = { 0, 0, 20375, 0 };
static const uint32_t w20[4] = { 0, 0, 20250, 0 };
static const uint32_t w21[4] = { 105491, 0, 16003, 0 };
static const uint32_t w_default[4] = { 48259, 48259, 48259, 48259 };
static const nshvc_chip_program programs[22] = {
    { 0x0180BC, 2192, w0 },
    { 0x0181E3, 1365, w1 },
    { 0x0182CE, 609, w2 },
    { 0x018325, 1794, w3 },
    { 0x01AB13, 152073, w4 },
    { 0x01AEF8, 1570, w5 },
    { 0x01BBC7, 0, w6 },
    { 0x01CF42, 21257, w7 },
    { 0x01D1EB, 0, w8 },
    { 0x048800, 0, w9 },
    { 0x048B64, 1664, w10 },
    { 0x048C95, 1534, w11 },
    { 0x048CC6, 0, w12 },
    { 0x048FCA, 0, w13 },
    { 0x04BF51, 0, w14 },
    { 0x07EE50, 18344, w15 },
    { 0x07EE88, 84818, w16 },
    { 0x07F121, 779, w17 },
    { 0x07F15B, 580, w18 },
    { 0x07F6DF, 8853, w19 },
    { 0x07FA88, 73338, w20 },
    { 0x07FAF2, 0, w21 },
};

const nshvc_cost_profile stuntracefx_cost_profile = {
    NSHVC_COST_PROFILE_VERSION,
    { { 282, 198, 284, 448 }, { 161, 719, 168, 363 } },
    4, programs, 22, { 0, 0, w_default },
    { { 51, 39 }, { 14, 0 } },
};
