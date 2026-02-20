module;

#include <cmath>
#include <algorithm>
#include <QPointF>
#include <QDebug>
#include <wobjectimpl.h>

module Artifact.Effect.MotionBlur;

import std;

namespace Artifact {

W_OBJECT_IMPL(MotionBlurEffect)
W_OBJECT_IMPL(MotionEstimator)

// ==================== MotionBlurEffect::Impl ====================

class MotionBlurEffect::Impl {
public:
    // Directional blur - linear blur in specified direction
    void directionalBlur(float* pixels, int width, int height, 
                        const DirectionalBlurSettings& settings) {
        if (settings.distance <= 0.0f || settings.samples <= 0) {
            return;
        }
        
        float angleRad = settings.angle * 3.14159265f / 180.0f;
        float cosA = std::cos(angleRad);
        float sinA = std::sin(angleRad);
        
        float dx = cosA * settings.distance;
        float dy = sinA * settings.distance;
        
        // Create temp buffer
        std::vector<float> temp(width * height * 4);
        
        int samples = std::min(settings.samples, 64);
        
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float r = 0, g = 0, b = 0, a = 0;
                float totalWeight = 0;
                
                for (int s = 0; s < samples; ++s) {
                    float t = (float)s / (samples - 1) - 0.5f;
                    
                    // Sample position
                    float sx = x + dx * t;
                    float sy = y + dy * t;
                    
                    // Weight (center-weighted)
                    float weight = 1.0f - std::abs(t) * 0.5f;
                    
                    // Clamp or wrap
                    if (settings.wrapEdges) {
                        sx = std::fmod(sx + width, width);
                        sy = std::fmod(sy + height, height);
                    } else {
                        if (sx < 0 || sx >= width || sy < 0 || sy >= height) {
                            continue;
                        }
                    }
                    
                    int ix = static_cast<int>(sx);
                    int iy = static_cast<int>(sy);
                    
                    int idx = (iy * width + ix) * 4;
                    
                    r += pixels[idx] * weight;
                    g += pixels[idx + 1] * weight;
                    b += pixels[idx + 2] * weight;
                    a += pixels[idx + 3] * weight;
                    totalWeight += weight;
                }
                
                if (totalWeight > 0) {
                    int outIdx = (y * width + x) * 4;
                    temp[outIdx] = r / totalWeight;
                    temp[outIdx + 1] = g / totalWeight;
                    temp[outIdx + 2] = b / totalWeight;
                    temp[outIdx + 3] = a / totalWeight;
                }
            }
        }
        
