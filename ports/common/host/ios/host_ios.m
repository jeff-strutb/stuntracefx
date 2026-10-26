/* host_ios.m: the iOS host -- the app, a full-screen view that shows the
 * frames, touch controls, the keyboard and game controllers, sound out.
 * The counterpart of host/macos/host_macos.m; host/posix under it supplies
 * time and files.
 *
 * UIKit keeps the main thread, so the game's main (built as port_main,
 * link.sh's HOST=ios) runs its own loop on a thread of its own, started
 * when the scene first becomes active. What UIKit reports reaches that
 * thread through a locked event queue that host_poll_event drains. When the
 * app leaves the foreground, the game thread is held inside
 * host_poll_event until it comes back: iOS allows no GPU work in the
 * background.
 *
 * Frames are drawn by the renderer (render/metal) into the view's
 * CAMetalLayer (host_metal_layer), one drawable pixel per screen pixel, or
 * arrive as ARGB pixels through host_present (the software renderer).
 *
 *   touch     in a race the wheel and pedals, else swipes and taps for the
 *             menus (PortTouch), as the keys the keyboard's game keys send
 *   keys      a hardware keyboard's HID usage, as a Win32 virtual key
 *   sound     a RemoteIO output unit pulling the game's mixer
 *   pads      GameController.framework, as on macOS
 *
 * PORT_SAVEDIR names the save folder (default the app's Application
 * Support/<app>); PORT_INPUTLOG=1 reports each key event on stderr (input
 * checks).
 */
#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#import <Metal/Metal.h>
#import <AVFoundation/AVFoundation.h>
#import <AudioToolbox/AudioToolbox.h>
#import <GameController/GameController.h>
#include <os/lock.h>
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>

#include "host.h"

/* the game's main, renamed by the build */
int port_main(int argc, char **argv);
static void host_audio_resume(void);

static char s_save[1024];
static const char *s_app_dir = "Game", *s_app_title = "Game";

void host_set_app_name(const char *dir, const char *title)
{
    if (dir)
        s_app_dir = dir;
    if (title)
        s_app_title = title;
}

/* ---- process -------------------------------------------------------------------- */
void host_init(int argc, char **argv)
{
    const char *e;
    (void)argc;
    (void)argv;
    @autoreleasepool {
        e = getenv("PORT_SAVEDIR");
        if (e) {
            snprintf(s_save, sizeof s_save, "%s", e);
        } else {
            NSString *base = [NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory,
                                                                 NSUserDomainMask, YES) firstObject];
            snprintf(s_save, sizeof s_save, "%s/%s", base ? [base fileSystemRepresentation] : ".", s_app_dir);
        }
        /* (a new app's container has no Application Support yet) */
        [[NSFileManager defaultManager] createDirectoryAtPath:[NSString stringWithUTF8String:s_save]
                                  withIntermediateDirectories:YES attributes:nil error:NULL];
    }
}

const char *host_resource_dir(void)
{
    static char res[1024];
    const char *e = getenv("PORT_RESDIR");
    if (e)
        return e;
    if (!res[0]) {
        @autoreleasepool {
            NSString *r = [[NSBundle mainBundle] resourcePath];
            snprintf(res, sizeof res, "%s", r ? [r fileSystemRepresentation] : ".");
        }
    }
    return res;
}

void host_shutdown(void)
{
    host_audio_close();
    host_window_close();
}

const char *host_save_dir(void) { return s_save; }

/* ---- the event queue UIKit fills and host_poll_event drains ----------------------- */
#define QMAX 256
static host_event s_q[QMAX];
static int s_qh, s_qt;
static os_unfair_lock s_qlock = OS_UNFAIR_LOCK_INIT;
/* by virtual key: how many sources (the keyboard, the touch controls) hold
   it, so one letting go of a key the other holds sends nothing */
static uint8_t s_held[256];

static void qpush(const host_event *e)
{
    int n;
    os_unfair_lock_lock(&s_qlock);
    n = (s_qt + 1) % QMAX;
    if (n != s_qh) {
        s_q[s_qt] = *e;
        s_qt = n;
    }
    os_unfair_lock_unlock(&s_qlock);
}

/* (main thread) */
static void key_event(int vk, int down)
{
    host_event e;
    if (vk <= 0 || vk >= 256)
        return;
    if (down ? s_held[vk]++ != 0 : s_held[vk] == 0 || --s_held[vk] != 0)
        return;
    memset(&e, 0, sizeof e);
    e.type = HOST_EV_KEY;
    e.vk = vk;
    e.down = down;
    qpush(&e);
    if (getenv("PORT_INPUTLOG"))
        fprintf(stderr, "input: key %02X %s\n", vk, down ? "down" : "up");
}

