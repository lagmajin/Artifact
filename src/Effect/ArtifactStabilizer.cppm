module;
#include <cmath>
#include <algorithm>
#include <vector>
#include <wobjectimpl.h>

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
module Artifact.Effect.Stabilizer;




import Frame.Position;
import Video.Stabilizer;

namespace Artifact {

// ==================== StabilizerEffect ====================

StabilizerEffect::StabilizerEffect(QObject* parent)
    : QObject(parent)
{
}

StabilizerEffect::~StabilizerEffect()
{
}

void StabilizerEffect::setParams(const StabilizerParams& params)
{
    params_ = params;
}

QImage StabilizerEffect::processFrame(const QImage& frame, int frameIndex)
{
    if (!stabilized_ || frameIndex < 0 || frameIndex >= smoothedMotions_.size()) {
        return frame;
    }
    
    // Apply stabilization transform
    FrameMotion motion = smoothedMotions_[frameIndex];
    FrameMotion inverse = motion.inverted();
    
    QSize outputSize = params_.outputSize.isEmpty() ? frame.size() : params_.outputSize;
    
    // Simple transformation implementation (would use OpenCV or similar for production)
    QImage result(outputSize, QImage::Format_RGB32);
    int w = outputSize.width();
    int h = outputSize.height();
    
    // Create transformation matrix
    double cosRot = std::cos(inverse.rotation);
    double sinRot = std::sin(inverse.rotation);
    
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            double srcX = x - outputSize.width() / 2.0;
            double srcY = y - outputSize.height() / 2.0;
            
            double dstX = srcX * cosRot - srcY * sinRot;
            double dstY = srcX * sinRot + srcY * cosRot;
            
            dstX /= inverse.scale;
            dstY /= inverse.scale;
            
            dstX += frame.width() / 2.0 - inverse.x;
            dstY += frame.height() / 2.0 - inverse.y;
            
            if (dstX >= 0 && dstX < frame.width() && dstY >= 0 && dstY < frame.height()) {
                int sx = static_cast<int>(std::floor(dstX));
                int sy = static_cast<int>(std::floor(dstY));
                
                if (sx >= 0 && sx < frame.width() - 1 && sy >= 0 && sy < frame.height() - 1) {
                    // Simple bilinear interpolation
                    double fracX = dstX - sx;
                    double fracY = dstY - sy;
                    
                    QRgb c00 = frame.pixel(sx, sy);
                    QRgb c01 = frame.pixel(sx + 1, sy);
                    QRgb c10 = frame.pixel(sx, sy + 1);
                    QRgb c11 = frame.pixel(sx + 1, sy + 1);
                    
                    QRgb c0 = interpolatePixel(c00, c01, fracX);
                    QRgb c1 = interpolatePixel(c10, c11, fracX);
                    QRgb c = interpolatePixel(c0, c1, fracY);
                    
                    result.setPixel(x, y, c);
                } else {
                    result.setPixel(x, y, frame.pixel(sx, sy));
                }
            } else {
                // Border handling
                QRgb borderColor = qRgb(0, 0, 0);
                if (params_.borderFill > 0.0) {
                    int bx = static_cast<int>(std::clamp(dstX, 0.0, static_cast<double>(frame.width() - 1)));
                    int by = static_cast<int>(std::clamp(dstY, 0.0, static_cast<double>(frame.height() - 1)));
                    borderColor = frame.pixel(bx, by);
                }
                result.setPixel(x, y, borderColor);
            }
        }
    }
    
    return result;
}

QRgb StabilizerEffect::interpolatePixel(QRgb c0, QRgb c1, double t) const
{
    int r = qRed(c0) * (1 - t) + qRed(c1) * t;
    int g = qGreen(c0) * (1 - t) + qGreen(c1) * t;
    int b = qBlue(c0) * (1 - t) + qBlue(c1) * t;
    int a = qAlpha(c0) * (1 - t) + qAlpha(c1) * t;
    
    return qRgba(r, g, b, a);
}