        // Copy back
        std::copy(temp.begin(), temp.end(), pixels);
    }
    
    // Radial blur - blur outward from center
    void radialBlur(float* pixels, int width, int height,
                   const RadialBlurSettings& settings) {
        if (settings.amount <= 0.0f || settings.samples <= 0) {
            return;
        }
        
        float centerX = settings.originPoint.x() * width;
        float centerY = settings.originPoint.y() * height;
        
        std::vector<float> temp(width * height * 4);
        
        int samples = std::min(settings.samples, 64);
        
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float r = 0, g = 0, b = 0, a = 0;
                float totalWeight = 0;
                
                // Direction from center
                float dirX = x - centerX;
                float dirY = y - centerY;
                
                for (int s = 0; s < samples; ++s) {
                    float t = (float)s / (samples - 1);
                    
                    // Weight with decay
                    float weight = std::pow(settings.decay, t * 10.0f);
                    
                    // Sample position (move toward or away from center)
                    float dir = settings.zoomIn ? -1.0f : 1.0f;
                    float sx = x + dirX * t * settings.amount * 0.1f * dir;
                    float sy = y + dirY * t * settings.amount * 0.1f * dir;
                    
                    // Clamp
                    sx = std::clamp(sx, 0.0f, (float)width - 1);
                    sy = std::clamp(sy, 0.0f, (float)height - 1);
                    
                    int ix = static_cast<int>(sx);
                    int iy = static_cast<int>(sy);
                    
                    int idx = (iy * width + ix) * 4;
                    
                    r += pixels[idx] * weight;
                    g += pixels[idx + 1] * weight;
                    b += pixels[idx + 2] * weight;
                    a += pixels[idx + 3] * weight;
                    totalWeight += weight;
                }
                
                if (totalWeight > 0) {
                    int outIdx = (y * width + x) * 4;
                    temp[outIdx] = r / totalWeight;
                    temp[outIdx + 1] = g / totalWeight;
                    temp[outIdx + 2] = b / totalWeight;
                    temp[outIdx + 3] = a / totalWeight;
                }
            }
        }
        
        std::copy(temp.begin(), temp.end(), pixels);
    }
    
    // Velocity-based blur using motion vectors
    void velocityBlur(float* pixels, int width, int height,
                     const float* motionVectors,
                     const VelocityBlurSettings& settings) {
        if (!motionVectors || settings.intensity <= 0.0f) {
            return;
        }
        
        std::vector<float> temp(width * height * 4);
        
        int samples = std::min(settings.samples, 32);
        
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float r = 0, g = 0, b = 0, a = 0;
                float totalWeight = 0;
                
                // Get motion vector at this pixel
                int mvIdx = (y * width + x) * 2; // RG channels
                float mvX = motionVectors[mvIdx] * settings.velocityScale * settings.intensity * 50.0f;
                float mvY = motionVectors[mvIdx + 1] * settings.velocityScale * settings.intensity * 50.0f;
                
                for (int s = 0; s < samples; ++s) {
                    float t = (float)s / (samples - 1) - 0.5f;
                    
                    // Sample position
                    float sx = x + mvX * t;
                    float sy = y + mvY * t;
                    
                    // Weight
                    float weight = 1.0f - std::abs(t) * 0.5f;
                    
                    // Clamp
                    if (settings.clampEdges) {
                        if (sx < 0 || sx >= width || sy < 0 || sy >= height) {
                            continue;
                        }
                    } else {
                        // Wrap
                        sx = std::fmod(sx + width, width);
                        sy = std::fmod(sy + height, height);
                    }
                    
                    int ix = static_cast<int>(sx);
                    int iy = static_cast<int>(sy);
                    
                    int idx = (iy * width + ix) * 4;
                    
                    r += pixels[idx] * weight;
                    g += pixels[idx + 1] * weight;
                    b += pixels[idx + 2] * weight;
                    a += pixels[idx + 3] * weight;
                    totalWeight += weight;
                }
                
                if (totalWeight > 0) {
                    int outIdx = (y * width + x) * 4;
                    temp[outIdx] = r / totalWeight;
                    temp[outIdx + 1] = g / totalWeight;
                    temp[outIdx + 2] = b / totalWeight;
                    temp[outIdx + 3] = a / totalWeight;
                }
            }
        }
        
        std::copy(temp.begin(), temp.end(), pixels);
    }
    
    // Camera motion blur (simulates camera movement)
    void cameraBlur(float* pixels, int width, int height,
                   const CameraBlurSettings& settings) {
        if (settings.intensity <= 0.0f) {
            return;
        }
        
        // Convert shutter angle to blur distance
        float blurDist = settings.shutterAngle / 360.0f * 10.0f * settings.intensity;
        
        DirectionalBlurSettings dirSettings;
        dirSettings.distance = blurDist;
        dirSettings.samples = settings.samples;
        dirSettings.angle = settings.shutterOffset;
        dirSettings.centerX = 0.5f;
        dirSettings.centerY = 0.5f;
        
        directionalBlur(pixels, width, height, dirSettings);
    }
};

// ==================== Implementation ====================

MotionBlurEffect::MotionBlurEffect(QObject* parent)
    : QObject(parent)
    , impl_(new Impl())
{
}

MotionBlurEffect::~MotionBlurEffect() = default;

void MotionBlurEffect::process(float* pixels, int width, int height, float time) {
    if (!enabled_) return;
    
    switch (type_) {
        case MotionBlurType::Directional:
            processDirectional(pixels, width, height, time);
            break;
        case MotionBlurType::Radial:
        case MotionBlurType::Zoom:
            processRadial(pixels, width, height, time);
            break;
        case MotionBlurType::Velocity:
        case MotionBlurType::Camera:
        case MotionBlurType::Transform:
            // These need motion vectors or special handling
            break;
    }
    
    emit frameProcessed();
}

