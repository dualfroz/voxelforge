#pragma once

#include <GLES3/gl3.h>

#include <vector>

#include "Sky.h"
#include "Village.h"
#include "Wildlife.h"

namespace vf {

class EntityRenderer {
public:
    void init();
    void draw(const SceneLook& look, TerrainView& terrain, const Wildlife& wildlife,
              const Village& village);

private:
    struct EntityVertex {
        float px, py, pz;
        float r, g, b;
    };

    void addBox(const Vec3& low, const Vec3& high, const Vec3& color);
    void addCreature(const WildCreature& wild, float brightness);
    void addVillager(const Villager& villager, float brightness);

    GLuint program = 0;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLint uViewProj = -1;
    GLint uFogColor = -1;
    GLint uFogRange = -1;
    std::vector<EntityVertex> vertices;
};

}
