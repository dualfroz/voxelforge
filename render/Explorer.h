#pragma once

#include <map>

#include "vf/Item.h"
#include "vf/Physics.h"
#include "vf/Survival.h"

namespace vf {

struct ExplorerControls {
    float forward = 0.0f;
    float strafe = 0.0f;
    bool jump = false;
    bool sink = false;
};

class Explorer {
public:
    Explorer();

    void move(const World& world, const ExplorerControls& controls, float dt);
    void tick(float dt);
    void hurt(DamageSource source, float amount);
    void respawn(const Vec3& position);
    void stash(ItemId item, int count) { pantry[item] += count; }
    ItemId eatBest();

    Player& body() { return player; }
    const Player& body() const { return player; }
    const SurvivalState& vitals() const { return survival; }
    const SurvivalConfig& limits() const { return config; }
    bool swimming() const { return inWater; }
    bool submerged() const { return underwater; }
    int foodCount() const;

private:
    void sampleWater(const World& world);
    void trackFall();

    Player player;
    PhysicsConfig landPhysics;
    PhysicsConfig waterPhysics;
    SurvivalConfig config;
    SurvivalState survival;
    std::map<ItemId, int> pantry;
    bool inWater = false;
    bool underwater = false;
    bool wasGrounded = true;
    float fallPeak = 0.0f;
    Vec3 lastTickPosition;
};

}