/* ---- keys: a hardware keyboard's HID usage -> Win32 virtual key ------------------- */
static int hid_vk(long u)
{
    static const struct { uint8_t hid, vk; } k[] = {
        { 0x28, 0x0D }, { 0x58, 0x0D }, { 0x29, 0x1B }, { 0x2A, 0x08 }, { 0x2B, 0x09 }, { 0x2C, 0x20 },
        { 0x4F, 0x27 }, { 0x50, 0x25 }, { 0x51, 0x28 }, { 0x52, 0x26 },
        { 0xE1, 0x10 }, { 0xE5, 0x10 }, { 0xE0, 0x11 }, { 0xE4, 0x11 }, { 0xE2, 0x12 }, { 0xE6, 0x12 },
        { 0x35, 0xC0 }, { 0x2D, 0xBD }, { 0x2E, 0xBB }, { 0x2F, 0xDB }, { 0x30, 0xDD }, { 0x31, 0xDC },
        { 0x33, 0xBA }, { 0x34, 0xDE }, { 0x36, 0xBC }, { 0x37, 0xBE }, { 0x38, 0xBF },
        { 0x4A, 0x24 }, { 0x4B, 0x21 }, { 0x4D, 0x23 }, { 0x4E, 0x22 }, { 0x49, 0x2D }, { 0x4C, 0x2E },
    };
    size_t i;
    if (u >= 0x04 && u <= 0x1D)
        return 'A' + (int)(u - 0x04);
    if (u >= 0x1E && u <= 0x26)
        return '1' + (int)(u - 0x1E);
    if (u == 0x27)
        return '0';
    if (u >= 0x3A && u <= 0x45)
        return 0x70 + (int)(u - 0x3A);
    for (i = 0; i < sizeof k / sizeof k[0]; i++)
        if (k[i].hid == u)
            return k[i].vk;
    return 0;
}

static uint8_t s_kb[256];            /* by HID usage: held */

static void kb_event(long u, int down)
{
    if (u < 0 || u >= 256 || s_kb[u] == down)
        return;
    s_kb[u] = (uint8_t)down;
    key_event(hid_vk(u), down);
}

/* ---- touch: the wheel and pedals in a race, swipes and taps elsewhere ------------- */
/* As the Boss Rally port's (ports/remastered/ios TouchPad.swift), on the
 * SNES pad of this game's race (object/gsu_record.c ctl_player): B
 * accelerates, A brakes (and reverses below a crawl), Left and Right steer,
 * L and R steer harder, Y boosts. Every press is the key the keyboard sends
 * for that button (runtime/main.c key()).
 *
 * While the player drives (host_set_driving):
 *   the gas    on by itself (the Settings app's Auto-Accelerate, on unless
 *              turned off): B is held all the while, let go while the brake
 *              is (the pedal wins). Turned off, the wheel's finger holds it
 *              for as long as it stays on the glass, and lifting coasts.
 *   the wheel  a finger right of the pedals; moving it sideways
 *              steers, full lock at 90 points either side, the neutral point
 *              sliding along when the finger goes past the lock so the thumb
 *              never runs out of room. The pad is digital: past a fifth of
 *              the lock it is Left or Right, at nine tenths also L or R (the
 *              hard turn). A wheel drawn where the finger landed turns with
 *              the steering.
 *   the pedals the left 30% of the screen, two pedals one above the other:
 *              the lower holds A, the brake (slow enough, the car reverses:
 *              the game's own rule); the upper holds Y, the boost.
 *   pause      the button at the top centre, or a tap with two fingers:
 *              START.
 * Elsewhere (menus, the pause menu, the title): a swipe is the d-pad, the
 * way the content follows the finger (a swipe to the left is Right, a swipe
 * up is Down); a tap is B and START together (every menu's confirm); a tap
 * with two fingers is Y (back). */
enum { VK_LEFT = 0x25, VK_UP = 0x26, VK_RIGHT = 0x27, VK_DOWN = 0x28, VK_START = 0x0D,
       VK_B = 'Z', VK_A = 'X', VK_Y = 'A', VK_L = 'Q', VK_R = 'W' };

static _Atomic int s_driving;              /* host_set_driving, from the game thread */

void host_set_driving(int driving) { atomic_store(&s_driving, driving != 0); }

/* a button pressed a moment (5 frames: a press the game's pad read sees) */
static void key_pulse(int vk)
{
    key_event(vk, 1);
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 80 * NSEC_PER_MSEC), dispatch_get_main_queue(),
                   ^{ key_event(vk, 0); });
}

static UIColor *rgb(unsigned c) { return [UIColor colorWithRed:(c >> 16) / 255.0 green:(c >> 8 & 0xFF) / 255.0 blue:(c & 0xFF) / 255.0 alpha:1]; }

