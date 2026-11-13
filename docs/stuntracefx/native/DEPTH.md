# Depth-tested 3D, done properly

Jeff, 2026-11-12, after the overhead view showed the start gantry under the
road: "I do want depth testing I just want it done correctly. It was half
assed before."

All rendering stays on the GPU. The CPU builds the display list with exact
vertices; the GPU's depth test decides what is in front, pixel by pixel, for
any camera (the game's chase view and the overhead view alike).

## Why the first one (0febd496 to b123e92f) fell short

Its own commits record the faults:

1. **The corners were the chip's rounded numbers.** Model points are rotated
   in fixed point. Command $04, the common one, uses a packed 8-bit matrix.
   The results are rounded to whole units, and far objects are halved on
   top of that. Faces that lie on one plane came out 0.4 to 3 units apart
   (e6750f27's dump of Sunset Valley's tunnel), so the depth test picked
   between them pixel by pixel.
2. **Every later fix was a tolerance on top of that error.** Planes counted
   as "the same" within 6 degrees and 3 units (or 0.2% of their distance).
   Layer levels counted overlaps, and a 0.2% polygon offset was added. Flicker
   was still left over (b123e92f: 3 pictures of 415 over the limit, seams at
   shared edges).
3. **One depth mapping for every picture.** Depth was 0.998 (1 - 32 iz) and
   the land sat at 0.999. Nothing was fitted to the picture's own nearest and
   farthest geometry.
4. **Two-sided parts were handled by offsetting back faces.** A two-sided
   part is two faces on the same corners, one facing each way, and a back
   face was simply pushed a step behind.
5. **Cars were seated by searching the picture.** Each picture, the
   up-facing faces under each tyre were searched and the car moved onto them
   (fx_seat). Other sprites lying under the road were also moved up.

## The design

### 1. Exact geometry

Every corner is computed in double precision from the source data:

- the model's own point: its bytes times its scale, as the point commands
  ($04, $38, $08, $4C, $34) read them;
- turned by the object's rotation, made in double from its angle bytes and
  their fractions between steps;
- plus its place from the eye: the draw record's whole place, 32 bits for a
  far one, never halved;
- turned by the view's rotation, made in double from the view's angles.

The chip's walk still decides which faces are drawn, in what order and with
what material. Only the corners' positions change. Solids and sprites take
their places the same way.

Result: faces on one plane in a model stay on one plane to double rounding,
about 1e-12 of their size. Faces on one plane across objects agree to the
rounding of the angles' sines, about 1e-9.

### 2. Depth with full precision (engine: its own depth buffer)

- Each corner carries its true view-space depth as w = Z, through the near
  plane's and the screen's clipping.
- The 3D draw list gets a depth buffer of its own: 32-bit float, reversed
  inside. It does not use the 1/16 slice of the frame's depth that a draw
  list shares today. The list declares its depth mapping once, and the GPU
  works each triangle's depth out from w by it, as it already does for an
  instanced mesh. A z handed over as a float near 1 could not carry the
  precision: its step is 6e-8, about 0.07 of a unit at 17,000.
- Worked out from w, one step is about Z / 8,000,000: 0.002 of a unit at
  the overhead's 17,000, and 0.0001 at 1,000 in the chase view. No
  per-picture fitting is needed. Tris and meshes in the list agree exactly.
- The land (its plane, mountains, forests, cities, clouds) is drawn first,
  tested against the depth but not writing it. The course then always lies
  over it, so the land never covers road that dips below its plane.
- The HUD, text and effects are drawn after the 3D at depth 0, their depth
  given as now (the per-draw switch for depth from w is off for them).

### 3. The game's own layering for faces on one plane (engine: depth bias)

The faces are still drawn in the game's order. A face is drawn one layer in
front of the highest earlier face it overlaps whose plane it shares (within
0.001 of a unit and 1e-6 of a turn, which exact geometry allows).

- A layer is the GPU's polygon offset, per draw: slope-scaled plus a
  constant, in the depth buffer's own units. This is what the GPU's depth
  bias is for. It stays right at grazing angles, where a fixed offset in
  depth (the first version's) is either too small or too large.
- Faces that look alike, such as a road cut into triangles, share a layer.
- There are no angle or distance tolerances.

### 4. Back faces

Back faces are culled per surface, not left to depth. A model face's own
corner order has no fixed winding (the chip decided facing from separate
facing triangles), so for these faces the CPU decides: it already has each
face's facing from the chip's facing triangle, now made from exact corners.
The GPU's cull mode is used for the instanced meshes the game builds itself,
wound clockwise on the screen as their front.

- **A closed model is drawn one-sided, its back faces culled.** A model is
  closed when every edge of its faces is shared by two faces going opposite
  ways. That is worked out once a model, exactly, from its point indices.
  The content's two-sided parts (two faces on the same corners, one facing
  each way) then show the side facing the eye, each twin culled by its own
  winding, with no offsets.
- **An open model is drawn two-sided.** These are the one-sided walls Jeff
  wanted seen from outside (2026-11-11: "renders polygons even if outside
  them"), such as tunnels and canyon walls. A face seen from behind is
  shaded by its own normal turned to the eye, as the chip shades a lit face.

The chip's walk now takes faces facing away as well, holding each one until
its model's walk is done; it still picks the walk's branches as before.

### 5. Cars

Measure first: a scratch dump of one picture's tyres, bodies and shadows
against the road faces under them. Then fix by what it shows.

The earlier measurement (2d4d4c3a) found tyre centres within 11 of the road,
bodies from 16 under to 2 over, and shadows 25 to 65 under. That points to:

- every car drawn one tyre radius (40.25) up along its own up axis;
- its shadow laid on the road face under it, as a layer of that face.

There is no per-picture surface search, and no other objects are moved.

### 6. Sprites, particles and water (engine: depth write off)

Sprites and particles go at their place's depth. See-through water is drawn
after the solid 3D, depth-tested but not written to the depth buffer.

## What the engine adds (nativeshvc, for Jeff to approve)

Every one of these is a standard GPU state on Metal, Vulkan and D3D12. Each
is a per-draw setting, so a list that sets none of them draws as it does
today. The engine's renderer session designs the shape and puts it to Jeff;
depth from w was its finding, after the first four were asked for.

| Setting | Values | What it is for |
|---|---|---|
| Cull mode | None, back, front | The game's instanced meshes one-sided |
| Depth bias | Constant plus slope-scaled, in the depth buffer's units | One layer for coplanar faces |
| Depth write | On, off | See-through water and other transparent draws |
| A list's own depth | 32-bit float, reversed, cleared each picture | Full precision for the 3D, in place of the shared 1/16 slice |
| Depth from w | A list's depth mapping; per draw, on or off | Triangles' depth worked out on the GPU from their true Z |

The list's own depth buffer is composited among the layers by its priority,
as now. The engine's session designs the shape; the game writes against it
only once it is on nativeshvc's main and tested on every backend.

## Size and reach

The game's side touches about 8 to 10 files, roughly 800 to 1,200 lines:

| File | What changes |
|---|---|
| fx_face.c | Exact corners; every face handed on, with its model's sidedness |
| model.c | Object-space points recorded at the point commands |
| fx_draw.c | w = Z per vertex, layers as depth bias, the land and water without depth writes |
| fx_solid.c | Exact places for solids |
| fx_ground.c | The land at its true depth, cut out under the course |
| frame.c | Sprites' depth |
| New, small file | Car seating |

The engine's side is its four settings above, in its own repository.

It covers every host: the engine's draw lists carry depth the same way on
Metal, Vulkan and D3D12. The game's chase view gets it too.

## Checking

Step 5's measurement is a run, asked for when reached. One run at the end
checks:

1. The overhead at the start: the gantry over the road, with tyres.
2. A lap each of Sunset Valley, Easy Ride and Aqua Tunnel, with the old
   flicker count: pixels where two faces of different colours lie within 2
   depth steps. The target is none away from faces' shared edges.
3. Tyres' distance from the road under them: 0 to 1.
