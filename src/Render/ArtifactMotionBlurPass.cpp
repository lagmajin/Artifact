module;
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>

module Artifact.Render.MotionBlurPass;
import Graphics.GPUcomputeContext;
import Graphics.Compute;

namespace Artifact {
using namespace Diligent;

namespace {
struct MotionBlurParams {
  float shutterAngle;
  float shutterPhase;
  float velocityScale;
  unsigned sampleCount;
  unsigned width;
  unsigned height;
  unsigned shutterShape;
  float trapezoidPlateau;
  unsigned hasCandidateMap = 0;
  unsigned candidateTileSize = 8;
  unsigned padding[2]{};
};

struct MotionBlurCandidateParams {
  unsigned width;
  unsigned height;
  unsigned stride;
  unsigned tileSize;
  float shutterStart;
  float shutterEnd;
  float velocityScale;
  unsigned padding = 0;
};

constexpr const char* kMotionBlurShader = R"(
cbuffer MotionBlurParams : register(b0) {
  float shutterAngle;
  float shutterPhase;
  float velocityScale;
  uint sampleCount;
  uint width;
  uint height;
  uint shutterShape;
  float trapezoidPlateau;
  uint hasCandidateMap;
  uint candidateTileSize;
  uint2 padding;
};
Texture2D<float4> InputColor : register(t0);
Texture2D<float4> InputVelocity : register(t1);
Texture2D<float> InputDepth : register(t2);
Texture2D<uint4> InputCandidate : register(t3);
RWTexture2D<float4> OutputColor : register(u0);

float pointSegmentDistanceSquared(float2 point, float2 start, float2 end) {
  const float2 segment = end - start;
  const float segmentLengthSquared = dot(segment, segment);
  const float t = segmentLengthSquared > 0.000001
      ? saturate(dot(point - start, segment) / segmentLengthSquared)
      : 0.0;
  const float2 delta = point - (start + segment * t);
  return dot(delta, delta);
}

[numthreads(8, 8, 1)]
void MotionBlurCS(uint3 id : SV_DispatchThreadID) {
  if (id.x >= width || id.y >= height) return;
  const int2 pixel = int2(id.xy);
  const uint count = clamp(sampleCount, 1u, 32u);
  // A zero-angle, zero-phase aperture is an exact current-frame copy. A
  // nonzero phase still samples a different instant even when count is one.
  if (shutterAngle <= 0.0 && abs(shutterPhase) < 0.000001) {
    OutputColor[id.xy] = InputColor.Load(int3(pixel, 0));
    return;
  }
  // The renderer stores screen-space NDC deltas as [0, 1] AOV values. Decode
  // them to signed UV/frame deltas, then convert to this dispatch's pixel grid.
  const float4 velocityCoverage = InputVelocity.Load(int3(pixel, 0));
  const float2 encodedVelocity = velocityCoverage.xy;
  float2 velocity = (encodedVelocity * 2.0 - 1.0) *
      float2(width, height) * velocityScale;
  bool exteriorTrail = false;
  float sourceDepth = 0.0;
  if (hasCandidateMap != 0 && velocityCoverage.a < 0.5) {
    const uint4 sourceIndices = InputCandidate.Load(int3(
        pixel / int(candidateTileSize), 0));
    float bestCandidateDistance = 1.0e30;
    float bestCandidateDepth = 1.0;
    [unroll]
    for (uint candidateSlot = 0; candidateSlot < 4; ++candidateSlot) {
      const uint sourceIndex = sourceIndices[candidateSlot];
      if (sourceIndex == 0xffffffff) continue;
      const int2 sourcePixel = int2(sourceIndex % width, sourceIndex / width);
      const float4 sourceVelocityCoverage =
          InputVelocity.Load(int3(sourcePixel, 0));
      const float2 sourceVelocity =
          (sourceVelocityCoverage.xy * 2.0 - 1.0) *
          float2(width, height) * velocityScale;
      sourceDepth = InputDepth.Load(int3(sourcePixel, 0));
      const float halfShutter = max(shutterAngle, 0.0) / 720.0;
      const float2 sourceCenter = float2(sourcePixel);
      const float2 trajectoryStart = sourceCenter +
          sourceVelocity * (shutterPhase - halfShutter);
      const float2 trajectoryEnd = sourceCenter +
          sourceVelocity * (shutterPhase + halfShutter);
      const float trajectoryDistanceSquared = pointSegmentDistanceSquared(
          float2(pixel), trajectoryStart, trajectoryEnd);
      if (trajectoryDistanceSquared < bestCandidateDistance - 0.0001 ||
          (abs(trajectoryDistanceSquared - bestCandidateDistance) <= 0.0001 &&
           sourceDepth < bestCandidateDepth)) {
        bestCandidateDistance = trajectoryDistanceSquared;
        bestCandidateDepth = sourceDepth;
        velocity = sourceVelocity;
      }
    }
    const float destinationDepth = InputDepth.Load(int3(pixel, 0));
    exteriorTrail = bestCandidateDistance <= 0.75 * 0.75 &&
        bestCandidateDepth <= destinationDepth + 0.02;
    sourceDepth = bestCandidateDepth;
  }
  if (!exteriorTrail && (length(velocity) < 0.0001 ||
      (count <= 1u && abs(shutterPhase) < 0.000001))) {
    OutputColor[id.xy] = InputColor.Load(int3(pixel, 0));
    return;
  }
  const float shutter = max(shutterAngle, 0.0) / 360.0;
  const float centerDepth = exteriorTrail
      ? sourceDepth : InputDepth.Load(int3(pixel, 0));
  const float4 center = InputColor.Load(int3(pixel, 0));
  float4 sum = 0.0;
  float weight = 0.0;
  float totalWeight = 0.0;
  [loop]
  for (uint i = 0; i < count; ++i) {
    const float shutterPosition = ((float(i) + 0.5) / float(count)) * 2.0 - 1.0;
    const float t = shutterPosition * (shutter * 0.5) + shutterPhase;
    float profileWeight = 1.0;
    if (shutterShape == 1u) {
      profileWeight = max(1.0 - abs(shutterPosition), 0.0);
    } else if (shutterShape == 2u) {
      const float plateau = clamp(trapezoidPlateau, 0.0, 0.9999);
      const float ramp = max(1.0 - plateau, 0.0001);
      profileWeight = 1.0 - saturate((abs(shutterPosition) - plateau) / ramp);
    }
    const int2 samplePixel = clamp(
        int2(round(float2(pixel) - velocity * t)), int2(0, 0),
        int2(width - 1, height - 1));
    totalWeight += profileWeight;
    const float sampleDepth = InputDepth.Load(int3(samplePixel, 0));
    if (exteriorTrail) {
      const float4 tap = abs(sampleDepth - centerDepth) < 0.02
          ? InputColor.Load(int3(samplePixel, 0)) : center;
      sum += tap * profileWeight;
      weight += profileWeight;
    } else {
      const float depthWeight = abs(sampleDepth - centerDepth) < 0.02
          ? profileWeight : 0.0;
      sum += InputColor.Load(int3(samplePixel, 0)) * depthWeight;
      weight += depthWeight;
    }
  }
  OutputColor[id.xy] = weight > 0.0
      ? sum / (exteriorTrail ? max(totalWeight, 0.000001) : weight)
      : center;
}
)";

constexpr const char* kMotionBlurCandidateSeedShader = R"(
cbuffer MotionBlurCandidateParams : register(b0) {
  uint width;
  uint height;
  uint stride;
  uint tileSize;
  float shutterStart;
  float shutterEnd;
  float velocityScale;
  uint padding;
};
Texture2D<float4> InputVelocity : register(t0);
Texture2D<float> InputDepth : register(t1);
RWTexture2D<uint4> OutputCandidate : register(u0);
[numthreads(8, 8, 1)]
void MotionBlurCandidateSeedCS(uint3 id : SV_DispatchThreadID) {
  const uint candidateWidth = (width + tileSize - 1) / tileSize;
  const uint candidateHeight = (height + tileSize - 1) / tileSize;
  if (id.x >= candidateWidth || id.y >= candidateHeight) return;
  uint4 bestIndex = 0xffffffff;
  float4 bestScore = 0.0;
  float4 bestDepth = 1.0;
  [loop]
  for (uint localY = 0; localY < tileSize; ++localY) {
    [loop]
    for (uint localX = 0; localX < tileSize; ++localX) {
      const uint2 pixel = id.xy * tileSize + uint2(localX, localY);
      if (pixel.x >= width || pixel.y >= height) continue;
      const float4 encoded = InputVelocity.Load(int3(pixel, 0));
      if (encoded.a < 0.5) continue;
      const float2 velocity = (encoded.xy * 2.0 - 1.0) *
          float2(width, height) * velocityScale;
      const float score = dot(velocity, velocity) *
          max(abs(shutterStart), abs(shutterEnd));
      if (score <= 0.000001) continue;
      const uint quadrant = (velocity.x < 0.0 ? 1u : 0u) +
          (velocity.y < 0.0 ? 2u : 0u);
      const float sourceDepth = InputDepth.Load(int3(pixel, 0));
      if (score > bestScore[quadrant] + 0.0001 ||
          (abs(score - bestScore[quadrant]) <= 0.0001 &&
           sourceDepth < bestDepth[quadrant])) {
        bestIndex[quadrant] = pixel.y * width + pixel.x;
        bestScore[quadrant] = score;
        bestDepth[quadrant] = sourceDepth;
      }
    }
  }
  OutputCandidate[id.xy] = bestIndex;
}
)";

