module;
#include <EngineFactory.h>
#include <EngineFactoryD3D12.h>
#include <DiligentCore/Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h>
#include <Buffer.h>
#include <PipelineState.h>
#include <Shader.h>
#include <ShaderResourceBinding.h>
#include <wobjectimpl.h>
#include <windows.h>

#include <QSize>
#include <QEvent>
#include <cstring>
#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>


#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>

module ArtifactDiligentEngineRenderWindow;

import Graphics;
import Mesh;
import Artifact.Render.DiligentDeviceManager;

namespace {
 Diligent::IEngineFactoryD3D12* resolveD3D12Factory()
 {
#if D3D12_SUPPORTED
#if DILIGENT_D3D12_SHARED
  return Diligent::LoadAndGetEngineFactoryD3D12();
#else
  return Diligent::GetEngineFactoryD3D12();
#endif
#else
  return nullptr;
#endif
 }

 Diligent::IEngineFactoryVk* resolveVkFactory()
 {
#if VULKAN_SUPPORTED
#if DILIGENT_VK_EXPLICIT_LOAD
  return Diligent::LoadAndGetEngineFactoryVk();
#else
  return Diligent::GetEngineFactoryVk();
#endif
#else
  return nullptr;
#endif
 }
}

namespace
{
 using namespace Diligent;

 const char* kSolidViewportVS = R"(
cbuffer TransformCB : register(b0)
{
    float4x4 WorldMatrix;
    float4x4 ViewMatrix;
    float4x4 ProjMatrix;
};

struct VSInput
{
    float3 position : ATTRIB0;
};

struct VSOutput
{
    float4 position : SV_POSITION;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    float4 worldPos = mul(float4(input.position, 1.0f), WorldMatrix);
    float4 viewPos = mul(worldPos, ViewMatrix);
    output.position = mul(viewPos, ProjMatrix);
    return output;
}
)";

 const char* kSolidViewportPS = R"(
cbuffer ColorCB : register(b0)
{
    float4 SolidColor;
};

struct PSInput
{
    float4 position : SV_POSITION;
};

float4 main(PSInput input) : SV_TARGET
{
    return SolidColor;
}
)";

 struct SolidViewportConstants
 {
  QMatrix4x4 world;
  QMatrix4x4 view;
  QMatrix4x4 proj;
  QVector4D color;
 };

 std::vector<QVector3D> makeFallbackCubeVertices()
 {
  return {
   {-1.0f, -1.0f,  1.0f}, { 1.0f, -1.0f,  1.0f}, {-1.0f,  1.0f,  1.0f},
   {-1.0f,  1.0f,  1.0f}, { 1.0f, -1.0f,  1.0f}, { 1.0f,  1.0f,  1.0f},

   {-1.0f, -1.0f, -1.0f}, {-1.0f,  1.0f, -1.0f}, { 1.0f, -1.0f, -1.0f},
   {-1.0f,  1.0f, -1.0f}, { 1.0f,  1.0f, -1.0f}, { 1.0f, -1.0f, -1.0f},

   {-1.0f, -1.0f, -1.0f}, {-1.0f, -1.0f,  1.0f}, {-1.0f,  1.0f, -1.0f},
   {-1.0f,  1.0f, -1.0f}, {-1.0f, -1.0f,  1.0f}, {-1.0f,  1.0f,  1.0f},

   { 1.0f, -1.0f, -1.0f}, { 1.0f,  1.0f, -1.0f}, { 1.0f, -1.0f,  1.0f},
   { 1.0f,  1.0f, -1.0f}, { 1.0f,  1.0f,  1.0f}, { 1.0f, -1.0f,  1.0f},

   {-1.0f,  1.0f, -1.0f}, {-1.0f,  1.0f,  1.0f}, { 1.0f,  1.0f, -1.0f},
   {-1.0f,  1.0f,  1.0f}, { 1.0f,  1.0f,  1.0f}, { 1.0f,  1.0f, -1.0f},

   {-1.0f, -1.0f, -1.0f}, { 1.0f, -1.0f, -1.0f}, {-1.0f, -1.0f,  1.0f},
   {-1.0f, -1.0f,  1.0f}, { 1.0f, -1.0f, -1.0f}, { 1.0f, -1.0f,  1.0f},
  };
 }
}

namespace Artifact {

 W_OBJECT_IMPL(ArtifactDiligentEngineRenderWindow)

 class ArtifactDiligentEngineRenderWindow::Impl
 {
 private:
  RefCntAutoPtr<IRenderDevice> pDevice;
  RefCntAutoPtr<IDeviceContext> pImmediateContext;
  RefCntAutoPtr<ISwapChain> pSwapChain;

