/*
 * common.glsl: the full window's records (fullwindow.h FwSpan, FwPoly,
 * FwEdge) read and written as the words the CPU lays them out in, and the
 * float steps of fullwindow.c's fill, each as the C writes it: no
 * contraction (precise), every multiply-add a fma, every divide rounded as
 * IEEE's (div_ieee).
 */
#extension GL_ARB_gpu_shader_int64 : require

#define SPAN_WORDS 26
#define POLY_WORDS 12
#define EDGE_WORDS 16

int64_t i64(uint lo, uint hi) { return int64_t(uint64_t(lo) | (uint64_t(hi) << 32)); }

/* a / b rounded as IEEE's divide: the quotient, then its remainder's
   correction (the GPU's divide may be off by an ulp or two) */
float div_ieee(float a, float b)
{
    precise float q = a / b;
    precise float r = fma(-q, b, a);
    precise float c = fma(r, 1.0 / b, q);
    return c;
}

struct Span {
    int y, x0, x1;
    int64_t xl, u, du, v, dv;
    uint base, mask, bank, por, hi, colour, textured;
    float pq, pdq, pa, pda, pb, pdb;
};
