module;

#include <QWidget>
#include <wobjectdefs.h>

export module Artifact.Widgets.RenderLayerEditor;

import Artifact.Widgets.RenderLayerWidgetv2;
import Color.Float;
import Utils.Id;

export namespace Artifact {
 using namespace ArtifactCore;

 class ArtifactRenderLayerEditor : public QWidget
 {
  W_OBJECT(ArtifactRenderLayerEditor)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactRenderLayerEditor(QWidget* parent = nullptr);
  ~ArtifactRenderLayerEditor();

  ArtifactLayerEditorWidgetV2* view() const;

  void setClearColor(const FloatColor& color);
  void setTargetLayer(const LayerID& id);
  void resetView();

 public/*slots*/:
  void play(); W_SLOT(play);
  void stop(); W_SLOT(stop);
  void takeScreenShot(); W_SLOT(takeScreenShot);
 };

}
