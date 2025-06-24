module;
#include <QWindow>


#include <SwapChain.h>
#include <RenderDevice.h>
#include <RefCntAutoPtr.hpp>

#include <wobjectimpl.h>

export module ArtifactDiligentEngineRenderWindow;

namespace Diligent {};

namespace Artifact
{
 using namespace Diligent;



 class ArtifactDiligentEngineRenderWindowPrivate;

 class ArtifactDiligentEngineRenderWindow : public QWindow
 {
  W_OBJECT(ArtifactDiligentEngineRenderWindow)
 private:
  class Impl;
	

  RefCntAutoPtr<IRenderDevice> pDevice;
  RefCntAutoPtr<IDeviceContext> pImmediateContext;
  RefCntAutoPtr<ISwapChain> pSwapChain;
  void render();
  void present();
 protected:
  void resizeEvent(QResizeEvent* event);
  void exposeEvent(QExposeEvent* event);
 public:
  explicit ArtifactDiligentEngineRenderWindow(QWindow* parent = nullptr);
  ~ArtifactDiligentEngineRenderWindow();
  void renderWireframeObject();
  bool initialize();
  bool m_initialized = false;
  void pickingRay(int posx,int posy);
 };

}