/* The steering wheel drawn under the finger: a rim, a hub and three spokes. */
static UIView *wheel_view(CGFloat size)
{
    UIView *v = [[UIView alloc] initWithFrame:CGRectMake(0, 0, size, size)];
    CGFloat k = size / 100;
    UIColor *gold = rgb(0xF5D34A);
    CAShapeLayer *rim = [CAShapeLayer layer], *hub = [CAShapeLayer layer], *sp = [CAShapeLayer layer];
    UIBezierPath *spokes = [UIBezierPath bezierPath];
    static const CGFloat s[3][4] = { { 50, 40, 50, 10 }, { 59, 56, 84, 70 }, { 41, 56, 16, 70 } };
    int i;
    v.userInteractionEnabled = NO;
    v.alpha = 0.9;
    rim.path = [UIBezierPath bezierPathWithOvalInRect:CGRectMake(6 * k, 6 * k, 88 * k, 88 * k)].CGPath;
    rim.fillColor = nil;
    rim.strokeColor = gold.CGColor;
    rim.lineWidth = 9 * k;
    [v.layer addSublayer:rim];
    hub.path = [UIBezierPath bezierPathWithOvalInRect:CGRectMake(39 * k, 39 * k, 22 * k, 22 * k)].CGPath;
    hub.fillColor = gold.CGColor;
    [v.layer addSublayer:hub];
    for (i = 0; i < 3; i++) {
        [spokes moveToPoint:CGPointMake(s[i][0] * k, s[i][1] * k)];
        [spokes addLineToPoint:CGPointMake(s[i][2] * k, s[i][3] * k)];
    }
    sp.path = spokes.CGPath;
    sp.strokeColor = gold.CGColor;
    sp.lineWidth = 7 * k;
    sp.lineCap = kCALineCapRound;
    [v.layer addSublayer:sp];
    v.layer.shadowColor = gold.CGColor;
    v.layer.shadowOpacity = 0.45;
    v.layer.shadowRadius = 8;
    v.layer.shadowOffset = CGSizeZero;
    return v;
}

/* A pedal: a ribbed pad with its name, brighter while pressed. */
static UIView *pedal_view(CGRect frame, unsigned colour, NSString *name)
{
    UIView *v = [[UIView alloc] initWithFrame:frame];
    UIColor *c = rgb(colour);
    CAShapeLayer *pad = [CAShapeLayer layer], *rb = [CAShapeLayer layer];
    UIBezierPath *ribs = [UIBezierPath bezierPath];
    UILabel *label = [[UILabel alloc] initWithFrame:CGRectMake(0, frame.size.height - 36, frame.size.width, 22)];
    int i;
    v.userInteractionEnabled = NO;
    v.alpha = 0.4;
    pad.path = [UIBezierPath bezierPathWithRoundedRect:CGRectInset(v.bounds, 4, 4) cornerRadius:16].CGPath;
    pad.fillColor = [UIColor colorWithWhite:0.08 alpha:0.55].CGColor;
    pad.strokeColor = c.CGColor;
    pad.lineWidth = 4;
    [v.layer addSublayer:pad];
    for (i = 0; i < 4 && 22 + i * 16 < frame.size.height - 40; i++) {
        [ribs moveToPoint:CGPointMake(22, 22 + i * 16)];
        [ribs addLineToPoint:CGPointMake(frame.size.width - 22, 22 + i * 16)];
    }
    rb.path = ribs.CGPath;
    rb.strokeColor = c.CGColor;
    rb.lineWidth = 5;
    rb.lineCap = kCALineCapRound;
    [v.layer addSublayer:rb];
    label.text = name;
    label.textAlignment = NSTextAlignmentCenter;
    label.font = [UIFont systemFontOfSize:13 weight:UIFontWeightHeavy];
    label.textColor = c;
    [v addSubview:label];
    return v;
}

static void pedal_pressed(UIView *v, BOOL on)
{
    v.alpha = on ? 0.85 : 0.4;
    v.transform = on ? CGAffineTransformMakeScale(0.94, 0.94) : CGAffineTransformIdentity;
}

/* The pause button at the top centre while the player drives: two bars. */
static UIControl *pause_button(void)
{
    UIControl *b = [[UIControl alloc] initWithFrame:CGRectMake(0, 0, 56, 44)];
    CAShapeLayer *bars = [CAShapeLayer layer];
    UIBezierPath *p = [UIBezierPath bezierPath];
    [p appendPath:[UIBezierPath bezierPathWithRoundedRect:CGRectMake(17, 10, 8, 24) cornerRadius:2]];
    [p appendPath:[UIBezierPath bezierPathWithRoundedRect:CGRectMake(31, 10, 8, 24) cornerRadius:2]];
    bars.path = p.CGPath;
    bars.fillColor = UIColor.whiteColor.CGColor;
    bars.shadowColor = UIColor.blackColor.CGColor;
    bars.shadowOpacity = 0.6;
    bars.shadowRadius = 3;
    bars.shadowOffset = CGSizeZero;
    [b.layer addSublayer:bars];
    b.alpha = 0.75;
    return b;
}

/* the Settings app's Auto-Accelerate (Settings.bundle, package_ios.sh), on
   until turned off */
static BOOL auto_accelerate(void)
{
    static dispatch_once_t once;
    dispatch_once(&once, ^{ [NSUserDefaults.standardUserDefaults registerDefaults:@{ @"autoAccelerate": @YES }]; });
    return [NSUserDefaults.standardUserDefaults boolForKey:@"autoAccelerate"];
}

typedef enum { ROLE_NONE, ROLE_WHEEL, ROLE_BRAKE, ROLE_BOOST, ROLE_MENU } Role;

@interface PortTouch : UIView
- (void)reset;
@end

@implementation PortTouch {
    NSMutableDictionary<NSValue *, NSArray *> *_fingers;   /* touch -> role, start x, start y, time */
    CGPoint _wheelOrigin;
    int _steerKeys;                        /* the steering keys held: bits Left, Right, L, R */
    BOOL _gas;                             /* B held for the accelerator */
    double _gestureBegan;
    int _gestureFingers;
    BOOL _gestureMoved;
    BOOL _driving;
    UIView *_wheel, *_brake, *_boost;
    UIControl *_pause;
    CADisplayLink *_link;
}

