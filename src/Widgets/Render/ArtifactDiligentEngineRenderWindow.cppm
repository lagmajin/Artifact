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
#include <QMouseEvent>
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
import Artifact.Render.Config;
import IO.ImageImporter;
import Image.Raw;
import Mesh;
import Artifact.Render.DiligentDeviceManager;

namespace {
 bool isFiniteMatrix(const QMatrix4x4& matrix)
 {
  const float* values = matrix.constData();
  for (int i = 0; i < 16; ++i) {
   if (!std::isfinite(values[i])) return false;
  }
  return true;
 }

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

 QVector<QVector<quint8>> buildViewportMipChain(
     const QVector<quint8>& base, Uint32 width, Uint32 height,
     bool srgb, bool renormalizeNormal)
 {
  QVector<QVector<quint8>> levels;
  if (base.isEmpty() || width == 0 || height == 0) {
   return levels;
  }
  levels.push_back(base);
  Uint32 levelWidth = width;
  Uint32 levelHeight = height;
  while (levelWidth > 1 || levelHeight > 1) {
   const Uint32 nextWidth = std::max(1u, levelWidth / 2u);
   const Uint32 nextHeight = std::max(1u, levelHeight / 2u);
   QVector<quint8> next(static_cast<qsizetype>(nextWidth) * nextHeight * 4);
   const auto& previous = levels.back();
   auto toLinear = [srgb](float value) {
    value /= 255.0f;
    return srgb ? (value <= 0.04045f ? value / 12.92f
                                    : std::pow((value + 0.055f) / 1.055f, 2.4f))
                : value;
   };
   auto toByte = [srgb](float value) {
    value = std::clamp(value, 0.0f, 1.0f);
    if (srgb) {
     value = value <= 0.0031308f ? value * 12.92f
                                 : 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
    }
    return static_cast<quint8>(std::lround(value * 255.0f));
   };
   for (Uint32 y = 0; y < nextHeight; ++y) {
    for (Uint32 x = 0; x < nextWidth; ++x) {
     float sum[4] = {};
     int count = 0;
     for (Uint32 oy = 0; oy < 2; ++oy) {
      const Uint32 sourceY = (std::min)(levelHeight - 1, y * 2 + oy);
      for (Uint32 ox = 0; ox < 2; ++ox) {
       const Uint32 sourceX = (std::min)(levelWidth - 1, x * 2 + ox);
       const qsizetype index = (static_cast<qsizetype>(sourceY) * levelWidth + sourceX) * 4;
       sum[0] += toLinear(previous[index + 0]);
       sum[1] += toLinear(previous[index + 1]);
       sum[2] += toLinear(previous[index + 2]);
       sum[3] += previous[index + 3] / 255.0f;
       ++count;
      }
     }
     float r = sum[0] / count;
     float g = sum[1] / count;
     float b = sum[2] / count;
     if (renormalizeNormal) {
      r = r * 2.0f - 1.0f;
      g = g * 2.0f - 1.0f;
      b = b * 2.0f - 1.0f;
      const float length = std::sqrt(r * r + g * g + b * b);
      if (length > 1.0e-5f) {
       r = r / length * 0.5f + 0.5f;
       g = g / length * 0.5f + 0.5f;
       b = b / length * 0.5f + 0.5f;
      }
     }
     const qsizetype output = (static_cast<qsizetype>(y) * nextWidth + x) * 4;
     next[output + 0] = toByte(r);
     next[output + 1] = toByte(g);
     next[output + 2] = toByte(b);
     next[output + 3] = static_cast<quint8>(std::lround(std::clamp(sum[3] / count, 0.0f, 1.0f) * 255.0f));
    }
   }
   levels.push_back(std::move(next));
   levelWidth = nextWidth;
   levelHeight = nextHeight;
  }
  return levels;
 }

