module;
#include <memory>
#include <wobjectdefs.h>

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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <QObject>
#include <QString>
#include <QUuid>
#include <QPointF>
export module Artifact.Color.Node;





export namespace Artifact {

// ============================================================
// Forward declarations
// ============================================================
class ColorNode;
class ColorNodeGraph;

// ============================================================
// NodePort — input/output connection point on a node
// ============================================================

enum class PortDirection {
    Input,
    Output
};

enum class PortDataType {
    Color,          // float RGBA pixel stream
    Mask,           // Single-channel mask (alpha)
    Value           // Scalar value (intensity, mix, etc.)
};

/// Unique port identifier (node ID + port index)
struct PortId {
    QUuid nodeId;
    int   portIndex = -1;

    bool isValid() const { return !nodeId.isNull() && portIndex >= 0; }
    bool operator==(const PortId& o) const { return nodeId == o.nodeId && portIndex == o.portIndex; }
    bool operator!=(const PortId& o) const { return !(*this == o); }
};

/// Descriptor for a single port on a node
struct PortDescriptor {
    QString       name;
    PortDirection direction;
    PortDataType  dataType;
};

/// A connection between two ports
struct NodeConnection {
    PortId source;      // output port
    PortId destination; // input port

    bool isValid() const { return source.isValid() && destination.isValid(); }
};

// ============================================================
// ColorNodeType — built-in node types
// ============================================================

enum class ColorNodeType {
    // I/O
    Input,              // Media / source input
    Output,             // Final output

    // Color correction
    LiftGammaGain,      // Lift / Gamma / Gain wheels
    Curves,             // RGB / Luma curves
    HueSaturation,      // Hue vs Sat, Hue vs Hue, etc.
    ColorBalance,       // Shadow / Midtone / Highlight balance
    Exposure,           // Exposure + offset
    Contrast,           // Contrast + pivot

    // Compositing
    Merge,              // Over / Add / Multiply blend
    LayerMixer,         // Multi-input layer mixer

    // Utility
    LUT,                // 3D LUT application
    ColorSpace,         // Color space transform
    Clamp,              // Clamp values to range
    Blur,               // Gaussian blur (for glow chains)
    Invert,             // Invert colors

    // Keying / Mask
    Qualifier,          // HSL qualifier (DaVinci-style)
    AlphaOutput,        // Output a mask/alpha

    // Custom
    Custom              // User-defined / OFX
};

// ============================================================
// ColorNode — base class for all color pipeline nodes
// ============================================================

class ColorNode : public QObject {
    W_OBJECT(ColorNode)
private:
    class Impl;
    std::unique_ptr<Impl> impl_;

public:
    explicit ColorNode(ColorNodeType type, QObject* parent = nullptr);
    ~ColorNode();

    // --- Identity ---
    QUuid id() const;
    ColorNodeType type() const;

    QString name() const;
    void setName(const QString& name);

    // --- Enable / Bypass ---
    bool isEnabled() const;
    void setEnabled(bool enabled);

    bool isBypassed() const;
    void setBypassed(bool bypassed);

    // --- Ports ---
    int inputPortCount() const;
    int outputPortCount() const;
    PortDescriptor inputPort(int index) const;
    PortDescriptor outputPort(int index) const;

    PortId inputPortId(int index) const;
    PortId outputPortId(int index) const;

    // --- UI Position (for node graph editor) ---
    QPointF position() const;
    void setPosition(const QPointF& pos);

    // --- Processing ---
    /// Process one frame of pixel data in-place
    /// @param pixels  RGBA float buffer (4 floats per pixel)
    /// @param width   Frame width
    /// @param height  Frame height
    virtual void process(float* pixels, int width, int height) = 0;

    /// Process with mask (for qualified corrections)
    virtual void processWithMask(float* pixels, const float* mask,
                                 int width, int height);

    // --- Mix / Intensity ---
    float mix() const;
    void setMix(float mix);