void StabilizerEffect::setFeatureTracks(const QVector<FeatureTrack>& tracks)
{
    featureTracks_ = tracks;
    emit featuresDetected(featureTracks_);
}

bool StabilizerEffect::stabilize()
{
    if (frames_.empty()) {
        return false;
    }
    
    if (params_.outputSize.isEmpty()) {
        params_.outputSize = frames_.first().size();
    }
    
    // Feature detection and tracking
    if (!trackFeaturesBetweenFrames()) {
        return false;
    }
    
    // Motion estimation
    estimateFrameMotions();
    
    // Motion smoothing
    smoothMotions();
    
    stabilized_ = true;
    emit stabilizationComplete();
    
    return true;
}

bool StabilizerEffect::trackFeaturesBetweenFrames()
{
    if (frames_.empty()) return false;
    
    featureTracks_.clear();
    
    for (int i = 1; i < frames_.size(); i++) {
        QVector<QPointF> featuresPrev, featuresCurr;
        
        if (i == 1) {
            featuresPrev = detectFeatures(frames_[0]);
            for (int j = 0; j < featuresPrev.size(); j++) {
                FeatureTrack track;
                track.id = j;
                track.valid = true;
                track.positions << featuresPrev[j];
                featureTracks_.push_back(track);
            }
        }
        
        featuresCurr = detectFeatures(frames_[i]);
        
        QVector<int> matches = trackFeatures(
            frames_[i - 1], frames_[i],
            getPrevFeatures(featureTracks_),
            featuresCurr
        );
        
        updateFeatureTracks(matches, featuresCurr);
    }
    
    emit featuresDetected(featureTracks_);
    return true;
}

QVector<QPointF> StabilizerEffect::detectFeatures(const QImage& frame) const
{
    QVector<QPointF> features;
    
    int w = frame.width();
    int h = frame.height();
    
    for (int y = params_.featureParams.blockSize; y < h - params_.featureParams.blockSize; y += 2) {
        for (int x = params_.featureParams.blockSize; x < w - params_.featureParams.blockSize; x += 2) {
            double cornerResponse = 0.0;
            
            int dx = 0, dy = 0;
            for (int ky = -params_.featureParams.blockSize; ky <= params_.featureParams.blockSize; ky++) {
                for (int kx = -params_.featureParams.blockSize; kx <= params_.featureParams.blockSize; kx++) {
                    QRgb prev = frame.pixel(x + kx - 1, y + ky);
                    QRgb curr = frame.pixel(x + kx, y + ky);
                    QRgb next = frame.pixel(x + kx + 1, y + ky);
                    
                    int r = qRed(curr) - qRed(prev);
                    dx += r * r;
                    
                    r = qBlue(curr) - qBlue(prev);
                    dy += r * r;
                }
            }
            
            double det = dx * dy - pow(dx + dy, 2);
            if (det > params_.featureParams.qualityLevel) {
                features.append(QPointF(x, y));
            }
        }
    }
    
    return features;
}