 const char* kSolidViewportVS = R"(
cbuffer TransformCB : register(b0)
{
    float4x4 WorldMatrix;
    float4x4 ViewMatrix;
    float4x4 ProjMatrix;
};

cbuffer SkinningCB : register(b1)
{
    float4x4 BoneMatrices[128];
};

struct VSInput
{
    float3 position : ATTRIB0;
    float3 normal : ATTRIB1;
    float2 uv : ATTRIB2;
    float4 boneIndices : ATTRIB3;
    float4 boneWeights : ATTRIB4;
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
    float3 skinnedPosition = input.position;
    float3 skinnedNormal = input.normal;
    float totalWeight = 0.0f;
    float3 weightedPosition = 0.0f;
    float3 weightedNormal = 0.0f;
    for (int influence = 0; influence < 4; ++influence)
    {
        const float weight = input.boneWeights[influence];
        const float index = input.boneIndices[influence];
        if (weight > 0.0f && weight < 1.0e20f &&
            index >= 0.0f && index < 128.0f &&
            frac(index) == 0.0f)
        {
            weightedPosition += mul(BoneMatrices[(int)index],
                                    float4(input.position, 1.0f)).xyz * weight;
            const float3x3 boneMatrix = (float3x3)BoneMatrices[(int)index];
            const float boneDeterminant = determinant(boneMatrix);
            if (abs(boneDeterminant) > 1.0e-8f)
            {
                const float3x3 boneNormalMatrix = transpose(
                    inverse(boneMatrix));
                weightedNormal += mul(boneNormalMatrix, input.normal) * weight;
            }
            else
            {
                weightedNormal += input.normal * weight;
            }
            totalWeight += weight;
        }
    }
    if (totalWeight > 0.0f && totalWeight < 1.0e20f)
    {
        skinnedPosition = weightedPosition / totalWeight;
        const float weightedNormalLength = length(weightedNormal);
        skinnedNormal = weightedNormalLength > 1.0e-8f
            ? weightedNormal / weightedNormalLength
            : input.normal;
    }
    float4 worldPos = mul(WorldMatrix, float4(skinnedPosition, 1.0f));
    float4 viewPos = mul(ViewMatrix, worldPos);
    output.position = mul(ProjMatrix, viewPos);
    output.worldPosition = worldPos.xyz;
    const float3x3 worldMatrix3x3 = (float3x3)WorldMatrix;
    const float worldDeterminant = determinant(worldMatrix3x3);
    const float3 transformedNormal = abs(worldDeterminant) > 1.0e-8f
        ? mul(transpose(inverse(worldMatrix3x3)), skinnedNormal)
        : skinnedNormal;
    const float transformedNormalLength = length(transformedNormal);
    output.normal = transformedNormalLength > 1.0e-8f
        ? transformedNormal / transformedNormalLength
        : float3(0.0f, 0.0f, 1.0f);
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
    float4 PrincipledFactors;
    float4 OpticalFactors;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPosition : TEXCOORD0;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD1;
};

float3 srgbToLinear(float3 value)
{
    return float3(
        value.r <= 0.04045 ? value.r / 12.92
                           : pow((value.r + 0.055) / 1.055, 2.4),
        value.g <= 0.04045 ? value.g / 12.92
                           : pow((value.g + 0.055) / 1.055, 2.4),
        value.b <= 0.04045 ? value.b / 12.92
                           : pow((value.b + 0.055) / 1.055, 2.4));
}

float4 main(PSInput input) : SV_TARGET
{
    const float pi = 3.14159265;
    float4 baseSample = BaseColorTexture.Sample(BaseColorSampler, input.uv);
    float4 sampledBaseColor = float4(
        baseSample.rgb * srgbToLinear(saturate(BaseColor.rgb)),
        baseSample.a * BaseColor.a);
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
    float transmissionWeight = saturate(PrincipledFactors.w);
    float sheenWeight = saturate(PrincipledFactors.x) *
                        (1.0 - materialMetallic);
    float coatWeight = saturate(PrincipledFactors.y);
    float coatRoughness = clamp(PrincipledFactors.z, 0.04, 1.0);
    float3 L = normalize(LightDirectionAndRoughness.xyz);
    float3 V =
        normalize(CameraPositionAndMetallic.xyz - input.worldPosition);
    float3 H = normalize(L + V);
    float NdotL = saturate(dot(N, L));
    float NdotV = max(saturate(dot(N, V)), 0.001);
    float NdotH = saturate(dot(N, H));
    float VdotH = saturate(dot(V, H));

    float alpha = max(materialRoughness * materialRoughness, 1e-4);
    float alpha2 = alpha * alpha;
    float denom = NdotH * NdotH * (alpha2 - 1.0) + 1.0;
    float distribution = alpha2 / max(pi * denom * denom, 0.001);
    float k =
        ((materialRoughness + 1.0) * (materialRoughness + 1.0)) * 0.125;
    float geometryV = NdotV / max(NdotV * (1.0 - k) + k, 0.001);
    float geometryL = NdotL / max(NdotL * (1.0 - k) + k, 0.001);
    float opticalIor = max(OpticalFactors.y, 1.0);
    float dielectricF0 = pow((opticalIor - 1.0) / (opticalIor + 1.0), 2.0);
    float3 f0 = lerp(float3(dielectricF0, dielectricF0, dielectricF0),
                     sampledBaseColor.rgb, materialMetallic);
    f0 *= lerp(1.0, saturate(OpticalFactors.x), 1.0 - materialMetallic);
    float3 fresnel = f0 + (1.0 - f0) * pow(1.0 - VdotH, 5.0);
    float3 specular = distribution * geometryV * geometryL * fresnel /
                      max(4.0 * NdotV * max(NdotL, 0.001), 0.001);
    float coatAlpha = coatRoughness * coatRoughness;
    float coatAlpha2 = coatAlpha * coatAlpha;
    float coatDenom = NdotH * NdotH * (coatAlpha2 - 1.0) + 1.0;
    float coatDistribution = coatAlpha2 /
        max(pi * coatDenom * coatDenom, 0.001);
    float coatFresnel = 0.04 + 0.96 * pow(1.0 - VdotH, 5.0);
    float3 coatSpecular = coatDistribution * geometryV * geometryL *
        coatFresnel / max(4.0 * NdotV * max(NdotL, 0.001), 0.001);
    float3 diffuse =
        (1.0 - transmissionWeight) * (1.0 - fresnel) * (1.0 - materialMetallic) *
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
    float3 T = refract(-V, N, 1.0 / max(opticalIor, 1.0));
    float transmissionDirectionValid = step(1e-4, dot(T, T));
    float transmissionEnvironmentWeight = saturate(T.y * 0.5 + 0.5);
    float3 transmissionEnvironment = lerp(
        float3(0.035, 0.030, 0.028),
        float3(0.28, 0.36, 0.48),
        transmissionEnvironmentWeight);
    transmissionEnvironment *= sampledBaseColor.rgb *
        (1.0 - materialMetallic) * (1.0 - materialRoughness * 0.65);
    float3 ambientDiffuse =
        diffuseEnvironment * sampledBaseColor.rgb *
        (1.0 - materialMetallic) * (1.0 - fresnel);
    float3 ambientSpecular = specularEnvironment * fresnel;
    float coatViewFresnel = 0.04 + 0.96 * pow(1.0 - saturate(dot(N, V)), 5.0);
    float coatEnergy = 1.0 - coatWeight * coatViewFresnel;
    ambientDiffuse *= coatEnergy;
    ambientSpecular *= coatEnergy;
    float3 sheen = sheenWeight * pow(1.0 - NdotV, 5.0) *
                   diffuseEnvironment;
    float3 coatEnvironment = specularEnvironment *
        coatViewFresnel;
    float transmissionF0 = pow((opticalIor - 1.0) / (opticalIor + 1.0), 2.0);
    float transmissionFresnel = transmissionF0 +
        (1.0 - transmissionF0) * pow(1.0 - NdotV, 5.0);
    float3 transmissionFallback = transmissionWeight *
        transmissionEnvironment * (1.0 - transmissionFresnel) *
        (0.55 + 0.45 * (1.0 - NdotL)) * transmissionDirectionValid;
    float3 color = ambientDiffuse * (1.0 - transmissionWeight) + ambientSpecular +
        coatEnvironment * coatWeight +
        (diffuse * coatEnergy + specular * coatEnergy +
         coatSpecular * coatWeight) * NdotL * 2.2 + sheen +
        transmissionFallback;
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
  float boneIndices[4];
  float boneWeights[4];
 };

