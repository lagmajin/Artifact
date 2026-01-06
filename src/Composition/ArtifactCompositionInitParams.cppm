// ReSharper disable IdentifierTypo
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
  FrameRate framerate_;
 	
 };

 ArtifactCompositionInitParams::ArtifactCompositionInitParams() :impl_(new Impl())
 {

 }

 ArtifactCompositionInitParams::~ArtifactCompositionInitParams()
 {
  delete impl_;
 }

 FloatColor ArtifactCompositionInitParams::backgroundColor() const
 {
  return impl_->backgroundColor_;
 }

 
 void ArtifactCompositionInitParams::setBackgroundColor(const FloatColor& color)
 {
  impl_->backgroundColor_ = color;
 }

 FrameRate ArtifactCompositionInitParams::framerate() const
 {
  return impl_->framerate_;
 }

 void ArtifactCompositionInitParams::setFrameRate(const FrameRate& framerate)
 {
  impl_->framerate_ = framerate;
 }
 // Destructor is defaulted in the header; no manual delete needed.

};
