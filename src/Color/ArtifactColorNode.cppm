module;
#include <QObject>
#include <QString>
#include <QUuid>
#include <QPointF>
#include <wobjectimpl.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Artifact.Color.Node;





namespace Artifact {

W_OBJECT_IMPL(ColorNode)
W_OBJECT_IMPL(ColorInputNode)
W_OBJECT_IMPL(ColorOutputNode)
W_OBJECT_IMPL(LiftGammaGainNode)
W_OBJECT_IMPL(ContrastNode)
W_OBJECT_IMPL(ColorSpaceNode)
W_OBJECT_IMPL(MergeNode)
W_OBJECT_IMPL(CurvesNode)
W_OBJECT_IMPL(HueSaturationNode)
W_OBJECT_IMPL(ColorBalanceNode)
W_OBJECT_IMPL(ExposureNode)
W_OBJECT_IMPL(InvertNode)
W_OBJECT_IMPL(ClampNode)
W_OBJECT_IMPL(QualifierNode)
W_OBJECT_IMPL(BlurNode)

// ============================================================
// ColorNode::Impl
// ============================================================

class ColorNode::Impl {
public:
    QUuid id_;
    ColorNodeType type_;
    QString name_;
    bool enabled_ = true;
    bool bypassed_ = false;
    QPointF position_;
    float mix_ = 1.0f;

    std::vector<PortDescriptor> inputPorts_;
    std::vector<PortDescriptor> outputPorts_;

