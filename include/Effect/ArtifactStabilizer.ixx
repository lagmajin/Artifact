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
export module Artifact.Effect.Stabilizer;
#include <wobjectdefs.h>
#include <QObject>
#include <QString>
#include <QVector>
#include <QImage>
#include <QPointF>
#include <QSize>
#include <QColor>




import Frame.Position;
import Video.Stabilizer;

export namespace Artifact {

// Re-export ArtifactCore types
using ArtifactCore::FeatureTrack;
using ArtifactCore::FrameMotion;
using ArtifactCore::FeatureDetectionParams;
using ArtifactCore::StabilizerParams;
using ArtifactCore::FramePosition;

/**
 * @brief Video stabilization effect
 */
class StabilizerEffect : public QObject {
    W_OBJECT(StabilizerEffect)
public:
    explicit StabilizerEffect(QObject* parent = nullptr);
    ~StabilizerEffect();
    
    // Parameters
    StabilizerParams& params() { return params_; }
    const StabilizerParams& params() const { return params_; }
    
    void setParams(const StabilizerParams& params);
    
    // Frame processing
    QImage processFrame(const QImage& frame, int frameIndex);
    
    // Feature tracking
    QVector<FeatureTrack> getFeatureTracks() const { return featureTracks_; }
    void setFeatureTracks(const QVector<FeatureTrack>& tracks);
    
    // Motion data
    QVector<FrameMotion> getFrameMotions() const { return frameMotions_; }
    QVector<FrameMotion> getSmoothedMotions() const { return smoothedMotions_; }
    
    // Stabilization
    bool stabilize();
    bool isStabilized() const { return stabilized_; }
    
    // Frame management
    void addFrame(const QImage& frame, const FramePosition& pos = FramePosition(0));
    void clearFrames();
    int frameCount() const { return frames_.size(); }
    
    // Processing state
    double getProcessingTime() const { return processingTime_; }
    int getFramesProcessed() const { return processedFrames_; }
    int getFeaturesDetected() const { return totalFeatures_; }
    
    // Debug visualization
    QImage visualizeFeatures(const QImage& frame, const QVector<QPointF>& features) const;
    QImage visualizeMotionVectors(const QImage& frame, const QVector<FeatureTrack>& tracks) const;
    
signals:
    void progressChanged(int current, int total) W_SIGNAL(progressChanged, current, total);
    void stabilizationComplete() W_SIGNAL(stabilizationComplete);
    void featuresDetected(const QVector<FeatureTrack>& tracks) W_SIGNAL(featuresDetected, tracks);
    void motionEstimated(const QVector<FrameMotion>& motions) W_SIGNAL(motionEstimated, motions);
    
private:
    StabilizerParams params_;
    QVector<QImage> frames_;
    QVector<FramePosition> framePositions_;
    QVector<FeatureTrack> featureTracks_;
    QVector<FrameMotion> frameMotions_;
    QVector<FrameMotion> smoothedMotions_;
    bool stabilized_ = false;
    double processingTime_ = 0.0;
    int processedFrames_ = 0;
    int totalFeatures_ = 0;
};

/**
 * @brief Live video stabilizer for real-time processing
 */
class LiveStabilizer : public QObject {
    W_OBJECT(LiveStabilizer)
public:
    explicit LiveStabilizer(QObject* parent = nullptr);
    ~LiveStabilizer();
    
    StabilizerParams& params() { return params_; }
    const StabilizerParams& params() const { return params_; }
    
    void setParams(const StabilizerParams& params);
    void setMaxHistorySize(int size);
    int maxHistorySize() const { return maxHistorySize_; }
    
    QImage processFrame(const QImage& frame);
    void reset();
    
signals:
    void stabilizationComplete() W_SIGNAL(stabilizationComplete);
    void frameProcessed(const QImage& stabilizedFrame) W_SIGNAL(frameProcessed, stabilizedFrame);
    
private:
    StabilizerParams params_;
    int maxHistorySize_ = 30;
    QVector<QImage> history_;
    QVector<FrameMotion> motionHistory_;
    bool initialized_ = false;
};

/**
 * @brief Batch stabilizer for video files
 */
class BatchStabilizer : public QObject {
    W_OBJECT(BatchStabilizer)
public:
    explicit BatchStabilizer(QObject* parent = nullptr);
    ~BatchStabilizer();
    
    StabilizerParams& params() { return params_; }
    const StabilizerParams& params() const { return params_; }
    
    void setParams(const StabilizerParams& params);
    void setInputFile(const QString& filePath);
    QString inputFile() const { return inputFile_; }
    void setOutputFile(const QString& filePath);
    QString outputFile() const { return outputFile_; }
    
    bool process();
    bool isProcessing() const { return isProcessing_; }
    
signals:
    void progressChanged(int current, int total) W_SIGNAL(progressChanged, current, total);
    void stabilizationComplete() W_SIGNAL(stabilizationComplete);
    void errorOccurred(const QString& error) W_SIGNAL(errorOccurred, error);
    
private:
    StabilizerParams params_;
    QString inputFile_;
    QString outputFile_;
    bool isProcessing_ = false;
    int currentFrame_ = 0;
    int totalFrames_ = 0;
};

/**
 * @brief Stabilization preset manager
 */
class StabilizationPreset : public QObject {
    W_OBJECT(StabilizationPreset)
public:
    enum class PresetType {
        Default,
        Smooth,
        Strong,
        Cinematic,
        Quick,
        Custom
    };
    
    explicit StabilizationPreset(QObject* parent = nullptr);
    ~StabilizationPreset();
    
    static StabilizerParams getPreset(PresetType presetType);
    static QString presetName(PresetType presetType);
    static QString presetDescription(PresetType presetType);
    
    static StabilizerParams defaultPreset();
    static StabilizerParams smoothPreset();
    static StabilizerParams strongPreset();
    static StabilizerParams cinematicPreset();
    static StabilizerParams quickPreset();
    
    static QVector<PresetType> allPresets();
};

} // namespace Artifact
