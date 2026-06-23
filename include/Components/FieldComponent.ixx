module;
#include <array>
#include <algorithm>
#include <cmath>
#include <memory>
#include <random>
#include <QJsonObject>

export module Artifact.Component.Field;

export namespace Artifact {

// ---- AbstractFieldComponent ----
class AbstractFieldComponent {
public:
    virtual ~AbstractFieldComponent() = default;
    virtual float evaluateAt(const std::array<float, 3>& worldPos) const = 0;
    virtual QJsonObject toJson() const { return {}; }
    virtual void fromJson(const QJsonObject&) {}
};

// ---- SphericalFieldComponent ----
class SphericalFieldComponent : public AbstractFieldComponent {
public:
    std::array<float, 3> center = {0, 0, 0};
    float radius = 100.0f;
    float falloffWidth = 20.0f;

    float evaluateAt(const std::array<float, 3>& pos) const override {
        float dx = pos[0] - center[0], dy = pos[1] - center[1], dz = pos[2] - center[2];
        float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
        if (dist <= radius) return 1.0f;
        if (falloffWidth <= 0.0f || dist >= radius + falloffWidth) return 0.0f;
        float t = (dist - radius) / falloffWidth;
        return 1.0f - t * t * (3.0f - 2.0f * t);
    }

    QJsonObject toJson() const override {
        QJsonObject o;
        o["centerX"] = center[0]; o["centerY"] = center[1]; o["centerZ"] = center[2];
        o["radius"] = radius; o["falloffWidth"] = falloffWidth;
        return o;
    }
    void fromJson(const QJsonObject& o) override {
        center = {static_cast<float>(o["centerX"].toDouble()), static_cast<float>(o["centerY"].toDouble()), static_cast<float>(o["centerZ"].toDouble())};
        radius = static_cast<float>(o["radius"].toDouble(100.0));
        falloffWidth = static_cast<float>(o["falloffWidth"].toDouble(20.0));
    }
};

// ---- BoxFieldComponent ----
class BoxFieldComponent : public AbstractFieldComponent {
public:
    std::array<float, 3> center = {0, 0, 0};
    std::array<float, 3> halfExtent = {50, 50, 50};
    float falloffWidth = 10.0f;

    float evaluateAt(const std::array<float, 3>& pos) const override {
        float dx = std::max(0.0f, std::abs(pos[0] - center[0]) - halfExtent[0]);
        float dy = std::max(0.0f, std::abs(pos[1] - center[1]) - halfExtent[1]);
        float dz = std::max(0.0f, std::abs(pos[2] - center[2]) - halfExtent[2]);
        float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
        if (dist <= 0.0f) return 1.0f;
        if (falloffWidth <= 0.0f || dist >= falloffWidth) return 0.0f;
        float t = dist / falloffWidth;
        return 1.0f - t * t * (3.0f - 2.0f * t);
    }

    QJsonObject toJson() const override {
        QJsonObject o;
        o["centerX"] = center[0]; o["centerY"] = center[1]; o["centerZ"] = center[2];
        o["halfX"] = halfExtent[0]; o["halfY"] = halfExtent[1]; o["halfZ"] = halfExtent[2];
        o["falloffWidth"] = falloffWidth;
        return o;
    }
    void fromJson(const QJsonObject& o) override {
        center = {static_cast<float>(o["centerX"].toDouble()), static_cast<float>(o["centerY"].toDouble()), static_cast<float>(o["centerZ"].toDouble())};
        halfExtent = {static_cast<float>(o["halfX"].toDouble(50.0)), static_cast<float>(o["halfY"].toDouble(50.0)), static_cast<float>(o["halfZ"].toDouble(50.0))};
        falloffWidth = static_cast<float>(o["falloffWidth"].toDouble(10.0));
    }
};

// ---- LinearFieldComponent ----
class LinearFieldComponent : public AbstractFieldComponent {
public:
    std::array<float, 3> startPos = {0, 0, 0};
    std::array<float, 3> endPos = {0, 100, 0};
    bool useSmoothstep = true;

