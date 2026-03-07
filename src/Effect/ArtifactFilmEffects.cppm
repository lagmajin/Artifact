module;

#include <cmath>
#include <random>
#include <algorithm>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRandomGenerator>
#include <QColor>
#include <QDebug>
#include <wobjectimpl.h>

module Artifact.Effect.Film;

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
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>




namespace Artifact {

W_OBJECT_IMPL(FilmEffectPreset)
W_OBJECT_IMPL(FilmEffectProcessor)

// ==================== Grain Presets ====================

FilmGrainEffect FilmGrainEffect::filmStock(int iso) {
    FilmGrainEffect g;
    g.intensity = std::clamp(iso / 800.0f, 0.1f, 1.0f);
    g.size = 1.0f + (iso / 400.0f);
    g.speed = 1.0f;
    g.colorGrain = true;
    return g;
}

FilmGrainEffect FilmGrainEffect::oldFilm() {
    FilmGrainEffect g;
    g.intensity = 0.5f;
    g.size = 2.0f;
    g.speed = 0.3f;
    g.colorGrain = false;
    return g;
}

FilmGrainEffect FilmGrainEffect::cinematic() {
    FilmGrainEffect g;
    g.intensity = 0.15f;
    g.size = 0.8f;
    g.speed = 0.5f;
    g.colorGrain = false;
    return g;
}

FilmGrainEffect FilmGrainEffect::highContrast() {
    FilmGrainEffect g;
    g.intensity = 0.35f;
    g.size = 1.5f;
    g.speed = 1.0f;
    g.colorGrain = true;
    g.threshold = 0.3f;
    return g;
}

// ==================== Scratches Presets ====================

FilmScratchesEffect FilmScratchesEffect::light() {
    FilmScratchesEffect s;
    s.count = 2;
    s.opacity = 0.2f;
    s.width = 0.5f;
    s.speed = 0.2f;
    return s;
}

FilmScratchesEffect FilmScratchesEffect::heavy() {
    FilmScratchesEffect s;
    s.count = 15;
    s.opacity = 0.7f;
    s.width = 2.0f;
    s.speed = 1.0f;
    return s;
}

FilmScratchesEffect FilmScratchesEffect::agitated() {
    FilmScratchesEffect s;
    s.count = 30;
    s.opacity = 0.9f;
    s.width = 1.5f;
    s.speed = 3.0f;
    s.vertical = true;
    return s;
}

// ==================== Dust Presets ====================

FilmDustEffect FilmDustEffect::light() {
    FilmDustEffect d;
    d.density = 20;
    d.size = 1.0f;
    d.opacity = 0.2f;
    d.speed = 0.1f;
    d.animated = true;
    return d;
}

FilmDustEffect FilmDustEffect::heavy() {
    FilmDustEffect d;
    d.density = 100;
    d.size = 3.0f;
    d.opacity = 0.5f;
    d.speed = 0.5f;
    d.animated = true;
    return d;
}

FilmDustEffect FilmDustEffect::vintage() {
    FilmDustEffect d;
    d.density = 50;
    d.size = 2.0f;
    d.opacity = 0.4f;
    d.speed = 0.2f;
    d.animated = false; // static dust
    return d;
}

// ==================== Vignette Presets ====================

FilmVignetteEffect FilmVignetteEffect::subtle() {
    FilmVignetteEffect v;
    v.intensity = 0.2f;
    v.softness = 0.8f;
    v.roundness = 1.0f;
    return v;
}

FilmVignetteEffect FilmVignetteEffect::heavy() {
    FilmVignetteEffect v;
    v.intensity = 0.8f;
    v.softness = 0.4f;
    v.roundness = 1.0f;
    return v;
}

FilmVignetteEffect FilmVignetteEffect::anamorphic() {
    FilmVignetteEffect v;
    v.intensity = 0.4f;
    v.softness = 0.3f;
    v.roundness = 0.3f; // horizontal streak
    v.size = 1.5f;
    return v;
}

FilmVignetteEffect FilmVignetteEffect::circular() {
    FilmVignetteEffect v;
    v.intensity = 0.5f;
    v.softness = 0.5f;
    v.roundness = 1.0f;
    v.size = 0.8f;
    return v;
}

// ==================== ColorFade Presets ====================

FilmColorFadeEffect FilmColorFadeEffect::faded() {
    FilmColorFadeEffect c;
    c.fadeAmount = 0.3f;
    c.contrast = 0.9f;
    c.brightness = -0.05f;
    return c;
}

FilmColorFadeEffect FilmColorFadeEffect::fadedWarm() {
    FilmColorFadeEffect c = faded();
    c.warmth = 0.3f;
    c.tint = QColor(255, 240, 200, 30);
    return c;
}

FilmColorFadeEffect FilmColorFadeEffect::fadedCool() {
    FilmColorFadeEffect c = faded();
    c.warmth = -0.3f;
    c.tint = QColor(200, 220, 255, 30);
    return c;
}

FilmColorFadeEffect FilmColorFadeEffect::bleachBypass() {
    FilmColorFadeEffect c;
    c.fadeAmount = 0.5f;
    c.contrast = 1.4f;
    c.brightness = 0.0f;
    return c;
}

// ==================== Flicker Presets ====================

FilmFlickerEffect FilmFlickerEffect::subtle() {
    FilmFlickerEffect f;
    f.intensity = 0.03f;
    f.speed = 10.0f;
    f.random = 0.3f;
    return f;
}

FilmFlickerEffect FilmFlickerEffect::heavy() {
    FilmFlickerEffect f;
    f.intensity = 0.15f;
    f.speed = 5.0f;
    f.random = 0.7f;
    return f;
}

FilmFlickerEffect FilmFlickerEffect::oldProjector() {
    FilmFlickerEffect f;
    f.intensity = 0.2f;
    f.speed = 2.0f; // 24fps-ish flicker
    f.random = 0.5f;
    f.additive = true;
    return f;
}

// ==================== LightLeak Presets ====================

FilmLightLeakEffect FilmLightLeakEffect::orange() {
    FilmLightLeakEffect l;
    l.leakColor = QColor(255, 120, 40, 120);
    l.intensity = 0.5f;
    l.softness = 0.7f;
    return l;
}

FilmLightLeakEffect FilmLightLeakEffect::red() {
    FilmLightLeakEffect l;
    l.leakColor = QColor(220, 30, 30, 100);
    l.intensity = 0.4f;
    l.softness = 0.6f;
    return l;
}

FilmLightLeakEffect FilmLightLeakEffect::green() {
    FilmLightLeakEffect l;
    l.leakColor = QColor(50, 200, 80, 80);
    l.intensity = 0.3f;
    l.softness = 0.8f;
    return l;
}

FilmLightLeakEffect FilmLightLeakEffect::creative() {
    FilmLightLeakEffect l;
    l.leakColor = QColor(255, 50, 150, 100);
    l.intensity = 0.6f;
    l.softness = 0.5f;
    return l;
}

// ==================== Jitter Presets ====================

FilmJitterEffect FilmJitterEffect::subtle() {
    FilmJitterEffect j;
    j.horizontalAmount = 0.5f;
    j.verticalAmount = 0.3f;
    j.speed = 3.0f;
    j.random = 0.4f;
    return j;
}

FilmJitterEffect FilmJitterEffect::heavy() {
    FilmJitterEffect j;
    j.horizontalAmount = 3.0f;
    j.verticalAmount = 1.5f;
    j.speed = 5.0f;
    j.random = 0.8f;
    return j;
}

FilmJitterEffect FilmJitterEffect::damaged() {
    FilmJitterEffect j;
    j.horizontalAmount = 5.0f;
    j.verticalAmount = 3.0f;
    j.speed = 8.0f;
    j.random = 1.0f;
    j.additive = false;
    return j;
}

// ==================== FilmEffectPreset Implementation ====================

FilmEffectPreset::FilmEffectPreset(const QString& id, 
                                   const QString& name,
                                   FilmEffectType type,
                                   QObject* parent)
    : QObject(parent)
    , id_(id)
    , name_(name)
    , type_(type)
{
}

FilmEffectPreset::~FilmEffectPreset() = default;

void FilmEffectPreset::applyMasterIntensity() {
    grain_.intensity *= masterIntensity_;
    scratches_.opacity *= masterIntensity_;
    dust_.opacity *= masterIntensity_;
    vignette_.intensity *= masterIntensity_;
    colorFade_.fadeAmount *= masterIntensity_;
    flicker_.intensity *= masterIntensity_;
    lightLeak_.intensity *= masterIntensity_;
    jitter_.horizontalAmount *= masterIntensity_;
    jitter_.verticalAmount *= masterIntensity_;
}

QString FilmEffectPreset::toJSON() const {
    QJsonObject root;
    root["id"] = id_;
    root["name"] = name_;
    root["description"] = description_;
    root["type"] = static_cast<int>(type_);
    root["masterIntensity"] = masterIntensity_;
    root["masterSaturation"] = masterSaturation_;
    root["enabled"] = enabled_;
    
    // Individual effects as nested objects
    QJsonObject grain;
    grain["intensity"] = grain_.intensity;
    grain["size"] = grain_.size;
    grain["speed"] = grain_.speed;
    grain["colorGrain"] = grain_.colorGrain;
    root["grain"] = grain;
    
    // ... other effects would be serialized similarly
    
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

bool FilmEffectPreset::fromJSON(const QString& json) {
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (doc.isNull()) return false;
    
    QJsonObject root = doc.object();
    id_ = root["id"].toString();
    name_ = root["name"].toString();
    description_ = root["description"].toString();
    type_ = static_cast<FilmEffectType>(root["type"].toInt(0));
    masterIntensity_ = root["masterIntensity"].toDouble(1.0);
    masterSaturation_ = root["masterSaturation"].toDouble(1.0);
    enabled_ = root["enabled"].toBool(true);
    
    // Load grain
    QJsonObject grain = root["grain"].toObject();
    grain_.intensity = grain["intensity"].toDouble(0.3);
    grain_.size = grain["size"].toDouble(1.0);
    grain_.speed = grain["speed"].toDouble(1.0);
    grain_.colorGrain = grain["colorGrain"].toBool(true);
    
    emit presetLoaded();
    return true;
}

// ==================== FilmPresets Implementation ====================

FilmEffectPreset* FilmPresets::oldMovie() {
    auto* p = new FilmEffectPreset("old_movie", "Old Movie", FilmEffectType::Grain);
    p->grain() = FilmGrainEffect::oldFilm();
    p->scratches() = FilmScratchesEffect::heavy();
    p->dust() = FilmDustEffect::vintage();
    p->vignette() = FilmVignetteEffect::heavy();
    p->flicker() = FilmFlickerEffect::oldProjector();
    p->colorFade() = FilmColorFadeEffect::faded();
    return p;
}

FilmEffectPreset* FilmPresets::vintage8mm() {
    auto* p = new FilmEffectPreset("vintage_8mm", "Vintage 8mm", FilmEffectType::Grain);
    p->grain().intensity = 0.6f;
    p->grain().size = 3.0f;
    p->grain().colorGrain = false;
    p->scratches().count = 10;
    p->scratches().vertical = true;
    p->vignette() = FilmVignetteEffect::circular();
    p->jitter() = FilmJitterEffect::damaged();
    p->colorFade().fadeAmount = 0.5f;
    return p;
}

FilmEffectPreset* FilmPresets::filmNoir() {
    auto* p = new FilmEffectPreset("film_noir", "Film Noir", FilmEffectType::Grain);
    p->grain() = FilmGrainEffect::cinematic();
    p->vignette() = FilmVignetteEffect::heavy();
    p->colorFade().fadeAmount = 0.6f;
    p->colorFade().contrast = 1.3f;
    p->setMasterSaturation(0.0f); // B&W
    return p;
}

FilmEffectPreset* FilmPresets::documentary() {
    auto* p = new FilmEffectPreset("documentary", "Documentary", FilmEffectType::Grain);
    p->grain() = FilmGrainEffect::filmStock(400);
    p->vignette() = FilmVignetteEffect::subtle();
    p->colorFade() = FilmColorFadeEffect::faded();
    return p;
}

FilmEffectPreset* FilmPresets::bleached() {
    auto* p = new FilmEffectPreset("bleached", "Bleached", FilmEffectType::Bleach);
    p->grain().intensity = 0.2f;
    p->colorFade() = FilmColorFadeEffect::bleachBypass();
    p->vignette() = FilmVignetteEffect::subtle();
    return p;
}

FilmEffectPreset* FilmPresets::crossProcessed() {
    auto* p = new FilmEffectPreset("cross_processed", "Cross Processed", FilmEffectType::CrossProcess);
    p->grain() = FilmGrainEffect::highContrast();
    p->lightLeak() = FilmLightLeakEffect::creative();
    p->vignette() = FilmVignetteEffect::subtle();
    p->colorFade().contrast = 1.2f;
    return p;
}

FilmEffectPreset* FilmPresets::damagedFilm() {
    auto* p = new FilmEffectPreset("damaged", "Damaged Film", FilmEffectType::Grain);
    p->grain().intensity = 0.8f;
    p->scratches() = FilmScratchesEffect::agitated();
    p->dust() = FilmDustEffect::heavy();
    p->flicker() = FilmFlickerEffect::heavy();
    p->jitter() = FilmJitterEffect::damaged();
    return p;
}

FilmEffectPreset* FilmPresets::sepiaTone() {
    auto* p = new FilmEffectPreset("sepia", "Sepia Tone", FilmEffectType::Sepia);
    p->grain() = FilmGrainEffect::cinematic();
    p->colorFade().fadeAmount = 0.4f;
    p->colorFade().warmth = 0.8f;
    p->vignette() = FilmVignetteEffect::subtle();
    return p;
}

FilmEffectPreset* FilmPresets::fadeOut() {
    auto* p = new FilmEffectPreset("fade_out", "Fade Out", FilmEffectType::ColorFade);
    p->colorFade().fadeAmount = 0.7f;
    p->colorFade().brightness = -0.1f;
    p->vignette() = FilmVignetteEffect::circular();
    return p;
}

// ==================== FilmEffectProcessor Implementation ====================

class FilmEffectProcessor::Impl {
public:
    FilmEffectPreset* currentPreset_ = nullptr;
    int seed_ = 12345;
    
    // Random generators
    std::mt19937 rng_;
    std::uniform_real_distribution<float> dist01_{0.0f, 1.0f};
    std::uniform_real_distribution<float> distMinus1_{-1.0f, 1.0f};
    
    Impl() {
        rng_.seed(seed_);
    }
    
    // Apply film grain
    void applyGrain(float* pixels, int width, int height, float time) {
        if (!currentPreset_) return;
        
        const auto& g = currentPreset_->grain();
        if (g.intensity <= 0.0f) return;
        
        // Animate seed based on time
        std::mt19937 localRng(seed_ + static_cast<int>(time * g.speed * 100));
        std::uniform_real_distribution<float> localDist(-g.intensity, g.intensity);
        
        for (int i = 0; i < width * height * 4; i += 4) {
            float noise = localDist(localRng);
            
            if (g.colorGrain) {
                // RGB noise
                pixels[i] += noise * localDist(localRng);
                pixels[i + 1] += noise * localDist(localRng);
                pixels[i + 2] += noise * localDist(localRng);
            } else {
                // Monochrome noise
                float mono = noise;
                pixels[i] += mono;
                pixels[i + 1] += mono;
                pixels[i + 2] += mono;
            }
        }
    }
    
    // Apply vignette
    void applyVignette(float* pixels, int width, int height, float time) {
        if (!currentPreset_) return;
        
        const auto& v = currentPreset_->vignette();
        if (v.intensity <= 0.0f) return;
        
        float cx = v.center.x() * width;
        float cy = v.center.y() * height;
        float maxDist = std::sqrt(width * width + height * height) * v.size;
        
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float dx = (x - cx) / maxDist;
                float dy = (y - cy) / (maxDist * v.roundness);
                float dist = std::sqrt(dx * dx + dy * dy);
                
                // Vignette falloff
                float vignette = 1.0f - std::clamp(dist, 0.0f, 1.0f);
                vignette = std::pow(vignette, 1.0f / (v.softness + 0.01f));
                
                float factor = 1.0f - (v.intensity * (1.0f - vignette));
                
                int idx = (y * width + x) * 4;
                pixels[idx] *= factor;
                pixels[idx + 1] *= factor;
                pixels[idx + 2] *= factor;
            }
        }
    }
    
