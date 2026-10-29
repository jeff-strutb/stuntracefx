/* main.m: the release builder's macOS front end (AppKit).
 *
 *   1 what to build for: the Mac, or the iPhone and iPad (signed with the
 *     player's own Xcode signing when this Mac has it, else an unsigned .ipa)
 *   2 the player's own ROM, checked by MD5, and the folder to build into
 *   3 the build (core/build.c), with progress
 *   4 the result: the app or .ipa, or what went wrong
 *
 * Arguments fill in the choices for a scripted run, and --build starts it:
 *   --target mac|ios  --dest DIR  --rom FILE  --build
 * RB_SNAPSHOT=DIR writes each page the window shows to DIR as a PNG.
 */
#import <Cocoa/Cocoa.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include "rb.h"

/* the pieces in Resources/payload */
int rb_host_payload(const char *name, char *path, size_t n, char *err, size_t errlen)
{
    @autoreleasepool {
        static NSDictionary<NSString *, NSString *> *map;
        if (!map)
            map = @{ @"game": @"stuntracefx", @"icon": @"AppIcon.icns", @"ios-app": @"ios/app", @"ios-sign": @"ios_sign.sh" };
        NSString *rel = map[@(name)];
        NSString *src = rel ? [[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:
                                  [@"payload" stringByAppendingPathComponent:rel]] : nil;
        if (!src || ![NSFileManager.defaultManager fileExistsAtPath:src]) {
            snprintf(err, errlen, "the builder is missing its copy of the game (%s); download it again", name);
            return 0;
        }
        snprintf(path, n, "%s", src.fileSystemRepresentation);
        return 1;
    }
}

@interface Builder : NSObject <NSApplicationDelegate, NSWindowDelegate>
@property (strong) NSWindow *window;
@property int target;
@property (copy) NSString *signing;          /* the iPhone build's signing, nil: unsigned */
@property BOOL destChosen;                    /* else the target's own: Applications, or the Desktop for an .ipa */
@property BOOL buildNow;                      /* --build: a scripted run */
@property (copy) NSString *dest, *rom;
@property BOOL romOK;
@property (strong) NSTextField *destLabel, *romRow;
@property (strong) NSButton *buildButton;
@property (strong) NSProgressIndicator *bar;
@property (strong) NSTextField *status;
@end

static volatile int g_cancel;
static atomic_int g_checks;      /* hashes still running */

static NSTextField *label(NSString *s, CGFloat size, BOOL bold)
{
    NSTextField *t = [NSTextField wrappingLabelWithString:s];
    t.font = bold ? [NSFont boldSystemFontOfSize:size] : [NSFont systemFontOfSize:size];
    t.selectable = YES;
    return t;
}

static NSTextField *mono(NSString *s)
{
    NSTextField *t = [NSTextField labelWithString:s];
    t.font = [NSFont monospacedSystemFontOfSize:11 weight:NSFontWeightRegular];
    t.textColor = NSColor.secondaryLabelColor;
    t.selectable = YES;
    return t;
}

static NSStackView *vstack(NSArray<NSView *> *views, CGFloat spacing)
{
    NSStackView *s = [NSStackView stackViewWithViews:views];
    s.orientation = NSUserInterfaceLayoutOrientationVertical;
    s.alignment = NSLayoutAttributeLeading;
    s.spacing = spacing;
    return s;
}

static NSStackView *hstack(NSArray<NSView *> *views)
{
    NSStackView *s = [NSStackView stackViewWithViews:views];
    s.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    s.spacing = 8;
    return s;
}

static void progress_cb(void *ctx, double f, const char *s)
{
    Builder *b = (__bridge Builder *)ctx;
    NSString *str = s ? @(s) : nil;
    dispatch_async(dispatch_get_main_queue(), ^{
        b.bar.doubleValue = f * 100;
        if (str)
            b.status.stringValue = str;
    });
}

@implementation Builder

- (void)applicationDidFinishLaunching:(NSNotification *)n
{
    NSMenu *bar = [NSMenu new], *app = [NSMenu new];
    NSMenuItem *item = [NSMenuItem new];
    [app addItemWithTitle:@"Quit " RB_BUILDER action:@selector(terminate:) keyEquivalent:@"q"];
    item.submenu = app;
    [bar addItem:item];
    NSApp.mainMenu = bar;

    self.target = RB_MACOS;
    self.dest = [NSSearchPathForDirectoriesInDomains(NSApplicationDirectory, NSUserDomainMask, YES) firstObject];
    {
        NSArray<NSString *> *a = NSProcessInfo.processInfo.arguments;
        NSUInteger i;
        for (i = 1; i < a.count; i++) {
            NSString *k = a[i], *v = i + 1 < a.count ? a[i + 1] : @"";
            if ([k isEqualToString:@"--build"]) { self.buildNow = YES; continue; }
            if ([k isEqualToString:@"--target"]) self.target = [v isEqualToString:@"ios"] ? RB_IOS : RB_MACOS;
            else if ([k isEqualToString:@"--dest"]) { self.dest = v; self.destChosen = YES; }
            else if ([k isEqualToString:@"--rom"]) self.rom = v;
            else continue;
            i++;
        }
    }
    if (self.target == RB_IOS && !self.destChosen)
        self.dest = [NSSearchPathForDirectoriesInDomains(NSDesktopDirectory, NSUserDomainMask, YES) firstObject];
    {
        char d[512];
        self.signing = rb_ios_signing(d, sizeof d) ? @(d) : nil;
    }
    self.window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 620, 520)
                                              styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable
                                                backing:NSBackingStoreBuffered defer:NO];
    self.window.title = @RB_BUILDER;
    self.window.delegate = self;
    [self.window center];
    if (self.buildNow) {
        rb_check c;
        self.romOK = self.rom && rb_check_rom(self.rom.fileSystemRepresentation, &c, NULL, NULL, NULL);
        [self toRom:nil];
        if (self.romOK)
            [self startBuild];
    } else {
        [self showTarget];
    }
    [self.window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)a { return YES; }

