#include <emscripten.h>
#include <emscripten/html5.h>
#include <GLES3/gl3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Atlas.h"
#include "Base64.h"
#include "Shaders.h"
#include "vf/Frustum.h"
#include "vf/Inventory.h"
#include "vf/Lighting.h"
#include "vf/LitMesh.h"
#include "vf/Physics.h"
#include "vf/Raycast.h"
#include "vf/Region.h"
#include "vf/Streaming.h"
#include "vf/WorldGen.h"

using namespace vf;

namespace {

struct GpuMesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ibo = 0;
    GLsizei indexCount = 0;
};

struct GpuChunk {
    GpuMesh opaque;
    GpuMesh transparent;
    Aabb bounds;
    Vec3 center;
};

struct KeyState {
    bool forward = false, back = false, left = false, right = false;
    bool jump = false, sink = false;
};

WorldGenParams genParams;
WorldGen worldGen{genParams};
World world;
StreamingConfig streamConfig{6, 8};
ChunkStreamer streamer{streamConfig};
Player player;
KeyState keys;
PhysicsConfig physics;
Hotbar hotbar;

std::deque<ColumnCoord> genQueue;
std::deque<Vec3i> meshQueue;
std::unordered_map<Vec3i, GpuChunk, Vec3iHash> gpuChunks;

bool haveLightCache = false;
ColumnCoord lightCacheColumn{};
LightField lightCache;
constexpr int lightMargin = 2;

GLuint program = 0;
GLint uViewProj = -1, uAtlas = -1, uLightDir = -1, uFogColor = -1;
GLuint atlasTexture = 0;

ColumnCoord lastCentre{2147483647, 2147483647};
double lastTime = 0.0;
int canvasWidth = 1280;
int canvasHeight = 720;

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::printf("shader compile error: %s\n", log);
    }
    return shader;
}

void buildProgram() {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vertexShaderSource());
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource());
    program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    uViewProj = glGetUniformLocation(program, "uViewProj");
    uAtlas = glGetUniformLocation(program, "uAtlas");
    uLightDir = glGetUniformLocation(program, "uLightDir");
    uFogColor = glGetUniformLocation(program, "uFogColor");
}

void uploadAtlas() {
    Atlas atlas = buildAtlas(16);
    glGenTextures(1, &atlasTexture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, atlasTexture);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, atlas.tileSize, atlas.tileSize, atlas.layers, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, atlas.pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void freeGpuMesh(GpuMesh& mesh) {
    if (mesh.vao) glDeleteVertexArrays(1, &mesh.vao);
    if (mesh.vbo) glDeleteBuffers(1, &mesh.vbo);
    if (mesh.ibo) glDeleteBuffers(1, &mesh.ibo);
    mesh = GpuMesh{};
}

void uploadGpuMesh(GpuMesh& gpu, const Mesh& mesh) {
    if (mesh.empty()) {
        freeGpuMesh(gpu);
        return;
    }
    if (gpu.vao == 0) {
        glGenVertexArrays(1, &gpu.vao);
        glGenBuffers(1, &gpu.vbo);
        glGenBuffers(1, &gpu.ibo);
    }
    glBindVertexArray(gpu.vao);
    glBindBuffer(GL_ARRAY_BUFFER, gpu.vbo);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(Vertex), mesh.vertices.data(),
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gpu.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(uint32_t),
                 mesh.indices.data(), GL_STATIC_DRAW);

    GLsizei stride = sizeof(Vertex);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride, (void*)(9 * sizeof(float)));
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, stride, (void*)(10 * sizeof(float)));

    gpu.indexCount = static_cast<GLsizei>(mesh.indices.size());
}

void removeChunkGpu(const Vec3i& coord) {
    auto it = gpuChunks.find(coord);
    if (it == gpuChunks.end()) return;
    freeGpuMesh(it->second.opaque);
    freeGpuMesh(it->second.transparent);
    gpuChunks.erase(it);
}

