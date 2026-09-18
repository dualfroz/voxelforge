#include "EntityRenderer.h"

#include <cmath>

#include "Gpu.h"
#include "Shaders.h"

namespace vf {

namespace {

struct Palette {
    Vec3 body;
    Vec3 head;
    Vec3 legs;
};

Palette speciesPalette(Species species) {
    switch (species) {
        case Species::Sheep: return {{0.93f, 0.93f, 0.90f}, {0.74f, 0.68f, 0.62f}, {0.62f, 0.58f, 0.52f}};
        case Species::Cow: return {{0.36f, 0.24f, 0.16f}, {0.56f, 0.46f, 0.38f}, {0.24f, 0.17f, 0.12f}};
        case Species::Pig: return {{0.94f, 0.62f, 0.62f}, {0.97f, 0.71f, 0.70f}, {0.82f, 0.52f, 0.52f}};
        case Species::Zombie: return {{0.17f, 0.45f, 0.56f}, {0.36f, 0.62f, 0.30f}, {0.24f, 0.22f, 0.50f}};
        case Species::Skeleton: return {{0.80f, 0.80f, 0.78f}, {0.90f, 0.90f, 0.88f}, {0.66f, 0.66f, 0.64f}};
        case Species::Creeper: return {{0.30f, 0.68f, 0.28f}, {0.38f, 0.78f, 0.33f}, {0.20f, 0.48f, 0.20f}};
        default: return {{0.8f, 0.8f, 0.8f}, {0.8f, 0.8f, 0.8f}, {0.8f, 0.8f, 0.8f}};
    }
}

Vec3 robeColor(VillagerProfession profession) {
    static const Vec3 robes[] = {
        {0.55f, 0.42f, 0.30f}, {0.36f, 0.55f, 0.30f}, {0.80f, 0.68f, 0.30f}, {0.28f, 0.46f, 0.66f},
        {0.86f, 0.86f, 0.84f}, {0.52f, 0.36f, 0.22f}, {0.70f, 0.20f, 0.18f}, {0.86f, 0.80f, 0.54f},
        {0.56f, 0.24f, 0.62f}, {0.30f, 0.30f, 0.34f}, {0.20f, 0.20f, 0.22f}, {0.46f, 0.34f, 0.24f},
        {0.92f, 0.92f, 0.90f}, {0.62f, 0.40f, 0.24f}, {0.60f, 0.60f, 0.58f},
    };
    int index = static_cast<int>(profession);
    if (index < 0 || index >= static_cast<int>(sizeof(robes) / sizeof(robes[0]))) index = 0;
    return robes[index];
}

Vec3 tinted(const Vec3& color, float brightness, float hurt) {
    Vec3 lit = color * brightness;
    if (hurt <= 0.0f) return lit;
    Vec3 red{0.95f * brightness + 0.05f, 0.15f * brightness, 0.15f * brightness};
    return lit * (1.0f - hurt) + red * hurt;
}

float sampleBrightness(TerrainView& terrain, const Vec3& position, float height) {
    int gx = static_cast<int>(std::floor(position.x));
    int gy = static_cast<int>(std::floor(position.y + height * 0.5f));
    int gz = static_cast<int>(std::floor(position.z));
    ColumnCoord column{floorDiv(gx, chunkSize), floorDiv(gz, chunkSize)};
    if (!terrain.columnReady(column)) return 1.0f;
    float sky = static_cast<float>(terrain.skyLevel()) / static_cast<float>(maxLight);
    float light = terrain.rawLight(column).combinedAtWorld(gx, gy, gz, sky);
    return 0.12f + 0.88f * light;
}

}

void EntityRenderer::init() {
    program = buildProgram(entityVertexShaderSource(), entityFragmentShaderSource());
    uViewProj = glGetUniformLocation(program, "uViewProj");
    uFogColor = glGetUniformLocation(program, "uFogColor");
    uFogRange = glGetUniformLocation(program, "uFogRange");
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    GLsizei stride = sizeof(EntityVertex);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
}

void EntityRenderer::addBox(const Vec3& low, const Vec3& high, const Vec3& color) {
    const Vec3 corners[8] = {
        {low.x, low.y, low.z},   {high.x, low.y, low.z},   {high.x, high.y, low.z},
        {low.x, high.y, low.z},  {low.x, low.y, high.z},   {high.x, low.y, high.z},
        {high.x, high.y, high.z}, {low.x, high.y, high.z},
    };
    const int faces[6][4] = {{4, 5, 6, 7}, {1, 0, 3, 2}, {5, 1, 2, 6},
                             {0, 4, 7, 3}, {7, 6, 2, 3}, {0, 1, 5, 4}};
    const float faceShade[6] = {0.82f, 0.82f, 0.68f, 0.68f, 1.0f, 0.5f};
    for (int face = 0; face < 6; ++face) {
        Vec3 shaded = color * faceShade[face];
        const int order[6] = {0, 1, 2, 0, 2, 3};
        for (int corner : order) {
            const Vec3& p = corners[faces[face][corner]];
            vertices.push_back({p.x, p.y, p.z, shaded.x, shaded.y, shaded.z});
        }
    }
}

void EntityRenderer::addCreature(const WildCreature& wild, float brightness) {
    const Creature& creature = wild.creature;
    const Mob& body = creature.body;
    Palette palette = speciesPalette(creature.species);
    float hurt = creature.alive ? wild.hurtSeconds / 0.3f : 1.0f;
    Vec3 p = body.position;
    float w = body.width * 0.5f;
    float h = body.height;
    Vec3 heading = normalize(Vec3{body.wanderDir.x, 0.0f, body.wanderDir.z});
    Vec3 bodyColor = tinted(palette.body, brightness, hurt);
    Vec3 headColor = tinted(palette.head, brightness, hurt);
    Vec3 legColor = tinted(palette.legs, brightness, hurt);

    if (isPassive(creature.species)) {
        addBox({p.x - w * 0.7f, p.y, p.z - w * 0.7f}, {p.x + w * 0.7f, p.y + h * 0.35f, p.z + w * 0.7f},
               legColor);
        addBox({p.x - w, p.y + h * 0.35f, p.z - w}, {p.x + w, p.y + h * 0.8f, p.z + w}, bodyColor);
        float head = h * 0.24f;
        Vec3 c = p + heading * (w + head * 0.5f);
        addBox({c.x - head, p.y + h * 0.62f, c.z - head}, {c.x + head, p.y + h * 0.62f + head * 2.0f, c.z + head},
               headColor);
        return;
    }
    if (creature.species == Species::Creeper) {
        addBox({p.x - w * 0.8f, p.y, p.z - w * 0.8f}, {p.x + w * 0.8f, p.y + h * 0.72f, p.z + w * 0.8f},
               bodyColor);
        float head = h * 0.16f;
        addBox({p.x - head, p.y + h * 0.72f, p.z - head}, {p.x + head, p.y + h, p.z + head}, headColor);
        return;
    }
    addBox({p.x - w * 0.6f, p.y, p.z - w * 0.6f}, {p.x + w * 0.6f, p.y + h * 0.42f, p.z + w * 0.6f},
           legColor);
    addBox({p.x - w, p.y + h * 0.42f, p.z - w * 0.7f}, {p.x + w, p.y + h * 0.76f, p.z + w * 0.7f},
           bodyColor);
    float head = h * 0.12f;
    addBox({p.x - head, p.y + h * 0.76f, p.z - head}, {p.x + head, p.y + h, p.z + head}, headColor);
}

void EntityRenderer::addVillager(const Villager& villager, float brightness) {
    const Mob& body = villager.body;
    Vec3 p = body.position;
    float w = body.width * 0.5f;
    float h = body.height;
    Vec3 robe = tinted(robeColor(villager.profession), brightness, 0.0f);
    Vec3 skin = tinted({0.80f, 0.62f, 0.48f}, brightness, 0.0f);
    bool asleep = villager.activity == VillagerActivity::Sleep &&
                  villager.state == VillagerState::AtTarget;
    if (asleep) {
        addBox({p.x - w * 1.6f, p.y, p.z - w}, {p.x + w * 1.6f, p.y + w * 1.2f, p.z + w}, robe);
        return;
    }
    addBox({p.x - w, p.y, p.z - w * 0.8f}, {p.x + w, p.y + h * 0.74f, p.z + w * 0.8f}, robe);
    float head = h * 0.13f;
    addBox({p.x - head, p.y + h * 0.74f, p.z - head}, {p.x + head, p.y + h, p.z + head}, skin);
    addBox({p.x - head * 0.25f, p.y + h * 0.78f, p.z - head - 0.12f},
           {p.x + head * 0.25f, p.y + h * 0.88f, p.z - head}, skin * 0.9f);
}

void EntityRenderer::draw(const SceneLook& look, TerrainView& terrain, const Wildlife& wildlife,
                          const Village& village) {
    vertices.clear();
    for (const WildCreature& wild : wildlife.creatures())
        addCreature(wild, sampleBrightness(terrain, wild.creature.body.position,
                                           wild.creature.body.height));
    for (const Villager& villager : village.villagers()) {
        if (!villager.alive) continue;
        addVillager(villager, sampleBrightness(terrain, villager.body.position, villager.body.height));
    }
    if (vertices.empty()) return;

    glUseProgram(program);
    glUniformMatrix4fv(uViewProj, 1, GL_FALSE, look.viewProj.m);
    glUniform3f(uFogColor, look.fogColor.x, look.fogColor.y, look.fogColor.z);
    glUniform2f(uFogRange, look.fogStart, look.fogEnd);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(EntityVertex), vertices.data(),
                 GL_STREAM_DRAW);
    glDisable(GL_CULL_FACE);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
    glEnable(GL_CULL_FACE);
}

}
