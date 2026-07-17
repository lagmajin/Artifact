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

module Artifact.Effect.Rasterizer.Mosaic;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;
import Core.Parallel;

namespace Artifact {

using namespace ArtifactCore;

class MosaicEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float cellSize_ = 8.0f;
    bool shapeMode_ = false;

    static cv::Vec4f avgColor(const cv::Mat& mat, int x0, int y0, int x1, int y1) {
        cv::Vec4f sum(0, 0, 0, 0);
        int count = 0;
        for (int y = y0; y < y1 && y < mat.rows; ++y) {
            for (int x = x0; x < x1 && x < mat.cols; ++x) {
                sum += mat.at<cv::Vec4f>(y, x);
                ++count;
            }
        }
        if (count > 0) sum /= static_cast<float>(count);
        return sum;
    }

    void fillRect(cv::Mat& mat, int x0, int y0, int x1, int y1, const cv::Vec4f& color) {
        for (int y = y0; y < y1 && y < mat.rows; ++y) {
            for (int x = x0; x < x1 && x < mat.cols; ++x) {
                mat.at<cv::Vec4f>(y, x) = color;
            }
        }
    }

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        auto& srcImage = src.image();
        const float* pixels = srcImage.rgba32fData();
        if (!pixels) {
            dst = src;
            return;
        }
        const int w = srcImage.width();
        const int h = srcImage.height();
        dst = src;
        cv::Mat mat(dst.image().height(), dst.image().width(), CV_32FC4, dst.image().rgba32fData());

        const int cell = std::max(1, static_cast<int>(cellSize_));
        // cache: key=cell origin, value=average color
        // lazy recompute each cell (cheap for small images)
        const int tileRows = (h + cell - 1) / cell;
        ArtifactCore::Parallel::For(0, tileRows, [&](int tileRow) {
            const int by = tileRow * cell;
            for (int bx = 0; bx < w; bx += cell) {
                const int x1 = std::min(bx + cell, w);
                const int y1 = std::min(by + cell, h);
                if (shapeMode_) {
                    // Diamond shape sample: Manhattan distance center
                    const int cx = (bx + x1) / 2;
                    const int cy = (by + y1) / 2;
                    const int radius = std::min(x1 - bx, y1 - by) / 2;
                    cv::Vec4f sum(0, 0, 0, 0);
                    int count = 0;
                    // diamond loop
                    for (int y = by; y < y1; ++y) {
                        for (int x = bx; x < x1; ++x) {
                            const int adx = std::abs(x - cx);
                            const int ady = std::abs(y - cy);
                            if (adx + ady <= radius) {
                                sum += mat.at<cv::Vec4f>(y, x);
                                ++count;
                            }
                        }
                    }
                    if (count > 0) sum /= static_cast<float>(count);
                    // fill diamond shape with averaged color
                    for (int y = by; y < y1; ++y) {
                        for (int x = bx; x < x1; ++x) {
                            const int adx = std::abs(x - cx);
                            const int ady = std::abs(y - cy);
                            if (adx + ady <= radius) {
                                mat.at<cv::Vec4f>(y, x) = sum;
                            }
                        }
                    }
                } else {
                    const cv::Vec4f c = avgColor(mat, bx, by, x1, y1);
                    fillRect(mat, bx, by, x1, y1, c);
                }
            }
        });
    }
};

