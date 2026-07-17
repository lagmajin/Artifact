module;
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>
#include <opencv2/opencv.hpp>
#include <QString>
#include <QVariant>
#include <cstring>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module Artifact.Effect.Rasterizer.FindEdges;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;

namespace Artifact {

using namespace ArtifactCore;

class FindEdgesEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float amount_ = 1.0f;
    bool invert_ = false;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        auto& srcImage = src.image();
        const float* pixels = srcImage.rgba32fData();
        if (!pixels) {
            dst = src;
            return;
        }
        dst = src;
        cv::Mat mat(dst.image().height(), dst.image().width(), CV_32FC4, dst.image().rgba32fData());

        std::vector<cv::Mat> channels;
        cv::split(mat, channels);
        cv::Mat color;
        cv::Mat alpha = channels[3];
        cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, color);

        cv::Mat gray;
        {
            std::vector<cv::Mat> tmp;
            cv::split(color, tmp);
            gray = tmp[0] * 0.299f + tmp[1] * 0.587f + tmp[2] * 0.114f;
        }

        cv::Mat edge;
        cv::Laplacian(gray, edge, CV_32F, 3, 1.0, 0.0, cv::BORDER_REPLICATE);
        edge = cv::abs(edge);
        cv::Mat edgeNorm;
        cv::normalize(edge, edgeNorm, 0.0, 1.0, cv::NORM_MINMAX);
        if (invert_) {
            edgeNorm = 1.0f - edgeNorm;
        }

        cv::Mat edge3;
        cv::merge(std::vector<cv::Mat>{edgeNorm, edgeNorm, edgeNorm}, edge3);

        cv::Mat result = color * (1.0f - amount_) + edge3 * amount_;
        result = cv::max(cv::Mat::zeros(result.size(), result.type()), result);

        std::vector<cv::Mat> out;
        cv::split(result, out);
        out.push_back(alpha);
        cv::merge(out, mat);
    }
};

class FindEdgesEffectGPUImpl : public ArtifactEffectImplBase {
public:
    float amount_=1.0f;
    bool invert_=false;
    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        FindEdgesEffectCPUImpl cpu;cpu.amount_=amount_;cpu.invert_=invert_;cpu.applyCPU(src,dst);
    }
    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;if(!acquireSharedRenderDeviceForCurrentBackend(device,context)){applyCPU(src,dst);return;}
        const auto& image=src.image();const float* pixels=image.rgba32fData();if(!pixels||image.width()<=0||image.height()<=0){applyCPU(src,dst);return;}
        Diligent::TextureDesc desc{};desc.Name="FindEdges/Input";desc.Type=Diligent::RESOURCE_DIM_TEX_2D;desc.Width=image.width();desc.Height=image.height();desc.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT;desc.MipLevels=1;desc.ArraySize=1;desc.SampleCount=1;desc.Usage=Diligent::USAGE_IMMUTABLE;desc.BindFlags=Diligent::BIND_SHADER_RESOURCE;Diligent::TextureSubResData sub{};sub.pData=pixels;sub.Stride=static_cast<Diligent::Uint64>(image.width())*sizeof(float)*4ull;Diligent::TextureData init{};init.pSubResources=&sub;init.NumSubresources=1;Diligent::RefCntAutoPtr<Diligent::ITexture> input;device->CreateTexture(desc,&init,&input);if(!input){applyCPU(src,dst);return;}
        Diligent::TextureDesc outDesc=desc;outDesc.Name="FindEdges/Output";outDesc.Usage=Diligent::USAGE_DEFAULT;outDesc.BindFlags=Diligent::BIND_SHADER_RESOURCE|Diligent::BIND_UNORDERED_ACCESS;Diligent::RefCntAutoPtr<Diligent::ITexture> output;device->CreateTexture(outDesc,nullptr,&output);if(!output){applyCPU(src,dst);return;}
        Diligent::BufferDesc cbDesc{};cbDesc.Name="FindEdges/Params";cbDesc.Size=sizeof(Params);cbDesc.Usage=Diligent::USAGE_DYNAMIC;cbDesc.BindFlags=Diligent::BIND_UNIFORM_BUFFER;cbDesc.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE;Diligent::RefCntAutoPtr<Diligent::IBuffer> params;device->CreateBuffer(cbDesc,nullptr,&params);if(!params){applyCPU(src,dst);return;}void* mapped=nullptr;context->MapBuffer(params,Diligent::MAP_WRITE,Diligent::MAP_FLAG_DISCARD,mapped);if(!mapped){applyCPU(src,dst);return;}Params values{amount_,invert_?1.0f:0.0f};std::memcpy(mapped,&values,sizeof(values));context->UnmapBuffer(params,Diligent::MAP_WRITE);
        static Diligent::ShaderResourceVariableDesc vars[]={{Diligent::SHADER_TYPE_COMPUTE,"FindEdgesParams",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_InputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_OutputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};ArtifactCore::GpuContext gpuContext{device,context};ArtifactCore::ComputeExecutor executor{gpuContext};ArtifactCore::ComputePipelineDesc pipeline{};pipeline.name="FindEdges/PSO";pipeline.shaderSource=kHlsl;pipeline.entryPoint="main";pipeline.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL;pipeline.variables=vars;pipeline.variableCount=3;pipeline.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;if(!executor.build(pipeline)||!executor.createShaderResourceBinding(true)||!executor.setBuffer("FindEdgesParams",params)||!executor.setTextureView("g_InputTexture",input->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE))||!executor.setTextureView("g_OutputTexture",output->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))){applyCPU(src,dst);return;}executor.dispatch(context,ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width,outDesc.Height,1,8,8,1),Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        Diligent::TextureDesc stagingDesc=outDesc;stagingDesc.Name="FindEdges/Readback";stagingDesc.Usage=Diligent::USAGE_STAGING;stagingDesc.BindFlags=Diligent::BIND_NONE;stagingDesc.CPUAccessFlags=Diligent::CPU_ACCESS_READ;Diligent::RefCntAutoPtr<Diligent::ITexture> staging;device->CreateTexture(stagingDesc,nullptr,&staging);if(!staging){applyCPU(src,dst);return;}Diligent::CopyTextureAttribs copy(output,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);context->CopyTexture(copy);context->Flush();context->WaitForIdle();Diligent::MappedTextureSubresource read{};context->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,read);if(!read.pData||!read.Stride){applyCPU(src,dst);return;}cv::Mat result(static_cast<int>(outDesc.Height),static_cast<int>(outDesc.Width),CV_32FC4,read.pData,read.Stride);dst.image().setFromCVMat(result);context->UnmapTextureSubresource(staging,0,0);
    }