    // Apply color fade/desaturation
    void applyColorFade(float* pixels, int width, int height, float time) {
        if (!currentPreset_) return;
        
        const auto& cf = currentPreset_->colorFade();
        if (cf.fadeAmount <= 0.0f && cf.contrast == 1.0f && cf.brightness == 0.0f) return;
        
        for (int i = 0; i < width * height * 4; i += 4) {
            float r = pixels[i];
            float g = pixels[i + 1];
            float b = pixels[i + 2];
            
            // Desaturation
            float luma = r * 0.299f + g * 0.587f + b * 0.114f;
            r = r + (luma - r) * cf.fadeAmount;
            g = g + (luma - g) * cf.fadeAmount;
            b = b + (luma - b) * cf.fadeAmount;
            
            // Contrast
            r = (r - 0.5f) * cf.contrast + 0.5f;
            g = (g - 0.5f) * cf.contrast + 0.5f;
            b = (b - 0.5f) * cf.contrast + 0.5f;
            
            // Brightness
            r += cf.brightness;
            g += cf.brightness;
            b += cf.brightness;
            
            // Warmth
            if (cf.warmth != 0.0f) {
                r += cf.warmth * 0.1f;
                b -= cf.warmth * 0.1f;
            }
            
            pixels[i] = std::clamp(r, 0.0f, 1.0f);
            pixels[i + 1] = std::clamp(g, 0.0f, 1.0f);
            pixels[i + 2] = std::clamp(b, 0.0f, 1.0f);
        }
    }
    
