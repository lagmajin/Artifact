module;
#include <QString>

export module Artifact.Layer.Media;

import Artifact.Layer.Abstract;

export namespace Artifact {

 class ArtifactMediaLayer : public ArtifactAbstractLayer {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactMediaLayer();
  ~ArtifactMediaLayer();

  void setSourceFile(const QString& path);
  QString sourceFile() const;

  void setHasAudio(bool hasAudio);
  void setHasVideo(bool hasVideo);

  void draw() override;
  bool hasAudio() const override;
  bool hasVideo() const override;
 };

 class ArtifactMediaLayer::Impl {
 public:
  QString sourceFile;
  bool hasAudio = true;
  bool hasVideo = true;
 };

 inline ArtifactMediaLayer::ArtifactMediaLayer() : impl_(new Impl())
 {
 }

 inline ArtifactMediaLayer::~ArtifactMediaLayer()
 {
  delete impl_;
 }

 inline void ArtifactMediaLayer::setSourceFile(const QString& path)
 {
  impl_->sourceFile = path;
 }

 inline QString ArtifactMediaLayer::sourceFile() const
 {
  return impl_->sourceFile;
 }

 inline void ArtifactMediaLayer::setHasAudio(bool hasAudio)
 {
  impl_->hasAudio = hasAudio;
 }

 inline void ArtifactMediaLayer::setHasVideo(bool hasVideo)
 {
  impl_->hasVideo = hasVideo;
 }

 inline void ArtifactMediaLayer::draw()
 {
  // TODO: Implement media playback rendering.
 }

 inline bool ArtifactMediaLayer::hasAudio() const
 {
  return impl_->hasAudio;
 }

 inline bool ArtifactMediaLayer::hasVideo() const
 {
  return impl_->hasVideo;
 }

}
