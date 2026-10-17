module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <QString>
#include <QVariant>
#include <opencv2/opencv.hpp>
#include <cstring>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module Artifact.Effect.Rasterizer.ChromaticAberration;

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.GpuImageUpload;
import Image.ImageSurfaceView;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;
import Core.Parallel;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;
import Memory.SharedPtr;

namespace Artifact {
using namespace ArtifactCore;

class ChromaticAberrationCPUImpl : public ArtifactEffectImplBase {
public:
    float redShift_=2.0f,blueShift_=2.0f,cx_=0.5f,cy_=0.5f;
    Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex_;

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        const auto& source=src.image();
        if(!source.rgba32fData()||source.width()<=0||source.height()<=0){dst=src;return;}
        dst=src.DeepCopy();
        const int width=source.width(),height=source.height();
        const auto sourceView=source.surfaceView();
        const std::size_t redChannel=source.colorDescriptor().channelOrder==ArtifactCore::SurfaceChannelOrder::RGBA?0u:2u;
        const std::size_t blueChannel=source.colorDescriptor().channelOrder==ArtifactCore::SurfaceChannelOrder::RGBA?2u:0u;
        const float red=std::clamp(redShift_,0.0f,50.0f),blue=std::clamp(blueShift_,0.0f,50.0f);
        const float centerX=cx_*static_cast<float>(width),centerY=cy_*static_cast<float>(height);
        float* output=dst.image().rgba32fData();
        ArtifactCore::withMutableColorFloat4View(dst.image().surfaceView(),output,[&](auto& view){
            ArtifactCore::Parallel::ForPixels(0,height,width,height,[&](int y){
                auto row=view.row(y);
                for(int x=0;x<width;++x){
                    const float dx=static_cast<float>(x)-centerX,dy=static_cast<float>(y)-centerY;
                    const float distance=std::sqrt(dx*dx+dy*dy);
                    const float normalized=distance/std::max(static_cast<float>(std::max(width,height))*0.7f,1.0f);
                    const float redOffset=normalized*red,blueOffset=normalized*blue;
                    const float inverseDistance=distance>0.0f?1.0f/distance:0.0f;
                    const int redX=std::clamp(static_cast<int>(static_cast<float>(x)+dx*inverseDistance*redOffset+0.5f),0,width-1);
                    const int redY=std::clamp(static_cast<int>(static_cast<float>(y)+dy*inverseDistance*redOffset+0.5f),0,height-1);
                    const int blueX=std::clamp(static_cast<int>(static_cast<float>(x)-dx*inverseDistance*blueOffset+0.5f),0,width-1);
                    const int blueY=std::clamp(static_cast<int>(static_cast<float>(y)-dy*inverseDistance*blueOffset+0.5f),0,height-1);
                    const auto* redSource=reinterpret_cast<const float*>(static_cast<const std::uint8_t*>(sourceView.data)+static_cast<std::size_t>(redY)*sourceView.rowStride)+static_cast<std::size_t>(redX)*4u;
                    const auto* blueSource=reinterpret_cast<const float*>(static_cast<const std::uint8_t*>(sourceView.data)+static_cast<std::size_t>(blueY)*sourceView.rowStride)+static_cast<std::size_t>(blueX)*4u;
                    row[x].r=redSource[redChannel];
                    row[x].b=blueSource[blueChannel];
                }
            });
        });
    }
};

