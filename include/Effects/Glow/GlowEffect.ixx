module;
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <QVariant>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Artifact.Effect.Glow;




import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Artifact.Render.DiligentDeviceManager;
import Image.ImageF32x4RGBAWithCache;

export namespace Artifact {

class GlowEffect : public ArtifactAbstractEffect {
private:
    class Impl;
    Impl* impl_;
public:
    GlowEffect();
    ~GlowEffect();

    // Expose properties via AbstractProperty API
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const ArtifactCore::UniString& name, const QVariant& value) override;

    void setGlowGain(float gain);
    float glowGain() const;

    void setLayerCount(int count);
    int layerCount() const;

    void setBaseSigma(float sigma);
    float baseSigma() const;

    void setSigmaGrowth(float growth);
    float sigmaGrowth() const;

    void setBaseAlpha(float alpha);
    float baseAlpha() const;

    void setAlphaFalloff(float falloff);
    float alphaFalloff() const;

    bool supportsGPU() const override {
        return true;
    }
};

};


