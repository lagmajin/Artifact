module;
#include <wobjectdefs.h>
#include <QObject>
#include <QJsonArray>
#include <QJsonObject>

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
export module Artifact.Render.Queue.Service;
import Utils.Id;
import Artifact.Render.Queue.Presets;





export namespace Artifact
{
 // Forward declarations
 class ArtifactRenderJob;

 class ArtifactRenderQueueService:public QObject
 {
 	W_OBJECT(ArtifactRenderQueueService)
 private:
   class Impl;
   Impl* impl_;
 public:
 explicit ArtifactRenderQueueService(QObject*parent=nullptr);
  ~ArtifactRenderQueueService();
  static ArtifactRenderQueueService* instance();
  void addRenderQueue();
  void addRenderQueueForComposition(const ArtifactCore::CompositionID& compositionId, const QString& compositionName);
  
  // 複数形式でジョブを追加（After Effects 風）
  void addRenderQueueWithPreset(const ArtifactCore::CompositionID& compositionId, const QString& compositionName, const QString& presetId);
  void addMultipleRenderQueuesForComposition(const ArtifactCore::CompositionID& compositionId, const QString& compositionName, const QVector<QString>& presetIds);
  
  void removeRenderQueue();
  void removeRenderQueueAt(int index);
  void duplicateRenderQueueAt(int index);
  void moveRenderQueue(int fromIndex, int toIndex);
  void removeAllRenderQueues();
  void removeRenderQueuesForComposition(const ArtifactCore::CompositionID& compositionId);
  bool hasRenderQueueForComposition(const ArtifactCore::CompositionID& compositionId) const;
  int renderQueueCountForComposition(const ArtifactCore::CompositionID& compositionId) const;
  ArtifactCore::CompositionID jobCompositionIdAt(int index) const;
   QString jobCompositionNameAt(int index) const;
   QString jobNameAt(int index) const;
   void setJobNameAt(int index, const QString& name);
   QString jobStatusAt(int index) const;
  int jobProgressAt(int index) const;
  QString jobOutputPathAt(int index) const;
  void setJobOutputPathAt(int index, const QString& outputPath);
  QString jobRenderBackendAt(int index) const;
  void setJobRenderBackendAt(int index, const QString& backend);
  bool jobFrameRangeAt(int index, int* startFrame, int* endFrame) const;
  void setJobFrameRangeAt(int index, int startFrame, int endFrame);
  bool jobOutputSettingsAt(
    int index,
    QString* outputFormat,
    QString* codec,
    QString* codecProfile,
    int* width,
    int* height,
    double* fps,
    int* bitrateKbps) const;
  bool jobOutputSettingsAt(
    int index,
    QString* outputFormat,
    QString* codec,
    int* width,
    int* height,
    double* fps,
    int* bitrateKbps) const;
  void setJobOutputSettingsAt(
    int index,
    const QString& outputFormat,
    const QString& codec,
    const QString& codecProfile,
    int width,
    int height,
    double fps,
    int bitrateKbps);
  void setJobOutputSettingsAt(
    int index,
    const QString& outputFormat,
    const QString& codec,
    int width,
    int height,
    double fps,
    int bitrateKbps);
  bool jobIntegratedRenderEnabledAt(int index) const;
  void setJobIntegratedRenderEnabledAt(int index, bool enabled);
  QString jobAudioSourcePathAt(int index) const;
  void setJobAudioSourcePathAt(int index, const QString& path);
  QString jobAudioCodecAt(int index) const;
  void setJobAudioCodecAt(int index, const QString& codec);
  int jobAudioBitrateKbpsAt(int index) const;
  void setJobAudioBitrateKbpsAt(int index, int bitrateKbps);
  QString jobEncoderBackendAt(int index) const;
  void setJobEncoderBackendAt(int index, const QString& backend);
  QString jobErrorMessageAt(int index) const;
  bool jobOverlayTransformAt(int index, float* offsetX, float* offsetY, float* scale, float* rotationDeg) const;
  void setJobOverlayTransform(int index, float offsetX, float offsetY, float scale, float rotationDeg);
  void resetJobForRerun(int index);
  int resetCompletedAndFailedJobsForRerun();

  // Render Recovery: 失敗フレーム検出
  struct FailedFrameInfo {
    int jobId;
    int frameNumber;
    QString errorMessage;
    qint64 timestamp;
  };
  QList<FailedFrameInfo> detectFailedFrames(int jobIndex) const;
  int rerenderFailedFrames(int jobIndex, const QList<int>& frameNumbers);

  void startRenderQueue();
  void startAllRenderQueues();
  
  // Job control methods
  void startAllJobs();
  void pauseAllJobs();
  void cancelAllJobs();
  
  // Job query methods
  int jobCount() const;
  // ArtifactRenderJob getJob(int index) const;  // Commented out - ArtifactRenderJob not exported
  int getTotalProgress() const;
  
public:
  void jobAdded(int index) W_SIGNAL(jobAdded, index)
  void jobRemoved(int index) W_SIGNAL(jobRemoved, index)
  void jobUpdated(int index) W_SIGNAL(jobUpdated, index)
  void jobStatusChanged(int index, int status) W_SIGNAL(jobStatusChanged, index, status)
  void jobProgressChanged(int index, int progress) W_SIGNAL(jobProgressChanged, index, progress)
  void allJobsCompleted() W_SIGNAL(allJobsCompleted)
  void allJobsRemoved() W_SIGNAL(allJobsRemoved)
  void queueReordered(int fromIndex, int toIndex) W_SIGNAL(queueReordered, fromIndex, toIndex)
  void previewFrameReady(int jobIndex, int frameNumber) W_SIGNAL(previewFrameReady, jobIndex, frameNumber)

  // Callback setters (Deprecated, use signals)
  void setJobAddedCallback(std::function<void(int)> callback);
  void setJobRemovedCallback(std::function<void(int)> callback);
  void setJobUpdatedCallback(std::function<void(int)> callback);
  void setJobStatusChangedCallback(std::function<void(int, int)> callback);
  void setJobProgressChangedCallback(std::function<void(int, int)> callback);
  void setAllJobsCompletedCallback(std::function<void()> callback);
  void setAllJobsRemovedCallback(std::function<void()> callback);
   void setQueueReorderedCallback(std::function<void(int, int)> callback);

    // Serialization for project save/load
    QJsonArray toJson() const;
    void fromJson(const QJsonArray& arr);
    void clearQueueForLoad();

    // Batch rendering
    int addAllCompositions();
    int addCompositions(const QList<ArtifactCore::CompositionID>& compIds);

    // Live preview
    QImage lastRenderedFrame() const;
    int lastRenderedFrameNumber() const;
    int lastRenderedJobIndex() const;

    // Render backend selection for the render queue service.
    enum class RenderBackend { Auto, CPU, GPU };
    void setRenderBackend(RenderBackend backend);
    RenderBackend renderBackend() const;
   };



};