class ChromaticAberrationGPUImpl : public ArtifactEffectImplBase {
public:
    float redShift_=2.0f,blueShift_=2.0f,cx_=0.5f,cy_=0.5f;
    Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex_;
    void applyCPU(const ImageF32x4RGBAWithCache& s,ImageF32x4RGBAWithCache& d) override { cpu_.redShift_=redShift_;cpu_.blueShift_=blueShift_;cpu_.cx_=cx_;cpu_.cy_=cy_;cpu_.applyCPU(s,d); }
    void applyGPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device; Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;
        if(!acquireSharedRenderDeviceForCurrentBackend(device,context)){applyCPU(src,dst);return;}
        const auto& image=src.image();if(image.width()<=0||image.height()<=0){applyCPU(src,dst);return;}const auto upload=ArtifactCore::makeGpuImageUploadBuffer(image.surfaceView(), ArtifactCore::GpuImageChannelOrder::Bgra);if(!upload.isValid()){applyCPU(src,dst);return;}
        Diligent::TextureDesc d{};d.Name="ChromaticAberration/Input";d.Type=Diligent::RESOURCE_DIM_TEX_2D;d.Width=image.width();d.Height=image.height();d.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT;d.MipLevels=1;d.ArraySize=1;d.SampleCount=1;d.Usage=Diligent::USAGE_IMMUTABLE;d.BindFlags=Diligent::BIND_SHADER_RESOURCE;Diligent::TextureSubResData sub{};sub.pData=upload.data();sub.Stride=static_cast<Diligent::Uint64>(upload.rowStride);Diligent::TextureData init{};init.pSubResources=&sub;init.NumSubresources=1;Diligent::RefCntAutoPtr<Diligent::ITexture> input;device->CreateTexture(d,&init,&input);if(!input){applyCPU(src,dst);return;}
        auto od=d;od.Name="ChromaticAberration/Output";od.Usage=Diligent::USAGE_DEFAULT;od.BindFlags=Diligent::BIND_SHADER_RESOURCE|Diligent::BIND_UNORDERED_ACCESS;if(!outputTex_||outputTex_->GetDesc().Width!=od.Width||outputTex_->GetDesc().Height!=od.Height||outputTex_->GetDesc().Format!=od.Format||outputTex_->GetDesc().BindFlags!=od.BindFlags){outputTex_.Release();device->CreateTexture(od,nullptr,&outputTex_);}if(!outputTex_){applyCPU(src,dst);return;}
        struct Params{float red,blue,cx,cy;};Diligent::BufferDesc bd{};bd.Name="ChromaticAberration/Params";bd.Size=sizeof(Params);bd.Usage=Diligent::USAGE_DYNAMIC;bd.BindFlags=Diligent::BIND_UNIFORM_BUFFER;bd.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE;Diligent::RefCntAutoPtr<Diligent::IBuffer> params;device->CreateBuffer(bd,nullptr,&params);if(!params){applyCPU(src,dst);return;}void*m=nullptr;context->MapBuffer(params,Diligent::MAP_WRITE,Diligent::MAP_FLAG_DISCARD,m);if(!m){applyCPU(src,dst);return;}Params p{redShift_,blueShift_,cx_,cy_};std::memcpy(m,&p,sizeof(p));context->UnmapBuffer(params,Diligent::MAP_WRITE);
        static Diligent::ShaderResourceVariableDesc vars[]={{Diligent::SHADER_TYPE_COMPUTE,"ChromaticAberrationParams",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_InputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_OutputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};ArtifactCore::GpuContext gc{device,context};ArtifactCore::ComputeExecutor ex{gc};ArtifactCore::ComputePipelineDesc pd{};pd.name="ChromaticAberration/PSO";pd.shaderSource=kHlsl;pd.entryPoint="main";pd.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL;pd.variables=vars;pd.variableCount=3;pd.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;if(!ex.build(pd)||!ex.createShaderResourceBinding(true)||!ex.setBuffer("ChromaticAberrationParams",params)||!ex.setTextureView("g_InputTexture",input->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE))||!ex.setTextureView("g_OutputTexture",outputTex_->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))){applyCPU(src,dst);return;}ex.dispatch(context,ArtifactCore::ComputeExecutor::makeDispatchAttribs(od.Width,od.Height,1,8,8,1),Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        auto sd=od;sd.Name="ChromaticAberration/Readback";sd.Usage=Diligent::USAGE_STAGING;sd.BindFlags=Diligent::BIND_NONE;sd.CPUAccessFlags=Diligent::CPU_ACCESS_READ;Diligent::RefCntAutoPtr<Diligent::ITexture> staging;device->CreateTexture(sd,nullptr,&staging);if(!staging){applyCPU(src,dst);return;}context->CopyTexture(Diligent::CopyTextureAttribs(outputTex_,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION));context->Flush();context->WaitForIdle();Diligent::MappedTextureSubresource read{};context->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,read);if(!read.pData||!read.Stride){applyCPU(src,dst);return;}const bool copied=dst.image().setFromGpuBGRA32F(static_cast<const float*>(read.pData),image.width(),image.height(),static_cast<std::size_t>(read.Stride),image.colorDescriptor());context->UnmapTextureSubresource(staging,0,0);if(!copied){applyCPU(src,dst);return;}
    }
private:
    ChromaticAberrationCPUImpl cpu_;
    static constexpr const char* kHlsl=R"(
