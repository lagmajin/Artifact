module;
#include "ofxCore.h"
#include "ofxImageEffect.h"
#include <memory>
#include <iostream>

export module Artifact.Effect.Ofx.Impl;

import std;
import Image.ImageF32x4RGBAWithCache;
import Artifact.Effect.ImplBase;
import Artifact.Effect.Context;

namespace Artifact {
namespace Ofx {

using namespace ArtifactCore;

// Concrete implementation mapping our core Image processing to OFX's C-actions
export class ArtifactOfxEffectImpl : public ArtifactEffectImplBase {
private:
    OfxPlugin* plugin_;
    OfxImageEffectHandle instanceHandle_;
    bool isGPUEnabled_;

public:
    ArtifactOfxEffectImpl(OfxPlugin* plugin) 
        : plugin_(plugin), instanceHandle_(nullptr), isGPUEnabled_(false) {}

    virtual ~ArtifactOfxEffectImpl() {
        release();
    }

    bool initialize() override {
        // Here we would call plugin_->mainEntry(kOfxActionCreateInstance, 
        //  instanceHandle, inArgs, outArgs) to create the instance context
        std::cout << "Creating OFX Effect Instance for plugin" << std::endl;
        return true;
    }

    void release() override {
        if (instanceHandle_) {
            // plugin_->mainEntry(kOfxActionDestroyInstance, instanceHandle_, nullptr, nullptr)
            instanceHandle_ = nullptr;
        }
    }

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        // Ensure our destination receives output properly from the C-API pointer
        // Here we would set up OfxPropertySetHandle for the clip bounds and buffers

        // Simulated action call
        // plugin_->mainEntry(kOfxImageEffectActionRender, instanceHandle_, inArgs, outArgs);
        
        // For the stub: pass-through with a dummy message
        if (!src.isReadyToUse()) {
             // Fill dest with error red or something conceptually
             dst = src.DeepCopy();
             return;
        }

        std::cout << "[OFX CPU Rendering] Applying generic pass-through..." << std::endl;
        
        // Actual OFX memory wrapper logic goes here to pass raw pointers and stride info
        dst = src.DeepCopy(); // Passthrough for now
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        // If the OFX plugin supports say OpenGL or OpenCL contexts,
        // we map our internal GPU texture/buffers to the OFX descriptor.
        // And fire the `kOfxImageEffectActionRender` with GPU context flags active.

        std::cout << "[OFX GPU Rendering] Applying generic hardware pass-through..." << std::endl;

        // Stub implementation: fallback to deep copy (pass-through)
        dst = src.DeepCopy();
    }
};

} // namespace Ofx
} // namespace Artifact
