module;
#include <algorithm>
#include <cmath>
#include <QColor>
#include <QJsonObject>
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
import Artifact.Composition.InOutPoints;
import Artifact.Event.Types;
import Artifact.Layer.Audio;
import Artifact.Service.Project;
import Event.Bus;
import Undo.UndoManager;
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
          static_cast<double>(i) * duration /
          std::max<qsizetype>(1, peaks.size() - 1))));
    }
  }
  return frames;
}

struct BeatAnalysis {
  QVector<qint64> frames;
  double bpm = 0.0;
};

BeatAnalysis analyzeBeats(const QVector<float>& peaks, qint64 begin, qint64 duration,
                          double framesPerSecond) {
  BeatAnalysis result;
  if (peaks.size() < 16 || duration < 2 || framesPerSecond <= 0.0) return result;
  const int sampleCount = static_cast<int>(peaks.size());
  const int minLag = std::max(2, static_cast<int>(std::llround(
      sampleCount * (framesPerSecond * 60.0 / 180.0) / duration)));
  const int maxLag = std::min(sampleCount / 2, static_cast<int>(std::llround(
      sampleCount * (framesPerSecond * 60.0 / 70.0) / duration)));
  if (maxLag <= minLag) return result;
  int bestLag = minLag;
  double bestScore = -1.0;
  for (int lag = minLag; lag <= maxLag; ++lag) {
    double score = 0.0;
    for (int i = lag; i < sampleCount; ++i) {
      score += std::abs(peaks[i]) * std::abs(peaks[i - lag]);
    }
    score /= std::max(1, sampleCount - lag);
    if (score > bestScore) { bestScore = score; bestLag = lag; }
  }
  const double beatFrames = static_cast<double>(bestLag) * duration / sampleCount;
  if (beatFrames < 1.0) return result;
  result.bpm = 60.0 * framesPerSecond / beatFrames;
  int anchorIndex = 0;
  const int anchorLimit = std::min(bestLag, sampleCount);
  for (int i = 1; i < anchorLimit; ++i) {
    if (std::abs(peaks[i]) > std::abs(peaks[anchorIndex])) anchorIndex = i;
  }
  const double anchorFrame = begin + static_cast<double>(anchorIndex) * duration / sampleCount;
  for (double frame = anchorFrame; frame <= begin + duration; frame += beatFrames) {
    result.frames.push_back(static_cast<qint64>(std::llround(frame)));
  }
  for (double frame = anchorFrame - beatFrames; frame >= begin; frame -= beatFrames) {
    result.frames.prepend(static_cast<qint64>(std::llround(frame)));
  }
  return result;
}

QVector<qint64> analyzeSections(const QVector<float>& peaks, qint64 begin, qint64 duration) {
  QVector<qint64> boundaries{begin};
  const int sampleCount = static_cast<int>(peaks.size());
  constexpr int kBins = 12;
  if (sampleCount < kBins * 2 || duration < 3) {
    boundaries.push_back(begin + duration);
    return boundaries;
  }
  double energy[kBins]{};
  for (int bin = 0; bin < kBins; ++bin) {
    const int first = bin * sampleCount / kBins;
    const int last = (bin + 1) * sampleCount / kBins;
    for (int i = first; i < last; ++i) energy[bin] += std::abs(peaks[i]);
    energy[bin] /= std::max(1, last - first);
  }
  QVector<std::pair<double, int>> changes;
  for (int bin = 2; bin + 2 < kBins; ++bin) {
    changes.push_back({std::abs(energy[bin] - energy[bin - 1]), bin});
  }
  std::sort(changes.begin(), changes.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });
  QVector<int> selected;
  for (const auto& change : changes) {
    bool separated = true;
    for (const int existing : selected) separated = separated && std::abs(existing - change.second) >= 2;
    if (separated) selected.push_back(change.second);
    if (selected.size() == 2) break;
  }
  std::sort(selected.begin(), selected.end());
  for (const int bin : selected) {
    boundaries.push_back(begin + static_cast<qint64>(
        std::llround(static_cast<double>(bin) * duration / kBins)));
  }
  boundaries.push_back(begin + duration);
  return boundaries;
}
}

class ArtifactAudioMiniWidget::Impl {
public:
  ArtifactCore::CompositionID compositionId;
  QString sourceName;
  QVector<float> peaks;
  QVector<qint64> transients;
  QVector<qint64> beats;
  QVector<qint64> sections;
  ArtifactCompositionPtr composition;
  double bpm = 0.0;
  qint64 begin = 0;
  qint64 duration = 1;
  qint64 currentFrame = 0;
  ArtifactCore::EventBus::Subscription changedSubscription;
  ArtifactCore::EventBus::Subscription frameSubscription;
};

