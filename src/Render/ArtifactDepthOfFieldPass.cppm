module;
#include <algorithm>
#include <cstring>
#include <memory>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>

module Artifact.Render.DepthOfFieldPass;
import Graphics.GPUcomputeContext;
import Graphics.Compute;

namespace Artifact {
using namespace Diligent;

namespace {
struct DepthOfFieldParams {
  float focusDistance;
  float nearClip;
  float farClip;
  float maxCocRadius;
  float cocScale;
  unsigned sampleCount;
  unsigned width;
  unsigned height;
  // Thin-lens physical model (active when usePhysical != 0).
  float focalLengthMm;
  float apertureScale;
  unsigned usePhysical;
  unsigned padding{};
};

constexpr const char* kDepthOfFieldShader = R"(
cbuffer DepthOfFieldParams : register(b0) {
  float focusDistance;   // view-space units
  float zNear;
  float zFar;
  float maxCocRadius;    // pixels at full defocus
  float cocScale;        // authored blur amount normalization (0..1)
  uint sampleCount;
  uint width;
  uint height;
  float focalLengthMm;   // 35mm-equivalent lens
  float apertureScale;   // authored aperture interpreted as f-stop scale
  uint usePhysical;      // thin-lens model active when nonzero
};
Texture2D<float4> InputColor : register(t0);
// Same non-linear [0,1] window depth the SSGI pass consumes.
Texture2D<float> InputDepth : register(t1);
RWTexture2D<float4> OutputColor : register(u0);

float viewDepth(float2 pixel) {
  const float d = InputDepth.Load(int3(int2(pixel), 0));
  return zNear * zFar / max(zFar - d * (zFar - zNear), 0.0001);
}

float circleOfConfusion(float depth) {
  if (maxCocRadius <= 0.0 || cocScale <= 0.0) return 0.0;
  const float coc = usePhysical != 0
      // Thin-lens: blur grows with distance from the focal plane and with the
      // aperture scale. The denominator keeps in-focus pixels sharp and
      // clamps gracefully behind the lens plane.
      ? abs(apertureScale * focalLengthMm * (focusDistance - depth)) /
        max(abs(focusDistance * (depth - max(focalLengthMm, 1.0))), 1.0)
      // Symmetric linear ramp reaching full radius at +/- focusDistance.
      : abs(depth - focusDistance) / max(focusDistance, 1.0);
  return saturate(coc * cocScale) * maxCocRadius;
}

[numthreads(8, 8, 1)]
void DepthOfFieldCS(uint3 id : SV_DispatchThreadID) {
  if (id.x >= width || id.y >= height) return;
  const int2 pixel = int2(id.xy);
  const float centerDepth = viewDepth(float2(pixel));
  const float radius = circleOfConfusion(centerDepth);
  const float4 center = InputColor.Load(int3(pixel, 0));
  if (radius < 0.5) {
    OutputColor[id.xy] = center;
    return;
  }
  const uint count = clamp(sampleCount, 4u, 64u);
  // Golden-angle spiral sampling.
  const float goldenAngle = 2.39996323;
  float4 sum = 0.0;
  float weight = 0.0;
  [loop]
  for (uint i = 0; i < count; ++i) {
    const float t = (float(i) + 0.5) / float(count);
    const float r = sqrt(t) * radius;
    const float a = float(i) * goldenAngle;
    const int2 samplePixel = clamp(
        int2(round(float2(pixel) + float2(cos(a), sin(a)) * r)),
        int2(0, 0), int2(width - 1, height - 1));
    const float sampleDepth = viewDepth(float2(samplePixel));
    const float sampleRadius = circleOfConfusion(sampleDepth);
    // Reject foreground samples bleeding over in-focus background.
    const float depthWeight = sampleDepth >= centerDepth
        ? saturate(sampleRadius / max(radius, 0.5))
        : 1.0;
    sum += InputColor.Load(int3(samplePixel, 0)) * depthWeight;
    weight += depthWeight;
  }
  OutputColor[id.xy] = weight > 0.0 ? sum / weight : center;
}
)";
}