    Impl(ColorNodeType type)
        : id_(QUuid::createUuid())
        , type_(type)
    {}
};

// ============================================================
// ColorNode
// ============================================================

ColorNode::ColorNode(ColorNodeType type, QObject* parent)
    : QObject(parent), impl_(std::make_unique<Impl>(type))
{
    // Default ports: 1 color input, 1 color output
    impl_->inputPorts_.push_back({ "Input", PortDirection::Input, PortDataType::Color });
    impl_->outputPorts_.push_back({ "Output", PortDirection::Output, PortDataType::Color });
}

ColorNode::~ColorNode() = default;

QUuid ColorNode::id() const { return impl_->id_; }
ColorNodeType ColorNode::type() const { return impl_->type_; }

QString ColorNode::name() const { return impl_->name_; }
void ColorNode::setName(const QString& name) {
    if (impl_->name_ != name) {
        impl_->name_ = name;
        emit nameChanged(name);
    }
}

bool ColorNode::isEnabled() const { return impl_->enabled_; }
void ColorNode::setEnabled(bool enabled) {
    if (impl_->enabled_ != enabled) {
        impl_->enabled_ = enabled;
        emit enabledChanged(enabled);
    }
}

bool ColorNode::isBypassed() const { return impl_->bypassed_; }
void ColorNode::setBypassed(bool bypassed) {
    if (impl_->bypassed_ != bypassed) {
        impl_->bypassed_ = bypassed;
        emit bypassedChanged(bypassed);
    }
}

int ColorNode::inputPortCount() const {
    return static_cast<int>(impl_->inputPorts_.size());
}

int ColorNode::outputPortCount() const {
    return static_cast<int>(impl_->outputPorts_.size());
}

PortDescriptor ColorNode::inputPort(int index) const {
    if (index >= 0 && index < static_cast<int>(impl_->inputPorts_.size()))
        return impl_->inputPorts_[index];
    return {};
}

PortDescriptor ColorNode::outputPort(int index) const {
    if (index >= 0 && index < static_cast<int>(impl_->outputPorts_.size()))
        return impl_->outputPorts_[index];
    return {};
}

PortId ColorNode::inputPortId(int index) const {
    return { impl_->id_, index };
}

PortId ColorNode::outputPortId(int index) const {
    return { impl_->id_, index };
}

QPointF ColorNode::position() const { return impl_->position_; }
void ColorNode::setPosition(const QPointF& pos) {
    if (impl_->position_ != pos) {
        impl_->position_ = pos;
        emit positionChanged(pos);
    }
}

void ColorNode::processWithMask(float* pixels, const float* mask,
                                 int width, int height)
{
    if (!mask) {
        process(pixels, width, height);
        return;
    }

    // Create a copy, process the copy, then blend using mask
    const int totalPixels = width * height;
    std::vector<float> original(pixels, pixels + totalPixels * 4);

    process(pixels, width, height);

    // Blend: result = original * (1 - mask) + processed * mask
    for (int i = 0; i < totalPixels; ++i) {
        float m = std::clamp(mask[i], 0.0f, 1.0f);
        float mixVal = m * impl_->mix_;
        int idx = i * 4;
        pixels[idx + 0] = original[idx + 0] * (1.0f - mixVal) + pixels[idx + 0] * mixVal;
        pixels[idx + 1] = original[idx + 1] * (1.0f - mixVal) + pixels[idx + 1] * mixVal;
        pixels[idx + 2] = original[idx + 2] * (1.0f - mixVal) + pixels[idx + 2] * mixVal;
        // Alpha channel untouched
    }
}

float ColorNode::mix() const { return impl_->mix_; }
void ColorNode::setMix(float mix) {
    impl_->mix_ = std::clamp(mix, 0.0f, 1.0f);
    emit paramsChanged();
}

// ============================================================
// ColorInputNode
// ============================================================

ColorInputNode::ColorInputNode(QObject* parent)
    : ColorNode(ColorNodeType::Input, parent)
{
    setName("Input");
}

ColorInputNode::~ColorInputNode() = default;

void ColorInputNode::process(float* /*pixels*/, int /*width*/, int /*height*/) {
    // Input node is a pass-through — the source pixels are already
    // loaded into the buffer by the graph evaluator
}

// ============================================================
// ColorOutputNode
// ============================================================

ColorOutputNode::ColorOutputNode(QObject* parent)
    : ColorNode(ColorNodeType::Output, parent)
{
    setName("Output");
}

ColorOutputNode::~ColorOutputNode() = default;

void ColorOutputNode::process(float* /*pixels*/, int /*width*/, int /*height*/) {
    // Output node is a pass-through — pixels go to the renderer as-is
}

// ============================================================
// LiftGammaGainNode
// ============================================================

LiftGammaGainNode::LiftGammaGainNode(QObject* parent)
    : ColorNode(ColorNodeType::LiftGammaGain, parent)
{
    setName("Lift Gamma Gain");
}

LiftGammaGainNode::~LiftGammaGainNode() = default;

void LiftGammaGainNode::setLift(float r, float g, float b) {
    liftR_ = r; liftG_ = g; liftB_ = b;
    emit paramsChanged();
}

void LiftGammaGainNode::setGamma(float r, float g, float b) {
    gammaR_ = r; gammaG_ = g; gammaB_ = b;
    emit paramsChanged();
}

void LiftGammaGainNode::setGain(float r, float g, float b) {
    gainR_ = r; gainG_ = g; gainB_ = b;
    emit paramsChanged();
}

void LiftGammaGainNode::setOffset(float r, float g, float b) {
    offsetR_ = r; offsetG_ = g; offsetB_ = b;
    emit paramsChanged();
}

void LiftGammaGainNode::process(float* pixels, int width, int height) {
    if (!isEnabled() || isBypassed()) return;

    const int total = width * height;
    const float mixVal = mix();

    // Precompute inverse gamma for pow()
    // Gamma formula: out = pow(in, 1/gamma)
    const float invGR = (gammaR_ > 0.001f) ? 1.0f / gammaR_ : 1.0f;
    const float invGG = (gammaG_ > 0.001f) ? 1.0f / gammaG_ : 1.0f;
    const float invGB = (gammaB_ > 0.001f) ? 1.0f / gammaB_ : 1.0f;

    for (int i = 0; i < total; ++i) {
        int idx = i * 4;
        float r = pixels[idx + 0];
        float g = pixels[idx + 1];
        float b = pixels[idx + 2];

        // DaVinci Resolve primary color correction formula:
        // result = gain * (pow(source, 1/gamma)) + offset + lift * (1 - source)

        // Apply lift (affects shadows most, fades towards highlights)
        float rr = r + liftR_ * (1.0f - r);
        float gg = g + liftG_ * (1.0f - g);
        float bb = b + liftB_ * (1.0f - b);

        // Apply gamma (power curve)
        rr = std::max(0.0f, rr);
        gg = std::max(0.0f, gg);
        bb = std::max(0.0f, bb);
        rr = std::pow(rr, invGR);
        gg = std::pow(gg, invGG);
        bb = std::pow(bb, invGB);

        // Apply gain (multiplier, affects highlights most)
        rr *= gainR_;
        gg *= gainG_;
        bb *= gainB_;

        // Apply offset (uniform shift)
        rr += offsetR_;
        gg += offsetG_;
        bb += offsetB_;

        // Mix with original
        if (mixVal < 1.0f) {
            rr = r + (rr - r) * mixVal;
            gg = g + (gg - g) * mixVal;
            bb = b + (bb - b) * mixVal;
        }

        pixels[idx + 0] = rr;
        pixels[idx + 1] = gg;
        pixels[idx + 2] = bb;
    }
}

// ============================================================
// ContrastNode
// ============================================================

ContrastNode::ContrastNode(QObject* parent)
    : ColorNode(ColorNodeType::Contrast, parent)
{
    setName("Contrast");
}

ContrastNode::~ContrastNode() = default;

void ContrastNode::setContrast(float c) {
    contrast_ = c;
    emit paramsChanged();
}

void ContrastNode::setPivot(float p) {
    pivot_ = std::max(0.001f, p);
    emit paramsChanged();
}

void ContrastNode::setSaturation(float s) {
    saturation_ = s;
    emit paramsChanged();
}

void ContrastNode::process(float* pixels, int width, int height) {
    if (!isEnabled() || isBypassed()) return;

    const int total = width * height;
    const float mixVal = mix();

    for (int i = 0; i < total; ++i) {
        int idx = i * 4;
        float r = pixels[idx + 0];
        float g = pixels[idx + 1];
        float b = pixels[idx + 2];

        // Contrast around pivot:
        // out = pivot * pow(in / pivot, contrast)
        float rr = (r > 0.0f) ? pivot_ * std::pow(r / pivot_, contrast_) : 0.0f;
        float gg = (g > 0.0f) ? pivot_ * std::pow(g / pivot_, contrast_) : 0.0f;
        float bb = (b > 0.0f) ? pivot_ * std::pow(b / pivot_, contrast_) : 0.0f;

        // Saturation adjustment
        if (std::abs(saturation_ - 1.0f) > 0.001f) {
            float luma = 0.2126f * rr + 0.7152f * gg + 0.0722f * bb;
            rr = luma + (rr - luma) * saturation_;
            gg = luma + (gg - luma) * saturation_;
            bb = luma + (bb - luma) * saturation_;
        }

        // Mix
        if (mixVal < 1.0f) {
            rr = r + (rr - r) * mixVal;
            gg = g + (gg - g) * mixVal;
            bb = b + (bb - b) * mixVal;
        }

        pixels[idx + 0] = rr;
        pixels[idx + 1] = gg;
        pixels[idx + 2] = bb;
    }
}

// ============================================================
// ColorSpaceNode
// ============================================================

ColorSpaceNode::ColorSpaceNode(QObject* parent)
    : ColorNode(ColorNodeType::ColorSpace, parent)
{
    setName("Color Space");
}

ColorSpaceNode::~ColorSpaceNode() = default;

void ColorSpaceNode::setSourceSpace(int space) {
    sourceSpace_ = space;
    emit paramsChanged();
}

void ColorSpaceNode::setTargetSpace(int space) {
    targetSpace_ = space;
    emit paramsChanged();
}

void ColorSpaceNode::process(float* /*pixels*/, int /*width*/, int /*height*/) {
    if (!isEnabled() || isBypassed()) return;
    // TODO: Integrate with ColorManager::getConversionMatrix()
    // For now this is a placeholder — actual CSC should use
    // the 3x3 matrix from the color management system
}

// ============================================================
// MergeNode
// ============================================================

MergeNode::MergeNode(QObject* parent)
    : ColorNode(ColorNodeType::Merge, parent)
{
    setName("Merge");
}

MergeNode::~MergeNode() = default;

void MergeNode::setBlendMode(BlendMode mode) {
    blendMode_ = mode;
    emit paramsChanged();
}

void MergeNode::setOpacity(float opacity) {
    opacity_ = std::clamp(opacity, 0.0f, 1.0f);
    emit paramsChanged();
}

void MergeNode::setSecondaryInput(const float* pixels) {
    secondaryInput_ = pixels;
}

void MergeNode::process(float* pixels, int width, int height) {
    if (!isEnabled() || isBypassed() || !secondaryInput_) return;

    const int total = width * height;
    const float op = opacity_ * mix();

    for (int i = 0; i < total; ++i) {
        int idx = i * 4;
        float aR = pixels[idx + 0];   // Background (A)
        float aG = pixels[idx + 1];
        float aB = pixels[idx + 2];
        float bR = secondaryInput_[idx + 0]; // Foreground (B)
        float bG = secondaryInput_[idx + 1];
        float bB = secondaryInput_[idx + 2];

        float rR, rG, rB;

        switch (blendMode_) {
        case BlendMode::Over:
            rR = aR * (1.0f - op) + bR * op;
            rG = aG * (1.0f - op) + bG * op;
            rB = aB * (1.0f - op) + bB * op;
            break;

        case BlendMode::Add:
            rR = aR + bR * op;
            rG = aG + bG * op;
            rB = aB + bB * op;
            break;

        case BlendMode::Multiply:
            rR = aR * (1.0f - op) + (aR * bR) * op;
            rG = aG * (1.0f - op) + (aG * bG) * op;
            rB = aB * (1.0f - op) + (aB * bB) * op;
            break;

        case BlendMode::Screen:
            rR = aR * (1.0f - op) + (1.0f - (1.0f - aR) * (1.0f - bR)) * op;
            rG = aG * (1.0f - op) + (1.0f - (1.0f - aG) * (1.0f - bG)) * op;
            rB = aB * (1.0f - op) + (1.0f - (1.0f - aB) * (1.0f - bB)) * op;
            break;

        case BlendMode::Subtract:
            rR = aR - bR * op;
            rG = aG - bG * op;
            rB = aB - bB * op;
            break;

        case BlendMode::Difference:
            rR = aR * (1.0f - op) + std::abs(aR - bR) * op;
            rG = aG * (1.0f - op) + std::abs(aG - bG) * op;
            rB = aB * (1.0f - op) + std::abs(aB - bB) * op;
            break;

        case BlendMode::SoftLight: {
            auto softLight = [](float a, float b) -> float {
                if (b < 0.5f) return a - (1.0f - 2.0f * b) * a * (1.0f - a);
                else return a + (2.0f * b - 1.0f) * (std::sqrt(a) - a);
            };
            rR = aR * (1.0f - op) + softLight(aR, bR) * op;
            rG = aG * (1.0f - op) + softLight(aG, bG) * op;
            rB = aB * (1.0f - op) + softLight(aB, bB) * op;
            break;
        }

        case BlendMode::Overlay: {
            auto overlay = [](float a, float b) -> float {
                if (a < 0.5f) return 2.0f * a * b;
                else return 1.0f - 2.0f * (1.0f - a) * (1.0f - b);
            };
            rR = aR * (1.0f - op) + overlay(aR, bR) * op;
            rG = aG * (1.0f - op) + overlay(aG, bG) * op;
            rB = aB * (1.0f - op) + overlay(aB, bB) * op;
            break;
        }

        default:
            rR = aR; rG = aG; rB = aB;
            break;
        }

        pixels[idx + 0] = rR;
        pixels[idx + 1] = rG;
        pixels[idx + 2] = rB;
    }
}

// ============================================================
// CurvesNode
// ============================================================

CurvesNode::CurvesNode(QObject* parent)
    : ColorNode(ColorNodeType::Curves, parent)
{
    setName("Curves");
    resetCurves();
}

CurvesNode::~CurvesNode() = default;

void CurvesNode::resetCurves() {
    CurvePoint p0{ 0.0f, 0.0f };
    CurvePoint p1{ 1.0f, 1.0f };
    masterCurve_ = { p0, p1 };
    redCurve_ = { p0, p1 };
    greenCurve_ = { p0, p1 };
    blueCurve_ = { p0, p1 };
    lutDirty_ = true;
    emit paramsChanged();
}

void CurvesNode::setMasterCurve(const std::vector<CurvePoint>& points) {
    masterCurve_ = points; lutDirty_ = true; emit paramsChanged();
}
void CurvesNode::setRedCurve(const std::vector<CurvePoint>& points) {
    redCurve_ = points; lutDirty_ = true; emit paramsChanged();
}
void CurvesNode::setGreenCurve(const std::vector<CurvePoint>& points) {
    greenCurve_ = points; lutDirty_ = true; emit paramsChanged();
}
void CurvesNode::setBlueCurve(const std::vector<CurvePoint>& points) {
    blueCurve_ = points; lutDirty_ = true; emit paramsChanged();
}

void CurvesNode::applySCurve(float strength) {
    float s = std::clamp(strength, 0.0f, 1.0f);
    masterCurve_ = {
        { 0.0f, 0.0f },
        { 0.25f, 0.25f - 0.15f * s },
        { 0.75f, 0.75f + 0.15f * s },
        { 1.0f, 1.0f }
    };
    lutDirty_ = true;
    emit paramsChanged();
}

float CurvesNode::evaluateCurve(const std::vector<CurvePoint>& points, float x) {
    if (points.empty()) return x;
    if (points.size() == 1) return points[0].y;
    if (x <= points.front().x) return points.front().y;
    if (x >= points.back().x) return points.back().y;

    // Find segment
    int seg = 0;
    for (int i = 1; i < static_cast<int>(points.size()); ++i) {
        if (x < points[i].x) { seg = i - 1; break; }
    }

    // Catmull-Rom spline interpolation
    float t = (x - points[seg].x) / (points[seg + 1].x - points[seg].x);

    int p0 = std::max(0, seg - 1);
    int p1 = seg;
    int p2 = seg + 1;
    int p3 = std::min(static_cast<int>(points.size()) - 1, seg + 2);

    float y0 = points[p0].y, y1 = points[p1].y;
    float y2 = points[p2].y, y3 = points[p3].y;

    float t2 = t * t, t3 = t2 * t;
    float result = 0.5f * (
        (2.0f * y1) +
        (-y0 + y2) * t +
        (2.0f * y0 - 5.0f * y1 + 4.0f * y2 - y3) * t2 +
        (-y0 + 3.0f * y1 - 3.0f * y2 + y3) * t3
    );
    return std::clamp(result, 0.0f, 1.0f);
}

void CurvesNode::buildLUTs() {
    for (int i = 0; i < 256; ++i) {
        float x = static_cast<float>(i) / 255.0f;
        masterLUT_[i] = evaluateCurve(masterCurve_, x);
        redLUT_[i]    = evaluateCurve(redCurve_, x);
        greenLUT_[i]  = evaluateCurve(greenCurve_, x);
        blueLUT_[i]   = evaluateCurve(blueCurve_, x);
    }
    lutDirty_ = false;
}

void CurvesNode::process(float* pixels, int width, int height) {
    if (!isEnabled() || isBypassed()) return;
    if (lutDirty_) buildLUTs();

    const int total = width * height;
    const float mixVal = mix();

    for (int i = 0; i < total; ++i) {
        int idx = i * 4;
        float r = pixels[idx + 0];
        float g = pixels[idx + 1];
        float b = pixels[idx + 2];

        // Look up in LUT (with linear interp between entries)
        auto lutLookup = [](const float* lut, float val) -> float {
            float fi = std::clamp(val, 0.0f, 1.0f) * 255.0f;
            int lo = static_cast<int>(fi);
            int hi = std::min(lo + 1, 255);
            float frac = fi - static_cast<float>(lo);
            return lut[lo] + (lut[hi] - lut[lo]) * frac;
        };

        // Apply per-channel curves, then master
        float rr = lutLookup(masterLUT_, lutLookup(redLUT_, r));
        float gg = lutLookup(masterLUT_, lutLookup(greenLUT_, g));
        float bb = lutLookup(masterLUT_, lutLookup(blueLUT_, b));

        if (mixVal < 1.0f) {
            rr = r + (rr - r) * mixVal;
            gg = g + (gg - g) * mixVal;
            bb = b + (bb - b) * mixVal;
        }

        pixels[idx + 0] = rr;
        pixels[idx + 1] = gg;
        pixels[idx + 2] = bb;
    }
}

// ============================================================
// HueSaturationNode
// ============================================================

HueSaturationNode::HueSaturationNode(QObject* parent)
    : ColorNode(ColorNodeType::HueSaturation, parent)
{
    setName("Hue/Saturation");
}

HueSaturationNode::~HueSaturationNode() = default;

void HueSaturationNode::setHueShift(float degrees) {
    hueShift_ = degrees; emit paramsChanged();
}
void HueSaturationNode::setSaturation(float s) {
    saturation_ = s; emit paramsChanged();
}
void HueSaturationNode::setLightness(float l) {
    lightness_ = l; emit paramsChanged();
}
void HueSaturationNode::setHueSectorSaturation(int index, float value) {
    if (index >= 0 && index < 6) { hueSat_[index] = value; emit paramsChanged(); }
}
float HueSaturationNode::hueSectorSaturation(int index) const {
    return (index >= 0 && index < 6) ? hueSat_[index] : 1.0f;
}

void HueSaturationNode::rgbToHsl(float r, float g, float b, float& h, float& s, float& l) {
    float maxC = std::max({ r, g, b });
    float minC = std::min({ r, g, b });
    float delta = maxC - minC;
    l = (maxC + minC) * 0.5f;
    if (delta < 0.00001f) { h = 0.0f; s = 0.0f; return; }
    s = (l > 0.5f) ? delta / (2.0f - maxC - minC) : delta / (maxC + minC);
    if (maxC == r)      h = std::fmod((g - b) / delta + 6.0f, 6.0f) / 6.0f;
    else if (maxC == g) h = ((b - r) / delta + 2.0f) / 6.0f;
    else                h = ((r - g) / delta + 4.0f) / 6.0f;
}

void HueSaturationNode::hslToRgb(float h, float s, float l, float& r, float& g, float& b) {
    if (s < 0.00001f) { r = g = b = l; return; }
    auto hue2rgb = [](float p, float q, float t) -> float {
        if (t < 0.0f) t += 1.0f;
        if (t > 1.0f) t -= 1.0f;
        if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
        if (t < 1.0f / 2.0f) return q;
        if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
        return p;
    };
    float q = (l < 0.5f) ? l * (1.0f + s) : l + s - l * s;
    float p = 2.0f * l - q;
    r = hue2rgb(p, q, h + 1.0f / 3.0f);
    g = hue2rgb(p, q, h);
    b = hue2rgb(p, q, h - 1.0f / 3.0f);
}

void HueSaturationNode::process(float* pixels, int width, int height) {
    if (!isEnabled() || isBypassed()) return;
    const int total = width * height;
    const float mixVal = mix();
    const float hueShiftNorm = hueShift_ / 360.0f;

    for (int i = 0; i < total; ++i) {
        int idx = i * 4;
        float r = pixels[idx + 0];
        float g = pixels[idx + 1];
        float b = pixels[idx + 2];

        float h, s, l;
        rgbToHsl(r, g, b, h, s, l);

        // Hue shift
        h += hueShiftNorm;
        if (h < 0.0f) h += 1.0f;
        if (h > 1.0f) h -= 1.0f;

        // Per-sector saturation (6 sectors, 60 degrees each)
        int sector = static_cast<int>(h * 6.0f) % 6;
        float sectorSat = hueSat_[sector];

        // Global + sector saturation
        s *= saturation_ * sectorSat;
        s = std::clamp(s, 0.0f, 1.0f);

        // Lightness
        l += lightness_;
        l = std::clamp(l, 0.0f, 1.0f);

        float rr, gg, bb;
        hslToRgb(h, s, l, rr, gg, bb);

        if (mixVal < 1.0f) {
            rr = r + (rr - r) * mixVal;
            gg = g + (gg - g) * mixVal;
            bb = b + (bb - b) * mixVal;
        }

        pixels[idx + 0] = rr;
        pixels[idx + 1] = gg;
        pixels[idx + 2] = bb;
    }
}

// ============================================================
// ColorBalanceNode
// ============================================================

ColorBalanceNode::ColorBalanceNode(QObject* parent)
    : ColorNode(ColorNodeType::ColorBalance, parent)
{
    setName("Color Balance");
}

ColorBalanceNode::~ColorBalanceNode() = default;

void ColorBalanceNode::setShadowBalance(float r, float g, float b) {
    shadowR_ = r; shadowG_ = g; shadowB_ = b; emit paramsChanged();
}
void ColorBalanceNode::setMidtoneBalance(float r, float g, float b) {
    midtoneR_ = r; midtoneG_ = g; midtoneB_ = b; emit paramsChanged();
}
void ColorBalanceNode::setHighlightBalance(float r, float g, float b) {
    highlightR_ = r; highlightG_ = g; highlightB_ = b; emit paramsChanged();
}
void ColorBalanceNode::setShadowRange(float range) {
    shadowRange_ = std::clamp(range, 0.01f, 0.99f); emit paramsChanged();
}
void ColorBalanceNode::setHighlightRange(float range) {
    highlightRange_ = std::clamp(range, 0.01f, 0.99f); emit paramsChanged();
}

void ColorBalanceNode::process(float* pixels, int width, int height) {
    if (!isEnabled() || isBypassed()) return;
    const int total = width * height;
    const float mixVal = mix();

    for (int i = 0; i < total; ++i) {
        int idx = i * 4;
        float r = pixels[idx + 0];
        float g = pixels[idx + 1];
        float b = pixels[idx + 2];

        // Compute luminance for zone weighting
        float luma = 0.2126f * r + 0.7152f * g + 0.0722f * b;

        // Smooth zone weights (using smoothstep-like transitions)
        float shadowW = 1.0f - std::clamp((luma - shadowRange_ + 0.1f) / 0.2f, 0.0f, 1.0f);
        float highlightW = std::clamp((luma - highlightRange_ + 0.1f) / 0.2f, 0.0f, 1.0f);
        float midtoneW = 1.0f - shadowW - highlightW;
        midtoneW = std::max(0.0f, midtoneW);

        float rr = r + shadowR_ * shadowW + midtoneR_ * midtoneW + highlightR_ * highlightW;
        float gg = g + shadowG_ * shadowW + midtoneG_ * midtoneW + highlightG_ * highlightW;
        float bb = b + shadowB_ * shadowW + midtoneB_ * midtoneW + highlightB_ * highlightW;

        if (mixVal < 1.0f) {
            rr = r + (rr - r) * mixVal;
            gg = g + (gg - g) * mixVal;
            bb = b + (bb - b) * mixVal;
        }

        pixels[idx + 0] = rr;
        pixels[idx + 1] = gg;
        pixels[idx + 2] = bb;
    }
}

// ============================================================
// ExposureNode
// ============================================================

ExposureNode::ExposureNode(QObject* parent)
    : ColorNode(ColorNodeType::Exposure, parent)
{
    setName("Exposure");
}

ExposureNode::~ExposureNode() = default;

void ExposureNode::setExposure(float ev) {
    exposure_ = std::clamp(ev, -10.0f, 10.0f); emit paramsChanged();
}
void ExposureNode::setGamma(float g) {
    gamma_ = std::max(0.01f, g); emit paramsChanged();
}
void ExposureNode::setOffset(float o) {
    offset_ = o; emit paramsChanged();
}

void ExposureNode::process(float* pixels, int width, int height) {
    if (!isEnabled() || isBypassed()) return;
    const int total = width * height;
    const float mixVal = mix();
    const float multiplier = std::pow(2.0f, exposure_);
    const float invGamma = (gamma_ > 0.01f) ? 1.0f / gamma_ : 1.0f;

    for (int i = 0; i < total; ++i) {
        int idx = i * 4;
        float r = pixels[idx + 0];
        float g = pixels[idx + 1];
        float b = pixels[idx + 2];

        // Exposure: multiply by 2^EV
        float rr = r * multiplier + offset_;
        float gg = g * multiplier + offset_;
        float bb = b * multiplier + offset_;

        // Gamma correction
        if (std::abs(gamma_ - 1.0f) > 0.001f) {
            rr = (rr > 0.0f) ? std::pow(rr, invGamma) : 0.0f;
            gg = (gg > 0.0f) ? std::pow(gg, invGamma) : 0.0f;
            bb = (bb > 0.0f) ? std::pow(bb, invGamma) : 0.0f;
        }

        if (mixVal < 1.0f) {
            rr = r + (rr - r) * mixVal;
            gg = g + (gg - g) * mixVal;
            bb = b + (bb - b) * mixVal;
        }

        pixels[idx + 0] = rr;
        pixels[idx + 1] = gg;
        pixels[idx + 2] = bb;
    }
}

// ============================================================
// InvertNode
// ============================================================

InvertNode::InvertNode(QObject* parent)
    : ColorNode(ColorNodeType::Invert, parent)
{
    setName("Invert");
}

InvertNode::~InvertNode() = default;

void InvertNode::setInvertChannels(bool r, bool g, bool b, bool a) {
    invertR_ = r; invertG_ = g; invertB_ = b; invertA_ = a;
    emit paramsChanged();
}

void InvertNode::process(float* pixels, int width, int height) {
    if (!isEnabled() || isBypassed()) return;
    const int total = width * height;
    const float mixVal = mix();

    for (int i = 0; i < total; ++i) {
        int idx = i * 4;
        float r = pixels[idx + 0];
        float g = pixels[idx + 1];
        float b = pixels[idx + 2];
        float a = pixels[idx + 3];

        float rr = invertR_ ? (1.0f - r) : r;
        float gg = invertG_ ? (1.0f - g) : g;
        float bb = invertB_ ? (1.0f - b) : b;
        float aa = invertA_ ? (1.0f - a) : a;

        if (mixVal < 1.0f) {
            rr = r + (rr - r) * mixVal;
            gg = g + (gg - g) * mixVal;
            bb = b + (bb - b) * mixVal;
            aa = a + (aa - a) * mixVal;
        }

        pixels[idx + 0] = rr;
        pixels[idx + 1] = gg;
        pixels[idx + 2] = bb;
        pixels[idx + 3] = aa;
    }
}

// ============================================================
// ClampNode
// ============================================================

ClampNode::ClampNode(QObject* parent)
    : ColorNode(ColorNodeType::Clamp, parent)
{
    setName("Clamp");
}

ClampNode::~ClampNode() = default;

void ClampNode::setRange(float minVal, float maxVal) {
    minValue_ = minVal; maxValue_ = maxVal; emit paramsChanged();
}
void ClampNode::setClampAlpha(bool clamp) {
    clampAlpha_ = clamp; emit paramsChanged();
}

void ClampNode::process(float* pixels, int width, int height) {
    if (!isEnabled() || isBypassed()) return;
    const int total = width * height;

    for (int i = 0; i < total; ++i) {
        int idx = i * 4;
        if (clampRGB_) {
            pixels[idx + 0] = std::clamp(pixels[idx + 0], minValue_, maxValue_);
            pixels[idx + 1] = std::clamp(pixels[idx + 1], minValue_, maxValue_);
            pixels[idx + 2] = std::clamp(pixels[idx + 2], minValue_, maxValue_);
        }
        if (clampAlpha_) {
            pixels[idx + 3] = std::clamp(pixels[idx + 3], minValue_, maxValue_);
        }
    }
}

// ============================================================
// QualifierNode (DaVinci Resolve-style HSL Qualifier)
// ============================================================

QualifierNode::QualifierNode(QObject* parent)
    : ColorNode(ColorNodeType::Qualifier, parent)
{
    setName("Qualifier");
}

QualifierNode::~QualifierNode() = default;

void QualifierNode::setHueRange(float center, float width, float softness) {
    hueCenter_ = center; hueWidth_ = width; hueSoftness_ = softness;
    emit paramsChanged();
}
void QualifierNode::setSatRange(float low, float high, float softness) {
    satLow_ = low; satHigh_ = high; satSoftness_ = softness;
    emit paramsChanged();
}
void QualifierNode::setLumRange(float low, float high, float softness) {
    lumLow_ = low; lumHigh_ = high; lumSoftness_ = softness;
    emit paramsChanged();
}
void QualifierNode::setInvert(bool inv) {
    invert_ = inv; emit paramsChanged();
}
const float* QualifierNode::mask() const {
    return maskBuffer_.empty() ? nullptr : maskBuffer_.data();
}
int QualifierNode::maskSize() const {
    return static_cast<int>(maskBuffer_.size());
}

void QualifierNode::process(float* pixels, int width, int height) {
    if (!isEnabled() || isBypassed()) return;
    const int total = width * height;
    maskBuffer_.resize(total);

    // Softness ramp helper
    auto softRamp = [](float val, float low, float high, float softness) -> float {
        if (val >= low && val <= high) return 1.0f;
        if (val < low)  return std::clamp((val - (low - softness)) / softness, 0.0f, 1.0f);
        if (val > high) return std::clamp(((high + softness) - val) / softness, 0.0f, 1.0f);
        return 0.0f;
    };

    // Hue distance with wrap-around
    auto hueDist = [](float h, float center, float width360) -> float {
        float d = std::abs(h - center);
        if (d > 180.0f) d = 360.0f - d;
        return d - width360 * 0.5f;
    };

    for (int i = 0; i < total; ++i) {
        int idx = i * 4;
        float r = pixels[idx + 0];
        float g = pixels[idx + 1];
        float b = pixels[idx + 2];

        // Convert to HSL
        float maxC = std::max({ r, g, b });
        float minC = std::min({ r, g, b });
        float delta = maxC - minC;
        float l = (maxC + minC) * 0.5f;
        float s = (delta < 0.00001f) ? 0.0f :
                  (l > 0.5f) ? delta / (2.0f - maxC - minC) : delta / (maxC + minC);
        float h = 0.0f;
        if (delta > 0.00001f) {
            if (maxC == r)      h = std::fmod((g - b) / delta + 6.0f, 6.0f) * 60.0f;
            else if (maxC == g) h = ((b - r) / delta + 2.0f) * 60.0f;
            else                h = ((r - g) / delta + 4.0f) * 60.0f;
        }

        // Evaluate qualification
        float hueD = hueDist(h, hueCenter_, hueWidth_);
        float hueQ = (hueD <= 0.0f) ? 1.0f :
                     std::clamp(1.0f - hueD / hueSoftness_, 0.0f, 1.0f);

        float satQ = softRamp(s, satLow_, satHigh_, satSoftness_);
        float lumQ = softRamp(l, lumLow_, lumHigh_, lumSoftness_);

        float qual = hueQ * satQ * lumQ;
        if (invert_) qual = 1.0f - qual;

        maskBuffer_[i] = qual;
    }

    // The qualifier node itself doesn't modify pixels
    // — the generated mask is used by downstream nodes via processWithMask()
}

// ============================================================
// BlurNode (Separable Gaussian)
// ============================================================

BlurNode::BlurNode(QObject* parent)
    : ColorNode(ColorNodeType::Blur, parent)
{
    setName("Blur");
}

BlurNode::~BlurNode() = default;

void BlurNode::setRadius(float radius) {
    radiusX_ = radius;
    if (lockAspect_) radiusY_ = radius;
    emit paramsChanged();
}
void BlurNode::setRadius(float rx, float ry) {
    radiusX_ = rx; radiusY_ = ry; lockAspect_ = false;
    emit paramsChanged();
}
void BlurNode::setLockAspect(bool lock) {
    lockAspect_ = lock;
    if (lock) radiusY_ = radiusX_;
    emit paramsChanged();
}

void BlurNode::process(float* pixels, int width, int height) {
    if (!isEnabled() || isBypassed()) return;
    if (radiusX_ < 0.5f && radiusY_ < 0.5f) return;

    const int total = width * height;
    std::vector<float> temp(total * 4);

    // Build 1D Gaussian kernel
    auto buildKernel = [](float radius) -> std::vector<float> {
        int size = static_cast<int>(std::ceil(radius * 3.0f));
        if (size < 1) size = 1;
        std::vector<float> kernel(size * 2 + 1);
        float sigma = radius;
        float sum = 0.0f;
        for (int i = -size; i <= size; ++i) {
            float val = std::exp(-0.5f * (i * i) / (sigma * sigma));
            kernel[i + size] = val;
            sum += val;
        }
        for (auto& v : kernel) v /= sum;
        return kernel;
    };

    // Horizontal pass
    if (radiusX_ >= 0.5f) {
        auto kernelH = buildKernel(radiusX_);
        int kSize = static_cast<int>(kernelH.size()) / 2;

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float sumR = 0, sumG = 0, sumB = 0, sumA = 0;
                for (int k = -kSize; k <= kSize; ++k) {
                    int sx = std::clamp(x + k, 0, width - 1);
                    int srcIdx = (y * width + sx) * 4;
                    float w = kernelH[k + kSize];
                    sumR += pixels[srcIdx + 0] * w;
                    sumG += pixels[srcIdx + 1] * w;
                    sumB += pixels[srcIdx + 2] * w;
                    sumA += pixels[srcIdx + 3] * w;
                }
                int dstIdx = (y * width + x) * 4;
                temp[dstIdx + 0] = sumR;
                temp[dstIdx + 1] = sumG;
                temp[dstIdx + 2] = sumB;
                temp[dstIdx + 3] = sumA;
            }
        }
    } else {
        std::copy(pixels, pixels + total * 4, temp.begin());
    }

    // Vertical pass (read from temp, write to pixels)
    if (radiusY_ >= 0.5f) {
        auto kernelV = buildKernel(radiusY_);
        int kSize = static_cast<int>(kernelV.size()) / 2;

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float sumR = 0, sumG = 0, sumB = 0, sumA = 0;
                for (int k = -kSize; k <= kSize; ++k) {
                    int sy = std::clamp(y + k, 0, height - 1);
                    int srcIdx = (sy * width + x) * 4;
                    float w = kernelV[k + kSize];
                    sumR += temp[srcIdx + 0] * w;
                    sumG += temp[srcIdx + 1] * w;
                    sumB += temp[srcIdx + 2] * w;
                    sumA += temp[srcIdx + 3] * w;
                }
                int dstIdx = (y * width + x) * 4;
                pixels[dstIdx + 0] = sumR;
                pixels[dstIdx + 1] = sumG;
                pixels[dstIdx + 2] = sumB;
                pixels[dstIdx + 3] = sumA;
            }
        }
    } else {
        std::copy(temp.begin(), temp.end(), pixels);
    }
}

