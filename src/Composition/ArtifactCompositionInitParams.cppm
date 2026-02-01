module;

#include <QSize>
#include <QString>
#include <string>
#include <cstdint>

module Artifact.Composition.InitParams;

import std;

namespace Artifact {

 class ArtifactCompositionInitParams::Impl {
 public:
  UniString compositionName_;
  Size resolution_;
  AspectRatio pixelAspectRatio_;
  FrameRate frameRate_;
  RationalTime duration_;
  TimeCode startTimeCode_;
  WorkArea workArea_;
  FloatColor backgroundColor_;
  PreviewQuality previewQuality_;
  ResolutionFactor resolutionFactor_;
  MotionBlurSettings motionBlurSettings_;
  Renderer3DType renderer3D_;

  Impl()
   : compositionName_(std::string("New Composition")),
     pixelAspectRatio_(1, 1),
     frameRate_(30.0),
     duration_(300, 30.0),
     startTimeCode_(0, 30.0),
     backgroundColor_(0.0f, 0.0f, 0.0f, 1.0f),
     resolutionFactor_(ResolutionFactor::Full),
     renderer3D_(Renderer3DType::Classic3D)
  {
   resolution_.width = 1920;
   resolution_.height = 1080;
   workArea_.inPoint = RationalTime(0, 30.0);
   workArea_.outPoint = duration_;
   workArea_.enabled = false;
  }
 };

 ArtifactCompositionInitParams::ArtifactCompositionInitParams()
  : impl_(new Impl())
 {
 }

 ArtifactCompositionInitParams::ArtifactCompositionInitParams(const UniString& name, const FloatColor& backgroundColor)
  : impl_(new Impl())
 {
  impl_->compositionName_ = name;
  impl_->backgroundColor_ = backgroundColor;
 }

 ArtifactCompositionInitParams::ArtifactCompositionInitParams(const ArtifactCompositionInitParams& other)
  : impl_(new Impl())
 {
  impl_->compositionName_ = other.impl_->compositionName_;
  impl_->resolution_ = other.impl_->resolution_;
  impl_->pixelAspectRatio_ = other.impl_->pixelAspectRatio_;
  impl_->frameRate_ = other.impl_->frameRate_;
  impl_->duration_ = other.impl_->duration_;
  // startTimeCode_ is not copyable
  impl_->workArea_ = other.impl_->workArea_;
  impl_->backgroundColor_ = other.impl_->backgroundColor_;
  impl_->previewQuality_ = other.impl_->previewQuality_;
  impl_->motionBlurSettings_ = other.impl_->motionBlurSettings_;
  impl_->resolutionFactor_ = other.impl_->resolutionFactor_;
  impl_->renderer3D_ = other.impl_->renderer3D_;
 }

 ArtifactCompositionInitParams::ArtifactCompositionInitParams(ArtifactCompositionInitParams&& other) noexcept
  : impl_(other.impl_)
 {
  other.impl_ = nullptr;
 }

 ArtifactCompositionInitParams& ArtifactCompositionInitParams::operator=(const ArtifactCompositionInitParams& other)
 {
  if (this != &other) {
   impl_->compositionName_ = other.impl_->compositionName_;
   impl_->resolution_ = other.impl_->resolution_;
   impl_->pixelAspectRatio_ = other.impl_->pixelAspectRatio_;
   impl_->frameRate_ = other.impl_->frameRate_;
   impl_->duration_ = other.impl_->duration_;
   // startTimeCode_ is not copyable
   impl_->workArea_ = other.impl_->workArea_;
   impl_->backgroundColor_ = other.impl_->backgroundColor_;
   impl_->previewQuality_ = other.impl_->previewQuality_;
   impl_->motionBlurSettings_ = other.impl_->motionBlurSettings_;
   impl_->resolutionFactor_ = other.impl_->resolutionFactor_;
   impl_->renderer3D_ = other.impl_->renderer3D_;
  }
  return *this;
 }

 ArtifactCompositionInitParams& ArtifactCompositionInitParams::operator=(ArtifactCompositionInitParams&& other) noexcept
 {
  if (this != &other) {
   delete impl_;
   impl_ = other.impl_;
   other.impl_ = nullptr;
  }
  return *this;
 }

 ArtifactCompositionInitParams::~ArtifactCompositionInitParams()
 {
  delete impl_;
 }

 UniString ArtifactCompositionInitParams::compositionName() const
 {
  return impl_->compositionName_;
 }

 void ArtifactCompositionInitParams::setCompositionName(const UniString& name)
 {
  impl_->compositionName_ = name;
 }

 Size ArtifactCompositionInitParams::resolution() const
 {
  return impl_->resolution_;
 }