constexpr const char* kMotionBlurCandidateFloodShader = R"(
cbuffer MotionBlurCandidateParams : register(b0) {
  uint width;
  uint height;
  uint stride;
  uint tileSize;
  float shutterStart;
  float shutterEnd;
  float velocityScale;
  uint padding;
};
Texture2D<float4> InputVelocity : register(t0);
Texture2D<float> InputDepth : register(t1);
Texture2D<uint4> InputCandidate : register(t2);
RWTexture2D<uint4> OutputCandidate : register(u0);

float candidateDistanceSquared(uint sourceIndex, float2 destination) {
  const int2 sourcePixel = int2(sourceIndex % width, sourceIndex / width);
  const float4 encoded = InputVelocity.Load(int3(sourcePixel, 0));
  if (encoded.a < 0.5) return 1.0e30;
  const float2 velocity = (encoded.xy * 2.0 - 1.0) *
      float2(width, height) * velocityScale;
  const float2 source = float2(sourcePixel);
  const float2 start = source + velocity * shutterStart;
  const float2 end = source + velocity * shutterEnd;
  const float2 segment = end - start;
  const float lengthSquared = dot(segment, segment);
  const float t = lengthSquared > 0.000001
      ? saturate(dot(destination - start, segment) / lengthSquared)
      : 0.0;
  const float2 delta = destination - (start + segment * t);
  return dot(delta, delta);
}

