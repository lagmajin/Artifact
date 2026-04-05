module;
#include <vector>
#include <cmath>
#include <random>
#include <algorithm>

module Artifact.Effect.Creative;

import Artifact.Effect.Abstract;
import Image.ImageF32x4RGBAWithCache;
import Math.Noise;

namespace Artifact {

ArtifactGlitchEffect::ArtifactGlitchEffect() {
    setDisplayName("Glitch");
    setEffectID("builtin.glitch");
    setPipelineStage(EffectPipelineStage::Rasterizer);
}

void ArtifactGlitchEffect::apply(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    int w = src.width();
    int h = src.height();
    auto srcImage = src.image();
    auto dstImage = srcImage.DeepCopy();

    float shiftX = 5.0f;
    
    for (int y = 0; y < h; ++y) {
        float rowOffset = 0;
        if (y % 15 < 3) rowOffset = 10.0f * (std::sin(y * 0.2f));
        
        for (int x = 0; x < w; ++x) {
            int sx = std::clamp(x + (int)rowOffset, 0, w - 1);
            auto c = srcImage.getPixel(sx, y);
            
            int rsx = std::clamp(sx + (int)shiftX, 0, w - 1);
            auto cr = srcImage.getPixel(rsx, y);
            
            int bsx = std::clamp(sx - (int)shiftX, 0, w - 1);
            auto cb = srcImage.getPixel(bsx, y);
            
            dstImage.setPixel(x, y, {cr.r(), c.g(), cb.b(), c.a()});
        }
    }
    dst = ImageF32x4RGBAWithCache(dstImage);
}

ArtifactHalftoneEffect::ArtifactHalftoneEffect() {
    setDisplayName("Halftone");
    setEffectID("builtin.halftone");
    setPipelineStage(EffectPipelineStage::Rasterizer);
}

void ArtifactHalftoneEffect::apply(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    int w = src.width();
    int h = src.height();
    auto srcImage = src.image();
    auto dstImage = srcImage.DeepCopy();
    
    int dotSize = 8;
    
    for (int y = 0; y < h; y += dotSize) {
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
    }
    dst = ImageF32x4RGBAWithCache(dstImage);
}

ArtifactOldTVEffect::ArtifactOldTVEffect() {
    setDisplayName("Old TV");
    setEffectID("builtin.old_tv");
    setPipelineStage(EffectPipelineStage::Rasterizer);
}

void ArtifactOldTVEffect::apply(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    int w = src.width();
    int h = src.height();
    auto srcImage = src.image();
    auto dstImage = srcImage.DeepCopy();
    
    static std::mt19937 gen(42);
    std::uniform_real_distribution<float> dis(-0.1f, 0.1f);
    
    for (int y = 0; y < h; ++y) {
        float scanline = (y % 4 == 0) ? 0.7f : 1.0f;
        float jitter = (dis(gen) > 0.08f) ? (dis(gen) * 5.0f) : 0.0f;
        
        for (int x = 0; x < w; ++x) {
            int sx = std::clamp(x + (int)jitter, 0, w - 1);
            auto c = srcImage.getPixel(sx, y);
            float noise = dis(gen) * 0.05f;
            float r = std::clamp(c.r() * scanline + noise, 0.0f, 1.0f);
            float g = std::clamp(c.g() * scanline + noise, 0.0f, 1.0f);
            float b = std::clamp(c.b() * scanline + noise, 0.0f, 1.0f);
            dstImage.setPixel(x, y, {r, g, b, c.a()});
        }
    }
    dst = ImageF32x4RGBAWithCache(dstImage);
}

} // namespace Artifact