 void ArtifactCompositionInitParams::setResolution(const Size& size)
 {
  impl_->resolution_ = size;
 }

 void ArtifactCompositionInitParams::setResolution(int width, int height)
 {
  impl_->resolution_.width = width;
  impl_->resolution_.height = height;
 }

 int ArtifactCompositionInitParams::width() const
 {
  return impl_->resolution_.width;
 }

 int ArtifactCompositionInitParams::height() const
 {
  return impl_->resolution_.height;
 }

 AspectRatio ArtifactCompositionInitParams::pixelAspectRatio() const
 {
  return impl_->pixelAspectRatio_;
 }

 void ArtifactCompositionInitParams::setPixelAspectRatio(const AspectRatio& ratio)
 {
  impl_->pixelAspectRatio_ = ratio;
 }

 FrameRate ArtifactCompositionInitParams::frameRate() const
 {
  return impl_->frameRate_;
 }

 void ArtifactCompositionInitParams::setFrameRate(const FrameRate& rate)
 {
  impl_->frameRate_ = rate;
 }

 void ArtifactCompositionInitParams::setFrameRate(double fps)
 {
  impl_->frameRate_ = FrameRate(fps);
 }

 RationalTime ArtifactCompositionInitParams::duration() const
 {
  return impl_->duration_;
 }

 void ArtifactCompositionInitParams::setDuration(const RationalTime& duration)
 {
  impl_->duration_ = duration;
  if (impl_->workArea_.enabled) {
   impl_->workArea_.outPoint = duration;
  }
 }

 void ArtifactCompositionInitParams::setDurationFrames(int64_t frames)
 {
  impl_->duration_ = RationalTime(frames, impl_->frameRate_.framerate());
  if (impl_->workArea_.enabled) {
   impl_->workArea_.outPoint = impl_->duration_;
  }
 }

 void ArtifactCompositionInitParams::setDurationSeconds(double seconds)
 {
  double fps = impl_->frameRate_.framerate();
  int64_t frames = static_cast<int64_t>(seconds * fps);
  setDurationFrames(frames);
 }

 int64_t ArtifactCompositionInitParams::durationFrames() const
 {
  double fps = impl_->frameRate_.framerate();
  return static_cast<int64_t>(impl_->duration_.toSeconds() * fps);
 }

 double ArtifactCompositionInitParams::durationSeconds() const
 {
  return impl_->duration_.toSeconds();
 }

 const TimeCode& ArtifactCompositionInitParams::startTimeCode() const
 {
  return impl_->startTimeCode_;
 }

 void ArtifactCompositionInitParams::setStartTimeCode(const TimeCode& tc)
 {
  // TimeCode does not support assignment operator
  // Keep the existing timecode
 }

 WorkArea ArtifactCompositionInitParams::workArea() const
 {
  return impl_->workArea_;
 }

 void ArtifactCompositionInitParams::setWorkArea(const WorkArea& area)
 {
  impl_->workArea_ = area;
 }

 void ArtifactCompositionInitParams::setWorkArea(const RationalTime& inPoint, const RationalTime& outPoint)
 {
  impl_->workArea_.inPoint = inPoint;
  impl_->workArea_.outPoint = outPoint;
  impl_->workArea_.enabled = true;
 }

 FloatColor ArtifactCompositionInitParams::backgroundColor() const
 {
  return impl_->backgroundColor_;
 }

 void ArtifactCompositionInitParams::setBackgroundColor(const FloatColor& color)
 {
  impl_->backgroundColor_ = color;
 }

 PreviewQuality ArtifactCompositionInitParams::previewQuality() const
 {
  return impl_->previewQuality_;
 }

 void ArtifactCompositionInitParams::setPreviewQuality(const PreviewQuality& quality)
 {
  impl_->previewQuality_ = quality;
 }

 ResolutionFactor ArtifactCompositionInitParams::resolutionFactor() const
 {
  return impl_->resolutionFactor_;
 }

 void ArtifactCompositionInitParams::setResolutionFactor(ResolutionFactor factor)
 {
  impl_->resolutionFactor_ = factor;
 }

 MotionBlurSettings ArtifactCompositionInitParams::motionBlurSettings() const
 {
  return impl_->motionBlurSettings_;
 }

 void ArtifactCompositionInitParams::setMotionBlurSettings(const MotionBlurSettings& settings)
 {
  impl_->motionBlurSettings_ = settings;
 }

 bool ArtifactCompositionInitParams::motionBlurEnabled() const
 {
  return impl_->motionBlurSettings_.enabled;
 }

