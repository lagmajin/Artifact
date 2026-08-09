module;
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

export namespace Artifact {

using namespace ArtifactCore;

class EffectID {
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

class ArtifactAbstractEffect {
private:
    class Impl;
    Impl* impl_;
protected:
    // apply single-frame image processing: src -> dst
    virtual void apply(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst);

public:
    ArtifactAbstractEffect();
    virtual ~ArtifactAbstractEffect();

    // lifecycle
    virtual bool initialize();
    virtual void release();

    // enabled
    void setEnabled(bool enabled);
    bool isEnabled() const;

    // compute mode
    ComputeMode computeMode() const;
    void setComputeMode(ComputeMode mode);
    virtual bool supportsGPU() const { return false; }

    // Opt-in: spatial effects may render beyond the source layer bounds.
    // Defaulting to false preserves legacy clipping behavior.
    void setAllowOverscan(bool enabled);
    bool allowOverscan() const;

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

    // pipeline stage
    EffectPipelineStage pipelineStage() const;
    void setPipelineStage(EffectPipelineStage stage);

    // effect execution
    void applyCPUOnly(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst);
    void applyConfigured(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst);

    // context
    void setContext(const EffectContext& context);

    // implementation management
    void setCPUImpl(SharedPtr<ArtifactEffectImplBase> impl);
    void setGPUImpl(SharedPtr<ArtifactEffectImplBase> impl);
    SharedPtr<ArtifactEffectImplBase> cpuImpl() const;
    SharedPtr<ArtifactEffectImplBase> gpuImpl() const;

    // Property interface (use ArtifactCore::AbstractProperty)
    virtual std::vector<ArtifactCore::AbstractProperty> getProperties() const;
    virtual void setPropertyValue(const ArtifactCore::UniString& name, const QVariant& value);

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

typedef SharedPtr<ArtifactAbstractEffect> ArtifactAbstractEffectPtr;
typedef WeakPtr<ArtifactAbstractEffect> ArtifactAbstractEffectWeakPtr;

};
