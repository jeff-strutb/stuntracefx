/*
 * typing.c: the initials typed on a keyboard (remaster.h rm_type_key).
 *
 * The car selection's initials (menu/initials.c) are set with the pad:
 * up and down step a letter through A-Z, left and right move between the
 * three. A keyboard's letters go there too: while that screen runs, the
 * front end's key hook queues each letter typed (and Backspace), and the
 * screen's frame (MenuInitialsPresses) takes them from the queue as it
 * takes the pad's presses, before them.
 */
#include "console_ext.h"
#include "remaster.h"

/* the frame the initials screen last ran in, and whether it ever did */
static uint32_t seen_frame;
static int seen;
/* the keys typed and not yet taken: 0-25 a letter, RM_TYPED_BACK */
static uint8_t queue[16];
static unsigned head, tail;

void rm_typing_screen(void)
{
    if (!seen || nshvc_frames() - seen_frame > 10)
        head = tail;                            /* the screen anew: nothing typed before it */
    seen_frame = nshvc_frames();
    seen = 1;
}

int rm_type_key(int vk, int down)
{
    int code;

    /* the screen ran lately: a menu frame takes a few video frames in
       Original mode (the hook comes between video frames) */
    if (!seen || nshvc_frames() - seen_frame > 10)
        return 0;
    if (vk >= 'A' && vk <= 'Z')
        code = vk - 'A';
    else if (vk == 0x08)                        /* Backspace */
        code = RM_TYPED_BACK;
    else
        return 0;
    if (down && tail - head < sizeof queue)
        queue[tail++ % sizeof queue] = (uint8_t)code;
    return 1;                                   /* the key's release too: no button of the pad's */
}

int rm_typed_next(void)
{
    if (head == tail)
        return -1;
    return queue[head++ % sizeof queue];
}