 struct SolidViewportMaterial
 {
  float baseColor[4];
  float cameraPositionAndMetallic[4];
 float lightDirectionAndRoughness[4];
 float principledFactors[4];
  float opticalFactors[4];
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
     , {-1.0f, -1.0f, -1.0f, -1.0f}
     , {0.0f, 0.0f, 0.0f, 0.0f}
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
 solidSkinningBuffer_ = nullptr;
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
  swapChainDesc.ColorBufferFormat =
      Artifact::RenderConfig::hdrDisplayEnabled()
          ? TEX_FORMAT_RGBA16_FLOAT
          : TEX_FORMAT_RGBA8_UNORM_SRGB;
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

 void ArtifactDiligentEngineRenderWindow::setMesh(ArtifactCore::SharedPtr<ArtifactCore::Mesh> mesh)
 {
 mesh_ = std::move(mesh);
   skinPoseMatrices_.clear();
   gpuSkinningActive_ = false;
   skinPoseDirty_ = true;
   if (mesh_ && !mesh_->skinBones().isEmpty()) {
       const QVector<QMatrix4x4> initialPose = mesh_->skinPoseMatrices();
       bool finitePose = initialPose.size() >= mesh_->skinBones().size();
       for (const QMatrix4x4& matrix : initialPose) {
           finitePose = finitePose && isFiniteMatrix(matrix);
       }
       const auto sourceMethod = mesh_->skinningMethod();
       const bool gpuCompatibleSkinning =
           sourceMethod == ArtifactCore::Mesh::SkinningMethod::LinearBlend;
       // The current GPU vertex input carries four influences; extended
       // packed weights must stay on the CPU deformer path.
       const bool hasExtendedWeights = mesh_->hasExtendedSkinningWeights();
       if (mesh_->skinBones().size() <= 128 && finitePose &&
           gpuCompatibleSkinning && !hasExtendedWeights) {
           // Model loading evaluates the initial pose on the CPU so that the
           // software path has usable geometry. Restore the source/morph
           // result before the GPU applies the same pose in the vertex shader.
           mesh_->restoreSkinningBase();
           skinPoseMatrices_ = initialPose;
           gpuSkinningActive_ = true;
       } else {
           // The preview SkinningCB is intentionally bounded. Preserve
           // correctness for larger rigs through the existing CPU LBS path.
           mesh_->applyDeformers(mesh_->skinPoseMatrices());
       }
   }
   meshDirty_ = true;
   skinPoseDirty_ = true;
   requestRender();
 }

