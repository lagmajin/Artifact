module;
#include <cstring>
#include <memory>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <QDebug>

module Artifact.Render.AccumulatorPass;
import Graphics.GPUcomputeContext;
import Graphics.Compute;

namespace Artifact {
namespace {
constexpr char kAccumulatorHlsl[] = R"(
cbuffer AccumulatorParams { float InputGain; float Feedback; float Decay; uint BlendMode; };
Texture2D<float4> CurrentTexture;
Texture2D<float4> PreviousTexture;
RWTexture2D<float4> OutputTexture;
float4 normalBlend(float4 previous, float4 current) {
  float a = saturate(current.a);
  return float4(current.rgb + previous.rgb * (1.0 - a), a + previous.a * (1.0 - a));
}
[numthreads(8,8,1)] void main(uint3 id : SV_DispatchThreadID) {
  uint w,h; OutputTexture.GetDimensions(w,h); if (id.x >= w || id.y >= h) return;
  float4 previous = PreviousTexture.Load(int3(id.xy,0)) * (Feedback * Decay);
  float4 current = CurrentTexture.Load(int3(id.xy,0)) * InputGain;
  float4 output = BlendMode == 0 ? normalBlend(previous, current) : previous + current;
  output.a = saturate(output.a); OutputTexture[id.xy] = output;
})";
}

class AccumulatorPass::Impl {
public:
  std::unique_ptr<ArtifactCore::GpuContext> gpuContext;
  std::unique_ptr<ArtifactCore::ComputeExecutor> executor;
  Diligent::RefCntAutoPtr<Diligent::IBuffer> parameters;
};

AccumulatorPass::AccumulatorPass() : impl_(new Impl()) {}
AccumulatorPass::~AccumulatorPass() { delete impl_; }
bool AccumulatorPass::initialize(Diligent::IRenderDevice* device, Diligent::IDeviceContext* context) {
  if (!device || !context) return false;
  impl_->gpuContext = std::make_unique<ArtifactCore::GpuContext>(device, context);
  impl_->executor = std::make_unique<ArtifactCore::ComputeExecutor>(*impl_->gpuContext);
  Diligent::BufferDesc desc; desc.Name = "AccumulatorParams"; desc.Usage = Diligent::USAGE_DYNAMIC;
  desc.Size = sizeof(AccumulatorPassParams); desc.BindFlags = Diligent::BIND_UNIFORM_BUFFER; desc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
  device->CreateBuffer(desc, nullptr, &impl_->parameters);
  Diligent::ShaderResourceVariableDesc variables[] = {
    {Diligent::SHADER_TYPE_COMPUTE, "AccumulatorParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
    {Diligent::SHADER_TYPE_COMPUTE, "CurrentTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
    {Diligent::SHADER_TYPE_COMPUTE, "PreviousTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
    {Diligent::SHADER_TYPE_COMPUTE, "OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};
  ArtifactCore::ComputePipelineDesc pipeline; pipeline.name = "AccumulatorPass"; pipeline.shaderSource = kAccumulatorHlsl; pipeline.entryPoint = "main";
  pipeline.sourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL; pipeline.variables = variables; pipeline.variableCount = 4;
  pipeline.defaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
  return impl_->parameters && impl_->executor->build(pipeline) && impl_->executor->createShaderResourceBinding(true);
}
bool AccumulatorPass::ready() const { return impl_ && impl_->parameters && impl_->executor && impl_->executor->ready(); }
bool AccumulatorPass::apply(Diligent::IDeviceContext* context, Diligent::ITextureView* current, Diligent::ITextureView* previous, Diligent::ITextureView* output, Diligent::Uint32 width, Diligent::Uint32 height, const AccumulatorPassParams& params) {
  if (!ready() || !context || !current || !previous || !output || !width || !height || current->GetTexture() == output->GetTexture() || previous->GetTexture() == output->GetTexture()) return false;
  void* mapped = nullptr; context->MapBuffer(impl_->parameters, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, mapped); if (!mapped) return false;
  std::memcpy(mapped, &params, sizeof(params)); context->UnmapBuffer(impl_->parameters, Diligent::MAP_WRITE);
  if (!impl_->executor->setBuffer("AccumulatorParams", impl_->parameters) || !impl_->executor->setTextureView("CurrentTexture", current) || !impl_->executor->setTextureView("PreviousTexture", previous) || !impl_->executor->setTextureView("OutputTexture", output)) return false;
  impl_->executor->dispatch(context, ArtifactCore::ComputeExecutor::makeDispatchAttribs(width, height, 1, 8, 8, 1), Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  return true;
}
}
