module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>
#include <opencv2/opencv.hpp>

module Artifact.Effect.Rasterizer.PixelSort;

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

class PixelSortCPUImpl : public ArtifactEffectImplBase {
public:
    int sortLen_=16; float sortKey_=0,sortOrder_=1,blend_=0.5f;

    static float keyVal(const float* p,float k){
        if(k<=0.5f)return p[0]*0.299f+p[1]*0.587f+p[2]*0.114f; // luma
        else{float hue=std::atan2(p[1]-p[2],p[0]-p[1]);return hue;} // approximate hue
    }

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        const auto& si=src.image();const float* sd=si.rgba32fData();
        if(!sd||si.width()<=0||!context_.sampler){dst=src;return;}
        const int W=si.width(),H=si.height(),sl=std::clamp(sortLen_,2,64);
        const float b=std::clamp(blend_,0.0f,1.0f);

        ImageF32x4RGBAWithCache prev;bool hp=context_.sampler->sampleCurrentLayerFrameRelative(-1,prev)&&prev.width()>0&&prev.image().rgba32fData();
        dst=src.DeepCopy();float* d=dst.image().rgba32fData();
        if(!hp)return;

        const float* pd=prev.image().rgba32fData();
        const int pw=prev.width(),ph=prev.height(),BS=8;
        const int vw=(W+BS-1)/BS,vh=(H+BS-1)/BS;
        std::vector<float> vx(vw*vh,0),vy(vw*vh,0);

        ArtifactCore::Parallel::For(0,vh,W*H,[&](int by){for(int bx=0;bx<vw;++bx){
            int sx=bx*BS,sy=by*BS,ex=std::min(sx+BS,W),ey=std::min(sy+BS,H);
            float best=1e12f,bdx=0,bdy=0;
            for(int dy=-16;dy<=16;dy+=4)for(int dx=-16;dx<=16;dx+=4){
                float diff=0;int cnt=0;
                for(int y=sy;y<ey;y+=2)for(int x=sx;x<ex;x+=2){
                    int cx=x+dx,cy=y+dy;
                    if((unsigned)cx<(unsigned)pw&&(unsigned)cy<(unsigned)ph){
                        auto*sp=sd+((size_t)y*W+x)*4,*pp=pd+((size_t)cy*pw+cx)*4;
                        float dr=sp[0]-pp[0],dg=sp[1]-pp[1],db=sp[2]-pp[2];
                        diff+=dr*dr+dg*dg+db*db;++cnt;
                    }
                }
                if(cnt>0){diff/=(float)cnt;if(diff<best){best=diff;bdx=(float)(-dx);bdy=(float)(-dy);}}
            }
            vx[by*vw+bx]=bdx;vy[by*vw+bx]=bdy;
        }});

ArtifactCore::Parallel::For(0,H,W*H,[&](int y){int bvY=y/BS;float* o=d+(size_t)y*W*4;
            for(int x=0;x<W;++x){int bvX=x/BS;
                float mx=vx[std::min(bvY,vh-1)*vw+std::min(bvX,vw-1)];
                float my=vy[std::min(bvY,vh-1)*vw+std::min(bvX,vw-1)];
                float mag=std::sqrt(mx*mx+my*my);
                if(mag<1.0f)continue;
                float dx=mx/mag,dy=my/mag;
                // Gather pixels along motion direction
                struct Sample{float val;float r,g,b,a;};
                std::vector<Sample> samples;
                for(int i=-sl;i<=sl;++i){
                    int sx=(int)((float)x+dx*(float)i+0.5f),sy=(int)((float)y+dy*(float)i+0.5f);
                    sx=std::clamp(sx,0,W-1);sy=std::clamp(sy,0,H-1);
                    const float*sp=sd+((size_t)sy*W+sx)*4;
                    samples.push_back({keyVal(sp,sortKey_),sp[0],sp[1],sp[2],sp[3]});
                }
                bool asc=sortOrder_<=0.5f;
                std::sort(samples.begin(),samples.end(),[asc](const Sample& a,const Sample& b){return asc?a.val<b.val:a.val>b.val;});

                float* p=o+(size_t)x*4;
                if(samples.size()>0){
                    auto& mid=samples[samples.size()/2];
                    p[0]=p[0]*(1-b)+mid.r*b;p[1]=p[1]*(1-b)+mid.g*b;
                    p[2]=p[2]*(1-b)+mid.b*b;p[3]=p[3]*(1-b)+mid.a*b;
                }
            }
        });
    }
};

