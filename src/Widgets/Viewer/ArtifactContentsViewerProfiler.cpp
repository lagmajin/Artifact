module;
#include <QWidget>

module Artifact.Contents.Viewer;
import Artifact.Widgets.PerformanceProfilerWidget;

namespace Artifact {

QWidget* createAudioPlaybackProfilerWidget(QWidget* parent) {
  auto* widget = new ArtifactPerformanceProfilerWidget(parent);
  widget->setTitleText(QStringLiteral("Audio Playback Profiler"));
  widget->setNamePrefixFilter(QStringLiteral("Audio/"));
  widget->setMinimumHeight(240);
  return widget;
}

} // namespace Artifact