struct DepthOfFieldPass::Impl {
  std::unique_ptr<ArtifactCore::GpuContext> gpuContext;
  std::unique_ptr<ArtifactCore::ComputeExecutor> executor;
  Diligent::RefCntAutoPtr<Diligent::IBuffer> params;
  Diligent::IRenderDevice* device = nullptr;
  bool ready = false;
};

DepthOfFieldPass::DepthOfFieldPass() : impl_(new Impl()) {}
DepthOfFieldPass::~DepthOfFieldPass() { delete impl_; }

bool DepthOfFieldPass::initialize(IRenderDevice* device, IDeviceContext* context) {
  if (!device || !context) return false;
  impl_->device = device;
  impl_->gpuContext = std::make_unique<ArtifactCore::GpuContext>(device, context);
  impl_->executor = std::make_unique<ArtifactCore::ComputeExecutor>(*impl_->gpuContext);
  BufferDesc desc;
  desc.Name = "Depth Of Field Params";
  desc.Usage = USAGE_DYNAMIC;
  desc.Size = sizeof(DepthOfFieldParams);
  desc.BindFlags = BIND_UNIFORM_BUFFER;
  desc.CPUAccessFlags = CPU_ACCESS_WRITE;
  device->CreateBuffer(desc, nullptr, &impl_->params);
  static const ShaderResourceVariableDesc variables[] = {
      {SHADER_TYPE_COMPUTE, "DepthOfFieldParams", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "InputColor", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "InputDepth", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "OutputColor", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };
  ArtifactCore::ComputePipelineDesc pipeline;
  pipeline.name = "Artifact Depth Of Field Pass";
  pipeline.shaderSource = kDepthOfFieldShader;
  pipeline.entryPoint = "DepthOfFieldCS";
  pipeline.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
  pipeline.variables = variables;
  pipeline.variableCount = static_cast<Uint32>(sizeof(variables) / sizeof(variables[0]));
  pipeline.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
  impl_->ready = impl_->params && impl_->executor->build(pipeline) &&
                 impl_->executor->createShaderResourceBinding(true);
  return impl_->ready;
}

bool DepthOfFieldPass::apply(IDeviceContext* context, ITextureView* color,
                             ITextureView* depth, ITextureView* output,
                             unsigned width, unsigned height,
                             const DepthOfFieldSettings& settings) {
  if (!impl_->ready || !context || !color || !depth || !output ||
      width == 0 || height == 0 || !settings.enabled) return false;
  DepthOfFieldParams params{
      std::max(settings.focusDistance, settings.nearClip),
      std::max(settings.nearClip, 0.001f),
      std::max(settings.farClip, settings.nearClip * 2.0f),
      std::max(settings.maxCocRadius, 0.0f),
      std::clamp(settings.cocScale, 0.0f, 1.0f),
      std::clamp(settings.sampleCount, 4u, 64u),
      width, height,
      std::max(settings.focalLength, 1.0f),
      // Aperture authored as an f-stop-like scale: larger value = more blur.
      std::max(settings.fStop, 0.0f),
      settings.fStop > 0.0f ? 1u : 0u};
  void* mapped = nullptr;
  context->MapBuffer(impl_->params, MAP_WRITE, MAP_FLAG_DISCARD, mapped);
  if (!mapped) return false;
  std::memcpy(mapped, &params, sizeof(params));
  context->UnmapBuffer(impl_->params, MAP_WRITE);
  auto& exec = *impl_->executor;
  if (!exec.setBuffer("DepthOfFieldParams", impl_->params) ||
      !exec.setTextureView("InputColor", color) ||
      !exec.setTextureView("InputDepth", depth) ||
      !exec.setTextureView("OutputColor", output)) return false;
  exec.dispatch(context, ArtifactCore::ComputeExecutor::makeDispatchAttribs(
      width, height, 1, 8, 8, 1));
  return true;
}

bool DepthOfFieldPass::ready() const { return impl_ && impl_->ready; }
}