 void ArtifactDiligentEngineRenderWindow::setSkinPoseMatrices(
     const QVector<QMatrix4x4>& boneMatrices)
 {
  if (!mesh_ || boneMatrices.isEmpty() || mesh_->skinBones().isEmpty()) {
   return;
  }
  const bool completePose = boneMatrices.size() >= mesh_->skinBones().size();
  bool finitePose = completePose;
  for (const QMatrix4x4& matrix : boneMatrices) {
      finitePose = finitePose && isFiniteMatrix(matrix);
  }
  const bool wasGpuSkinningActive = gpuSkinningActive_;
  bool cpuFallback = false;
  const auto sourceMethod = mesh_->skinningMethod();
  const bool gpuCompatibleSkinning =
      sourceMethod == ArtifactCore::Mesh::SkinningMethod::LinearBlend;
  const bool hasExtendedWeights = mesh_->hasExtendedSkinningWeights();
  if (boneMatrices.size() <= 128 && finitePose && gpuCompatibleSkinning &&
      !hasExtendedWeights) {
      if (!wasGpuSkinningActive) {
          // CPU fallback may have left the mesh deformed. Restore bind space
          // before the GPU shader applies the pose again.
          mesh_->restoreSkinningBase();
      }
      skinPoseMatrices_ = boneMatrices;
      gpuSkinningActive_ = true;
  } else {
      skinPoseMatrices_.clear();
      gpuSkinningActive_ = false;
      mesh_->applyDeformers(boneMatrices);
      cpuFallback = true;
  }
  skinPoseDirty_ = true;
  if (cpuFallback || wasGpuSkinningActive != gpuSkinningActive_) {
      meshDirty_ = true;
  }
  requestRender();
 }

