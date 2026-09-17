#pragma once

#include <algorithm>
#include <cmath>

namespace self_ring {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kGameUnitToMetres = 0.0254f;

struct Vec3 { float x{}, y{}, z{}; };
struct Vec2 { float x{}, y{}; };
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
inline Vec3 operator*(Vec3 a, float s) { return {a.x*s, a.y*s, a.z*s}; }
inline float Dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
inline Vec3 Cross(Vec3 a, Vec3 b) {
    return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};
}
inline bool Finite(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
inline bool Normalize(Vec3& v) {
    const float length = std::sqrt(Dot(v, v));
    if (!std::isfinite(length) || length < 0.0001f) return false;
    v = v * (1.0f / length);
    return true;
}

struct Camera {
    Vec3 position, right, up, forward;
    float width{}, height{}, focal{};

    bool Configure(Vec3 pos, Vec3 facing, Vec3 top, float fov, float w, float h) {
        if (!Finite(pos) || !Finite(top) || !Normalize(facing) ||
            !std::isfinite(fov) || fov <= 0.05f || fov >= kPi-0.05f ||
            !std::isfinite(w) || !std::isfinite(h) || w <= 0 || h <= 0) return false;
        position = pos;
        forward = facing;
        right = Cross(top, forward);
        if (!Normalize(right)) return false;
        up = Cross(forward, right);
        width = w;
        height = h;
        focal = h / (2.0f * std::tan(fov * 0.5f));
        return true;
    }

    Vec3 ToView(Vec3 p) const {
        const Vec3 relative = p-position;
        return {Dot(relative, right), Dot(relative, up), Dot(relative, forward)};
    }
    bool Project(Vec3 world, Vec2& out) const {
        const auto view = ToView(world);
        if (!Finite(view) || view.z < 0.05f) return false;
        out = {width*0.5f + view.x*focal/view.z, height*0.5f - view.y*focal/view.z};
        return std::isfinite(out.x) && std::isfinite(out.y);
    }

    // Clip in camera space before perspective division; never join points behind the camera.
    bool Segment(Vec3 a, Vec3 b, Vec2& outA, Vec2& outB) const {
        a = ToView(a);
        b = ToView(b);
        if (!Finite(a) || !Finite(b) || (a.z < 0.05f && b.z < 0.05f)) return false;
        if (a.z < 0.05f) a = a + (b-a) * ((0.05f-a.z)/(b.z-a.z));
        if (b.z < 0.05f) b = b + (a-b) * ((0.05f-b.z)/(a.z-b.z));
        outA = {width*0.5f + a.x*focal/a.z, height*0.5f - a.y*focal/a.z};
        outB = {width*0.5f + b.x*focal/b.z, height*0.5f - b.y*focal/b.z};
        return std::isfinite(outA.x) && std::isfinite(outA.y) &&
               std::isfinite(outB.x) && std::isfinite(outB.y);
    }
};
} // namespace self_ring