    float evaluateAt(const std::array<float, 3>& pos) const override {
        float dx = endPos[0] - startPos[0], dy = endPos[1] - startPos[1], dz = endPos[2] - startPos[2];
        float len = std::sqrt(dx*dx + dy*dy + dz*dz);
        if (len <= 0.0f) return 1.0f;
        float ndx = dx / len, ndy = dy / len, ndz = dz / len;
        float px = pos[0] - startPos[0], py = pos[1] - startPos[1], pz = pos[2] - startPos[2];
        float t = std::clamp((px*ndx + py*ndy + pz*ndz) / len, 0.0f, 1.0f);
        float v = 1.0f - t;
        if (useSmoothstep) v = v * v * (3.0f - 2.0f * v);
        return v;
    }

    QJsonObject toJson() const override {
        QJsonObject o;
        o["sx"] = startPos[0]; o["sy"] = startPos[1]; o["sz"] = startPos[2];
        o["ex"] = endPos[0]; o["ey"] = endPos[1]; o["ez"] = endPos[2];
        o["useSmoothstep"] = useSmoothstep;
        return o;
    }
    void fromJson(const QJsonObject& o) override {
        startPos = {static_cast<float>(o["sx"].toDouble()), static_cast<float>(o["sy"].toDouble()), static_cast<float>(o["sz"].toDouble())};
        endPos = {static_cast<float>(o["ex"].toDouble()), static_cast<float>(o["ey"].toDouble(100.0)), static_cast<float>(o["ez"].toDouble())};
        useSmoothstep = o["useSmoothstep"].toBool(true);
    }
};

// ---- RadialFieldComponent ----
class RadialFieldComponent : public AbstractFieldComponent {
public:
    std::array<float, 3> center = {0, 0, 0};
    std::array<float, 3> axis = {0, 1, 0};
    float innerRadius = 0.0f;
    float outerRadius = 100.0f;

    float evaluateAt(const std::array<float, 3>& pos) const override {
        float dx = pos[0] - center[0], dy = pos[1] - center[1], dz = pos[2] - center[2];
        float axLen = std::sqrt(axis[0]*axis[0] + axis[1]*axis[1] + axis[2]*axis[2]);
        if (axLen <= 0.0f) return 1.0f;
        float nx = axis[0] / axLen, ny = axis[1] / axLen, nz = axis[2] / axLen;
        float dot = dx*nx + dy*ny + dz*nz;
        float px = dx - dot*nx, py = dy - dot*ny, pz = dz - dot*nz;
        float dist = std::sqrt(px*px + py*py + pz*pz);
        if (dist <= innerRadius) return 1.0f;
        if (outerRadius <= innerRadius || dist >= outerRadius) return 0.0f;
        float t = (dist - innerRadius) / (outerRadius - innerRadius);
        return 1.0f - t * t * (3.0f - 2.0f * t);
    }

    QJsonObject toJson() const override {
        QJsonObject o;
        o["cx"] = center[0]; o["cy"] = center[1]; o["cz"] = center[2];
        o["ax"] = axis[0]; o["ay"] = axis[1]; o["az"] = axis[2];
        o["innerRadius"] = innerRadius; o["outerRadius"] = outerRadius;
        return o;
    }
    void fromJson(const QJsonObject& o) override {
        center = {static_cast<float>(o["cx"].toDouble()), static_cast<float>(o["cy"].toDouble()), static_cast<float>(o["cz"].toDouble())};
        axis = {static_cast<float>(o["ax"].toDouble(0.0)), static_cast<float>(o["ay"].toDouble(1.0)), static_cast<float>(o["az"].toDouble(0.0))};
        innerRadius = static_cast<float>(o["innerRadius"].toDouble());
        outerRadius = static_cast<float>(o["outerRadius"].toDouble(100.0));
    }
};

// ---- NoiseFieldComponent ----
class NoiseFieldComponent : public AbstractFieldComponent {
public:
    std::array<float, 3> center = {0, 0, 0};
    float scale = 100.0f;
    float amplitude = 1.0f;
    int octaves = 3;
    int seed = 12345;

