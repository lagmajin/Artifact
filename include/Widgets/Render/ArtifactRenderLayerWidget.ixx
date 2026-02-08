module;

#include <QWidget>

#include <wobjectdefs.h>
export module Artifact.Widgets.Render.Layer;

import Core.Scale.Zoom;
import Color.Float;
import Utils.Id;
import Core;
import Tool;

export namespace Artifact
{
 using namespace ArtifactCore;



 class ArtifactLayerEditor2DWidget :public QWidget
 {
  W_OBJECT(ArtifactLayerEditor2DWidget)
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
  void paintEvent(QPaintEvent* event) override;
  void showEvent(QShowEvent* event) override;
  void closeEvent(QCloseEvent* event) override;

  void focusInEvent(QFocusEvent* event) override;

 public:
  explicit ArtifactLayerEditor2DWidget(QWidget* parent = nullptr);
  ~ArtifactLayerEditor2DWidget();
  void setClearColor(const FloatColor& color);
  void setEditMode(EditMode mode);
  void setDisplayMode(DisplayMode mode);
  void setTargetLayerId(int id);
  void setTargetLayer(const LayerID& id);
  void resetView();
  float zoom() const;
  
  // Zoom toolbar methods
  void zoomIn();
  void zoomOut();
  void zoomFitWindow();
  void zoomTo100();
  void setZoomLevel(float zoomLevel);
  void setZoom(const ZoomScale2D& scale);
  QPointF pan() const;
  void setPan(const QPointF& offset);
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
