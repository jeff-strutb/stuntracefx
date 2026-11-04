#version 450
/*
 * quad.frag: a screen quad's pixel (fullwindow.h FW_QUAD), its texel by the
 * quad's integer rule (fullwindow.c quad_col, quad_row, quad_colour): row
 * r's texel row (v + dv r) >> 32, or (v + dv r) / mask; column k's (u + du
 * k) >> 32, or divided by the word pq holds; each then times its 8.8 step
 * where one is set; the texel at base + row * 256 + column, or a 2-bit
 * bitmap's palette byte, through COLOR's and PLOT's rules (flat: the
 * colour); left out where the water is nearer on the pixel's ray.
 */
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"

layout(std430, binding = 4) readonly buffer Banks { uint banks[]; };
layout(std430, binding = 5) readonly buffer Slot { int slot[]; };
layout(std430, binding = 21) readonly buffer Quads { uint qs[]; };
layout(push_constant) uniform Push { float fx, fy, cx, cy, tw, th, p0, p1; vec4 water; } pc;
layout(location = 0) flat in uint in_q;
layout(location = 0) out uint out_colour;
layout(location = 1) out uint out_id;

int texel_colour(uint v, uint por, uint hi)
{
    uint c = (por & 4u) != 0u ? (hi | (v >> 4)) : (por & 8u) != 0u ? (hi | (v & 15u)) : v;
    c &= 255u;
    if ((por & 1u) == 0u) {
        if ((por & 8u) != 0u) {
            if ((c & 15u) == 0u)
                return -1;
        } else if (c == 0u) {
            return -1;
        }
    }
    return int(c);
}

void main()
{
    uint b = in_q * SPAN_WORDS;
    int x = int(gl_FragCoord.x), y = int(gl_FragCoord.y), c;
    int64_t xl = i64(qs[b + 4u], qs[b + 5u]), u = i64(qs[b + 6u], qs[b + 7u]), du = i64(qs[b + 8u], qs[b + 9u]);
    int64_t v = i64(qs[b + 10u], qs[b + 11u]), dv = i64(qs[b + 12u], qs[b + 13u]);
    uint base = qs[b + 14u] & 0xFFFFu, mask = qs[b + 14u] >> 16, w15 = qs[b + 15u];
    uint bank = w15 & 255u, por = (w15 >> 8) & 255u, hi = (w15 >> 16) & 255u, colour = w15 >> 24;
    uint flags = (qs[b + 16u] >> 16) & 255u;
    int64_t a = v + dv * int64_t(y - int(qs[b])), e = u + du * (int64_t(x) - xl);
    int64_t t = mask != 0u && (flags & 32u) == 0u ? a / int64_t(mask) : a >> 32;
    int64_t k = (flags & 8u) != 0u ? e / int64_t(qs[b + 18u]) : e >> 32;
    uint mx = qs[b + 19u], my = qs[b + 21u];
    if (mx != 0u)
        k = (k * int64_t(mx)) >> 8;
    if (my != 0u)
        t = (t * int64_t(my)) >> 8;
    if ((flags & 32u) != 0u) {
        /* a line: its Bresenham steps (fullwindow.c quad_on_line) */
        bool ymaj = (mask & 1u) != 0u;
        int64_t sx = (mask & 2u) != 0u ? -1l : 1l, sy = (mask & 4u) != 0u ? -1l : 1l;
        int64_t i = ymaj ? sy * (int64_t(y) - v) : sx * (int64_t(x) - xl), n, st;
        if (i < 0l || i > du)
            discard;
        n = i * dv - u;
        st = n <= 0l ? 0l : (n + du - 1l) / du;
        if (ymaj ? int64_t(x) != xl + sx * st : int64_t(y) != v + sy * st)
            discard;
        c = int(colour);
    } else if ((flags & 2u) != 0u) {
        c = int(colour);
    } else if ((flags & 4u) != 0u) {
        /* a 2-bit bitmap: the byte's pixel picks a palette byte (pad1[3..6]) */
        uint at = uint(slot[bank]) * 65536u + (uint(int64_t(base) + t * 32l + (k >> 2)) & 0xFFFFu);
        uint px = ((banks[at >> 2] >> ((at & 3u) * 8u)) >> (2u * uint(k & 3l))) & 3u;
        uint pal = qs[b + 17u];                     /* pad1[3..6]: the fourth word's bytes 0 to 3 */
        c = texel_colour((pal >> (8u * px)) & 255u, por, hi);
    } else {
        uint at = uint(slot[bank]) * 65536u + ((base + (uint(t & 255l) << 8) + uint(k & 255l)) & 0xFFFFu);
        c = texel_colour((banks[at >> 2] >> ((at & 3u) * 8u)) & 255u, por, hi);
    }
    if (c < 0)
        discard;
    float za = uintBitsToFloat(qs[b + 24u]), zx = uintBitsToFloat(qs[b + 25u]), zy = uintBitsToFloat(qs[b + 26u]);
    if (pc.water.w != 0.0 && (za != 0.0 || zx != 0.0 || zy != 0.0)) {
        precise float z = fma(zy, float(y), fma(zx, float(x), za));
        precise float w = fma(pc.water.z, float(y), fma(pc.water.y, float(x), pc.water.x));
        if (z > 0.0 && w > 0.0 && z < w)
            discard;
    }
    out_colour = uint(c);
    out_id = 0u;
}
