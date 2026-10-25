/*
 * sound_queue.c: the two sound command queues.
 *
 * Game code asks for sounds by queueing command bytes, each queue a 16-byte
 * ring with a head and a count; the frame sends the oldest byte of each to
 * the sound driver once it has taken the last.
 */
#include "game.h"

/* Appended unless sound is muted or the ring is full. */
static void queue_push(uint8_t *ring, uint8_t head, uint8_t *count, uint8_t command)
{
    uint8_t slot;

    if (sound_muted || *count >= 0x10)
        return;
    slot = (uint8_t)(*count + head);
    if (slot >= 0x10)
        slot -= 0x10;
    ring[slot] = command;
    ++*count;
}

void sound_queue_raw(uint8_t command)
{
    queue_push(sound_port0_queue, sound_port0_head, &sound_port0_count, command);
}

/* For the player whose car is being processed: bit 7 marks player 2. */
void sound_queue(uint8_t command)
{
    if (current_player)
        command |= 0x80;
    sound_queue_raw(command);
}

void sound_queue_port1(uint8_t command)
{
    queue_push(sound_port1_queue, sound_port1_head, &sound_port1_count, command);
}
