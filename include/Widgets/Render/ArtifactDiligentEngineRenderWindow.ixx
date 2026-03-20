module;
#include <QColor>
#include <QPoint>
#include <memory>
#include <QWindow>
#include <QWidget>


#include <wobjectimpl.h>
#include <Buffer.h>
#include <PipelineState.h>
#include <RefCntAutoPtr.hpp>
#include <SwapChain.h>
#include <RenderDevice.h>
#include <DeviceContext.h>
#include <ShaderResourceBinding.h>



export module ArtifactDiligentEngineRenderWindow;

import Mesh;

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
   std::shared_ptr<ArtifactCore::Mesh> mesh_;

   RefCntAutoPtr<IRenderDevice> pDevice;
   RefCntAutoPtr<IDeviceContext> pImmediateContext;
   RefCntAutoPtr<ISwapChain> pSwapChain;
   ShadingMode shadingMode_ = ShadingMode::Solid;
   QColor clearColor_{ 38, 38, 44 };
   bool useSoftwareFallback_ = false;
   bool usingSharedDevice_ = false;
   bool solidResourcesReady_ = false;
   bool meshDirty_ = true;
   RefCntAutoPtr<IBuffer> solidVertexBuffer_;
   RefCntAutoPtr<IBuffer> solidTransformBuffer_;
   RefCntAutoPtr<IBuffer> solidColorBuffer_;
   RefCntAutoPtr<IPipelineState> solidPso_;
   RefCntAutoPtr<IPipelineState> wirePso_;
   RefCntAutoPtr<IShaderResourceBinding> solidSrb_;
   RefCntAutoPtr<IShaderResourceBinding> wireSrb_;
   Uint32 solidVertexCount_ = 0;
   void render();
   void present();
   void ensureSolidResources();
   void uploadMeshGeometry();
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
  void setMesh(std::shared_ptr<ArtifactCore::Mesh> mesh);
  void clearMesh();
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
