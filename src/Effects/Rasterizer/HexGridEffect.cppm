module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <numbers>
#include <QString>
#include <QVariant>
#include <cstring>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <opencv2/opencv.hpp>

module Artifact.Effect.Rasterizer.HexGrid;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;
import Core.Parallel;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;

namespace Artifact {
using namespace ArtifactCore;

class HexGridCPUImpl : public ArtifactEffectImplBase {
public:
    float cellSize_=32,lineWidth_=2,angle_=0;

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        const auto& si=src.image(); int W=si.width(),H=si.height();
        float cs=std::max(cellSize_,4.0f),lw=std::max(lineWidth_,0.5f);
        float rad=angle_*std::numbers::pi_v<float>/180.0f;
        float cr=std::cos(rad),sr=std::sin(rad);
        float hw=cs,hh=cs*0.8660254f; // sqrt(3)/2
        dst=src.DeepCopy();float* d=dst.image().rgba32fData();
        Parallel::For(0,H,[&](int y){float* o=d+(size_t)y*W*4;
            for(int x=0;x<W;++x){float* p=o+(size_t)x*4;
                float rx=(float)x*cr-(float)y*sr,ry=(float)x*sr+(float)y*cr;
                // Convert to hex coordinates
                float q=rx/hw,r=ry/hh;
                float qf=q-std::floor(q),rf=r-std::floor(r);
                int qi=(int)std::floor(q),ri=(int)std::floor(r);
                float dx,dy;
                if((ri&1)==0){// even rows
                    dx=(qf-0.5f)*hw;dy=(rf-0.5f)*hh;
                }else{// odd rows offset
                    dx=(qf-0.0f)*hw;dy=(rf-0.5f)*hh;
                }
                float dist=std::max(std::abs(dx)/hw,std::abs(dy)/hh)*2.0f;
                float v=dist>1.0f-lw/cs?0.0f:1.0f;
                p[0]=v;p[1]=v;p[2]=v;p[3]=1.0f;
            }
        });
    }
};

class HexGridGPUImpl : public ArtifactEffectImplBase {
public:
    float cellSize_=32,lineWidth_=2,angle_=0;
    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override { cpuFallback(src,dst); }
    void applyGPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;if(!acquireSharedRenderDeviceForCurrentBackend(device,context)){cpuFallback(src,dst);return;}
        const auto& image=src.image();if(image.width()<=0||image.height()<=0){cpuFallback(src,dst);return;}
        Diligent::TextureDesc od{};od.Name="HexGrid/Output";od.Type=Diligent::RESOURCE_DIM_TEX_2D;od.Width=image.width();od.Height=image.height();od.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT;od.MipLevels=1;od.ArraySize=1;od.SampleCount=1;od.Usage=Diligent::USAGE_DEFAULT;od.BindFlags=Diligent::BIND_SHADER_RESOURCE|Diligent::BIND_UNORDERED_ACCESS;Diligent::RefCntAutoPtr<Diligent::ITexture> output;device->CreateTexture(od,nullptr,&output);if(!output){cpuFallback(src,dst);return;}
        struct Params{float cell,line,angle,pad;};Diligent::BufferDesc bd{};bd.Name="HexGrid/Params";bd.Size=sizeof(Params);bd.Usage=Diligent::USAGE_DYNAMIC;bd.BindFlags=Diligent::BIND_UNIFORM_BUFFER;bd.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE;Diligent::RefCntAutoPtr<Diligent::IBuffer> params;device->CreateBuffer(bd,nullptr,&params);if(!params){cpuFallback(src,dst);return;}void*m=nullptr;context->MapBuffer(params,Diligent::MAP_WRITE,Diligent::MAP_FLAG_DISCARD,m);if(!m){cpuFallback(src,dst);return;}Params p{cellSize_,lineWidth_,angle_,0};std::memcpy(m,&p,sizeof(p));context->UnmapBuffer(params,Diligent::MAP_WRITE);
        static Diligent::ShaderResourceVariableDesc vars[]={{Diligent::SHADER_TYPE_COMPUTE,"HexGridParams",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_OutputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};ArtifactCore::GpuContext gc{device,context};ArtifactCore::ComputeExecutor ex{gc};ArtifactCore::ComputePipelineDesc pd{};pd.name="HexGrid/PSO";pd.shaderSource=kHlsl;pd.entryPoint="main";pd.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL;pd.variables=vars;pd.variableCount=2;pd.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;if(!ex.build(pd)||!ex.createShaderResourceBinding(true)||!ex.setBuffer("HexGridParams",params)||!ex.setTextureView("g_OutputTexture",output->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))){cpuFallback(src,dst);return;}ex.dispatch(context,ArtifactCore::ComputeExecutor::makeDispatchAttribs(od.Width,od.Height,1,8,8,1),Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        auto sd=od;sd.Name="HexGrid/Readback";sd.Usage=Diligent::USAGE_STAGING;sd.BindFlags=Diligent::BIND_NONE;sd.CPUAccessFlags=Diligent::CPU_ACCESS_READ;Diligent::RefCntAutoPtr<Diligent::ITexture> staging;device->CreateTexture(sd,nullptr,&staging);if(!staging){cpuFallback(src,dst);return;}context->CopyTexture(Diligent::CopyTextureAttribs(output,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION));context->Flush();context->WaitForIdle();Diligent::MappedTextureSubresource read{};context->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,read);if(!read.pData||!read.Stride){cpuFallback(src,dst);return;}cv::Mat result(image.height(),image.width(),CV_32FC4,read.pData,read.Stride);dst.image().setFromCVMat(result);context->UnmapTextureSubresource(staging,0,0);
    }
