module;

#include <algorithm>
#include <QString>
#include <QVector>

module Artifact.Widgets.AudioMiniWidget;

import Artifact.Composition.Abstract;
import Artifact.Layer.Audio;

namespace Artifact {

void* audioMiniWidgetCompositionAddress(void* compositionHandle);

bool extractAudioMiniWidgetData(void* compositionHandle, QString& sourceName,
                                QVector<float>& peaks, qint64& begin,
                                qint64& duration, double& framesPerSecond) {
  auto* composition = static_cast<ArtifactAbstractComposition*>(
      audioMiniWidgetCompositionAddress(compositionHandle));
  if (!composition) return false;
  framesPerSecond = static_cast<double>(composition->frameRate().framerate());
  for (const auto& layer : composition->allLayer()) {
    const auto audio = ArtifactCore::dynamicPointerCast<ArtifactAudioLayer>(layer);
    if (!audio || !audio->isLoaded()) continue;
    const auto waveform = audio->buildWaveformData(512);
    if (waveform.peaks.isEmpty()) continue;
    sourceName = audio->layerName();
    peaks = waveform.peaks;
    begin = audio->inPoint().framePosition();
    duration = std::max<qint64>(1, audio->outPoint().framePosition() - begin);
    return true;
  }
  return false;
}

} // namespace Artifact
