/*
 * joypad.c: the joypads as the race reads them: the raster's accumulation
 * of the buttons between the steps. The menus' read is in frame.c.
 *
 * Not converted, no use on the engine: JoypadPortCheck and its inner entries
 * (the Super Multitap test, which only sets a flag nothing else reads).
 */
#include "save/save.h"
#include "system/game.h"
#include "system/state.h"

/*
 * Auto-accelerate (an option, on unless the player turns it off; the
 * menus' option_auto_accelerate): on a touch screen, player 1's gas (B) is
 * held while they drive, let go while the brake (A) is held, as the touch
 * scheme of the earlier front end had it (a touch screen has no button to
 * keep a thumb on). The race calls it before it folds in the pads, only
 * while it is reading the player's controls: not in a demo, not paused.
 */
void JoypadAutoAccelerate(void)
{
    if (!option_auto_accelerate() || nshvc_control_source(0).kind != NSHVC_SOURCE_TOUCH)
        return;
    if (!(frame_state.pad_now[0] & PAD_A))
        frame_state.pad_now[0] |= PAD_B;
}

/*
 * @routine 7E:B1AE JoypadRead
 * WHAT IT DOES: Both pads as the course screen's raster reads them: the
 * buttons pressed since the last read, in the presses and their copy, and the
 * buttons held.
 */
void JoypadRead(void)
{
    for (int i = 0; i < 2; i++) {
        uint16_t now = frame_state.pad_now[i];
        uint16_t pressed = (uint16_t)((now ^ frame_state.held[i]) & now);

        frame_state.pressed[i] = pressed;
        frame_state.pressed_copy[i] = pressed;
        frame_state.held[i] = now;
    }
}

/*
 * @routine 7E:B1E1 JoypadAccumulate
 * WHAT IT DOES: Folds this frame's pads into the step's presses and held
 * buttons; the second pad's press mirror is computed from each pad in turn,
 * the second write staying, as the original does.
 */
void JoypadAccumulate(void)
{
    for (int i = 0; i < 2; i++) {
        uint16_t now = frame_state.pad_now[i];

        frame_state.pressed_copy[i] |= (uint16_t)((now ^ g.pad_last[i]) & now);
        g.pad_last[i] = now;
    }
    frame_state.pressed[1] = (uint16_t)((frame_state.pad_now[1] ^ frame_state.held[1]) & frame_state.pad_now[1]);
    frame_state.held[1] |= frame_state.pad_now[1];
    frame_state.held[0] |= frame_state.pad_now[0];
}
