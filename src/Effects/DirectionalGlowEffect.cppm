module;
#include <utility>
#include <algorithm>
#include <cmath>
#include <memory>
#include <QVariant>
#include <vector>
#include <opencv2/opencv.hpp>
#include <cstring>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module Artifact.Effect.DirectionalGlow;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Core.Parallel;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;

namespace Artifact {

static cv::Mat directionalBlur1D(const cv::Mat& src, float angleDeg, float length) {
    if (src.empty()) {
        return src;
    }

    const int radius = std::max(1, static_cast<int>(length * 0.5f));
    const float angleRad = angleDeg * (3.14159265f / 180.0f);
    const float dx = std::cos(angleRad);
    const float dy = std::sin(angleRad);

    cv::Mat result = cv::Mat::zeros(src.size(), src.type());

    ArtifactCore::Parallel::For(0, src.rows, [&](int y) {
        for (int x = 0; x < src.cols; ++x) {
            cv::Vec4f sum(0, 0, 0, 0);
            float totalWeight = 0.0f;

            for (int s = -radius; s <= radius; ++s) {
                float sx = x + dx * s;
                float sy = y + dy * s;
                sx = std::clamp(sx, 0.0f, static_cast<float>(src.cols - 1));
                sy = std::clamp(sy, 0.0f, static_cast<float>(src.rows - 1));

                const int ix = static_cast<int>(sx);
                const int iy = static_cast<int>(sy);
                const float fx = sx - ix;
                const float fy = sy - iy;
                const int ix2 = std::min(ix + 1, src.cols - 1);
                const int iy2 = std::min(iy + 1, src.rows - 1);

                const cv::Vec4f p00 = src.at<cv::Vec4f>(iy, ix);
                const cv::Vec4f p10 = src.at<cv::Vec4f>(iy, ix2);
                const cv::Vec4f p01 = src.at<cv::Vec4f>(iy2, ix);
                const cv::Vec4f p11 = src.at<cv::Vec4f>(iy2, ix2);

                const cv::Vec4f sample = p00 * (1.0f - fx) * (1.0f - fy) +
                                         p10 * fx * (1.0f - fy) +
                                         p01 * (1.0f - fx) * fy +
                                         p11 * fx * fy;

                const float weight = std::exp(-0.5f * (s * s) / (radius * radius * 0.25f));
                sum += sample * weight;
                totalWeight += weight;
            }

            result.at<cv::Vec4f>(y, x) = totalWeight > 0.0f ? sum / totalWeight : cv::Vec4f(0, 0, 0, 0);
        }
    }

    return result;
}

static QVector<float> getAnglesForPattern(StreakPattern pattern, float angleOffset) {
    QVector<float> angles;
    switch (pattern) {
    case StreakPattern::Horizontal:
        angles.append(0.0f + angleOffset);
        break;
    case StreakPattern::Cross:
        angles.append(0.0f + angleOffset);
        angles.append(90.0f + angleOffset);
        break;
    case StreakPattern::Star:
        angles.append(0.0f + angleOffset);
        angles.append(45.0f + angleOffset);
        angles.append(90.0f + angleOffset);
        angles.append(135.0f + angleOffset);
        break;
    case StreakPattern::Custom:
        break;
    }
    return angles;
}

class DirectionalGlowCPUImpl : public ArtifactEffectImplBase {
public:
    float threshold_ = 0.8f;
    float intensity_ = 1.0f;
    float length1_ = 64.0f;
    float length2_ = 128.0f;
    float weight1_ = 0.6f;
    float weight2_ = 0.4f;
    StreakPattern pattern_ = StreakPattern::Horizontal;
    QVector<float> customAngles_;
    float angleOffset_ = 0.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        auto& srcImage = src.image();
        const float* srcData = srcImage.rgba32fData();
        if (!srcData) {
            dst = src;
            return;
        }

        cv::Mat mat(srcImage.height(), srcImage.width(), CV_32FC4, const_cast<float*>(srcData));

        cv::Mat bright = cv::Mat::zeros(mat.size(), CV_32FC4);
        ArtifactCore::Parallel::For(0, mat.rows, [&](int y) {
            for (int x = 0; x < mat.cols; ++x) {
                const cv::Vec4f p = mat.at<cv::Vec4f>(y, x);
                const float lum = 0.299f * p[2] + 0.587f * p[1] + 0.114f * p[0];
                if (lum > threshold_) {
                    const float scale = (lum - threshold_) / (1.0f - threshold_);
                    bright.at<cv::Vec4f>(y, x) = p * scale;
                }
            }
        });

        const QVector<float> angles = customAngles_.isEmpty()
            ? getAnglesForPattern(pattern_, angleOffset_)
            : customAngles_;
        if (angles.isEmpty()) {
            dst = src;
            return;
        }

        cv::Mat streaks = cv::Mat::zeros(mat.size(), CV_32FC4);
        for (float angle : angles) {
            const cv::Mat s1 = directionalBlur1D(bright, angle, length1_);
            const cv::Mat s2 = directionalBlur1D(bright, angle, length2_);
            streaks += s1 * weight1_ + s2 * weight2_;
        }

        cv::Mat result = mat.clone();
        ArtifactCore::Parallel::For(0, mat.rows, [&](int y) {
            for (int x = 0; x < mat.cols; ++x) {
                cv::Vec4f& dstP = result.at<cv::Vec4f>(y, x);
                const cv::Vec4f streakP = streaks.at<cv::Vec4f>(y, x) * intensity_;
                dstP += streakP;
                dstP[0] = std::clamp(dstP[0], 0.0f, 1.0f);
                dstP[1] = std::clamp(dstP[1], 0.0f, 1.0f);
                dstP[2] = std::clamp(dstP[2], 0.0f, 1.0f);
            }
        });

        dst.image().setFromRGBA32F(result.ptr<float>(), result.cols, result.rows);
    });
};

