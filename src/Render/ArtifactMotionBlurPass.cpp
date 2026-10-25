module;
#include <algorithm>
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
  unsigned padding[2]{};
};

constexpr const char* kMotionBlurShader = R"(
cbuffer MotionBlurParams : register(b0) {
  float shutterAngle;
  float shutterPhase;
  float velocityScale;
  uint sampleCount;
  uint width;
  uint height;
};
Texture2D<float4> InputColor : register(t0);
Texture2D<float2> InputVelocity : register(t1);
Texture2D<float> InputDepth : register(t2);
RWTexture2D<float4> OutputColor : register(u0);

[numthreads(8, 8, 1)]
void MotionBlurCS(uint3 id : SV_DispatchThreadID) {
  if (id.x >= width || id.y >= height) return;
  const int2 pixel = int2(id.xy);
  const float2 velocity = InputVelocity.Load(int3(pixel, 0)) * velocityScale;
  const uint count = clamp(sampleCount, 1u, 32u);
  if (count <= 1u || length(velocity) < 0.0001) {
    OutputColor[id.xy] = InputColor.Load(int3(pixel, 0));
    return;
  }
  const float shutter = max(shutterAngle, 0.0) / 360.0;
  float4 sum = 0.0;
  float weight = 0.0;
  [loop]
  for (uint i = 0; i < count; ++i) {
    const float t = ((float(i) + 0.5) / float(count) - 0.5) * shutter + shutterPhase;
    const int2 samplePixel = clamp(
        int2(round(float2(pixel) - velocity * t)), int2(0, 0),
        int2(width - 1, height - 1));
    const float sampleDepth = InputDepth.Load(int3(samplePixel, 0));
    const float centerDepth = InputDepth.Load(int3(pixel, 0));
    const float depthWeight = abs(sampleDepth - centerDepth) < 0.02 ? 1.0 : 0.0;
    sum += InputColor.Load(int3(samplePixel, 0)) * depthWeight;
    weight += depthWeight;
  }
  const float4 center = InputColor.Load(int3(pixel, 0));
  OutputColor[id.xy] = weight > 0.0 ? sum / weight : center;
}
)";
}

struct MotionBlurPass::Impl {
  std::unique_ptr<ArtifactCore::GpuContext> gpuContext;
  std::unique_ptr<ArtifactCore::ComputeExecutor> executor;
  Diligent::RefCntAutoPtr<Diligent::IBuffer> params;
  Diligent::IRenderDevice* device = nullptr;
  bool ready = false;
};

MotionBlurPass::MotionBlurPass() : impl_(new Impl()) {}
MotionBlurPass::~MotionBlurPass() { delete impl_; }

bool MotionBlurPass::initialize(IRenderDevice* device, IDeviceContext* context) {
  if (!device || !context) return false;
  impl_->device = device;
  impl_->gpuContext = std::make_unique<ArtifactCore::GpuContext>(device, context);
  impl_->executor = std::make_unique<ArtifactCore::ComputeExecutor>(*impl_->gpuContext);
  BufferDesc desc;
  desc.Name = "Motion Blur Params";
  desc.Usage = USAGE_DYNAMIC;
  desc.Size = sizeof(MotionBlurParams);
  desc.BindFlags = BIND_UNIFORM_BUFFER;
  desc.CPUAccessFlags = CPU_ACCESS_WRITE;
  device->CreateBuffer(desc, nullptr, &impl_->params);
  static const ShaderResourceVariableDesc variables[] = {
      {SHADER_TYPE_COMPUTE, "MotionBlurParams", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "InputColor", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "InputVelocity", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "InputDepth", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "OutputColor", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };
  ArtifactCore::ComputePipelineDesc pipeline;
  pipeline.name = "Artifact Motion Blur Pass";
  pipeline.shaderSource = kMotionBlurShader;
  pipeline.entryPoint = "MotionBlurCS";
  pipeline.variables = variables;
  pipeline.variableCount = static_cast<Uint32>(sizeof(variables) / sizeof(variables[0]));
  pipeline.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
  impl_->ready = impl_->params && impl_->executor->build(pipeline) &&
                 impl_->executor->createShaderResourceBinding(true);
  return impl_->ready;
}

bool MotionBlurPass::apply(IDeviceContext* context, ITextureView* color,
                           ITextureView* velocity, ITextureView* depth,
                           ITextureView* output, unsigned width, unsigned height,
                           const MotionBlurSettings& settings) {
  if (!impl_->ready || !context || !color || !velocity || !depth || !output ||
      width == 0 || height == 0 || !settings.enabled) return false;
  MotionBlurParams params{settings.shutterAngle, settings.shutterPhase,
                          settings.velocityScale,
                          std::clamp(settings.sampleCount, 1u, 32u), width, height};
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
      !exec.setTextureView("OutputColor", output)) return false;
  exec.dispatch(context, ArtifactCore::ComputeExecutor::makeDispatchAttribs(
      width, height, 1, 8, 8, 1));
  return true;
}

bool MotionBlurPass::ready() const { return impl_ && impl_->ready; }
}
