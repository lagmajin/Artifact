module;
#include <EngineFactory.h>
#include <EngineFactoryD3D12.h>
#include <DiligentCore/Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h>
#include <Buffer.h>
#include <PipelineState.h>
#include <Shader.h>
#include <ShaderResourceBinding.h>
#include <Sampler.h>
#include <Texture.h>
#include <wobjectimpl.h>
#include <windows.h>

#include <QSize>
#include <QEvent>
#include <cstring>
#include <QMatrix4x4>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QVector>
#include <QtMath>


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
import IO.ImageImporter;
import Image.Raw;
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

 QVector<quint8> expandTextureToRgba8(const ArtifactCore::RawImage& rawImage)
 {
  QVector<quint8> rgba8;
  const int pixelCount = rawImage.width * rawImage.height;
  const int channelSize = rawImage.getPixelTypeSizeInBytes();
  if (pixelCount <= 0 || rawImage.channels <= 0 || channelSize <= 0) {
   return rgba8;
  }

  rgba8.resize(pixelCount * 4);
  const quint8* srcBytes = rawImage.data.constData();
  const int srcStride = rawImage.channels * channelSize;
  auto sampleChannel = [&](int pixelIndex, int channelIndex) -> quint8 {
   const int sourceIndex =
       pixelIndex * srcStride + channelIndex * channelSize;
   if (rawImage.pixelType == QStringLiteral("uint8")) {
    return rawImage.data[static_cast<size_t>(sourceIndex)];
   }
   if (rawImage.pixelType == QStringLiteral("uint16")) {
    quint16 value = 0;
    std::memcpy(&value, srcBytes + sourceIndex, sizeof(value));
    return static_cast<quint8>(value / 257u);
   }
   if (rawImage.pixelType == QStringLiteral("float")) {
    float value = 0.0f;
    std::memcpy(&value, srcBytes + sourceIndex, sizeof(value));
    return static_cast<quint8>(
        std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
   }
   return 0;
  };

  for (int pixel = 0; pixel < pixelCount; ++pixel) {
   const quint8 c0 = sampleChannel(pixel, 0);
   const quint8 c1 =
       rawImage.channels > 1 ? sampleChannel(pixel, 1) : c0;
   const quint8 c2 =
       rawImage.channels > 2 ? sampleChannel(pixel, 2) : c0;
   const quint8 c3 =
       rawImage.channels > 3 ? sampleChannel(pixel, 3) : 255;
   rgba8[pixel * 4 + 0] = c0;
   rgba8[pixel * 4 + 1] = c1;
   rgba8[pixel * 4 + 2] = c2;
   rgba8[pixel * 4 + 3] = c3;
  }
  return rgba8;
 }

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
    float3 normal : ATTRIB1;
    float2 uv : ATTRIB2;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 worldPosition : TEXCOORD0;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD1;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    float4 worldPos = mul(WorldMatrix, float4(input.position, 1.0f));
    float4 viewPos = mul(ViewMatrix, worldPos);
    output.position = mul(ProjMatrix, viewPos);
    output.worldPosition = worldPos.xyz;
    output.normal = normalize(mul((float3x3)WorldMatrix, input.normal));
    output.uv = input.uv;
    return output;
}
)";

 const char* kSolidViewportPS = R"(
Texture2D BaseColorTexture : register(t0);
Texture2D MetallicRoughnessTexture : register(t1);
Texture2D NormalTexture : register(t2);
SamplerState BaseColorSampler : register(s0);

cbuffer ColorCB : register(b0)
{
    float4 BaseColor;
    float4 CameraPositionAndMetallic;
    float4 LightDirectionAndRoughness;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPosition : TEXCOORD0;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD1;
};

