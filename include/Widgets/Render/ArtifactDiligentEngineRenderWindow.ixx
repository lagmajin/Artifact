module;
#include <QColor>
#include <QPoint>
#include <QWindow>
#include <QWidget>


#include <wobjectimpl.h>
#include <DiligentCore\Common\interface\RefCntAutoPtr.hpp>
#include <DiligentCore\Graphics\GraphicsEngine\interface\SwapChain.h>
#include <DiligentCore\Graphics\GraphicsEngine\interface\RenderDevice.h>
#include <DiligentCore\Graphics\GraphicsEngine\interface\DeviceContext.h>



export module ArtifactDiligentEngineRenderWindow;

namespace Diligent {};

namespace Artifact
{
 using namespace Diligent;


 class ArtifactDiligentEngineRenderWindow : public QWindow
 {
  W_OBJECT(ArtifactDiligentEngineRenderWindow)
 private:
  class Impl;
  Impl* impl_;

  RefCntAutoPtr<IRenderDevice> pDevice;
  RefCntAutoPtr<IDeviceContext> pImmediateContext;
  RefCntAutoPtr<ISwapChain> pSwapChain;
  void render();
  void present();
 protected:
  void resizeEvent(QResizeEvent* event);
  void exposeEvent(QExposeEvent* event);


  void keyPressEvent(QKeyEvent*) override;


  void mousePressEvent(QMouseEvent*) override;

 public:
  explicit ArtifactDiligentEngineRenderWindow(QWindow* parent = nullptr);
  ~ArtifactDiligentEngineRenderWindow();
  void renderWireframeObject();
  void renderQuadWithTexture();
  bool initialize();
  bool m_initialized = false;
  void pickingRay(int posx,int posy);

 };

 class DiligentViewportWidget :public QWidget {
 private:
  class Impl;
  Impl* impl_;


 protected:
  void keyPressEvent(QKeyEvent* event) override;


  void resizeEvent(QResizeEvent* event) override;
 public:
  explicit DiligentViewportWidget(QWidget* parent = nullptr);
  ~DiligentViewportWidget();
  void initializeDiligentEngineSafely();

  QSize sizeHint() const override;

 };



}