- (void)setPage:(NSView *)content buttons:(NSArray<NSButton *> *)buttons
{
    NSView *root = [NSView new];
    NSStackView *bb = hstack(buttons);
    content.translatesAutoresizingMaskIntoConstraints = NO;
    bb.translatesAutoresizingMaskIntoConstraints = NO;
    [root addSubview:content];
    [root addSubview:bb];
    [NSLayoutConstraint activateConstraints:@[
        [content.topAnchor constraintEqualToAnchor:root.topAnchor constant:24],
        [content.leadingAnchor constraintEqualToAnchor:root.leadingAnchor constant:28],
        [content.trailingAnchor constraintEqualToAnchor:root.trailingAnchor constant:-28],
        [bb.trailingAnchor constraintEqualToAnchor:root.trailingAnchor constant:-20],
        [bb.bottomAnchor constraintEqualToAnchor:root.bottomAnchor constant:-20],
        [content.bottomAnchor constraintLessThanOrEqualToAnchor:bb.topAnchor constant:-16],
    ]];
    self.window.contentView = root;
    [self snapshot];
}

- (void)snapshot
{
    static int n;
    NSString *dir = NSProcessInfo.processInfo.environment[@"RB_SNAPSHOT"];
    if (!dir)
        return;
    int k = ++n;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 300 * NSEC_PER_MSEC), dispatch_get_main_queue(), ^{
        NSView *v = self.window.contentView;
        NSRect r = v.bounds;
        NSImage *pdf = [[NSImage alloc] initWithData:[v dataWithPDFInsideRect:r]];
        NSImage *img = [[NSImage alloc] initWithSize:r.size];
        [v.effectiveAppearance performAsCurrentDrawingAppearance:^{
            [img lockFocus];
            [NSColor.windowBackgroundColor setFill];
            NSRectFill(r);
            [pdf drawInRect:r];
            [img unlockFocus];
        }];
        NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initWithData:img.TIFFRepresentation];
        [[rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}]
            writeToFile:[dir stringByAppendingPathComponent:[NSString stringWithFormat:@"page%02d.png", k]] atomically:YES];
    });
}

- (NSButton *)button:(NSString *)t action:(SEL)a
{
    NSButton *b = [NSButton buttonWithTitle:t target:self action:a];
    return b;
}