[numthreads(8, 8, 1)]
void MotionBlurCandidateFloodCS(uint3 id : SV_DispatchThreadID) {
  const uint candidateWidth = (width + tileSize - 1) / tileSize;
  const uint candidateHeight = (height + tileSize - 1) / tileSize;
  if (id.x >= candidateWidth || id.y >= candidateHeight) return;
  const int2 pixel = int2(id.xy);
  const float2 destination = min(
      (float2(pixel) + 0.5) * float(tileSize),
      float2(width - 1, height - 1));
  uint4 bestIndex = 0xffffffff;
  float4 bestDistance = 1.0e30;
  float4 bestDepth = 1.0;
  [unroll]
  for (int y = -1; y <= 1; ++y) {
    [unroll]
    for (int x = -1; x <= 1; ++x) {
      const int2 neighbor = pixel + int2(x, y) * int(stride);
      if (any(neighbor < 0) || neighbor.x >= int(candidateWidth) ||
          neighbor.y >= int(candidateHeight)) continue;
      const uint4 candidates = InputCandidate.Load(int3(neighbor, 0));
      [unroll]
      for (uint candidateSlot = 0; candidateSlot < 4; ++candidateSlot) {
        const uint candidate = candidates[candidateSlot];
        if (candidate == 0xffffffff) continue;
        const float distanceSquared = candidateDistanceSquared(
            candidate, destination);
        const int2 sourcePixel = int2(candidate % width, candidate / width);
        const float sourceDepth = InputDepth.Load(int3(sourcePixel, 0));
        const float4 encoded = InputVelocity.Load(int3(sourcePixel, 0));
        const float2 velocity = (encoded.xy * 2.0 - 1.0) *
            float2(width, height) * velocityScale;
        const uint quadrant = (velocity.x < 0.0 ? 1u : 0u) +
            (velocity.y < 0.0 ? 2u : 0u);
        if (distanceSquared < bestDistance[quadrant] - 0.0001 ||
            (abs(distanceSquared - bestDistance[quadrant]) <= 0.0001 &&
             sourceDepth < bestDepth[quadrant])) {
          bestIndex[quadrant] = candidate;
          bestDistance[quadrant] = distanceSquared;
          bestDepth[quadrant] = sourceDepth;
        }
      }
    }
  }
  OutputCandidate[id.xy] = bestIndex;
}
)";
}

