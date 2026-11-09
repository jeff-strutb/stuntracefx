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

## 3. Typing and a layer past the original picture

- **Keys that type text, kept off the pad while typing.** Today every key that goes down is put in the text queue and
  also presses whatever pad button it is bound to. The default bindings put B, Y, A, X, L and R on Z, A, X, S, Q and W.
  So a game that turns on `nshvc_text_input(1)` to let the player type a name gets a pad button with each of those
  letters, and a button that confirms or backs out of the screen fires while the player types. A game cannot filter this
  itself, because the player may bind keys anywhere. Asked: while text input is on, keys that type text ('A'-'Z', digits,
  space, Backspace) go to the text queue only and press no pad button. Enter can keep pressing its button, so a typed
  name can be confirmed.
- **A layer's own map outside the original picture.** On a screen larger than the original's 256 x 224, a tile layer
  shows its own map wrapped beyond that picture. That is right for a texture, but not for a map that also holds the
  screen's contents: a hole cut for another layer's picture, panels waiting beside the shown screen for a slide, a
  frame. These repeat into the margins, so a game cannot fill a wider window with its menu's background texture
  while keeping its contents to the original picture. Asked: a second map per layer, read only outside the original
  picture at the layer's own scroll and wrapped, e.g. `nshvc_map outside` in `nshvc_layer`, where an empty map keeps
  today's behaviour. 2.x had this as `nshvc_ext_layer_outside`.