    // --- Serialization helpers ---
    // (Future: save/load node params to JSON)

signals:
    void nameChanged(const QString& name)   W_SIGNAL(nameChanged, name);
    void enabledChanged(bool enabled)        W_SIGNAL(enabledChanged, enabled);
    void bypassedChanged(bool bypassed)      W_SIGNAL(bypassedChanged, bypassed);
    void positionChanged(const QPointF& pos) W_SIGNAL(positionChanged, pos);
    void paramsChanged()                     W_SIGNAL(paramsChanged);
};

// ============================================================
// Concrete node types
// ============================================================

/// Input node — provides the source image to the graph
class ColorInputNode : public ColorNode {
    W_OBJECT(ColorInputNode)
public:
    explicit ColorInputNode(QObject* parent = nullptr);
    ~ColorInputNode();
    void process(float* pixels, int width, int height) override;
};

/// Output node — marks the end of the graph
class ColorOutputNode : public ColorNode {
    W_OBJECT(ColorOutputNode)
public:
    explicit ColorOutputNode(QObject* parent = nullptr);
    ~ColorOutputNode();
    void process(float* pixels, int width, int height) override;
};

/// Lift/Gamma/Gain correction node (DaVinci primary wheels)
class LiftGammaGainNode : public ColorNode {
    W_OBJECT(LiftGammaGainNode)
private:
    // Lift (shadows): RGB offset
    float liftR_ = 0.0f, liftG_ = 0.0f, liftB_ = 0.0f;
    // Gamma (midtones): RGB power
    float gammaR_ = 1.0f, gammaG_ = 1.0f, gammaB_ = 1.0f;
    // Gain (highlights): RGB multiplier
    float gainR_ = 1.0f, gainG_ = 1.0f, gainB_ = 1.0f;
    // Offset: global RGB shift
    float offsetR_ = 0.0f, offsetG_ = 0.0f, offsetB_ = 0.0f;

public:
    explicit LiftGammaGainNode(QObject* parent = nullptr);
    ~LiftGammaGainNode();

    void setLift(float r, float g, float b);
    void setGamma(float r, float g, float b);
    void setGain(float r, float g, float b);
    void setOffset(float r, float g, float b);

    void process(float* pixels, int width, int height) override;
};

/// Contrast node with adjustable pivot
class ContrastNode : public ColorNode {
    W_OBJECT(ContrastNode)
private:
    float contrast_ = 1.0f;
    float pivot_ = 0.18f;       // 18% grey (standard film pivot)
    float saturation_ = 1.0f;

public:
    explicit ContrastNode(QObject* parent = nullptr);
    ~ContrastNode();

    void setContrast(float c);
    float contrast() const { return contrast_; }

    void setPivot(float p);
    float pivot() const { return pivot_; }

    void setSaturation(float s);
    float saturation() const { return saturation_; }

    void process(float* pixels, int width, int height) override;
};

/// Color Space transform node
class ColorSpaceNode : public ColorNode {
    W_OBJECT(ColorSpaceNode)
private:
    int sourceSpace_ = 0;   // enum indices
    int targetSpace_ = 0;

public:
    explicit ColorSpaceNode(QObject* parent = nullptr);
    ~ColorSpaceNode();

    void setSourceSpace(int space);
    void setTargetSpace(int space);

    void process(float* pixels, int width, int height) override;
};

/// Merge/Blend node — combines two inputs
class MergeNode : public ColorNode {
    W_OBJECT(MergeNode)
public:
    enum class BlendMode {
        Over,
        Add,
        Multiply,
        Screen,
        Subtract,
        Difference,
        SoftLight,
        Overlay
    };

private:
    BlendMode blendMode_ = BlendMode::Over;
    float opacity_ = 1.0f;

    // Secondary input buffer (set by graph evaluator)
    const float* secondaryInput_ = nullptr;

public:
    explicit MergeNode(QObject* parent = nullptr);
    ~MergeNode();

    void setBlendMode(BlendMode mode);
    BlendMode blendMode() const { return blendMode_; }

    void setOpacity(float opacity);
    float opacity() const { return opacity_; }

    /// Called by graph evaluator to provide second input
    void setSecondaryInput(const float* pixels);