 public:
  Impl();
  ~Impl();
  bool initialize();
  void clear();
  void drawGrid();
  void postProcessing();
 };

 ArtifactDiligentEngineRenderWindow::Impl::~Impl()
 {

 }

 bool ArtifactDiligentEngineRenderWindow::Impl::initialize()
 {
  return false;
 }

 void ArtifactDiligentEngineRenderWindow::Impl::drawGrid()
 {

 }


  ArtifactDiligentEngineRenderWindow::ArtifactDiligentEngineRenderWindow(QWindow* parent /*= nullptr*/) :QWindow(parent)
  {

  }

ArtifactDiligentEngineRenderWindow::~ArtifactDiligentEngineRenderWindow()
{
 if (solidSrb_) {
  solidSrb_ = nullptr;
 }
 if (wireSrb_) {
  wireSrb_ = nullptr;
 }
 solidPso_ = nullptr;
 wirePso_ = nullptr;
 solidVertexBuffer_ = nullptr;
 solidTransformBuffer_ = nullptr;
 solidColorBuffer_ = nullptr;
  if (usingSharedDevice_) {
   pSwapChain.Release();
   pImmediateContext.Release();
   pDevice.Release();
   releaseSharedRenderDevice();
  usingSharedDevice_ = false;
  }
 }

 void ArtifactDiligentEngineRenderWindow::renderWireframeObject()
 {
  setShadingMode(ShadingMode::Wireframe);
 }

bool ArtifactDiligentEngineRenderWindow::initialize()
{
  const QString backendStr = qEnvironmentVariable("ARTIFACT_RENDER_BACKEND").toLower();
  if (backendStr == "software" || backendStr == "sw") {
      useSoftwareFallback_ = true;
      m_initialized = true;
      return true;
  }

  if (!acquireSharedRenderDeviceForCurrentBackend(pDevice, pImmediateContext)) {
      useSoftwareFallback_ = true;
      m_initialized = true;
      return true;
  }

  usingSharedDevice_ = true;
  Win32NativeWindow nativeWindow;
  nativeWindow.hWnd = reinterpret_cast<HWND>(winId());
  SwapChainDesc swapChainDesc;
  FullScreenModeDesc fullScreenDesc;
  fullScreenDesc.Fullscreen = false;

  bool initSuccess = false;
  if (sharedRenderDeviceType() == RENDER_DEVICE_TYPE_VULKAN) {
      auto* pFactoryVk = resolveVkFactory();
      if (pFactoryVk) {
          pFactoryVk->CreateSwapChainVk(pDevice, pImmediateContext, swapChainDesc, nativeWindow, &pSwapChain);
          initSuccess = pSwapChain != nullptr;
      }
  } else {
      auto* pFactory = resolveD3D12Factory();
      if (pFactory) {
          pFactory->CreateSwapChainD3D12(pDevice, pImmediateContext, swapChainDesc, fullScreenDesc, nativeWindow, &pSwapChain);
          initSuccess = pSwapChain != nullptr;
      }
  }

  if (!initSuccess) {
      pSwapChain.Release();
      pImmediateContext.Release();
      pDevice.Release();
      if (usingSharedDevice_) {
          releaseSharedRenderDevice();
          usingSharedDevice_ = false;
      }
      useSoftwareFallback_ = true;
  }
  
  m_initialized = true;

  return true;
 }

 void ArtifactDiligentEngineRenderWindow::setMesh(std::shared_ptr<ArtifactCore::Mesh> mesh)
 {
  mesh_ = std::move(mesh);
  meshDirty_ = true;
  requestRender();
 }

 void ArtifactDiligentEngineRenderWindow::clearMesh()
 {
  mesh_.reset();
  meshDirty_ = true;
  requestRender();
 }

 void ArtifactDiligentEngineRenderWindow::setShadingMode(ShadingMode mode)
 {
  shadingMode_ = mode;
  requestRender();
 }

 ArtifactDiligentEngineRenderWindow::ShadingMode ArtifactDiligentEngineRenderWindow::shadingMode() const
 {
  return shadingMode_;
 }

 void ArtifactDiligentEngineRenderWindow::setClearColor(const QColor& color)
 {
  clearColor_ = color;
  requestRender();
 }

 QColor ArtifactDiligentEngineRenderWindow::clearColor() const
 {
  return clearColor_;
 }

 void ArtifactDiligentEngineRenderWindow::requestRender()
 {
  if (!isExposed())
   return;
  if (!m_initialized)
   initialize();
  render();
  present();
 }

