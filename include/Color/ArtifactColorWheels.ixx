module;
#include <QObject>
#include <QVector>
#include <QColor>
#include <array>
#include <wobjectdefs.h>

export module Artifact.Color.Wheels;

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



import ColorCollection.ColorGrading;

export namespace Artifact {

// Re-export types from ArtifactCore for backwards compatibility
using ArtifactCore::ColorWheelType;
using ArtifactCore::ColorWheelParams;
using ArtifactCore::ThreeWayColorParams;
using ArtifactCore::ColorLevelsParams;
using ArtifactCore::CurvePoint;

/**
 * @brief Qt-compatible color wheel processor wrapper
 * @note Implementation delegates to ArtifactCore::ColorWheelsProcessor
 */
class ColorWheelsProcessor : public QObject {
    W_OBJECT(ColorWheelsProcessor)
private:
    class Impl;
    Impl* impl_;
    
public:
    explicit ColorWheelsProcessor(QObject* parent = nullptr);
    ~ColorWheelsProcessor();
    
    // Type
    void setWheelType(ArtifactCore::ColorWheelType type);
    ArtifactCore::ColorWheelType wheelType() const { return impl_->core_.wheelType(); }
    
    // Lift/Gamma/Gain
    ColorWheelParams& wheels() { return impl_->core_.wheels(); }
    const ColorWheelParams& wheels() const { return impl_->core_.wheels(); }
    
    void setLift(float r, float g, float b);
    void setGamma(float r, float g, float b);
    void setGain(float r, float g, float b);
    void setOffset(float r, float g, float b);
    
    // Three-way
    ThreeWayColorParams& threeWay() { return impl_->core_.threeWay(); }
    const ThreeWayColorParams& threeWay() const { return impl_->core_.threeWay(); }
    
    // Levels
    ColorLevelsParams& levels() { return impl_->core_.levels(); }
    const ColorLevelsParams& levels() const { return impl_->core_.levels(); }
    
    // Processing
    void process(float* pixels, int width, int height);
    
    // Presets
    static ColorWheelsProcessor* createWarmLook();
    static ColorWheelsProcessor* createCoolLook();
    static ColorWheelsProcessor* createHighContrast();
    static ColorWheelsProcessor* createFade();
    
    signals:
    void paramsChanged() W_SIGNAL(paramsChanged)
};

/**
 * @brief Qt-compatible color curves wrapper
 * @note Implementation delegates to ArtifactCore::ColorCurves
 */
class ColorCurves : public QObject {
    W_OBJECT(ColorCurves)
private:
    class Impl;
    Impl* impl_;
    
public:
    explicit ColorCurves(QObject* parent = nullptr);
    ~ColorCurves();
    
    // Master curve
    void setMasterCurve(const std::vector<CurvePoint>& points);
    const std::vector<CurvePoint>& masterCurve() const { return impl_->core_.masterCurve(); }
    
    // Channel curves
    void setRedCurve(const std::vector<CurvePoint>& points);
    void setGreenCurve(const std::vector<CurvePoint>& points);
    void setBlueCurve(const std::vector<CurvePoint>& points);
    
    // Predefined curves
    void applySCurve();
    void applyFadeIn();
    void applyFadeOut();
    void applyInvert();
    void applyPosterize(int levels);
    
    // Processing
    void process(float* pixels, int width, int height);
    void buildLUT();
    
    // Reset
    void reset();
    
    signals:
    void curveChanged() W_SIGNAL(curveChanged)
};

/**
 * @brief Qt-compatible color grader wrapper
 * @note Implementation delegates to ArtifactCore::ColorGrader
 */
class ColorGrader : public QObject {
    W_OBJECT(ColorGrader)
private:
    class Impl;
    Impl* impl_;
    
    ColorWheelsProcessor* wheelsProcessor_ = nullptr;
    ColorCurves* curvesProcessor_ = nullptr;
    
public:
    explicit ColorGrader(QObject* parent = nullptr);
    ~ColorGrader();
    
    // Sub-processors
    ColorWheelsProcessor* wheels() { return wheelsProcessor_; }
    ColorCurves* curves() { return curvesProcessor_; }
    
    // Levels
    ColorLevelsParams& levels() { return impl_->core_.levels(); }
    const ColorLevelsParams& levels() const { return impl_->core_.levels(); }
    
    // Master controls
    void setEnabled(bool e);
    bool isEnabled() const { return impl_->core_.isEnabled(); }
    
    void setIntensity(float i);
    float intensity() const { return impl_->core_.intensity(); }
    
    // Process
    void process(float* pixels, int width, int height);
    
    // Presets
    static ColorGrader* createFilmLook();
    static ColorGrader* createCinematic();
    static ColorGrader* createNoir();
    
    signals:
    void changed() W_SIGNAL(changed)
};

} // namespace Artifact