    void process(float* pixels, int width, int height) override;
};

// ============================================================
// Additional concrete node types
// ============================================================

/// Curves node — master + per-channel RGB curves
class CurvesNode : public ColorNode {
    W_OBJECT(CurvesNode)
public:
    /// A single control point on a curve (x=input, y=output, both 0-1)
    struct CurvePoint {
        float x = 0.0f;
        float y = 0.0f;
    };

private:
    std::vector<CurvePoint> masterCurve_;
    std::vector<CurvePoint> redCurve_;
    std::vector<CurvePoint> greenCurve_;
    std::vector<CurvePoint> blueCurve_;

    // Pre-built 256-entry LUT for each channel
    float masterLUT_[256]{};
    float redLUT_[256]{};
    float greenLUT_[256]{};
    float blueLUT_[256]{};
    bool lutDirty_ = true;

    void buildLUTs();
    static float evaluateCurve(const std::vector<CurvePoint>& points, float x);

public:
    explicit CurvesNode(QObject* parent = nullptr);
    ~CurvesNode();

    void setMasterCurve(const std::vector<CurvePoint>& points);
    void setRedCurve(const std::vector<CurvePoint>& points);
    void setGreenCurve(const std::vector<CurvePoint>& points);
    void setBlueCurve(const std::vector<CurvePoint>& points);

    const std::vector<CurvePoint>& masterCurve() const { return masterCurve_; }
    const std::vector<CurvePoint>& redCurve() const { return redCurve_; }
    const std::vector<CurvePoint>& greenCurve() const { return greenCurve_; }
    const std::vector<CurvePoint>& blueCurve() const { return blueCurve_; }

    /// Apply an S-curve preset
    void applySCurve(float strength = 0.5f);
    /// Reset all curves to identity
    void resetCurves();

    void process(float* pixels, int width, int height) override;
};

/// Hue / Saturation / Lightness node
class HueSaturationNode : public ColorNode {
    W_OBJECT(HueSaturationNode)
private:
    float hueShift_ = 0.0f;        // -180 to +180 degrees
    float saturation_ = 1.0f;       // 0 = grey, 1 = original, 2 = oversaturated
    float lightness_ = 0.0f;        // -1 to +1

    // Per-hue saturation adjustments (6 sectors: R, Y, G, C, B, M)
    float hueSat_[6] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };

    static void rgbToHsl(float r, float g, float b, float& h, float& s, float& l);
    static void hslToRgb(float h, float s, float l, float& r, float& g, float& b);

public:
    explicit HueSaturationNode(QObject* parent = nullptr);
    ~HueSaturationNode();

    void setHueShift(float degrees);
    float hueShift() const { return hueShift_; }

    void setSaturation(float s);
    float saturation() const { return saturation_; }

    void setLightness(float l);
    float lightness() const { return lightness_; }

    /// Set per-hue saturation: index 0=Red, 1=Yellow, 2=Green, 3=Cyan, 4=Blue, 5=Magenta
    void setHueSectorSaturation(int index, float value);
    float hueSectorSaturation(int index) const;

    void process(float* pixels, int width, int height) override;
};

/// Color Balance node — Shadow / Midtone / Highlight balance (three-way corrector)
class ColorBalanceNode : public ColorNode {
    W_OBJECT(ColorBalanceNode)
private:
    // Shadow balance (R, G, B offset applied to dark regions)
    float shadowR_ = 0.0f, shadowG_ = 0.0f, shadowB_ = 0.0f;
    // Midtone balance
    float midtoneR_ = 0.0f, midtoneG_ = 0.0f, midtoneB_ = 0.0f;
    // Highlight balance
    float highlightR_ = 0.0f, highlightG_ = 0.0f, highlightB_ = 0.0f;

    // Range thresholds
    float shadowRange_ = 0.33f;
    float highlightRange_ = 0.66f;

public:
    explicit ColorBalanceNode(QObject* parent = nullptr);
    ~ColorBalanceNode();

    void setShadowBalance(float r, float g, float b);
    void setMidtoneBalance(float r, float g, float b);
    void setHighlightBalance(float r, float g, float b);

    void setShadowRange(float range);
    float shadowRange() const { return shadowRange_; }
    void setHighlightRange(float range);
    float highlightRange() const { return highlightRange_; }

    void process(float* pixels, int width, int height) override;
};