PixelSortEffect::PixelSortEffect():ArtifactAbstractEffect(){setPipelineStage(EffectPipelineStage::Rasterizer);syncImpls();}
PixelSortEffect::~PixelSortEffect()=default;
int PixelSortEffect::sortLength()const{return sortLen_;}
void PixelSortEffect::setSortLength(int v){sortLen_=std::clamp(v,2,64);syncImpls();}
float PixelSortEffect::sortKey()const{return sortKey_;}
void PixelSortEffect::setSortKey(float v){sortKey_=std::clamp(v,0.0f,1.0f);syncImpls();}
float PixelSortEffect::sortOrder()const{return sortOrder_;}
void PixelSortEffect::setSortOrder(float v){sortOrder_=std::clamp(v,0.0f,1.0f);syncImpls();}
float PixelSortEffect::blend()const{return blend_;}
void PixelSortEffect::setBlend(float v){blend_=std::clamp(v,0.0f,1.0f);syncImpls();}
std::vector<AbstractProperty> PixelSortEffect::getProperties()const{
    std::vector<AbstractProperty> props;
    props.reserve(4);

    auto addInt = [&props](const char* name, int value, int minValue, int maxValue) {
        AbstractProperty prop;
        prop.setName(QString::fromLatin1(name));
        prop.setType(PropertyType::Integer);
        const QVariant variantValue(value);
        prop.setValue(variantValue);
        prop.setDefaultValue(variantValue);
        prop.setMinValue(QVariant(minValue));
        prop.setMaxValue(QVariant(maxValue));
        props.push_back(std::move(prop));
    };

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

    addInt("sortLength", sortLen_, 2, 64);
    addFloat("sortKey", sortKey_, 0.0f, 1.0f);
    addFloat("sortOrder", sortOrder_, 0.0f, 1.0f);
    addFloat("blend", blend_, 0.0f, 1.0f);
    return props;
}
void PixelSortEffect::setPropertyValue(const UniString& n,const QVariant& v){
    const QString k=n.toQString();
    if(k=="sortLength")setSortLength(v.toInt());
    else if(k=="sortKey")setSortKey(v.toFloat());
    else if(k=="sortOrder")setSortOrder(v.toFloat());
    else if(k=="blend")setBlend(v.toFloat());
}
void PixelSortEffect::syncImpls(){
    auto c=ArtifactCore::makeShared<PixelSortCPUImpl>();
    c->sortLen_=sortLen_;c->sortKey_=sortKey_;c->sortOrder_=sortOrder_;c->blend_=blend_;setCPUImpl(c);
}

class PixelSortProEffect::Impl {
public:
    float angle = 0.0f;
    int maxSegment = 180;
    float lowThreshold = 0.18f;
    float highThreshold = 0.92f;
    float keyMode = 0.0f;
    float descending = 0.0f;
    float edgeProtection = 0.35f;
    float mix = 1.0f;
};