class DirectionalGlowGPUImpl : public ArtifactEffectImplBase {
public:
    float threshold_ = 0.8f;
    float intensity_ = 1.0f;
    float length1_ = 64.0f;
    float length2_ = 128.0f;
    float weight1_ = 0.6f;
    float weight2_ = 0.4f;
    StreakPattern pattern_ = StreakPattern::Horizontal;
    QVector<float> customAngles_;
    float angleOffset_ = 0.0f;
    mutable Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
    mutable Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context_;
    mutable Diligent::RefCntAutoPtr<Diligent::IBuffer> paramsCB_;
    mutable bool pipelineReady_ = false;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        if (!customAngles_.isEmpty()) { applyCPU(src,dst); return; }
        if (!acquireSharedRenderDeviceForCurrentBackend(device_,context_)) { applyCPU(src,dst); return; }
        auto gpuContext=std::make_unique<ArtifactCore::GpuContext>(device_,context_);auto executor=std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext);
        if(!paramsCB_){Diligent::BufferDesc d;d.Name="DirectionalGlow/Params";d.Size=sizeof(ParamsCB);d.Usage=Diligent::USAGE_DYNAMIC;d.BindFlags=Diligent::BIND_UNIFORM_BUFFER;d.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE;device_->CreateBuffer(d,nullptr,&paramsCB_);}if(!paramsCB_){applyCPU(src,dst);return;}
        static Diligent::ShaderResourceVariableDesc vars[]={{Diligent::SHADER_TYPE_COMPUTE,"DirectionalGlowParams",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_InputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_OutputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};
        if(!pipelineReady_){ArtifactCore::ComputePipelineDesc d;d.name="DirectionalGlow/PSO";d.shaderSource=kHlsl;d.entryPoint="main";d.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL;d.variables=vars;d.variableCount=3;d.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;if(!executor->build(d)||!executor->createShaderResourceBinding(true)||!executor->setBuffer("DirectionalGlowParams",paramsCB_)){applyCPU(src,dst);return;}pipelineReady_=true;}
        Diligent::RefCntAutoPtr<Diligent::ITexture> input;if(!createTexture(src,&input,"DirectionalGlow/Input")){applyCPU(src,dst);return;}auto od=input->GetDesc();od.Usage=Diligent::USAGE_DEFAULT;od.BindFlags=Diligent::BIND_UNORDERED_ACCESS|Diligent::BIND_SHADER_RESOURCE;od.Name="DirectionalGlow/Output";Diligent::RefCntAutoPtr<Diligent::ITexture> output;device_->CreateTexture(od,nullptr,&output);if(!output){applyCPU(src,dst);return;}
        ParamsCB p{threshold_,intensity_,length1_,length2_,weight1_,weight2_,static_cast<float>(static_cast<int>(pattern_)),angleOffset_};void*mapped=nullptr;context_->MapBuffer(paramsCB_,Diligent::MAP_WRITE,Diligent::MAP_FLAG_DISCARD,mapped);if(!mapped){applyCPU(src,dst);return;}std::memcpy(mapped,&p,sizeof(p));context_->UnmapBuffer(paramsCB_,Diligent::MAP_WRITE);if(!executor->setTextureView("g_InputTexture",input->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE))||!executor->setTextureView("g_OutputTexture",output->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))){applyCPU(src,dst);return;}executor->dispatch(context_,ArtifactCore::ComputeExecutor::makeDispatchAttribs(od.Width,od.Height,1,8,8,1),Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);if(!readback(device_,context_,output,dst,"DirectionalGlow/Readback")){applyCPU(src,dst);}
    }