    // Apply flicker
    void applyFlicker(float* pixels, int width, int height, float time) {
        if (!currentPreset_) return;
        
        const auto& f = currentPreset_->flicker();
        if (f.intensity <= 0.0f) return;
        
        // Generate flicker value
        float flicker = 1.0f;
        
        if (f.random > 0.0f) {
            // Random flicker
            std::mt19937 localRng(seed_ + static_cast<int>(time * f.speed));
            std::uniform_real_distribution<float> dist(1.0f - f.intensity, 1.0f + f.intensity * f.random);
            flicker = dist(localRng);
        } else {
            // Pattern flicker (sine wave)
            flicker = 1.0f + std::sin(time * f.speed * 6.28f) * f.intensity;
        }
        
        if (!f.additive) {
            // Multiplicative (darker flickers)
            for (int i = 0; i < width * height * 4; ++i) {
                pixels[i] *= flicker;
            }
        } else {
            // Additive
            float add = (flicker - 1.0f) * 0.5f;
            for (int i = 0; i < width * height * 4; ++i) {
                pixels[i] += add;
            }
        }
    }
};

W_OBJECT_IMPL(FilmEffectProcessor)

FilmEffectProcessor::FilmEffectProcessor(QObject* parent)
    : QObject(parent)
    , impl_(new Impl())
{
}

