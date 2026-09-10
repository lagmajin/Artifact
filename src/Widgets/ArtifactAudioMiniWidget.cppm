module;
#include <algorithm>
#include <cmath>
#include <QColor>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QRect>
#include <QString>
#include <QVector>
#include <wobjectimpl.h>

module Artifact.Widgets.AudioMiniWidget;

import Artifact.Audio.Waveform;
import Artifact.Composition.Abstract;
import Artifact.Event.Types;
import Artifact.Layer.Audio;
import Artifact.Service.Project;
import Event.Bus;
import Widgets.Utils.CSS;
import std;

namespace Artifact {

namespace {
QVector<qint64> transientFrames(const QVector<float>& peaks, qint64 begin, qint64 duration) {
  QVector<qint64> frames;
  if (peaks.size() < 3 || duration < 1) return frames;
  double sum = 0.0;
  for (const float peak : peaks) sum += std::abs(peak);
  const double threshold = std::max(0.08, (sum / peaks.size()) * 1.65);
  for (int i = 1; i + 1 < peaks.size(); ++i) {
    const double value = std::abs(peaks[i]);
    if (value >= threshold && value >= std::abs(peaks[i - 1]) && value > std::abs(peaks[i + 1])) {
      frames.push_back(begin + static_cast<qint64>(std::llround(
          static_cast<double>(i) * duration / std::max(1, peaks.size() - 1))));
    }
  }
  return frames;
}
}

class ArtifactAudioMiniWidget::Impl {
public:
  ArtifactCore::CompositionID compositionId;
  QString sourceName;
  QVector<float> peaks;
  QVector<qint64> transients;
  qint64 begin = 0;
  qint64 duration = 1;
  qint64 currentFrame = 0;
  ArtifactCore::EventBus::Subscription changedSubscription;
  ArtifactCore::EventBus::Subscription frameSubscription;
};

W_OBJECT_IMPL(ArtifactAudioMiniWidget)

ArtifactAudioMiniWidget::ArtifactAudioMiniWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl()) {
  setMinimumHeight(118);
  setFocusPolicy(Qt::StrongFocus);
  impl_->changedSubscription = ArtifactCore::globalEventBus().subscribe<CompositionChangedEvent>(
      [this](const CompositionChangedEvent& event) {
        if (impl_ && event.compositionId == impl_->compositionId.toString()) refresh();
      });
  impl_->frameSubscription = ArtifactCore::globalEventBus().subscribe<FrameChangedEvent>(
      [this](const FrameChangedEvent& event) {
        if (!impl_ || event.compositionId != impl_->compositionId.toString()) return;
        impl_->currentFrame = event.frame;
        update();
      });
}

ArtifactAudioMiniWidget::~ArtifactAudioMiniWidget() { delete impl_; }

void ArtifactAudioMiniWidget::setComposition(const ArtifactCore::CompositionID& compositionId) {
  if (!impl_ || impl_->compositionId == compositionId) return;
  impl_->compositionId = compositionId;
  refresh();
}

void ArtifactAudioMiniWidget::refresh() {
  if (!impl_) return;
  impl_->peaks.clear(); impl_->transients.clear(); impl_->sourceName.clear();
  auto* service = ArtifactProjectService::instance();
  const auto result = service ? service->findComposition(impl_->compositionId) : FindCompositionResult{};
  const auto composition = result.success ? result.ptr.lock() : ArtifactCompositionPtr{};
  if (composition) {
    for (const auto& layer : composition->allLayer()) {
      const auto audio = ArtifactCore::dynamicPointerCast<ArtifactAudioLayer>(layer);
      if (!audio || !audio->isLoaded()) continue;
      const auto waveform = audio->buildWaveformData(512);
      if (waveform.peaks.isEmpty()) continue;
      impl_->sourceName = audio->layerName();
      impl_->peaks = waveform.peaks;
      impl_->begin = audio->inPoint().framePosition();
      impl_->duration = std::max<qint64>(1, audio->outPoint().framePosition() - impl_->begin);
      impl_->transients = transientFrames(impl_->peaks, impl_->begin, impl_->duration);
      break;
    }
  }
  update();
}