 void ArtifactDiligentEngineRenderWindow::ensureSolidResources()
 {
  if (solidResourcesReady_ || !pDevice) {
   return;
  }

  ShaderCreateInfo vsInfo;
  vsInfo.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
  vsInfo.Desc.ShaderType = SHADER_TYPE_VERTEX;
  vsInfo.Desc.Name = "ArtifactSolidViewportVS";
  vsInfo.EntryPoint = "main";
  vsInfo.Source = kSolidViewportVS;
  vsInfo.SourceLength = static_cast<Uint32>(std::strlen(kSolidViewportVS));

  ShaderCreateInfo psInfo;
  psInfo.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
  psInfo.Desc.ShaderType = SHADER_TYPE_PIXEL;
  psInfo.Desc.Name = "ArtifactSolidViewportPS";
  psInfo.EntryPoint = "main";
  psInfo.Source = kSolidViewportPS;
  psInfo.SourceLength = static_cast<Uint32>(std::strlen(kSolidViewportPS));

  RefCntAutoPtr<IShader> vs;
  RefCntAutoPtr<IShader> ps;
  pDevice->CreateShader(vsInfo, &vs);
  pDevice->CreateShader(psInfo, &ps);
  if (!vs || !ps) {
   return;
  }

  BufferDesc transformDesc;
  transformDesc.Name = "ArtifactSolidViewportTransformCB";
  transformDesc.Usage = USAGE_DYNAMIC;
  transformDesc.BindFlags = BIND_UNIFORM_BUFFER;
  transformDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
  transformDesc.Size = sizeof(float) * 16 * 3;
  pDevice->CreateBuffer(transformDesc, nullptr, &solidTransformBuffer_);

  BufferDesc colorDesc;
  colorDesc.Name = "ArtifactSolidViewportColorCB";
  colorDesc.Usage = USAGE_DYNAMIC;
  colorDesc.BindFlags = BIND_UNIFORM_BUFFER;
  colorDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
  colorDesc.Size = sizeof(float) * 4;
  pDevice->CreateBuffer(colorDesc, nullptr, &solidColorBuffer_);

  auto createPSO = [&](FILL_MODE fillMode, RefCntAutoPtr<IPipelineState>& outPSO, RefCntAutoPtr<IShaderResourceBinding>& outSRB) {
   GraphicsPipelineStateCreateInfo psoCI;
   psoCI.PSODesc.Name = (fillMode == FILL_MODE_WIREFRAME) ? "ArtifactSolidViewportWirePSO" : "ArtifactSolidViewportPSO";
   psoCI.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
   psoCI.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

   auto& gp = psoCI.GraphicsPipeline;
   gp.NumRenderTargets = 1;
   gp.RTVFormats[0] = TEX_FORMAT_RGBA8_UNORM_SRGB;
   gp.DSVFormat = TEX_FORMAT_D32_FLOAT;
   gp.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
   gp.RasterizerDesc.FillMode = fillMode;
   gp.RasterizerDesc.CullMode = CULL_MODE_BACK;
   gp.RasterizerDesc.FrontCounterClockwise = False;
   gp.RasterizerDesc.ScissorEnable = True;
   gp.DepthStencilDesc.DepthEnable = True;
   gp.DepthStencilDesc.DepthWriteEnable = True;

   LayoutElement layout[] = {
    LayoutElement{0, 0, 3, VT_FLOAT32, False},
   };
   gp.InputLayout.LayoutElements = layout;
   gp.InputLayout.NumElements = 1;
   psoCI.pVS = vs;
   psoCI.pPS = ps;

   ShaderResourceVariableDesc vars[] = {
    {SHADER_TYPE_VERTEX, "TransformCB", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
    {SHADER_TYPE_PIXEL, "ColorCB", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
   };
   psoCI.PSODesc.ResourceLayout.Variables = vars;
   psoCI.PSODesc.ResourceLayout.NumVariables = 2;

   pDevice->CreateGraphicsPipelineState(psoCI, &outPSO);
   if (outPSO) {
    outPSO->CreateShaderResourceBinding(&outSRB, true);
    if (outSRB) {
     outPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "TransformCB")->Set(solidTransformBuffer_);
     outPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "ColorCB")->Set(solidColorBuffer_);
    }
   }
  };

  createPSO(FILL_MODE_SOLID, solidPso_, solidSrb_);
  createPSO(FILL_MODE_WIREFRAME, wirePso_, wireSrb_);

  solidResourcesReady_ = static_cast<bool>(solidPso_) && static_cast<bool>(wirePso_) &&
                         static_cast<bool>(solidTransformBuffer_) && static_cast<bool>(solidColorBuffer_);
 }

