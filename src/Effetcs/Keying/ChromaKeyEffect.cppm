module;
#include <opencv2/opencv.hpp>
#include <cmath>

module Artifact.Effect.Keying.ChromaKey;

import std;
import Artifact.Effect.Keying.ChromaKey;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4_RGBA;
import Image.ImageF32x4RGBAWithCache;
import Artifact.Effect.Abstract;
import FloatRGBA;
import Utils.String.UniString;

namespace Artifact {
 using namespace ArtifactCore;

void ChromaKeyEffectCPUImpl::applyCPU(const ArtifactCore::ImageF32x4RGBAWithCache& src, ArtifactCore::ImageF32x4RGBAWithCache& dst) {
    const ArtifactCore::ImageF32x4_RGBA& srcImage = src.image();
    cv::Mat srcMat = srcImage.toCVMat();

    // Ensure we have data
    if (srcMat.empty()) {
        dst = src;
        return;
    }
    
    cv::Mat dstMat = srcMat.clone();

    // Loop
    int rows = dstMat.rows;
    int cols = dstMat.cols;
    
    // Use stored parameters
    float kr = keyColor_.r();
    float kg = keyColor_.g();
    float kb = keyColor_.b();
    
    float sim = similarity_;
    float smooth = smoothness_;
    float spill = spillReduction_;
    
    if(smooth < 0.001f) smooth = 0.001f;

    for(int y=0; y<rows; ++y) {
        cv::Vec4f* ptr = dstMat.ptr<cv::Vec4f>(y);
        for(int x=0; x<cols; ++x) {
            float r = ptr[x][0];
            float g = ptr[x][1];
            float b = ptr[x][2];
            float a = ptr[x][3];

            // Euclidian distance
            float dist = std::sqrt(std::pow(r - kr, 2) + std::pow(g - kg, 2) + std::pow(b - kb, 2));

            float alphaFactor = 1.0f;
            if (dist < sim) {
                alphaFactor = 0.0f;
            } else if (dist < sim + smooth) {
                alphaFactor = (dist - sim) / smooth;
            }
            
            // Simple spill reduction (optional/basic)
            if (dist < sim + smooth + 0.2f && spill > 0.0f) {
                 float gray = r * 0.299f + g * 0.587f + b * 0.114f;
                 // Factor depends on how "green" it is
                 float factor = spill * (1.0f - std::min(1.0f, (dist - sim) / (smooth + 0.2f)));
                 if (factor > 0.0f) {
                    ptr[x][0] = r * (1.0f - factor) + gray * factor;
                    ptr[x][1] = g * (1.0f - factor) + gray * factor;
                    ptr[x][2] = b * (1.0f - factor) + gray * factor;
                 }
            }

            ptr[x][3] = a * alphaFactor;
        }
    }

    ImageF32x4_RGBA dstImage;
    dstImage.setFromCVMat(dstMat);
    dst = ImageF32x4RGBAWithCache(dstImage);
}

ChromaKeyEffect::ChromaKeyEffect() : ArtifactAbstractEffect() {
    typedCpuImpl_ = std::make_shared<ChromaKeyEffectCPUImpl>();
    setCPUImpl(typedCpuImpl_);
    setDisplayName("Chroma Key");
    setEffectID("Effect.Keying.ChromaKey");
}

void ChromaKeyEffect::setKeyColor(const FloatRGBA& color) {
    typedCpuImpl_->setKeyColor(color);
}
const FloatRGBA& ChromaKeyEffect::keyColor() const {
    return typedCpuImpl_->keyColor();
}

void ChromaKeyEffect::setSimilarity(float val) {
    typedCpuImpl_->setSimilarity(val);
}
float ChromaKeyEffect::similarity() const {
    return typedCpuImpl_->similarity();
}

void ChromaKeyEffect::setSmoothness(float val) {
    typedCpuImpl_->setSmoothness(val);
}
float ChromaKeyEffect::smoothness() const {
    return typedCpuImpl_->smoothness();
}

void ChromaKeyEffect::setSpillReduction(float val) {
    typedCpuImpl_->setSpillReduction(val);
}
float ChromaKeyEffect::spillReduction() const {
    return typedCpuImpl_->spillReduction();
}

}
