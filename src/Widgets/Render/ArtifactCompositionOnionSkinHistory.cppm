module;

#include <QImage>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVector>

#include <algorithm>
#include <limits>

export module Artifact.Widgets.CompositionOnionSkinHistory;

import Artifact.Render.IRenderer;

export namespace Artifact {

class CompositionOnionSkinHistory final {
public:
  void reset();
  void trim(int frameCount);

  void draw(ArtifactIRenderer* renderer, bool enabled,
            bool hasAnimatedContent, const QString& compositionId,
            qint64 currentFrame, float canvasWidth, float canvasHeight,
            int frameCount, int opacityPercent);

  bool queueCapture(QObject* owner, ArtifactIRenderer* renderer,
                    bool enabled, bool hasAnimatedContent,
                    const QString& compositionId, qint64 currentFrame);

private:
  struct Frame {
    QImage image;
    QString compositionId;
    qint64 frame = -1;
  };

  QVector<Frame> frames_;
  bool capturePending_ = false;
  QMutex mutex_;
  quint64 generation_ = 0;
  QString lastQueuedCompositionId_;
  qint64 lastQueuedFrame_ = std::numeric_limits<qint64>::min();
};

} // namespace Artifact

namespace Artifact {

void CompositionOnionSkinHistory::reset() {
  QMutexLocker locker(&mutex_);
  ++generation_;
  frames_.clear();
  capturePending_ = false;
  lastQueuedCompositionId_.clear();
  lastQueuedFrame_ = std::numeric_limits<qint64>::min();
}

void CompositionOnionSkinHistory::trim(int frameCount) {
  const int boundedFrameCount = std::clamp(frameCount, 1, 5);
  QMutexLocker locker(&mutex_);
  while (frames_.size() > boundedFrameCount) {
    frames_.removeLast();
  }
}

void CompositionOnionSkinHistory::draw(
    ArtifactIRenderer* renderer, bool enabled, bool hasAnimatedContent,
    const QString& compositionId, qint64 currentFrame, float canvasWidth,
    float canvasHeight, int frameCount, int opacityPercent) {
  if (!renderer || !enabled || canvasWidth <= 0.0f || canvasHeight <= 0.0f) {
    return;
  }

  if (!hasAnimatedContent) {
    reset();
    return;
  }

  QVector<Frame> frames;
  {
    QMutexLocker locker(&mutex_);
    for (const auto& entry : frames_) {
      if (entry.compositionId == compositionId && entry.frame != currentFrame &&
          !entry.image.isNull()) {
        frames.push_back(entry);
      }
    }
  }

  if (frames.isEmpty()) {
    return;
  }

  const int clampedFrameCount = std::clamp(frameCount, 1, 5);
  const int drawCount =
      std::min(clampedFrameCount, static_cast<int>(frames.size()));
  const float baseOpacity = std::clamp(
      opacityPercent / 100.0f, 0.05f, 0.80f);

  for (int i = 0; i < drawCount; ++i) {
    const int index = frames.size() - 1 - i;
    const QImage& frameImage = frames.at(index).image;
    if (frameImage.isNull()) {
      continue;
    }
    const float ageFactor = 1.0f - (static_cast<float>(i) /
                                    std::max(1.0f, static_cast<float>(drawCount)));
    const float opacity = std::clamp(
        baseOpacity * (0.45f + 0.55f * ageFactor), 0.05f, 0.85f);
    renderer->drawSprite(0.0f, 0.0f, canvasWidth, canvasHeight, frameImage,
                         opacity);
  }
}

bool CompositionOnionSkinHistory::queueCapture(
    QObject* owner, ArtifactIRenderer* renderer, bool enabled,
    bool hasAnimatedContent, const QString& compositionId,
    qint64 currentFrame) {
  if (!renderer || !enabled || !owner) {
    return false;
  }

  if (!hasAnimatedContent) {
    reset();
    return false;
  }

  quint64 captureGeneration = 0;
  {
    QMutexLocker locker(&mutex_);
    if (capturePending_ ||
        (lastQueuedCompositionId_ == compositionId &&
         lastQueuedFrame_ == currentFrame)) {
      return false;
    }
    capturePending_ = true;
    lastQueuedCompositionId_ = compositionId;
    lastQueuedFrame_ = currentFrame;
    captureGeneration = generation_;
  }

  QPointer<QObject> ownerGuard(owner);
  const bool accepted = renderer->readbackToImageAsync(
      [this, ownerGuard, compositionId, currentFrame,
       captureGeneration](const QImage& capturedFrame) {
        if (!ownerGuard) {
          return;
        }
        const QImage capturedCopy = capturedFrame.copy();
        QMetaObject::invokeMethod(
            ownerGuard.data(),
            [this, capturedCopy, compositionId, currentFrame,
             captureGeneration]() mutable {
              QMutexLocker locker(&mutex_);
              if (captureGeneration != generation_) {
                return;
              }
              capturePending_ = false;
              if (capturedCopy.isNull()) {
                return;
              }
              frames_.prepend(Frame{capturedCopy, compositionId, currentFrame});
              while (frames_.size() > 5) {
                frames_.removeLast();
              }
            },
            Qt::QueuedConnection);
      });
  if (!accepted) {
    QMutexLocker locker(&mutex_);
    if (captureGeneration == generation_) {
      capturePending_ = false;
    }
  }
  return accepted;
}

} // namespace Artifact