// ============================================================
// ColorNodeFactory
// ============================================================

std::unique_ptr<ColorNode> ColorNodeFactory::create(ColorNodeType type) {
    switch (type) {
    case ColorNodeType::Input:          return std::make_unique<ColorInputNode>();
    case ColorNodeType::Output:         return std::make_unique<ColorOutputNode>();
    case ColorNodeType::LiftGammaGain:  return std::make_unique<LiftGammaGainNode>();
    case ColorNodeType::Curves:         return std::make_unique<CurvesNode>();
    case ColorNodeType::HueSaturation:  return std::make_unique<HueSaturationNode>();
    case ColorNodeType::ColorBalance:   return std::make_unique<ColorBalanceNode>();
    case ColorNodeType::Exposure:       return std::make_unique<ExposureNode>();
    case ColorNodeType::Contrast:       return std::make_unique<ContrastNode>();
    case ColorNodeType::Merge:          return std::make_unique<MergeNode>();
    case ColorNodeType::ColorSpace:     return std::make_unique<ColorSpaceNode>();
    case ColorNodeType::Clamp:          return std::make_unique<ClampNode>();
    case ColorNodeType::Blur:           return std::make_unique<BlurNode>();
    case ColorNodeType::Invert:         return std::make_unique<InvertNode>();
    case ColorNodeType::Qualifier:      return std::make_unique<QualifierNode>();
    default:
        return nullptr;
    }
}

