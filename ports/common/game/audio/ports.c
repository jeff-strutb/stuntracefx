/*
 * ports.c: the four ports between the game and the sound driver. The game
 * writes a command to a port; the driver shows what it is working on, and
 * the game waits for a port to clear before writing the next command.
 */
#include "game.h"

uint8_t sound_in[4];                    /* what the driver shows */
uint8_t sound_out[4];                   /* what the game last wrote */

/* One queue to its port: 0 while the port still shows a command, else
   the queue's oldest byte. 1 if the port was written, the byte in *v. */
static int queue_send(int port, uint8_t *ring, uint8_t *head, uint8_t *count, uint8_t *v)
{
    if (sound_in[port])
        *v = 0;
    else {
        if (!*count)
            return 0;
        *v = ring[*head];
        if (--*count)
            *head = (uint8_t)(*head + 1 >= 0x10 ? 0 : *head + 1);
    }
    sound_out[port] = *v;
    return 1;
}

/* The frame's sound output: a direct byte for port 0 ($10E2) or port 1
   ($10E3) goes alone; otherwise, unless held ($10DA, $10DC), on odd
   frames each port gets its queue's next byte (port 1's kept at $0DD2);
   then ports 2 and 3 get $0E47 and $0E49 (0 while $0E4B is set). */
void sound_ports_send(void)
{
    uint8_t v;

    if ((v = W8(0x10E2))) {
        sound_out[0] = v;
        W8(0x10E2) = 0;
        return;
    }
    if ((v = W8(0x10E3))) {
        sound_out[1] = v;
        W8(0x10E3) = 0;
        return;
    }
    if (W8(0x10DA) || W8(0x10DC) || !(frame_count & 1))
        return;
    queue_send(0, sound_port0_queue, &sound_port0_head, &sound_port0_count, &v);
    if (queue_send(1, sound_port1_queue, &sound_port1_head, &sound_port1_count, &v))
        W8(0x0DD2) = v;
    if (W8(0x0E4B))
        sound_out[2] = sound_out[3] = 0;
    else {
        sound_out[2] = W8(0x0E47);
        sound_out[3] = W8(0x0E49);
    }
}