W_OBJECT_IMPL(ArtifactAudioMiniWidget)

ArtifactAudioMiniWidget::ArtifactAudioMiniWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl()) {
  setMinimumHeight(180);
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
  impl_->peaks.clear();
  impl_->transients.clear();
  impl_->beats.clear();
  impl_->sections.clear();
  impl_->sourceName.clear();
  impl_->composition.reset();
  impl_->bpm = 0.0;
  auto* service = ArtifactProjectService::instance();
  const auto result = service ? service->findComposition(impl_->compositionId) : FindCompositionResult{};
  const auto composition = result.success ? result.ptr.lock() : ArtifactCompositionPtr{};
  if (composition) {
    impl_->composition = composition;
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
      const BeatAnalysis analysis = analyzeBeats(
          impl_->peaks, impl_->begin, impl_->duration,
          std::max<double>(1.0,
                           static_cast<double>(composition->frameRate().framerate())));
      impl_->beats = analysis.frames;
      impl_->bpm = analysis.bpm;
      impl_->sections = analyzeSections(impl_->peaks, impl_->begin, impl_->duration);
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
                       ? QStringLiteral("Audio Mini · %1 · %2 BPM · %3 beats")
                             .arg(impl_->sourceName)
                             .arg(impl_->bpm, 0, 'f', 1)
                             .arg(impl_->beats.size())
                       : QStringLiteral("Audio Mini · select or add an audio layer"));
  if (!impl_ || impl_->peaks.isEmpty()) return;
  const QRect wave(content.left() + 76, content.top() + 24, content.width() - 76,
                   std::max(34, content.height() - 108));
  painter.drawText(QRect(content.left(), wave.top(), 70, wave.height()),
                   Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Waveform"));
  painter.setPen(QColor(97, 205, 146));
  const int center = wave.center().y();
  for (int x = 0; x < wave.width(); ++x) {
    const int index = std::clamp(
        static_cast<int>(static_cast<double>(x) * impl_->peaks.size() /
                         wave.width()),
        0, static_cast<int>(impl_->peaks.size() - 1));
    const int amplitude = static_cast<int>(std::clamp(std::abs(impl_->peaks[index]), 0.0f, 1.0f) * wave.height() * 0.48f);
    painter.drawLine(wave.left() + x, center - amplitude, wave.left() + x, center + amplitude);
  }
  painter.setPen(QColor(241, 191, 82));
  for (const qint64 frame : impl_->transients) {
    const double ratio = static_cast<double>(frame - impl_->begin) / impl_->duration;
    const int x = wave.left() + static_cast<int>(ratio * wave.width());
    painter.drawLine(x, wave.top(), x, wave.top() + 8);
  }
  const int beatY = wave.bottom() + 4;
  painter.setPen(QColor(theme.textColor));
  painter.drawText(QRect(content.left(), beatY, 70, 14),
                   Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Beats"));
  painter.setPen(QColor(57, 196, 229));
  for (int i = 0; i < impl_->beats.size(); ++i) {
    const double ratio = static_cast<double>(impl_->beats[i] - impl_->begin) / impl_->duration;
    const int x = wave.left() + static_cast<int>(ratio * wave.width());
    painter.drawLine(x, beatY, x, beatY + ((i % 4) == 0 ? 11 : 7));
  }
  const int sectionY = beatY + 14;
  painter.setPen(QColor(theme.textColor));
  painter.drawText(QRect(content.left(), sectionY, 70, 16),
                   Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Sections"));
  static const QColor sectionColors[] = {
      QColor(124, 91, 172), QColor(52, 117, 164), QColor(144, 104, 54)};
  for (int i = 0; i + 1 < impl_->sections.size(); ++i) {
    const double startRatio = static_cast<double>(impl_->sections[i] - impl_->begin) / impl_->duration;
    const double endRatio = static_cast<double>(impl_->sections[i + 1] - impl_->begin) / impl_->duration;
    const QRect sectionRect(wave.left() + static_cast<int>(startRatio * wave.width()), sectionY,
                            std::max(1, static_cast<int>((endRatio - startRatio) * wave.width())), 16);
    painter.fillRect(sectionRect.adjusted(1, 1, -1, -1), sectionColors[i % 3]);
    painter.setPen(Qt::white);
    painter.drawText(sectionRect, Qt::AlignCenter,
                     i == 0 ? QStringLiteral("Intro")
                            : (i + 2 == impl_->sections.size() ? QStringLiteral("Outro")
                                                               : QStringLiteral("Section %1").arg(i + 1)));
  }
  const double playheadRatio = std::clamp(static_cast<double>(impl_->currentFrame - impl_->begin) / impl_->duration, 0.0, 1.0);
  painter.setPen(QColor(theme.accentColor));
  const int playheadX = wave.left() + static_cast<int>(playheadRatio * wave.width());
  painter.drawLine(playheadX, wave.top(), playheadX, wave.bottom());
  painter.setPen(QColor(theme.textColor));
  const int actionY = sectionY + 19;
  painter.drawText(QRect(content.left(), actionY, std::max(0, content.width() - 310), 18),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("B / Shift+B: next / previous beat"));
  const QRect transientAction(content.right() - 300, actionY, 142, 18);
  const QRect fourBeatAction(content.right() - 150, actionY, 150, 18);
  painter.setPen(QColor(112, 122, 135));
  painter.drawRect(transientAction);
  painter.drawRect(fourBeatAction);
  painter.setPen(QColor(theme.textColor));
  painter.drawText(transientAction, Qt::AlignCenter, QStringLiteral("Mark Transients"));
  painter.drawText(fourBeatAction, Qt::AlignCenter, QStringLiteral("Mark Every 4 Beats"));
}

void ArtifactAudioMiniWidget::seekBeat(bool next) {
  if (!impl_ || impl_->beats.isEmpty()) return;
  qint64 target = impl_->beats.front();
  if (next) {
    for (const qint64 frame : impl_->beats) if (frame > impl_->currentFrame) { target = frame; break; }
  } else {
    target = impl_->beats.front();
    for (const qint64 frame : impl_->beats) { if (frame >= impl_->currentFrame) break; target = frame; }
  }
  ArtifactCore::globalEventBus().publish<TimelineSeekRequestedEvent>(
      TimelineSeekRequestedEvent{static_cast<double>(target)});
}

bool ArtifactAudioMiniWidget::addGuideMarkers(bool everyFourBeats) {
  if (!impl_ || !impl_->composition) return false;
  auto* points = impl_->composition->inOutPoints();
  if (!points) return false;
  const QVector<qint64>& candidates = everyFourBeats ? impl_->beats : impl_->transients;
  if (candidates.isEmpty()) return false;
  const QJsonObject before = points->toJson();
  int added = 0;
  for (int i = 0; i < candidates.size(); ++i) {
    if (everyFourBeats && (i % 4) != 0) continue;
    const FramePosition position(candidates[i]);
    if (points->getMarkerAt(position)) continue;
    points->addMarker(position,
                      everyFourBeats ? QStringLiteral("Music · 4-beat guide")
                                     : QStringLiteral("Music · transient"),
                      MarkerType::Comment);
    ++added;
  }
  if (added == 0) return false;
  const QJsonObject after = points->toJson();
  points->fromJson(before);
  auto* manager = UndoManager::instance();
  return manager && manager->push(
      std::make_unique<InOutPointsSnapshotCommand>(points, before, after));
}

void ArtifactAudioMiniWidget::mouseReleaseEvent(QMouseEvent* event) {
  if (!impl_ || !event || event->button() != Qt::LeftButton || impl_->peaks.isEmpty()) return;
  const QRect content = rect().adjusted(10, 8, -10, -8);
  const QRect lane(content.left() + 76, content.top() + 24, content.width() - 76,
                   std::max(34, content.height() - 108));
  const int actionY = lane.bottom() + 37;
  const QRect transientAction(content.right() - 300, actionY, 142, 18);
  const QRect fourBeatAction(content.right() - 150, actionY, 150, 18);
  if (transientAction.contains(event->pos()) || fourBeatAction.contains(event->pos())) {
    addGuideMarkers(fourBeatAction.contains(event->pos()));
    event->accept();
    return;
  }
  const QRect wave = lane;
  if (!wave.contains(event->pos())) return;
  const double ratio = std::clamp((event->position().x() - wave.left()) / std::max(1.0, static_cast<double>(wave.width())), 0.0, 1.0);
  const qint64 frame = impl_->begin + static_cast<qint64>(std::llround(ratio * impl_->duration));
  ArtifactCore::globalEventBus().publish<TimelineSeekRequestedEvent>(
      TimelineSeekRequestedEvent{static_cast<double>(frame)});
}

void ArtifactAudioMiniWidget::keyPressEvent(QKeyEvent* event) {
  if (!event || event->key() != Qt::Key_B) { QWidget::keyPressEvent(event); return; }
  seekBeat(!(event->modifiers() & Qt::ShiftModifier));
  event->accept();
}

} // namespace Artifact
