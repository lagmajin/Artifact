module;
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
export module Artifact.Effect.Keying.ChromaKey;




import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import FloatRGBA;

export namespace Artifact {

 using namespace ArtifactCore;

class ChromaKeyEffectCPUImpl : public ArtifactEffectImplBase {
private:
    FloatRGBA keyColor_{0.0f, 1.0f, 0.0f, 1.0f}; // Green
    float similarity_ = 0.4f;
    float smoothness_ = 0.1f;
    float spillReduction_ = 0.5f;

public:
    ChromaKeyEffectCPUImpl() = default;
    virtual ~ChromaKeyEffectCPUImpl() = default;

    void setKeyColor(const FloatRGBA& color) { keyColor_ = color; }
    const FloatRGBA& keyColor() const { return keyColor_; }

    void setSimilarity(float val) { similarity_ = val; }
    float similarity() const { return similarity_; }

    void setSmoothness(float val) { smoothness_ = val; }
    float smoothness() const { return smoothness_; }
    
    void setSpillReduction(float val) { spillReduction_ = val; }
    float spillReduction() const { return spillReduction_; }

    void applyCPU(const ArtifactCore::ImageF32x4RGBAWithCache& src, ArtifactCore::ImageF32x4RGBAWithCache& dst) override;
};

class ChromaKeyEffect : public ArtifactAbstractEffect {
private:
    std::shared_ptr<ChromaKeyEffectCPUImpl> typedCpuImpl_;

public:
    ChromaKeyEffect();
    ~ChromaKeyEffect() = default;

    // Expose properties via AbstractProperty API
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const ArtifactCore::UniString& name, const QVariant& value) override;

    void setKeyColor(const FloatRGBA& color);
    const FloatRGBA& keyColor() const;

    void setSimilarity(float val);
    float similarity() const;

    void setSmoothness(float val);
    float smoothness() const;
    
    void setSpillReduction(float val);
    float spillReduction() const;
};

}