 void ArtifactDiligentEngineRenderWindow::refreshMeshGeometry()
 {
  if (!mesh_) return;
  if (gpuSkinningActive_) {
      // Mesh deformer setters evaluate CPU LBS so the software path remains
      // usable. GPU uploads must contain the Morph result before skinning,
      // otherwise the vertex shader would apply the pose a second time.
      mesh_->restoreSkinningBase();
  }
  meshDirty_ = true;
  requestRender();
 }

 bool ArtifactDiligentEngineRenderWindow::gpuSkinningActive() const
 {
  return gpuSkinningActive_;
 }

 void ArtifactDiligentEngineRenderWindow::clearMesh()
 {
 mesh_.reset();
  skinPoseMatrices_.clear();
  gpuSkinningActive_ = false;
  skinPoseDirty_ = true;
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
    float roughness,
    float sheen,
    float clearcoat,
    float clearcoatRoughness,
    float transmission,
    float specular,
    float ior)
{
  materialBaseColor_ = baseColor;
  materialMetallic_ = std::clamp(metallic, 0.0f, 1.0f);
  materialRoughness_ = std::clamp(roughness, 0.04f, 1.0f);
  materialSheen_ = std::clamp(sheen, 0.0f, 1.0f);
  materialClearcoat_ = std::clamp(clearcoat, 0.0f, 1.0f);
  materialClearcoatRoughness_ = std::clamp(clearcoatRoughness, 0.04f, 1.0f);
  materialTransmission_ = std::clamp(transmission, 0.0f, 1.0f);
  materialSpecular_ = std::clamp(specular, 0.0f, 1.0f);
  materialIor_ = std::clamp(ior, 1.0f, 3.0f);
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

  BufferDesc skinningDesc;
  skinningDesc.Name = "ArtifactSolidViewportSkinningCB";
  skinningDesc.Usage = USAGE_DYNAMIC;
  skinningDesc.BindFlags = BIND_UNIFORM_BUFFER;
  skinningDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
  skinningDesc.Size = sizeof(float) * 16 * 128;
  pDevice->CreateBuffer(skinningDesc, nullptr, &solidSkinningBuffer_);

  auto createPSO = [&](FILL_MODE fillMode, RefCntAutoPtr<IPipelineState>& outPSO, RefCntAutoPtr<IShaderResourceBinding>& outSRB) {
   GraphicsPipelineStateCreateInfo psoCI;
   psoCI.PSODesc.Name = (fillMode == FILL_MODE_WIREFRAME) ? "ArtifactSolidViewportWirePSO" : "ArtifactSolidViewportPSO";
   psoCI.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
   psoCI.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

   auto& gp = psoCI.GraphicsPipeline;
   gp.NumRenderTargets = 1;
   gp.RTVFormats[0] = Artifact::RenderConfig::hdrDisplayEnabled()
                          ? TEX_FORMAT_RGBA16_FLOAT
                          : TEX_FORMAT_RGBA8_UNORM_SRGB;
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
    LayoutElement{3, 0, 4, VT_FLOAT32, False, LAYOUT_ELEMENT_AUTO_OFFSET, sizeof(SolidViewportVertex)},
    LayoutElement{4, 0, 4, VT_FLOAT32, False, LAYOUT_ELEMENT_AUTO_OFFSET, sizeof(SolidViewportVertex)},
   };
   gp.InputLayout.LayoutElements = layout;
   gp.InputLayout.NumElements = 5;
   psoCI.pVS = vs;
   psoCI.pPS = ps;

   ShaderResourceVariableDesc vars[] = {
    {SHADER_TYPE_VERTEX, "TransformCB", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
    {SHADER_TYPE_VERTEX, "SkinningCB", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
    {SHADER_TYPE_PIXEL, "ColorCB", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
    {SHADER_TYPE_PIXEL, "BaseColorTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
    {SHADER_TYPE_PIXEL, "MetallicRoughnessTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
    {SHADER_TYPE_PIXEL, "NormalTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
    {SHADER_TYPE_PIXEL, "BaseColorSampler", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
   };
   psoCI.PSODesc.ResourceLayout.Variables = vars;
   psoCI.PSODesc.ResourceLayout.NumVariables = 7;

   pDevice->CreateGraphicsPipelineState(psoCI, &outPSO);
   if (outPSO) {
    outPSO->CreateShaderResourceBinding(&outSRB, true);
     outPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "TransformCB")->Set(solidTransformBuffer_);
     outPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "SkinningCB")->Set(solidSkinningBuffer_);
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
                         static_cast<bool>(solidTransformBuffer_) &&
                         static_cast<bool>(solidColorBuffer_) &&
                         static_cast<bool>(solidSkinningBuffer_);
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
  const auto mipChain = buildViewportMipChain(rgba8, width, height, true, false);
  textureDesc.MipLevels = static_cast<Uint32>(mipChain.size());
  textureDesc.Format = TEX_FORMAT_RGBA8_UNORM_SRGB;
  textureDesc.Usage = USAGE_IMMUTABLE;
  textureDesc.BindFlags = BIND_SHADER_RESOURCE;

  std::vector<TextureSubResData> subresources;
  subresources.reserve(static_cast<size_t>(mipChain.size()));
  Uint32 mipWidth = width;
  for (const auto& mip : mipChain) {
   subresources.push_back({mip.constData(), static_cast<Uint64>(mipWidth) * 4u});
   mipWidth = std::max(1u, mipWidth / 2u);
  }
  TextureData textureData;
  textureData.pSubResources = subresources.data();
  textureData.NumSubresources = static_cast<Uint32>(subresources.size());
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
  const auto mipChain = buildViewportMipChain(rgba8, width, height, false, false);
  textureDesc.MipLevels = static_cast<Uint32>(mipChain.size());
  textureDesc.Format = TEX_FORMAT_RGBA8_UNORM;
  textureDesc.Usage = USAGE_IMMUTABLE;
  textureDesc.BindFlags = BIND_SHADER_RESOURCE;

  std::vector<TextureSubResData> subresources;
  subresources.reserve(static_cast<size_t>(mipChain.size()));
  Uint32 mipWidth = width;
  for (const auto& mip : mipChain) {
   subresources.push_back({mip.constData(), static_cast<Uint64>(mipWidth) * 4u});
   mipWidth = std::max(1u, mipWidth / 2u);
  }
  TextureData textureData;
  textureData.pSubResources = subresources.data();
  textureData.NumSubresources = static_cast<Uint32>(subresources.size());
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
  const auto mipChain = buildViewportMipChain(rgba8, width, height, false, true);
  textureDesc.MipLevels = static_cast<Uint32>(mipChain.size());
  textureDesc.Format = TEX_FORMAT_RGBA8_UNORM;
  textureDesc.Usage = USAGE_IMMUTABLE;
  textureDesc.BindFlags = BIND_SHADER_RESOURCE;

  std::vector<TextureSubResData> subresources;
  subresources.reserve(static_cast<size_t>(mipChain.size()));
  Uint32 mipWidth = width;
  for (const auto& mip : mipChain) {
   subresources.push_back({mip.constData(), static_cast<Uint64>(mipWidth) * 4u});
   mipWidth = std::max(1u, mipWidth / 2u);
  }
  TextureData textureData;
  textureData.pSubResources = subresources.data();
  textureData.NumSubresources = static_cast<Uint32>(subresources.size());
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
   const auto boneIndicesAttr =
       mesh_->vertexAttributes().get<QVector4D>("boneIndices");
   const auto boneWeightsAttr =
       mesh_->vertexAttributes().get<QVector4D>("boneWeights");
   vertices.reserve(renderData.positions.size());
   for (qsizetype i = 0; i < renderData.positions.size(); ++i) {
    const QVector3D& position = renderData.positions[i];
    const QVector3D normal =
        i < renderData.normals.size() ? renderData.normals[i].normalized()
                                      : QVector3D(0.0f, 0.0f, 1.0f);
    const QVector2D uv =
        i < renderData.uvs.size() ? renderData.uvs[i] : QVector2D();
    const QVector4D boneIndices = gpuSkinningActive_ && boneIndicesAttr &&
        i < boneIndicesAttr->size()
        ? (*boneIndicesAttr)[static_cast<int>(i)]
        : QVector4D(-1.0f, -1.0f, -1.0f, -1.0f);
    const QVector4D boneWeights = gpuSkinningActive_ && boneWeightsAttr &&
        i < boneWeightsAttr->size()
        ? (*boneWeightsAttr)[static_cast<int>(i)]
        : QVector4D(0.0f, 0.0f, 0.0f, 0.0f);
    vertices.push_back({
     {position.x(), position.y(), position.z()},
     {normal.x(), normal.y(), normal.z()},
     {uv.x(), uv.y()},
     {boneIndices.x(), boneIndices.y(), boneIndices.z(), boneIndices.w()},
     {boneWeights.x(), boneWeights.y(), boneWeights.z(), boneWeights.w()}
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
  const int viewportWidth = std::max(1, width());
  const int viewportHeight = std::max(1, height());
  const float aspect = static_cast<float>(viewportWidth) /
                       static_cast<float>(viewportHeight);
  const float ndcX = 2.0f * static_cast<float>(posx) / viewportWidth - 1.0f;
  const float ndcY = 1.0f - 2.0f * static_cast<float>(posy) / viewportHeight;

  QMatrix4x4 view;
  view.setToIdentity();
  view.translate(0.0f, 0.0f, -previewDistance_);
  view.rotate(previewPitch_, 1.0f, 0.0f, 0.0f);
  view.rotate(previewYaw_, 0.0f, 1.0f, 0.0f);
  view.translate(-previewTarget_.x(), -previewTarget_.y(), -previewTarget_.z());
  QMatrix4x4 projection;
  projection.setToIdentity();
  projection.perspective(45.0f, aspect, 0.1f, 100.0f);

  bool invertible = false;
  const QMatrix4x4 inverse = (projection * view).inverted(&invertible);
  if (!invertible) return;
  const QVector4D nearPoint = inverse * QVector4D(ndcX, ndcY, -1.0f, 1.0f);
  const QVector4D farPoint = inverse * QVector4D(ndcX, ndcY, 1.0f, 1.0f);
  if (std::abs(nearPoint.w()) < 1.0e-6f ||
      std::abs(farPoint.w()) < 1.0e-6f) return;
  const QVector3D nearWorld = nearPoint.toVector3D() / nearPoint.w();
  const QVector3D farWorld = farPoint.toVector3D() / farPoint.w();
  const QVector3D direction = farWorld - nearWorld;
  if (!std::isfinite(direction.x()) || !std::isfinite(direction.y()) ||
      !std::isfinite(direction.z()) || direction.lengthSquared() < 1.0e-8f) {
   return;
  }
  pickingRayOrigin_ = nearWorld;
  pickingRayDirection_ = direction.normalized();
 }

 QVector3D ArtifactDiligentEngineRenderWindow::pickingRayOrigin() const
 {
  return pickingRayOrigin_;
 }

 QVector3D ArtifactDiligentEngineRenderWindow::pickingRayDirection() const
 {
  return pickingRayDirection_;
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

  if (solidPso_ && solidVertexBuffer_ && solidTransformBuffer_ &&
      solidColorBuffer_ && solidSkinningBuffer_ && solidVertexCount_ > 0) {
      const auto scDesc = pSwapChain->GetDesc();
      const float aspect = (scDesc.Height > 0) ? static_cast<float>(scDesc.Width) / static_cast<float>(scDesc.Height) : 1.0f;

      QMatrix4x4 world;
      world.setToIdentity();
      if (mesh_) {
          QVector3D minBound = mesh_->boundingBoxMin();
          QVector3D maxBound = mesh_->boundingBoxMax();
          if (gpuSkinningActive_ && !skinPoseMatrices_.isEmpty()) {
              const QVector3D corners[8] = {
                  {minBound.x(), minBound.y(), minBound.z()},
                  {maxBound.x(), minBound.y(), minBound.z()},
                  {minBound.x(), maxBound.y(), minBound.z()},
                  {maxBound.x(), maxBound.y(), minBound.z()},
                  {minBound.x(), minBound.y(), maxBound.z()},
                  {maxBound.x(), minBound.y(), maxBound.z()},
                  {minBound.x(), maxBound.y(), maxBound.z()},
                  {maxBound.x(), maxBound.y(), maxBound.z()}
              };
              QVector3D skinnedMin = minBound;
              QVector3D skinnedMax = maxBound;
              // Vertices without valid influences remain in bind space.
              for (const QMatrix4x4& boneMatrix : skinPoseMatrices_) {
                  for (const QVector3D& corner : corners) {
                      const QVector3D transformed = boneMatrix.map(corner);
                      if (!std::isfinite(transformed.x()) ||
                          !std::isfinite(transformed.y()) ||
                          !std::isfinite(transformed.z())) {
                          continue;
                      }
                      skinnedMin.setX((std::min)(skinnedMin.x(), transformed.x()));
                      skinnedMin.setY((std::min)(skinnedMin.y(), transformed.y()));
                      skinnedMin.setZ((std::min)(skinnedMin.z(), transformed.z()));
                      skinnedMax.setX((std::max)(skinnedMax.x(), transformed.x()));
                      skinnedMax.setY((std::max)(skinnedMax.y(), transformed.y()));
                      skinnedMax.setZ((std::max)(skinnedMax.z(), transformed.z()));
                  }
              }
              minBound = skinnedMin;
              maxBound = skinnedMax;
          }
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

      if (skinPoseDirty_ && gpuSkinningActive_) {
      struct SkinningCB {
          float boneMatrices[128][16];
      } skinning{};
      for (int i = 0; i < 128; ++i) {
          skinning.boneMatrices[i][0] = 1.0f;
          skinning.boneMatrices[i][5] = 1.0f;
          skinning.boneMatrices[i][10] = 1.0f;
          skinning.boneMatrices[i][15] = 1.0f;
      }
      const int skinCount = std::min<int>(
          static_cast<int>(skinPoseMatrices_.size()), 128);
      for (int i = 0; i < skinCount; ++i) {
          std::memcpy(skinning.boneMatrices[i],
                      skinPoseMatrices_[i].constData(),
                      sizeof(skinning.boneMatrices[i]));
      }
      bool skinPoseUploaded = false;
      pImmediateContext->MapBuffer(
          solidSkinningBuffer_, MAP_WRITE, MAP_FLAG_DISCARD, mappedData);
      if (mappedData) {
          std::memcpy(mappedData, &skinning, sizeof(skinning));
          pImmediateContext->UnmapBuffer(solidSkinningBuffer_, MAP_WRITE);
          skinPoseUploaded = true;
      }
      if (skinPoseUploaded) {
          skinPoseDirty_ = false;
      }
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
          },
          {
              materialSheen_,
              materialClearcoat_,
              materialClearcoatRoughness_,
              materialTransmission_
          },
          {
              materialSpecular_,
              materialIor_,
              0.0f,
              0.0f
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
  if (!event) return;
  QWindow::keyPressEvent(event);
 }

 void ArtifactDiligentEngineRenderWindow::mousePressEvent(QMouseEvent* event)
 {
  if (!event) return;
  pickingRay(static_cast<int>(event->position().x()),
             static_cast<int>(event->position().y()));
  QWindow::mousePressEvent(event);
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
  if (!event) return;
  QWidget::keyPressEvent(event);
 }

 void DiligentViewportWidget::resizeEvent(QResizeEvent* event)
 {
  if (!event) return;
  QWidget::resizeEvent(event);
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