namespace {

float pixelSortProKey(const cv::Vec4f& pixel, float mode) {
    if (mode < 0.5f) {
        return pixel[0] * 0.2126f + pixel[1] * 0.7152f + pixel[2] * 0.0722f;
    }
    const float maximum = std::max({pixel[0], pixel[1], pixel[2]});
    const float minimum = std::min({pixel[0], pixel[1], pixel[2]});
    const float delta = maximum - minimum;
    if (delta <= 0.00001f) return 0.0f;
    float hue = 0.0f;
    if (maximum == pixel[0]) hue = (pixel[1] - pixel[2]) / delta;
    else if (maximum == pixel[1]) hue = 2.0f + (pixel[2] - pixel[0]) / delta;
    else hue = 4.0f + (pixel[0] - pixel[1]) / delta;
    hue /= 6.0f;
    if (hue < 0.0f) hue += 1.0f;
    return hue;
}

} // namespace

PixelSortProEffect::PixelSortProEffect() : impl_(new Impl()) {
    setEffectID(ArtifactCore::UniString("builtin.pixel_sort_pro"));
    setDisplayName(ArtifactCore::UniString("Pixel Sort Pro"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setAllowOverscan(true);
}

PixelSortProEffect::~PixelSortProEffect() {
    delete impl_;
    impl_ = nullptr;
}

void PixelSortProEffect::apply(const ImageF32x4RGBAWithCache& src,
                               ImageF32x4RGBAWithCache& dst) {
    const auto& image = src.image();
    const int width = image.width();
    const int height = image.height();
    const float* pixels = image.rgba32fData();
    if (!pixels || width <= 0 || height <= 0) {
        dst = src;
        return;
    }

    cv::Mat source(height, width, CV_32FC4, const_cast<float*>(pixels));
    const cv::Point2f center(width * 0.5f, height * 0.5f);
    const cv::Mat rotation = cv::getRotationMatrix2D(center, -impl_->angle, 1.0);
    cv::Mat aligned;
    cv::warpAffine(source, aligned, rotation, source.size(), cv::INTER_LINEAR,
                   cv::BORDER_REFLECT_101);
    cv::Mat sorted = aligned.clone();

    for (int y = 0; y < height; ++y) {
        const cv::Vec4f* inputRow = aligned.ptr<cv::Vec4f>(y);
        cv::Vec4f* outputRow = sorted.ptr<cv::Vec4f>(y);
        int x = 0;
        while (x < width) {
            const float key = pixelSortProKey(inputRow[x], impl_->keyMode);
            if (key < impl_->lowThreshold || key > impl_->highThreshold) {
                ++x;
                continue;
            }
            const int start = x;
            while (x < width && x - start < impl_->maxSegment) {
                const float runKey = pixelSortProKey(inputRow[x], impl_->keyMode);
                if (runKey < impl_->lowThreshold || runKey > impl_->highThreshold) break;
                ++x;
            }
            const int end = x;
            if (end - start < 2) continue;
            std::vector<cv::Vec4f> segment(inputRow + start, inputRow + end);
            const bool descending = impl_->descending >= 0.5f;
            std::stable_sort(segment.begin(), segment.end(), [&](const auto& a, const auto& b) {
                const float aKey = pixelSortProKey(a, impl_->keyMode);
                const float bKey = pixelSortProKey(b, impl_->keyMode);
                return descending ? aKey > bKey : aKey < bKey;
            });
            for (int i = start; i < end; ++i) {
                const cv::Vec4f original = inputRow[i];
                const cv::Vec4f sortedPixel = segment[static_cast<std::size_t>(i - start)];
                const float leftKey = pixelSortProKey(inputRow[std::max(0, i - 1)], 0.0f);
                const float rightKey = pixelSortProKey(inputRow[std::min(width - 1, i + 1)], 0.0f);
                const float edge = std::clamp(std::abs(rightKey - leftKey) * 4.0f, 0.0f, 1.0f);
                const float amount = impl_->mix *
                    (1.0f - edge * impl_->edgeProtection);
                outputRow[i] = original * (1.0f - amount) + sortedPixel * amount;
            }
        }
    }

    cv::Mat restored;
    const cv::Mat inverse = cv::getRotationMatrix2D(center, impl_->angle, 1.0);
    cv::warpAffine(sorted, restored, inverse, source.size(), cv::INTER_LINEAR,
                   cv::BORDER_REFLECT_101);
    auto result = image.DeepCopy();
    result.setFromRGBA32F(restored.ptr<float>(), width, height);
    float* resultPixels = result.rgba32fData();
    if (resultPixels) {
        for (std::size_t pixel = 0; pixel < image.totalPixels(); ++pixel) {
            resultPixels[pixel * 4u + 3u] = pixels[pixel * 4u + 3u];
        }
    }
    result.setColorDescriptor(image.colorDescriptor());
    dst = ImageF32x4RGBAWithCache(result);
}

std::vector<AbstractProperty> PixelSortProEffect::getProperties() const {
    std::vector<AbstractProperty> properties;
    properties.reserve(8);
    auto addFloat = [&](const char* name, const char* label, float value,
                        float minimum, float maximum) {
        auto& property = properties.emplace_back();
        property.setName(QString::fromUtf8(name));
        property.setDisplayLabel(QString::fromUtf8(label));
        property.setType(PropertyType::Float);
        property.setValue(value);
        property.setDefaultValue(value);
        property.setHardRange(minimum, maximum);
        property.setAnimatable(true);
    };
    addFloat("angle", "Direction", impl_->angle, -180.0f, 180.0f);
    auto& segment = properties.emplace_back();
    segment.setName(QStringLiteral("maxSegment"));
    segment.setDisplayLabel(QStringLiteral("Maximum Segment"));
    segment.setType(PropertyType::Integer);
    segment.setValue(impl_->maxSegment);
    segment.setDefaultValue(180);
    segment.setHardRange(2, 2048);
    addFloat("lowThreshold", "Low Threshold", impl_->lowThreshold, 0.0f, 4.0f);
    addFloat("highThreshold", "High Threshold", impl_->highThreshold, 0.0f, 4.0f);
    addFloat("keyMode", "Sort Key (Luma / Hue)", impl_->keyMode, 0.0f, 1.0f);
    addFloat("descending", "Sort Order", impl_->descending, 0.0f, 1.0f);
    addFloat("edgeProtection", "Edge Protection", impl_->edgeProtection, 0.0f, 1.0f);
    addFloat("mix", "Mix", impl_->mix, 0.0f, 1.0f);
    return properties;
}

void PixelSortProEffect::setPropertyValue(const UniString& name,
                                          const QVariant& value) {
    const QString key = name.toQString();
    const float raw = value.toFloat();
    const float number = std::isfinite(raw) ? raw : 0.0f;
    if (key == QStringLiteral("angle")) impl_->angle = std::clamp(number, -180.0f, 180.0f);
    else if (key == QStringLiteral("maxSegment")) impl_->maxSegment = std::clamp(value.toInt(), 2, 2048);
    else if (key == QStringLiteral("lowThreshold")) impl_->lowThreshold = std::clamp(number, 0.0f, 4.0f);
    else if (key == QStringLiteral("highThreshold")) impl_->highThreshold = std::clamp(number, impl_->lowThreshold, 4.0f);
    else if (key == QStringLiteral("keyMode")) impl_->keyMode = std::clamp(number, 0.0f, 1.0f);
    else if (key == QStringLiteral("descending")) impl_->descending = std::clamp(number, 0.0f, 1.0f);
    else if (key == QStringLiteral("edgeProtection")) impl_->edgeProtection = std::clamp(number, 0.0f, 1.0f);
    else if (key == QStringLiteral("mix")) impl_->mix = std::clamp(number, 0.0f, 1.0f);
    else ArtifactAbstractEffect::setPropertyValue(name, value);
}

EffectROIHint PixelSortProEffect::roiHint() const {
    return EffectROIHint{
        .kind = EffectROIHintKind::Displacement,
        .expansionPixels = static_cast<float>(impl_->maxSegment),
        .requiresFullFrame = true
    };
}
} // namespace Artifact
