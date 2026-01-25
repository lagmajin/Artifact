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
  UniString compositionName_;
  PreviewQuality quality_;
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

 PreviewQuality ArtifactCompositionInitParams::quality() const
 {
  return impl_->quality_;
 }

 void ArtifactCompositionInitParams::setQuality(const PreviewQuality& quality)
 {
  impl_->quality_=quality;
 }

 UniString ArtifactCompositionInitParams::compositionName() const
 {
  return impl_->compositionName_;
 }

 void ArtifactCompositionInitParams::setCompositionName(const UniString& name)
 {
    impl_->compositionName_ = name;
 }

 // Destructor is defaulted in the header; no manual delete needed.

};
