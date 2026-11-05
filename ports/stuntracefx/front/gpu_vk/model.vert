#version 450
/*
 * model.vert: a display-list face's corner (scene.h): its point turned by
 * the chip's integer steps (sc_points, the Metal shader's), moved to the
 * slot's place, a far point's depth held at $43FE, then projected (the
 * full window's focal lengths and centre) with the near plane at z = 1.
 */
layout(std430, binding = 14) readonly buffer Verts { uint vs[]; };      /* GpuVert: point, face, u, v */
layout(std430, binding = 15) readonly buffer Points { uint pts[]; };    /* ScPoint: bytes, mirrored; slot */
layout(std430, binding = 16) readonly buffer Slots { uint sls[]; };     /* ScSlot: 16 shorts */
layout(std430, binding = 19) readonly buffer FSlots { float fsl[]; };    /* ScFSlot: m[9], pos[3], zs, 3 unused */
layout(std430, binding = 20) readonly buffer FPoints { uint fpts[]; };   /* ScFPoint: p[3] (float bits), fslot */
layout(push_constant) uniform Push { float fx, fy, cx, cy, tw, th, p0, p1; vec4 water; } pc;
layout(location = 0) out float out_u;
layout(location = 1) out float out_v;
layout(location = 2) flat out uint out_face;
layout(location = 3) out float out_z;      /* the true depth (a far object's halvings undone), perspective-correct: exact on a face */

int s8(uint v) { return int(v << 24) >> 24; }
int s16(uint v) { return int(v << 16) >> 16; }
uint mult8(uint a, uint b) { return uint(s8(a) * s8(b)) & 0xFFFFu; }
uint fmult_rol(uint a, uint b) { return uint((s16(a) * s16(b)) >> 15) & 0xFFFFu; }
uint packed_out(uint s, uint scale) { return mult8(((s + s) & 0xFFFFu) >> 8, scale); }
uint slot_short(uint sl, uint i)
{
    uint w = sls[sl * 8u + i / 2u];
    return (i & 1u) != 0u ? w >> 16 : w & 0xFFFFu;
}

int point_coord(uint sl, uint word, int j)
{
    uint b0 = word & 255u, b1 = (word >> 8) & 255u, b2 = (word >> 16) & 255u, mirrored = word >> 24;
    uint scale = slot_short(sl, 12u), shift = slot_short(sl, 13u), xt, rest, r;
    uint m0 = slot_short(sl, uint(j)), m3 = slot_short(sl, uint(3 + j)), m6 = slot_short(sl, uint(6 + j));
    if (shift < 3u) {
        xt = mult8(m0 >> 8, b0);
        rest = (mult8(m3 >> 8, b1) + mult8(m6 >> 8, b2)) & 0xFFFFu;
        r = packed_out((mirrored != 0u ? rest - xt : rest + xt) & 0xFFFFu, scale);
    } else {
        xt = fmult_rol(mult8(b0, scale), m0);
        rest = (fmult_rol(mult8(b1, scale), m3) + fmult_rol(mult8(b2, scale), m6)) & 0xFFFFu;
        r = (mirrored != 0u ? rest - xt : rest + xt) & 0xFFFFu;
    }
    return s16((r + slot_short(sl, uint(9 + j))) & 0xFFFFu);
}

void main()
{
    uint at = uint(gl_VertexIndex) * 4u, p = vs[at];
    float x, y, z, zs;
    if ((p & 0x80000000u) != 0u) {      /* a solid's point (solids.c): its slot's float matrix and place */
        uint q = (p & 0x7FFFFFFFu) * 4u, f = fpts[q + 3u] * 16u;
        float px = uintBitsToFloat(fpts[q]), py = uintBitsToFloat(fpts[q + 1u]), pz = uintBitsToFloat(fpts[q + 2u]);
        x = fsl[f + 9u] + px * fsl[f + 0u] + py * fsl[f + 3u] + pz * fsl[f + 6u];
        y = fsl[f + 10u] + px * fsl[f + 1u] + py * fsl[f + 4u] + pz * fsl[f + 7u];
        z = fsl[f + 11u] + px * fsl[f + 2u] + py * fsl[f + 5u] + pz * fsl[f + 8u];
        zs = fsl[f + 12u];
    } else {
        uint word = pts[p * 2u], sl = pts[p * 2u + 1u];
        x = float(point_coord(sl, word, 0));
        y = float(point_coord(sl, word, 1));
        int zi = point_coord(sl, word, 2);
        if (zi >= 0x100 && zi > 0x43FE)
            zi = 0x43FE;
        z = float(zi);
        zs = float(1u << slot_short(sl, 15u));
    }
    gl_Position = vec4(2.0 * (pc.cx * z + pc.fx * x) / pc.tw - z, 2.0 * (pc.cy * z + pc.fy * y) / pc.th - z, z - 1.0, z);
    out_u = uintBitsToFloat(vs[at + 2u]);
    out_v = uintBitsToFloat(vs[at + 3u]);
    out_face = vs[at + 1u];
    out_z = z * zs;
}