struct MotionBlurPass::Impl {
  ArtifactCore::GpuContext* gpuContext = nullptr;
  std::unique_ptr<ArtifactCore::ComputeExecutor> executor;
  std::unique_ptr<ArtifactCore::ComputeExecutor> candidateSeedExecutor;
  std::unique_ptr<ArtifactCore::ComputeExecutor> candidateFloodExecutor;
  Diligent::RefCntAutoPtr<Diligent::IBuffer> params;
  Diligent::RefCntAutoPtr<Diligent::IBuffer> candidateParams;
  Diligent::RefCntAutoPtr<Diligent::ITexture> dummyCandidateTexture;
  Diligent::RefCntAutoPtr<Diligent::ITextureView> dummyCandidateSRV;
  Diligent::IRenderDevice* device = nullptr;
  Diligent::IDeviceContext* context = nullptr;
  bool ready = false;
};

MotionBlurPass::MotionBlurPass() : impl_(new Impl()) {}
MotionBlurPass::~MotionBlurPass() {
  if (impl_) {
    impl_->executor.reset();
    impl_->candidateSeedExecutor.reset();
    impl_->candidateFloodExecutor.reset();
    delete impl_->gpuContext;
    impl_->gpuContext = nullptr;
    delete impl_;
  }
}

bool MotionBlurPass::initialize(IRenderDevice* device, IDeviceContext* context) {
  if (!device || !context) return false;
  if (impl_->ready && impl_->device == device && impl_->context == context) {
    return true;
  }
  auto gpuContext = std::make_unique<ArtifactCore::GpuContext>(device, context);
  auto executor = std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext);
  BufferDesc desc;
  desc.Name = "Motion Blur Params";
  desc.Usage = USAGE_DYNAMIC;
  desc.Size = sizeof(MotionBlurParams);
  desc.BindFlags = BIND_UNIFORM_BUFFER;
  desc.CPUAccessFlags = CPU_ACCESS_WRITE;
  Diligent::RefCntAutoPtr<Diligent::IBuffer> params;
  device->CreateBuffer(desc, nullptr, &params);
  if (!params) {
    return false;
  }
  BufferDesc candidateParamsDesc;
  candidateParamsDesc.Name = "Motion Blur Candidate Params";
  candidateParamsDesc.Usage = USAGE_DYNAMIC;
  candidateParamsDesc.Size = sizeof(MotionBlurCandidateParams);
  candidateParamsDesc.BindFlags = BIND_UNIFORM_BUFFER;
  candidateParamsDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
  Diligent::RefCntAutoPtr<Diligent::IBuffer> candidateParams;
  device->CreateBuffer(candidateParamsDesc, nullptr, &candidateParams);
  if (!candidateParams) return false;

  TextureDesc dummyCandidateDesc;
  dummyCandidateDesc.Name = "Motion Blur Dummy Candidate Index";
  dummyCandidateDesc.Type = RESOURCE_DIM_TEX_2D;
  dummyCandidateDesc.Width = 1;
  dummyCandidateDesc.Height = 1;
  dummyCandidateDesc.Format = TEX_FORMAT_RGBA32_UINT;
  dummyCandidateDesc.MipLevels = 1;
  dummyCandidateDesc.ArraySize = 1;
  dummyCandidateDesc.SampleCount = 1;
  dummyCandidateDesc.Usage = USAGE_DEFAULT;
  dummyCandidateDesc.BindFlags = BIND_SHADER_RESOURCE;
  Diligent::RefCntAutoPtr<Diligent::ITexture> dummyCandidateTexture;
  device->CreateTexture(dummyCandidateDesc, nullptr, &dummyCandidateTexture);
  if (!dummyCandidateTexture) return false;
  auto* dummyCandidateSRV = dummyCandidateTexture->GetDefaultView(
      TEXTURE_VIEW_SHADER_RESOURCE);
  if (!dummyCandidateSRV) return false;

  static const ShaderResourceVariableDesc variables[] = {
      {SHADER_TYPE_COMPUTE, "MotionBlurParams", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "InputColor", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "InputVelocity", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "InputDepth", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "InputCandidate", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "OutputColor", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };
  ArtifactCore::ComputePipelineDesc pipeline;
  pipeline.name = "Artifact Motion Blur Pass";
  pipeline.shaderSource = kMotionBlurShader;
  pipeline.entryPoint = "MotionBlurCS";
  pipeline.variables = variables;
  pipeline.variableCount = static_cast<Uint32>(sizeof(variables) / sizeof(variables[0]));
  pipeline.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
  if (!executor->build(pipeline) ||
      !executor->createShaderResourceBinding(true)) {
    return false;
  }

  static const ShaderResourceVariableDesc seedVariables[] = {
      {SHADER_TYPE_COMPUTE, "MotionBlurCandidateParams", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "InputVelocity", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "InputDepth", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "OutputCandidate", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };
  auto candidateSeedExecutor =
      std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext);
  ArtifactCore::ComputePipelineDesc seedPipeline;
  seedPipeline.name = "Artifact Motion Blur Candidate Seed";
  seedPipeline.shaderSource = kMotionBlurCandidateSeedShader;
  seedPipeline.entryPoint = "MotionBlurCandidateSeedCS";
  seedPipeline.variables = seedVariables;
  seedPipeline.variableCount = static_cast<Uint32>(
      sizeof(seedVariables) / sizeof(seedVariables[0]));
  seedPipeline.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
  if (!candidateSeedExecutor->build(seedPipeline) ||
      !candidateSeedExecutor->createShaderResourceBinding(true)) {
    return false;
  }

  static const ShaderResourceVariableDesc floodVariables[] = {
      {SHADER_TYPE_COMPUTE, "MotionBlurCandidateParams", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "InputVelocity", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "InputDepth", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "InputCandidate", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "OutputCandidate", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };
  auto candidateFloodExecutor =
      std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext);
  ArtifactCore::ComputePipelineDesc floodPipeline;
  floodPipeline.name = "Artifact Motion Blur Candidate Jump Flood";
  floodPipeline.shaderSource = kMotionBlurCandidateFloodShader;
  floodPipeline.entryPoint = "MotionBlurCandidateFloodCS";
  floodPipeline.variables = floodVariables;
  floodPipeline.variableCount = static_cast<Uint32>(
      sizeof(floodVariables) / sizeof(floodVariables[0]));
  floodPipeline.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
  if (!candidateFloodExecutor->build(floodPipeline) ||
      !candidateFloodExecutor->createShaderResourceBinding(true)) {
    return false;
  }

  impl_->executor.reset();
  impl_->candidateSeedExecutor.reset();
  impl_->candidateFloodExecutor.reset();
  delete impl_->gpuContext;
  impl_->gpuContext = gpuContext.release();
  impl_->executor = std::move(executor);
  impl_->candidateSeedExecutor = std::move(candidateSeedExecutor);
  impl_->candidateFloodExecutor = std::move(candidateFloodExecutor);
  impl_->params = std::move(params);
  impl_->candidateParams = std::move(candidateParams);
  impl_->dummyCandidateTexture = std::move(dummyCandidateTexture);
  impl_->dummyCandidateSRV = dummyCandidateSRV;
  impl_->device = device;
  impl_->context = context;
  impl_->ready = true;
  return true;
}