const LightField& columnLight(ColumnCoord column) {
    if (haveLightCache && lightCacheColumn == column) return lightCache;
    int top = worldGen.stackChunkCount() * chunkSize;
    Vec3i origin{column.x * chunkSize - lightMargin, 0, column.z * chunkSize - lightMargin};
    Vec3i dims{chunkSize + 2 * lightMargin, top, chunkSize + 2 * lightMargin};
    lightCache = computeLight(world, origin, dims);
    lightCacheColumn = column;
    haveLightCache = true;
    return lightCache;
}

void meshChunk(const Vec3i& coord) {
    ChunkMesh mesh = meshChunkLit(world, coord, columnLight({coord.x, coord.z}));
    if (mesh.empty()) {
        removeChunkGpu(coord);
        return;
    }
    GpuChunk& gpu = gpuChunks[coord];
    uploadGpuMesh(gpu.opaque, mesh.opaque);
    uploadGpuMesh(gpu.transparent, mesh.transparent);
    float ox = static_cast<float>(coord.x * chunkSize);
    float oy = static_cast<float>(coord.y * chunkSize);
    float oz = static_cast<float>(coord.z * chunkSize);
    gpu.bounds = Aabb{{ox, oy, oz}, {ox + chunkSize, oy + chunkSize, oz + chunkSize}};
    gpu.center = Vec3{ox + chunkSize * 0.5f, oy + chunkSize * 0.5f, oz + chunkSize * 0.5f};
}

void enqueueMesh(const Vec3i& coord) {
    for (const Vec3i& pending : meshQueue)
        if (pending == coord) return;
    meshQueue.push_back(coord);
}

void enqueueColumnMeshes(ColumnCoord column) {
    for (int cy = 0; cy < worldGen.stackChunkCount(); ++cy)
        enqueueMesh({column.x, cy, column.z});
}

bool columnGenerated(ColumnCoord column) {
    return world.chunkAt({column.x, 0, column.z}) != nullptr;
}

void generateColumnNow(ColumnCoord column) {
    if (!columnGenerated(column)) worldGen.generateColumn(world, column.x, column.z);
    haveLightCache = false;
    enqueueColumnMeshes(column);
    const ColumnCoord neighbours[4] = {{column.x + 1, column.z}, {column.x - 1, column.z},
                                       {column.x, column.z + 1}, {column.x, column.z - 1}};
    for (ColumnCoord n : neighbours)
        if (columnGenerated(n)) enqueueColumnMeshes(n);
}

void unloadColumn(ColumnCoord column) {
    for (int cy = 0; cy < worldGen.stackChunkCount(); ++cy) {
        Vec3i coord{column.x, cy, column.z};
        removeChunkGpu(coord);
        world.removeChunk(coord);
    }
    haveLightCache = false;
}

ColumnCoord playerColumn() {
    return {floorDiv(static_cast<int>(std::floor(player.position.x)), chunkSize),
            floorDiv(static_cast<int>(std::floor(player.position.z)), chunkSize)};
}

MoveInput sampleInput() {
    MoveInput input;
    input.forward = (keys.forward ? 1.0f : 0.0f) - (keys.back ? 1.0f : 0.0f);
    input.strafe = (keys.right ? 1.0f : 0.0f) - (keys.left ? 1.0f : 0.0f);
    if (player.flying) {
        input.up = keys.jump;
        input.down = keys.sink;
    } else {
        input.jump = keys.jump;
    }
    return input;
}

const char* blockName(Block block) {
    switch (block) {
        case Block::Grass: return "Grass";
        case Block::Dirt: return "Dirt";
        case Block::Stone: return "Stone";
        case Block::Sand: return "Sand";
        case Block::Wood: return "Wood";
        case Block::Leaves: return "Leaves";
        case Block::Water: return "Water";
        case Block::Snow: return "Snow";
        case Block::CoalOre: return "Coal";
        case Block::IronOre: return "Iron";
        case Block::GoldOre: return "Gold";
        case Block::Gravel: return "Gravel";
        case Block::Glowstone: return "Glow";
        default: return "?";
    }
}

