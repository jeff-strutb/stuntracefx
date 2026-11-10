/*
 * course_script.c: the course script: the byte-coded list of objects and
 * settings a course starts with.
 */
#include <stdio.h>
#include <string.h>

#include "data/cart.h"
#include "fx/fx.h"
#include "object/object.h"
#include "system/state.h"

/* the course script's commands: an entry's first byte */
enum {
    SCRIPT_RESTART = 0x00,              /* back to the script's start */
    SCRIPT_SPAWN = 0x02,                /* an object, from a $16-byte entry */
    SCRIPT_END = 0x04,
    SCRIPT_CLEAR = 0x06,                /* empty the buckets and pools, the course state cleared */
    SCRIPT_MIRROR = 0x08,               /* the heading the following objects spawn with */
    SCRIPT_SPAWN_COUNTED = 0x0A,        /* an object with its count, step and counter (+$48-$4A) */
    SCRIPT_SPAWN_FLAGGED = 0x0C,        /* an object with bit 0 of +$09 */
    SCRIPT_SPAWN_CAR = 0x0E,            /* a car: a player's, its heading, kept for placing */
    SCRIPT_SPAWN_NUMBERED = 0x10,       /* an object with its link and a number from the spawn counter */
    SCRIPT_PLACE = 0x12,                /* a start place's x, y, z and heading */
    SCRIPT_COURSE_FLAGS = 0x14,         /* course flags added */
    SCRIPT_DIRECTOR_1 = 0x16,           /* the first camera's director */
    SCRIPT_DIRECTOR_2 = 0x18,           /* the second camera's director */
    SCRIPT_OPTIONS = 0x1A               /* the course options */
};

/* the fields after the command byte of the entries that are not spawns */
enum {
    MIRROR_HEADING = 0x01,              /* SCRIPT_MIRROR */
    PLACE_NUMBER = 0x01, PLACE_X = 0x02, PLACE_Y = 0x04, PLACE_Z = 0x06, PLACE_HEADING = 0x08, /* SCRIPT_PLACE */
    WORD_VALUE = 0x01,                  /* SCRIPT_COURSE_FLAGS, SCRIPT_OPTIONS */
    DIRECTOR_HANDLER = 0x01, DIRECTOR_HANDLER_BANK = 0x03 /* SCRIPT_DIRECTOR_1, _2 */
};

/* A bare object for the script's directors: kept in `slot`, running from
   the entry's handler, linked to camera `link`. */
static void bare_object(uint8_t bank, uint16_t entry, uint16_t *slot, uint16_t link)
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
    o->handler = script16(bank, entry, DIRECTOR_HANDLER);
    o->handler_bank = script8(bank, entry, DIRECTOR_HANDLER_BANK);
}

/*
 * @routine 08:801C CourseScriptRun
 * WHAT IT DOES: Runs the course script at bank:$8000 + at. Each entry starts
 * with its command (the original's offset of its handler in the table at
 * $08:8000; the table's handlers are the cases here). Spawns objects, sets
 * the course's flags and placing tables, empties the pools; the end entry
 * leaves the address after it for the course's track table.
 */