class MosaicEffectGPUImpl : public ArtifactEffectImplBase {
public:
    float cellSize_ = 8.0f;
    bool shapeMode_ = false;
    mutable Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
    mutable Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context_;
    mutable Diligent::RefCntAutoPtr<Diligent::IBuffer> paramsCB_;
    mutable bool pipelineReady_ = false;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }
    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        if (!acquireSharedRenderDeviceForCurrentBackend(device_, context_)) { applyCPU(src,dst); return; }
        auto gpuContext = std::make_unique<ArtifactCore::GpuContext>(device_, context_);
        auto executor = std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext);
        if (!paramsCB_) { Diligent::BufferDesc cbDesc; cbDesc.Name="Mosaic/ParamsCB"; cbDesc.Size=sizeof(ParamsCB); cbDesc.Usage=Diligent::USAGE_DYNAMIC; cbDesc.BindFlags=Diligent::BIND_UNIFORM_BUFFER; cbDesc.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE; device_->CreateBuffer(cbDesc,nullptr,&paramsCB_); }
        if (!paramsCB_) { applyCPU(src,dst); return; }
        static Diligent::ShaderResourceVariableDesc vars[] = {
            {Diligent::SHADER_TYPE_COMPUTE, "MosaicParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };
        if (!pipelineReady_) { ArtifactCore::ComputePipelineDesc desc; desc.name="Mosaic/PSO"; desc.shaderSource=kMosaicHlsl; desc.entryPoint="main"; desc.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL; desc.variables=vars; desc.variableCount=3; desc.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC; if (!executor->build(desc) || !executor->createShaderResourceBinding(true) || !executor->setBuffer("MosaicParams", paramsCB_)) { applyCPU(src,dst); return; } pipelineReady_=true; }
        Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex; if (!createTextureFromImage(src, device_, &inputTex, "Mosaic/InputTexture")) { applyCPU(src,dst); return; }
        Diligent::TextureDesc outDesc=inputTex->GetDesc(); outDesc.Usage=Diligent::USAGE_DEFAULT; outDesc.BindFlags=Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE; outDesc.Name="Mosaic/OutputTexture"; Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex; device_->CreateTexture(outDesc,nullptr,&outputTex); if (!outputTex) { applyCPU(src,dst); return; }
        void* mapped=nullptr; context_->MapBuffer(paramsCB_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, mapped); if (!mapped) { applyCPU(src,dst); return; }
        ParamsCB params{}; params.cellSize=cellSize_; params.shapeMode=shapeMode_?1.0f:0.0f; std::memcpy(mapped,&params,sizeof(params)); context_->UnmapBuffer(paramsCB_, Diligent::MAP_WRITE);
        if (!executor->setTextureView("g_InputTexture", inputTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) || !executor->setTextureView("g_OutputTexture", outputTex->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) { applyCPU(src,dst); return; }
        auto attribs=ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width,outDesc.Height,1,8,8,1); executor->dispatch(context_, attribs, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        if (!readbackTexture(device_, context_, outputTex, dst, "Mosaic/StagingTexture")) { applyCPU(src,dst); return; }
    }
public:
    MosaicEffectCPUImpl cpuImpl_;

private:
    struct ParamsCB { float cellSize=8.0f; float shapeMode=0.0f; float pad[2]{}; };
    static constexpr const char* kMosaicHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0); RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer MosaicParams : register(b0){ float g_CellSize; float g_ShapeMode; float2 g_Pad; };
[numthreads(8,8,1)] void main(uint3 dtid:SV_DispatchThreadID){ uint w,h; g_OutputTexture.GetDimensions(w,h); if(dtid.x>=w||dtid.y>=h) return; int cell=max(1,(int)g_CellSize); int2 p=int2(dtid.xy); int2 cellOrigin=int2((p.x/cell)*cell, (p.y/cell)*cell); int2 cellMax=int2(min(cellOrigin.x+cell,(int)w), min(cellOrigin.y+cell,(int)h)); float4 sum=0; int count=0; for(int y=cellOrigin.y;y<cellMax.y;++y){ for(int x=cellOrigin.x;x<cellMax.x;++x){ sum += g_InputTexture[int2(x,y)]; count++; }} float4 avg = sum / max(count,1); if(g_ShapeMode > 0.5f){ int2 c=(cellOrigin+cellMax)/2; int radius=max(1,min(cellMax.x-cellOrigin.x, cellMax.y-cellOrigin.y)/2); if(abs(p.x-c.x)+abs(p.y-c.y) <= radius) g_OutputTexture[dtid.xy]=avg; else g_OutputTexture[dtid.xy]=g_InputTexture[dtid.xy]; } else { g_OutputTexture[dtid.xy]=avg; } }
)";
    static bool createTextureFromImage(const ImageF32x4RGBAWithCache& src, Diligent::IRenderDevice* device, Diligent::ITexture** outTex, const char* name){ const auto& img=src.image(); const float* data=img.rgba32fData(); if(!device||!outTex||!data||img.width()<=0||img.height()<=0) return false; Diligent::TextureDesc desc; desc.Type=Diligent::RESOURCE_DIM_TEX_2D; desc.Width=img.width(); desc.Height=img.height(); desc.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT; desc.ArraySize=1; desc.MipLevels=1; desc.SampleCount=1; desc.Usage=Diligent::USAGE_IMMUTABLE; desc.BindFlags=Diligent::BIND_SHADER_RESOURCE; desc.Name=name; Diligent::TextureSubResData sub{}; sub.pData=data; sub.Stride=static_cast<Diligent::Uint64>(img.width())*sizeof(float)*4ull; Diligent::TextureData init{}; init.pSubResources=&sub; init.NumSubresources=1; device->CreateTexture(desc,&init,outTex); return *outTex!=nullptr; }
    static bool readbackTexture(Diligent::IRenderDevice* device, Diligent::IDeviceContext* ctx, Diligent::ITexture* src, ImageF32x4RGBAWithCache& dst, const char* name){ if(!device||!ctx||!src) return false; const auto desc=src->GetDesc(); Diligent::TextureDesc stagingDesc; stagingDesc.Type=Diligent::RESOURCE_DIM_TEX_2D; stagingDesc.Width=desc.Width; stagingDesc.Height=desc.Height; stagingDesc.Format=desc.Format; stagingDesc.ArraySize=1; stagingDesc.MipLevels=1; stagingDesc.SampleCount=1; stagingDesc.Usage=Diligent::USAGE_STAGING; stagingDesc.CPUAccessFlags=Diligent::CPU_ACCESS_READ; stagingDesc.Name=name; Diligent::RefCntAutoPtr<Diligent::ITexture> staging; device->CreateTexture(stagingDesc,nullptr,&staging); if(!staging) return false; Diligent::CopyTextureAttribs copy(src,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION); ctx->CopyTexture(copy); Diligent::MappedTextureSubresource mapped{}; ctx->Flush(); ctx->WaitForIdle(); ctx->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,mapped); if(!mapped.pData||mapped.Stride==0) return false; cv::Mat temp(static_cast<int>(desc.Height), static_cast<int>(desc.Width), CV_32FC4, mapped.pData, mapped.Stride); dst.image().setFromCVMat(temp); ctx->UnmapTextureSubresource(staging,0,0); return true; }
};