/* ---- 1: what to build for ------------------------------------------------------------------------ */
- (void)showTarget
{
    NSButton *mac = [NSButton radioButtonWithTitle:@"Mac" target:self action:@selector(pickTarget:)];
    NSButton *ios = [NSButton radioButtonWithTitle:@"iPhone and iPad (an .ipa)" target:self action:@selector(pickTarget:)];
    NSTextField *macWhat = label(@"An app for this Mac (macOS 12 or later, Apple silicon and Intel), drawn with Metal; "
                                 @"a keyboard or a game controller.", 12, NO);
    NSTextField *iosWhat = label(@"An app for the iPhone and iPad (iOS 15 or later), played by touch or with a game controller; "
                                 @"the title's PUSH START reads TAP TO START.", 12, NO);
    NSTextField *sign;
    mac.tag = RB_MACOS;
    ios.tag = RB_IOS;
    (self.target == RB_IOS ? ios : mac).state = NSControlStateValueOn;
    macWhat.textColor = iosWhat.textColor = NSColor.secondaryLabelColor;
    if (self.signing) {
        sign = label([NSString stringWithFormat:@"The iPhone build is signed with your Xcode signing (%@), ready to install on "
                                                @"your iPhone or iPad.", self.signing], 12, NO);
        sign.textColor = NSColor.secondaryLabelColor;
    } else {
        sign = label(@"No Xcode signing certificate with a provisioning profile was found on this Mac, so the iPhone build "
                     @"will be an unsigned .ipa: you sign and install it yourself (for example with AltStore or Sideloadly).",
                     12, NO);
        sign.textColor = NSColor.systemRedColor;
    }
    for (NSTextField *t in @[ macWhat, iosWhat, sign ])
        [t.widthAnchor constraintEqualToConstant:520].active = YES;
    NSButton *next = [self button:@"Continue" action:@selector(toRom:)];
    next.keyEquivalent = @"\r";
    [self setPage:vstack(@[
        label(@RB_BUILDER, 20, YES),
        label(@RB_BUILDER " makes a native build of Stunt Race FX (Super Nintendo, 1994) from your own copy of it: "
              @"the decompilation's C, compiled for your machine. Choose what to build it for.", 13, NO),
        vstack(@[ mac, macWhat ], 2),
        vstack(@[ ios, iosWhat ], 2),
        sign,
    ], 14) buttons:@[ next ]];
}

- (void)pickTargetValue:(int)t
{
    self.target = t;
    if (!self.destChosen)
        self.dest = [NSSearchPathForDirectoriesInDomains(t == RB_IOS ? NSDesktopDirectory : NSApplicationDirectory,
                                                         NSUserDomainMask, YES) firstObject];
}

- (void)pickTarget:(NSButton *)b { [self pickTargetValue:(int)b.tag]; }

- (void)chooseDest:(id)s
{
    NSOpenPanel *p = [NSOpenPanel openPanel];
    p.canChooseDirectories = YES;
    p.canChooseFiles = NO;
    p.canCreateDirectories = YES;
    p.prompt = @"Choose";
    p.directoryURL = [NSURL fileURLWithPath:self.dest];
    [p beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse r) {
        if (r == NSModalResponseOK) {
            self.dest = p.URL.path;
            self.destChosen = YES;
            self.destLabel.stringValue = self.dest;
        }
    }];
}

/* ---- 2: the player's ROM -------------------------------------------------------------------- */
- (void)toRom:(id)s
{
    NSButton *b = [self button:@"Choose..." action:@selector(chooseFile:)];
    self.romRow = mono(@"No file chosen");
    self.romRow.lineBreakMode = NSLineBreakByTruncatingMiddle;
    self.destLabel = mono(self.dest);
    self.destLabel.lineBreakMode = NSLineBreakByTruncatingMiddle;
    [self.destLabel setContentCompressionResistancePriority:NSLayoutPriorityDefaultLow forOrientation:NSLayoutConstraintOrientationHorizontal];
    NSButton *back = [self button:@"Back" action:@selector(backToTarget:)];
    self.buildButton = [self button:@"Build" action:@selector(build:)];
    self.buildButton.keyEquivalent = @"\r";
    [self setPage:vstack(@[
        label(@"Your copy of the game", 20, YES),
        label(@"The game's data is copyrighted, so the builder cannot include it. Provide a ROM of the Stunt Race FX "
              @"cartridge: the No-Intro dump of " RB_ROM_NAME " (.sfc; a .smc with a copier's header is accepted). "
              @"The build takes the game's data from it; the ROM's code is not used.", 13, NO),
        vstack(@[ hstack(@[ label(@"Stunt Race FX: cartridge ROM", 13, YES), b ]),
                  mono(@"Expected MD5  " RB_ROM_MD5), self.romRow ], 4),
        vstack(@[ label(@"Build into:", 13, YES),
                  hstack(@[ self.destLabel, [self button:@"Choose..." action:@selector(chooseDest:)] ]) ], 4),
    ], 14) buttons:@[ back, self.buildButton ]];
    if (self.rom)
        [self check:self.rom];
    [self updateBuildButton];
}

