#pragma once
#include <QWindow>


#include <SwapChain.h>
#include <RenderDevice.h>
#include <RefCntAutoPtr.hpp>


namespace Artifact
{
 using namespace Diligent;

 class ArtifactDiligentEngineRenderWindowPrivate;

 class ArtifactDiligentEngineRenderWindow : public QWindow
 {
  Q_OBJECT
 private:
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
  bool initialize();
  bool m_initialized = false;
  void pickingRay(int posx,int posy);
 };

}