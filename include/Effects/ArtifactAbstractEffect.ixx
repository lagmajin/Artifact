module;
#include "../../../ArtifactCore/include/Define/DllExportMacro.hpp"
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
#include <QString>

export module Artifact.Effect.Abstract;




import Utils.Id;
import Utils.String.UniString;
import Artifact.Effect.Context;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Artifact.Effect.ImplBase;
import Memory.SharedPtr;
import Property.Abstract;
import Artifact.Render.ROI;
import Artifact.Render.PointwiseEffectFusion;
import Audio.Modulation.Router;

export namespace Artifact {

using namespace ArtifactCore;

class ArtifactAbstractEffect;
using ArtifactAbstractEffectPtr = SharedPtr<ArtifactAbstractEffect>;
using ArtifactAbstractEffectWeakPtr = WeakPtr<ArtifactAbstractEffect>;

enum class GpuSpatialEffectKind : std::uint8_t {
    SeparableGaussianBlur,
    Sharpen,
    Stripes,
    HexGrid,
    Vignette,
    ChromaticAberration,
    Voronoi,
    Bricks,
    Kaleidoscope,
    Halftone,
    RasterGlow,
    Generic,
    LensDistortion,
    ChromaKey,
};

enum class GpuRasterEffectDomain : std::uint8_t {
    None,
    Pointwise,
    Spatial,
};

struct GpuSpatialEffectNode {
    GpuSpatialEffectKind kind = GpuSpatialEffectKind::SeparableGaussianBlur;
    // Dedicated spatial effects may need more than the eight parameters
    // exposed by the generic resident shader. Keep this fixed-size and
    // allocation-free so the stack remains safe in the render hot path.
    std::array<float, 16> parameters{};
    std::uint8_t resolutionScaledParameterMask = 0;
    // Key into the shared generic-resident shader registry. Only meaningful
    // when kind == GpuSpatialEffectKind::Generic; ignored otherwise.
    std::uint32_t genericKey = 0;
};

struct GpuSpatialEffectStack {
    static constexpr std::size_t kCapacity = 8;
    std::array<GpuSpatialEffectNode, kCapacity> nodes{};
    std::size_t count = 0;

    LIBRARY_DLL_API bool append(const GpuSpatialEffectNode& node);
};

enum class GpuGenericResourceKind : std::uint8_t {
    Filter,
    Generator,
};

struct GpuGenericShaderRecord {
    const char* shaderBody = nullptr;
    const char* entryPoint = "main";
    GpuGenericResourceKind resource = GpuGenericResourceKind::Filter;
};

constexpr std::uint32_t gpuGenericKeyFromString(const char* text) {
    std::uint32_t hash = 2166136261u;
    while (text && *text) {
        hash ^= static_cast<std::uint32_t>(*text++);
        hash *= 16777619u;
    }
    return hash;
}

// Shared registry for generic-resident shaders. Effects register once from
// their constructor (always before any render); the render pipeline looks up
// by node.genericKey without inspecting concrete effect types. Missing keys
// fail closed to the CPU path.
LIBRARY_DLL_API void registerGpuGenericShader(
    std::uint32_t key, const GpuGenericShaderRecord& record);
LIBRARY_DLL_API bool findGpuGenericShader(
    std::uint32_t key, GpuGenericShaderRecord* outRecord);

class LIBRARY_DLL_API EffectID {
public:
    EffectID() = default;
    EffectID(const QString& value) : value_(value) {}
    EffectID(const char* value) : value_(QString::fromUtf8(value ? value : "")) {}

    const QString& toString() const noexcept { return value_; }
    bool isEmpty() const noexcept { return value_.isEmpty(); }

    friend bool operator==(const EffectID& lhs, const EffectID& rhs) {
        return lhs.value_ == rhs.value_;
    }

private:
    // Factory IDs are stable string keys (for example "blur"), not UUIDs.
    QString value_;
};

enum class ComputeMode {
    CPU,
    GPU,
    AUTO // おまかせモード
};

enum class EffectPipelineStage {
    PreProcess,
    Generator,
    GeometryTransform,
    MaterialRender,
    Rasterizer,
    LayerTransform
};

struct EffectUIDescriptor {
    QString displayName;
    bool preview = true;
    bool preset = true;
    bool appearance = false;
    bool fallback = false;
    QString section = QStringLiteral("Advanced");
};

class LIBRARY_DLL_API ArtifactAbstractEffect {
private:
    class Impl;
    Impl* impl_;
protected:
    // apply single-frame image processing: src -> dst
    virtual void apply(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst);
    // Hook for adapters that need the host's frame/time context while keeping
    // the public context API non-breaking for existing effects.
    virtual void onContextUpdated(const EffectContext& context) { (void)context; }

public:
    ArtifactAbstractEffect();
    virtual ~ArtifactAbstractEffect();

    // lifecycle
    virtual bool initialize();
    virtual void release();

    // enabled
    void setEnabled(bool enabled);
    bool isEnabled() const;

    // Global contribution, applied after an effect generates its result.
    void setMix(float mix);
    float mix() const;

    // compute mode
    ComputeMode computeMode() const;
    void setComputeMode(ComputeMode mode);
    virtual bool supportsGPU() const { return false; }

