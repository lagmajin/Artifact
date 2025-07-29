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
  void resizeEvent(QResizeEvent*) override;
  QSize sizeHint() const;


  void showEvent(QShowEvent* event) override;


  void paintEvent(QPaintEvent* event) override;


  void wheelEvent(QWheelEvent* event) override;


  void keyPressEvent(QKeyEvent* event) override;

  //void exposeEvent(QExposeEvent*) override;

 public:
  explicit ArtifactDiligentEngineComposition2DWindow(QWidget* parent = nullptr);
  ~ArtifactDiligentEngineComposition2DWindow();
  void setCanvasColor(const FloatColor& color);
  void drawGizmo();
  bool clear(const Diligent::float4& clearColor);
  void saveScreenShotToFile();
  void saveScreenShotToClipboard();
  IRenderDevice* GetRenderDevice() const;
  IDeviceContext* GetDeviceContext() const;
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