float4 main(PSInput input) : SV_TARGET
{
    const float pi = 3.14159265;
    float4 sampledBaseColor =
        BaseColorTexture.Sample(BaseColorSampler, input.uv) * BaseColor;
    float3 N = normalize(input.normal);
    float3 tangentNormal =
        NormalTexture.Sample(BaseColorSampler, input.uv).xyz * 2.0 - 1.0;
    float3 positionDx = ddx(input.worldPosition);
    float3 positionDy = ddy(input.worldPosition);
    float2 uvDx = ddx(input.uv);
    float2 uvDy = ddy(input.uv);
    float determinant = uvDx.x * uvDy.y - uvDx.y * uvDy.x;
    if (abs(determinant) > 1e-6) {
        float inverseDeterminant = 1.0 / determinant;
        float3 tangent =
            normalize((positionDx * uvDy.y - positionDy * uvDx.y) *
                      inverseDeterminant);
        float3 bitangent =
            normalize((-positionDx * uvDy.x + positionDy * uvDx.x) *
                      inverseDeterminant);
        N = normalize(
            tangent * tangentNormal.x +
            bitangent * tangentNormal.y +
            N * tangentNormal.z);
    }
    float4 metallicRoughnessSample =
        MetallicRoughnessTexture.Sample(BaseColorSampler, input.uv);
    float materialMetallic =
        saturate(CameraPositionAndMetallic.w * metallicRoughnessSample.b);
    float materialRoughness =
        clamp(
            LightDirectionAndRoughness.w * metallicRoughnessSample.g,
            0.04,
            1.0);
    float3 L = normalize(LightDirectionAndRoughness.xyz);
    float3 V =
        normalize(CameraPositionAndMetallic.xyz - input.worldPosition);
    float3 H = normalize(L + V);
    float NdotL = saturate(dot(N, L));
    float NdotV = max(saturate(dot(N, V)), 0.001);
    float NdotH = saturate(dot(N, H));
    float VdotH = saturate(dot(V, H));

    float alpha = max(materialRoughness * materialRoughness, 0.04);
    float alpha2 = alpha * alpha;
    float denom = NdotH * NdotH * (alpha2 - 1.0) + 1.0;
    float distribution = alpha2 / max(pi * denom * denom, 0.001);
    float k =
        ((materialRoughness + 1.0) * (materialRoughness + 1.0)) * 0.125;
    float geometryV = NdotV / max(NdotV * (1.0 - k) + k, 0.001);
    float geometryL = NdotL / max(NdotL * (1.0 - k) + k, 0.001);
    float3 f0 =
        lerp(float3(0.04, 0.04, 0.04), sampledBaseColor.rgb, materialMetallic);
    float3 fresnel = f0 + (1.0 - f0) * pow(1.0 - VdotH, 5.0);
    float3 specular = distribution * geometryV * geometryL * fresnel /
                      max(4.0 * NdotV * max(NdotL, 0.001), 0.001);
    float3 diffuse =
        (1.0 - fresnel) * (1.0 - materialMetallic) *
        sampledBaseColor.rgb / pi;
    // Neutral analytic environment fallback.  It keeps the viewport material
    // readable before an HDRI cubemap has been generated, and deliberately
    // stays in linear light until the final display conversion below.
    float skyWeight = saturate(N.y * 0.5 + 0.5);
    float3 diffuseEnvironment = lerp(
        float3(0.055, 0.050, 0.045),
        float3(0.34, 0.40, 0.52),
        skyWeight);
    float3 R = reflect(-V, N);
    float reflectionWeight = saturate(R.y * 0.5 + 0.5);
    float3 specularEnvironment = lerp(
        float3(0.025, 0.020, 0.018),
        float3(0.22, 0.30, 0.44),
        reflectionWeight);
    specularEnvironment *= lerp(1.0, 0.16, materialRoughness);
    float3 ambientDiffuse =
        diffuseEnvironment * sampledBaseColor.rgb *
        (1.0 - materialMetallic) * (1.0 - fresnel);
    float3 ambientSpecular = specularEnvironment * fresnel;
    float3 color = ambientDiffuse + ambientSpecular +
        (diffuse + specular) * NdotL * 2.2;
    color = color / (color + 1.0);
    color = pow(color, 1.0 / 2.2);
    return float4(color, sampledBaseColor.a);
}
)";

 struct SolidViewportVertex
 {
  float position[3];
  float normal[3];
  float uv[2];
 };

 struct SolidViewportMaterial
 {
  float baseColor[4];
  float cameraPositionAndMetallic[4];
  float lightDirectionAndRoughness[4];
 };

 std::vector<QVector3D> makeFallbackCubePositions()
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

 std::vector<SolidViewportVertex> makeFallbackCubeVertices()
 {
  const auto positions = makeFallbackCubePositions();
  std::vector<SolidViewportVertex> vertices;
  vertices.reserve(positions.size());
  for (size_t i = 0; i < positions.size(); i += 3) {
   const QVector3D edgeA = positions[i + 1] - positions[i];
   const QVector3D edgeB = positions[i + 2] - positions[i];
   const QVector3D normal = QVector3D::crossProduct(edgeA, edgeB).normalized();
   const float uvs[3][2] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}};
   for (size_t corner = 0; corner < 3; ++corner) {
    const QVector3D& position = positions[i + corner];
    vertices.push_back({
     {position.x(), position.y(), position.z()},
     {normal.x(), normal.y(), normal.z()},
     {uvs[corner][0], uvs[corner][1]}
    });
   }
  }
  return vertices;
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
 if (pImmediateContext) {
  pImmediateContext->Flush();
  pImmediateContext->WaitForIdle();
 }
 if (solidSrb_) {
  solidSrb_ = nullptr;
 }
 if (wireSrb_) {
  wireSrb_ = nullptr;
 }
 solidPso_ = nullptr;
 wirePso_ = nullptr;
 solidVertexBuffer_ = nullptr;
 solidIndexBuffer_ = nullptr;
 solidTransformBuffer_ = nullptr;
 solidColorBuffer_ = nullptr;
 baseColorTextureSrv_ = nullptr;
 baseColorTexture_ = nullptr;
 metallicRoughnessTextureSrv_ = nullptr;
 metallicRoughnessTexture_ = nullptr;
 normalTextureSrv_ = nullptr;
 normalTexture_ = nullptr;
 baseColorSampler_ = nullptr;
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

