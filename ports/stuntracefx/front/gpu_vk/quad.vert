#version 450
/*
 * quad.vert: a screen quad's corner (fullwindow.h FW_QUAD): six a quad,
 * two triangles over its rectangle of pixels, rows y to y + rows, columns
 * x0 to x1 + 1, so the rasterizer's pixel centres are its pixels.
 */
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"

layout(std430, binding = 21) readonly buffer Quads { uint qs[]; };
layout(push_constant) uniform Push { float fx, fy, cx, cy, tw, th, p0, p1; vec4 water; } pc;
layout(location = 0) flat out uint out_q;

void main()
{
    uint q = uint(gl_VertexIndex) / 6u, c = uint(gl_VertexIndex) % 6u, b = q * SPAN_WORDS;
    int y = int(qs[b]), x0 = int(qs[b + 1u]), x1 = int(qs[b + 2u]), rows = int(qs[b + 3u]);
    float px = (c == 1u || c == 2u || c == 4u) ? float(x1 + 1) : float(x0);
    float py = (c == 2u || c == 4u || c == 5u) ? float(y + rows) : float(y);
    gl_Position = vec4(2.0 * px / pc.tw - 1.0, 2.0 * py / pc.th - 1.0, 0.0, 1.0);
    out_q = q;
}
