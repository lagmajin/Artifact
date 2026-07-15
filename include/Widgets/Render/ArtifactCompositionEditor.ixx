module;
#include <utility>

#include <QWidget>
#include <QResizeEvent>
#include <QKeyEvent>
#include <wobjectdefs.h>
export module Artifact.Widgets.CompositionEditor;

import Color.Float;
import Artifact.Composition.Abstract;
import Artifact.Widgets.CompositionRenderController;

export namespace Artifact {

class ArtifactCompositionEditor: public QWidget {
 W_OBJECT(ArtifactCompositionEditor)
private:
 class Impl;
 Impl* impl_;

public:
 explicit ArtifactCompositionEditor(QWidget* parent = nullptr);
 ~ArtifactCompositionEditor();

 void setComposition(ArtifactCompositionPtr composition);
 void setClearColor(const FloatColor& color);
 CompositionRenderController* renderController() const;

 QSize sizeHint() const override;
  void resizeEvent(QResizeEvent* event) override;
  bool event(QEvent* event) override;

public /*slots*/:
  void play(); W_SLOT(play);
  void pause(); W_SLOT(pause);
  void togglePlayPause(); W_SLOT(togglePlayPause);
  void stop(); W_SLOT(stop);
  void resetView(); W_SLOT(resetView);
  void zoomIn(); W_SLOT(zoomIn);
  void zoomOut(); W_SLOT(zoomOut);
  void zoomFit(); W_SLOT(zoomFit);
 void zoomFill(); W_SLOT(zoomFill);
 void zoom100(); W_SLOT(zoom100);
  bool handleImportPlacementKeyPress(QKeyEvent* event);
  void toggleViewportToolboxes();

private:
  void refreshEnabledState();

signals:
  void videoDebugMessage(const QString& msg) W_SIGNAL(videoDebugMessage, msg);
 };

export void openContentsViewerCompareSurface();

}
