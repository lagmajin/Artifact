module;
//#include <EngineFactoryVk.h>

#include <DiligentCore/Graphics/GraphicsEngine/interface/EngineFactory.h>
#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h>
//#include <EngineFactoryD3D12.h>
#include <wobjectimpl.h>
#include <windows.h>


module ArtifactDiligentEngineRenderWindow;
//#include "../../../include/Widgets/Render/ArtifactDiligentEngineRenderWindow.hpp"

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib,"DiligentCore.lib")
#pragma comment(lib,"Diligent-Common.lib")
#pragma comment(lib,"Diligent-BasicPlatform.lib")
#pragma comment(lib,"Diligent-Win32Platform.lib")
#pragma comment(lib,"Diligent-GraphicsEngine.lib")
#pragma comment(lib,"Diligent-GraphicsEngineD3D12-static.lib")
#pragma comment(lib,"Diligent-GraphicsEngineD3DBase.lib")
#pragma comment(lib,"Diligent-GraphicsTools.lib")
#pragma comment(lib,"Diligent-GraphicsAccessories.lib")
#pragma comment(lib,"Diligent-Archiver-static.lib")

#pragma comment(lib,"Archiver_64d.lib")
#pragma comment(lib,"Diligent-HLSL2GLSLConverterLib.lib")


namespace Artifact {

 W_OBJECT_IMPL(ArtifactDiligentEngineRenderWindow)

 class ArtifactDiligentEngineRenderWindowPrivate
 {
 private:
  RefCntAutoPtr<IRenderDevice> pDevice;
  RefCntAutoPtr<IDeviceContext> pImmediateContext;
  RefCntAutoPtr<ISwapChain> pSwapChain;
 public:
  ArtifactDiligentEngineRenderWindowPrivate();
  ~ArtifactDiligentEngineRenderWindowPrivate();
  bool initialize();
  void clear();
  void drawGrid();
  void postProcessing();
 };

ArtifactDiligentEngineRenderWindowPrivate::ArtifactDiligentEngineRenderWindowPrivate()
 {

 }

 ArtifactDiligentEngineRenderWindowPrivate::~ArtifactDiligentEngineRenderWindowPrivate()
 {

 }

 bool ArtifactDiligentEngineRenderWindowPrivate::initialize()
 {

  return false;
 }

 void ArtifactDiligentEngineRenderWindowPrivate::clear()
 {

 }

 void ArtifactDiligentEngineRenderWindowPrivate::drawGrid()
 {

 }

 void ArtifactDiligentEngineRenderWindowPrivate::postProcessing()
 {

 }

 ArtifactDiligentEngineRenderWindow::ArtifactDiligentEngineRenderWindow(QWindow* parent /*= nullptr*/) :QWindow(parent)
 {

 }

 ArtifactDiligentEngineRenderWindow::~ArtifactDiligentEngineRenderWindow()
 {

 }

 void ArtifactDiligentEngineRenderWindow::renderWireframeObject()
 {
  PipelineStateCreateInfo PSOCreateInfo;
  auto desc=PSOCreateInfo.PSODesc;


  

	// GraphicsPipelineCreateInfo& GraphicsPipeline = PSOCreateInfo.

  //RasterizerStateDesc& RasterizerDesc = GraphicsPipeline.RasterizerDesc;
  //RasterizerDesc.FillMode = FILL_MODE_WIREFRAME; // ワイヤーフレームモードを指定
  //RasterizerDesc.CullMode = CULL_MODE_NONE;
 }

 bool ArtifactDiligentEngineRenderWindow::initialize()
 {
  auto* pFactory = GetEngineFactoryD3D12();

  EngineD3D12CreateInfo CreationAttribs = {};
  CreationAttribs.EnableValidation = true;

  // ウィンドウハンドルを設定
  Win32NativeWindow VkWindow;
  VkWindow.hWnd = reinterpret_cast<HWND>(winId());
  pFactory->CreateDeviceAndContextsD3D12(CreationAttribs, &pDevice, &pImmediateContext);

  // スワップチェインを作成
  SwapChainDesc SCDesc;
  
  FullScreenModeDesc desc;

  desc.Fullscreen = false;

  pFactory->CreateSwapChainD3D12(pDevice, pImmediateContext, SCDesc, desc, VkWindow, &pSwapChain);

  
  m_initialized = true;

  return true;
 }

 void ArtifactDiligentEngineRenderWindow::pickingRay(int posx, int posy)
 {

 }

 void ArtifactDiligentEngineRenderWindow::render()
 {
  if (!m_initialized)
   return;

  auto* pRTV = pSwapChain->GetCurrentBackBufferRTV();
  auto* pDSV = pSwapChain->GetDepthBufferDSV();

  pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  const float ClearColor[] = { 0.650f, 0.350f, 0.350f, 1.0f };
  // Let the engine perform required state transitions
  pImmediateContext->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
 
  
 }

 void ArtifactDiligentEngineRenderWindow::present()
 {
  pSwapChain->Present();
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

};