static const CGFloat k_steer_range = 90;   /* points of sideways travel to full lock */
static const CGFloat k_brake_zone = 0.3;   /* the left of the screen: the pedals */
static const CGFloat k_boost_above = 0.45; /* the pedals' split: the boost above this much of the height */
static const CGFloat k_wheel_size = 160;
static const CGFloat k_wheel_degrees = 120;
static const double k_tap_seconds = 0.25;
static const CGFloat k_tap_points = 12;
static const CGFloat k_swipe_points = 30;

- (instancetype)initWithFrame:(CGRect)f
{
    if (!(self = [super initWithFrame:f]))
        return nil;
    self.multipleTouchEnabled = YES;
    self.backgroundColor = UIColor.clearColor;
    _fingers = [NSMutableDictionary new];
    _brake = pedal_view(CGRectMake(0, 0, 84, 128), 0xE84A3C, @"BRAKE");
    _boost = pedal_view(CGRectMake(0, 0, 84, 112), 0x4AA8F5, @"BOOST");
    _wheel = wheel_view(k_wheel_size);
    _pause = pause_button();
    _brake.hidden = _boost.hidden = _wheel.hidden = _pause.hidden = YES;
    [_pause addTarget:self action:@selector(pausePressed) forControlEvents:UIControlEventTouchUpInside];
    [self addSubview:_brake];
    [self addSubview:_boost];
    [self addSubview:_wheel];
    [self addSubview:_pause];
    _link = [CADisplayLink displayLinkWithTarget:self selector:@selector(tick)];
    [_link addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
    return self;
}

- (void)layoutSubviews
{
    CGRect b = self.bounds;
    UIEdgeInsets s = self.safeAreaInsets;
    [super layoutSubviews];
    _brake.center = CGPointMake(round(b.size.width * k_brake_zone * 0.5), round(b.size.height * 0.7));
    _boost.center = CGPointMake(_brake.center.x, round(b.size.height * 0.27));
    (void)s;
    _pause.center = CGPointMake(round(b.size.width / 2), round(MAX(s.top, 6) + 24));
}

- (void)pausePressed { key_pulse(VK_START); }

/* the pedals are shown while the player drives; a change between driving
   and the menus starts afresh */
- (void)tick
{
    BOOL d = atomic_load(&s_driving) != 0;
    BOOL show = d;
    if (d != _driving) {
        _driving = d;
        [self reset];
        if (getenv("PORT_INPUTLOG"))
            fprintf(stderr, "input: touch %s\n", d ? "drives" : "is the menus'");
    }
    [self gas];                            /* (Auto-Accelerate may have changed in Settings) */
    if (_brake.hidden == show) {
        _brake.hidden = _boost.hidden = _pause.hidden = !show;
        [self setNeedsLayout];
    }
}

- (BOOL)has:(Role)r
{
    for (NSValue *k in _fingers)
        if ([_fingers[k][0] intValue] == r)
            return YES;
    return NO;
}

- (Role)roleAt:(CGPoint)p
{
    if (_driving) {
        if (p.x < self.bounds.size.width * k_brake_zone)
            return p.y < self.bounds.size.height * k_boost_above ? ROLE_BOOST : ROLE_BRAKE;
        return [self has:ROLE_WHEEL] ? ROLE_NONE : ROLE_WHEEL;
    }
    return [self has:ROLE_MENU] ? ROLE_NONE : ROLE_MENU;
}

/* B, the accelerator: held while driving with Auto-Accelerate (or, without
   it, while the wheel's finger is down), unless the brake is */
- (void)gas
{
    BOOL on = _driving && !self.hasBrake && (auto_accelerate() || [self has:ROLE_WHEEL]);
    if (on != _gas) {
        _gas = on;
        key_event(VK_B, on);
    }
}

- (BOOL)hasBrake { return [self has:ROLE_BRAKE]; }

/* the steering keys for s (-1 left .. 1 right): Left or Right past a fifth
   of the lock, and L or R at nine tenths */
- (void)steer:(CGFloat)s
{
    int want = 0, ch, i;
    static const int vk[4] = { VK_LEFT, VK_RIGHT, VK_L, VK_R };
    if (s <= -0.2) want |= 1;
    if (s >= 0.2) want |= 2;
    if (s <= -0.9) want |= 4;
    if (s >= 0.9) want |= 8;
    ch = want ^ _steerKeys;
    for (i = 0; i < 4; i++)
        if (ch & 1 << i)
            key_event(vk[i], (want >> i) & 1);
    _steerKeys = want;
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)ev
{
    double now = CACurrentMediaTime();
    (void)ev;
    for (UITouch *t in touches) {
        CGPoint p = [t locationInView:self];
        Role r;
        if (_fingers.count == 0) {
            _gestureBegan = now;
            _gestureFingers = 0;
            _gestureMoved = NO;
        }
        _gestureFingers++;
        r = [self roleAt:p];
        _fingers[[NSValue valueWithNonretainedObject:t]] = @[ @(r), @(p.x), @(p.y), @(now) ];
        switch (r) {
        case ROLE_WHEEL:
            _wheelOrigin = p;
            _wheel.center = p;
            _wheel.transform = CGAffineTransformIdentity;
            _wheel.hidden = NO;
            break;
        case ROLE_BRAKE:
            key_event(VK_A, 1);
            pedal_pressed(_brake, YES);
            break;
        case ROLE_BOOST:
            key_event(VK_Y, 1);
            pedal_pressed(_boost, YES);
            break;
        default:
            break;
        }
        if (getenv("PORT_INPUTLOG"))
            fprintf(stderr, "input: touch at %.0f,%.0f of %.0fx%.0f: role %d\n", p.x, p.y, self.bounds.size.width,
                    self.bounds.size.height, (int)r);
    }
    [self gas];
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)ev
{
    (void)ev;
    for (UITouch *t in touches) {
        NSArray *f = _fingers[[NSValue valueWithNonretainedObject:t]];
        CGPoint p = [t locationInView:self];
        CGFloat raw, s;
        if (!f)
            continue;
        if (hypot(p.x - [f[1] doubleValue], p.y - [f[2] doubleValue]) > k_tap_points)
            _gestureMoved = YES;
        if ([f[0] intValue] != ROLE_WHEEL)
            continue;
        raw = p.x - _wheelOrigin.x;
        if (raw > k_steer_range) {
            _wheelOrigin.x = p.x - k_steer_range;
            raw = k_steer_range;
        } else if (raw < -k_steer_range) {
            _wheelOrigin.x = p.x + k_steer_range;
            raw = -k_steer_range;
        }
        s = raw / k_steer_range;
        [self steer:s];
        _wheel.center = CGPointMake(_wheelOrigin.x, _wheel.center.y);   /* the wheel follows the sliding neutral */
        _wheel.transform = CGAffineTransformMakeRotation(s * k_wheel_degrees * M_PI / 180);
    }
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)ev { (void)ev; [self end:touches cancelled:NO]; }
- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)ev { (void)ev; [self end:touches cancelled:YES]; }

