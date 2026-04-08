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
import Artifact.Effect.ImplBase;
import Property.Abstract;

export namespace Artifact {

using namespace ArtifactCore;

class EffectID : public Id {
public:
    using Id::Id; // Idのコンストラクタを継承
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

    // context
    void setContext(const EffectContext& context);

    // implementation management
    void setCPUImpl(std::shared_ptr<ArtifactEffectImplBase> impl);
    void setGPUImpl(std::shared_ptr<ArtifactEffectImplBase> impl);
    std::shared_ptr<ArtifactEffectImplBase> cpuImpl() const;
    std::shared_ptr<ArtifactEffectImplBase> gpuImpl() const;

    // Property interface (use ArtifactCore::AbstractProperty)
    virtual std::vector<ArtifactCore::AbstractProperty> getProperties() const;
    virtual void setPropertyValue(const ArtifactCore::UniString& name, const QVariant& value);
};

typedef std::shared_ptr<ArtifactAbstractEffect> ArtifactAbstractEffectPtr;
typedef std::weak_ptr<ArtifactAbstractEffect> ArtifactAbstractEffectWeakPtr;

};