- (void)backToTarget:(id)s { [self showTarget]; }

- (void)updateBuildButton
{
    self.buildButton.enabled = self.romOK && atomic_load(&g_checks) == 0;
}

- (void)chooseFile:(NSButton *)b
{
    NSOpenPanel *p = [NSOpenPanel openPanel];
    p.canChooseFiles = YES;
    p.canChooseDirectories = NO;
    [p beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse r) {
        if (r == NSModalResponseOK)
            [self check:p.URL.path];
    }];
}

- (void)check:(NSString *)path
{
    NSTextField *row = self.romRow;
    NSString *name = path.lastPathComponent;
    self.rom = path;
    self.romOK = NO;
    row.stringValue = [NSString stringWithFormat:@"%@: checking...", name];
    row.textColor = NSColor.secondaryLabelColor;
    atomic_fetch_add(&g_checks, 1);
    [self updateBuildButton];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        rb_check cc;
        rb_check_rom(path.fileSystemRepresentation, &cc, NULL, NULL, NULL);
        dispatch_async(dispatch_get_main_queue(), ^{
            atomic_fetch_sub(&g_checks, 1);
            if ([self.rom isEqualToString:path]) {
                NSString *verdict = cc.ok ? @"✓ matches" : @"✗ does not match";
                NSString *note = cc.note[0] ? [NSString stringWithFormat:@"\n%s", cc.note] : @"";
                row.stringValue = [NSString stringWithFormat:@"%@\nYour file's MD5  %s  %@%@", name, cc.md5, verdict, note];
                row.textColor = cc.ok ? NSColor.systemGreenColor : NSColor.systemRedColor;
                self.romOK = cc.ok;
            }
            [self updateBuildButton];
            [self snapshot];
        });
    });
}

/* ---- 3: the build ------------------------------------------------------------------------------ */
- (void)build:(id)s
{
    rb_job job = { 0 };
    int ours = 0;
    job.target = self.target;
    job.dest_dir = self.dest.fileSystemRepresentation;
    if (rb_output_exists(&job, &ours)) {
        char out[2048];
        rb_output_path(&job, out, sizeof out);
        NSAlert *a = [NSAlert new];
        if (!ours) {
            a.messageText = [NSString stringWithFormat:@"%s already exists", rb_output_name(self.target)];
            a.informativeText = [NSString stringWithFormat:@"%s was not made by " RB_BUILDER ", so it will not be replaced. "
                                                           @"Move it away or choose another folder.", out];
            [a beginSheetModalForWindow:self.window completionHandler:nil];
            return;
        }
        a.messageText = [NSString stringWithFormat:@"Replace %s?", rb_output_name(self.target)];
        a.informativeText = [NSString stringWithFormat:@"An earlier build is at %s. Your records and settings are kept; they "
                                                       @"live in your Library (on the iPhone, with the app's data), not in the build.", out];
        [a addButtonWithTitle:@"Replace"];
        [a addButtonWithTitle:@"Cancel"];
        [a beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse r) {
            if (r == NSAlertFirstButtonReturn)
                [self startBuild];
        }];
        return;
    }
    [self startBuild];
}

