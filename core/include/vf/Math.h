#pragma once

#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace vf {

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;

    Vec3() = default;
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator-() const { return {-x, -y, -z}; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
};

inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline float length(const Vec3& v) { return std::sqrt(dot(v, v)); }

inline Vec3 normalize(const Vec3& v) {
    float len = length(v);
    if (len <= 1e-8f) return {0.0f, 0.0f, 0.0f};
    float inv = 1.0f / len;
    return {v.x * inv, v.y * inv, v.z * inv};
}

struct Vec3i {
    int x = 0, y = 0, z = 0;

    Vec3i() = default;
    Vec3i(int x, int y, int z) : x(x), y(y), z(z) {}

    bool operator==(const Vec3i& o) const { return x == o.x && y == o.y && z == o.z; }
    bool operator!=(const Vec3i& o) const { return !(*this == o); }
    Vec3i operator+(const Vec3i& o) const { return {x + o.x, y + o.y, z + o.z}; }
};

struct Vec3iHash {
    std::size_t operator()(const Vec3i& v) const {
        std::uint64_t h = 1469598103934665603ull;
        for (int c : {v.x, v.y, v.z}) {
            h ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(c));
            h *= 1099511628211ull;
        }
        return static_cast<std::size_t>(h);
    }
};

struct Mat4 {
    float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

    float& at(int col, int row) { return m[col * 4 + row]; }
    float at(int col, int row) const { return m[col * 4 + row]; }

    static Mat4 identity() { return Mat4{}; }

    Mat4 operator*(const Mat4& o) const {
        Mat4 out;
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k) sum += at(k, row) * o.at(col, k);
                out.at(col, row) = sum;
            }
        }
        return out;
    }
};

inline Mat4 translation(const Vec3& t) {
    Mat4 out;
    out.at(3, 0) = t.x;
    out.at(3, 1) = t.y;
    out.at(3, 2) = t.z;
    return out;
}

inline Mat4 perspective(float fovYRadians, float aspect, float near, float far) {
    float f = 1.0f / std::tan(fovYRadians * 0.5f);
    Mat4 out;
    for (int i = 0; i < 16; ++i) out.m[i] = 0.0f;
    out.at(0, 0) = f / aspect;
    out.at(1, 1) = f;
    out.at(2, 2) = (far + near) / (near - far);
    out.at(2, 3) = -1.0f;
    out.at(3, 2) = (2.0f * far * near) / (near - far);
    return out;
}

inline Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
    Vec3 forward = normalize(target - eye);
    Vec3 right = normalize(cross(forward, up));
    Vec3 trueUp = cross(right, forward);
    Mat4 out;
    out.at(0, 0) = right.x;   out.at(1, 0) = right.y;   out.at(2, 0) = right.z;
    out.at(0, 1) = trueUp.x;  out.at(1, 1) = trueUp.y;  out.at(2, 1) = trueUp.z;
    out.at(0, 2) = -forward.x; out.at(1, 2) = -forward.y; out.at(2, 2) = -forward.z;
    out.at(3, 0) = -dot(right, eye);
    out.at(3, 1) = -dot(trueUp, eye);
    out.at(3, 2) = dot(forward, eye);
    return out;
}

inline Vec3 directionFromYawPitch(float yawRadians, float pitchRadians) {
    return {
        std::cos(pitchRadians) * std::sin(yawRadians),
        std::sin(pitchRadians),
        -std::cos(pitchRadians) * std::cos(yawRadians),
    };
}

}
