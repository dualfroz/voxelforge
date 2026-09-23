# voxelforge

voxelforge is a Minecraft-like voxel engine written in C++17. The simulation
core (`core/`) has no graphics dependencies: it builds with a plain C++
compiler and is covered by a native test suite of 1207 test cases. On top of it,
`render/` is a WebGL2 front end compiled to WebAssembly with Emscripten, a
playable browser demo with streamed terrain, lighting, a day/night cycle,
weather, flowing water, mobs and a working village.

## Features

### World and terrain

- Chunked world (16x16x16 chunks) with cross-chunk block access and 130 block
  types carrying solidity, opacity, transparency, light emission and per-face
  atlas tiles (`Block`, `Chunk`, `World`).
- Seeded Perlin noise, plus Worley noise and fractal Brownian motion
  (`Noise`, `NoiseAdv`).
- Infinite deterministic terrain: climate biomes (ocean, beach, plains, forest,
  desert, mountains, snow), per-biome heightmaps, 3D spaghetti caves,
  depth-banded ore veins and trees (`WorldGen`), plus a fixed-size heightmap
  generator (`Terrain`).
- Five-axis climate biome model (temperature, humidity, continentalness,
  erosion, weirdness) with eleven biomes, rivers, surface layers, decorator
  densities and biome blending (`BiomeGen`).
- Structures: trees and huts (`Structures`), and per-chunk villages, dungeons,
  mineshafts, ravines and desert temples (`StructureGen`).
- Nether and End generators: Nether biomes and columns, End islands and
  obsidian pillars (`NetherGen`, `EndGen`).
- Dimensions and portals: Overworld/Nether/End travel with coordinate scaling,
  portal frame detection and portal linking (`Dimension`, `Portal`).
- Regional climate: snow accumulation, ice formation and melting driven by
  temperature (`Climate`).

### Rendering support (GL-free)

- Greedy meshing (Lysenko sweep) with face culling across chunk borders
  (`Mesh`).
- Lit meshing with per-vertex ambient occlusion and smoothed light, split into
  opaque and transparent sub-meshes (`LitMesh`).
- Skylight and block light as BFS flood fills over a region (`Lighting`) and an
  incremental light engine that updates on block edits (`LightEngine`).
- Frustum culling with Gribb-Hartmann plane extraction (`Frustum`) and voxel
  raycasting with the Amanatides-Woo DDA (`Raycast`).
- Procedural texture atlas: tile palette, materials, patterns and a tile
  sampler that draws every block face in code (`TilePalette`, `TileTexture`).
- Chunk streaming with load/unload hysteresis (`Streaming`), a staged
  generate/light/mesh pipeline (`ChunkPipeline`), an LRU or distance-weighted
  chunk cache (`ChunkCache`) and a thread-pool job system with priorities and
  dependency graphs (`JobSystem`).

### Physics and survival

- Player controller with AABB collisions, walking, flying, gravity and jumping
  (`Physics`), and a swept AABB resolver with per-block collision shapes for
  slabs, stairs, fences, carpets, ladders and vines (`Collide`).
- Cellular water automaton with sources, flow levels and settling (`Fluid`).
- Volume-based fluid physics: submerged fraction, surface and eye depth sampled
  from world blocks and partial automaton levels, buoyancy by fluid density,
  linear and quadratic drag, currents along falling water levels, breath,
  drowning and lava burns with water breathing and fire resistance
  (`Buoyancy`).
- Health, hunger, saturation, exhaustion, regeneration, starvation, fall
  damage, breath and drowning, block hardness and mining time (`Survival`).
- Damage types, armor, combat stats (`Damage`), status effects (`Effect`) and
  explosions with blast resistance, drops and knockback (`Explosion`).

### Creatures and AI

- Mobs with physics bodies and A* pathfinding (`Entity`, `Pathfind`).
- Sheep, cows, pigs, zombies, skeletons and creepers with species stats, an
  AI state machine (wander, flee, chase, attack) and drop tables (`Mob`).
