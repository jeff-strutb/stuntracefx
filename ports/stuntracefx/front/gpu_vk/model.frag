#version 450
/*
 * model.frag: a display-list face's pixel (scene.h), the fill's rules:
 * flat, its colour (0 not drawn); textured, the texel at ((v << 8 | u) &
 * mask) + base in its bank through COLOR's and PLOT's rules, u and v the
 * corners' perspective-correct with the texture's offsets (8.8). The
 * face's index + 1 beside it, for the check.
 */
layout(std430, binding = 4) readonly buffer Banks { uint banks[]; };
layout(std430, binding = 5) readonly buffer Slot { int slot[]; };
layout(std430, binding = 17) readonly buffer Faces { uint fcs[]; };     /* ScFace: 6 words */
layout(location = 0) in float in_u;
layout(location = 1) in float in_v;
layout(location = 2) flat in uint in_face;
layout(location = 3) in float in_z;
layout(push_constant) uniform Push { float fx, fy, cx, cy, tw, th, p0, p1; vec4 water; } pc;
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
    uint f = in_face * 6u, w2 = fcs[f + 2u], w3 = fcs[f + 3u], w4 = fcs[f + 4u], w5 = fcs[f + 5u];
    uint textured = w2 & 255u, colour = (w2 >> 8) & 255u, bank = (w2 >> 16) & 255u, por = w2 >> 24, hi = w3 & 255u;
    out_id = in_face + 1u;
    if (pc.water.w != 0.0 && fcs[f + 1u] != 2u) {
        /* the water nearer on the pixel's ray (fullwindow.c sunk): a line has no depth there, and is not cut */
        precise float x = gl_FragCoord.x - 0.5, y = gl_FragCoord.y - 0.5, iz = 1.0 / in_z;
        precise float w = fma(pc.water.z, y, fma(pc.water.y, x, pc.water.x));
        if (iz > 0.0 && w > 0.0 && iz < w)
            discard;
    }
    if (textured == 0u) {
        if (colour == 0u)
            discard;
        out_colour = colour;
        return;
    }
    float ou = float(int(w5 << 16) >> 16), ov = float(int(w5) >> 16);
    uint iu = uint(int(floor(in_u + ou / 256.0))) & 255u, iv = uint(int(floor(in_v + ov / 256.0))) & 255u;
    uint a = (((iv << 8) | iu) & (w4 >> 16)) + (w4 & 0xFFFFu);
    uint at = uint(slot[bank]) * 65536u + (a & 0xFFFFu);
    int c = texel_colour((banks[at >> 2] >> ((at & 3u) * 8u)) & 255u, por, hi);
    if (c < 0)
        discard;
    out_colour = uint(c);
}