void MotionBlurEffect::processWithMotionVectors(float* pixels,
                                               const float* motionVectors,
                                               int width, int height,
                                               float time) {
    if (!enabled_) return;
    
    if (type_ == MotionBlurType::Velocity) {
        impl_->velocityBlur(pixels, width, height, motionVectors, velocitySettings_);
    } else if (type_ == MotionBlurType::Camera) {
        impl_->cameraBlur(pixels, width, height, cameraSettings_);
    }
    
    emit frameProcessed();
}

void MotionBlurEffect::processDirectional(float* pixels, int width, int height, float time) {
    if (!enabled_) return;
    
    float intensity = directionalSettings_.distance * masterIntensity_;
    DirectionalBlurSettings s = directionalSettings_;
    s.distance = intensity;
    
    impl_->directionalBlur(pixels, width, height, s);
    emit frameProcessed();
}

void MotionBlurEffect::processRadial(float* pixels, int width, int height, float time) {
    if (!enabled_) return;
    
    float intensity = radialSettings_.amount * masterIntensity_;
    RadialBlurSettings s = radialSettings_;
    s.amount = intensity;
    
    impl_->radialBlur(pixels, width, height, s);
    emit frameProcessed();
}

// ==================== Presets ====================

MotionBlurEffect* MotionBlurPresets::horizontalPan() {
    auto* e = new MotionBlurEffect();
    e->setType(MotionBlurType::Directional);
    e->directionalSettings().angle = 0;
    e->directionalSettings().distance = 15;
    e->directionalSettings().samples = 24;
    return e;
}

MotionBlurEffect* MotionBlurPresets::verticalPan() {
    auto* e = new MotionBlurEffect();
    e->setType(MotionBlurType::Directional);
    e->directionalSettings().angle = 90;
    e->directionalSettings().distance = 15;
    e->directionalSettings().samples = 24;
    return e;
}

MotionBlurEffect* MotionBlurPresets::diagonalPan() {
    auto* e = new MotionBlurEffect();
    e->setType(MotionBlurType::Directional);
    e->directionalSettings().angle = 45;
    e->directionalSettings().distance = 20;
    e->directionalSettings().samples = 32;
    return e;
}

MotionBlurEffect* MotionBlurPresets::zoomIn() {
    auto* e = new MotionBlurEffect();
    e->setType(MotionBlurType::Radial);
    e->radialSettings().amount = 15;
    e->radialSettings().samples = 24;
    e->radialSettings().zoomIn = true;
    e->radialSettings().originPoint = QVector2D(0.5f, 0.5f);
    return e;
}

MotionBlurEffect* MotionBlurPresets::zoomOut() {
    auto* e = new MotionBlurEffect();
    e->setType(MotionBlurType::Radial);
    e->radialSettings().amount = 15;
    e->radialSettings().samples = 24;
    e->radialSettings().zoomIn = false;
    e->radialSettings().originPoint = QVector2D(0.5f, 0.5f);
    return e;
}

MotionBlurEffect* MotionBlurPresets::spin() {
    auto* e = new MotionBlurEffect();
    // Spin is simulated with directional at center
    e->setType(MotionBlurType::Directional);
    e->directionalSettings().distance = 5;
    e->directionalSettings().centerX = 0.5f;
    e->directionalSettings().centerY = 0.5f;
    // Note: Real spin blur needs rotational blur
    return e;
}

MotionBlurEffect* MotionBlurPresets::fastMotion() {
    auto* e = new MotionBlurEffect();
    e->setType(MotionBlurType::Velocity);
    e->velocitySettings().intensity = 0.8f;
    e->velocitySettings().samples = 24;
    e->velocitySettings().velocityScale = 2.0f;
    return e;
}

MotionBlurEffect* MotionBlurPresets::slowMotion() {
    auto* e = new MotionBlurEffect();
    e->setType(MotionBlurType::Velocity);
    e->velocitySettings().intensity = 0.3f;
    e->velocitySettings().samples = 16;
    e->velocitySettings().velocityScale = 0.5f;
    return e;
}

MotionBlurEffect* MotionBlurPresets::cameraBlur() {
    auto* e = new MotionBlurEffect();
    e->setType(MotionBlurType::Camera);
    e->cameraSettings().shutterAngle = 180;
    e->cameraSettings().samples = 32;
    e->cameraSettings().intensity = 1.0f;
    return e;
}

