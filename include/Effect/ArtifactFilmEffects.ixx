module;
#include <QObject>
#include <QString>
#include <QVector>
#include <QColor>
#include <random>
#include <memory>
#include <wobjectdefs.h>

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
export module Artifact.Effect.Film;





W_REGISTER_ARGTYPE(QColor)
W_REGISTER_ARGTYPE(QVector2D)

export namespace Artifact {

/**
 * @brief Film effect types
 */
enum class FilmEffectType {
    Grain,          // Film grain / noise
    Scratches,      // Horizontal scratches
    Dust,           // Dust particles
    Vignette,       // Edge darkening
    ColorFade,      // Color fading / desaturation
    Flicker,        // Light flicker
    LightLeak,      // Light leak effect
    Hair,           // Film hair / strands
    FrameLines,     // Frame boundary lines
    Jitter,         // Frame jitter / instability
    Bleach,         // Bleach bypass effect
    Sepia,          // Sepia toning
    Negative,       // Film negative effect
    CrossProcess,   // Cross processing
    ChromaticAberration  // Color fringing (aging effect)
};

/**
 * @brief Film grain / noise effect
 */
class FilmGrainEffect {
public:
    float intensity = 0.3f;        // 0-1, grain strength
    float size = 1.0f;              // grain particle size
    float speed = 1.0f;              // animation speed
    float threshold = 0.5f;         // luminance threshold
    bool colorGrain = true;         // color vs monochrome
    
    // Presets
    static FilmGrainEffect filmStock(int iso = 400);
    static FilmGrainEffect oldFilm();
    static FilmGrainEffect cinematic();
    static FilmGrainEffect highContrast();
};

/**
 * @brief Film scratches effect
 */
class FilmScratchesEffect {
public:
    int count = 5;                  // number of scratches
    float opacity = 0.5f;           // scratch visibility
    float width = 1.0f;             // scratch width
    float speed = 0.5f;             // animation speed
    bool vertical = false;          // vertical vs horizontal
    QColor color = QColor(200, 200, 180, 255); // scratch color
    
    // Presets
    static FilmScratchesEffect light();
    static FilmScratchesEffect heavy();
    static FilmScratchesEffect agitated();
};

/**
 * @brief Dust / dirt effect
 */
class FilmDustEffect {
public:
    int density = 50;              // number of dust particles
    float size = 2.0f;              // dust particle size
    float opacity = 0.4f;           // dust visibility
    float speed = 0.3f;             // movement speed
    bool animated = true;           // animate dust
    QColor color = QColor(150, 140, 120, 180);
    
    // Presets
    static FilmDustEffect light();
    static FilmDustEffect heavy();
    static FilmDustEffect vintage();
};

/**
 * @brief Vignette effect (edge darkening)
 */
class FilmVignetteEffect {
public:
    float intensity = 0.5f;        // 0-1, darkness
    float softness = 0.5f;         // 0-1, edge softness
    float roundness = 1.0f;         // 0-1, shape roundness
    float size = 1.0f;              // vignette size
    QVector2D center = QVector2D(0.5f, 0.5f); // center position
    QColor tintColor = QColor(0, 0, 0, 255); // tint
    
    // Presets
    static FilmVignetteEffect subtle();
    static FilmVignetteEffect heavy();
    static FilmVignetteEffect anamorphic();
    static FilmVignetteEffect circular();
};

/**
 * @brief Color fade / desaturation effect
 */
class FilmColorFadeEffect {
public:
    float fadeAmount = 0.3f;       // 0-1, desaturation
    float contrast = 1.0f;          // contrast adjustment
    float brightness = 0.0f;        // brightness adjustment
    QColor tint = QColor(255, 248, 220, 0); // tint (no tint = 0 alpha)
    float warmth = 0.0f;           // -1 to 1, warm/cool
    
    // Presets
    static FilmColorFadeEffect faded();
    static FilmColorFadeEffect fadedWarm();
    static FilmColorFadeEffect fadedCool();
    static FilmColorFadeEffect bleachBypass();
};

/**
 * @brief Light flicker effect
 */
class FilmFlickerEffect {
public:
    float intensity = 0.1f;        // flicker strength
    float speed = 5.0f;             // flicker frequency
    float random = 0.5f;            // randomness vs pattern
    bool additive = true;           // additive vs multiplicative
    
    // Presets
    static FilmFlickerEffect subtle();
    static FilmFlickerEffect heavy();
    static FilmFlickerEffect oldProjector();
};

/**
 * @brief Light leak effect
 */
class FilmLightLeakEffect {
public:
    QColor leakColor = QColor(255, 100, 50, 150);
    float intensity = 0.5f;
    float size = 0.5f;
    float softness = 0.7f;
    float speed = 0.2f;
    int seed = 0;                   // for random variations
    
