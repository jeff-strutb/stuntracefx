/*
 * handlers.c: the objects' handlers, found by the address the original
 * keeps for them in an object (objects.h).
 */
#include <stdio.h>
#include <stdlib.h>

#include "game.h"
#include "objects.h"

static const struct {
    uint32_t addr;
    ObjectHandler *run;
} handlers[] = {
    {0, NULL},
};

ObjectHandler *object_handler(uint32_t addr)
{
    size_t i;

    for (i = 0; i < sizeof handlers / sizeof handlers[0]; i++)
        if (handlers[i].run && handlers[i].addr == addr)
            return handlers[i].run;
    return NULL;
}

void object_handler_run(uint32_t addr, uint16_t obj)
{
    ObjectHandler *h = object_handler(addr);

    if (!h) {
        fprintf(stderr, "no object handler at %02X:%04X\n", (unsigned)(addr >> 16), (unsigned)(addr & 0xFFFF));
        abort();
    }
    h(obj);
}
