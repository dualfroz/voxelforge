#include "Sky.h"

#include <algorithm>
#include <cmath>

namespace vf {

namespace {

const Vec3 nightSky{0.02f, 0.03f, 0.07f};
const Vec3 daySky{0.55f, 0.71f, 0.92f};
const Vec3 horizonGlow{0.92f, 0.52f, 0.32f};
const Vec3 rainSky{0.42f, 0.45f, 0.5f};
const Vec3 flashSky{0.9f, 0.92f, 1.0f};
const Vec3 deepWater{0.04f, 0.16f, 0.3f};

Vec3 mixColor(const Vec3& a, const Vec3& b, float t) { return a * (1.0f - t) + b * t; }

}

SceneLook composeLook(const SkyInputs& inputs) {
    const TimeOfDay& clock = *inputs.clock;
    float brightness = static_cast<float>(clock.skyBrightness());
    float rain = static_cast<float>(inputs.weather->rainGradient());
    float sunHeight = static_cast<float>(clock.sunHeight());
    float angle = static_cast<float>((clock.dayFraction() - clock.settings().sunriseFraction) * 6.2831853);

    SceneLook look;
    look.sunDir = normalize(Vec3{std::cos(angle), std::sin(angle), 0.35f});
    look.sunStrength = std::clamp(sunHeight * 2.5f, 0.0f, 1.0f) * (1.0f - 0.6f * rain);

    Vec3 sky = mixColor(nightSky, daySky, brightness);
    float horizon = std::clamp(1.0f - std::fabs(sunHeight) * 4.0f, 0.0f, 1.0f);
    sky = mixColor(sky, horizonGlow * std::max(brightness, 0.4f), horizon * 0.45f);
    sky = mixColor(sky, rainSky * brightness, rain * 0.7f);
    sky = mixColor(sky, flashSky, inputs.lightningFlash * 0.8f);

    const Player& viewer = *inputs.viewer;
    Vec3 eye = eyePosition(viewer);
    Mat4 view = lookAt(eye, eye + directionFromYawPitch(viewer.yaw, viewer.pitch), {0, 1, 0});
    look.viewProj = perspective(1.2217f, inputs.aspect, 0.1f, 480.0f) * view;
    look.fogColor = sky;
    look.fogStart = inputs.viewDistance * 0.55f;
    look.fogEnd = inputs.viewDistance;
    if (inputs.submerged) {
        look.fogColor = deepWater * (0.3f + 0.7f * brightness);
        look.fogStart = 0.0f;
        look.fogEnd = 22.0f;
    }
    return look;
}

}