- Behavior trees with selectors, sequences, parallels, decorators and a
  blackboard (`Behavior`).
- Natural spawning: spawn rules per species, light/biome/time checks with a
  named reason for every refusal, mob caps, packs, despawn bands and spawner
  blocks (`Spawning`).
- Villagers: professions, workstations, beds and the bell as claimable points
  of interest, a daily schedule, pathing, levels, gossip, census and breeding
  (`Villager`, `Gossip`), and villager trading (`Trade`).
- Raids with waves, bad omen, raider kinds and iron golems (`Raid`).
- Boss fights: the Ender Dragon with end crystals and the Wither, with boss
  bars and phases (`Boss`).

### Items and inventories

- Item registry with tools, tiers, food values and stack sizes (`Item`),
  stack metadata and durability (`ItemStack`), the hotbar (`Inventory`).
- Containers, menus and slot interactions including shift-click and drag
  (`Container`, `ContainerMenu`, `SlotAction`).
- Shaped, shapeless and smelting recipes and a furnace (`Crafting`).
- Equipment slots with attribute modifiers (`Equipment`), enchanting
  (`Enchant`), brewing and a potion catalog with brewing paths (`Brewing`,
  `PotionCatalog`).
- Loot tables with pools, block and mob drops, chest fills and fishing loot
  (`Loot`, `LootTables`).

### Simulation

- World clock with day phases, sky light curve, moon phases and sunrise/sunset
  detection (`TimeOfDay`), and a weather state machine with rain, thunder and
  lightning strikes (`Weather`).
- Random ticks and a scheduled block tick queue (`RandomTick`, `BlockTick`).
- Farming: wheat, carrots, potatoes, pumpkin and melon stems (`Farming`).
- Redstone circuits with wires, torches, repeaters, comparators and lamps
  (`Circuit`), pistons (`Piston`), hoppers and dispensers (`Hopper`,
  `Dispenser`), rails and minecarts (`Rail`, `Minecart`).
- A deterministic orchestrator that runs clock, weather, climate, farming,
  block ticks, redstone, logistics, minecarts and creatures in one step and
  reports changed blocks (`WorldSim`).

### Persistence

- Binary world save with RLE (`Serialize`), region files (`Region`),
  Anvil-style regions with bit-packed palette sections (`Anvil`) and NBT tags
  (`Nbt`).
- Store/RLE/LZ77 compression (`Codec`), a v3 save format with fluid and light
  snapshots (`SaveV3`), save version migration (`SaveMigration`), integrity
  scanning and repair (`SaveIntegrity`), region compaction (`RegionCompact`),
  incremental backups with retention (`WorldBackup`) and player data
  (`PlayerData`).

### Networking

- Packet protocol (handshake, login, join, chunk data, block changes, entity
  movement, keep-alive) (`Protocol`).
- Reliable and unreliable channels over a loopback link with configurable
  latency and loss (`Transport`).
- Client sessions with view tracking and bandwidth limits (`NetSession`), a
  multi-client server (`NetServer`) and server/client chunk replication
  (`ChunkSync`).

### Tooling and data

- Command dispatcher with game rules (`Command`), scoreboard with statistics,
  objectives and teams (`Scoreboard`) and advancements (`Advancement`).
- JSON parser, writer and JSON pointer (`Json`), data packs with recipes and
  block tags (`DataPack`).
- World editing: selections, brushes (sphere, cylinder, replace, smooth,
  erode, overlay, naturalize, gradient), undo/redo history and schematics with
  rotation and mirroring (`Selection`, `Brush`, `EditHistory`, `Schematic`).

## Architecture