- (void)end:(NSSet<UITouch *> *)touches cancelled:(BOOL)cancelled
{
    double now = CACurrentMediaTime();
    for (UITouch *t in touches) {
        NSValue *key = [NSValue valueWithNonretainedObject:t];
        NSArray *f = _fingers[key];
        CGPoint p = [t locationInView:self];
        CGFloat dx, dy, dist;
        if (!f)
            continue;
        [_fingers removeObjectForKey:key];
        switch ([f[0] intValue]) {
        case ROLE_WHEEL:
            [self steer:0];
            _wheel.hidden = YES;
            break;
        case ROLE_BRAKE:
            if (![self has:ROLE_BRAKE]) {
                key_event(VK_A, 0);
                pedal_pressed(_brake, NO);
            }
            break;
        case ROLE_BOOST:
            if (![self has:ROLE_BOOST]) {
                key_event(VK_Y, 0);
                pedal_pressed(_boost, NO);
            }
            break;
        case ROLE_MENU:
            if (cancelled || _gestureFingers != 1)
                break;
            dx = p.x - [f[1] doubleValue];
            dy = p.y - [f[2] doubleValue];
            dist = hypot(dx, dy);
            if (dist <= k_tap_points && now - [f[3] doubleValue] <= k_tap_seconds) {
                key_pulse(VK_B);                                   /* confirm: B or START, as every menu takes */
                key_pulse(VK_START);
            } else if (dist >= k_swipe_points) {
                /* the content follows the finger */
                key_pulse(fabs(dx) >= fabs(dy) ? (dx < 0 ? VK_RIGHT : VK_LEFT) : (dy < 0 ? VK_DOWN : VK_UP));
            }
            break;
        default:
            break;
        }
    }
    [self gas];
    if (_fingers.count == 0 && !cancelled && !_gestureMoved && _gestureFingers == 2 &&
        now - _gestureBegan <= k_tap_seconds + 0.15)
        key_pulse(_driving ? VK_START : VK_Y);                     /* pause; in the menus, back */
}

- (void)reset
{
    for (NSValue *k in _fingers.allKeys) {
        switch ([_fingers[k][0] intValue]) {
        case ROLE_BRAKE: key_event(VK_A, 0); break;
        case ROLE_BOOST: key_event(VK_Y, 0); break;
        default: break;
        }
    }
    [_fingers removeAllObjects];
    [self steer:0];
    if (_gas) {                            /* the next screen refresh presses it again */
        _gas = NO;
        key_event(VK_B, 0);
    }
    _wheel.hidden = YES;
    pedal_pressed(_brake, NO);
    pedal_pressed(_boost, NO);
}
@end

/* ---- the view ------------------------------------------------------------------------ */
@interface PortView : UIView
@end

static PortView *s_view;
static PortTouch *s_controls;
static CAMetalLayer *s_layer;
static CALayer *s_pic;
static _Atomic int s_pw, s_ph;        /* the drawable's size in pixels */

@implementation PortView
+ (Class)layerClass { return [CAMetalLayer class]; }

- (void)layoutSubviews
{
    CGFloat sc = self.window.windowScene.screen.nativeScale ?: 1;
    CGSize b = self.bounds.size, want = CGSizeMake(floor(b.width * sc + 0.5), floor(b.height * sc + 0.5));
    [super layoutSubviews];
    self.contentScaleFactor = sc;
    if (want.width >= 1 && want.height >= 1 && !CGSizeEqualToSize(s_layer.drawableSize, want))
        s_layer.drawableSize = want;
    atomic_store(&s_pw, (int)want.width);
    atomic_store(&s_ph, (int)want.height);
}
@end

