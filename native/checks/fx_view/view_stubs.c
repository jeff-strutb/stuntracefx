/* What the picture's layer needs from the game; nothing in the viewer. */
#include "nshvc/nshvc.h"
#include "system/state.h"

Game g;
void race_frame_art_draw(nshvc_draw_list l) { (void)l; }
void race_frame_art_draw_over(nshvc_draw_list l) { (void)l; }
int race_full_window(void) { return 0; }
int race_two_full_window(void) { return 0; }
int race_course_full_window(void) { return 0; }
#include "video/picture.h"
Picture pic;
void screen_two_rect(int player, int *x, int *y, int *w, int *h) { (void)player; *x = *y = 0; *w = 256; *h = 224; }
void screen_two_anchor(int player, int x0, int x1, int y0, int y1, int *dx, int *dy) { (void)player; (void)x0; (void)x1; (void)y0; (void)y1; *dx = *dy = 0; }
#include "object/object.h"
ObjectTable objects;
Camera cameras[2];
float race_step_progress(float between) { (void)between; return 0; }
int RaceViewForPicture(int panel) { (void)panel; return 0; }
uint16_t draw_record_gaze(int record) { (void)record; return 0; }
int screen_view_anchor_x(int x0, int x1) { (void)x0; (void)x1; return 0; }
int screen_anchor_y(int y0, int y1) { (void)y0; (void)y1; return 0; }
#include "video/video.h"
const ScreenShape *screen_shape(void) { static const ScreenShape s = { 256, 224, 0, 0, { 0, 0, 0, 0 } }; return &s; }
