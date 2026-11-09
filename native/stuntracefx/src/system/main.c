/*
 * main.c: the game on the engine. The engine owns the loop and calls these.
 */
#include "audio/music.h"
#include "audio/sound.h"
#include "data/cart.h"
#include "fx/fx_draw.h"
#include "nshvc/nshvc.h"
#include "system/game.h"
#include "video/picture.h"

static void init(void)
{
    cart_load();
    sound_init();
    video_init();
    fx_draw_init();
    session_start();
}

static void update(const nshvc_input *input)
{
    input_read(input);
    session_update();
}

static void draw(float between)
{
    (void)between;
    video_draw();
}

int main(int argc, char **argv)
{
    static nshvc_game game;

    game.title = "Stunt Race FX";
    game.id = "stunt-race-fx";
    game.update_hz = 60;
    game.init = init;
    game.update = update;
    game.draw = draw;
    game.music.hz = 32040;
    game.music.tick = music_tick;
    game.music.queue_bytes = 4096;
    return nshvc_run(&game, argc, argv);
}