@interface PortViewController : UIViewController
@end

@implementation PortViewController
- (void)loadView
{
    s_view = [[PortView alloc] initWithFrame:CGRectZero];
    s_view.backgroundColor = [UIColor blackColor];
    s_layer = (CAMetalLayer *)s_view.layer;
    s_layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    s_layer.framebufferOnly = NO;     /* the renderer copies its frame in */
    s_layer.magnificationFilter = kCAFilterNearest;
    s_layer.opaque = YES;
    s_controls = [[PortTouch alloc] initWithFrame:CGRectZero];
    s_controls.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [s_view addSubview:s_controls];
    self.view = s_view;
}
- (void)viewDidLayoutSubviews
{
    [super viewDidLayoutSubviews];
    s_controls.frame = s_view.bounds;
}
- (BOOL)prefersStatusBarHidden { return YES; }
- (BOOL)prefersHomeIndicatorAutoHidden { return YES; }
- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures { return UIRectEdgeAll; }
- (UIInterfaceOrientationMask)supportedInterfaceOrientations { return UIInterfaceOrientationMaskLandscape; }
- (BOOL)canBecomeFirstResponder { return YES; }
- (void)viewDidAppear:(BOOL)animated
{
    [super viewDidAppear:animated];
    [self becomeFirstResponder];
}
- (void)pressesBegan:(NSSet<UIPress *> *)presses withEvent:(UIPressesEvent *)ev
{
    int any = 0;
    for (UIPress *p in presses)
        if (p.key) {
            kb_event((long)p.key.keyCode, 1);
            any = 1;
        }
    if (!any)
        [super pressesBegan:presses withEvent:ev];
}
- (void)pressesEnded:(NSSet<UIPress *> *)presses withEvent:(UIPressesEvent *)ev
{
    int any = 0;
    for (UIPress *p in presses)
        if (p.key) {
            kb_event((long)p.key.keyCode, 0);
            any = 1;
        }
    if (!any)
        [super pressesEnded:presses withEvent:ev];
}
- (void)pressesCancelled:(NSSet<UIPress *> *)presses withEvent:(UIPressesEvent *)ev
{
    [self pressesEnded:presses withEvent:ev];
}
@end

/* ---- the foreground: the game thread held while the app is away ------------------- */
static _Atomic int s_active;
static pthread_mutex_t s_pm = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_pc = PTHREAD_COND_INITIALIZER;
static int s_parked, s_started;

static void focus_event(int down)
{
    host_event e;
    memset(&e, 0, sizeof e);
    e.type = HOST_EV_FOCUS;
    e.down = down;
    qpush(&e);
}

static void *game_thread(void *p)
{
    static char name[] = "stuntracefx";
    char *argv[] = { name, NULL };
    (void)p;
    exit(port_main(1, argv));
}

static void app_active(void)
{
    pthread_mutex_lock(&s_pm);
    atomic_store(&s_active, 1);
    pthread_cond_broadcast(&s_pc);
    pthread_mutex_unlock(&s_pm);
    focus_event(1);
    if (!s_started) {
        pthread_attr_t a;
        pthread_t t;
        s_started = 1;
        pthread_attr_init(&a);
        pthread_attr_setstacksize(&a, 16u << 20);   /* the main thread's room, and more */
        pthread_create(&t, &a, game_thread, NULL);
        pthread_attr_destroy(&a);
    }
}

static void app_inactive(void)
{
    struct timespec until;
    int i;
    /* keys held go up, or they stay down for good */
    [s_controls reset];
    for (i = 0; i < 256; i++)
        kb_event(i, 0);
    focus_event(0);
    pthread_mutex_lock(&s_pm);
    atomic_store(&s_active, 0);
    /* the game thread parks at its next poll, within a frame; a stuck
       one is not waited for long */
    clock_gettime(CLOCK_REALTIME, &until);
    until.tv_nsec += 500000000L;
    if (until.tv_nsec >= 1000000000L) {
        until.tv_sec++;
        until.tv_nsec -= 1000000000L;
    }
    while (s_started && !s_parked)
        if (pthread_cond_timedwait(&s_pc, &s_pm, &until))
            break;
    pthread_mutex_unlock(&s_pm);
}

/* ---- the app and its scene ------------------------------------------------------------ */
@interface PortSceneDelegate : UIResponder <UIWindowSceneDelegate>
@property (strong, nonatomic) UIWindow *window;
@end

@implementation PortSceneDelegate
- (void)scene:(UIScene *)scene willConnectToSession:(UISceneSession *)session options:(UISceneConnectionOptions *)opt
{
    (void)session;
    (void)opt;
    self.window = [[UIWindow alloc] initWithWindowScene:(UIWindowScene *)scene];
    self.window.rootViewController = [PortViewController new];
    self.window.backgroundColor = [UIColor blackColor];
    [self.window makeKeyAndVisible];
}
- (void)sceneDidBecomeActive:(UIScene *)scene
{
    (void)scene;
    app_active();
    host_audio_resume();
}
- (void)sceneWillResignActive:(UIScene *)scene
{
    (void)scene;
    app_inactive();
}
@end

