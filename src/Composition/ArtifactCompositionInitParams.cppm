module;
#include <memory>

module Artifact.Composition.InitParams;

import std;

namespace Artifact {

 class ArtifactCompositionInitParams::Impl {
 public:
  Impl() = default;
  ~Impl() = default;
  FloatColor backgroundColor_;
 };

 ArtifactCompositionInitParams::ArtifactCompositionInitParams() :impl_(new Impl())
 {

 }

 ArtifactCompositionInitParams::~ArtifactCompositionInitParams()
 {

 }

 FloatColor ArtifactCompositionInitParams::backgroundColor() const
 {
  return impl_->backgroundColor_;
 }

 void ArtifactCompositionInitParams::setBackgroundColor(const FloatColor& color)
 {
  impl_->backgroundColor_ = color;
 }



 // Destructor is defaulted in the header; no manual delete needed.

};
