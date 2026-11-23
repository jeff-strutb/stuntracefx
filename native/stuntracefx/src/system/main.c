/*
 * main.c: the game on the engine. The engine owns the loop and calls these.
 */
#include <stdio.h>
#include <string.h>

#include "audio/music.h"
#include "audio/sound.h"
#include "data/cart.h"
#include "fx/fx_draw.h"
#include "input/controls.h"
#include "nshvc/nshvc.h"
#include "pause/pause.h"
#include "race/race.h"
#include "system/display.h"
#include "system/fps.h"
#include "system/game.h"
#include "system/perf.h"
#include "video/between.h"
#include "video/picture.h"
#include "video/video.h"

/* nshvc_game.save_in_game_folder, API 6.12: the saves in the game's own folder
   where the host can (Windows and Linux: "save" beside the program; macOS:
   "stunt-race-fx-save" beside the .app; iOS and Android keep the app's
   container), the files in the system's folder carried over the first time.
   An older engine keeps them in the system's folder. */
#define SAVE_IN_GAME_FOLDER (NSHVC_VERSION_MAJOR > 6 || (NSHVC_VERSION_MAJOR == 6 && NSHVC_VERSION_MINOR >= 12))

static void init(void)
{
    cart_load();
    sound_init();
    video_init();
    fx_draw_init();
    session_start();
    display_start();                    /* full screen, or the window the player chose */
}

static void update_body(const nshvc_input *input)
{
    uint16_t pad[2];

    if (system_stopped())
        return;
    controls_read(input, pad);          /* the devices as the game's pads */
    if (!pause_update(input, pad))      /* the pause screen; 0 while it holds the game */
        return;
    if (race_view_hold()) {             /* the race's view switching: the game held (race/overhead.c) */
        pause_hold_other();
        return;
    }
    video_update_begin();               /* the update before's world kept, its art and maps written */
    input_read(pad);
    session_update();
    video_update_end();
}

static void update(const nshvc_input *input)
{
    display_update();
    perf_update_begin();
    update_body(input);
    perf_update_end();
}

static void draw_body(float between)
{
    if (pause_capturing()) {
        controls_hide();                /* nothing over the pictures the pause's blur is made from */
        fps_hide();
    } else {
        controls_draw();                /* the touch screen's controls, over everything */
        fps_draw();                     /* the pictures drawn a second, at the top right */
    }
    if (pause_draw())
        return;                         /* the pause screen's picture: the last one, blurred */
    between = race_overhead_between(between);   /* while the view switches, the picture as it stood */
    if (pause_holding())
        between = 1.0f;                 /* the picture as the game's last update left it */
    video_between_begin(between);       /* the moving 2D between the last two updates (pic eased in place) */
    fx_tween_draw(between);             /* the race's 3D at the step's progress, between steps */
    nshvc_profile_begin(perf_video);
    video_draw();
    nshvc_profile_end();
    video_between_end();                /* the game's own pic back */
}

static void draw(float between)
{
    perf_draw_begin();
    draw_body(between);
    perf_draw_end();                    /* SRFX_PROFILE=1: where the time goes, once a second (perf.c) */
}

int main(int argc, char **argv)
{
    static nshvc_game game;
    int i, n = 0;

    /* --window W,H[,LEFT,TOP,RIGHT,BOTTOM]: for a check, the window's shape
       and safe area in window pixels (a headless run has no window of its
       own); taken out before the engine's options */
    for (i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--window") && i + 1 < argc) {
            float v[6] = { 0, 0, 0, 0, 0, 0 };
            int k = sscanf(argv[++i], "%f,%f,%f,%f,%f,%f", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]);

            if (k >= 2)
                screen_window_set(v, k);
            else
                fprintf(stderr, "--window needs WIDTH,HEIGHT\n");
            continue;
        }
        argv[n++] = argv[i];
    }
    argc = n;

    game.title = "Stunt Race FX";
    game.id = "stunt-race-fx";
    game.update_hz = 60;
#if SAVE_IN_GAME_FOLDER
    game.save_in_game_folder = 1;       /* records and settings beside the game, so a copied game carries them */
#endif
    game.init = init;
    game.update = update;
    game.draw = draw;
    game.music.hz = 32040;
    game.music.tick = music_tick;
    game.music.queue_bytes = 4096;
    return nshvc_run(&game, argc, argv);
}
