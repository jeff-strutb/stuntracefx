/*
 * heading.c: a velocity from a speed (a signed byte) and a heading, with a
 * pitch or without, each product a mul8 of the S-CPU's sine tables.
 */
#include "fixmath.h"

Vec3s heading_vector(int8_t speed, uint8_t heading)
{
    Vec3s v;

    v.x = mul8(speed, sin8(heading));
    v.y = 0;
    v.z = mul8(speed, cos8(heading));
    return v;
}

Vec3s heading_vector_pitched(int8_t speed, uint8_t heading, uint8_t pitch)
{
    Vec3s v;

    v.x = mul8(mul8(speed, sin8(heading)), cos8(pitch));
    v.y = mul8(speed, sin8(pitch));
    v.z = mul8(mul8(speed, cos8(heading)), cos8(pitch));
    return v;
}

Vec3s vec3_shl(Vec3s v, int k)
{
    v.x = (int16_t)(uint16_t)((uint16_t)v.x << k);
    v.y = (int16_t)(uint16_t)((uint16_t)v.y << k);
    v.z = (int16_t)(uint16_t)((uint16_t)v.z << k);
    return v;
}
