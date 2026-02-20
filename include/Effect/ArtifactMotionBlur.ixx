module;
#include <QObject>
#include <QVector2D>
#include <QVector3D>
#include <QMatrix4x4>
#include <memory>
#include <wobjectdefs.h>

export module Artifact.Effect.MotionBlur;

import std;

W_REGISTER_ARGTYPE(QVector2D)
W_REGISTER_ARGTYPE(QVector3D)

export namespace Artifact {

/**
 * @brief Motion blur types
 */
enum class MotionBlurType {
    Velocity,       // Velocity-based (uses motion vectors)
    Directional,    // Linear directional blur
    Radial,          // Radial blur from center
    Zoom,            // Zoom blur from point
    Camera,         // Camera motion blur
    Transform       // Transform-based blur
};

/**
 * @brief Velocity-based motion blur settings
 */
class VelocityBlurSettings {
public:
    float intensity = 0.5f;        // 0-1, blur strength
    int samples = 16;               // number of samples
    bool useMotionVectors = true;  // use MV or estimate
    float velocityScale = 1.0f;    // velocity multiplier
    bool clampEdges = true;        // clamp at edges
    float centerBias = 0.0f;      // bias toward center
};

/**
 * @brief Directional / Linear blur settings
 */
class DirectionalBlurSettings {
public:
    float angle = 0.0f;            // degrees
    float distance = 10.0f;        // blur distance in pixels
    int samples = 16;               // number of samples
    float centerX = 0.5f;          // center X (0-1 normalized)
    float centerY = 0.5f;          // center Y (0-1 normalized)
    bool wrapEdges = false;        // wrap vs clamp
};

/**
 * @brief Radial / Zoom blur settings
 */
class RadialBlurSettings {
public:
    enum class Origin {
        Center,
        Point,
        FollowLayer
    };
    
    float amount = 10.0f;          // blur amount
    int samples = 16;               // number of samples
    Origin origin = Origin::Center;
    QVector2D originPoint = QVector2D(0.5f, 0.5f);
    float decay = 0.9f;            // sample decay
    bool zoomIn = false;           // zoom in vs out
};

/**
 * @brief Camera motion blur settings
 */
class CameraBlurSettings {
public:
    float shutterAngle = 180.0f;   // degrees (180 = half frame)
    float shutterOffset = 0.0f;    // degrees
    float intensity = 1.0f;
    bool includeStatic = true;     // include non-moving objects
    int samples = 32;
};

/**
 * @brief Motion blur effect processor
 */
class MotionBlurEffect : public QObject {
    W_OBJECT(MotionBlurEffect)
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    
    // Settings for each type
    VelocityBlurSettings velocitySettings_;
    DirectionalBlurSettings directionalSettings_;
    RadialBlurSettings radialSettings_;
    CameraBlurSettings cameraSettings_;
    
    MotionBlurType type_ = MotionBlurType::Velocity;
    bool enabled_ = true;
    float masterIntensity_ = 1.0f;
    
public:
    explicit MotionBlurEffect(QObject* parent = nullptr);
    ~MotionBlurEffect();
    
    // Type
    MotionBlurType type() const { return type_; }
    void setType(MotionBlurType type) { type_ = type; emit changed(); }
    
    // Master controls
    bool enabled() const { return enabled_; }
    void setEnabled(bool e) { enabled_ = e; emit changed(); }
    
    float masterIntensity() const { return masterIntensity_; }
    void setMasterIntensity(float i) { masterIntensity_ = i; emit changed(); }
    
    // Velocity settings
    VelocityBlurSettings& velocitySettings() { return velocitySettings_; }
    const VelocityBlurSettings& velocitySettings() const { return velocitySettings_; }
    void setVelocityIntensity(float i) { velocitySettings_.intensity = i; emit changed(); }
    
    // Directional settings
    DirectionalBlurSettings& directionalSettings() { return directionalSettings_; }
    const DirectionalBlurSettings& directionalSettings() const { return directionalSettings_; }
    void setDirectionalAngle(float deg) { directionalSettings_.angle = deg; emit changed(); }
    void setDirectionalDistance(float d) { directionalSettings_.distance = d; emit changed(); }
    
    // Radial settings
    RadialBlurSettings& radialSettings() { return radialSettings_; }
    const RadialBlurSettings& radialSettings() const { return radialSettings_; }
    void setRadialAmount(float a) { radialSettings_.amount = a; emit changed(); }
    void setRadialOrigin(float x, float y) { 
        radialSettings_.originPoint = QVector2D(x, y); 
        emit changed(); 
    }
    
    // Camera settings
    CameraBlurSettings& cameraSettings() { return cameraSettings_; }
    const CameraBlurSettings& cameraSettings() const { return cameraSettings_; }
    void setShutterAngle(float deg) { cameraSettings_.shutterAngle = deg; emit changed(); }
    
    // Process frame
    // For velocity blur, pass motion vectors as separate buffer
    void process(float* pixels, int width, int height, float time);
    void processWithMotionVectors(float* pixels, 
                                  const float* motionVectors, // RG = velocity
                                  int width, int height, 
                                  float time);
    
    // Simple directional/radial without motion vectors
    void processDirectional(float* pixels, int width, int height, float time);
    void processRadial(float* pixels, int width, int height, float time);
    
signals:
    void changed() W_SIGNAL(changed);
    void frameProcessed() W_SIGNAL(frameProcessed);
};

/**
 * @brief Motion blur presets
 */
class MotionBlurPresets {
public:
    // Directional presets
    static MotionBlurEffect* horizontalPan();
    static MotionBlurEffect* verticalPan();
    static MotionBlurEffect* diagonalPan();
    
    // Radial presets
    static MotionBlurEffect* zoomIn();
    static MotionBlurEffect* zoomOut();
    static MotionBlurEffect* spin();
    
    // Velocity presets
    static MotionBlurEffect* fastMotion();
    static MotionBlurEffect* slowMotion();
    static MotionBlurEffect* cameraBlur();
    
    // Creative
    static MotionBlurEffect* dramatic();
    static MotionBlurEffect* subtle();
};

/**
 * @brief Motion estimator - estimates motion between frames
 * 
 * Simple block-based motion estimation for when MV aren't available
 */
class MotionEstimator : public QObject {
    W_OBJECT(MotionEstimator)
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    
    int searchRadius_ = 16;
    int blockSize_ = 8;
    int frameGap_ = 1;
    
public:
    explicit MotionEstimator(QObject* parent = nullptr);
    ~MotionEstimator();
    
    // Estimate motion between two frames
    // Outputs RG float texture (R = X velocity, G = Y velocity)
    void estimate(const float* currentFrame,
                  const float* previousFrame,
                  float* outputMotionVectors,
                  int width, int height);
    
    // Settings
    int searchRadius() const { return searchRadius_; }
    void setSearchRadius(int r) { searchRadius_ = r; }
    
    int blockSize() const { return blockSize_; }
    void setBlockSize(int s) { blockSize_ = s; }
    
    int frameGap() const { return frameGap_; }
    void setFrameGap(int g) { frameGap_ = g; }
    
signals:
    void estimationComplete() W_SIGNAL(estimationComplete);
};

} // namespace Artifact