bool MotionBlurPass::apply(IDeviceContext* context, ITextureView* color,
                           ITextureView* velocity, ITextureView* depth,
                           ITextureView* output, unsigned width, unsigned height,
                           const MotionBlurSettings& settings) {
  return applyInternal(context, color, velocity, depth, output, nullptr,
                       width, height, settings);
}

bool MotionBlurPass::applyWithCandidates(
    IDeviceContext* context, ITextureView* color, ITextureView* velocity,
    ITextureView* depth, ITextureView* output,
    ITextureView* candidateIndex0SRV, ITextureView* candidateIndex0UAV,
    ITextureView* candidateIndex1SRV, ITextureView* candidateIndex1UAV,
    unsigned width, unsigned height, const MotionBlurSettings& settings) {
  if (!candidateIndex0SRV || !candidateIndex0UAV || !candidateIndex1SRV ||
      !candidateIndex1UAV || width == 0 || height == 0) {
    return false;
  }
  const float shutterAngle = std::isfinite(settings.shutterAngle)
      ? std::max(settings.shutterAngle, 0.0f) : 180.0f;
  const float shutterPhase = std::isfinite(settings.shutterPhase)
      ? settings.shutterPhase : 0.0f;
  const unsigned sampleCount = std::clamp(settings.sampleCount, 1u, 32u);
  const bool needsMotion = settings.enabled &&
      (std::abs(shutterPhase) > 0.000001f ||
       (shutterAngle > 0.0f && sampleCount > 1u)) &&
      (!std::isfinite(settings.velocityScale) ||
       std::abs(settings.velocityScale) > 0.000001f);
  if (!needsMotion) {
    return applyInternal(context, color, velocity, depth, output, nullptr,
                         width, height, settings);
  }
  auto* candidateMap = buildCandidateMap(
      context, velocity, depth, candidateIndex0SRV, candidateIndex0UAV,
      candidateIndex1SRV, candidateIndex1UAV, width, height, settings);
  return candidateMap && applyInternal(context, color, velocity, depth, output,
                                       candidateMap, width, height, settings);
}

