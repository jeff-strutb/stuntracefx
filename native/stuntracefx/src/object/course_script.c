/*
 * course_script.c: the course script: the byte-coded list of objects and
 * settings a course starts with.
 */
#include <string.h>

#include "data/cart.h"
#include "fx/fx.h"
#include "object/object.h"
#include "system/state.h"

/* A bare object for the script's opcodes $16 and $18: kept in `slot`,
   running from the entry's handler, linked to camera `link`. */
static void bare_object(uint8_t bank, uint16_t at, uint16_t *slot, uint16_t link)
{
    ObjRef r = ObjectAlloc();
    Object *o;

    if (!r) {
        CrashGreen();
        return;
    }
    o = obj_at(r);
    ObjectInit(o);
    *slot = r;
    o->timer = 3;
    o->link = link;
    o->handler = cart16(bank, (uint16_t)(0x8001 + at));
    o->handler_bank = cart8(bank, (uint16_t)(0x8003 + at));
}

/*
 * @routine 08:801C CourseScriptRun
 * WHAT IT DOES: Runs the course script at bank:$8000 + at. Each entry's
 * opcode is the offset of its handler in the table at $08:8000. Spawns
 * objects, sets the course's flags and placing tables, empties the pools;
 * the end entry leaves the address after it for the course's track table.
 */
void CourseScriptRun(uint8_t bank, uint16_t at)
{
    uint16_t x = at;

    for (;;) {
        uint16_t op = cart8(bank, (uint16_t)(0x8000 + x)), y = x, a;
        uint8_t v;
        Object *o;

        switch (cart16(0x08, (uint16_t)(0x8000 + op))) {
        case 0x8049:                            /* $02 */
            ObjectSpawnFromScript(y, bank);
            x = (uint16_t)(y + 0x16);
            break;
        case 0x8055:                            /* $0C */
            o = ObjectSpawnFromScript(y, bank);
            o->flags09 |= 0x01;
            x = (uint16_t)(y + 0x16);
            break;
        case 0x806F:                            /* $10 */
            o = ObjectSpawnFromScript(y, bank);
            o->link27 = cart16(bank, (uint16_t)(0x8016 + y));
            o->word29 = ++g.spawn_counter;
            x = (uint16_t)(y + 0x18);
            break;
        case 0x808F:                            /* $0A */
            o = ObjectSpawnFromScript(y, bank);
            v = cart8(bank, (uint16_t)(0x8016 + y));
            o->count48 = v;
            o->frame5a = (uint8_t)(v | 0x80);
            o->byte49 = cart8(bank, (uint16_t)(0x8017 + y));
            o->byte4a = cart8(bank, (uint16_t)(0x8018 + y));
            x = (uint16_t)(y + 0x19);
            break;
        case 0x80BA:                            /* $06 */
            memset(objects.bucket_head, 0, sizeof objects.bucket_head);
            g.spawn_mirror = 0;
            g.spawn_counter = 0;
            objects.place_count = 0;
            objects.list_length = 0;
            fxram_put(FX_COURSE_FLAGS, 0);
            g.course_flags = 0;
            fxram_put(FX_DRIVE_LEVEL, g.course);
            g.frame_record_count = 0;
            g.course_options = 0;
            g.rivals_left = 0;            /* and pace_signal, the same byte */
            ObjectPoolInit();
            x = (uint16_t)(y + 1);
            break;
        case 0x8112:                            /* $08 */
            g.spawn_mirror = cart8(bank, (uint16_t)(0x8001 + y));
            x = (uint16_t)(y + 2);
            break;
        case 0x8126:                            /* $0E */
            if (g.hud_mode == 3 && (cart8(bank, (uint16_t)(0x8018 + y)) & 0x1C)) {
                x = (uint16_t)(y + 0x1A);
                break;
            }
            o = ObjectSpawnFromScript(y, bank);
            v = cart8(bank, (uint16_t)(0x8018 + y));
            if (v & 0x01)
                g.player_car = obj_ref(o);
            if (v & 0x02)
                g.player2_car = obj_ref(o);
            o->timer = 4;
            o->heading = cart8(bank, (uint16_t)(0x8019 + y));
            o->link27 = cart16(bank, (uint16_t)(0x8016 + y));
            a = objects.place_count;
            if (a != 0x0008) {
                objects.place_count = (uint16_t)(a + 2);
                objects.place_objects[a / 2] = obj_ref(o);
                objects.place_objects[(uint16_t)(a + 2) / 2] = 0;
            }
            x = (uint16_t)(y + 0x1A);
            break;
        case 0x8196: {                          /* $12 */
            uint16_t *e = objects.place_table[(uint8_t)(cart8(bank, (uint16_t)(0x8001 + y)) - 1)];

            e[0] = cart16(bank, (uint16_t)(0x8002 + y));
            e[1] = cart16(bank, (uint16_t)(0x8004 + y));
            e[2] = cart16(bank, (uint16_t)(0x8006 + y));
            e[3] = (uint16_t)((e[3] & 0xFF00) | cart8(bank, (uint16_t)(0x8008 + y)));
            x = (uint16_t)(y + 9);
            break;
        }
        case 0x81CC:                            /* $14 */
            a = (uint16_t)(g.course_flags | cart16(bank, (uint16_t)(0x8001 + y)));
            fxram_put(FX_COURSE_FLAGS, a);
            g.course_flags = a;
            x = (uint16_t)(y + 3);
            break;
        case 0x81E6:                            /* $1A */
            g.course_options = cart16(bank, (uint16_t)(0x8001 + y));
            x = (uint16_t)(y + 3);
            break;
        case 0x81F8:                            /* $16 */
            bare_object(bank, y, &g.director[0], LINK_CAMERA_1);
            x = (uint16_t)(y + 4);
            break;
        case 0x8233:                            /* $18 */
            bare_object(bank, y, &g.director[1], LINK_CAMERA_2);
            x = (uint16_t)(y + 4);
            break;
        case 0x802C:                            /* $04: the end */
            y = (uint16_t)(y + 1);
            fxram_put(FX_TRACK_TABLE, (uint16_t)(y + 0x8000));
            fxram_put(FX_TRACK_TABLE_BANK, bank);
            return;
        default:                                /* $00: back to offset 0 */
            x = op;
            break;
        }
    }
}
