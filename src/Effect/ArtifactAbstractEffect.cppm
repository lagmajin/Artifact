module;


module Artifact.Effect.Abstract;


namespace Artifact {

 class ArtifactAbstractEffect::Impl {
 private:

 public:
  Impl();
  ~Impl();
 };

 ArtifactAbstractEffect::Impl::Impl()
 {

 }

 ArtifactAbstractEffect::Impl::~Impl()
 {

 }

 ArtifactAbstractEffect::ArtifactAbstractEffect():impl_(new Impl())
 {

 }

 ArtifactAbstractEffect::~ArtifactAbstractEffect()
 {
  delete impl_;
 }

 bool ArtifactAbstractEffect::supportsGPU() const
 {
  return false;
 }

 ComputeMode ArtifactAbstractEffect::computeMode() const
 {

  return ComputeMode::AUTO;
 }

 void ArtifactAbstractEffect::setComputeMode(ComputeMode mode)
 {

 }

};