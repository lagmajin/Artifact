module;
#include <utility>
#include <wobjectdefs.h>
#include <QObject>

export module Artifact.Service.ActiveContext;

import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;

export namespace Artifact
{
 class ArtifactActiveContextService : public QObject
 {
  W_OBJECT(ArtifactActiveContextService)
 private:






  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactActiveContextService(QObject* parent = nullptr);
  ~ArtifactActiveContextService();
  
  void setHandler(QObject* obj);
  void setActiveComposition(ArtifactCompositionPtr comp);
  ArtifactCompositionPtr activeComposition() const;

  // Actions
  void play(); W_SLOT(play);
  void pause(); W_SLOT(pause);
  void togglePlayPause(); W_SLOT(togglePlayPause);
  void stop(); W_SLOT(stop);
  void nextFrame(); W_SLOT(nextFrame);
  void prevFrame(); W_SLOT(prevFrame);
  void goToStart(); W_SLOT(goToStart);
  void goToEnd(); W_SLOT(goToEnd);
  void seekToFrame(int64_t frame); W_SLOT(seekToFrame);

  // Layer Actions (AE Style)
  void setLayerInAtCurrentTime(); W_SLOT(setLayerInAtCurrentTime);
  void setLayerOutAtCurrentTime(); W_SLOT(setLayerOutAtCurrentTime);
  void trimLayerInAtCurrentTime(); W_SLOT(trimLayerInAtCurrentTime);
  void trimLayerOutAtCurrentTime(); W_SLOT(trimLayerOutAtCurrentTime);
  void splitLayerAtCurrentTime(); W_SLOT(splitLayerAtCurrentTime);

  void activeCompositionChanged(ArtifactCompositionPtr comp) W_SIGNAL(activeCompositionChanged, comp);

 };
};
