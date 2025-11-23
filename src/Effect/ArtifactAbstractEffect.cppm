module;


module Artifact.Effects;


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


};