private:
    struct Params{float amount,invert,pad0=0.0f,pad1=0.0f;};
    static constexpr const char* kHlsl=R"(
Texture2D<float4> g_InputTexture:register(t0);RWTexture2D<float4> g_OutputTexture:register(u0);cbuffer FindEdgesParams:register(b0){float g_Amount;float g_Invert;float2 g_Pad;}
float luma(float3 c){return dot(c,float3(0.299,0.587,0.114));}float4 sampleP(int2 p,uint w,uint h){p.x=clamp(p.x,0,(int)w-1);p.y=clamp(p.y,0,(int)h-1);return g_InputTexture[uint2(p)];}
[numthreads(8,8,1)]void main(uint3 id:SV_DispatchThreadID){uint w,h;g_OutputTexture.GetDimensions(w,h);if(id.x>=w||id.y>=h)return;int2 p=int2(id.xy);float gx=-luma(sampleP(p+int2(-1,-1),w,h).rgb)-2*luma(sampleP(p+int2(-1,0),w,h).rgb)-luma(sampleP(p+int2(-1,1),w,h).rgb)+luma(sampleP(p+int2(1,-1),w,h).rgb)+2*luma(sampleP(p+int2(1,0),w,h).rgb)+luma(sampleP(p+int2(1,1),w,h).rgb);float gy=-luma(sampleP(p+int2(-1,-1),w,h).rgb)-2*luma(sampleP(p+int2(0,-1),w,h).rgb)-luma(sampleP(p+int2(1,-1),w,h).rgb)+luma(sampleP(p+int2(-1,1),w,h).rgb)+2*luma(sampleP(p+int2(0,1),w,h).rgb)+luma(sampleP(p+int2(1,1),w,h).rgb);float e=saturate(length(float2(gx,gy))*g_Amount);if(g_Invert>0.5)e=1-e;g_OutputTexture[id.xy]=float4(e,e,e,1);}
)";
};

FindEdgesEffect::FindEdgesEffect() {
    setDisplayName(UniString("Find Edges"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<FindEdgesEffectCPUImpl>());
    auto gpu=std::make_shared<FindEdgesEffectGPUImpl>();gpu->amount_=amount_;gpu->invert_=invert_;setGPUImpl(gpu);setComputeMode(ComputeMode::AUTO);
}

FindEdgesEffect::~FindEdgesEffect() = default;

float FindEdgesEffect::amount() const { return amount_; }
void FindEdgesEffect::setAmount(float v) { amount_ = std::clamp(v, 0.0f, 5.0f); syncImpls(); }
bool FindEdgesEffect::invert() const { return invert_; }
void FindEdgesEffect::setInvert(bool v) { invert_ = v; syncImpls(); }

void FindEdgesEffect::syncImpls() {
    if (auto* c = dynamic_cast<FindEdgesEffectCPUImpl*>(cpuImpl().get())) {
        c->amount_ = amount_;
        c->invert_ = invert_;
    }
    if (auto* g = dynamic_cast<FindEdgesEffectGPUImpl*>(gpuImpl().get())) {
        g->amount_ = amount_;
        g->invert_ = invert_;
    }
}

std::vector<AbstractProperty> FindEdgesEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    auto& a = props.emplace_back(); a.setName("Amount"); a.setType(PropertyType::Float); a.setValue(amount_);
    auto& i = props.emplace_back(); i.setName("Invert"); i.setType(PropertyType::Boolean); i.setValue(invert_);
    return props;
}

void FindEdgesEffect::setPropertyValue(const UniString& n, const QVariant& v) {
    const QString k = n.toQString();
    if (k == "Amount") setAmount(v.toFloat());
    else if (k == "Invert") setInvert(v.toBool());
}

} // namespace Artifact
