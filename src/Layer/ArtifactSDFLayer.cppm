module;
#include <utility>
#include <cmath>
#include <algorithm>
#include <vector>
#include <wobjectimpl.h>
#include <QImage>
#include <QPainter>
#include <QMatrix4x4>
#include <QVariant>

module Artifact.Layer.SDF;

import Artifact.Layer.Abstract;
import Animation.Transform3D;
import Time.Rational;
import Property.Group;
import Property;
import Color.Float;

namespace Artifact {

// ---------------------------------------------------------------------------
// Internal math helpers
// ---------------------------------------------------------------------------
namespace {

struct Vec3 { float x, y, z; };

inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
inline Vec3 operator*(Vec3 a, float s) { return {a.x*s, a.y*s, a.z*s}; }
inline Vec3 operator*(float s, Vec3 a) { return a*s; }
inline float dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
inline float len(Vec3 a) { return std::sqrt(dot(a,a)); }
inline Vec3  normalize(Vec3 a) { float l = len(a); return l > 1e-6f ? a*(1.0f/l) : Vec3{0,0,1}; }
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline Vec3 absv(Vec3 a) { return {std::abs(a.x), std::abs(a.y), std::abs(a.z)}; }
inline Vec3 maxv(Vec3 a, Vec3 b) { return {std::max(a.x,b.x), std::max(a.y,b.y), std::max(a.z,b.z)}; }

// Rotate a point by Euler angles (degrees, ZYX order) around origin
Vec3 applyRotation(Vec3 p, float rx, float ry, float rz) {
    auto toRad = [](float d) { return d * (3.14159265358979f / 180.0f); };
    float cx = std::cos(toRad(rx)), sx = std::sin(toRad(rx));
    float cy = std::cos(toRad(ry)), sy = std::sin(toRad(ry));
    float cz = std::cos(toRad(rz)), sz = std::sin(toRad(rz));
    // Z
    float x1 = cz*p.x - sz*p.y, y1 = sz*p.x + cz*p.y, z1 = p.z;
    // Y
    float x2 =  cy*x1 + sy*z1, y2 = y1, z2 = -sy*x1 + cy*z1;
    // X
    float x3 = x2, y3 = cx*y2 - sx*z2, z3 = sx*y2 + cx*z2;
    return {x3, y3, z3};
}

// ---------------------------------------------------------------------------
// SDF primitives (returns signed distance in world units)
// ---------------------------------------------------------------------------
float sdSphere(Vec3 p, float r) {
    return len(p) - r;
}

float sdBox(Vec3 p, Vec3 b) {
    Vec3 q = absv(p) - b;
    return len(maxv(q, {0,0,0})) + std::min(std::max(q.x, std::max(q.y, q.z)), 0.0f);
}

float sdTorus(Vec3 p, float R, float r) {
    float qx = std::sqrt(p.x*p.x + p.z*p.z) - R;
    return std::sqrt(qx*qx + p.y*p.y) - r;
}

// ---------------------------------------------------------------------------
// Per-object SDF evaluation
// ---------------------------------------------------------------------------
float evalObject(Vec3 p, const SDFObject& obj) {
    // Transform point into object space
    Vec3 lp = p - Vec3{obj.posX, obj.posY, obj.posZ};
    lp = applyRotation(lp, -obj.rotX, -obj.rotY, -obj.rotZ);
    // Scale
    float sx = obj.scaleX > 0.001f ? obj.scaleX : 0.001f;
    float sy = obj.scaleY > 0.001f ? obj.scaleY : 0.001f;
    float sz = obj.scaleZ > 0.001f ? obj.scaleZ : 0.001f;
    Vec3 sp = {lp.x / sx, lp.y / sy, lp.z / sz};
    float minScale = std::min({sx, sy, sz});

    float d = 1e30f;
    switch (obj.shapeType) {
        case SDFShapeType::Sphere:
            d = sdSphere(sp, 1.0f) * minScale;
            break;
        case SDFShapeType::Box:
            d = sdBox(sp, {1.0f, 1.0f, 1.0f}) * minScale;
            break;
        case SDFShapeType::Torus: {
            float minorRatio = clampf(obj.param0, 0.05f, 0.49f);
            d = sdTorus(sp, 1.0f - minorRatio, minorRatio) * minScale;
            break;
        }
    }
    return d;
}

// Smooth-min (Inigo Quilez polynomial)
float smin(float a, float b, float k) {
    if (k < 1e-6f) return std::min(a, b);
    float h = clampf(0.5f + 0.5f*(b-a)/k, 0.0f, 1.0f);
    return a + h*(b-a) - k*h*(1.0f-h);
}

// ---------------------------------------------------------------------------
// Scene evaluation with colour blending
// ---------------------------------------------------------------------------
struct SceneResult { float dist; ArtifactCore::FloatColor color; };

SceneResult evalScene(Vec3 p,
                      const std::vector<SDFObject>& objects,
                      SDFOp op, float k)
{
    SceneResult res{1e30f, ArtifactCore::FloatColor{0,0,0,0}};
    if (objects.empty()) return res;

    res.dist = evalObject(p, objects[0]);
    res.color = objects[0].color;

    for (int i = 1; i < static_cast<int>(objects.size()); ++i) {
        float d2 = evalObject(p, objects[i]);
        const auto& c2 = objects[i].color;

        if (op == SDFOp::SmoothUnion && k > 1e-6f) {
            float h = clampf(0.5f + 0.5f*(d2-res.dist)/k, 0.0f, 1.0f);
            // Blend colour proportionally
            float blend = h;
            res.color = ArtifactCore::FloatColor{
                res.color.r() + blend*(c2.r()-res.color.r()),
                res.color.g() + blend*(c2.g()-res.color.g()),
                res.color.b() + blend*(c2.b()-res.color.b()),
                res.color.a() + blend*(c2.a()-res.color.a())
            };
            res.dist = smin(res.dist, d2, k);
        } else {
            if (d2 < res.dist) {
                res.dist = d2;
                res.color = c2;
            }
        }
    }
    return res;
}

// Estimate surface normal via central differences
Vec3 calcNormal(Vec3 p,
                const std::vector<SDFObject>& objects,
                SDFOp op, float k)
{
    const float e = 0.5f;
    auto sd = [&](Vec3 q){ return evalScene(q, objects, op, k).dist; };
    Vec3 n{
        sd({p.x+e,p.y,p.z}) - sd({p.x-e,p.y,p.z}),
        sd({p.x,p.y+e,p.z}) - sd({p.x,p.y-e,p.z}),
        sd({p.x,p.y,p.z+e}) - sd({p.x,p.y,p.z-e})
    };
    return normalize(n);
}

// ---------------------------------------------------------------------------
// Blinn-Phong shading
// ---------------------------------------------------------------------------
ArtifactCore::FloatColor shade(Vec3 hitPos, Vec3 normal,
                               const ArtifactCore::FloatColor& surfaceColor,
                               Vec3 camDir)
{
    // Key light
    Vec3 lightDir = normalize({1.0f, -1.5f, -1.0f});
    float diff = clampf(-dot(normal, lightDir), 0.0f, 1.0f);

    // Rim / fill
    float rim = clampf(1.0f - std::abs(dot(normal, camDir)), 0.0f, 1.0f) * 0.25f;
    float ambient = 0.15f;

    float lit = ambient + diff * 0.75f + rim;
    lit = clampf(lit, 0.0f, 1.0f);

    // Specular (Blinn-Phong)
    Vec3 viewDir = normalize({-camDir.x, -camDir.y, -camDir.z});
    Vec3 halfVec = normalize({lightDir.x + viewDir.x,
                              lightDir.y + viewDir.y,
                              lightDir.z + viewDir.z} * -1.0f);
    float spec = std::pow(clampf(dot(normal, halfVec), 0.0f, 1.0f), 32.0f) * 0.4f;

    return ArtifactCore::FloatColor{
        clampf(surfaceColor.r() * lit + spec, 0.0f, 1.0f),
        clampf(surfaceColor.g() * lit + spec, 0.0f, 1.0f),
        clampf(surfaceColor.b() * lit + spec, 0.0f, 1.0f),
        surfaceColor.a()
    };
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// ArtifactSDFLayer::Impl
// ---------------------------------------------------------------------------
struct ArtifactSDFLayer::Impl {
    std::vector<SDFObject> objects;
    SDFOp op = SDFOp::SmoothUnion;
    float smoothK = 20.0f;
    int renderW = 512;
    int renderH = 288;
};

W_OBJECT_IMPL(ArtifactSDFLayer)

ArtifactSDFLayer::ArtifactSDFLayer()
    : sdfImpl_(new Impl())
{
    setLayerName("SDF Layer 1");

    // Default scene: one sphere
    SDFObject sphere;
    sphere.shapeType = SDFShapeType::Sphere;
    sphere.scaleX = sphere.scaleY = sphere.scaleZ = 80.0f;
    sphere.color = ArtifactCore::FloatColor{0.3f, 0.6f, 1.0f, 1.0f};
    sdfImpl_->objects.push_back(sphere);
}

ArtifactSDFLayer::~ArtifactSDFLayer()
{
    delete sdfImpl_;
}

// ---------------------------------------------------------------------------
// SDF scene management
// ---------------------------------------------------------------------------
void ArtifactSDFLayer::clearObjects()              { sdfImpl_->objects.clear(); Q_EMIT changed(); }
void ArtifactSDFLayer::addObject(const SDFObject& obj) { sdfImpl_->objects.push_back(obj); Q_EMIT changed(); }
void ArtifactSDFLayer::setObjectAt(int i, const SDFObject& obj) {
    if (i >= 0 && i < static_cast<int>(sdfImpl_->objects.size())) {
        sdfImpl_->objects[i] = obj;
        Q_EMIT changed();
    }
}
void ArtifactSDFLayer::removeObjectAt(int i) {
    if (i >= 0 && i < static_cast<int>(sdfImpl_->objects.size())) {
        sdfImpl_->objects.erase(sdfImpl_->objects.begin() + i);
        Q_EMIT changed();
    }
}
int ArtifactSDFLayer::objectCount() const { return static_cast<int>(sdfImpl_->objects.size()); }
const SDFObject& ArtifactSDFLayer::objectAt(int i) const { return sdfImpl_->objects[i]; }
SDFOp ArtifactSDFLayer::combineOp() const { return sdfImpl_->op; }
void ArtifactSDFLayer::setCombineOp(SDFOp op) { sdfImpl_->op = op; Q_EMIT changed(); }
float ArtifactSDFLayer::smoothK() const { return sdfImpl_->smoothK; }
void ArtifactSDFLayer::setSmoothing(float k) { sdfImpl_->smoothK = std::max(0.0f, k); Q_EMIT changed(); }
int ArtifactSDFLayer::renderWidth() const { return sdfImpl_->renderW; }
int ArtifactSDFLayer::renderHeight() const { return sdfImpl_->renderH; }
void ArtifactSDFLayer::setRenderSize(int w, int h) {
    sdfImpl_->renderW = std::max(1, w);
    sdfImpl_->renderH = std::max(1, h);
    Q_EMIT changed();
}

// ---------------------------------------------------------------------------
// CPU ray marching → QImage
// ---------------------------------------------------------------------------
QImage ArtifactSDFLayer::toQImage() const
{
    const int W = sdfImpl_->renderW;
    const int H = sdfImpl_->renderH;
    QImage img(W, H, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    if (sdfImpl_->objects.empty()) return img;

    // Camera: look toward -Z, positioned at +Z
    const float camDist = std::max(sdfImpl_->renderW, sdfImpl_->renderH) * 1.5f;
    const Vec3 camPos{0.0f, 0.0f, camDist};
    const Vec3 camDir{0.0f, 0.0f, -1.0f};

    const float fovFactor = 0.7f;
    const float halfW = static_cast<float>(W) * 0.5f;
    const float halfH = static_cast<float>(H) * 0.5f;

    const auto& objs = sdfImpl_->objects;
    const SDFOp op = sdfImpl_->op;
    const float k = sdfImpl_->smoothK;

    const int maxSteps = 64;
    const float maxDist = camDist * 3.0f;
    const float hitEps  = 0.5f;

    for (int py = 0; py < H; ++py) {
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(py));
        for (int px = 0; px < W; ++px) {
            // NDC [-1, 1]
            float u = (static_cast<float>(px) + 0.5f - halfW) / halfW * fovFactor;
            float v = -(static_cast<float>(py) + 0.5f - halfH) / halfH * fovFactor;

            Vec3 rd = normalize({u, v, -1.0f});
            Vec3 ro = camPos;

            float t = 0.0f;
            SceneResult hit{1e30f, ArtifactCore::FloatColor{0,0,0,0}};
            bool found = false;
            for (int s = 0; s < maxSteps && t < maxDist; ++s) {
                Vec3 p{ro.x + rd.x*t, ro.y + rd.y*t, ro.z + rd.z*t};
                SceneResult res = evalScene(p, objs, op, k);
                if (res.dist < hitEps) {
                    hit = res;
                    found = true;
                    // refine
                    Vec3 hitPt{ro.x + rd.x*t, ro.y + rd.y*t, ro.z + rd.z*t};
                    Vec3 n = calcNormal(hitPt, objs, op, k);
                    auto shaded = shade(hitPt, n, hit.color, camDir);
                    // Premultiplied alpha
                    float a = clampf(shaded.a(), 0.0f, 1.0f);
                    int ri = static_cast<int>(clampf(shaded.r(), 0.0f, 1.0f) * a * 255.0f);
                    int gi = static_cast<int>(clampf(shaded.g(), 0.0f, 1.0f) * a * 255.0f);
                    int bi = static_cast<int>(clampf(shaded.b(), 0.0f, 1.0f) * a * 255.0f);
                    int ai = static_cast<int>(a * 255.0f);
                    line[px] = (ai << 24) | (ri << 16) | (gi << 8) | bi;
                    break;
                }
                t += res.dist * 0.9f;
            }
        }
    }
    return img;
}

// ---------------------------------------------------------------------------
// draw(): QImage をレンダラーでコンポジット
// ---------------------------------------------------------------------------
void ArtifactSDFLayer::draw(ArtifactIRenderer* renderer)
{
    if (!renderer || !isVisible()) return;

    QImage img = toQImage();
    if (img.isNull()) return;

    const RationalTime frameTime(currentFrame(), 30);
    const auto& t3 = transform3D();

    QMatrix4x4 transform;
    transform.translate(
        static_cast<float>(t3.positionXAt(frameTime)),
        static_cast<float>(t3.positionYAt(frameTime)));
    transform.rotate(
        static_cast<float>(t3.rotationAt(frameTime)),
        0.0f, 0.0f, 1.0f);
    transform.scale(
        static_cast<float>(t3.scaleXAt(frameTime)) / 100.0f,
        static_cast<float>(t3.scaleYAt(frameTime)) / 100.0f);

    renderer->drawSpriteTransformed(
        -sdfImpl_->renderW * 0.5f,
        -sdfImpl_->renderH * 0.5f,
        static_cast<float>(sdfImpl_->renderW),
        static_cast<float>(sdfImpl_->renderH),
        transform,
        img,
        opacity());
}

// ---------------------------------------------------------------------------
// Inspector properties
// ---------------------------------------------------------------------------
std::vector<ArtifactCore::PropertyGroup> ArtifactSDFLayer::getLayerPropertyGroups() const
{
    auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();

    ArtifactCore::PropertyGroup sdfGroup("SDF Settings");

    auto opProp = persistentLayerProperty(
        QStringLiteral("SDF Settings/Combine Op"),
        ArtifactCore::PropertyType::Integer,
        static_cast<int>(sdfImpl_->op), -200);
    opProp->setTooltip(QStringLiteral("0: Union, 1: Smooth Union"));
    sdfGroup.addProperty(opProp);

    auto smoothProp = persistentLayerProperty(
        QStringLiteral("SDF Settings/Smooth K"),
        ArtifactCore::PropertyType::Float,
        static_cast<double>(sdfImpl_->smoothK), -195);
    smoothProp->setHardRange(0.0, 500.0);
    smoothProp->setSoftRange(0.0, 100.0);
    sdfGroup.addProperty(smoothProp);

    groups.push_back(sdfGroup);
    return groups;
}

bool ArtifactSDFLayer::setLayerPropertyValue(const QString& propertyPath, const QVariant& value)
{
    if (propertyPath == "SDF Settings/Combine Op") {
        setCombineOp(static_cast<SDFOp>(value.toInt()));
        return true;
    }
    if (propertyPath == "SDF Settings/Smooth K") {
        setSmoothing(static_cast<float>(value.toDouble()));
        return true;
    }
    return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
}

} // namespace Artifact