ITextureView* MotionBlurPass::buildCandidateMap(
    IDeviceContext* context, ITextureView* velocity, ITextureView* depth,
    ITextureView* candidateIndex0SRV, ITextureView* candidateIndex0UAV,
    ITextureView* candidateIndex1SRV, ITextureView* candidateIndex1UAV,
    unsigned width, unsigned height, const MotionBlurSettings& settings) {
  if (!impl_->ready || !context || context != impl_->context || !velocity ||
      !depth || !candidateIndex0SRV || !candidateIndex0UAV ||
      !candidateIndex1SRV || !candidateIndex1UAV || width == 0 || height == 0 ||
      width > 0x7fffffffu || height > 0x7fffffffu ||
      static_cast<std::uint64_t>(width) * height >= 0xffffffffull ||
      !impl_->candidateSeedExecutor || !impl_->candidateFloodExecutor ||
      !impl_->candidateParams) {
    return nullptr;
  }
  const float shutterAngle = std::isfinite(settings.shutterAngle)
      ? std::max(settings.shutterAngle, 0.0f)
      : 180.0f;
  const float shutterPhase = std::isfinite(settings.shutterPhase)
      ? settings.shutterPhase : 0.0f;
  const float velocityScale = std::isfinite(settings.velocityScale)
      ? settings.velocityScale : 1.0f;
  const float halfShutter = shutterAngle / 720.0f;
  constexpr unsigned candidateTileSize = 8;
  const unsigned candidateWidth =
      (width + candidateTileSize - 1) / candidateTileSize;
  const unsigned candidateHeight =
      (height + candidateTileSize - 1) / candidateTileSize;
  MotionBlurCandidateParams candidateParams{
      width, height, 0, candidateTileSize, shutterPhase - halfShutter,
      shutterPhase + halfShutter, velocityScale, 0};
  void* mapped = nullptr;
  context->MapBuffer(impl_->candidateParams, MAP_WRITE, MAP_FLAG_DISCARD,
                     mapped);
  if (!mapped) return nullptr;
  std::memcpy(mapped, &candidateParams, sizeof(candidateParams));
  context->UnmapBuffer(impl_->candidateParams, MAP_WRITE);

  auto& seed = *impl_->candidateSeedExecutor;
  if (!seed.setBuffer("MotionBlurCandidateParams", impl_->candidateParams) ||
      !seed.setTextureView("InputVelocity", velocity) ||
      !seed.setTextureView("InputDepth", depth) ||
      !seed.setTextureView("OutputCandidate", candidateIndex0UAV)) {
    return nullptr;
  }
  seed.dispatch(context, ArtifactCore::ComputeExecutor::makeDispatchAttribs(
      candidateWidth, candidateHeight, 1, 8, 8, 1));

  unsigned maxDimension = std::max(candidateWidth, candidateHeight);
  unsigned stride = 1;
  while (stride < maxDimension && stride <= 0x7fffffffu) stride <<= 1;
  stride >>= 1;
  unsigned inputIndex = 0;
  while (stride > 0) {
    candidateParams.stride = stride;
    mapped = nullptr;
    context->MapBuffer(impl_->candidateParams, MAP_WRITE, MAP_FLAG_DISCARD,
                       mapped);
    if (!mapped) return nullptr;
    std::memcpy(mapped, &candidateParams, sizeof(candidateParams));
    context->UnmapBuffer(impl_->candidateParams, MAP_WRITE);

    auto& flood = *impl_->candidateFloodExecutor;
    ITextureView* inputSRV = inputIndex == 0
        ? candidateIndex0SRV : candidateIndex1SRV;
    ITextureView* outputUAV = inputIndex == 0
        ? candidateIndex1UAV : candidateIndex0UAV;
    if (!flood.setBuffer("MotionBlurCandidateParams", impl_->candidateParams) ||
        !flood.setTextureView("InputVelocity", velocity) ||
        !flood.setTextureView("InputDepth", depth) ||
        !flood.setTextureView("InputCandidate", inputSRV) ||
        !flood.setTextureView("OutputCandidate", outputUAV)) {
      return nullptr;
    }
    flood.dispatch(context,
        ArtifactCore::ComputeExecutor::makeDispatchAttribs(
            candidateWidth, candidateHeight, 1, 8, 8, 1));
    inputIndex = 1u - inputIndex;
    stride >>= 1;
  }
  return inputIndex == 0 ? candidateIndex0SRV : candidateIndex1SRV;
}