void ArtifactDiligentEngineRenderWindow::setBaseColorTexture(const QString& path)
{
  const QString normalizedPath = path.trimmed();
  if (baseColorTexturePath_ == normalizedPath) {
    return;
  }
  baseColorTexturePath_ = normalizedPath;
  baseColorTextureDirty_ = true;
  requestRender();
}

void ArtifactDiligentEngineRenderWindow::setMetallicRoughnessTexture(
    const QString& path)
{
  const QString normalizedPath = path.trimmed();
  if (metallicRoughnessTexturePath_ == normalizedPath) {
    return;
  }
  metallicRoughnessTexturePath_ = normalizedPath;
  metallicRoughnessTextureDirty_ = true;
  requestRender();
}

void ArtifactDiligentEngineRenderWindow::setNormalTexture(const QString& path)
{
  const QString normalizedPath = path.trimmed();
  if (normalTexturePath_ == normalizedPath) {
    return;
  }
  normalTexturePath_ = normalizedPath;
  normalTextureDirty_ = true;
  requestRender();
}

void ArtifactDiligentEngineRenderWindow::setPbrMaterial(
    const QColor& baseColor,
    float metallic,
    float roughness)
{
  materialBaseColor_ = baseColor;
  materialMetallic_ = std::clamp(metallic, 0.0f, 1.0f);
  materialRoughness_ = std::clamp(roughness, 0.04f, 1.0f);
  requestRender();
}

void ArtifactDiligentEngineRenderWindow::setPreviewCamera(float zoom, float yawDeg, float pitchDeg, const QVector3D& target)
{
  previewZoom_ = std::max(0.05f, zoom);
  previewYaw_ = yawDeg;
  previewPitch_ = pitchDeg;
  previewTarget_ = target;
  previewDistance_ = 5.0f / previewZoom_;
  requestRender();
}

