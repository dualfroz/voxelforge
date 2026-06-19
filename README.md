# voxelforge

A voxel engine in C++17 with a playable browser demo. The renderer targets
OpenGL ES 3.0 / WebGL2 through Emscripten (WASM); the simulation core is
GL-free and builds natively with plain g++ for unit testing.

## Features (v1)

- Chunked world (16x16x16), cross-chunk block access.
- Greedy meshing (Lysenko sweep): coplanar same-type faces merge into
  rectangles, faces are culled against neighbours across chunk borders.
- FPP controller with AABB voxel collisions (walk + fly), gravity, jump,
  move-and-slide resolution.
- Terrain generation from seeded Perlin noise (heightmap, water, sand, snow,
  trees).
- Procedural texture-array atlas (one RGBA layer per tile, generated in code).
- Frustum culling per chunk (Gribb-Hartmann plane extraction).
- Block placing / breaking via voxel raycast (Amanatides and Woo DDA).
- Binary world save/load (RLE), wired to browser localStorage in the demo.

## Features (v2 - depth)

All added GL-free in `core/` and unit tested natively:

- Infinite streamed world: `ChunkStreamer` keeps a disc of chunk columns
  resident around the player with separate load/unload rings (hysteresis so
  pacing a boundary does not thrash), feeding a nearest-first generation queue
  and a mesh queue that the render loop time-slices.
- Procedural depth: `WorldGen` produces deterministic per-chunk terrain with
  climate biomes (ocean, beach, plains, forest, desert, mountains, snow),
  3D-noise spaghetti caves, and depth-banded ore veins (coal, iron, gold) plus
  gravel; trees planted per column.
- Lighting: `computeLight` flood-fills skylight (straight down at full strength,
  attenuating one level per step sideways) and block light from Glowstone, both
  as BFS. `meshChunkLit` bakes per-vertex ambient occlusion (classic 0-3 rule)
  and smoothed light into the mesh.
- Water / transparency: the lit mesher splits each chunk into opaque and
  transparent sub-meshes; the renderer draws water last with blending, depth
  writes off, chunks sorted back-to-front.
- Inventory: a 9-slot `Hotbar` with selection, wheel/number scrolling, stacking
  on break and consumption on place; drawn as an HTML HUD.
- Region save/load: `Region` groups chunks into 16x16-column region files with a
  per-chunk RLE index; `serializeRegions` writes every resident region to
  browser localStorage.

## Layout

```
core/              GL-free simulation, native gcc build, unit tested
  include/vf/      Math, Block, Chunk, World, Mesh, Noise, Terrain,
                   Raycast, Physics, Frustum, Serialize, Lighting, LitMesh,
                   Streaming, WorldGen, Region, Inventory
  src/             implementations
render/            WebGL2 layer, Emscripten build
  main.cpp         context, input, streaming, GPU chunk buffers, render loop
  Shaders.h        GLSL ES 3.0 vertex + fragment (light + AO)
  Atlas.*          procedural texture array
  Base64.*         localStorage payload encoding
  shell.html       page shell with crosshair, hotbar HUD, controls
tests/             native test harness + suites (74 tests)
```

The split is deliberate: `core/` has zero GL includes so it compiles and runs
tests without any GPU or graphics libraries, which is the automated proof that
the engine logic is correct. `render/` is the only part that needs Emscripten.

## Build and test the core (native)

```
make test
```

Compiles `core/src` + `tests` with g++ (`-std=c++17`) and runs the suite.
Expected tail:

```
74 tests, 59296 checks, 0 failed checks, 0 failed tests
```

## Build the web demo (Emscripten -> WASM)

Emscripten must be on PATH:

```
source ~/emsdk/emsdk_env.sh
make web
```

Produces `web/index.html`, `web/index.js`, `web/index.wasm`.

## Run the demo

WebGL needs to be served over http (not opened as a file://). Any static
server works:

```
make run-web           # emrun on http://localhost:8080
# or
python3 -m http.server 8080 --directory web
```

Then open the served `index.html`.

## Controls

- Click the canvas to lock the mouse.
- `WASD` move, mouse look, `Space` jump.
- `F` toggle fly; in fly mode `Space` / `Shift` go up / down.
- Left click break block, right click place block.
- `1`-`9` or the mouse wheel pick the hotbar block to place.
- `K` save world (region files) to localStorage, `L` load it back.

## Verification status

- Core tests (native gcc) pass, 74 in total. On top of v1 (greedy mesher,
  Perlin/terrain, raycast, AABB collisions, frustum, serialize) the depth work
  is covered: skylight/block-light flood-fill (attenuation, sealed-pocket
  darkness, radial emitter falloff), per-vertex AO and opaque/transparent split,
  streaming ring load/unload with hysteresis, biome/cave/ore world generation
  and determinism, region file round-trips, and the hotbar model. Each new
  mechanism was checked counterfactually (break the code, watch the test fail).
- The Emscripten build produces a valid WASM artifact. The in-browser render
  was NOT verified here: this environment has no GPU / headless WebGL. Open the
  served page to see it run.

## v3 (later)

- Native desktop build (GLFW + glad) once `libglfw3-dev` / `libgl1-mesa-dev`
  are installed; `core/` already builds natively, only a native `render/`
  backend is missing.
- Worker-threaded generation and meshing (currently time-sliced on the main
  loop), greedy merging of the lit mesh, and per-face water sorting.
- Shadow maps, mobs + physics, sound, LOD, texture packs.
```