 void ArtifactDiligentEngineRenderWindow::uploadMeshGeometry()
 {
  std::vector<QVector3D> vertices;
  if (mesh_) {
    const auto renderData = mesh_->generateRenderData();
   vertices.reserve(renderData.positions.size());
   for (const auto& pos : renderData.positions) {
    vertices.push_back(pos);
   }
  }

  if (vertices.empty()) {
   vertices = makeFallbackCubeVertices();
  }

  solidVertexCount_ = static_cast<Uint32>(vertices.size());
  if (!pDevice || !pImmediateContext || solidVertexCount_ == 0) {
   return;
  }

  const Uint64 requiredSize = sizeof(QVector3D) * static_cast<Uint64>(vertices.size());
  if (!solidVertexBuffer_ || solidVertexBuffer_->GetDesc().Size != requiredSize) {
   BufferDesc vbDesc;
   vbDesc.Name = "ArtifactSolidViewportVB";
   vbDesc.Usage = USAGE_DYNAMIC;
   vbDesc.BindFlags = BIND_VERTEX_BUFFER;
   vbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
   vbDesc.Size = requiredSize;
   pDevice->CreateBuffer(vbDesc, nullptr, &solidVertexBuffer_);
  }

  if (!solidVertexBuffer_) {
   return;
  }

  void* mappedData = nullptr;
  pImmediateContext->MapBuffer(solidVertexBuffer_, MAP_WRITE, MAP_FLAG_DISCARD, mappedData);
  if (mappedData) {
   std::memcpy(mappedData, vertices.data(), requiredSize);
   pImmediateContext->UnmapBuffer(solidVertexBuffer_, MAP_WRITE);
   meshDirty_ = false;
  }
 }

 void ArtifactDiligentEngineRenderWindow::pickingRay(int posx, int posy)
 {

 }