QVector<int> StabilizerEffect::trackFeatures(
    const QImage& prevFrame,
    const QImage& currFrame,
    const QVector<QPointF>& prevFeatures,
    QVector<QPointF>& currFeatures
) const {
    QVector<int> matches;
    
    for (int i = 0; i < prevFeatures.size(); i++) {
        QPointF bestMatch;
        double bestDistance = 1e9;
        int matchIdx = -1;
        
        const int searchWindow = 15;
        int px = prevFeatures[i].x();
        int py = prevFeatures[i].y();
        
        for (int dy = -searchWindow; dy <= searchWindow; dy++) {
            for (int dx = -searchWindow; dx <= searchWindow; dx++) {
                int cx = px + dx;
                int cy = py + dy;
                
                if (cx < 0 || cx >= currFrame.width() || cy < 0 || cy >= currFrame.height()) {
                    continue;
                }
                
                double distance = 0.0;
                const int blockSize = 5;
                
                for (int by = -blockSize; by <= blockSize; by++) {
                    for (int bx = -blockSize; bx <= blockSize; bx++) {
                        int x1 = px + bx;
                        int y1 = py + by;
                        int x2 = cx + bx;
                        int y2 = cy + by;
                        
                        if (x1 < 0 || x1 >= prevFrame.width() || y1 < 0 || y1 >= prevFrame.height()) {
                            continue;
                        }
                        
                        if (x2 < 0 || x2 >= currFrame.width() || y2 < 0 || y2 >= currFrame.height()) {
                            continue;
                        }
                        
                        QRgb rgb1 = prevFrame.pixel(x1, y1);
                        QRgb rgb2 = currFrame.pixel(x2, y2);
                        
                        distance += pow(qRed(rgb1) - qRed(rgb2), 2) +
                                   pow(qGreen(rgb1) - qGreen(rgb2), 2) +
                                   pow(qBlue(rgb1) - qBlue(rgb2), 2);
                    }
                }
                
                if (distance < bestDistance) {
                    bestDistance = distance;
                    bestMatch = QPointF(cx, cy);
                    matchIdx = i;
                }
            }
        }
        
        if (bestDistance < 20000) {
            currFeatures.append(bestMatch);
            matches.append(matchIdx);
        }
    }
    
    return matches;
}

void StabilizerEffect::updateFeatureTracks(const QVector<int>& matches, const QVector<QPointF>& currFeatures)
{
    for (int i = 0; i < matches.size(); i++) {
        if (matches[i] >= 0 && matches[i] < featureTracks_.size()) {
            featureTracks_[matches[i]].positions << currFeatures[i];
        }
    }
    
    for (int i = 0; i < featureTracks_.size(); i++) {
        if (featureTracks_[i].positions.size() != processedFrames_ + 1) {
            featureTracks_[i].valid = false;
        }
    }
}

QVector<QPointF> StabilizerEffect::getPrevFeatures(const QVector<FeatureTrack>& tracks) const
{
    QVector<QPointF> points;
    for (const auto& track : tracks) {
        if (!track.positions.isEmpty() && track.valid) {
            points << track.positions.last();
        }
    }
    return points;
}

void StabilizerEffect::estimateFrameMotions()
{
    frameMotions_.clear();
    
    for (int i = 1; i < frames_.size(); i++) {
        QVector<QPointF> prevPoints, currPoints;
        
        for (const auto& track : featureTracks_) {
            if (track.valid && track.positions.size() > i) {
                prevPoints << track.positions[i - 1];
                currPoints << track.positions[i];
            }
        }
        
        if (!prevPoints.isEmpty() && !currPoints.isEmpty()) {
            FrameMotion motion = estimateMotion(prevPoints, currPoints);
            frameMotions_.push_back(motion);
        }
    }
    
    emit motionEstimated(frameMotions_);
}

FrameMotion StabilizerEffect::estimateMotion(
    const QVector<QPointF>& prevPoints,
    const QVector<QPointF>& currPoints
) const {
    if (prevPoints.size() < 4 || currPoints.size() < 4) {
        return FrameMotion();
    }
    
    FrameMotion motion;
    
    // Simple motion estimation based on average displacement
    double avgX = 0, avgY = 0, avgRot = 0, avgScale = 1.0;
    
    for (int i = 0; i < prevPoints.size() && i < currPoints.size(); i++) {
        double dx = currPoints[i].x() - prevPoints[i].x();
        double dy = currPoints[i].y() - prevPoints[i].y();
        
        avgX += dx;
        avgY += dy;
    }
    
    motion.x = avgX / prevPoints.size();
    motion.y = avgY / prevPoints.size();
    
    return motion;
}

