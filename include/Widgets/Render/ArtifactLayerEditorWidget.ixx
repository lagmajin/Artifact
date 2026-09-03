module;
#include <utility>
#include <wobjectdefs.h>
#include <QWidget>

export module Artifact.Widgets.LayerEditorWidget;

import Core.Scale.Zoom;
import Color.Float;
import Utils.Id;
//import Core;
import Tool;

export namespace Artifact {
 using namespace ArtifactCore;

 class ArtifactLayerEditorWidget :public QWidget
 {
  W_OBJECT(ArtifactLayerEditorWidget)
 private:
  class Impl;
  Impl* impl_;
 protected:
  void keyPressEvent(QKeyEvent* event) override;
  void keyReleaseEvent(QKeyEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;
  void paintEvent(QPaintEvent* event) override;
  void showEvent(QShowEvent* event) override;
  void hideEvent(QHideEvent* event) override;
  void closeEvent(QCloseEvent* event) override;

  void focusInEvent(QFocusEvent* event) override;
  void focusOutEvent(QFocusEvent* event) override;

 public:
  explicit ArtifactLayerEditorWidget(QWidget* parent = nullptr);
  ~ArtifactLayerEditorWidget();
  void setClearColor(const FloatColor& color);
  void setEditMode(EditMode mode);
  void setDisplayMode(DisplayMode mode);
  //void setTargetLayerId(int id);
  void setTargetLayer(const LayerID& id);
  void setPan(const QPointF& offset);
  void resetView();
  void fitToViewport();
  void panBy(const QPointF& delta);
  void zoomAroundPoint(const QPointF& viewportPos, float newZoom);
 
  float zoom() const;
  void setTargetLayer(LayerID& id);
  void clearTargetLayer();
  QImage grabScreenShot();
  void ChangeRenderAPI();
 public/**/:
  void mousePosUpdated();
 public/*slots*/:
  void play(); W_SLOT(play);
  void stop(); W_SLOT(stop);

  void takeScreenShot(); W_SLOT(takeScreenShot);
 };





};
