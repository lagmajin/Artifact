
module;
#include <QString>
export module Artifact.Effect.Abstract;

import std;
import Utils.Id;
import Utils.String.UniString;
import Artifact.Effect.Context;
import Image.ImageF32x4RGBAWithCache;

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
  virtual void apply(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) = 0;
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
 };

 typedef std::shared_ptr<ArtifactAbstractEffect> ArtifactAbstractEffectPtr;
 typedef std::weak_ptr<ArtifactAbstractEffect>	 ArtifactAbstractEffectWeakPtr;

};