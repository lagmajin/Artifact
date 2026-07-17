module;
#include <utility>
#include <vector>
#include <cmath>
#include <random>
#include <algorithm>
#include <cstring>
#include <opencv2/opencv.hpp>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module Artifact.Effect.Creative;

import Artifact.Effect.Abstract;
import Image.ImageF32x4RGBAWithCache;
import Math.Noise;
import Math.Random;
import Core.Parallel;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;

namespace Artifact {

namespace {

bool runCreativeCompute(const ImageF32x4RGBAWithCache& src,
                        ImageF32x4RGBAWithCache& dst,
                        const char* label,
                        const char* hlsl) {
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;
    if (!acquireSharedRenderDeviceForCurrentBackend(device, context)) return false;
    const auto& image = src.image();
    const float* pixels = image.rgba32fData();
    if (!pixels || image.width() <= 0 || image.height() <= 0) return false;

    Diligent::TextureDesc inputDesc;
    inputDesc.Name = label;
    inputDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    inputDesc.Width = image.width(); inputDesc.Height = image.height();
    inputDesc.Format = Diligent::TEX_FORMAT_RGBA32_FLOAT;
    inputDesc.MipLevels = 1; inputDesc.ArraySize = 1; inputDesc.SampleCount = 1;
    inputDesc.Usage = Diligent::USAGE_IMMUTABLE;
    inputDesc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    Diligent::TextureSubResData inputData{};
    inputData.pData = pixels;
    inputData.Stride = static_cast<Diligent::Uint64>(image.width()) * sizeof(float) * 4ull;
    Diligent::TextureData textureData{};
    textureData.pSubResources = &inputData;
    textureData.NumSubresources = 1;
    Diligent::RefCntAutoPtr<Diligent::ITexture> input;
    device->CreateTexture(inputDesc, &textureData, &input);
    if (!input) return false;

    Diligent::TextureDesc outputDesc = inputDesc;
    outputDesc.Name = label;
    outputDesc.Usage = Diligent::USAGE_DEFAULT;
    outputDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE;
    Diligent::RefCntAutoPtr<Diligent::ITexture> output;
    device->CreateTexture(outputDesc, nullptr, &output);
    if (!output) return false;

    static Diligent::ShaderResourceVariableDesc vars[] = {
        {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};
    ArtifactCore::GpuContext gpuContext{device, context};
    ArtifactCore::ComputeExecutor executor{gpuContext};
    ArtifactCore::ComputePipelineDesc pipeline{};
    pipeline.name = label; pipeline.shaderSource = hlsl; pipeline.entryPoint = "main";
    pipeline.sourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
    pipeline.variables = vars; pipeline.variableCount = 2;
    pipeline.defaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    if (!executor.build(pipeline) || !executor.createShaderResourceBinding(true) ||
        !executor.setTextureView("g_InputTexture", input->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) ||
        !executor.setTextureView("g_OutputTexture", output->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) return false;
    executor.dispatch(context, ArtifactCore::ComputeExecutor::makeDispatchAttribs(outputDesc.Width, outputDesc.Height, 1, 8, 8, 1), Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    Diligent::TextureDesc stagingDesc = outputDesc;
    stagingDesc.Name = label; stagingDesc.Usage = Diligent::USAGE_STAGING;
    stagingDesc.BindFlags = Diligent::BIND_NONE; stagingDesc.CPUAccessFlags = Diligent::CPU_ACCESS_READ;
    Diligent::RefCntAutoPtr<Diligent::ITexture> staging;
    device->CreateTexture(stagingDesc, nullptr, &staging);
    if (!staging) return false;
    Diligent::CopyTextureAttribs copy(
        output, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
        staging, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    context->CopyTexture(copy);
    context->Flush(); context->WaitForIdle();
    Diligent::MappedTextureSubresource mapped{};
    context->MapTextureSubresource(staging, 0, 0, Diligent::MAP_READ, Diligent::MAP_FLAG_NONE, nullptr, mapped);
    if (!mapped.pData || !mapped.Stride) return false;
    cv::Mat result(static_cast<int>(outputDesc.Height), static_cast<int>(outputDesc.Width), CV_32FC4, mapped.pData, mapped.Stride);
    dst.image().setFromCVMat(result);
    context->UnmapTextureSubresource(staging, 0, 0);
    return true;
}

constexpr char kGlitchComputeHlsl[] = R"(
Texture2D<float4> g_InputTexture : register(t0); RWTexture2D<float4> g_OutputTexture : register(u0);
uint hash(uint v) { v ^= v >> 16; v *= 0x7feb352du; v ^= v >> 15; v *= 0x846ca68bu; return v ^ (v >> 16); }
float random01(uint x, uint y, uint salt) { return (hash(x * 73856093u ^ y * 19349663u ^ salt) & 0x00ffffffu) / 16777215.0f; }
[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) { uint w,h; g_OutputTexture.GetDimensions(w,h); if(id.x>=w||id.y>=h)return; float rowOffset=(id.y%15u<4u)?(random01(0u,id.y,1u)*24.0f-12.0f):0.0f; float shift=3.0f+random01(0u,id.y,2u)*5.0f; int sx=clamp((int)id.x+(int)rowOffset,0,(int)w-1); int rx=clamp(sx+(int)shift,0,(int)w-1); int bx=clamp(sx-(int)shift,0,(int)w-1); float4 c=g_InputTexture[uint2(sx,id.y)]; g_OutputTexture[id.xy]=float4(g_InputTexture[uint2(rx,id.y)].r,c.g,g_InputTexture[uint2(bx,id.y)].b,c.a); })";

constexpr char kOldTVComputeHlsl[] = R"(
Texture2D<float4> g_InputTexture : register(t0); RWTexture2D<float4> g_OutputTexture : register(u0);
uint hash(uint v) { v ^= v >> 16; v *= 0x7feb352du; v ^= v >> 15; v *= 0x846ca68bu; return v ^ (v >> 16); }
float random01(uint x,uint y,uint salt) { return (hash(x*73856093u ^ y*19349663u ^ salt) & 0x00ffffffu) / 16777215.0f; }
[numthreads(8,8,1)] void main(uint3 id:SV_DispatchThreadID) { uint w,h;g_OutputTexture.GetDimensions(w,h);if(id.x>=w||id.y>=h)return; float scanline=(id.y%4u==0u)?0.7f:1.0f; float jitter=random01(0u,id.y,17u)<0.08f?(random01(0u,id.y,18u)*10.0f-5.0f):0.0f; int sx=clamp((int)id.x+(int)jitter,0,(int)w-1); float4 c=g_InputTexture[uint2(sx,id.y)]; float noise=(random01(id.x,id.y,19u)*2.0f-1.0f)*0.005f; g_OutputTexture[id.xy]=float4(saturate(c.rgb*scanline+noise),c.a); })";

constexpr char kHalftoneComputeHlsl[] = R"(
Texture2D<float4> g_InputTexture : register(t0); RWTexture2D<float4> g_OutputTexture : register(u0);
[numthreads(8,8,1)] void main(uint3 id:SV_DispatchThreadID) { uint w,h;g_OutputTexture.GetDimensions(w,h);if(id.x>=w||id.y>=h)return; uint bx=(id.x/8u)*8u,by=(id.y/8u)*8u; float lum=0;uint count=0;[unroll]for(uint dy=0;dy<8u;++dy){[unroll]for(uint dx=0;dx<8u;++dx){uint x=bx+dx,y=by+dy;if(x<w&&y<h){float3 c=g_InputTexture[uint2(x,y)].rgb;lum+=(c.r+c.g+c.b)/3.0f;++count;}}}lum/=max(1u,count);float2 d=float2(id.xy)-float2(bx+4u,by+4u);float radius=4.0f*lum;float v=length(d)<radius?0.0f:1.0f;g_OutputTexture[id.xy]=float4(v,v,v,1); })";

} // namespace

ArtifactGlitchEffect::ArtifactGlitchEffect() {
    setDisplayName("Glitch");
    setEffectID("builtin.glitch");
    setPipelineStage(EffectPipelineStage::Rasterizer);
}

void ArtifactGlitchEffect::apply(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    if (computeMode() != ComputeMode::CPU && runCreativeCompute(src, dst, "CreativeGlitch", kGlitchComputeHlsl)) {
        return;
    }
    int w = src.width();
    int h = src.height();
    auto srcImage = src.image();
    auto dstImage = srcImage.DeepCopy();

    ArtifactCore::RandomStream rng(0x474C49544348ull);
    
    // Each row owns a disjoint destination range.  Keep the per-row RNG fork so
    // CPU and future GPU/reference comparisons remain deterministic.
    ArtifactCore::Parallel::For(0, h, [&](int y) {
        auto rowRng = rng.fork(static_cast<uint64_t>(y));
        float rowOffset = 0.0f;
        if (y % 15 < 4) {
            rowOffset = rowRng.range(-12.0f, 12.0f);
        }
        float shiftX = 3.0f + rowRng.range(0.0f, 5.0f);
        
        for (int x = 0; x < w; ++x) {
            int sx = std::clamp(x + (int)rowOffset, 0, w - 1);
            auto c = srcImage.getPixel(sx, y);
            
            int rsx = std::clamp(sx + (int)shiftX, 0, w - 1);
            auto cr = srcImage.getPixel(rsx, y);
            
            int bsx = std::clamp(sx - (int)shiftX, 0, w - 1);
            auto cb = srcImage.getPixel(bsx, y);
            
            dstImage.setPixel(x, y, {cr.r(), c.g(), cb.b(), c.a()});
        }
    });
    dst = ImageF32x4RGBAWithCache(dstImage);
}

ArtifactHalftoneEffect::ArtifactHalftoneEffect() {
    setDisplayName("Halftone");
    setEffectID("builtin.halftone");
    setPipelineStage(EffectPipelineStage::Rasterizer);
}

void ArtifactHalftoneEffect::apply(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    if (computeMode() != ComputeMode::CPU && runCreativeCompute(src, dst, "CreativeHalftone", kHalftoneComputeHlsl)) {
        return;
    }
    int w = src.width();
    int h = src.height();
    auto srcImage = src.image();
    auto dstImage = srcImage.DeepCopy();
    
    int dotSize = 8;
    
    const int tileRows = (h + dotSize - 1) / dotSize;
    // A tile row never overlaps another tile row in dstImage.
    ArtifactCore::Parallel::For(0, tileRows, [&](int tileRow) {
        const int y = tileRow * dotSize;
        for (int x = 0; x < w; x += dotSize) {
            float lum = 0;
            int count = 0;
            for (int dy = 0; dy < dotSize && y + dy < h; ++dy) {
                for (int dx = 0; dx < dotSize && x + dx < w; ++dx) {
                    auto c = srcImage.getPixel(x + dx, y + dy);
                    lum += (c.r() + c.g() + c.b()) / 3.0f;
                    count++;
                }
            }
            lum /= (count > 0 ? count : 1);
            
            float radius = (dotSize / 2.0f) * lum;
            float cx = x + dotSize / 2.0f;
            float cy = y + dotSize / 2.0f;
            
            for (int dy = 0; dy < dotSize && y + dy < h; ++dy) {
                for (int dx = 0; dx < dotSize && x + dx < w; ++dx) {
                    float dist = std::sqrt((x + dx - cx)*(x + dx - cx) + (y + dy - cy)*(y + dy - cy));
                    if (dist < radius) {
                        dstImage.setPixel(x + dx, y + dy, {0, 0, 0, 1});
                    } else {
                        dstImage.setPixel(x + dx, y + dy, {1, 1, 1, 1});
                    }
                }
            }
        }
    });
    dst = ImageF32x4RGBAWithCache(dstImage);
}

ArtifactOldTVEffect::ArtifactOldTVEffect() {
    setDisplayName("Old TV");
    setEffectID("builtin.old_tv");
    setPipelineStage(EffectPipelineStage::Rasterizer);
}

void ArtifactOldTVEffect::apply(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    if (computeMode() != ComputeMode::CPU && runCreativeCompute(src, dst, "CreativeOldTV", kOldTVComputeHlsl)) {
        return;
    }
    int w = src.width();
    int h = src.height();
    auto srcImage = src.image();
    auto dstImage = srcImage.DeepCopy();
    
    ArtifactCore::RandomStream rng(42);
    
    // rowRng is forked from y, so scheduling does not affect the Old TV noise.
    ArtifactCore::Parallel::For(0, h, [&](int y) {
        auto rowRng = rng.fork(static_cast<uint64_t>(y));
        float scanline = (y % 4 == 0) ? 0.7f : 1.0f;
        float jitter = (rowRng.chance(0.08f)) ? rowRng.range(-5.0f, 5.0f) : 0.0f;
        
        for (int x = 0; x < w; ++x) {
            int sx = std::clamp(x + (int)jitter, 0, w - 1);
            auto c = srcImage.getPixel(sx, y);
            float noise = rowRng.range(-0.1f, 0.1f) * 0.05f;
            float r = std::clamp(c.r() * scanline + noise, 0.0f, 1.0f);
            float g = std::clamp(c.g() * scanline + noise, 0.0f, 1.0f);
            float b = std::clamp(c.b() * scanline + noise, 0.0f, 1.0f);
            dstImage.setPixel(x, y, {r, g, b, c.a()});
        }
    });
    dst = ImageF32x4RGBAWithCache(dstImage);
}

} // namespace Artifact