float ArtifactDiligentEngineRenderWindow::previewZoom() const
{
  return previewZoom_;
}

float ArtifactDiligentEngineRenderWindow::previewYaw() const
{
  return previewYaw_;
}

float ArtifactDiligentEngineRenderWindow::previewPitch() const
{
  return previewPitch_;
}

QVector3D ArtifactDiligentEngineRenderWindow::previewTarget() const
{
  return previewTarget_;
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
  colorDesc.Size = sizeof(SolidViewportMaterial);
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
    LayoutElement{0, 0, 3, VT_FLOAT32, False, LAYOUT_ELEMENT_AUTO_OFFSET, sizeof(SolidViewportVertex)},
    LayoutElement{1, 0, 3, VT_FLOAT32, False, LAYOUT_ELEMENT_AUTO_OFFSET, sizeof(SolidViewportVertex)},
    LayoutElement{2, 0, 2, VT_FLOAT32, False, LAYOUT_ELEMENT_AUTO_OFFSET, sizeof(SolidViewportVertex)},
   };
   gp.InputLayout.LayoutElements = layout;
   gp.InputLayout.NumElements = 3;
   psoCI.pVS = vs;
   psoCI.pPS = ps;

   ShaderResourceVariableDesc vars[] = {
    {SHADER_TYPE_VERTEX, "TransformCB", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
    {SHADER_TYPE_PIXEL, "ColorCB", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
    {SHADER_TYPE_PIXEL, "BaseColorTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
    {SHADER_TYPE_PIXEL, "MetallicRoughnessTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
    {SHADER_TYPE_PIXEL, "NormalTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
    {SHADER_TYPE_PIXEL, "BaseColorSampler", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
   };
   psoCI.PSODesc.ResourceLayout.Variables = vars;
   psoCI.PSODesc.ResourceLayout.NumVariables = 6;

   pDevice->CreateGraphicsPipelineState(psoCI, &outPSO);
   if (outPSO) {
    outPSO->CreateShaderResourceBinding(&outSRB, true);
     outPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "TransformCB")->Set(solidTransformBuffer_);
     outPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "ColorCB")->Set(solidColorBuffer_);
     outPSO->GetStaticVariableByName(
         SHADER_TYPE_PIXEL, "BaseColorSampler")->Set(baseColorSampler_);
   }
  };

  SamplerDesc samplerDesc;
  samplerDesc.Name = "ArtifactSolidViewportBaseColorSampler";
  samplerDesc.MinFilter = FILTER_TYPE_LINEAR;
  samplerDesc.MagFilter = FILTER_TYPE_LINEAR;
  samplerDesc.MipFilter = FILTER_TYPE_LINEAR;
  samplerDesc.AddressU = TEXTURE_ADDRESS_WRAP;
  samplerDesc.AddressV = TEXTURE_ADDRESS_WRAP;
  samplerDesc.AddressW = TEXTURE_ADDRESS_WRAP;
  pDevice->CreateSampler(samplerDesc, &baseColorSampler_);

  createPSO(FILL_MODE_SOLID, solidPso_, solidSrb_);
  createPSO(FILL_MODE_WIREFRAME, wirePso_, wireSrb_);

  solidResourcesReady_ = static_cast<bool>(solidPso_) && static_cast<bool>(wirePso_) &&
                         static_cast<bool>(solidTransformBuffer_) && static_cast<bool>(solidColorBuffer_);
 }

 void ArtifactDiligentEngineRenderWindow::updateBaseColorTexture()
 {
  if (!baseColorTextureDirty_ || !pDevice) {
   return;
  }

  baseColorTextureDirty_ = false;
  baseColorTextureSrv_ = nullptr;
  baseColorTexture_ = nullptr;

  QVector<quint8> rgba8;
  Uint32 width = 1;
  Uint32 height = 1;
  if (!baseColorTexturePath_.isEmpty()) {
   ArtifactCore::ImageImporter importer;
   if (importer.open(baseColorTexturePath_)) {
    const ArtifactCore::RawImage rawImage = importer.readImage();
    if (rawImage.isValid()) {
     rgba8 = expandTextureToRgba8(rawImage);
     width = static_cast<Uint32>(rawImage.width);
     height = static_cast<Uint32>(rawImage.height);
    }
   }
  }
  if (rgba8.isEmpty()) {
   rgba8 = {255, 255, 255, 255};
   width = 1;
   height = 1;
  }

  TextureDesc textureDesc;
  textureDesc.Name = "ArtifactSolidViewportBaseColorTexture";
  textureDesc.Type = RESOURCE_DIM_TEX_2D;
  textureDesc.Width = width;
  textureDesc.Height = height;
  textureDesc.MipLevels = 1;
  textureDesc.Format = TEX_FORMAT_RGBA8_UNORM_SRGB;
  textureDesc.Usage = USAGE_IMMUTABLE;
  textureDesc.BindFlags = BIND_SHADER_RESOURCE;

  TextureSubResData subresource;
  subresource.pData = rgba8.constData();
  subresource.Stride = static_cast<Uint64>(width) * 4u;
  TextureData textureData;
  textureData.pSubResources = &subresource;
  textureData.NumSubresources = 1;
  pDevice->CreateTexture(textureDesc, &textureData, &baseColorTexture_);
  if (baseColorTexture_) {
   baseColorTextureSrv_ =
       baseColorTexture_->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
  }
 }

 void ArtifactDiligentEngineRenderWindow::updateMetallicRoughnessTexture()
 {
  if (!metallicRoughnessTextureDirty_ || !pDevice) {
   return;
  }

  metallicRoughnessTextureDirty_ = false;
  metallicRoughnessTextureSrv_ = nullptr;
  metallicRoughnessTexture_ = nullptr;

  QVector<quint8> rgba8;
  Uint32 width = 1;
  Uint32 height = 1;
  if (!metallicRoughnessTexturePath_.isEmpty()) {
   ArtifactCore::ImageImporter importer;
   if (importer.open(metallicRoughnessTexturePath_)) {
    const ArtifactCore::RawImage rawImage = importer.readImage();
    if (rawImage.isValid()) {
     rgba8 = expandTextureToRgba8(rawImage);
     width = static_cast<Uint32>(rawImage.width);
     height = static_cast<Uint32>(rawImage.height);
    }
   }
  }
  if (rgba8.isEmpty()) {
   rgba8 = {255, 255, 255, 255};
   width = 1;
   height = 1;
  }

  TextureDesc textureDesc;
  textureDesc.Name = "ArtifactSolidViewportMetallicRoughnessTexture";
  textureDesc.Type = RESOURCE_DIM_TEX_2D;
  textureDesc.Width = width;
  textureDesc.Height = height;
  textureDesc.MipLevels = 1;
  textureDesc.Format = TEX_FORMAT_RGBA8_UNORM;
  textureDesc.Usage = USAGE_IMMUTABLE;
  textureDesc.BindFlags = BIND_SHADER_RESOURCE;

  TextureSubResData subresource;
  subresource.pData = rgba8.constData();
  subresource.Stride = static_cast<Uint64>(width) * 4u;
  TextureData textureData;
  textureData.pSubResources = &subresource;
  textureData.NumSubresources = 1;
  pDevice->CreateTexture(
      textureDesc, &textureData, &metallicRoughnessTexture_);
  if (metallicRoughnessTexture_) {
   metallicRoughnessTextureSrv_ =
       metallicRoughnessTexture_->GetDefaultView(
           TEXTURE_VIEW_SHADER_RESOURCE);
  }
 }

 void ArtifactDiligentEngineRenderWindow::updateNormalTexture()
 {
  if (!normalTextureDirty_ || !pDevice) {
   return;
  }

  normalTextureDirty_ = false;
  normalTextureSrv_ = nullptr;
  normalTexture_ = nullptr;

  QVector<quint8> rgba8;
  Uint32 width = 1;
  Uint32 height = 1;
  if (!normalTexturePath_.isEmpty()) {
   ArtifactCore::ImageImporter importer;
   if (importer.open(normalTexturePath_)) {
    const ArtifactCore::RawImage rawImage = importer.readImage();
    if (rawImage.isValid()) {
     rgba8 = expandTextureToRgba8(rawImage);
     width = static_cast<Uint32>(rawImage.width);
     height = static_cast<Uint32>(rawImage.height);
    }
   }
  }
  if (rgba8.isEmpty()) {
   rgba8 = {128, 128, 255, 255};
   width = 1;
   height = 1;
  }

  TextureDesc textureDesc;
  textureDesc.Name = "ArtifactSolidViewportNormalTexture";
  textureDesc.Type = RESOURCE_DIM_TEX_2D;
  textureDesc.Width = width;
  textureDesc.Height = height;
  textureDesc.MipLevels = 1;
  textureDesc.Format = TEX_FORMAT_RGBA8_UNORM;
  textureDesc.Usage = USAGE_IMMUTABLE;
  textureDesc.BindFlags = BIND_SHADER_RESOURCE;

  TextureSubResData subresource;
  subresource.pData = rgba8.constData();
  subresource.Stride = static_cast<Uint64>(width) * 4u;
  TextureData textureData;
  textureData.pSubResources = &subresource;
  textureData.NumSubresources = 1;
  pDevice->CreateTexture(textureDesc, &textureData, &normalTexture_);
  if (normalTexture_) {
   normalTextureSrv_ =
       normalTexture_->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
  }
 }

 void ArtifactDiligentEngineRenderWindow::uploadMeshGeometry()
 {
  std::vector<SolidViewportVertex> vertices;
  std::vector<Uint32> indices;
  if (mesh_) {
    const auto renderData = mesh_->generateRenderData();
   vertices.reserve(renderData.positions.size());
   for (qsizetype i = 0; i < renderData.positions.size(); ++i) {
    const QVector3D& position = renderData.positions[i];
    const QVector3D normal =
        i < renderData.normals.size() ? renderData.normals[i].normalized()
                                      : QVector3D(0.0f, 0.0f, 1.0f);
    const QVector2D uv =
        i < renderData.uvs.size() ? renderData.uvs[i] : QVector2D();
    vertices.push_back({
     {position.x(), position.y(), position.z()},
     {normal.x(), normal.y(), normal.z()},
     {uv.x(), uv.y()}
    });
   }
   if (renderData.normals.size() != renderData.positions.size()) {
    for (size_t i = 0; i + 2 < vertices.size(); i += 3) {
     const QVector3D p0(
         vertices[i].position[0], vertices[i].position[1], vertices[i].position[2]);
     const QVector3D p1(
         vertices[i + 1].position[0], vertices[i + 1].position[1], vertices[i + 1].position[2]);
     const QVector3D p2(
         vertices[i + 2].position[0], vertices[i + 2].position[1], vertices[i + 2].position[2]);
     const QVector3D faceNormal =
         QVector3D::crossProduct(p1 - p0, p2 - p0).normalized();
     for (size_t corner = 0; corner < 3; ++corner) {
      vertices[i + corner].normal[0] = faceNormal.x();
      vertices[i + corner].normal[1] = faceNormal.y();
      vertices[i + corner].normal[2] = faceNormal.z();
     }
    }
   }
   indices.reserve(renderData.indices.size());
   for (const auto index : renderData.indices) {
    indices.push_back(static_cast<Uint32>(index));
   }
  }

  if (vertices.empty()) {
   vertices = makeFallbackCubeVertices();
  }

  solidVertexCount_ = static_cast<Uint32>(vertices.size());
  solidIndexCount_ = static_cast<Uint32>(indices.size());
  if (!pDevice || !pImmediateContext || solidVertexCount_ == 0) {
   return;
  }

  const Uint64 requiredSize = sizeof(SolidViewportVertex) * static_cast<Uint64>(vertices.size());
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
  }

  if (solidIndexCount_ > 0) {
   const Uint64 requiredIndexSize =
       sizeof(Uint32) * static_cast<Uint64>(indices.size());
   if (!solidIndexBuffer_ ||
       solidIndexBuffer_->GetDesc().Size != requiredIndexSize) {
    BufferDesc indexDesc;
    indexDesc.Name = "ArtifactSolidViewportIB";
    indexDesc.Usage = USAGE_DYNAMIC;
    indexDesc.BindFlags = BIND_INDEX_BUFFER;
    indexDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    indexDesc.Size = requiredIndexSize;
    pDevice->CreateBuffer(indexDesc, nullptr, &solidIndexBuffer_);
   }
   if (solidIndexBuffer_) {
    mappedData = nullptr;
    pImmediateContext->MapBuffer(
        solidIndexBuffer_, MAP_WRITE, MAP_FLAG_DISCARD, mappedData);
    if (mappedData) {
     std::memcpy(mappedData, indices.data(), requiredIndexSize);
     pImmediateContext->UnmapBuffer(solidIndexBuffer_, MAP_WRITE);
    }
   }
  } else {
   solidIndexBuffer_ = nullptr;
  }
  meshDirty_ = false;
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
  updateBaseColorTexture();
  updateMetallicRoughnessTexture();
  updateNormalTexture();
  if (meshDirty_) {
      uploadMeshGeometry();
  }

  auto* pRTV = pSwapChain->GetCurrentBackBufferRTV();
  auto* pDSV = pSwapChain->GetDepthBufferDSV();

  pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  float r = static_cast<float>(clearColor_.redF());
  float g = static_cast<float>(clearColor_.greenF());
  float b = static_cast<float>(clearColor_.blueF());
  float a = static_cast<float>(clearColor_.alphaF());
  if (shadingMode_ == ShadingMode::Wireframe) {
   r = (std::min)(1.0f, r + 0.10f);
   g = (std::min)(1.0f, g + 0.10f);
   b = (std::min)(1.0f, b + 0.10f);
  } else if (shadingMode_ == ShadingMode::SolidWithWire) {
   b = (std::min)(1.0f, b + 0.12f);
  }
  const float ClearColor[] = { r, g, b, a };
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
      QMatrix4x4 view;
      view.setToIdentity();
      view.translate(0.0f, 0.0f, -previewDistance_);
      view.rotate(previewPitch_, 1.0f, 0.0f, 0.0f);
      view.rotate(previewYaw_, 0.0f, 1.0f, 0.0f);
      view.translate(-previewTarget_.x(), -previewTarget_.y(), -previewTarget_.z());

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

      const float yawRadians = qDegreesToRadians(previewYaw_);
      const float pitchRadians = qDegreesToRadians(previewPitch_);
      const float cosPitch = std::cos(pitchRadians);
      const QVector3D cameraPosition =
          previewTarget_ +
          QVector3D(
              -std::sin(yawRadians) * cosPitch,
              std::sin(pitchRadians),
              std::cos(yawRadians) * cosPitch) *
              previewDistance_;
      const SolidViewportMaterial material{
          {
              static_cast<float>(materialBaseColor_.redF()),
              static_cast<float>(materialBaseColor_.greenF()),
              static_cast<float>(materialBaseColor_.blueF()),
              static_cast<float>(materialBaseColor_.alphaF())
          },
          {
              cameraPosition.x(),
              cameraPosition.y(),
              cameraPosition.z(),
              metallicRoughnessTexturePath_.isEmpty()
                  ? materialMetallic_
                  : 1.0f
          },
          {
              0.45f,
              0.75f,
              0.55f,
              metallicRoughnessTexturePath_.isEmpty()
                  ? materialRoughness_
                  : 1.0f
          }
      };
      pImmediateContext->MapBuffer(solidColorBuffer_, MAP_WRITE, MAP_FLAG_DISCARD, mappedData);
      if (mappedData) {
          std::memcpy(mappedData, &material, sizeof(material));
          pImmediateContext->UnmapBuffer(solidColorBuffer_, MAP_WRITE);
      }

      auto* pPSO = (shadingMode_ == ShadingMode::Wireframe) ? wirePso_.RawPtr() : solidPso_.RawPtr();
      auto* pSRB = (shadingMode_ == ShadingMode::Wireframe) ? wireSrb_.RawPtr() : solidSrb_.RawPtr();
      if (shadingMode_ == ShadingMode::SolidWithWire && solidPso_ && wirePso_) {
          pPSO = solidPso_.RawPtr();
          pSRB = solidSrb_.RawPtr();
      }

      pImmediateContext->SetPipelineState(pPSO);
      if (auto* textureVariable =
              pSRB->GetVariableByName(
                  SHADER_TYPE_PIXEL, "BaseColorTexture")) {
          textureVariable->Set(baseColorTextureSrv_);
      }
      if (auto* textureVariable =
              pSRB->GetVariableByName(
                  SHADER_TYPE_PIXEL, "MetallicRoughnessTexture")) {
          textureVariable->Set(metallicRoughnessTextureSrv_);
      }
      if (auto* textureVariable =
              pSRB->GetVariableByName(
                  SHADER_TYPE_PIXEL, "NormalTexture")) {
          textureVariable->Set(normalTextureSrv_);
      }
      pImmediateContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

       Uint64 offsets[] = { 0 };
       IBuffer* vertexBuffers[] = { solidVertexBuffer_.RawPtr() };
       pImmediateContext->SetVertexBuffers(0, 1, vertexBuffers, offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);

      if (solidIndexBuffer_ && solidIndexCount_ > 0) {
          pImmediateContext->SetIndexBuffer(
              solidIndexBuffer_, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
          DrawIndexedAttribs drawAttrs;
          drawAttrs.NumIndices = solidIndexCount_;
          drawAttrs.IndexType = VT_UINT32;
          drawAttrs.Flags = DRAW_FLAG_NONE;
          pImmediateContext->DrawIndexed(drawAttrs);
      } else {
          DrawAttribs drawAttrs;
          drawAttrs.NumVertices = solidVertexCount_;
          drawAttrs.Flags = DRAW_FLAG_NONE;
          pImmediateContext->Draw(drawAttrs);
      }

      if (shadingMode_ == ShadingMode::SolidWithWire && wirePso_ && wireSrb_) {
           pImmediateContext->SetPipelineState(wirePso_);
           if (auto* textureVariable =
                   wireSrb_->GetVariableByName(
                       SHADER_TYPE_PIXEL, "BaseColorTexture")) {
               textureVariable->Set(baseColorTextureSrv_);
           }
           if (auto* textureVariable =
                   wireSrb_->GetVariableByName(
                       SHADER_TYPE_PIXEL, "MetallicRoughnessTexture")) {
               textureVariable->Set(metallicRoughnessTextureSrv_);
           }
           if (auto* textureVariable =
                   wireSrb_->GetVariableByName(
                       SHADER_TYPE_PIXEL, "NormalTexture")) {
               textureVariable->Set(normalTextureSrv_);
           }
           pImmediateContext->CommitShaderResources(wireSrb_, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
           pImmediateContext->SetVertexBuffers(0, 1, vertexBuffers, offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
           if (solidIndexBuffer_ && solidIndexCount_ > 0) {
               pImmediateContext->SetIndexBuffer(
                   solidIndexBuffer_, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
               DrawIndexedAttribs wireDrawAttrs;
               wireDrawAttrs.NumIndices = solidIndexCount_;
               wireDrawAttrs.IndexType = VT_UINT32;
               wireDrawAttrs.Flags = DRAW_FLAG_NONE;
               pImmediateContext->DrawIndexed(wireDrawAttrs);
           } else {
               DrawAttribs wireDrawAttrs;
               wireDrawAttrs.NumVertices = solidVertexCount_;
               wireDrawAttrs.Flags = DRAW_FLAG_NONE;
               pImmediateContext->Draw(wireDrawAttrs);
           }
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
