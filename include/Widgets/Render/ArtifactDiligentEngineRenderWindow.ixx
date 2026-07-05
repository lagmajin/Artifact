module;
#include <utility>
#include <memory>


#include <Buffer.h>
#include <PipelineState.h>
#include <RefCntAutoPtr.hpp>
#include <Sampler.h>
#include <SwapChain.h>
#include <Texture.h>
#include <RenderDevice.h>
#include <DeviceContext.h>
#include <ShaderResourceBinding.h>
#include <QColor>
#include <QPoint>
#include <QString>
#include <QWindow>
#include <QWidget>
#include <wobjectimpl.h>


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
   RefCntAutoPtr<IBuffer> solidIndexBuffer_;
   RefCntAutoPtr<IBuffer> solidTransformBuffer_;
   RefCntAutoPtr<IBuffer> solidColorBuffer_;
   RefCntAutoPtr<ITexture> baseColorTexture_;
   RefCntAutoPtr<ITextureView> baseColorTextureSrv_;
   RefCntAutoPtr<ITexture> metallicRoughnessTexture_;
   RefCntAutoPtr<ITextureView> metallicRoughnessTextureSrv_;
   RefCntAutoPtr<ITexture> normalTexture_;
   RefCntAutoPtr<ITextureView> normalTextureSrv_;
   RefCntAutoPtr<ISampler> baseColorSampler_;
   RefCntAutoPtr<IPipelineState> solidPso_;
   RefCntAutoPtr<IPipelineState> wirePso_;
   RefCntAutoPtr<IShaderResourceBinding> solidSrb_;
   RefCntAutoPtr<IShaderResourceBinding> wireSrb_;
   Uint32 solidVertexCount_ = 0;
   Uint32 solidIndexCount_ = 0;
   QString baseColorTexturePath_;
   QString metallicRoughnessTexturePath_;
   QString normalTexturePath_;
   QColor materialBaseColor_{ 180, 185, 195 };
   float materialMetallic_ = 0.0f;
   float materialRoughness_ = 0.55f;
   bool baseColorTextureDirty_ = true;
   bool metallicRoughnessTextureDirty_ = true;
   bool normalTextureDirty_ = true;
   float previewZoom_ = 1.0f;
   float previewYaw_ = 35.0f;
   float previewPitch_ = 25.0f;
   QVector3D previewTarget_{0.0f, 0.0f, 0.0f};
   float previewDistance_ = 4.0f;
   void render();
   void present();
   void ensureSolidResources();
   void uploadMeshGeometry();
   void updateBaseColorTexture();
   void updateMetallicRoughnessTexture();
   void updateNormalTexture();
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
  void setBaseColorTexture(const QString& path);
  void setMetallicRoughnessTexture(const QString& path);
  void setNormalTexture(const QString& path);
  void setPbrMaterial(const QColor& baseColor, float metallic, float roughness);
  void setPreviewCamera(float zoom, float yawDeg, float pitchDeg, const QVector3D& target = QVector3D(0.0f, 0.0f, 0.0f));
  float previewZoom() const;
  float previewYaw() const;
  float previewPitch() const;
  QVector3D previewTarget() const;
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
