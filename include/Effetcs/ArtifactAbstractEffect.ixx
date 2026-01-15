
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
  virtual void apply();
 public:
  ArtifactAbstractEffect();
  virtual ~ArtifactAbstractEffect();
  ComputeMode computeMode() const;
  void setComputeMode(ComputeMode mode);
  virtual bool supportsGPU() const = 0;
 
  UniString effectID() const;
  UniString displayName() const;
   
 };

 typedef std::shared_ptr<ArtifactAbstractEffect> ArtifactAbstractEffectPtr;
 typedef std::weak_ptr<ArtifactAbstractEffect>	 ArtifactAbstractEffectWeakPtr;

};