QString ColorNodeFactory::typeName(ColorNodeType type) {
    switch (type) {
    case ColorNodeType::Input:          return "Input";
    case ColorNodeType::Output:         return "Output";
    case ColorNodeType::LiftGammaGain:  return "LiftGammaGain";
    case ColorNodeType::Curves:         return "Curves";
    case ColorNodeType::HueSaturation:  return "HueSaturation";
    case ColorNodeType::ColorBalance:   return "ColorBalance";
    case ColorNodeType::Exposure:       return "Exposure";
    case ColorNodeType::Contrast:       return "Contrast";
    case ColorNodeType::Merge:          return "Merge";
    case ColorNodeType::LayerMixer:     return "LayerMixer";
    case ColorNodeType::LUT:            return "LUT";
    case ColorNodeType::ColorSpace:     return "ColorSpace";
    case ColorNodeType::Clamp:          return "Clamp";
    case ColorNodeType::Blur:           return "Blur";
    case ColorNodeType::Invert:         return "Invert";
    case ColorNodeType::Qualifier:      return "Qualifier";
    case ColorNodeType::AlphaOutput:    return "AlphaOutput";
    case ColorNodeType::Custom:         return "Custom";
    default: return "Unknown";
    }
}

QString ColorNodeFactory::typeDisplayName(ColorNodeType type) {
    switch (type) {
    case ColorNodeType::Input:          return "Input";
    case ColorNodeType::Output:         return "Output";
    case ColorNodeType::LiftGammaGain:  return "Lift/Gamma/Gain";
    case ColorNodeType::Curves:         return "Curves";
    case ColorNodeType::HueSaturation:  return "Hue/Saturation";
    case ColorNodeType::ColorBalance:   return "Color Balance";
    case ColorNodeType::Exposure:       return "Exposure";
    case ColorNodeType::Contrast:       return "Contrast";
    case ColorNodeType::Merge:          return "Merge";
    case ColorNodeType::LayerMixer:     return "Layer Mixer";
    case ColorNodeType::LUT:            return "LUT";
    case ColorNodeType::ColorSpace:     return "Color Space";
    case ColorNodeType::Clamp:          return "Clamp";
    case ColorNodeType::Blur:           return "Blur";
    case ColorNodeType::Invert:         return "Invert";
    case ColorNodeType::Qualifier:      return "Qualifier";
    case ColorNodeType::AlphaOutput:    return "Alpha Output";
    case ColorNodeType::Custom:         return "Custom";
    default: return "Unknown";
    }
}

std::vector<ColorNodeType> ColorNodeFactory::allTypes() {
    return {
        ColorNodeType::Input,
        ColorNodeType::Output,
        ColorNodeType::LiftGammaGain,
        ColorNodeType::Curves,
        ColorNodeType::HueSaturation,
        ColorNodeType::ColorBalance,
        ColorNodeType::Exposure,
        ColorNodeType::Contrast,
        ColorNodeType::Merge,
        ColorNodeType::ColorSpace,
        ColorNodeType::Clamp,
        ColorNodeType::Blur,
        ColorNodeType::Invert,
        ColorNodeType::Qualifier,
    };
}

} // namespace Artifact
