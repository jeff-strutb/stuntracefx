# What the game needs from nativeshvc

Requests from the Stunt Race FX conversion to the engine's owner, through Jeff. Each is general, for every game, and
names no game. None has been made in the engine by the game's sessions.

## 1. Every native input, as true inputs (Jeff, 2026-11-09)

Jeff: "NativeSVHC should support all native inputs that it runs on, whether mouse, keyboard, or controller, and make those
available to the game as true inputs. If it doesn't now, it needs to."

Today (API 4.4) `nshvc_input` carries only SNES pad bits per player, two mice as SNES mouse deltas, and two light guns' aim
in screen pixels. A game cannot see:

- **The keyboard as keys.** Which key went down and up, as `nshvc_key`, for every key (arrows, Shift, Control, Escape,
  function keys), not only the letters, digits, space, backspace and enter that `nshvc_text_read` gives. Without it a game
  cannot offer "press the key you want" when remapping, though `nshvc_control_bind` takes an `nshvc_key`.
- **The pointer in window pixels.** Its position, its buttons and its wheel, and touches, as the host has them, for a
  game's own interface (a pause button, a panel), apart from an emulated SNES mouse or gun.
- **The controller as itself.** Its buttons and sticks as the host reports them, beside the SNES pad mapping.

## 2. Input while the game is paused

`nshvc_pause(1)` stops `update`, and input reaches the game only through `update`. Jeff: the game owns how it pauses, so the
game keeps updating and stops its own logic; this needs nothing from the engine. Listed so the engine's owner knows it was
considered.

## 3. Typing

- **Keys that type text, kept off the pad while typing.** Today every key that goes down is put in the text queue and
  also presses whatever pad button it is bound to. The default bindings put B, Y, A, X, L and R on Z, A, X, S, Q and W.
  So a game that turns on `nshvc_text_input(1)` to let the player type a name gets a pad button with each of those
  letters, and a button that confirms or backs out of the screen fires while the player types. A game cannot filter this
  itself, because the player may bind keys anywhere. Asked: while text input is on, keys that type text ('A'-'Z', digits,
  space, Backspace) go to the text queue only and press no pad button. Enter can keep pressing its button, so a typed
  name can be confirmed.

## 4. Binding a controller's buttons

Part of Jeff's direction 1: every native input reaches the game as a true input, and a player can rebind it.

API 4.5 binds keyboard keys only: `nshvc_control_bind(player, button, nshvc_key)`. The engine maps every controller to
the pad itself, and a game cannot change which controller button presses which pad button. A game's pause screen can let
the player rebind keys ("press the key you want", from `keys_pressed`), but not a controller's buttons.

A game cannot do it with 4.5. `nshvc_input.pad[]` merges the keyboard, every controller and the touch screen. The bits a
controller presses there look the same as a keyboard's, so a game cannot take the engine's controller mapping back out
and put its own in. It sees `controller[].buttons` beside the pad, but a pad bit that a key and a controller button both
press can't be split between them. Mapping every device itself, the game would have to repeat the engine's own mapping of
keys, controllers and touch, which the engine owns.

Asked, for every game:
- `nshvc_control_bind_controller(player, button, ctl)`: the controller button `ctl` (an `NSHVC_CTL_*`, by its place)
  presses `button` for that player's controllers, taking `ctl` from whichever button had it, as `nshvc_control_bind` does
  for keys.
- `nshvc_control_name(player, button)` names the controller button bound to `button` when the player's source is a
  controller, in that controller family's own terms (A, Cross, and so on).
- The bindings last until changed, and a game can put the engine's defaults back. The game keeps them in its own save,
  as it does its keys.

With that, a game's pause screen takes a controller button pressed (from `controller[].buttons`) for a SNES button the
way it takes a key now.

A touch screen's on-screen controls are the engine's, with nothing to rebind; nothing is asked for touch.

## 5. The game's icon on every platform (Jeff's bug, 2026-11-09)

Jeff: "Built app (mac at least, probably all platforms) doesn't use the app icon specified." The game now passes
`ICON icon/icon.png` (800 x 800, no alpha) to `nshvc_game()`. Today `cmake/NshvcGame.cmake` uses `ICON` only on Android
(one PNG in `res/mipmap`). On Apple it copies the file into `Resources` but sets no `MACOSX_BUNDLE_ICON_FILE`, so
Info.plist has no `CFBundleIconFile` and macOS shows the generic icon; iOS gets no asset catalog or `CFBundleIcons`;
Windows gets no resource script, so the window class finds no icon 1; Linux gets nothing.

Asked: `nshvc_game(... ICON <one square image>)` makes each platform's form at build time:
- macOS: an `.icns` (each size at 1x and 2x, via `sips` and `iconutil`) in Resources, named by `MACOSX_BUNDLE_ICON_FILE`.
- iOS: an asset catalog compiled by `actool`, its partial Info.plist merged (`CFBundleIcons`).
- Windows: an `.ico` (16 to 256) in a resource script linked into the program as the icon the window class loads.
- Linux: a PNG beside the program for a desktop file; Android as now.
A working recipe for all of these from one image is in the old port's `ports/stuntracefx/icon/icons.sh`.