void CourseScriptRun(uint8_t bank, uint16_t at)
{
    for (;;) {
        uint16_t entry = at;
        uint8_t command = cart8(bank, (uint16_t)(SCRIPT_BASE + entry));
        uint8_t v;
        Object *o;

        switch (command) {
        case SCRIPT_SPAWN:
            ObjectSpawnFromScript(entry, bank);
            at = (uint16_t)(entry + 0x16);
            break;
        case SCRIPT_SPAWN_FLAGGED:
            o = ObjectSpawnFromScript(entry, bank);
            o->depth_lift |= 0x01;
            at = (uint16_t)(entry + 0x16);
            break;
        case SCRIPT_SPAWN_NUMBERED:
            o = ObjectSpawnFromScript(entry, bank);
            o->script_args.entry_link = script16(bank, entry, ENTRY_LINK);
            o->script_args.number = ++g.spawn_counter;
            at = (uint16_t)(entry + 0x18);
            break;
        case SCRIPT_SPAWN_COUNTED:
            o = ObjectSpawnFromScript(entry, bank);
            v = script8(bank, entry, ENTRY_COUNT);
            o->script_args.count = v;
            o->shown_frame = (uint8_t)(v | 0x80);
            o->script_args.step = script8(bank, entry, ENTRY_STEP);
            o->script_args.counter = script8(bank, entry, ENTRY_COUNTER);
            at = (uint16_t)(entry + 0x19);
            break;
        case SCRIPT_CLEAR:
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
            at = (uint16_t)(entry + 1);
            break;
        case SCRIPT_MIRROR:
            g.spawn_mirror = script8(bank, entry, MIRROR_HEADING);
            at = (uint16_t)(entry + 2);
            break;
        case SCRIPT_SPAWN_CAR: {
            uint8_t players = script8(bank, entry, ENTRY_PLAYER);
            uint16_t n;

            if (g.hud_mode == 3 && (players & 0x1C)) {
                at = (uint16_t)(entry + 0x1A);
                break;
            }
            o = ObjectSpawnFromScript(entry, bank);
            if (players & 0x01)
                g.player_car = obj_ref(o);
            if (players & 0x02)
                g.player2_car = obj_ref(o);
            o->timer = 4;
            o->heading = script8(bank, entry, ENTRY_HEADING);
            o->car.car_script = script16(bank, entry, ENTRY_LINK);
            n = objects.place_count;    /* bytes: two an object, at most four */
            if (n != 0x0008) {
                objects.place_count = (uint16_t)(n + 2);
                objects.place_objects[n / 2] = obj_ref(o);
                objects.place_objects[n / 2 + 1] = 0;
            }
            at = (uint16_t)(entry + 0x1A);
            break;
        }
        case SCRIPT_PLACE: {
            uint16_t *e = objects.place_table[(uint8_t)(script8(bank, entry, PLACE_NUMBER) - 1)];

            e[0] = script16(bank, entry, PLACE_X);
            e[1] = script16(bank, entry, PLACE_Y);
            e[2] = script16(bank, entry, PLACE_Z);
            e[3] = (uint16_t)((e[3] & 0xFF00) | script8(bank, entry, PLACE_HEADING));
            at = (uint16_t)(entry + 9);
            break;
        }
        case SCRIPT_COURSE_FLAGS: {
            uint16_t flags = (uint16_t)(g.course_flags | script16(bank, entry, WORD_VALUE));

            fxram_put(FX_COURSE_FLAGS, flags);
            g.course_flags = flags;
            at = (uint16_t)(entry + 3);
            break;
        }
        case SCRIPT_OPTIONS:
            g.course_options = script16(bank, entry, WORD_VALUE);
            at = (uint16_t)(entry + 3);
            break;
        case SCRIPT_DIRECTOR_1:
            bare_object(bank, entry, &g.director[0], LINK_CAMERA_1);
            at = (uint16_t)(entry + 4);
            break;
        case SCRIPT_DIRECTOR_2:
            bare_object(bank, entry, &g.director[1], LINK_CAMERA_2);
            at = (uint16_t)(entry + 4);
            break;
        case SCRIPT_END:
            fxram_put(FX_TRACK_TABLE, (uint16_t)(SCRIPT_BASE + entry + 1));
            fxram_put(FX_TRACK_TABLE_BANK, bank);
            return;
        case SCRIPT_RESTART:
            at = 0;
            break;
        default:                        /* past the table's commands: no script has one */
            fprintf(stderr, "CourseScriptRun: command $%02X at %02X:%04X is not in the table\n",
                    command, bank, (unsigned)(SCRIPT_BASE + entry));
            return;
        }
    }
}