FilmEffectProcessor::~FilmEffectProcessor() = default;

void FilmEffectProcessor::setPreset(FilmEffectPreset* preset) {
    impl_->currentPreset_ = preset;
    emit presetChanged(preset);
}

FilmEffectPreset* FilmEffectProcessor::currentPreset() const {
    return impl_->currentPreset_;
}

void FilmEffectProcessor::process(float* pixels, int width, int height, float time) {
    if (!impl_->currentPreset_ || !impl_->currentPreset_->enabled()) {
        return;
    }
    
    // Apply effects in order
    impl_->applyColorFade(pixels, width, height, time);
    impl_->applyVignette(pixels, width, height, time);
    impl_->applyFlicker(pixels, width, height, time);
    impl_->applyGrain(pixels, width, height, time);
    
    emit frameProcessed();
}

void FilmEffectProcessor::setGrainIntensity(float intensity) {
    if (impl_->currentPreset_) {
        impl_->currentPreset_->grain().intensity = intensity;
        emit changed();
    }
}

void FilmEffectProcessor::setVignetteIntensity(float intensity) {
    if (impl_->currentPreset_) {
        impl_->currentPreset_->vignette().intensity = intensity;
        emit changed();
    }
}

void FilmEffectProcessor::setFlickerIntensity(float intensity) {
    if (impl_->currentPreset_) {
        impl_->currentPreset_->flicker().intensity = intensity;
        emit changed();
    }
}

void FilmEffectProcessor::setSeed(int seed) {
    impl_->seed_ = seed;
    impl_->rng_.seed(seed);
}

void FilmEffectProcessor::randomizeSeed() {
    setSeed(QRandomGenerator::global()->bounded(100000));
}

std::vector<FilmEffectPreset*> FilmEffectProcessor::allPresets() {
    return {
        FilmPresets::oldMovie(),
        FilmPresets::vintage8mm(),
        FilmPresets::filmNoir(),
        FilmPresets::documentary(),
        FilmPresets::bleached(),
        FilmPresets::crossProcessed(),
        FilmPresets::damagedFilm(),
        FilmPresets::sepiaTone(),
        FilmPresets::fadeOut()
    };
}

FilmEffectPreset* FilmEffectProcessor::getPresetById(const QString& id) {
    static std::map<QString, FilmEffectPreset*> presets;
    
    if (presets.empty()) {
        for (auto* p : allPresets()) {
            presets[p->id()] = p;
        }
    }
    
    auto it = presets.find(id);
    return (it != presets.end()) ? it->second : nullptr;
}

} // namespace Artifact
