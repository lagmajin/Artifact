module;
#include <QMutexLocker>

module Artifact.Render.CompositionChangeDetector;

namespace Artifact {

void CompositionChangeDetector::markLayerChanged(const QString& layerId) {
  QMutexLocker locker(&mutex_);
  changedLayers_.insert(layerId);
}

void CompositionChangeDetector::markCompositionChanged() {
  QMutexLocker locker(&mutex_);
  compositionSettingsChanged_ = true;
}

bool CompositionChangeDetector::needsFullRedraw() const {
  QMutexLocker locker(&mutex_);
  return compositionSettingsChanged_ || changedLayers_.size() > 2;
}

QSet<QString> CompositionChangeDetector::getChangedLayers() const {
  QMutexLocker locker(&mutex_);
  return changedLayers_;
}

void CompositionChangeDetector::reset() {
  QMutexLocker locker(&mutex_);
  changedLayers_.clear();
  compositionSettingsChanged_ = false;
}

QString CompositionChangeDetector::debugInfo() const {
  QMutexLocker locker(&mutex_);
  return QString("ChangedLayers: %1, CompositionChanged: %2")
      .arg(changedLayers_.size())
      .arg(compositionSettingsChanged_);
}

}
