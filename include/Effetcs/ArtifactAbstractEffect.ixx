module;
#include <QString>
export module Artifact.Effect.Abstract;

import std;
import Utils.Id;
import Utils.String.UniString;
import Artifact.Effect.Context;
import Image.ImageF32x4RGBAWithCache;
import Artifact.Effect.ImplBase;

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

    // context
    void setContext(const EffectContext& context);

    // implementation management
    void setCPUImpl(std::shared_ptr<ArtifactEffectImplBase> impl);
    void setGPUImpl(std::shared_ptr<ArtifactEffectImplBase> impl);
    std::shared_ptr<ArtifactEffectImplBase> cpuImpl() const;
    std::shared_ptr<ArtifactEffectImplBase> gpuImpl() const;
};

typedef std::shared_ptr<ArtifactAbstractEffect> ArtifactAbstractEffectPtr;
typedef std::weak_ptr<ArtifactAbstractEffect> ArtifactAbstractEffectWeakPtr;

};