private:
    struct ParamsCB{float threshold,intensity,length1,length2,weight1,weight2,pattern,angleOffset;};
    static constexpr const char* kHlsl=R"(Texture2D<float4> g_InputTexture:register(t0);RWTexture2D<float4> g_OutputTexture:register(u0);cbuffer DirectionalGlowParams:register(b0){float g_Threshold;float g_Intensity;float g_Length1;float g_Length2;float g_Weight1;float g_Weight2;float g_Pattern;float g_AngleOffset;}float4 sampleBright(int2 p,uint2 size){p=clamp(p,int2(0,0),int2(size)-1);float4 v=g_InputTexture[p];float lum=dot(v.rgb,float3(0.114,0.587,0.299));return lum>g_Threshold?v*((lum-g_Threshold)/max(0.001,1.0-g_Threshold)):0;}float3 streak(int2 p,uint2 size,float angle,float len){float2 d=float2(cos(angle),sin(angle));int radius=(int)max(1.0,len*.5);float3 sum=0;float total=0;for(int s=-128;s<=128;++s){if(abs(s)>radius)continue;int2 q=p+int2(round(d*s));float w=exp(-.5*s*s/max(1.0,radius*radius*.25));sum+=sampleBright(q,size).rgb*w;total+=w;}return total>0?sum/total:0;}[numthreads(8,8,1)]void main(uint3 id:SV_DispatchThreadID){uint w,h;g_OutputTexture.GetDimensions(w,h);if(id.x>=w||id.y>=h)return;int2 p=int2(id.xy);float a=g_AngleOffset*0.0174532925;float3 s=streak(p,uint2(w,h),a,g_Length1)*g_Weight1+streak(p,uint2(w,h),a,g_Length2)*g_Weight2;if(g_Pattern>0.5&&g_Pattern<1.5)s+=streak(p,uint2(w,h),a+1.5707963,g_Length1)*g_Weight1+streak(p,uint2(w,h),a+1.5707963,g_Length2)*g_Weight2;if(g_Pattern>1.5&&g_Pattern<2.5)for(int k=1;k<4;k+=1){float ang=a+1.5707963*k*.5;s+=streak(p,uint2(w,h),ang,g_Length1)*g_Weight1+streak(p,uint2(w,h),ang,g_Length2)*g_Weight2;}float4 px=g_InputTexture[id.xy];px.rgb=saturate(px.rgb+s*g_Intensity);g_OutputTexture[id.xy]=px;})";
    bool createTexture(const ImageF32x4RGBAWithCache&src,Diligent::ITexture**out,const char*name){const auto&i=src.image();const float*data=i.rgba32fData();if(!out||!data||i.width()<=0||i.height()<=0)return false;Diligent::TextureDesc d;d.Type=Diligent::RESOURCE_DIM_TEX_2D;d.Width=i.width();d.Height=i.height();d.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT;d.ArraySize=1;d.MipLevels=1;d.SampleCount=1;d.Usage=Diligent::USAGE_IMMUTABLE;d.BindFlags=Diligent::BIND_SHADER_RESOURCE;d.Name=name;Diligent::TextureSubResData sub{};sub.pData=data;sub.Stride=static_cast<Diligent::Uint64>(i.width())*sizeof(float)*4ull;Diligent::TextureData init{};init.pSubResources=&sub;init.NumSubresources=1;device_->CreateTexture(d,&init,out);return *out!=nullptr;}
    static bool readback(Diligent::IRenderDevice*dev,Diligent::IDeviceContext*ctx,Diligent::ITexture*src,ImageF32x4RGBAWithCache&dst,const char*name){if(!dev||!ctx||!src)return false;auto d=src->GetDesc();Diligent::TextureDesc s;s.Type=Diligent::RESOURCE_DIM_TEX_2D;s.Width=d.Width;s.Height=d.Height;s.Format=d.Format;s.ArraySize=1;s.MipLevels=1;s.SampleCount=1;s.Usage=Diligent::USAGE_STAGING;s.CPUAccessFlags=Diligent::CPU_ACCESS_READ;s.Name=name;Diligent::RefCntAutoPtr<Diligent::ITexture>staging;dev->CreateTexture(s,nullptr,&staging);if(!staging)return false;ctx->CopyTexture(Diligent::CopyTextureAttribs(src,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION));ctx->Flush();ctx->WaitForIdle();Diligent::MappedTextureSubresource m{};ctx->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,m);if(!m.pData||!m.Stride)return false;cv::Mat temp((int)d.Height,(int)d.Width,CV_32FC4,m.pData,m.Stride);dst.image().setFromCVMat(temp);ctx->UnmapTextureSubresource(staging,0,0);return true;}
    DirectionalGlowCPUImpl cpuImpl_;
};