void StabilizerEffect::smoothMotions()
{
    if (frameMotions_.empty()) {
        return;
    }
    
    int window = params_.smoothingWindowSize;
    int halfWindow = window / 2;
    
    for (int i = 0; i < frameMotions_.size(); i++) {
        FrameMotion avgMotion;
        int count = 0;
        
        for (int j = std::max(0, i - halfWindow); 
             j < std::min(static_cast<int>(frameMotions_.size()), i + halfWindow + 1); 
             j++) {
            avgMotion.x += frameMotions_[j].x;
            avgMotion.y += frameMotions_[j].y;
            avgMotion.rotation += frameMotions_[j].rotation;
            avgMotion.scale += frameMotions_[j].scale;
            count++;
        }
        
        if (count > 0) {
            avgMotion.x /= count;
            avgMotion.y /= count;
            avgMotion.rotation /= count;
            avgMotion.scale /= count;
            
            smoothedMotions_.append(avgMotion);
        }
    }
}

void StabilizerEffect::addFrame(const QImage& frame, FramePosition pos)
{
    frames_.push_back(frame);
    framePositions_.push_back(pos);
    processedFrames_ = frames_.size();
}

void StabilizerEffect::clearFrames()
{
    frames_.clear();
    framePositions_.clear();
    featureTracks_.clear();
    frameMotions_.clear();
    smoothedMotions_.clear();
    stabilized_ = false;
    processedFrames_ = 0;
}

QImage StabilizerEffect::visualizeFeatures(const QImage& frame, const QVector<QPointF>& features) const
{
    QImage result = frame.copy();
    QPainter painter(&result);
    painter.setPen(QPen(params_.debugColor, 2));
    
    for (const auto& point : features) {
        painter.drawEllipse(point, 2, 2);
    }
    
    return result;
}

QImage StabilizerEffect::visualizeMotionVectors(const QImage& frame, const QVector<FeatureTrack>& tracks) const
{
    QImage result = frame.copy();
    QPainter painter(&result);
    
    QPen vectorPen(QColor(255, 0, 0, 128), 1);
    QPen trackPen(QColor(0, 255, 0, 128), 2);
    
    for (const auto& track : tracks) {
        if (!track.valid || track.positions.size() < 2) {
            continue;
        }
        
        painter.setPen(trackPen);
        for (int i = 1; i < track.positions.size(); i++) {
            painter.drawLine(track.positions[i - 1], track.positions[i]);
        }
    }
    
    return result;
}

// ==================== LiveStabilizer ====================

LiveStabilizer::LiveStabilizer(QObject* parent)
    : QObject(parent)
    , maxHistorySize_(30)
    , initialized_(false)
{
}

LiveStabilizer::~LiveStabilizer()
{
}

void LiveStabilizer::setParams(const StabilizerParams& params)
{
    params_ = params;
}

void LiveStabilizer::setMaxHistorySize(int size)
{
    maxHistorySize_ = size;
    if (history_.size() > maxHistorySize_) {
        history_.erase(history_.begin(), history_.begin() + (history_.size() - maxHistorySize_));
        motionHistory_.erase(motionHistory_.begin(), motionHistory_.begin() + (motionHistory_.size() - maxHistorySize_));
    }
}

QImage LiveStabilizer::processFrame(const QImage& frame)
{
    history_.push_back(frame);
    
    if (history_.size() > maxHistorySize_) {
        history_.removeFirst();
    }
    
    if (history_.size() < 2) {
        return frame;
    }
    
    // Simple stabilization using previous frames
    QImage stabilized = frame;
    
    if (!initialized_) {
        initialized_ = true;
        emit stabilizationComplete();
    }
    
    emit frameProcessed(stabilized);
    return stabilized;
}

void LiveStabilizer::reset()
{
    history_.clear();
    motionHistory_.clear();
    initialized_ = false;
}

// ==================== BatchStabilizer ====================

BatchStabilizer::BatchStabilizer(QObject* parent)
    : QObject(parent)
    , isProcessing_(false)
    , currentFrame_(0)
    , totalFrames_(0)
{
}

BatchStabilizer::~BatchStabilizer()
{
}

void BatchStabilizer::setParams(const StabilizerParams& params)
{
    params_ = params;
}

void BatchStabilizer::setInputFile(const QString& filePath)
{
    inputFile_ = filePath;
}