@interface PortAppDelegate : UIResponder <UIApplicationDelegate>
@end

@implementation PortAppDelegate
- (UISceneConfiguration *)application:(UIApplication *)app
    configurationForConnectingSceneSession:(UISceneSession *)session
                                   options:(UISceneConnectionOptions *)opt
{
    UISceneConfiguration *c = [[UISceneConfiguration alloc] initWithName:@"Default" sessionRole:session.role];
    (void)app;
    (void)opt;
    c.delegateClass = [PortSceneDelegate class];
    return c;
}
@end

int main(int argc, char **argv)
{
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([PortAppDelegate class]));
    }
}

/* ---- the window, from the game thread ------------------------------------------------ */
/* the scene made the view before the game thread started */
int host_window_open(int width, int height, const char *title)
{
    (void)width;
    (void)height;
    (void)title;
    return s_layer != nil;
}

void host_window_close(void) {}

int host_window_visible(void) { return s_layer && atomic_load(&s_active); }

/* the view's CAMetalLayer, for render/metal; its drawable is kept at the
   view's size in pixels on the main thread (PortView's layoutSubviews) */
CAMetalLayer *host_metal_layer(void) { return s_layer; }

void host_metal_layer_fit(int *w, int *h)
{
    *w = atomic_load(&s_pw);
    *h = atomic_load(&s_ph);
}

void *host_window_handle(void) { return (__bridge void *)s_layer; }
void host_window_pixels(int *w, int *h) { host_metal_layer_fit(w, h); }

/* a picture of ARGB pixels (0xAARRGGBB, top row first) onto the view, at
   the shape `aspect` (width over height), centred in the largest
   rectangle of that shape the view holds */
void host_present(const uint32_t *argb, int w, int h, float aspect)
{
    NSData *d;
    if (!s_view || !argb)
        return;
    if (aspect <= 0)
        aspect = (float)w / (float)h;
    d = [NSData dataWithBytes:argb length:(NSUInteger)w * (NSUInteger)h * 4];
    dispatch_async(dispatch_get_main_queue(), ^{
        CGColorSpaceRef rgb = CGColorSpaceCreateDeviceRGB();
        CGDataProviderRef dp = CGDataProviderCreateWithCFData((__bridge CFDataRef)d);
        CGImageRef img = CGImageCreate((size_t)w, (size_t)h, 8, 32, (size_t)w * 4, rgb,
                                       kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst, dp, NULL, false,
                                       kCGRenderingIntentDefault);
        CGRect b = s_view.bounds;
        CGFloat fw = b.size.width, fh = b.size.height;
        if (fh > 0 && fw / fh > aspect)
            fw = floor(fh * aspect + 0.5);
        else
            fh = floor(fw / aspect + 0.5);
        [CATransaction begin];
        [CATransaction setDisableActions:YES];
        if (!s_pic) {
            s_pic = [CALayer layer];
            s_pic.magnificationFilter = kCAFilterNearest;
            s_pic.contentsGravity = kCAGravityResize;
            [s_view.layer insertSublayer:s_pic atIndex:0];
        }
        s_pic.frame = CGRectMake(floor((b.size.width - fw) / 2), floor((b.size.height - fh) / 2), fw, fh);
        s_pic.contents = (__bridge id)img;
        [CATransaction commit];
        CGImageRelease(img);
        CGDataProviderRelease(dp);
        CGColorSpaceRelease(rgb);
    });
}

int host_poll_event(host_event *ev, uint32_t wait_ms)
{
    uint64_t until = host_ticks_ns() + (uint64_t)wait_ms * 1000000ull;
    int got;
    /* away from the foreground: held here until the app is back */
    if (!atomic_load(&s_active)) {
        pthread_mutex_lock(&s_pm);
        s_parked = 1;
        pthread_cond_broadcast(&s_pc);
        while (!atomic_load(&s_active))
            pthread_cond_wait(&s_pc, &s_pm);
        s_parked = 0;
        pthread_mutex_unlock(&s_pm);
    }
    for (;;) {
        os_unfair_lock_lock(&s_qlock);
        got = s_qh != s_qt;
        if (got) {
            *ev = s_q[s_qh];
            s_qh = (s_qh + 1) % QMAX;
        }
        os_unfair_lock_unlock(&s_qlock);
        if (got || wait_ms == 0 || (wait_ms != 0xFFFFFFFFu && host_ticks_ns() >= until))
            return got;
        host_sleep_ms(1);
    }
}

void host_message_box(const char *text, const char *caption)
{
    NSString *t = [NSString stringWithUTF8String:text ? text : ""];
    NSString *c = [NSString stringWithUTF8String:caption ? caption : s_app_title];
    dispatch_semaphore_t done = dispatch_semaphore_create(0);
    fprintf(stderr, "%s: %s\n", [c UTF8String], [t UTF8String]);
    if ([NSThread isMainThread] || !s_view)
        return;
    dispatch_async(dispatch_get_main_queue(), ^{
        UIAlertController *a = [UIAlertController alertControllerWithTitle:c message:t
                                                            preferredStyle:UIAlertControllerStyleAlert];
        [a addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault
                                            handler:^(UIAlertAction *x) { (void)x; dispatch_semaphore_signal(done); }]];
        [s_view.window.rootViewController presentViewController:a animated:YES completion:nil];
    });
    dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);
}

