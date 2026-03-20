module;
#include <QColor>
#include <QPoint>
#include <QWindow>
#include <QWidget>


#include <wobjectimpl.h>
#include <RefCntAutoPtr.hpp>
#include <SwapChain.h>
#include <RenderDevice.h>
#include <DeviceContext.h>



export module ArtifactDiligentEngineRenderWindow;

namespace Diligent {};

namespace Artifact
{
 using namespace Diligent;


 export class ArtifactDiligentEngineRenderWindow : public QWindow
 {
  W_OBJECT(ArtifactDiligentEngineRenderWindow)
 public:
  enum class ShadingMode
  {
   Solid,
   Wireframe,
   SolidWithWire
  };

 private:
  class Impl;
  Impl* impl_;

  RefCntAutoPtr<IRenderDevice> pDevice;
  RefCntAutoPtr<IDeviceContext> pImmediateContext;
  RefCntAutoPtr<ISwapChain> pSwapChain;
  ShadingMode shadingMode_ = ShadingMode::Solid;
  QColor clearColor_{ 38, 38, 44 };
  bool useSoftwareFallback_ = false;
  bool usingSharedDevice_ = false;
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
  void setShadingMode(ShadingMode mode);
  ShadingMode shadingMode() const;
  void setClearColor(const QColor& color);
  QColor clearColor() const;
  void requestRender();
  bool m_initialized = false;
  void pickingRay(int posx,int posy);

 };

 export class DiligentViewportWidget :public QWidget {
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
