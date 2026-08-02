module;
#include <cmath>
#include <algorithm>
#include <vector>
#include <wobjectimpl.h>
#include <QFileInfo>
#include <QDir>

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
import Core.Parallel;

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
    if (!params_.outputSize.isEmpty()) {
        params_.outputSize.setWidth(std::clamp(params_.outputSize.width(), 1, 16384));
        params_.outputSize.setHeight(std::clamp(params_.outputSize.height(), 1, 16384));
    }
    params_.borderFill = std::isfinite(params_.borderFill)
        ? std::clamp(params_.borderFill, 0.0, 1.0) : 0.0;
    params_.smoothingWindowSize = std::clamp(params_.smoothingWindowSize, 1, 10000);
    frameMotions_.clear();
    smoothedMotions_.clear();
    stabilized_ = false;
}

QImage StabilizerEffect::processFrame(const QImage& frame, int frameIndex)
{
    if (frame.isNull() || frame.width() <= 0 || frame.height() <= 0 ||
        frame.width() > 16384 || frame.height() > 16384) {
        return frame;
    }
    if (!stabilized_ || frameIndex < 0 || frameIndex >= smoothedMotions_.size()) {
        return frame;
    }
    
    // Apply stabilization transform
    FrameMotion motion = smoothedMotions_[frameIndex];
    FrameMotion inverse = motion.inverted();
    
    QSize outputSize = params_.outputSize.isEmpty() ? frame.size() : params_.outputSize;
    if (outputSize.width() <= 0 || outputSize.height() <= 0) {
        outputSize = frame.size();
    }
    
    // Simple transformation implementation (would use OpenCV or similar for production)
    QImage result(outputSize, QImage::Format_ARGB32_Premultiplied);
    const QImage source = (frame.format() == QImage::Format_RGB32 ||
                           frame.format() == QImage::Format_ARGB32)
        ? frame
        : frame.convertToFormat(QImage::Format_ARGB32);
    int w = outputSize.width();
    int h = outputSize.height();
    
    // Create transformation matrix
    const double rotation = std::isfinite(inverse.rotation) ? inverse.rotation : 0.0;
    const double scale = std::isfinite(inverse.scale) &&
                         std::abs(inverse.scale) > 1.0e-6
        ? inverse.scale : 1.0;
    const double translationX = std::isfinite(inverse.x) ? inverse.x : 0.0;
    const double translationY = std::isfinite(inverse.y) ? inverse.y : 0.0;
    double cosRot = std::cos(rotation);
    double sinRot = std::sin(rotation);
    
    ArtifactCore::Parallel::For(0, h, w * h, [&](int y) {
        auto* resultRow = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < w; x++) {
            double srcX = x - outputSize.width() / 2.0;
            double srcY = y - outputSize.height() / 2.0;
            
            double dstX = srcX * cosRot - srcY * sinRot;
            double dstY = srcX * sinRot + srcY * cosRot;
            
            dstX /= scale;
            dstY /= scale;
            
            dstX += frame.width() / 2.0 - translationX;
            dstY += frame.height() / 2.0 - translationY;
            
            if (dstX >= 0 && dstX < source.width() && dstY >= 0 && dstY < source.height()) {
                int sx = static_cast<int>(std::floor(dstX));
                int sy = static_cast<int>(std::floor(dstY));
                
                if (sx >= 0 && sx < source.width() - 1 && sy >= 0 && sy < source.height() - 1) {
                    // Simple bilinear interpolation
                    double fracX = dstX - sx;
                    double fracY = dstY - sy;
                    
                    const auto* row0 = reinterpret_cast<const QRgb*>(source.constScanLine(sy));
                    const auto* row1 = reinterpret_cast<const QRgb*>(source.constScanLine(sy + 1));
                    QRgb c00 = row0[sx];
                    QRgb c01 = row0[sx + 1];
                    QRgb c10 = row1[sx];
                    QRgb c11 = row1[sx + 1];
                    
                    QRgb c0 = interpolatePixel(c00, c01, fracX);
                    QRgb c1 = interpolatePixel(c10, c11, fracX);
                    QRgb c = interpolatePixel(c0, c1, fracY);
                    
                    resultRow[x] = c;
                } else {
                    resultRow[x] = reinterpret_cast<const QRgb*>(source.constScanLine(sy))[sx];
                }
            } else {
                // Border handling
                QRgb borderColor = qRgb(0, 0, 0);
                if (params_.borderFill > 0.0 && std::isfinite(dstX) &&
                    std::isfinite(dstY)) {
                    int bx = static_cast<int>(std::clamp(
                        dstX, 0.0, static_cast<double>(source.width() - 1)));
                    int by = static_cast<int>(std::clamp(
                        dstY, 0.0, static_cast<double>(source.height() - 1)));
                    borderColor = reinterpret_cast<const QRgb*>(source.constScanLine(by))[bx];
                }
                resultRow[x] = borderColor;
            }
        }
    });
    
    return result;
}