DirectionalGlowEffect::DirectionalGlowEffect() {
    setDisplayName(UniString("Directional Glow / Streaks"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<DirectionalGlowCPUImpl>());
    setGPUImpl(std::make_shared<DirectionalGlowGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
}

DirectionalGlowEffect::~DirectionalGlowEffect() = default;

void DirectionalGlowEffect::syncImpls() {
    if (auto cpu = std::dynamic_pointer_cast<DirectionalGlowCPUImpl>(cpuImpl())) {
        cpu->threshold_ = threshold_;
        cpu->intensity_ = intensity_;
        cpu->length1_ = length1_;
        cpu->length2_ = length2_;
        cpu->weight1_ = weight1_;
        cpu->weight2_ = weight2_;
        cpu->pattern_ = pattern_;
        cpu->customAngles_ = customAngles_;
        cpu->angleOffset_ = angleOffset_;
    }
    if (auto gpu = std::dynamic_pointer_cast<DirectionalGlowGPUImpl>(gpuImpl())) {
        gpu->threshold_ = threshold_;
        gpu->intensity_ = intensity_;
        gpu->length1_ = length1_;
        gpu->length2_ = length2_;
        gpu->weight1_ = weight1_;
        gpu->weight2_ = weight2_;
        gpu->pattern_ = pattern_;
        gpu->customAngles_ = customAngles_;
        gpu->angleOffset_ = angleOffset_;
    }
}

std::vector<AbstractProperty> DirectionalGlowEffect::getProperties() const {
    std::vector<AbstractProperty> props;

    AbstractProperty threshProp;
    threshProp.setName("Threshold");
    threshProp.setType(PropertyType::Float);
    threshProp.setValue(threshold_);
    props.push_back(threshProp);

    AbstractProperty intensityProp;
    intensityProp.setName("Intensity");
    intensityProp.setType(PropertyType::Float);
    intensityProp.setValue(intensity_);
    props.push_back(intensityProp);

    AbstractProperty len1Prop;
    len1Prop.setName("Length 1 (Inner)");
    len1Prop.setType(PropertyType::Float);
    len1Prop.setValue(length1_);
    props.push_back(len1Prop);

    AbstractProperty len2Prop;
    len2Prop.setName("Length 2 (Outer)");
    len2Prop.setType(PropertyType::Float);
    len2Prop.setValue(length2_);
    props.push_back(len2Prop);

    AbstractProperty w1Prop;
    w1Prop.setName("Weight 1");
    w1Prop.setType(PropertyType::Float);
    w1Prop.setValue(weight1_);
    props.push_back(w1Prop);

    AbstractProperty w2Prop;
    w2Prop.setName("Weight 2");
    w2Prop.setType(PropertyType::Float);
    w2Prop.setValue(weight2_);
    props.push_back(w2Prop);

    AbstractProperty patternProp;
    patternProp.setName("Pattern");
    patternProp.setType(PropertyType::Integer);
    patternProp.setValue(static_cast<int>(pattern_));
    props.push_back(patternProp);

    AbstractProperty angleProp;
    angleProp.setName("Angle Offset");
    angleProp.setType(PropertyType::Float);
    angleProp.setValue(angleOffset_);
    props.push_back(angleProp);

    return props;
}

void DirectionalGlowEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Threshold")) setThreshold(value.toFloat());
    else if (key == QStringLiteral("Intensity")) setIntensity(value.toFloat());
    else if (key == QStringLiteral("Length 1 (Inner)")) setLength1(value.toFloat());
    else if (key == QStringLiteral("Length 2 (Outer)")) setLength2(value.toFloat());
    else if (key == QStringLiteral("Weight 1")) setWeight1(value.toFloat());
    else if (key == QStringLiteral("Weight 2")) setWeight2(value.toFloat());
    else if (key == QStringLiteral("Pattern")) setPattern(static_cast<StreakPattern>(value.toInt()));
    else if (key == QStringLiteral("Angle Offset")) setAngleOffset(value.toFloat());
}

} // namespace Artifact
