/* stuntracefx's cost profile: written by tools/nshvc-profile 1.0 (docs/cost-profile.md) */
#include "nshvc/nshvc.h"

static const uint32_t w0[4] = { 2244101, 0, 0, 0 };
static const uint32_t w1[4] = { 684831, 0, 0, 0 };
static const uint32_t w2[4] = { 103935, 0, 0, 0 };
static const uint32_t w3[4] = { 1220055, 0, 0, 0 };
static const uint32_t w4[4] = { 110433, 0, 31991, 4228688 };
static const uint32_t w5[4] = { 19232, 39903, 0, 0 };
static const uint32_t w6[4] = { 81920, 54336, 0, 0 };
static const uint32_t w7[4] = { 0, 97436, 0, 0 };
static const uint32_t w8[4] = { 120320, 120320, 120320, 120320 };
static const uint32_t w9[4] = { 51351, 51351, 51351, 51351 };
static const uint32_t w10[4] = { 15075, 0, 0, 0 };
static const uint32_t w11[4] = { 14347, 6360, 0, 0 };
static const uint32_t w12[4] = { 0, 77770, 0, 0 };
static const uint32_t w13[4] = { 15470, 187647, 0, 0 };
static const uint32_t w14[4] = { 104155, 104155, 104155, 104155 };
static const uint32_t w15[4] = { 0, 24459, 0, 0 };
static const uint32_t w16[4] = { 84776, 0, 0, 0 };
static const uint32_t w17[4] = { 140119, 0, 0, 26723 };
static const uint32_t w18[4] = { 106778, 64583, 23247, 0 };
static const uint32_t w19[4] = { 69997, 0, 20378, 0 };
static const uint32_t w20[4] = { 103799, 0, 20248, 0 };
static const uint32_t w21[4] = { 105831, 1103199, 13823, 0 };
static const uint32_t w_default[4] = { 48259, 48259, 48259, 48259 };
static const nshvc_chip_program programs[22] = {
    { 0x0180BC, 0, w0 },
    { 0x0181E3, 0, w1 },
    { 0x0182CE, 0, w2 },
    { 0x018325, 0, w3 },
    { 0x01AB13, 150596, w4 },
    { 0x01AEF8, 1610, w5 },
    { 0x01BBC7, 0, w6 },
    { 0x01CF42, 28617, w7 },
    { 0x01D1EB, 0, w8 },
    { 0x048800, 0, w9 },
    { 0x048B64, 1664, w10 },
    { 0x048C95, 1494, w11 },
    { 0x048CC6, 0, w12 },
    { 0x048FCA, 91, w13 },
    { 0x04BF51, 0, w14 },
    { 0x07EE50, 0, w15 },
    { 0x07EE88, 0, w16 },
    { 0x07F121, 0, w17 },
    { 0x07F15B, 0, w18 },
    { 0x07F6DF, 0, w19 },
    { 0x07FA88, 0, w20 },
    { 0x07FAF2, 0, w21 },
};

const nshvc_cost_profile stuntracefx_cost_profile = {
    NSHVC_COST_PROFILE_VERSION,
    { { 217, 681, 414, 678 }, { 160, 1069, 371, 348 } },
    4, programs, 22, { 0, 0, w_default },
};