MosaicEffect::MosaicEffect() {
    setDisplayName(UniString("Mosaic"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<MosaicEffectCPUImpl>());
    setGPUImpl(std::make_shared<MosaicEffectGPUImpl>());
}
MosaicEffect::~MosaicEffect() = default;

float MosaicEffect::cellSize() const { return cellSize_; }
void MosaicEffect::setCellSize(float v) { cellSize_ = std::max(1.0f, v); syncImpls(); }
bool MosaicEffect::shapeMode() const { return shapeMode_; }
void MosaicEffect::setShapeMode(bool v) { shapeMode_ = v; syncImpls(); }

void MosaicEffect::syncImpls() {
    if (auto* c = dynamic_cast<MosaicEffectCPUImpl*>(cpuImpl().get())) {
        c->cellSize_ = cellSize_;
        c->shapeMode_ = shapeMode_;
    }
    if (auto* g = dynamic_cast<MosaicEffectGPUImpl*>(gpuImpl().get())) {
        g->cpuImpl_.cellSize_ = cellSize_;
        g->cpuImpl_.shapeMode_ = shapeMode_;
    }
}

std::vector<AbstractProperty> MosaicEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    auto& c = props.emplace_back(); c.setName("Cell Size"); c.setType(PropertyType::Float); c.setValue(cellSize_);
    auto& s = props.emplace_back(); s.setName("Shape Mode"); s.setType(PropertyType::Boolean); s.setValue(shapeMode_);
    return props;
}

void MosaicEffect::setPropertyValue(const UniString& n, const QVariant& v) {
    const QString k = n.toQString();
    if (k == "Cell Size") setCellSize(v.toFloat());
    else if (k == "Shape Mode") setShapeMode(v.toBool());
}

} // namespace Artifact