/* ---- audio out: a RemoteIO unit pulling the game's mixer --------------------------- */
static AudioComponentInstance s_unit;
static host_audio_fn s_afn;
static void *s_auser;

static OSStatus render(void *ref, AudioUnitRenderActionFlags *flags, const AudioTimeStamp *ts,
                       UInt32 bus, UInt32 frames, AudioBufferList *io)
{
    (void)ref; (void)flags; (void)ts; (void)bus;
    if (s_afn)
        s_afn((float *)io->mBuffers[0].mData, (int)frames, s_auser);
    else
        memset(io->mBuffers[0].mData, 0, io->mBuffers[0].mDataByteSize);
    return noErr;
}

int host_audio_open(int rate, host_audio_fn fn, void *user)
{
    AudioComponentDescription desc = { kAudioUnitType_Output, kAudioUnitSubType_RemoteIO,
                                       kAudioUnitManufacturer_Apple, 0, 0 };
    AudioStreamBasicDescription fmt;
    AURenderCallbackStruct cb = { render, NULL };
    AudioComponent comp;
    if (s_unit)
        return 1;
    @autoreleasepool {
        AVAudioSession *as = [AVAudioSession sharedInstance];
        /* the game's sound mixes with what else plays, and the silent
           switch silences it */
        [as setCategory:AVAudioSessionCategoryAmbient error:NULL];
        [as setPreferredSampleRate:rate error:NULL];
        [as setActive:YES error:NULL];
    }
    comp = AudioComponentFindNext(NULL, &desc);
    if (!comp || AudioComponentInstanceNew(comp, &s_unit) != noErr)
        return 0;
    memset(&fmt, 0, sizeof fmt);
    fmt.mSampleRate = rate;
    fmt.mFormatID = kAudioFormatLinearPCM;
    fmt.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    fmt.mFramesPerPacket = 1;
    fmt.mChannelsPerFrame = 2;
    fmt.mBitsPerChannel = 32;
    fmt.mBytesPerFrame = 8;
    fmt.mBytesPerPacket = 8;
    s_afn = fn;
    s_auser = user;
    if (AudioUnitSetProperty(s_unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0,
                             &fmt, sizeof fmt) != noErr ||
        AudioUnitSetProperty(s_unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0,
                             &cb, sizeof cb) != noErr ||
        AudioUnitInitialize(s_unit) != noErr || AudioOutputUnitStart(s_unit) != noErr) {
        AudioComponentInstanceDispose(s_unit);
        s_unit = NULL;
        return 0;
    }
    return 1;
}

/* (main thread) back in the foreground: an interruption (a call, Siri)
   may have stopped the unit */
static void host_audio_resume(void)
{
    if (s_unit) {
        [[AVAudioSession sharedInstance] setActive:YES error:NULL];
        AudioOutputUnitStart(s_unit);
    }
}

void host_audio_close(void)
{
    if (s_unit) {
        AudioOutputUnitStop(s_unit);
        AudioUnitUninitialize(s_unit);
        AudioComponentInstanceDispose(s_unit);
        s_unit = NULL;
    }
}

/* ---- game controllers ------------------------------------------------------------- */
/* any controller iOS knows (Xbox, PlayStation, MFi, Switch Pro), in the
 * order GameController lists those with a full gamepad profile */
int host_pad_read(int index, host_pad *o)
{
    @autoreleasepool {
        GCController *c = nil;
        GCExtendedGamepad *g;
        unsigned b = 0;
        int n = 0;
        memset(o, 0, sizeof *o);
        for (GCController *k in GCController.controllers)
            if (k.extendedGamepad && n++ == index) {
                c = k;
                break;
            }
        if (!c || !(g = c.extendedGamepad))
            return 0;
        o->x = g.leftThumbstick.xAxis.value;
        o->y = -g.leftThumbstick.yAxis.value;
        if (g.buttonA.pressed) b |= 1u << 0;
        if (g.buttonB.pressed) b |= 1u << 1;
        if (g.buttonX.pressed) b |= 1u << 2;
        if (g.buttonY.pressed) b |= 1u << 3;
        if (g.leftShoulder.pressed) b |= 1u << 4;
        if (g.rightShoulder.pressed) b |= 1u << 5;
        if (g.buttonOptions.pressed) b |= 1u << 6;
        if (g.buttonMenu.pressed) b |= 1u << 7;
        if (g.leftThumbstickButton.pressed) b |= 1u << 8;
        if (g.rightThumbstickButton.pressed) b |= 1u << 9;
        if (g.leftTrigger.value > 0.5f) b |= 1u << 10;
        if (g.rightTrigger.value > 0.5f) b |= 1u << 11;
        if (g.dpad.up.pressed) b |= 1u << 12;
        if (g.dpad.right.pressed) b |= 1u << 13;
        if (g.dpad.down.pressed) b |= 1u << 14;
        if (g.dpad.left.pressed) b |= 1u << 15;
        o->buttons = b;
        return 1;
    }
}
