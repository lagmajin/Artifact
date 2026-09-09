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
import Memory.SharedPtr;

export namespace Artifact {

 using namespace ArtifactCore;

// Keylight-style classical keyer. Breaking reorg of the legacy
// similarity/smoothness Euclidean-RGB keyer: YCbCr screen matte +
// despill modes + edge finishing + matte views. EffectID is preserved so
// existing projects reload; legacy property names are migrated in
// setPropertyValue().
class ChromaKeyEffectCPUImpl : public ArtifactEffectImplBase {
private:
    FloatRGBA keyColor_{0.0f, 1.0f, 0.0f, 1.0f};
    float hueTolerance_ = 0.28f;
    float edgeSoftness_ = 0.12f;
    float clipBlack_ = 0.0f;
    float clipWhite_ = 1.0f;
    float despillStrength_ = 0.7f;
    int despillMode_ = 1;
    float choke_ = 0.0f;
    float matteBlur_ = 0.0f;
    int viewMode_ = 0;

public:
    ChromaKeyEffectCPUImpl() = default;
    virtual ~ChromaKeyEffectCPUImpl() = default;

    void setKeyColor(const FloatRGBA& color) {
        const auto safeChannel = [](float value, float fallback) {
            return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : fallback;
        };
        keyColor_ = FloatRGBA(safeChannel(color.r(), 0.0f),
                              safeChannel(color.g(), 1.0f),
                              safeChannel(color.b(), 0.0f),
                              safeChannel(color.a(), 1.0f));
    }
    const FloatRGBA& keyColor() const { return keyColor_; }

    void setHueTolerance(float val) { hueTolerance_ = std::isfinite(val) ? std::clamp(val, 0.0f, 1.0f) : 0.28f; }
    float hueTolerance() const { return hueTolerance_; }
    void setEdgeSoftness(float val) { edgeSoftness_ = std::isfinite(val) ? std::clamp(val, 0.0001f, 1.0f) : 0.12f; }
    float edgeSoftness() const { return edgeSoftness_; }
    void setClipBlack(float val) { clipBlack_ = std::isfinite(val) ? std::clamp(val, 0.0f, 1.0f) : 0.0f; }
    float clipBlack() const { return clipBlack_; }
    void setClipWhite(float val) { clipWhite_ = std::isfinite(val) ? std::clamp(val, 0.0f, 1.0f) : 1.0f; }
    float clipWhite() const { return clipWhite_; }
    void setDespillStrength(float val) { despillStrength_ = std::isfinite(val) ? std::clamp(val, 0.0f, 1.0f) : 0.7f; }
    float despillStrength() const { return despillStrength_; }
    void setDespillMode(int mode) { despillMode_ = std::clamp(mode, 0, 2); }
    int despillMode() const { return despillMode_; }
    void setChoke(float val) { choke_ = std::isfinite(val) ? std::clamp(val, -1.0f, 1.0f) : 0.0f; }
    float choke() const { return choke_; }
    void setMatteBlur(float val) { matteBlur_ = std::isfinite(val) ? std::clamp(val, 0.0f, 2.0f) : 0.0f; }
    float matteBlur() const { return matteBlur_; }
    void setViewMode(int mode) { viewMode_ = std::clamp(mode, 0, 3); }
    int viewMode() const { return viewMode_; }

    void applyCPU(const ArtifactCore::ImageF32x4RGBAWithCache& src, ArtifactCore::ImageF32x4RGBAWithCache& dst) override;
};

class ChromaKeyEffect : public ArtifactAbstractEffect {
private:
    SharedPtr<ChromaKeyEffectCPUImpl> typedCpuImpl_;

    void syncGpuImpl();

public:
    ChromaKeyEffect();
    ~ChromaKeyEffect() = default;

    // Expose properties via AbstractProperty API
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const ArtifactCore::UniString& name, const QVariant& value) override;

    void setKeyColor(const FloatRGBA& color);
    const FloatRGBA& keyColor() const;

    void setHueTolerance(float val);
    float hueTolerance() const;
    void setEdgeSoftness(float val);
    float edgeSoftness() const;
    void setClipBlack(float val);
    float clipBlack() const;
    void setClipWhite(float val);
    float clipWhite() const;
    void setDespillStrength(float val);
    float despillStrength() const;
    void setDespillMode(int mode);
    int despillMode() const;
    void setChoke(float val);
    float choke() const;
    void setMatteBlur(float val);
    float matteBlur() const;
    void setViewMode(int mode);
    int viewMode() const;
};

}