- (void)startBuild
{
    NSButton *cancel = [self button:@"Cancel" action:@selector(cancelBuild:)];
    self.bar = [NSProgressIndicator new];
    self.bar.indeterminate = NO;
    self.bar.minValue = 0;
    self.bar.maxValue = 100;
    [self.bar.widthAnchor constraintEqualToConstant:540].active = YES;
    self.status = label(@"Starting", 13, NO);
    [self setPage:vstack(@[ label([NSString stringWithFormat:@"Building " RB_PRODUCT " for the %s", rb_target_name(self.target)], 20, YES),
                            self.bar, self.status ], 14) buttons:@[ cancel ]];
    g_cancel = 0;
    int target = self.target;
    NSString *dest = self.dest, *rom = self.rom;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        rb_job job = { 0 };
        char err[1024] = "", out[2048];
        /* the paths copied: fileSystemRepresentation's are autoreleased, and a
           build can wait minutes (a keychain prompt when signing) */
        char *d = strdup(dest.fileSystemRepresentation), *r = strdup(rom.fileSystemRepresentation);
        job.target = target;
        job.dest_dir = d;
        job.rom = r;
        int ok = d && r && rb_build(&job, progress_cb, (__bridge void *)self, &g_cancel, err, sizeof err);
        if (d)
            rb_output_path(&job, out, sizeof out);
        free(d);
        free(r);
        NSString *e = @(err), *o = @(out);
        dispatch_async(dispatch_get_main_queue(), ^{ [self showResult:ok error:e output:o]; });
    });
}

- (void)cancelBuild:(NSButton *)b
{
    g_cancel = 1;
    b.enabled = NO;
    self.status.stringValue = @"Stopping...";
}

/* ---- 4: the result ------------------------------------------------------------------------------ */
- (void)showResult:(int)ok error:(NSString *)err output:(NSString *)out
{
    if (self.buildNow)                  /* a scripted run: its result on stdout, its status the exit code */
        printf("%s: %s\n", ok ? "built" : "failed", (ok ? out : err).UTF8String);
    if (ok) {
        NSButton *show = [self button:@"Show in Finder" action:@selector(reveal:)];
        NSButton *done = [self button:@"Done" action:@selector(terminate:)];
        NSString *how;
        NSArray<NSButton *> *buttons;
        show.identifier = out;
        if (self.target == RB_IOS) {
            how = self.signing ? @"It is signed with your Xcode signing. Install it on your iPhone or iPad from Xcode (Window, Devices and Simulators: "
                                 @"drag it onto the device's installed apps) or Apple Configurator."
                               : @"It is not signed: sign and install it yourself, for example with AltStore or Sideloadly.";
            show.keyEquivalent = @"\r";
            buttons = @[ done, show ];
        } else {
            NSButton *play = [self button:@"Open the Game" action:@selector(play:)];
            play.identifier = out;
            play.keyEquivalent = @"\r";
            how = @"View, Enter Full Screen (Control-Command-F) fills the screen. Tab switches between Original and Enhanced "
                  @"mode, F1 sets the frame rate to 60, F2 turns Widescreen on and off.";
            buttons = @[ done, show, play ];
        }
        [self setPage:vstack(@[ label(@RB_PRODUCT " is ready", 20, YES),
                                label([NSString stringWithFormat:@"The build is complete. It carries everything it needs, so your "
                                                                 @"ROM is no longer required to play. %@", how], 13, NO),
                                mono(out) ], 14) buttons:buttons];
    } else {
        NSButton *back = [self button:@"Back" action:@selector(toRom:)];
        NSButton *quit = [self button:@"Quit" action:@selector(terminate:)];
        NSTextField *msg = label(err.length ? err : @"The build failed.", 13, NO);
        msg.textColor = NSColor.systemRedColor;
        [self setPage:vstack(@[ label(@"The build did not finish", 20, YES), msg ], 14) buttons:@[ quit, back ]];
    }
    if (self.buildNow)
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 600 * NSEC_PER_MSEC), dispatch_get_main_queue(), ^{ exit(ok ? 0 : 1); });
}

- (void)reveal:(NSButton *)b { [NSWorkspace.sharedWorkspace activateFileViewerSelectingURLs:@[ [NSURL fileURLWithPath:b.identifier] ]]; }

- (void)play:(NSButton *)b
{
    [NSWorkspace.sharedWorkspace openApplicationAtURL:[NSURL fileURLWithPath:b.identifier]
                                        configuration:[NSWorkspaceOpenConfiguration configuration]
                                    completionHandler:nil];
}

- (void)terminate:(id)s { [NSApp terminate:nil]; }

@end

int main(int argc, const char **argv)
{
    (void)argc;
    (void)argv;
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        Builder *b = [Builder new];
        app.activationPolicy = NSApplicationActivationPolicyRegular;
        app.delegate = b;
        [app run];
    }
    return 0;
}
