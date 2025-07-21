module;
#include <QWindow>
#include <QWidget>
#include <DiligentCore\Common\interface\BasicMath.hpp>
#include <wobjectdefs.h>
#include <DiligentCore\Graphics\GraphicsEngine\interface\RenderDevice.h>
#include <DiligentCore\Graphics\GraphicsEngine\interface\DeviceContext.h>
export module Widgets.Render.Composition;

namespace Diligent{}//dummy

export namespace Artifact {

 using namespace Diligent;

 class ArtifactDiligentEngineComposition2DWindow:public QWindow {
  W_OBJECT(ArtifactDiligentEngineComposition2DWindow)
 private:
  class Impl;
  Impl* impl_;
 protected:
  

  void resizeEvent(QResizeEvent*) override;

 public:
  explicit ArtifactDiligentEngineComposition2DWindow(QWindow* parent = nullptr);
  ~ArtifactDiligentEngineComposition2DWindow();
  void drawGizmo();
  bool clear(const Diligent::float4& clearColor);
  IRenderDevice* GetRenderDevice() const;
  IDeviceContext* GetDeviceContext() const;
 };

 class ArtifactDiligentEngineComposition2DWidget :public QWidget{
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactDiligentEngineComposition2DWidget(QWidget* parent = nullptr);
  ~ArtifactDiligentEngineComposition2DWidget();
 };



};