    // Generic-resident key for GpuSpatialEffectKind::Generic nodes. Effects
    // that contribute generic nodes return the key they registered their
    // shader record under; the default 0 never resolves in the registry.
    virtual std::uint32_t gpuGenericKey() const { return 0; }

    // GPU-resident raster paths ask effects to contribute their execution
    // nodes; the renderer deliberately does not inspect concrete effect types.
    // Returning false establishes an explicit CPU boundary for this effect.
    virtual bool appendGpuPointwiseNodes(
        ArtifactCore::PointwiseEffectStack& stack,
        std::uint32_t& parameterSlot) const {
        (void)stack;
        (void)parameterSlot;
        return false;
    }

    virtual GpuRasterEffectDomain gpuRasterEffectDomain() const {
        return GpuRasterEffectDomain::None;
    }

    // Effects contribute backend-neutral spatial nodes. The renderer owns
    // execution and resources and never inspects concrete effect types.
    virtual bool appendGpuSpatialNodes(GpuSpatialEffectStack& stack) const {
        (void)stack;
        return false;
    }

    // Opt-in: spatial effects may render beyond the source layer bounds.
    // Defaulting to false preserves legacy clipping behavior.
    void setAllowOverscan(bool enabled);
    bool allowOverscan() const;

    // Optional effect-local rectangle in source-surface pixel coordinates.
    // When set, the effect is blended only inside this region; mask images
    // can further restrict the same result.
    bool hasEffectRegion() const;
    QRectF effectRegion() const;
    void setEffectRegion(const QRectF& region);
    void clearEffectRegion();

    // Per-effect mask metadata.
    // Runtime blending can hook into this later without changing the preset format.
    bool hasMask() const;
    void setMaskEnabled(bool enabled);
    bool maskEnabled() const;
    void setMaskImage(const SharedPtr<ImageF32x4_RGBA>& maskImage);
    SharedPtr<ImageF32x4_RGBA> maskImage() const;
    void setMaskLayerId(const QString& layerId);
    QString maskLayerId() const;
    void setMaskName(const QString& name);
    QString maskName() const;
    void setMaskInverted(bool inverted);
    bool maskInverted() const;
    void setMaskOpacity(float opacity);
    float maskOpacity() const;

    // Additional effect-level mask images. These combine with the primary mask.
    void addEffectMaskImage(const SharedPtr<ImageF32x4_RGBA>& maskImage);
    void removeEffectMaskImage(int index);
    void clearEffectMaskImages();
    int effectMaskImageCount() const;
    SharedPtr<ImageF32x4_RGBA> effectMaskImage(int index) const;

    // identification
    UniString effectID() const;
    void setEffectID(const UniString& id);
    UniString displayName() const;
    void setDisplayName(const UniString& name);
    virtual EffectUIDescriptor uiDescriptor() const;

    // pipeline stage
    EffectPipelineStage pipelineStage() const;
    void setPipelineStage(EffectPipelineStage stage);
    static bool isStageOrderValid(const std::vector<ArtifactAbstractEffectPtr>& effects);
    static std::vector<ArtifactAbstractEffectPtr> sortedByStage(
        const std::vector<ArtifactAbstractEffectPtr>& effects);

    // effect execution
    void applyCPUOnly(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst);
    void applyConfigured(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst);

    // context
    void setContext(const EffectContext& context);

    // A per-effect control-rate router. Callers configure sources and
    // mappings explicitly; the router advances from EffectContext frame time.
    ArtifactCore::Audio::Modulation::ModulationRouter& modulationRouter();
    QString modulationPropertyPath(const QString& propertyName) const;

    // implementation management
    void setCPUImpl(SharedPtr<ArtifactEffectImplBase> impl);
    void setGPUImpl(SharedPtr<ArtifactEffectImplBase> impl);
    SharedPtr<ArtifactEffectImplBase> cpuImpl() const;
    SharedPtr<ArtifactEffectImplBase> gpuImpl() const;

    // Property interface (use ArtifactCore::AbstractProperty)
    // getProperties() は表示用スナップショット、editableProperties() が正本（キーフレーム保持）。
    // 新規Effectは editableProperties() を正として実装し、getProperties() は互換のためのビューとする。
    virtual std::vector<ArtifactCore::AbstractProperty> getProperties() const;
    virtual void setPropertyValue(const ArtifactCore::UniString& name, const QVariant& value);
    // Handles controls shared by every effect. Derived implementations should
    // call this before routing their own properties.
    bool setCommonPropertyValue(const QString& name, const QVariant& value);

    // Stable property objects used by editors and animation. getProperties()
    // remains the effect-specific value description API; these objects retain
    // keyframes and expressions across inspector rebuilds.
    std::vector<SharedPtr<ArtifactCore::AbstractProperty>> editableProperties();
    SharedPtr<ArtifactCore::AbstractProperty> editableProperty(const QString& name);

    // ROI hint for partial evaluation
    virtual EffectROIHint roiHint() const { return EffectROIHint{}; }

    // Canonical ROI expansion entry point for effect hosts. Keeping the
    // conversion here lets a future ROI renderer consume every effect through
    // one API instead of knowing individual blur/glow implementations.
    virtual RenderROI expandedROI(const RenderROI& input) const {
        return roiHint().apply(input);
    }
};

};
