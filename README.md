# DoomClone

DoomClone is a Doom-style engine prototype in C++23 centered on classic 
software-rendering algorithms: BSP partitioning/traversal, wall clipping,
visplane span rendering, fixed-point arithmetic, and WAD-like lump serialization.
A lot of algorithms such as wall clipping, rendering engine principles were taken 
from the book by Fabien Sanglard "Game Engine Black Book - Doom".
## Build and Run

Requirements:
- CMake 3.16+
- C++23 compiler
- SDL2 development libraries
- Sidenote: The project is developed mostly for unix-based systems, so problems can emerge when compiling/launching on a windows machine

```bash
cmake -S . -B build
cmake --build build -j
./build/DoomClone
```

Runtime modes in `src/main.cpp`:
- `F1`: `GAMEPLAY_3D`
- `F2`: `EDITOR_2D`
- `F3`: `BSP_VIEWER`

## Algorithm Overview

The renderer and content pipeline are built around these algorithm groups:
- BSP tree construction from linedefs.
- BSP-ordered runtime visibility traversal.
- Wall clipping against accumulated occlusion intervals.
- Visplane aggregation and horizontal span rasterization.
- Fixed-point numeric pipeline (`Q16.16`) for geometry/simulation/rendering.
- Lump-directory-based serialization and full-file rewrite semantics.

## BSP Construction Algorithm

Implementation: `src/bsp/bsp.cpp` (`BSPBuilder`).

### 1) Seed segment set

Input linedefs are converted to `Seg` primitives. For two-sided linedefs, a reversed segment is also created so both directions are represented in classification and recursion.

### 2) Splitter selection

`SelectSplittingLine` evaluates each candidate with `EvaluateSplitter`.

For each candidate, all segments are classified as `FRONT`, `BACK`, or `SPANNING`. The score is:

`abs(left - right) + spanning * 8`

Degenerate splitters that leave one side empty are heavily penalized to avoid recursion stalls.

### 3) Segment-side classification

`DetermineSegmentPosition` uses signed cross products against the splitter direction:
- both endpoints on one side -> front/back,
- opposite signs -> potential spanning,
- zero/zero -> collinear, resolved by direction alignment (dot product).

For spanning candidates, a line-intersection solve validates whether the crossing lies within the segment interval before splitting.

### 4) Segment splitting

`SplitBySplitter`:
- computes intersection,
- inserts a new vertex,
- creates two replacement linedefs/segs,
- pushes resulting pieces into front/back sets based on classification.

### 5) Recursive partition

`BuildBSPTree(std::vector<Seg>&)` recurses until:
- segment list is empty, or
- region is convex / trivially small (`IsConvex` / size guard).

Leafs are emitted as `SubSector`; internal nodes store partition line (`x`, `y`, `dx`, `dy`) and child indices.

### 6) Leaf encoding

Subsector references are encoded with high bit `NF_SUBSECTOR`, enabling fast runtime "node vs leaf" checks without additional type structures.

## Runtime BSP Traversal Algorithm

Implementation: `RenderBSPNode` in `src/renderer/game/renderer.cpp`.

Per node:
1. Determine player side of partition (`PointOnSide`).
2. Recurse into near child first.
3. Recurse into far child second.

This near-first ordering is critical because it lets wall clipping consume nearer occluders before distant geometry is processed.

## Wall Clipping and Column Rasterization

Implementation: `RenderSeg`, `ClipSolidWall`, `ClipPassWall`, `StoreWallRange`, `RenderSegLoop` in `src/renderer/game/renderer.cpp`.

### Projection and FOV clipping

`RenderSeg` converts segment endpoints to view angles, clips to FOV, and maps to screen columns using precomputed lookup tables (`viewAngleToX`, `xToViewAngle`).

### Interval-occlusion clipping

The renderer maintains `m_solidSegs`, an ordered list of occluded x-intervals.

- `ClipSolidWall`: merges the new interval into the occlusion list and renders only newly visible fragments.
- `ClipPassWall`: clips against current occlusion but does not merge as fully solid, preserving portal visibility.

This interval-based clipping reduces overdraw and acts as the core visibility filter in the wall pass.