QRgb StabilizerEffect::interpolatePixel(QRgb c0, QRgb c1, double t) const
{
    const double safeT = std::isfinite(t) ? std::clamp(t, 0.0, 1.0) : 0.0;
    const auto channel = [safeT](const int first, const int second) {
        return std::clamp(static_cast<int>(std::lround(
            first * (1.0 - safeT) + second * safeT)), 0, 255);
    };
    const int r = channel(qRed(c0), qRed(c1));
    const int g = channel(qGreen(c0), qGreen(c1));
    const int b = channel(qBlue(c0), qBlue(c1));
    const int a = channel(qAlpha(c0), qAlpha(c1));
    
    return qRgba(r, g, b, a);
}

void StabilizerEffect::setFeatureTracks(const QVector<FeatureTrack>& tracks)
{
    featureTracks_ = tracks;
    frameMotions_.clear();
    smoothedMotions_.clear();
    stabilized_ = false;
    totalFeatures_ = 0;
    for (const auto& track : featureTracks_) {
        if (track.valid) {
            ++totalFeatures_;
        }
    }
    emit featuresDetected(featureTracks_);
}

bool StabilizerEffect::stabilize()
{
    if (frames_.empty()) {
        return false;
    }

    // 再実行時に前回の平滑化結果を残さない。
    smoothedMotions_.clear();
    stabilized_ = false;
    
    if (params_.outputSize.isEmpty()) {
        params_.outputSize = frames_.first().size();
    }

    if (frames_.size() == 1) {
        frameMotions_.clear();
        smoothedMotions_.clear();
        smoothedMotions_.append(FrameMotion{});
        stabilized_ = true;
        emit stabilizationComplete();
        return true;
    }
    
    // Feature detection and tracking
    if (!trackFeaturesBetweenFrames()) {
        return false;
    }
    
    // Motion estimation
    estimateFrameMotions();
    if (frameMotions_.isEmpty()) {
        return false;
    }
    
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
    totalFeatures_ = 0;
    
    for (int i = 1; i < frames_.size(); i++) {
        QVector<QPointF> featuresPrev, featuresCurr;
        
        if (i == 1) {
            featuresPrev = detectFeatures(frames_[0]);
            totalFeatures_ = featuresPrev.size();
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

    // 追跡途中では、まだ後続フレームの位置が揃っていない。
    // 全フレームを処理した後に完全なトラックだけを有効扱いにする。
    for (auto& track : featureTracks_) {
        track.valid = track.positions.size() == frames_.size();
    }

    if (featureTracks_.isEmpty()) {
        return false;
    }
    totalFeatures_ = 0;
    for (const auto& track : featureTracks_) {
        if (track.valid) {
            ++totalFeatures_;
        }
    }
    
    emit featuresDetected(featureTracks_);
    return true;
}

QVector<QPointF> StabilizerEffect::detectFeatures(const QImage& frame) const
{
    QVector<QPointF> features;
    struct Candidate {
        QPointF point;
        double response = 0.0;
    };
    std::vector<Candidate> candidates;
    
    int w = frame.width();
    int h = frame.height();
    if (w <= 2 || h <= 2) {
        return features;
    }
    const int blockSize = std::max(1, params_.featureParams.blockSize);
    const int minDimension = std::min(w, h);
    if (blockSize > (minDimension - 2) / 2) {
        return features;
    }
    const double qualityLevel = std::isfinite(params_.featureParams.qualityLevel)
        ? params_.featureParams.qualityLevel : 0.0;

    for (int y = blockSize; y < h - blockSize; y += 2) {
        for (int x = blockSize; x < w - blockSize; x += 2) {
            double sumGxx = 0.0;
            double sumGyy = 0.0;
            double sumGxy = 0.0;
            for (int ky = -blockSize; ky <= blockSize; ky++) {
                for (int kx = -blockSize; kx <= blockSize; kx++) {
                    const int px = x + kx;
                    const int py = y + ky;
                    const double gx = static_cast<double>(qGray(frame.pixel(px + 1, py)))
                                    - static_cast<double>(qGray(frame.pixel(px - 1, py)));
                    const double gy = static_cast<double>(qGray(frame.pixel(px, py + 1)))
                                    - static_cast<double>(qGray(frame.pixel(px, py - 1)));
                    sumGxx += gx * gx;
                    sumGyy += gy * gy;
                    sumGxy += gx * gy;
                }
            }

            const double det = sumGxx * sumGyy - sumGxy * sumGxy;
            const double trace = sumGxx + sumGyy;
            const double cornerResponse = det - 0.04 * trace * trace;
            if (cornerResponse > qualityLevel) {
                candidates.push_back({QPointF(x, y), cornerResponse});
            }
        }
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) {
                  return a.response > b.response;
              });

    const int maxFeatures = std::max(0, params_.featureParams.maxFeatures);
    if (maxFeatures == 0) {
        return features;
    }
    const double minDistance = std::isfinite(params_.featureParams.minDistance)
        ? std::max(0.0, params_.featureParams.minDistance) : 0.0;
    const double minDistanceSquared = minDistance * minDistance;
    for (const auto& candidate : candidates) {
        bool sufficientlySeparated = true;
        for (const auto& selected : features) {
            const double dx = candidate.point.x() - selected.x();
            const double dy = candidate.point.y() - selected.y();
            if (dx * dx + dy * dy < minDistanceSquared) {
                sufficientlySeparated = false;
                break;
            }
        }
        if (sufficientlySeparated) {
            features.append(candidate.point);
            if (features.size() >= maxFeatures) {
                break;
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
    if (prevFrame.isNull() || currFrame.isNull() ||
        prevFrame.width() <= 0 || prevFrame.height() <= 0 ||
        currFrame.width() <= 0 || currFrame.height() <= 0) {
        return matches;
    }
    
    for (int i = 0; i < prevFeatures.size(); i++) {
        if (!std::isfinite(prevFeatures[i].x()) ||
            !std::isfinite(prevFeatures[i].y())) {
            continue;
        }
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
    // trackFeatures() keeps the detected candidates and appends the selected
    // best-match positions. The appended range is the actual track output.
    for (auto& track : featureTracks_) {
        track.valid = false;
    }
    const int matchedOffset = currFeatures.size() - matches.size();
    for (int i = 0; i < matches.size(); i++) {
        const int matchedIndex = matchedOffset + i;
        if (matches[i] >= 0 && matches[i] < featureTracks_.size()
            && matchedIndex >= 0 && matchedIndex < currFeatures.size()) {
            featureTracks_[matches[i]].positions << currFeatures[matchedIndex];
            featureTracks_[matches[i]].valid = true;
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
    
    // Estimate a 2D similarity transform from centered point pairs.
    // This keeps translation separate from rotation/scale so the stabilizer
    // can apply the same transform model in processFrame().
    QPointF prevCenter;
    QPointF currCenter;
    const int pointCount = std::min(prevPoints.size(), currPoints.size());
    for (int i = 0; i < pointCount; ++i) {
        prevCenter += prevPoints[i];
        currCenter += currPoints[i];
    }
    prevCenter /= static_cast<double>(pointCount);
    currCenter /= static_cast<double>(pointCount);

    double dot = 0.0;
    double cross = 0.0;
    double prevMagnitude = 0.0;
    for (int i = 0; i < pointCount; ++i) {
        const QPointF p = prevPoints[i] - prevCenter;
        const QPointF q = currPoints[i] - currCenter;
        dot += p.x() * q.x() + p.y() * q.y();
        cross += p.x() * q.y() - p.y() * q.x();
        prevMagnitude += p.x() * p.x() + p.y() * p.y();
    }

    const bool canEstimateShape = prevMagnitude > 1e-9;
    const double estimatedRotation = canEstimateShape ? std::atan2(cross, dot) : 0.0;
    const double estimatedScale = canEstimateShape
        ? std::max(1e-6, std::sqrt(dot * dot + cross * cross) / prevMagnitude)
        : 1.0;
    const double effectiveRotation = params_.stabilizeRotation ? estimatedRotation : 0.0;
    const double cosRotation = std::cos(effectiveRotation);
    const double sinRotation = std::sin(effectiveRotation);
    const double effectiveScale = params_.stabilizeScale ? estimatedScale : 1.0;

    const QPointF transformedPrev(
        effectiveScale * (cosRotation * prevCenter.x() - sinRotation * prevCenter.y()),
        effectiveScale * (sinRotation * prevCenter.x() + cosRotation * prevCenter.y()));

    motion.x = currCenter.x() - transformedPrev.x();
    motion.y = currCenter.y() - transformedPrev.y();
    motion.rotation = effectiveRotation;
    motion.scale = effectiveScale;
    motion.center = prevCenter;
    
    return motion;
}

void StabilizerEffect::smoothMotions()
{
    if (frameMotions_.empty()) {
        return;
    }

    smoothedMotions_.clear();
    
    int window = std::max(1, params_.smoothingWindowSize);
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
    if (frame.isNull() || frame.width() <= 0 || frame.height() <= 0 ||
        frame.width() > 16384 || frame.height() > 16384 ||
        frames_.size() >= 10000) {
        return;
    }
    frames_.push_back(frame);
    framePositions_.push_back(pos);
    processedFrames_ = frames_.size();
    frameMotions_.clear();
    smoothedMotions_.clear();
    stabilized_ = false;
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
    totalFeatures_ = 0;
    processingTime_ = 0.0;
}

QImage StabilizerEffect::visualizeFeatures(const QImage& frame, const QVector<QPointF>& features) const
{
    QImage result = frame.copy();
    QPainter painter(&result);
    painter.setPen(QPen(params_.debugColor, 2));
    
    for (const auto& point : features) {
        if (!std::isfinite(point.x()) || !std::isfinite(point.y())) {
            continue;
        }
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
            const auto& from = track.positions[i - 1];
            const auto& to = track.positions[i];
            if (!std::isfinite(from.x()) || !std::isfinite(from.y()) ||
                !std::isfinite(to.x()) || !std::isfinite(to.y())) {
                continue;
            }
            painter.drawLine(from, to);
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
    if (!params_.outputSize.isEmpty()) {
        params_.outputSize.setWidth(std::clamp(params_.outputSize.width(), 1, 16384));
        params_.outputSize.setHeight(std::clamp(params_.outputSize.height(), 1, 16384));
    }
    params_.borderFill = std::isfinite(params_.borderFill)
        ? std::clamp(params_.borderFill, 0.0, 1.0) : 0.0;
    params_.smoothingWindowSize = std::clamp(params_.smoothingWindowSize, 1, 10000);
    history_.clear();
    motionHistory_.clear();
    initialized_ = false;
}

void LiveStabilizer::setMaxHistorySize(int size)
{
    maxHistorySize_ = std::clamp(size, 1, 10000);
    if (history_.size() > maxHistorySize_) {
        history_.erase(history_.begin(), history_.begin() + (history_.size() - maxHistorySize_));
        motionHistory_.erase(motionHistory_.begin(), motionHistory_.begin() + (motionHistory_.size() - maxHistorySize_));
    }
}

QImage LiveStabilizer::processFrame(const QImage& frame)
{
    if (frame.isNull() || frame.width() <= 0 || frame.height() <= 0 ||
        frame.width() > 16384 || frame.height() > 16384) {
        return frame;
    }
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
    if (!params_.outputSize.isEmpty()) {
        params_.outputSize.setWidth(std::clamp(params_.outputSize.width(), 1, 16384));
        params_.outputSize.setHeight(std::clamp(params_.outputSize.height(), 1, 16384));
    }
    params_.borderFill = std::isfinite(params_.borderFill)
        ? std::clamp(params_.borderFill, 0.0, 1.0) : 0.0;
    params_.smoothingWindowSize = std::clamp(params_.smoothingWindowSize, 1, 10000);
    if (!isProcessing_) {
        currentFrame_ = 0;
        totalFrames_ = 0;
    }
}

void BatchStabilizer::setInputFile(const QString& filePath)
{
    inputFile_ = filePath.trimmed();
}

void BatchStabilizer::setOutputFile(const QString& filePath)
{
    outputFile_ = filePath.trimmed();
}

bool BatchStabilizer::process()
{
    if (inputFile_.isEmpty() || outputFile_.isEmpty() || isProcessing_) {
        return false;
    }
    const QFileInfo inputInfo(inputFile_);
    if (!inputInfo.isFile() ||
        QDir::cleanPath(inputInfo.absoluteFilePath()) ==
            QDir::cleanPath(QFileInfo(outputFile_).absoluteFilePath())) {
        return false;
    }
    const QFileInfo outputInfo(outputFile_);
    if (outputInfo.exists() && !outputInfo.isFile()) {
        return false;
    }
    if (!QDir().mkpath(outputInfo.absolutePath())) {
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
