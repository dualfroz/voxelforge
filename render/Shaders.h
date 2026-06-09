#pragma once

namespace vf {

inline const char* vertexShaderSource() {
    return R"(#version 300 es
precision highp float;
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv;
layout(location = 3) in float aTile;
layout(location = 4) in float aLight;
layout(location = 5) in float aAo;
uniform mat4 uViewProj;
out vec3 vNormal;
out vec2 vUv;
flat out int vTile;
out float vDepth;
out float vLight;
out float vAo;
void main() {
    vec4 clip = uViewProj * vec4(aPos, 1.0);
    gl_Position = clip;
    vNormal = aNormal;
    vUv = aUv;
    vTile = int(aTile + 0.5);
    vDepth = clip.w;
    vLight = aLight;
    vAo = aAo;
}
)";
}

inline const char* fragmentShaderSource() {
    return R"(#version 300 es
precision highp float;
precision highp sampler2DArray;
in vec3 vNormal;
in vec2 vUv;
flat in int vTile;
in float vDepth;
in float vLight;
in float vAo;
uniform sampler2DArray uAtlas;
uniform vec3 uLightDir;
uniform vec3 uFogColor;
out vec4 fragColor;
void main() {
    vec4 texel = texture(uAtlas, vec3(vUv, float(vTile)));
    float ndl = max(dot(normalize(vNormal), normalize(uLightDir)), 0.0);
    float directional = 0.55 + 0.45 * ndl;
    float baked = 0.18 + 0.82 * vLight;
    float ao = 0.45 + 0.55 * vAo;
    vec3 color = texel.rgb * directional * baked * ao;
    float fog = clamp((vDepth - 90.0) / 90.0, 0.0, 1.0);
    color = mix(color, uFogColor, fog);
    fragColor = vec4(color, texel.a);
}
)";
}

} // namespace vf