### Column drawing and portal slices

`StoreWallRange` computes per-range projection scale (`ScaleFromGlobalAngle`) and initializes a `drawseg_t` state. `RenderSegLoop` then iterates columns:
- computes top/bottom wall pixel bounds,
- handles upper/lower portal slices for two-sided walls,
- updates per-column clip arrays (`m_ceilClip`, `m_floorClip`).

These clip arrays are later reused by floor/ceiling processing.

## Visplane and Span Rendering Algorithm

Implementation: `src/renderer/game/renderer_plane.cpp`.

### 1) Plane bucketing

`FindVisPlane` groups floor/ceiling contributions by `(height, textureIndex, lightLevel)`. Matching properties reuse a plane bucket.

### 2) Overlap-safe range handling

`CheckVisPlane` decides whether the current x-range can extend an existing visplane or requires allocating a new visplane when overlap constraints are violated.

### 3) Span extraction

For each visplane, `RenderVisPlanes` scans from `minX` to `maxX + 1`. `MakeSpans` compares previous/current top-bottom bounds and opens/closes horizontal spans.

### 4) Texture-space mapping

`MapPlane` computes:
- distance to row using `ySlope`,
- per-pixel stepping (`xStep`, `yStep`),
- initial texture fractions (`xFrac`, `yFrac`).

`DrawSpan` samples indexed flat textures and resolves final RGB through `PaletteManager`.

## Fixed-Point Math Algorithm

Implementation: `src/core/fixed_math.cpp`.

Numeric model: `fixed_t` in `Q16.16` (`FRAC_BITS = 16`).

- `FixedMul(a, b)`: 64-bit intermediate multiply, right-shift by 16.
- `FixedDiv(a, b)`: overflow-guarded divide; saturates to `INT_MIN`/`INT_MAX` on dangerous ratios, otherwise delegates to `FixedDiv2`.
- `DoubleToFixed`: explicit NaN/inf handling, saturation, and rounded conversion to fixed-point units.

This keeps arithmetic deterministic and stable in hot loops (movement, projection, clipping, plane stepping).

## WAD Serialization Algorithm

Implementation: `src/core/serialization/wad_serializer.cpp` and `src/renderer/editor/editor_serialization.cpp`.

### File model

WAD data is represented as:
- `header` (`magicNumber`, `numDirectories`, `directoryOffset`),
- `directoryEntry` (`offset`, `size`, `name[8]`),
- lump payload bytes.

### Load path

`WadSerializer::Load`:
1. open file,
2. read header,
3. read directory,
4. optionally read all lump payloads (`LoadAllLumps`).

`ReadLump` performs random-access read by jumping to `entry.offset` and copying `entry.size` bytes.

### Write path (full rewrite)

`WriteLumpsToPath` performs deterministic rewrite:
1. write provisional header,
2. write lump payloads sequentially and capture actual offsets/sizes,
3. write directory table,
4. patch final header (`numDirectories`, `directoryOffset`) at file start.

This avoids in-place fragmentation and guarantees directory consistency with current payload layout.

### Level save algorithm

`EditorLevel::Save`:
1. convert editor coordinates to engine fixed-point space,
2. rebuild BSP from current geometry,
3. serialize linedefs/sidedefs/vertices/segs/subsectors/nodes/sectors,
4. replace or insert target `Map*` lump,
5. rewrite WAD with updated lump list.

### Texture and palette serialization

- `PaletteManager` loads indexed palette data and provides index->RGB lookup.
- `FlatTexture::Write` ensures `F_START`/`F_END` bounds exist, then replaces/inserts the target flat lump.

## Minimal Data Types Used by Algorithms

Core structures are defined in `include/defs.h`:
- `Vertex`, `Seg`, `Node`, `SubSector`, `Visplane`, `Sector`, `LineDef`, `SideDef`.

These types are shared between builder, renderer, and serialization code paths.

## Current Status

Implemented:
- BSP build and runtime traversal.
- Solid/pass wall clipping and column rendering.
- Visplane floor/ceiling span renderer.
- WAD-based level save/load path.

In progress:
- full wall texture path,
- stronger far-node culling,
- additional lighting/colormap behavior.
