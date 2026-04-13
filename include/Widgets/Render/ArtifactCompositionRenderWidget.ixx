module;
#include <utility>

#include <QWidget>
#include <QEnterEvent>
#include <QEvent>
#include <wobjectdefs.h>
export module Artifact.Widgets.CompositionRenderWidget;


import Color.Float;
import Artifact.Composition.Abstract;
import Artifact.Render.IRenderer;

export namespace Artifact {

 class ArtifactCompositionRenderWidget : public QWidget {
  W_OBJECT(ArtifactCompositionRenderWidget)
 private:
  class Impl;
  Impl* impl_;

 protected:
  void resizeEvent(QResizeEvent* event) override;
  void paintEvent(QPaintEvent* event) override;
  void showEvent(QShowEvent* event) override;
  void closeEvent(QCloseEvent* event) override;
  void leaveEvent(QEvent* event) override;
  void enterEvent(QEnterEvent* event) override;
  
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  
  void keyPressEvent(QKeyEvent* event) override;
  void keyReleaseEvent(QKeyEvent* event) override;
  void focusInEvent(QFocusEvent* event) override;
  void focusOutEvent(QFocusEvent* event) override;

 public:
  explicit ArtifactCompositionRenderWidget(QWidget* parent = nullptr);
  ~ArtifactCompositionRenderWidget();

  void setComposition(ArtifactCompositionPtr composition);
  void setClearColor(const FloatColor& color);
  
  void play();
  void stop();
  void resetView();
  void zoomIn();
  void zoomOut();
  void zoomFit();
  void zoom100();
 };

}