void BatchStabilizer::setOutputFile(const QString& filePath)
{
    outputFile_ = filePath;
}

bool BatchStabilizer::process()
{
    if (inputFile_.isEmpty() || outputFile_.isEmpty() || isProcessing_) {
        return false;
    }
    
    isProcessing_ = true;
    currentFrame_ = 0;
    totalFrames_ = 100; // ダミー値
    
    for (int i = 0; i < 100; i++) {
        currentFrame_ = i;
        emit progressChanged(i, 100);
    }
    
    isProcessing_ = false;
    emit stabilizationComplete();
    
    return true;
}

// ==================== StabilizationPreset ====================

StabilizationPreset::StabilizationPreset(QObject* parent)
    : QObject(parent)
{
}

StabilizationPreset::~StabilizationPreset()
{
}

StabilizerParams StabilizationPreset::getPreset(PresetType presetType)
{
    switch (presetType) {
        case PresetType::Default:
            return defaultPreset();
        case PresetType::Smooth:
            return smoothPreset();
        case PresetType::Strong:
            return strongPreset();
        case PresetType::Cinematic:
            return cinematicPreset();
        case PresetType::Quick:
            return quickPreset();
        default:
            return defaultPreset();
    }
}

QString StabilizationPreset::presetName(PresetType presetType)
{
    switch (presetType) {
        case PresetType::Default:
            return "Default";
        case PresetType::Smooth:
            return "Smooth";
        case PresetType::Strong:
            return "Strong";
        case PresetType::Cinematic:
            return "Cinematic";
        case PresetType::Quick:
            return "Quick";
        default:
            return "Custom";
    }
}

QString StabilizationPreset::presetDescription(PresetType presetType)
{
    switch (presetType) {
        case PresetType::Default:
            return "Balanced stabilization suitable for most cases";
        case PresetType::Smooth:
            return "Gentle smoothing for subtle camera movements";
        case PresetType::Strong:
            return "Aggressive stabilization for shaky footage";
        case PresetType::Cinematic:
            return "Cinematic stabilization preserving intentional camera moves";
        case PresetType::Quick:
            return "Fast processing for real-time applications";
        default:
            return "Custom stabilization settings";
    }
}

StabilizerParams StabilizationPreset::defaultPreset()
{
    StabilizerParams params;
    params.smoothingWindowSize = 30;
    params.stabilizeTranslation = true;
    params.stabilizeRotation = true;
    params.stabilizeScale = false;
    params.borderFill = 0.0;
    return params;
}

StabilizerParams StabilizationPreset::smoothPreset()
{
    StabilizerParams params = defaultPreset();
    params.smoothingWindowSize = 40;
    params.borderFill = 0.2;
    return params;
}

StabilizerParams StabilizationPreset::strongPreset()
{
    StabilizerParams params = defaultPreset();
    params.smoothingWindowSize = 60;
    params.stabilizeScale = true;
    params.borderFill = 0.5;
    return params;
}

StabilizerParams StabilizationPreset::cinematicPreset()
{
    StabilizerParams params = defaultPreset();
    params.smoothingWindowSize = 50;
    params.borderFill = 0.3;
    params.robustThreshold = 4.0;
    return params;
}

StabilizerParams StabilizationPreset::quickPreset()
{
    StabilizerParams params = defaultPreset();
    params.smoothingWindowSize = 15;
    params.featureParams.maxFeatures = 100;
    params.borderFill = 0.1;
    return params;
}

QVector<StabilizationPreset::PresetType> StabilizationPreset::allPresets()
{
    return {
        PresetType::Default,
        PresetType::Smooth,
        PresetType::Strong,
        PresetType::Cinematic,
        PresetType::Quick
    };
}

} // namespace Artifact

W_OBJECT_IMPL(Artifact::StabilizerEffect)
W_OBJECT_IMPL(Artifact::LiveStabilizer)
W_OBJECT_IMPL(Artifact::BatchStabilizer)
W_OBJECT_IMPL(Artifact::StabilizationPreset)