    float evaluateAt(const std::array<float, 3>& pos) const override {
        float nx = pos[0] / scale, ny = pos[1] / scale, nz = pos[2] / scale;
        float value = 0.0f, amp = amplitude, freq = 1.0f;
        for (int i = 0; i < octaves; ++i) {
            value += amp * perlin(nx * freq, ny * freq, nz * freq);
            amp *= 0.5f;
            freq *= 2.0f;
        }
        return std::clamp(value * 0.5f + 0.5f, 0.0f, 1.0f);
    }

    QJsonObject toJson() const override {
        QJsonObject o;
        o["cx"] = center[0]; o["cy"] = center[1]; o["cz"] = center[2];
        o["scale"] = scale; o["amplitude"] = amplitude;
        o["octaves"] = octaves; o["seed"] = seed;
        return o;
    }
    void fromJson(const QJsonObject& o) override {
        center = {static_cast<float>(o["cx"].toDouble()), static_cast<float>(o["cy"].toDouble()), static_cast<float>(o["cz"].toDouble())};
        scale = static_cast<float>(o["scale"].toDouble(100.0));
        amplitude = static_cast<float>(o["amplitude"].toDouble(1.0));
        octaves = o["octaves"].toInt(3);
        seed = o["seed"].toInt(12345);
    }

private:
    static float hash(const std::array<float, 3>& p) {
        float v = std::sin(p[0] * 127.1f + p[1] * 311.7f + p[2] * 74.7f) * 43758.5453f;
        return v - std::floor(v);
    }
    static float lerp(float a, float b, float t) { return a + t * (b - a); }
    static float smoothstep(float t) { return t * t * (3.0f - 2.0f * t); }
    static float perlin(float x, float y, float z) {
        int ix = static_cast<int>(std::floor(x));
        int iy = static_cast<int>(std::floor(y));
        int iz = static_cast<int>(std::floor(z));
        float fx = x - ix, fy = y - iy, fz = z - iz;
        float sx = smoothstep(fx), sy = smoothstep(fy), sz = smoothstep(fz);
        const float n000 = hash({static_cast<float>(ix), static_cast<float>(iy), static_cast<float>(iz)}) * 2.0f - 1.0f;
        const float n100 = hash({static_cast<float>(ix + 1), static_cast<float>(iy), static_cast<float>(iz)}) * 2.0f - 1.0f;
        const float n010 = hash({static_cast<float>(ix), static_cast<float>(iy + 1), static_cast<float>(iz)}) * 2.0f - 1.0f;
        const float n110 = hash({static_cast<float>(ix + 1), static_cast<float>(iy + 1), static_cast<float>(iz)}) * 2.0f - 1.0f;
        const float n001 = hash({static_cast<float>(ix), static_cast<float>(iy), static_cast<float>(iz + 1)}) * 2.0f - 1.0f;
        const float n101 = hash({static_cast<float>(ix + 1), static_cast<float>(iy), static_cast<float>(iz + 1)}) * 2.0f - 1.0f;
        const float n011 = hash({static_cast<float>(ix), static_cast<float>(iy + 1), static_cast<float>(iz + 1)}) * 2.0f - 1.0f;
        const float n111 = hash({static_cast<float>(ix + 1), static_cast<float>(iy + 1), static_cast<float>(iz + 1)}) * 2.0f - 1.0f;
        float nx00 = lerp(n000, n100, sx), nx10 = lerp(n010, n110, sx);
        float nx01 = lerp(n001, n101, sx), nx11 = lerp(n011, n111, sx);
        float nxy0 = lerp(nx00, nx10, sy), nxy1 = lerp(nx01, nx11, sy);
        return lerp(nxy0, nxy1, sz);
    }
};

// ---- SolidFieldComponent ----
class SolidFieldComponent : public AbstractFieldComponent {
public:
    float value = 1.0f;

    float evaluateAt(const std::array<float, 3>&) const override { return value; }

    QJsonObject toJson() const override {
        QJsonObject o; o["value"] = value; return o;
    }
    void fromJson(const QJsonObject& o) override {
        value = static_cast<float>(o["value"].toDouble(1.0));
    }
};

} // namespace Artifact