bool MotionBlurPass::applyInternal(
    IDeviceContext* context, ITextureView* color, ITextureView* velocity,
    ITextureView* depth, ITextureView* output, ITextureView* candidateMap,
    unsigned width, unsigned height, const MotionBlurSettings& settings) {
  if (!impl_->ready || !context || context != impl_->context || !color ||
      !velocity || !depth || !output ||
      width == 0 || height == 0 || !settings.enabled) return false;
  const float shutterAngle = std::isfinite(settings.shutterAngle)
      ? std::max(settings.shutterAngle, 0.0f)
      : 180.0f;
  const float shutterPhase = std::isfinite(settings.shutterPhase)
      ? settings.shutterPhase
      : 0.0f;
  const float velocityScale = std::isfinite(settings.velocityScale)
      ? settings.velocityScale
      : 1.0f;
  const float trapezoidPlateau = std::isfinite(settings.trapezoidPlateau)
      ? std::clamp(settings.trapezoidPlateau, 0.0f, 0.9999f)
      : 0.5f;
  const unsigned shutterShape =
      static_cast<unsigned>(settings.shutterShape) <=
              static_cast<unsigned>(MotionBlurShutterShape::Trapezoid)
          ? static_cast<unsigned>(settings.shutterShape)
          : static_cast<unsigned>(MotionBlurShutterShape::Rectangular);
  MotionBlurParams params{shutterAngle, shutterPhase, velocityScale,
                          std::clamp(settings.sampleCount, 1u, 32u), width, height,
                          shutterShape, trapezoidPlateau,
                          candidateMap ? 1u : 0u, 8u, {0, 0}};
  void* mapped = nullptr;
  context->MapBuffer(impl_->params, MAP_WRITE, MAP_FLAG_DISCARD, mapped);
  if (!mapped) return false;
  std::memcpy(mapped, &params, sizeof(params));
  context->UnmapBuffer(impl_->params, MAP_WRITE);
  auto& exec = *impl_->executor;
  if (!exec.setBuffer("MotionBlurParams", impl_->params) ||
      !exec.setTextureView("InputColor", color) ||
      !exec.setTextureView("InputVelocity", velocity) ||
      !exec.setTextureView("InputDepth", depth) ||
      !exec.setTextureView("InputCandidate",
                           candidateMap ? candidateMap : impl_->dummyCandidateSRV) ||
      !exec.setTextureView("OutputColor", output)) return false;
  exec.dispatch(context, ArtifactCore::ComputeExecutor::makeDispatchAttribs(
      width, height, 1, 8, 8, 1));
  return true;
}

bool MotionBlurPass::ready() const { return impl_ && impl_->ready; }
}