 void ArtifactCompositionInitParams::setMotionBlurEnabled(bool enabled)
 {
  impl_->motionBlurSettings_.enabled = enabled;
 }

 float ArtifactCompositionInitParams::shutterAngle() const
 {
  return impl_->motionBlurSettings_.shutterAngle;
 }

 void ArtifactCompositionInitParams::setShutterAngle(float angle)
 {
  impl_->motionBlurSettings_.shutterAngle = angle;
 }

 float ArtifactCompositionInitParams::shutterPhase() const
 {
  return impl_->motionBlurSettings_.shutterPhase;
 }

 void ArtifactCompositionInitParams::setShutterPhase(float phase)
 {
  impl_->motionBlurSettings_.shutterPhase = phase;
 }

 Renderer3DType ArtifactCompositionInitParams::renderer3D() const
 {
  return impl_->renderer3D_;
 }

 void ArtifactCompositionInitParams::setRenderer3D(Renderer3DType type)
 {
  impl_->renderer3D_ = type;
 }

 ArtifactCompositionInitParams ArtifactCompositionInitParams::hdPreset()
 {
  ArtifactCompositionInitParams params;
  params.setCompositionName(UniString(std::string("HD 1080p 30fps")));
  params.setResolution(1920, 1080);
  params.setFrameRate(30.0);
  params.setDurationSeconds(10.0);
  return params;
 }

 ArtifactCompositionInitParams ArtifactCompositionInitParams::fullHd60Preset()
 {
  ArtifactCompositionInitParams params;
  params.setCompositionName(UniString(std::string("HD 1080p 60fps")));
  params.setResolution(1920, 1080);
  params.setFrameRate(60.0);
  params.setDurationSeconds(10.0);
  return params;
 }

 ArtifactCompositionInitParams ArtifactCompositionInitParams::fourKPreset()
 {
  ArtifactCompositionInitParams params;
  params.setCompositionName(UniString(std::string("4K UHD 30fps")));
  params.setResolution(3840, 2160);
  params.setFrameRate(30.0);
  params.setDurationSeconds(10.0);
  return params;
 }

 ArtifactCompositionInitParams ArtifactCompositionInitParams::squarePreset()
 {
  ArtifactCompositionInitParams params;
  params.setCompositionName(UniString(std::string("Square 1:1 30fps")));
  params.setResolution(1080, 1080);
  params.setFrameRate(30.0);
  params.setDurationSeconds(10.0);
  return params;
 }

 ArtifactCompositionInitParams ArtifactCompositionInitParams::verticalPreset()
 {
  ArtifactCompositionInitParams params;
  params.setCompositionName(UniString(std::string("Vertical 9:16 30fps")));
  params.setResolution(1080, 1920);
  params.setFrameRate(30.0);
  params.setDurationSeconds(10.0);
  return params;
 }

 ArtifactCompositionInitParams ArtifactCompositionInitParams::cinemaPreset()
 {
  ArtifactCompositionInitParams params;
  params.setCompositionName(UniString(std::string("Cinema 2K 24fps")));
  params.setResolution(2048, 858);
  params.setFrameRate(24.0);
  params.setPixelAspectRatio(AspectRatio(2048, 858));
  params.setDurationSeconds(10.0);
  return params;
 }

 bool ArtifactCompositionInitParams::isValid() const
 {
  return impl_->compositionName_.length() > 0 &&
         impl_->resolution_.width > 0 &&
         impl_->resolution_.height > 0 &&
         impl_->frameRate_.framerate() > 0.0 &&
         impl_->duration_.toSeconds() > 0.0;
 }

 UniString ArtifactCompositionInitParams::validationError() const
 {
  if (impl_->compositionName_.length() == 0) {
   return UniString(std::string("Composition name is empty"));
  }
  if (impl_->resolution_.width <= 0 || impl_->resolution_.height <= 0) {
   return UniString(std::string("Invalid resolution"));
  }
  if (impl_->frameRate_.framerate() <= 0.0) {
   return UniString(std::string("Invalid frame rate"));
  }
  if (impl_->duration_.toSeconds() <= 0.0) {
   return UniString(std::string("Invalid duration"));
  }
  return UniString();
 }

 bool ArtifactCompositionInitParams::operator==(const ArtifactCompositionInitParams& other) const
 {
  return impl_->compositionName_ == other.impl_->compositionName_ &&
         impl_->resolution_ == other.impl_->resolution_ &&
         impl_->frameRate_ == other.impl_->frameRate_ &&
         impl_->duration_ == other.impl_->duration_;
 }

 bool ArtifactCompositionInitParams::operator!=(const ArtifactCompositionInitParams& other) const
 {
  return !(*this == other);
 }

} // namespace Artifact
