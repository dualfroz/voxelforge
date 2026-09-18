#pragma once

#include "vf/Physics.h"
#include "vf/TimeOfDay.h"
#include "vf/Weather.h"

namespace vf {

struct SceneLook {
    Mat4 viewProj;
    Vec3 fogColor;
    float fogStart = 90.0f;
    float fogEnd = 180.0f;
    Vec3 sunDir{0.0f, 1.0f, 0.0f};
    float sunStrength = 1.0f;
};

struct SkyInputs {
    const TimeOfDay* clock = nullptr;
    const Weather* weather = nullptr;
    const Player* viewer = nullptr;
    float aspect = 1.0f;
    float viewDistance = 96.0f;
    float lightningFlash = 0.0f;
    bool submerged = false;
};

SceneLook composeLook(const SkyInputs& inputs);

}
