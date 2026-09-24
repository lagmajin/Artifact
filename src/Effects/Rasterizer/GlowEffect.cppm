module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <QString>
#include <QVariant>
#include <opencv2/opencv.hpp>

module Artifact.Effect.Rasterizer.Glow;

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;
import Core.Parallel;
import Memory.SharedPtr;

namespace Artifact {
using namespace ArtifactCore;

namespace {

// Resident-path shader. P0 threshold, P1 radius, P2 intensity. This body
// relies on the pipeline's ResidentGenericParams b0 prelude and has no CPU
// readback; the CPU implementation remains the fallback and parity oracle.
static constexpr const char* kRasterizerGlowResidentHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);

float4 glowSampleClamped(int2 position, uint width, uint height)
{
    return g_InputTexture[uint2(clamp(position, int2(0, 0),
        int2(width - 1, height - 1)))];
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
    uint width, height;
    g_OutputTexture.GetDimensions(width, height);
    if (dispatchId.x >= width || dispatchId.y >= height) return;

    const float4 base = g_InputTexture[dispatchId.xy];
    const int radius = clamp((int)g_P1, 1, 4);
    const float sigma = max(g_P1 * 0.3, 0.001);
    const float denominator = max(1.0 - g_P0, 0.0001);
    float3 blurredBright = 0.0;
    float totalWeight = 0.0;
    for (int y = -radius; y <= radius; ++y) {
        for (int x = -radius; x <= radius; ++x) {
            const float distanceSquared = float(x * x + y * y);
            const float weight = exp(-distanceSquared / (2.0 * sigma * sigma));
            const float4 sample = glowSampleClamped(
                int2(dispatchId.xy) + int2(x, y), width, height);
            const float luminance = dot(sample.rgb, float3(0.299, 0.587, 0.114));
            const float contribution = luminance >= g_P0
                ? saturate((luminance - g_P0) / denominator) : 0.0;
            blurredBright += sample.rgb * contribution * weight;
            totalWeight += weight;
        }
    }
    const float3 result = saturate(base.rgb +
        blurredBright / max(totalWeight, 0.0001) * g_P2);
    g_OutputTexture[dispatchId.xy] = float4(result, base.a);
}
)";

} // namespace

class GlowCPUImpl : public ArtifactEffectImplBase {
public:
    float threshold_=0.5f,radius_=20.0f,intensity_=1.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        const auto& si=src.image();const float* sd=si.rgba32fData();
        if(!sd||si.width()<=0){dst=src;return;}
        const int W=si.width(),H=si.height();
        const float th=std::clamp(threshold_,0.0f,1.0f),rad=std::max(radius_,1.0f);
        const float it=std::clamp(intensity_,0.0f,5.0f);
        cv::Mat srcM(H,W,CV_32FC4,(void*)sd);
        cv::Mat bright; srcM.copyTo(bright);
Parallel::For(0,H,W*H,[&](int y){auto* r=bright.ptr<cv::Vec4f>(y);for(int x=0;x<W;++x){
            float l=r[x][0]*0.299f+r[x][1]*0.587f+r[x][2]*0.114f;
            if(l<th)r[x]=cv::Vec4f(0,0,0,0);else{float m=(l-th)/(1.0f-th);r[x]*=m;}}});
        cv::Mat blur; int kr=std::min((int)rad,128);
        cv::GaussianBlur(bright,blur,cv::Size(kr*2+1,kr*2+1),rad*0.3f);
        dst=src.DeepCopy();float* d=dst.image().rgba32fData();
Parallel::For(0,H,W*H,[&](int y){auto* br=blur.ptr<cv::Vec4f>(y);float* o=d+(size_t)y*W*4;
            for(int x=0;x<W;++x){float* p=o+(size_t)x*4;
                p[0]=std::clamp(p[0]+br[x][0]*it,0.0f,1.0f);p[1]=std::clamp(p[1]+br[x][1]*it,0.0f,1.0f);
                p[2]=std::clamp(p[2]+br[x][2]*it,0.0f,1.0f);}});
    }
};

RasterizerGlowEffect::RasterizerGlowEffect():ArtifactAbstractEffect(){
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setComputeMode(ComputeMode::AUTO);
    registerGpuGenericShader(
        RasterizerGlowEffect::kGpuGenericKey,
        GpuGenericShaderRecord{
            kRasterizerGlowResidentHlsl, "main", GpuGenericResourceKind::Filter});
    syncImpls();
}
RasterizerGlowEffect::~RasterizerGlowEffect()=default;
float RasterizerGlowEffect::threshold()const{return threshold_;}void RasterizerGlowEffect::setThreshold(float v){threshold_=std::clamp(v,0.0f,1.0f);syncImpls();}
float RasterizerGlowEffect::radius()const{return radius_;}void RasterizerGlowEffect::setRadius(float v){radius_=std::max(v,1.0f);syncImpls();}
float RasterizerGlowEffect::intensity()const{return intensity_;}void RasterizerGlowEffect::setIntensity(float v){intensity_=std::clamp(v,0.0f,5.0f);syncImpls();}
std::vector<AbstractProperty> RasterizerGlowEffect::getProperties()const{
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

    addFloat("threshold", threshold_, 0.0f, 1.0f);
    addFloat("radius", radius_, 1.0f, 200.0f);
    addFloat("intensity", intensity_, 0.0f, 5.0f);
    return props;
}
void RasterizerGlowEffect::setPropertyValue(const UniString& n,const QVariant& v){const QString k=n.toQString();if(k=="threshold")setThreshold(v.toFloat());else if(k=="radius")setRadius(v.toFloat());else if(k=="intensity")setIntensity(v.toFloat());}
void RasterizerGlowEffect::syncImpls(){auto c=ArtifactCore::makeShared<GlowCPUImpl>();c->threshold_=threshold_;c->radius_=radius_;c->intensity_=intensity_;setCPUImpl(c);}
} // namespace Artifact
