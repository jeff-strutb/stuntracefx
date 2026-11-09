/*
 * frame.c: what the original's vertical blank did for the game's code: the
 * joypads read, the sound commands sent, the brightness stepped.
 */
#include "audio/sound.h"
#include "system/game.h"
#include "video/picture.h"

FrameState frame_state;

void input_read(const nshvc_input *input)
{
    int i;

    for (i = 0; i < 2; i++) {
        uint16_t now = input->pad[i];

        frame_state.pressed[i] = (uint16_t)((now ^ frame_state.held[i]) & now);
        frame_state.held[i] = now;
        frame_state.menu_pressed[i] |= frame_state.pressed[i];
    }
}

/* Every other call the brightness moves one step toward its target. */
static void brightness_step(void)
{
    frame_state.half_rate ^= 1;
    if (frame_state.half_rate)
        return;
    if (frame_state.brightness < frame_state.brightness_target)
        frame_state.brightness++;
    else if (frame_state.brightness > frame_state.brightness_target)
        frame_state.brightness--;
}

void frame_end(void)
{
    brightness_step();
    sound_frame();
    frame_state.frames++;
    pic.brightness = frame_state.brightness;
}
