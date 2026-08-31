module;
#include <QMutex>
#include <QMutexLocker>
#include <QSet>
#include <QString>

export module Artifact.Render.CompositionChangeDetector;

export namespace Artifact {

class CompositionChangeDetector {
  QSet<QString> changedLayers_;
  bool compositionSettingsChanged_ = false;
  mutable QMutex mutex_;

public:
  void markLayerChanged(const QString& layerId);
  void markCompositionChanged();
  bool needsFullRedraw() const;
  QSet<QString> getChangedLayers() const;
  void reset();
  QString debugInfo() const;
};

}