void ArtifactAudioMiniWidget::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  const auto& theme = ArtifactCore::currentDCCTheme();
  painter.fillRect(rect(), QColor(theme.secondaryBackgroundColor));
  const QRect content = rect().adjusted(10, 8, -10, -8);
  painter.setPen(QColor(theme.textColor));
  painter.drawText(QRect(content.left(), content.top(), content.width(), 18), Qt::AlignLeft | Qt::AlignVCenter,
                   impl_ && !impl_->sourceName.isEmpty()
                       ? QStringLiteral("Audio Mini · %1 · %2 transient cues").arg(impl_->sourceName).arg(impl_->transients.size())
                       : QStringLiteral("Audio Mini · select or add an audio layer"));
  if (!impl_ || impl_->peaks.isEmpty()) return;
  const QRect wave(content.left(), content.top() + 26, content.width(), std::max(38, content.height() - 48));
  painter.setPen(QColor(97, 205, 146));
  const int center = wave.center().y();
  for (int x = 0; x < wave.width(); ++x) {
    const int index = std::clamp(static_cast<int>(static_cast<double>(x) * impl_->peaks.size() / wave.width()), 0, impl_->peaks.size() - 1);
    const int amplitude = static_cast<int>(std::clamp(std::abs(impl_->peaks[index]), 0.0f, 1.0f) * wave.height() * 0.48f);
    painter.drawLine(wave.left() + x, center - amplitude, wave.left() + x, center + amplitude);
  }
  painter.setPen(QColor(241, 191, 82));
  for (const qint64 frame : impl_->transients) {
    const double ratio = static_cast<double>(frame - impl_->begin) / impl_->duration;
    const int x = wave.left() + static_cast<int>(ratio * wave.width());
    painter.drawLine(x, wave.top(), x, wave.top() + 8);
  }
  const double playheadRatio = std::clamp(static_cast<double>(impl_->currentFrame - impl_->begin) / impl_->duration, 0.0, 1.0);
  painter.setPen(QColor(theme.accentColor));
  const int playheadX = wave.left() + static_cast<int>(playheadRatio * wave.width());
  painter.drawLine(playheadX, wave.top(), playheadX, wave.bottom());
  painter.setPen(QColor(theme.textColor));
  painter.drawText(QRect(content.left(), wave.bottom() + 3, content.width(), 16), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("Click: seek  ·  B / Shift+B: next / previous transient  ·  Sections: analysis pending"));
}

void ArtifactAudioMiniWidget::seekBeat(bool next) {
  if (!impl_ || impl_->transients.isEmpty()) return;
  qint64 target = impl_->transients.front();
  if (next) {
    for (const qint64 frame : impl_->transients) if (frame > impl_->currentFrame) { target = frame; break; }
  } else {
    target = impl_->transients.front();
    for (const qint64 frame : impl_->transients) { if (frame >= impl_->currentFrame) break; target = frame; }
  }
  ArtifactCore::globalEventBus().publish<TimelineSeekRequestedEvent>(TimelineSeekRequestedEvent{target});
}

void ArtifactAudioMiniWidget::mouseReleaseEvent(QMouseEvent* event) {
  if (!impl_ || !event || event->button() != Qt::LeftButton || impl_->peaks.isEmpty()) return;
  const QRect wave = rect().adjusted(10, 34, -10, -20);
  if (!wave.contains(event->pos())) return;
  const double ratio = std::clamp((event->position().x() - wave.left()) / std::max(1.0, static_cast<double>(wave.width())), 0.0, 1.0);
  const qint64 frame = impl_->begin + static_cast<qint64>(std::llround(ratio * impl_->duration));
  ArtifactCore::globalEventBus().publish<TimelineSeekRequestedEvent>(TimelineSeekRequestedEvent{frame});
}

void ArtifactAudioMiniWidget::keyPressEvent(QKeyEvent* event) {
  if (!event || event->key() != Qt::Key_B) { QWidget::keyPressEvent(event); return; }
  seekBeat(!(event->modifiers() & Qt::ShiftModifier));
  event->accept();
}

} // namespace Artifact
