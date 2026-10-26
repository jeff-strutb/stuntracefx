/*
 * car_parts.c: a car's loose parts (object/car_parts.c of the decomp). A
 * part is an object whose +$06 is its car. The part sits on the car: at the
 * car's position plus its offset, the vector of the part's offset bytes
 * +$48-$4A turned by the car's angles. When it is knocked loose it is
 * detached (it moves out to its offset steps at a time, then is released)
 * or it falls back onto the car (it moves and turns toward its place).
 *
 * Object fields are named by offset (OBJ, OBJ_W, objects.h); the data bank
 * is $7E throughout.
 */
#include "game.h"
#include "fixmath.h"
#include "objects.h"
#include "gsu_record.h"
#include "object_more.h"
#include "car_parts.h"

/* The part's three offset bytes, copied here for the turn: $1F58-$1F5A. */
#define offset_byte(i)  W8(0x1F58 + (i))

/* The Z word of the turn: the doubled offset's Z, then the part's Z target. */
#define turn_z          W16(0x05F2)

/* The sine and cosine of the last turn's angle. */
#define turn_sin        W8(0x0624)
#define turn_cos        W8(0x0625)

/* Two's complement negation of a byte. */
static uint8_t neg8(uint8_t a)
{
    return (uint8_t)(~a + 1);
}

/* Whether bit m of the object's +$25 is set. */
static bool flag25(uint16_t obj, uint8_t m)
{
    return (*OBJ(obj, 0x25) & m) != 0;
}

/* Gives the object its frame: the handler at $08:h. */
static void set_handler(uint16_t obj, uint16_t h)
{
    OBJ_W(obj, 0x1A) = h;
    *OBJ(obj, 0x1C) = 0x08;
}

/* The part's offset on its car. The car's angles turn the offset in three
   steps: by +$16 (x and y), then by +$14 (y and z), then by -$15 (x and z),
   each step a 2-D rotation of bytes. The results are doubled. The turn
   leaves the sine and cosine of its last angle in work RAM. */
void part_offset(uint16_t part, uint16_t car, uint16_t off[3])
{
    int16_t x1, y1, x2, z2, x3, z3;
    uint8_t last;

    offset_byte(0) = *OBJ(part, 0x48);
    offset_byte(1) = *OBJ(part, 0x49);
    offset_byte(2) = *OBJ(part, 0x4A);

    rotate8((int8_t)offset_byte(0), (int8_t)offset_byte(1), *OBJ(car, 0x16), &x1, &y1);
    rotate8((int8_t)(uint8_t)y1, (int8_t)offset_byte(2), *OBJ(car, 0x14), &x2, &z2);
    last = neg8(*OBJ(car, 0x15));
    rotate8((int8_t)(uint8_t)x1, (int8_t)(uint8_t)z2, last, &x3, &z3);
    turn_sin = (uint8_t)sin8(last);
    turn_cos = (uint8_t)cos8(last);

    off[0] = (uint16_t)x3 << 1;
    off[1] = (uint16_t)x2 << 1;
    off[2] = (uint16_t)z3 << 1;
    turn_z = off[2];
}

/* The part takes its car's position and +$14-$16, and the offset is
   returned in off for the caller to add. */
static void part_on_car(uint16_t part, uint16_t car, uint16_t off[3])
{
    part_offset(part, car, off);
    object_copy_position(car, part);
    object_copy_14(car, part);
}

/* The detaching part's frame. Its count +$4B goes up each frame. At 8 the
   part is released. At 7 the car's bit 7 of +$25 is cleared. Then the part
   sits on its car plus its offset, taken k times: k is the count when it is
   below 4, and 9 minus the count from 4 up (5 down to 1). */