EM_JS(void, pushHotbar, (int selected, const char* payload), {
    if (typeof window.renderHotbar === 'function')
        window.renderHotbar(selected, UTF8ToString(payload));
});

void refreshHud() {
    std::string payload;
    for (int i = 0; i < hotbarSize; ++i) {
        const ItemStack& stack = hotbar.slot(i);
        payload += blockName(stack.block);
        payload += ",";
        payload += std::to_string(stack.count);
        if (i + 1 < hotbarSize) payload += ";";
    }
    pushHotbar(hotbar.selectedIndex(), payload.c_str());
}

void editBlock(int gx, int gy, int gz, Block block) {
    world.setBlock(gx, gy, gz, block);
    haveLightCache = false;
    static const int offsets[7][3] = {{0, 0, 0},  {1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                                      {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};
    for (auto& o : offsets) {
        Vec3i coord{floorDiv(gx + o[0], chunkSize), floorDiv(gy + o[1], chunkSize),
                    floorDiv(gz + o[2], chunkSize)};
        world.ensureChunk(coord);
        enqueueMesh(coord);
    }
}

void interact(bool destroy) {
    Vec3 eye = eyePosition(player);
    Vec3 dir = directionFromYawPitch(player.yaw, player.pitch);
    RaycastHit hit = raycastVoxels(world, eye, dir, 8.0f);
    if (!hit.hit) return;
    if (destroy) {
        Block broken = world.blockAt(hit.block.x, hit.block.y, hit.block.z);
        editBlock(hit.block.x, hit.block.y, hit.block.z, Block::Air);
        hotbar.add(broken, 1);
        refreshHud();
    } else {
        if (hotbar.slot(hotbar.selectedIndex()).empty()) return;
        Vec3i target = hit.block + hit.normal;
        Aabb box = playerBox(player);
        Aabb cell{{(float)target.x, (float)target.y, (float)target.z},
                  {(float)target.x + 1.0f, (float)target.y + 1.0f, (float)target.z + 1.0f}};
        bool overlaps = box.min.x < cell.max.x && box.max.x > cell.min.x &&
                        box.min.y < cell.max.y && box.max.y > cell.min.y &&
                        box.min.z < cell.max.z && box.max.z > cell.min.z;
        if (!overlaps) {
            editBlock(target.x, target.y, target.z, hotbar.selectedBlock());
            hotbar.consumeSelected();
            refreshHud();
        }
    }
}

EM_JS(void, storeWorld, (const char* text), {
    try { localStorage.setItem('voxelforgeRegions', UTF8ToString(text)); } catch (e) {}
});

EM_JS(char*, readWorld, (), {
    var text = String.fromCharCode();
    try {
        var stored = localStorage.getItem('voxelforgeRegions');
        if (stored) text = stored;
    } catch (e) {}
    var size = lengthBytesUTF8(text) + 1;
    var ptr = _malloc(size);
    stringToUTF8(text, ptr, size);
    return ptr;
});

void saveWorld() {
    std::vector<uint8_t> bytes = serializeRegions(world);
    storeWorld(base64Encode(bytes).c_str());
    std::printf("world saved (%zu region bytes)\n", bytes.size());
}

void loadWorld() {
    char* text = readWorld();
    std::string encoded(text);
    free(text);
    if (encoded.empty()) return;
    std::vector<uint8_t> bytes = base64Decode(encoded);
    if (!deserializeRegions(bytes.data(), bytes.size(), world)) return;
    for (auto& entry : gpuChunks) {
        freeGpuMesh(entry.second.opaque);
        freeGpuMesh(entry.second.transparent);
    }
    gpuChunks.clear();
    meshQueue.clear();
    haveLightCache = false;
    for (const auto& entry : world.allChunks()) enqueueMesh(entry.first);
    std::printf("world loaded\n");
}

EM_BOOL onKey(int type, const EmscriptenKeyboardEvent* e, void*) {
    bool down = (type == EMSCRIPTEN_EVENT_KEYDOWN);
    std::string code = e->code;
    if (code == "KeyW") keys.forward = down;
    else if (code == "KeyS") keys.back = down;
    else if (code == "KeyA") keys.left = down;
    else if (code == "KeyD") keys.right = down;
    else if (code == "Space") keys.jump = down;
    else if (code == "ShiftLeft") keys.sink = down;
    else if (down && code == "KeyF") player.flying = !player.flying;
    else if (down && code == "KeyK") saveWorld();
    else if (down && code == "KeyL") loadWorld();
    else if (down && code.rfind("Digit", 0) == 0) {
        int digit = code[5] - '1';
        if (digit >= 0 && digit < hotbarSize) { hotbar.select(digit); refreshHud(); }
    } else return EM_FALSE;
    return EM_TRUE;
}

EM_BOOL onWheel(int, const EmscriptenWheelEvent* e, void*) {
    hotbar.scroll(e->deltaY > 0 ? 1 : -1);
    refreshHud();
    return EM_TRUE;
}

EM_BOOL onMouseMove(int, const EmscriptenMouseEvent* e, void*) {
    EmscriptenPointerlockChangeEvent lock;
    if (emscripten_get_pointerlock_status(&lock) == EMSCRIPTEN_RESULT_SUCCESS && !lock.isActive)
        return EM_FALSE;
    const float sensitivity = 0.0022f;
    player.yaw += e->movementX * sensitivity;
    player.pitch -= e->movementY * sensitivity;
    const float limit = 1.5533f;
    if (player.pitch > limit) player.pitch = limit;
    if (player.pitch < -limit) player.pitch = -limit;
    return EM_TRUE;
}

EM_BOOL onMouseDown(int, const EmscriptenMouseEvent* e, void*) {
    EmscriptenPointerlockChangeEvent lock;
    if (emscripten_get_pointerlock_status(&lock) != EMSCRIPTEN_RESULT_SUCCESS || !lock.isActive) {
        emscripten_request_pointerlock("#canvas", EM_TRUE);
        return EM_TRUE;
    }
    if (e->button == 0) interact(true);
    else if (e->button == 2) interact(false);
    return EM_TRUE;
}

void updateStreaming() {
    ColumnCoord centre = playerColumn();
    if (!(centre == lastCentre)) {
        lastCentre = centre;
        StreamDelta delta = streamer.update(centre);
        for (ColumnCoord column : delta.load) genQueue.push_back(column);
        for (ColumnCoord column : delta.unload) unloadColumn(column);
    }

    int genBudget = 2;
    while (genBudget-- > 0 && !genQueue.empty()) {
        ColumnCoord column = genQueue.front();
        genQueue.pop_front();
        if (streamer.isLoaded(column)) generateColumnNow(column);
    }

    int meshBudget = 6;
    while (meshBudget-- > 0 && !meshQueue.empty()) {
        Vec3i coord = meshQueue.front();
        meshQueue.pop_front();
        if (world.chunkAt(coord)) meshChunk(coord);
    }
}

Mat4 currentViewProj() {
    Vec3 eye = eyePosition(player);
    Vec3 forward = directionFromYawPitch(player.yaw, player.pitch);
    Mat4 view = lookAt(eye, eye + forward, {0, 1, 0});
    float aspect = static_cast<float>(canvasWidth) / static_cast<float>(canvasHeight);
    Mat4 proj = perspective(1.2217f, aspect, 0.1f, 480.0f);
    return proj * view;
}

void frame() {
    double now = emscripten_get_now();
    float dt = static_cast<float>((now - lastTime) / 1000.0);
    lastTime = now;
    if (dt > 0.1f) dt = 0.1f;

    MoveInput input = sampleInput();
    int substeps = 4;
    float sub = dt / substeps;
    for (int i = 0; i < substeps; ++i) stepPlayer(world, player, input, physics, sub);

    updateStreaming();

    Mat4 viewProj = currentViewProj();
    Frustum frustum = Frustum::fromMatrix(viewProj);
    Vec3 eye = eyePosition(player);

    glViewport(0, 0, canvasWidth, canvasHeight);
    glClearColor(0.55f, 0.71f, 0.92f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glDisable(GL_BLEND);

    glUseProgram(program);
    glUniformMatrix4fv(uViewProj, 1, GL_FALSE, viewProj.m);
    glUniform3f(uLightDir, 0.45f, 0.85f, 0.28f);
    glUniform3f(uFogColor, 0.55f, 0.71f, 0.92f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, atlasTexture);
    glUniform1i(uAtlas, 0);

    std::vector<const GpuChunk*> visibleTransparent;
    for (const auto& entry : gpuChunks) {
        const GpuChunk& gpu = entry.second;
        if (!frustum.intersectsAabb(gpu.bounds)) continue;
        if (gpu.opaque.indexCount > 0) {
            glBindVertexArray(gpu.opaque.vao);
            glDrawElements(GL_TRIANGLES, gpu.opaque.indexCount, GL_UNSIGNED_INT, nullptr);
        }
        if (gpu.transparent.indexCount > 0) visibleTransparent.push_back(&gpu);
    }

    std::sort(visibleTransparent.begin(), visibleTransparent.end(),
              [&](const GpuChunk* a, const GpuChunk* b) {
                  Vec3 da = a->center - eye;
                  Vec3 db = b->center - eye;
                  return dot(da, da) > dot(db, db);   // far first
              });

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    for (const GpuChunk* gpu : visibleTransparent) {
        glBindVertexArray(gpu->transparent.vao);
        glDrawElements(GL_TRIANGLES, gpu->transparent.indexCount, GL_UNSIGNED_INT, nullptr);
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

EM_BOOL onResize(int, const EmscriptenUiEvent*, void*) {
    double w, h;
    emscripten_get_element_css_size("#canvas", &w, &h);
    canvasWidth = static_cast<int>(w);
    canvasHeight = static_cast<int>(h);
    emscripten_set_canvas_element_size("#canvas", canvasWidth, canvasHeight);
    return EM_TRUE;
}

} // namespace

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
    buildProgram();
    uploadAtlas();

    ColumnCoord spawnColumn{0, 0};
    StreamDelta initial = streamer.update(spawnColumn);
    for (int dx = -1; dx <= 1; ++dx)
        for (int dz = -1; dz <= 1; ++dz)
            worldGen.generateColumn(world, spawnColumn.x + dx, spawnColumn.z + dz);
    for (ColumnCoord column : initial.load) genQueue.push_back(column);
    lastCentre = spawnColumn;

    int spawnX = 8, spawnZ = 8;
    int spawnY = worldGen.surfaceHeight(spawnX, spawnZ) + 3;
    player.position = {static_cast<float>(spawnX) + 0.5f, static_cast<float>(spawnY),
                       static_cast<float>(spawnZ) + 0.5f};
    refreshHud();

    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, onKey);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, onKey);
    emscripten_set_wheel_callback("#canvas", nullptr, EM_TRUE, onWheel);
    emscripten_set_mousemove_callback("#canvas", nullptr, EM_TRUE, onMouseMove);
    emscripten_set_mousedown_callback("#canvas", nullptr, EM_TRUE, onMouseDown);
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, onResize);

    lastTime = emscripten_get_now();
    emscripten_set_main_loop(frame, 0, EM_TRUE);
    return 0;
}
