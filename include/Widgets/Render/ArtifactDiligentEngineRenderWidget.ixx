module;
#include <QWindow>
#include <QWidget>
#include <wobjectdefs.h>
#include <DiligentCore\Common\interface\BasicMath.hpp>

#include <DiligentCore\Graphics\GraphicsEngine\interface\RenderDevice.h>
#include <DiligentCore\Graphics\GraphicsEngine\interface\DeviceContext.h>
export module Widgets.Render.Composition;

import Color.Float;

namespace Diligent{}//dummy

export namespace Artifact {

 using namespace Diligent;
 using namespace ArtifactCore;

 class ArtifactDiligentEngineComposition2DWindow:public QWidget {
  W_OBJECT(ArtifactDiligentEngineComposition2DWindow)
 private:
  class Impl;
  Impl* impl_;
 protected:
  QSize sizeHint() const;
  void resizeEvent(QResizeEvent*) override;

  void showEvent(QShowEvent* event) override;
  void paintEvent(QPaintEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;


  void mouseMoveEvent(QMouseEvent* event) override;
  void closeEvent(QCloseEvent* event) override;
 public:
  explicit ArtifactDiligentEngineComposition2DWindow(QWidget* parent = nullptr);
  ~ArtifactDiligentEngineComposition2DWindow();
  void setCanvasColor(const FloatColor& color);
  void drawGizmo();
  bool clear(const Diligent::float4& clearColor);
  void saveScreenShotToFile();
  void saveScreenShotToClipboard();
  void saveScreenShotToClipboardByQt();
  IRenderDevice* GetRenderDevice() const;
  IDeviceContext* GetDeviceContext() const;

  void previewStart();
  void previewPause();
  void previewStop();

//signals
  void screenClicked(const QPoint& point) W_SIGNAL(screenClicked,point)


 };

 class ArtifactDiligentEngineComposition2DWidget :public QWidget{
  W_OBJECT(ArtifactDiligentEngineComposition2DWidget)
 private:
  class Impl;
  Impl* impl_;
 protected:


  void paintEvent(QPaintEvent* event) override;

 public:
  explicit ArtifactDiligentEngineComposition2DWidget(QWidget* parent = nullptr);
  ~ArtifactDiligentEngineComposition2DWidget();
  void setCanvasColor(const FloatColor& color);
 };



};