 void ArtifactDiligentEngineRenderWindow::render()
 {
  if (!m_initialized)
    return;

  if (useSoftwareFallback_) {
      return; // fallback placeholder
  }

  if (!pSwapChain || !pImmediateContext) return;

  ensureSolidResources();
  if (meshDirty_) {
      uploadMeshGeometry();
  }

  auto* pRTV = pSwapChain->GetCurrentBackBufferRTV();
  auto* pDSV = pSwapChain->GetDepthBufferDSV();

  pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  float r = static_cast<float>(clearColor_.redF());
  float g = static_cast<float>(clearColor_.greenF());
  float b = static_cast<float>(clearColor_.blueF());
  if (shadingMode_ == ShadingMode::Wireframe) {
   r = (std::min)(1.0f, r + 0.10f);
   g = (std::min)(1.0f, g + 0.10f);
   b = (std::min)(1.0f, b + 0.10f);
  } else if (shadingMode_ == ShadingMode::SolidWithWire) {
   b = (std::min)(1.0f, b + 0.12f);
  }
  const float ClearColor[] = { r, g, b, 1.0f };
  // Let the engine perform required state transitions
  pImmediateContext->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  if (solidPso_ && solidVertexBuffer_ && solidTransformBuffer_ && solidColorBuffer_ && solidVertexCount_ > 0) {
      const auto scDesc = pSwapChain->GetDesc();
      const float aspect = (scDesc.Height > 0) ? static_cast<float>(scDesc.Width) / static_cast<float>(scDesc.Height) : 1.0f;

      QMatrix4x4 world;
      world.setToIdentity();
      if (mesh_) {
          const QVector3D minBound = mesh_->boundingBoxMin();
          const QVector3D maxBound = mesh_->boundingBoxMax();
          const QVector3D center = (minBound + maxBound) * 0.5f;
          const QVector3D extent = maxBound - minBound;
          const float maxExtent = std::max({ extent.x(), extent.y(), extent.z(), 1.0f });
          world.translate(-center);
          world.scale(2.0f / maxExtent);
      }
      world.rotate(25.0f, 1.0f, 0.0f, 0.0f);
      world.rotate(35.0f, 0.0f, 1.0f, 0.0f);

      QMatrix4x4 view;
      view.setToIdentity();
      view.translate(0.0f, 0.0f, -4.0f);

      QMatrix4x4 proj;
      proj.setToIdentity();
      proj.perspective(45.0f, aspect, 0.1f, 100.0f);

      struct TransformCB {
          float world[16];
          float view[16];
          float proj[16];
      } transform{};
      std::memcpy(transform.world, world.constData(), sizeof(transform.world));
      std::memcpy(transform.view, view.constData(), sizeof(transform.view));
      std::memcpy(transform.proj, proj.constData(), sizeof(transform.proj));

      void* mappedData = nullptr;
      pImmediateContext->MapBuffer(solidTransformBuffer_, MAP_WRITE, MAP_FLAG_DISCARD, mappedData);
      if (mappedData) {
          std::memcpy(mappedData, &transform, sizeof(transform));
          pImmediateContext->UnmapBuffer(solidTransformBuffer_, MAP_WRITE);
      }

      const float solidColor[4] = {
          std::clamp(clearColor_.redF() * 1.35f + 0.10f, 0.0f, 1.0f),
          std::clamp(clearColor_.greenF() * 1.35f + 0.10f, 0.0f, 1.0f),
          std::clamp(clearColor_.blueF() * 1.35f + 0.10f, 0.0f, 1.0f),
          1.0f
      };
      pImmediateContext->MapBuffer(solidColorBuffer_, MAP_WRITE, MAP_FLAG_DISCARD, mappedData);
      if (mappedData) {
          std::memcpy(mappedData, solidColor, sizeof(solidColor));
          pImmediateContext->UnmapBuffer(solidColorBuffer_, MAP_WRITE);
      }

      auto* pPSO = (shadingMode_ == ShadingMode::Wireframe) ? wirePso_.RawPtr() : solidPso_.RawPtr();
      auto* pSRB = (shadingMode_ == ShadingMode::Wireframe) ? wireSrb_.RawPtr() : solidSrb_.RawPtr();
      if (shadingMode_ == ShadingMode::SolidWithWire && solidPso_ && wirePso_) {
          pPSO = solidPso_.RawPtr();
          pSRB = solidSrb_.RawPtr();
      }

      pImmediateContext->SetPipelineState(pPSO);
      pImmediateContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

       Uint64 offsets[] = { 0 };
       IBuffer* vertexBuffers[] = { solidVertexBuffer_.RawPtr() };
       pImmediateContext->SetVertexBuffers(0, 1, vertexBuffers, offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);

      DrawAttribs drawAttrs;
      drawAttrs.NumVertices = solidVertexCount_;
      drawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
      pImmediateContext->Draw(drawAttrs);

      if (shadingMode_ == ShadingMode::SolidWithWire && wirePso_ && wireSrb_) {
           pImmediateContext->SetPipelineState(wirePso_);
           pImmediateContext->CommitShaderResources(wireSrb_, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
           pImmediateContext->SetVertexBuffers(0, 1, vertexBuffers, offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
           pImmediateContext->Draw(drawAttrs);
       }
  }
 }

 void ArtifactDiligentEngineRenderWindow::present()
 {
  if (useSoftwareFallback_) return;

  if (pSwapChain) {
      pSwapChain->Present();
  }
 }

 void ArtifactDiligentEngineRenderWindow::resizeEvent(QResizeEvent* event)
 {
  Q_UNUSED(event);
  if (isExposed())
  {
   if (!m_initialized)
	initialize();
   render();
   present();
  }
 }

 void ArtifactDiligentEngineRenderWindow::exposeEvent(QExposeEvent* event)
 {
  Q_UNUSED(event);
  if (isExposed())
  {
   if (!m_initialized)
	initialize();
   render();
   present();
  }
 }

 void ArtifactDiligentEngineRenderWindow::keyPressEvent(QKeyEvent* event)
 {
  
 }

 void ArtifactDiligentEngineRenderWindow::mousePressEvent(QMouseEvent* event)
 {
  
 }

 class DiligentViewportWidget::Impl {
 private:

 public:
  Impl();
  ~Impl();
 };

 DiligentViewportWidget::Impl::Impl()
 {

 }

 DiligentViewportWidget::Impl::~Impl()
 {

 }

 void DiligentViewportWidget::keyPressEvent(QKeyEvent* event)
 {
  //throw std::logic_error("The method or operation is not implemented.");
 }

 void DiligentViewportWidget::resizeEvent(QResizeEvent* event)
 {
  //throw std::logic_error("The method or operation is not implemented.");
 }

 DiligentViewportWidget::DiligentViewportWidget(QWidget* parent /*= nullptr*/):QWidget(parent)
 {

 }

 DiligentViewportWidget::~DiligentViewportWidget()
 {

 }

 void DiligentViewportWidget::initializeDiligentEngineSafely()
 {

 }

 QSize DiligentViewportWidget::sizeHint() const
 {
  
  return QSize(600,400);
 }

};
