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

### 2. Depth with full precision (engine: its own depth buffer, 1 nearest)

- Each corner's depth is worked out by the game from its exact view-space
  Z, carried through the near plane's and the screen's clipping: z = near / Z,
  near being the game's own near-plane distance. That falls with distance
  and is linear across the screen, so the GPU interpolates it exactly.
- The 3D draw list gets a depth buffer of its own: 32-bit float, its depth
  order 1 nearest (drawn where greater or equal, cleared to 0). It does not
  use the 1/16 slice of the frame's depth that a draw list shares today.
- With 1 nearest and z = near / Z, a 32-bit float resolves about Z /
  8,000,000: 0.002 of a unit at the overhead's 17,000, and 0.0001 at 1,000
  in the chase view. Handed over the other way (0 nearest, z near 1) it
  could not: its step there is 6e-8, about 0.07 of a unit at 17,000.
- The ground (the plane at the course's level, and a stunt arena's floor)
  stands in for the original's ground band under the horizon. Its level is
  the median of where the course's shapes meet the ground, so much of the
  course lies below it. It has a range of depths of its own, z = 1e-7 near
  / Z, the way a depth range per pass works: all of it lies farther than
  anything of the 3D nearer than 10,000,000 units, so the course always
  lies over it, and its depths are as fine relative to their size as the
  course's.
- Everything else of the land is at the world's true depths, with the
  course: the mountains, mesas, the canyon's fill, the tunnels' rock, the
  trees, buildings, clouds and the sea bed. They stand outside the course,
  so they hide it only where they really stand in front of it, as from the
  overhead view's eye outside the course. They hide each other as they
  stand, and stand in front of the ground. Their corners are whole units,
  so their 1 / Z is taken as it is, whatever object was drawn before.
- Jeff approved "the land drawn first, tested but not written". It is
  changed because, so drawn, the land's parts could not hide each other.
  Only the ground needs to lie behind the course.
- The HUD and text are drawn at z = 1, the nearest. The race's art drawn
  before the 3D (the bezel) is at 0, the farthest; the panels drawn over it
  in the full window are at 1.

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
- A solid's discs on one plane (a tyre's side and its hub, an eye's marks)
  are each drawn one layer in front of the one under it.

### 4. Back faces

Back faces are culled per surface, not left to depth. A model face's own
corner order has no fixed winding (the chip decided facing from separate
facing triangles), so for these faces the CPU decides: it already has each
face's facing from the chip's facing triangle, now made from exact corners.
The GPU's cull mode is used for the instanced meshes the game builds itself
(the land's trees, buildings and clouds): their back faces culled, each
triangle wound clockwise on the screen seen from outside (the engine's
default front), worked out from its outward normal when the mesh is made.

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

### 5. Cars, seated exactly (Jeff, 2026-11-12: "Option 2")

Measured with exact corners on Easy Ride, Sunset Valley and Aqua Tunnel, on
the grid and at speed:

- tyre middles from 7 under to 9 over the road (radius 40.25);
- the body's lowest point 1 to 17 under it;
- the shadow, two faces tilted with the car, 31 to 66 under it, which is
  39 to 58 under the tyres' middles;
- the body's one level face is its roof, 130 over them.

The original hid all of it by painting the cars after the road.

With the depth test the cars are drawn after the course, each car's parts
together with its tyres first, so every face of the course under them is
known. A car is the object with tyres; an object belongs to the car its own
or its parents' chain reaches (no model is named).

- Each tyre is moved along the normal of the ground under its middle, to
  its radius over it. Its ground is the track the game drives the car on,
  never a face only drawn (Jeff, 2026-11-14: "treat only truly 3d objects
  as things the car drives over, and have the actual track itself be the
  base for the tires"): the collision models of the drawn pieces its car's
  record touches (record +$14, as FxCollideModel and FxRecordTouches use
  them), each a set of cells, a cell the half-spaces of its faces; the
  topmost up-facing surface whose cell holds the point straight over or
  under the middle, within 100 over and 100 under it (fx_track.c). Water,
  painted lines and decorations have no collision model and never lift a
  tyre; ramps and raised pieces do. With none, the tyre keeps the game's
  height. The drawn faces were the ground before: Aqua Tunnel's tube
  draws only two floor bands and a centre line a piece, so the car rose
  onto each band and fell a radius between them (Bug 6), and centre
  lines drawn flat over crests and dips lifted it 6 to 56.
- The car's other parts move by its tyres' mean move.
- A face of the car whose corners all lie no more than 0.3 of a tyre's
  radius over the ground under its middle (the shadow; never the body,
  whose lowest corner ends 25 over) is laid on that ground. It then lies
  in the road's plane, and the layers draw it over the road.

### 6. Sprites, particles and water (engine: depth write off)

Sprites and particles go at their object's depth: its exact place, else
the place in its draw record as the chip made it.

See-through water is drawn after the solid 3D and before the weather and
the effects, depth-tested but not written to the depth buffer. Where a
track piece goes below it (Aqua Tunnel's and Harbor City's), the region
the piece's crossings enclose on the water's plane is a hole: each water
triangle is drawn less each hole, in convex pieces cut along the hole's
edges. A crossing is worked out from its edge's ends in one order, so the
pieces meet exactly. The old cut, which drew what lies past the water
first and again inside each hole, is gone, with every other piece drawn
only without the depth test: one picture's 3D is drawn one way.

## What the engine adds (nativeshvc, for Jeff to approve)

Every one of these is a standard GPU state on Metal, Vulkan and D3D12. Each
is a per-draw setting, so a list that sets none of them draws as it does
today. The engine knows nothing of the game (Jeff, 2026-11-12: "I don't
want the engine knowing anything about a game. It should implement an
engine properly, you should implement the game properly"): the engine's
renderer session designs each setting as a general engine would and puts it
to Jeff, and the game works out its own depths.

| Setting | Values | What it is for |
|---|---|---|
| Cull mode | None, back, front | The game's instanced meshes one-sided |
| Depth bias | Constant plus slope-scaled, in the depth buffer's units | One layer for coplanar faces |
| Depth write | On, off | See-through water and other transparent draws |
| A list's own depth | 32-bit float, cleared each picture, its order 0 or 1 nearest | Full precision for the 3D, in place of the shared 1/16 slice |

The list's own depth buffer is composited among the layers by its priority,
as now. The engine's session designs the shape; the game writes against it
only once it is on nativeshvc's main and tested on every backend.

## Size and reach

The game's side touches about 8 to 10 files, roughly 800 to 1,200 lines:

| File | What changes |
|---|---|
| fx_face.c | Exact corners; every face handed on, with its model's sidedness |
| model.c | Object-space points recorded at the point commands |
| fx_draw.c | z = near / Z per vertex, layers as depth bias, the land and water without depth writes |
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