/// Exposure node — EV-based exposure adjustment
class ExposureNode : public ColorNode {
    W_OBJECT(ExposureNode)
private:
    float exposure_ = 0.0f;     // EV stops (-10 to +10)
    float gamma_ = 1.0f;        // Gamma correction
    float offset_ = 0.0f;       // Black level offset

public:
    explicit ExposureNode(QObject* parent = nullptr);
    ~ExposureNode();

    void setExposure(float ev);
    float exposure() const { return exposure_; }

    void setGamma(float g);
    float gamma() const { return gamma_; }

    void setOffset(float o);
    float offset() const { return offset_; }

    void process(float* pixels, int width, int height) override;
};

/// Invert node — invert color channels
class InvertNode : public ColorNode {
    W_OBJECT(InvertNode)
private:
    bool invertR_ = true;
    bool invertG_ = true;
    bool invertB_ = true;
    bool invertA_ = false;

public:
    explicit InvertNode(QObject* parent = nullptr);
    ~InvertNode();

    void setInvertChannels(bool r, bool g, bool b, bool a = false);

    void process(float* pixels, int width, int height) override;
};

/// Clamp node — restrict values to a given range
class ClampNode : public ColorNode {
    W_OBJECT(ClampNode)
private:
    float minValue_ = 0.0f;
    float maxValue_ = 1.0f;
    bool clampRGB_ = true;
    bool clampAlpha_ = false;

public:
    explicit ClampNode(QObject* parent = nullptr);
    ~ClampNode();

    void setRange(float minVal, float maxVal);
    float minValue() const { return minValue_; }
    float maxValue() const { return maxValue_; }
    void setClampAlpha(bool clamp);

    void process(float* pixels, int width, int height) override;
};

/// HSL Qualifier node — DaVinci Resolve-style color qualifier
/// Selects pixels based on Hue, Saturation, Luminance ranges
/// Outputs a mask via the secondary output port
class QualifierNode : public ColorNode {
    W_OBJECT(QualifierNode)
private:
    // Hue range (0-360)
    float hueCenter_ = 0.0f;
    float hueWidth_ = 30.0f;
    float hueSoftness_ = 10.0f;

    // Saturation range (0-1)
    float satLow_ = 0.0f;
    float satHigh_ = 1.0f;
    float satSoftness_ = 0.05f;

    // Luminance range (0-1)
    float lumLow_ = 0.0f;
    float lumHigh_ = 1.0f;
    float lumSoftness_ = 0.05f;

    bool invert_ = false;

    // Generated mask buffer (allocated on first use)
    std::vector<float> maskBuffer_;

public:
    explicit QualifierNode(QObject* parent = nullptr);
    ~QualifierNode();

    // Hue settings
    void setHueRange(float center, float width, float softness = 10.0f);
    float hueCenter() const { return hueCenter_; }
    float hueWidth() const { return hueWidth_; }

    // Saturation settings
    void setSatRange(float low, float high, float softness = 0.05f);

    // Luminance settings
    void setLumRange(float low, float high, float softness = 0.05f);

    void setInvert(bool inv);
    bool isInverted() const { return invert_; }

    /// Get the generated mask (valid after process() call)
    const float* mask() const;
    int maskSize() const;

    void process(float* pixels, int width, int height) override;
};

/// Gaussian Blur node — for glow effects and softening
class BlurNode : public ColorNode {
    W_OBJECT(BlurNode)
private:
    float radiusX_ = 5.0f;     // Horizontal blur radius (pixels)
    float radiusY_ = 5.0f;     // Vertical blur radius (pixels)
    bool lockAspect_ = true;    // Keep X and Y equal

public:
    explicit BlurNode(QObject* parent = nullptr);
    ~BlurNode();

    void setRadius(float radius);
    void setRadius(float rx, float ry);
    float radiusX() const { return radiusX_; }
    float radiusY() const { return radiusY_; }

    void setLockAspect(bool lock);
    bool lockAspect() const { return lockAspect_; }

    void process(float* pixels, int width, int height) override;
};

// ============================================================
// Node Factory
// ============================================================

class ColorNodeFactory {
public:
    static std::unique_ptr<ColorNode> create(ColorNodeType type);
    static QString typeName(ColorNodeType type);
    static QString typeDisplayName(ColorNodeType type);
    static std::vector<ColorNodeType> allTypes();
};

} // namespace Artifact
