#include <emscripten.h>
#include <emscripten/html5.h>
#include <GLES3/gl3.h>

#include <algorithm>
#include <cmath>
#include <string>

#include "EntityRenderer.h"
#include "Explorer.h"
#include "Hands.h"
#include "Hud.h"
#include "Sky.h"
#include "Storage.h"
#include "Terrain.h"
#include "Village.h"
#include "WaterFlow.h"
#include "Wildlife.h"
#include "vf/Command.h"
#include "vf/Inventory.h"

using namespace vf;

namespace {

struct KeyState {
    bool forward = false, back = false, left = false, right = false;
    bool jump = false, sink = false;
};

constexpr float tickSeconds = 1.0f / 20.0f;
constexpr int timeScales[3] = {1, 30, 240};
constexpr WorldTick morningTick = 7200;

WorldGenParams genParams;
WorldGen worldGen{genParams};
World world;
TerrainView terrain{world, worldGen, StreamingConfig{6, 8}};
WaterFlow water{world, terrain};
Wildlife wildlife{world, terrain, worldGen, genParams.seed};
Village village{world, terrain};
EntityRenderer entityRenderer;
Explorer explorer;
Hotbar hotbar;
TimeOfDay worldClock{morningTick};
Weather weather{genParams.seed};
Hands hands{world, terrain, water, wildlife, village, explorer, hotbar};
KeyState keys;
Vec3 spawnPoint;

int timeScaleIndex = 0;
float tickAccumulator = 0.0f;
int tickCounter = 0;
bool populated = false;
float flash = 0.0f;
std::string message;
float messageSeconds = 0.0f;

double lastTime = 0.0;
int canvasWidth = 1280;
int canvasHeight = 720;
int framesThisSecond = 0;
double fpsWindowStart = 0.0;
int fps = 0;
float hudTimer = 0.0f;

void say(const std::string& text) {
    message = text;
    messageSeconds = 3.0f;
}

ColumnCoord columnOf(const Vec3& position) {
    return {floorDiv(static_cast<int>(std::floor(position.x)), chunkSize),
            floorDiv(static_cast<int>(std::floor(position.z)), chunkSize)};
}

float surfaceTop(int gx, int gz) {
    for (int y = genParams.maxY - 1; y >= 0; --y)
        if (world.solidAt(gx, y, gz)) return static_cast<float>(y + 1);
    return static_cast<float>(genParams.maxY);
}

void foundSettlement() {
    if (village.settleNear(worldGen, {0, 0}, 48)) {
        Vec3 entrance = village.entrance();
        spawnPoint = {entrance.x, surfaceTop(static_cast<int>(std::floor(entrance.x)),
                                             static_cast<int>(std::floor(entrance.z))),
                      entrance.z};
        return;
    }
    terrain.generateNow({0, 0});
    spawnPoint = {8.5f, surfaceTop(8, 8) + 1.0f, 8.5f};
}

int effectiveSkyLevel() {
    int dimming = static_cast<int>(std::lround(weather.rainGradient() * 3.0));
    return std::max(worldClock.settings().nightSkyLight, worldClock.skyLightLevel() - dimming);
}

bool villageThreatened() {
    if (!village.founded() || worldClock.isDaylight()) return false;
    Vec3i centre = village.centre();
    for (const WildCreature& wild : wildlife.creatures()) {
        if (!wild.creature.alive || !isHostile(wild.creature.species)) continue;
        float dx = wild.creature.body.position.x - centre.x;
        float dz = wild.creature.body.position.z - centre.z;
        if (dx * dx + dz * dz < 24.0f * 24.0f) return true;
    }
    return false;
}

void gameTick() {
    int scale = timeScales[timeScaleIndex];
    worldClock.advance(scale);
    weather.advance(scale);
    if (!weather.strikesThisTick().empty()) flash = 1.0f;
    terrain.setSkyLevel(effectiveSkyLevel());

    Player& body = explorer.body();
    WildlifeImpact impact = wildlife.tick(body.position, worldClock.isDaylight(), tickSeconds);
    if (impact.damage > 0.0f) {
        explorer.hurt(DamageSource::MobAttack, impact.damage);
        body.velocity.y += 4.0f + std::max(0.0f, impact.knockback.y);
    }
    if (impact.exploded) say("A creeper exploded");

    village.tick(worldClock, villageThreatened(), tickSeconds);
    if (++tickCounter % 4 == 0) water.tick(body.position);

    explorer.tick(tickSeconds);
    if (!explorer.vitals().alive) {
        explorer.respawn(spawnPoint);
        say("You died and respawned at the village");
    }
}

void saveWorld() {
    say("World saved (" + std::to_string(saveTerrain(world)) + " region bytes)");
}

void loadWorld() {
    switch (loadTerrain(world)) {
        case LoadOutcome::NothingSaved: say("Nothing saved yet"); break;
        case LoadOutcome::Damaged: say("Saved world is damaged"); break;
        case LoadOutcome::Loaded:
            terrain.reloadAll();
            say("World loaded");
            break;
    }
}

EM_BOOL onKey(int type, const EmscriptenKeyboardEvent* e, void*) {
    bool down = (type == EMSCRIPTEN_EVENT_KEYDOWN);
    std::string code = e->code;
    Player& body = explorer.body();
    if (code == "KeyW") keys.forward = down;
    else if (code == "KeyS") keys.back = down;
    else if (code == "KeyA") keys.left = down;
    else if (code == "KeyD") keys.right = down;
    else if (code == "Space") keys.jump = down;
    else if (code == "ShiftLeft") keys.sink = down;
    else if (down && code == "KeyF") {
        body.flying = !body.flying;
        say(body.flying ? "Flying: no damage, instant mining" : "Walking: survival rules apply");
    } else if (down && code == "KeyT") {
        timeScaleIndex = (timeScaleIndex + 1) % 3;
        say("Time runs " + std::to_string(timeScales[timeScaleIndex]) + "x");
    } else if (down && code == "KeyQ") {
        ItemId eaten = explorer.eatBest();
        say(eaten == ItemId::None ? "No food - hunt animals for it"
                                  : std::string("Ate ") + itemDisplayName(eaten));
    } else if (down && code == "KeyK") saveWorld();
    else if (down && code == "KeyL") loadWorld();
    else if (down && code.rfind("Digit", 0) == 0) {
        int digit = code[5] - '1';
        if (digit >= 0 && digit < hotbarSize) {
            hotbar.select(digit);
            pushHotbar(hotbar);
        }
    } else return EM_FALSE;
    return EM_TRUE;
}

EM_BOOL onWheel(int, const EmscriptenWheelEvent* e, void*) {
    hotbar.scroll(e->deltaY > 0 ? 1 : -1);
    pushHotbar(hotbar);
    return EM_TRUE;
}

bool pointerLocked() {
    EmscriptenPointerlockChangeEvent lock;
    return emscripten_get_pointerlock_status(&lock) == EMSCRIPTEN_RESULT_SUCCESS && lock.isActive;
}

EM_BOOL onMouseMove(int, const EmscriptenMouseEvent* e, void*) {
    if (!pointerLocked()) return EM_FALSE;
    Player& body = explorer.body();
    const float sensitivity = 0.0022f;
    const float limit = 1.5533f;
    body.yaw += e->movementX * sensitivity;
    body.pitch = std::clamp(body.pitch - e->movementY * sensitivity, -limit, limit);
    return EM_TRUE;
}

EM_BOOL onMouseDown(int, const EmscriptenMouseEvent* e, void*) {
    if (!pointerLocked()) {
        emscripten_request_pointerlock("#canvas", EM_TRUE);
        return EM_TRUE;
    }
    if (e->button == 0) hands.press();
    else if (e->button == 2) hands.place();
    return EM_TRUE;
}

EM_BOOL onMouseUp(int, const EmscriptenMouseEvent* e, void*) {
    if (e->button == 0) hands.release();
    return EM_TRUE;
}

EM_BOOL onResize(int, const EmscriptenUiEvent*, void*) {
    double w, h;
    emscripten_get_element_css_size("#canvas", &w, &h);
    canvasWidth = static_cast<int>(w);
    canvasHeight = static_cast<int>(h);
    emscripten_set_canvas_element_size("#canvas", canvasWidth, canvasHeight);
    return EM_TRUE;
}

void drawScene() {
    SkyInputs sky;
    sky.clock = &worldClock;
    sky.weather = &weather;
    sky.viewer = &explorer.body();
    sky.aspect = static_cast<float>(canvasWidth) / static_cast<float>(canvasHeight);
    sky.viewDistance = static_cast<float>(terrain.loadRadius() * chunkSize);
    sky.lightningFlash = flash;
    sky.submerged = explorer.submerged();
    SceneLook look = composeLook(sky);

    glViewport(0, 0, canvasWidth, canvasHeight);
    glClearColor(look.fogColor.x, look.fogColor.y, look.fogColor.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glDisable(GL_BLEND);

    terrain.drawOpaque(look, eyePosition(explorer.body()));
    entityRenderer.draw(look, terrain, wildlife, village);
    terrain.drawTransparent();
}

void publishHud() {
    const Player& body = explorer.body();
    const SurvivalState& vitals = explorer.vitals();
    HudFrame hud;
    hud.fps = fps;
    hud.position = body.position;
    hud.biome = biomeDisplayName(worldGen.biomeAt(static_cast<int>(std::floor(body.position.x)),
                                                  static_cast<int>(std::floor(body.position.z))));
    hud.phase = dayPhaseName(worldClock.phase());
    hud.day = worldClock.dayCount() + 1;
    int minutes = static_cast<int>(worldClock.dayFraction() * 24.0 * 60.0);
    hud.hour = minutes / 60;
    hud.minute = minutes % 60;
    hud.moonPhase = worldClock.moonPhase();
    hud.skyLight = terrain.skyLevel();
    hud.weather = weatherStateName(weather.state());
    hud.timeScale = timeScales[timeScaleIndex];
    hud.health = vitals.health;
    hud.maxHealth = explorer.limits().maxHealth;
    hud.hunger = vitals.hunger;
    hud.maxHunger = explorer.limits().maxHunger;
    hud.air = vitals.airRemaining;
    hud.maxAir = explorer.limits().breathHoldSeconds;
    hud.food = explorer.foodCount();
    hud.flying = body.flying;
    hud.swimming = explorer.swimming();
    hud.underwater = explorer.submerged();
    hud.passive = wildlife.livingIn(SpawnCategory::Passive);
    hud.hostile = wildlife.livingIn(SpawnCategory::Hostile);
    VillageCensus census = village.census();
    hud.villagers = census.living;
    hud.employed = census.employed;
    hud.beds = census.beds;
    hud.workstations = census.workstations;
    hud.waterSites = water.activeSites();
    hud.flowingCells = water.flowingCells();
    hud.columns = static_cast<int>(terrain.residentColumns());
    hud.meshes = static_cast<int>(terrain.meshedChunks());
    hud.pending = static_cast<int>(terrain.pendingMeshes());
    hud.target = hands.describeTarget();
    hud.mining = hands.miningFraction();
    hud.message = messageSeconds > 0.0f ? message : "";
    pushHud(hud);
}

void frame() {
    double now = emscripten_get_now();
    float dt = std::min(static_cast<float>((now - lastTime) / 1000.0), 0.1f);
    lastTime = now;
    ++framesThisSecond;
    if (now - fpsWindowStart >= 1000.0) {
        fps = framesThisSecond;
        framesThisSecond = 0;
        fpsWindowStart = now;
    }

    ExplorerControls controls;
    controls.forward = (keys.forward ? 1.0f : 0.0f) - (keys.back ? 1.0f : 0.0f);
    controls.strafe = (keys.right ? 1.0f : 0.0f) - (keys.left ? 1.0f : 0.0f);
    controls.jump = keys.jump;
    controls.sink = keys.sink;
    explorer.move(world, controls, dt);

    terrain.update(columnOf(explorer.body().position));
    if (!populated && terrain.pendingMeshes() == 0) {
        wildlife.populate(explorer.body().position, worldClock.isDaylight());
        populated = true;
    }

    tickAccumulator += dt;
    int ticks = 0;
    while (tickAccumulator >= tickSeconds && ticks++ < 4) {
        tickAccumulator -= tickSeconds;
        gameTick();
    }
    if (tickAccumulator > tickSeconds) tickAccumulator = 0.0f;

    hands.aim();
    std::string report = hands.work(dt);
    if (!report.empty()) say(report);
    flash = std::max(0.0f, flash - dt * 3.0f);
    messageSeconds = std::max(0.0f, messageSeconds - dt);

    drawScene();

    hudTimer -= dt;
    if (hudTimer <= 0.0f) {
        hudTimer = 0.15f;
        publishHud();
    }
}

}

int main() {
    EmscriptenWebGLContextAttributes attrs;
    emscripten_webgl_init_context_attributes(&attrs);
    attrs.majorVersion = 2;
    attrs.minorVersion = 0;
    attrs.depth = EM_TRUE;
    attrs.antialias = EM_TRUE;
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_create_context("#canvas", &attrs);
    emscripten_webgl_make_context_current(ctx);

    onResize(0, nullptr, nullptr);
    terrain.initGpu();
    entityRenderer.init();

    foundSettlement();
    explorer.respawn(spawnPoint);
    explorer.body().yaw = 3.14159265f;
    terrain.recentre(columnOf(spawnPoint));
    terrain.setSkyLevel(effectiveSkyLevel());
    pushHotbar(hotbar);
    say(village.founded() ? "Welcome to the village" : "No village site found nearby");

    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, onKey);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, onKey);
    emscripten_set_wheel_callback("#canvas", nullptr, EM_TRUE, onWheel);
    emscripten_set_mousemove_callback("#canvas", nullptr, EM_TRUE, onMouseMove);
    emscripten_set_mousedown_callback("#canvas", nullptr, EM_TRUE, onMouseDown);
    emscripten_set_mouseup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, onMouseUp);
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, onResize);

    lastTime = emscripten_get_now();
    fpsWindowStart = lastTime;
    emscripten_set_main_loop(frame, 0, EM_TRUE);
    return 0;
}