private:
    void cpuFallback(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst){HexGridCPUImpl c;c.cellSize_=cellSize_;c.lineWidth_=lineWidth_;c.angle_=angle_;c.applyCPU(src,dst);}
    static constexpr const char* kHlsl=R"(
RWTexture2D<float4> g_OutputTexture:register(u0);cbuffer HexGridParams:register(b0){float g_Cell;float g_Line;float g_Angle;float g_Pad;}
[numthreads(8,8,1)]void main(uint3 id:SV_DispatchThreadID){uint w,h;g_OutputTexture.GetDimensions(w,h);if(id.x>=w||id.y>=h)return;float a=g_Angle*0.0174532925,cr=cos(a),sr=sin(a),rx=id.x*cr-id.y*sr,ry=id.x*sr+id.y*cr;float hh=g_Cell*0.8660254,q=rx/g_Cell,r=ry/hh,qf=q-floor(q),rf=r-floor(r);int ri=(int)floor(r);float2 d;if((ri&1)==0)d=float2((qf-0.5)*g_Cell,(rf-0.5)*hh);else d=float2(qf*g_Cell,(rf-0.5)*hh);float dist=max(abs(d.x)/g_Cell,abs(d.y)/hh)*2;float v=dist>1-g_Line/max(g_Cell,0.001)?0:1;g_OutputTexture[id.xy]=float4(v,v,v,1);}
)";
};

HexGridEffect::HexGridEffect():ArtifactAbstractEffect(){setPipelineStage(EffectPipelineStage::Rasterizer);syncImpls();setGPUImpl(std::make_shared<HexGridGPUImpl>());}
HexGridEffect::~HexGridEffect()=default;
float HexGridEffect::cellSize()const{return cellSize_;}void HexGridEffect::setCellSize(float v){cellSize_=std::max(v,4.0f);syncImpls();}
float HexGridEffect::lineWidth()const{return lineWidth_;}void HexGridEffect::setLineWidth(float v){lineWidth_=std::max(v,0.5f);syncImpls();}
float HexGridEffect::angle()const{return angle_;}void HexGridEffect::setAngle(float v){angle_=v;syncImpls();}
std::vector<AbstractProperty> HexGridEffect::getProperties()const{
    std::vector<AbstractProperty> props;
    props.reserve(3);

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

    addFloat("cellSize", cellSize_, 4.0f, 200.0f);
    addFloat("lineWidth", lineWidth_, 0.5f, 20.0f);
    addFloat("angle", angle_, 0.0f, 360.0f);
    return props;
}
void HexGridEffect::setPropertyValue(const UniString& n,const QVariant& v){const QString k=n.toQString();if(k=="cellSize")setCellSize(v.toFloat());else if(k=="lineWidth")setLineWidth(v.toFloat());else if(k=="angle")setAngle(v.toFloat());}
void HexGridEffect::syncImpls(){auto c=std::make_shared<HexGridCPUImpl>();c->cellSize_=cellSize_;c->lineWidth_=lineWidth_;c->angle_=angle_;setCPUImpl(c);if(auto* g=dynamic_cast<HexGridGPUImpl*>(gpuImpl().get())){g->cellSize_=cellSize_;g->lineWidth_=lineWidth_;g->angle_=angle_;}}
} // namespace Artifact