MotionBlurEffect* MotionBlurPresets::dramatic() {
    auto* e = new MotionBlurEffect();
    e->setType(MotionBlurType::Directional);
    e->directionalSettings().angle = 15;
    e->directionalSettings().distance = 30;
    e->directionalSettings().samples = 48;
    e->setMasterIntensity(1.2f);
    return e;
}

MotionBlurEffect* MotionBlurPresets::subtle() {
    auto* e = new MotionBlurEffect();
    e->setType(MotionBlurType::Directional);
    e->directionalSettings().angle = 0;
    e->directionalSettings().distance = 5;
    e->directionalSettings().samples = 12;
    e->setMasterIntensity(0.5f);
    return e;
}

// ==================== MotionEstimator Implementation ====================

class MotionEstimator::Impl {
public:
    // Simple block matching motion estimation
    void estimateBlock(const float* current, const float* previous,
                      float* output, int width, int height,
                      int blockSize, int searchRadius) {
        
        int blocksX = width / blockSize;
        int blocksY = height / blockSize;
        
        for (int by = 0; by < blocksY; ++by) {
            for (int bx = 0; bx < blocksX; ++bx) {
                int blockX = bx * blockSize;
                int blockY = by * blockSize;
                
                // Get current block average color
                float currR = 0, currG = 0, currB = 0;
                for (int y = 0; y < blockSize && blockY + y < height; ++y) {
                    for (int x = 0; x < blockSize && blockX + x < width; ++x) {
                        int idx = (blockY + y) * width + (blockX + x);
                        currR += current[idx * 4];
                        currG += current[idx * 4 + 1];
                        currB += current[idx * 4 + 2];
                    }
                }
                currR /= (blockSize * blockSize);
                currG /= (blockSize * blockSize);
                currB /= (blockSize * blockSize);
                
                // Search in previous frame
                float bestX = 0, bestY = 0;
                float bestDist = 1e10f;
                
                for (int dy = -searchRadius; dy <= searchRadius; ++dy) {
                    for (int dx = -searchRadius; dx <= searchRadius; ++dx) {
                        int prevX = blockX + dx;
                        int prevY = blockY + dy;
                        
                        if (prevX < 0 || prevX >= width - blockSize ||
                            prevY < 0 || prevY >= height - blockSize) {
                            continue;
                        }
                        
                        // Calculate difference
                        float diffR = 0, diffG = 0, diffB = 0;
                        for (int y = 0; y < blockSize; ++y) {
                            for (int x = 0; x < blockSize; ++x) {
                                int idx = (prevY + y) * width + (prevX + x);
                                diffR += std::abs(current[(blockY + y) * width + (blockX + x) * 4] - 
                                                 previous[idx * 4]);
                                diffG += std::abs(current[(blockY + y) * width + (blockX + x) * 4 + 1] - 
                                                 previous[idx * 4 + 1]);
                                diffB += std::abs(current[(blockY + y) * width + (blockX + x) * 4 + 2] - 
                                                 previous[idx * 4 + 2]);
                            }
                        }
                        
                        float dist = diffR + diffG + diffB;
                        
                        if (dist < bestDist) {
                            bestDist = dist;
                            bestX = (float)dx;
                            bestY = (float)dy;
                        }
                    }
                }
                
                // Write motion vector for all pixels in block
                for (int y = 0; y < blockSize && blockY + y < height; ++y) {
                    for (int x = 0; x < blockSize && blockX + x < width; ++x) {
                        int idx = ((blockY + y) * width + (blockX + x)) * 2;
                        output[idx] = bestX;
                        output[idx + 1] = bestY;
                    }
                }
            }
        }
    }
};

MotionEstimator::MotionEstimator(QObject* parent)
    : QObject(parent)
    , impl_(new Impl())
{
}

MotionEstimator::~MotionEstimator() = default;

void MotionEstimator::estimate(const float* currentFrame,
                               const float* previousFrame,
                               float* outputMotionVectors,
                               int width, int height) {
    impl_->estimateBlock(currentFrame, previousFrame, outputMotionVectors,
                        width, height, blockSize_, searchRadius_);
    emit estimationComplete();
}

} // namespace Artifact
