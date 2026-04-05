module;
#include <QWidget>
#include <QResizeEvent>
#include <wobjectdefs.h>

export module Artifact.Widgets.CompositionEditor;

import Color.Float;
import Artifact.Composition.Abstract;

export namespace Artifact {

class ArtifactCompositionEditor : public QWidget {
 W_OBJECT(ArtifactCompositionEditor)
private:
 class Impl;
 Impl* impl_;

public:
 explicit ArtifactCompositionEditor(QWidget* parent = nullptr);
 ~ArtifactCompositionEditor();

 void setComposition(ArtifactCompositionPtr composition);
 void setClearColor(const FloatColor& color);

 QSize sizeHint() const override;
  void resizeEvent(QResizeEvent* event) override;
  bool event(QEvent* event) override;

public /*slots*/:
  void play(); W_SLOT(play);
  void stop(); W_SLOT(stop);
  void resetView(); W_SLOT(resetView);
  void zoomIn(); W_SLOT(zoomIn);
  void zoomOut(); W_SLOT(zoomOut);
  void zoomFit(); W_SLOT(zoomFit);
  void zoomFill(); W_SLOT(zoomFill);
  void zoom100(); W_SLOT(zoom100);

signals:
  void videoDebugMessage(const QString& msg) W_SIGNAL(videoDebugMessage, msg);
 };

}