static void detach_frame(uint16_t part)
{
    uint16_t car = OBJ_W(part, 0x06), off[3];
    uint8_t count, k;
    unsigned i;

    count = *OBJ(part, 0x4B);
    if (count == 0x08) {
        object_release_long(part);
        return;
    }
    if (count == 0x07)
        *OBJ(car, 0x25) &= 0x7F;

    count = (uint8_t)(count + 1);
    *OBJ(part, 0x4B) = count;
    part_on_car(part, car, off);

    k = count >= 4 ? (uint8_t)(9 - count) : count;
    do {
        for (i = 0; i < 3; i++)
            OBJ_W(part, 0x0C + 2 * i) = (uint16_t)(OBJ_W(part, 0x0C + 2 * i) + off[i]);
        k--;
    } while (k);
    object_rebucket(part);
}

/* A part comes off its car: the car's bit 7 of +$25 is set, the part takes
   the car's kind (+$49), its count +$4B is 0 and its frame is
   part_detach_step. */
void part_detach(uint16_t part)
{
    uint16_t car = OBJ_W(part, 0x06);

    *OBJ(car, 0x25) |= 0x80;
    object_set_5d_by_kind(part, *OBJ(car, 0x49));
    *OBJ(part, 0x4B) = 0x00;
    set_handler(part, 0xAC27);
    detach_frame(part);
}

/* The detaching part's frame (see part_detach). */
void part_detach_step(uint16_t part)
{
    detach_frame(part);
}

/* The returning part's frame. Its frame count is 3. It takes its car's
   record velocity, and moves a quarter of the way toward its place on the
   car (position and +$14-$16 alike, the approach at least 4 a frame). Then
   it is moved by its velocity and rebucketed. On its last count (+$4B = 1)
   a sound plays: $4D if the part is marked (+$25 bit 1), else $4E. While
   the count lasts it counts down. Then, unless the car's +$25 bit 2 is set,
   its frame becomes the release frame ($08:F927), and with its own +$25
   bit 0 the car's bit 2 is cleared. */
static void return_frame(uint16_t part)
{
    uint16_t car = OBJ_W(part, 0x06), off[3], target[3];
    unsigned i;
    bool moved;

    *OBJ(part, 0x36) = 0x03;
    velocity_from_record_of(part, car);
    part_offset(part, car, off);

    for (i = 0; i < 3; i++)
        target[i] = (uint16_t)(OBJ_W(car, 0x0C + 2 * i) + off[i]);
    turn_z = target[2];

    for (i = 0; i < 3; i++)
        OBJ_W(part, 0x0C + 2 * i) = (uint16_t)approach16((int16_t)OBJ_W(part, 0x0C + 2 * i),
                                                          (int16_t)target[i], 4, 2, &moved);
    for (i = 0; i < 3; i++)
        *OBJ(part, 0x14 + i) = approach8(*OBJ(part, 0x14 + i), *OBJ(car, 0x14 + i), 4, 2, &moved);

    object_apply_velocity(part);
    if (*OBJ(part, 0x4B) == 0x01)
        sound_queue(flag25(part, 0x02) ? 0x4D : 0x4E);
    object_rebucket(part);
    *OBJ(part, 0x36) = 0x03;

    if (*OBJ(part, 0x4B)) {
        (*OBJ(part, 0x4B))--;
        return;
    }
    car = OBJ_W(part, 0x06);
    if (!flag25(car, 0x04))
        set_handler(part, 0xF927);
    if (flag25(part, 0x01))
        *OBJ(car, 0x25) &= 0xFB;
}

/* The returning part's frame (see part_return). */
void part_return_step(uint16_t part)
{
    return_frame(part);
}

/* The flying part turns back to its car: its frame becomes
   part_return_step, its count +$4B is 10, and a sound plays first: $4C
   when the part is marked (+$25 bit 1), else $4B. Then that frame runs. */
void part_return(uint16_t part)
{
    set_handler(part, 0xAE0F);
    *OBJ(part, 0x4B) = 0x0A;
    sound_queue(flag25(part, 0x02) ? 0x4C : 0x4B);
    return_frame(part);
}