    // Presets
    static FilmLightLeakEffect orange();
    static FilmLightLeakEffect red();
    static FilmLightLeakEffect green();
    static FilmLightLeakEffect creative();
};

/**
 * @brief Jitter / instability effect
 */
class FilmJitterEffect {
public:
    float horizontalAmount = 1.0f;
    float verticalAmount = 0.5f;
    float speed = 2.0f;
    float random = 0.7f;
    bool additive = true;
    
    // Presets
    static FilmJitterEffect subtle();
    static FilmJitterEffect heavy();
    static FilmJitterEffect damaged();
};

/**
 * @brief Complete film effect preset combining multiple effects
 */
class FilmEffectPreset : public QObject {
    W_OBJECT(FilmEffectPreset)
private:
    QString id_;
    QString name_;
    QString description_;
    FilmEffectType type_;
    
    // Individual effect parameters
    FilmGrainEffect grain_;
    FilmScratchesEffect scratches_;
    FilmDustEffect dust_;
    FilmVignetteEffect vignette_;
    FilmColorFadeEffect colorFade_;
    FilmFlickerEffect flicker_;
    FilmLightLeakEffect lightLeak_;
    FilmJitterEffect jitter_;
    
    // Master controls
    float masterIntensity_ = 1.0f;
    float masterSaturation_ = 1.0f;
    bool enabled_ = true;
    
public:
    explicit FilmEffectPreset(const QString& id, 
                             const QString& name,
                             FilmEffectType type,
                             QObject* parent = nullptr);
    ~FilmEffectPreset();
    
    // ID & Name
    QString id() const { return id_; }
    QString name() const { return name_; }
    void setName(const QString& name) { name_ = name; }
    
    QString description() const { return description_; }
    void setDescription(const QString& desc) { description_ = desc; }
    
    FilmEffectType type() const { return type_; }
    
    // Master controls
    float masterIntensity() const { return masterIntensity_; }
    void setMasterIntensity(float i) { masterIntensity_ = i; emit changed(); }
    
    float masterSaturation() const { return masterSaturation_; }
    void setMasterSaturation(float s) { masterSaturation_ = s; emit changed(); }
    
    bool enabled() const { return enabled_; }
    void setEnabled(bool e) { enabled_ = e; emit changed(); }
    
    // Effect getters
    FilmGrainEffect& grain() { return grain_; }
    FilmScratchesEffect& scratches() { return scratches_; }
    FilmDustEffect& dust() { return dust_; }
    FilmVignetteEffect& vignette() { return vignette_; }
    FilmColorFadeEffect& colorFade() { return colorFade_; }
    FilmFlickerEffect& flicker() { return flicker_; }
    FilmLightLeakEffect& lightLeak() { return lightLeak_; }
    FilmJitterEffect& jitter() { return jitter_; }
    
    // Apply master intensity to all effects
    void applyMasterIntensity();
    
    // Serialize
    QString toJSON() const;
    bool fromJSON(const QString& json);
    
signals:
    void changed() W_SIGNAL(changed);
    void presetLoaded() W_SIGNAL(presetLoaded);
};

// ==================== Built-in Presets ====================

class FilmPresets {
public:
    static FilmEffectPreset* createPreset(FilmEffectType type);
    
    // Common presets
    static FilmEffectPreset* oldMovie();
    static FilmEffectPreset* vintage8mm();
    static FilmEffectPreset* filmNoir();
    static FilmEffectPreset* documentary();
    static FilmEffectPreset* bleached();
    static FilmEffectPreset* crossProcessed();
    static FilmEffectPreset* damagedFilm();
    static FilmEffectPreset* sepiaTone();
    static FilmEffectPreset* fadeOut();
};

/**
 * @brief Film effect processor - applies effects to frames
 */
class FilmEffectProcessor : public QObject {
    W_OBJECT(FilmEffectProcessor)
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    
public:
    explicit FilmEffectProcessor(QObject* parent = nullptr);
    ~FilmEffectProcessor();
    
    // Load preset
    void setPreset(FilmEffectPreset* preset);
    FilmEffectPreset* currentPreset() const;
    
    // Process frame
    void process(float* pixels, int width, int height, float time);
    
    // Real-time parameter updates
    void setGrainIntensity(float intensity);
    void setVignetteIntensity(float intensity);
    void setFlickerIntensity(float intensity);
    
    // Seed for random (for frame variation)
    void setSeed(int seed);
    void randomizeSeed();
    
    // Presets
    static std::vector<FilmEffectPreset*> allPresets();
    static FilmEffectPreset* getPresetById(const QString& id);
    
signals:
    void presetChanged(FilmEffectPreset* preset) W_SIGNAL(presetChanged, preset);
    void frameProcessed() W_SIGNAL(frameProcessed);
};

} // namespace Artifact