Texture2D<float4> g_InputTexture:register(t0);RWTexture2D<float4> g_OutputTexture:register(u0);cbuffer ChromaticAberrationParams:register(b0){float g_Red;float g_Blue;float g_Cx;float g_Cy;}
float4 atp(int2 p,uint w,uint h){return (g_InputTexture[uint2(clamp(p.x,0,(int)w-1),clamp(p.y,0,(int)h-1))]).bgra;}
[numthreads(8,8,1)]void main(uint3 id:SV_DispatchThreadID){uint w,h;g_OutputTexture.GetDimensions(w,h);if(id.x>=w||id.y>=h)return;float2 q=float2(id.xy),c=float2(g_Cx*w,g_Cy*h),v=q-c;float dist=length(v),n=dist/max(max(w,h)*0.7,1);float2 dir=dist>0?v/dist:float2(0,0);float rs=n*g_Red,bs=n*g_Blue;float4 r=atp(int2(q+dir*rs+0.5),w,h),b=atp(int2(q-dir*bs+0.5),w,h),o=(g_InputTexture[id.xy]).bgra;o.r=r.r;o.b=b.b;g_OutputTexture[id.xy]=(o).bgra;}
)";
};

ChromaticAberrationEffect::ChromaticAberrationEffect():ArtifactAbstractEffect(){setPipelineStage(EffectPipelineStage::Rasterizer);syncImpls();setGPUImpl(ArtifactCore::makeShared<ChromaticAberrationGPUImpl>());}
ChromaticAberrationEffect::~ChromaticAberrationEffect()=default;
float ChromaticAberrationEffect::redShift()const{return redShift_;}void ChromaticAberrationEffect::setRedShift(float v){redShift_=std::isfinite(v)?std::clamp(v,0.0f,50.0f):2.0f;syncImpls();}
float ChromaticAberrationEffect::blueShift()const{return blueShift_;}void ChromaticAberrationEffect::setBlueShift(float v){blueShift_=std::isfinite(v)?std::clamp(v,0.0f,50.0f):2.0f;syncImpls();}
float ChromaticAberrationEffect::centerX()const{return cx_;}void ChromaticAberrationEffect::setCenterX(float v){cx_=std::isfinite(v)?std::clamp(v,0.0f,1.0f):0.5f;syncImpls();}
float ChromaticAberrationEffect::centerY()const{return cy_;}void ChromaticAberrationEffect::setCenterY(float v){cy_=std::isfinite(v)?std::clamp(v,0.0f,1.0f):0.5f;syncImpls();}
std::vector<AbstractProperty> ChromaticAberrationEffect::getProperties()const{
    std::vector<AbstractProperty> props;
    props.reserve(4);

    auto addFloat = [&props](const char* name, float value, float minValue, float maxValue) {
        AbstractProperty prop;
        prop.setName(QString::fromLatin1(name));
        prop.setType(PropertyType::Float);
        const QVariant variantValue(static_cast<double>(value));
        prop.setValue(variantValue);
        prop.setDefaultValue(variantValue);
        prop.setMinValue(QVariant(static_cast<double>(minValue)));
        prop.setMaxValue(QVariant(static_cast<double>(maxValue)));
        props.push_back(std::move(prop));
    };

    addFloat("redShift", redShift_, 0.0f, 50.0f);
    addFloat("blueShift", blueShift_, 0.0f, 50.0f);
    addFloat("centerX", cx_, 0.0f, 1.0f);
    addFloat("centerY", cy_, 0.0f, 1.0f);
    return props;
}
void ChromaticAberrationEffect::setPropertyValue(const UniString& n,const QVariant& v){const QString k=n.toQString();if(k=="redShift")setRedShift(v.toFloat());else if(k=="blueShift")setBlueShift(v.toFloat());else if(k=="centerX")setCenterX(v.toFloat());else if(k=="centerY")setCenterY(v.toFloat());}
void ChromaticAberrationEffect::syncImpls(){auto c=ArtifactCore::makeShared<ChromaticAberrationCPUImpl>();c->redShift_=redShift_;c->blueShift_=blueShift_;c->cx_=cx_;c->cy_=cy_;setCPUImpl(c);if(auto* g=dynamic_cast<ChromaticAberrationGPUImpl*>(gpuImpl().get())){g->redShift_=redShift_;g->blueShift_=blueShift_;g->cx_=cx_;g->cy_=cy_;}}
} // namespace Artifact