```
core/                 GL-free engine, builds natively, unit tested
  include/vf/         public headers, one per module (namespace vf)
  src/                implementations
render/               WebGL2 front end, built with Emscripten
  main.cpp            setup, input, game tick, frame loop, HUD wiring
  Terrain.*           streaming, lighting per column, GPU chunk meshes
  Sky.*               sky colour, sun direction and fog from clock and weather
  Hands.*             targeting, mining, hitting mobs, placing blocks
  Explorer.*          player body, swimming, survival stats
  Wildlife.*          natural spawning, mob AI ticks, combat
  Village.*           village placement, points of interest, villager ticks
  WaterFlow.*         water automaton sites for poured water
  EntityRenderer.*    box models for mobs and villagers
  Gpu.*, Shaders.h    GL helpers and GLSL ES 3.0 shaders
  Atlas.*             texture array built from the core tile sampler
  Storage.*, Base64.* terrain save and load through localStorage
  Hud.*, shell.html   HTML HUD and page shell
tests/                native test harness and one suite per module
```

`core/` never includes a GL header, so all engine logic compiles and runs in
the test binary without a GPU. `render/` only connects core modules to WebGL,
input and the page; it contains no engine rules of its own.

## Building

Requirements: a C++17 compiler (GCC or Clang) and `make`. The web demo also
needs [Emscripten](https://emscripten.org/).

### Native tests

```
make test
```

Compiles `core/src` and `tests` into `build/voxelforgeTests` and runs it from
the repository root. The last line reports the totals:

```
1207 tests, 378986 checks, 0 failed checks, 0 failed tests
```

### Web demo

```
source /path/to/emsdk/emsdk_env.sh
make web
```

Produces `web/index.html`, `web/index.js` and `web/index.wasm`. WebGL needs the
page to be served over HTTP:

```
make run-web
```

or any static server, for example `python3 -m http.server 8080 --directory web`,
then open `http://localhost:8080/`.

## The demo

The demo starts next to a village placed on the first suitable plains, forest
or desert site near the origin. What runs in the browser:

- Streamed terrain with biomes, caves, ores and trees around the player.
- Baked light with ambient occlusion; the sky light follows the clock, so the
  terrain darkens at night while glowstone keeps its light.
- Day/night cycle with sun direction, dawn and dusk colours, moon phases, and
  weather with rain darkening the sky and lightning flashes.
- Mining with block hardness, placing blocks from the hotbar.
- Water poured from the hotbar runs through the fluid automaton; inside water
  the player swims, and underwater the breath bar drains and drowning damage
  starts.
- Animals and, at night, monsters spawn by the spawn rules; monsters chase and
  hit the player, creepers explode and break terrain, animals flee when hit and
  drop food.
- Villagers claim beds and workstations, go to work during the day, meet at the
  bell and go to bed at night, and panic when monsters come close.
- Health, hunger and breath with regeneration, starvation and fall damage;
  death respawns the player at the village.

### Controls

| Input | Action |
|---|---|
| Click | lock the mouse |
| `W` `A` `S` `D`, mouse | move, look |
| `Space` | jump; swim up in water |
| `Shift` | dive in water; descend while flying |
| `F` | toggle flying (no damage, instant mining) |
| Hold left button | mine the targeted block or hit a mob |
| Right button | place the selected block (water flows) |
| `1`-`9`, wheel | select a hotbar slot |
| `Q` | eat the best food carried |
| `T` | cycle time speed (1x, 30x, 240x) |
| `K` / `L` | save / load the terrain to localStorage |

## Limitations

- The only renderer is the WebGL2 demo; there is no native desktop front end.
- Networking runs over an in-memory loopback link with simulated latency and
  loss; there is no socket transport.
- The player and mobs use the axis-by-axis resolver in `Physics` and the demo
  swims with its own parameters; the swept resolver in `Collide` and the fluid
  model in `Buoyancy` are not wired into them yet.
- Most systems listed under items, simulation, persistence, networking and
  tooling are exercised by the tests but not exposed in the demo.
- The demo save stores terrain only, not mobs, villagers, time or the player.
- Generation, lighting and meshing in the demo run time-sliced on the main
  thread; a change of sky level re-bakes the loaded columns over a few seconds.
