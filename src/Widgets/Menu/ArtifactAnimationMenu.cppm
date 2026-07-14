module;
#include <cmath>
#include <numeric>
#include <vector>
#include <utility>
#include <QIcon>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QHash>
#include <QKeySequence>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QCursor>
#include <QMetaObject>
#include <QPoint>
#include <QSignalBlocker>
#include <QThread>
#include <QStringList>
#include <QVector>
#include <wobjectimpl.h>

module Menu.Animation;
import std;

import Event.Bus;
import Application.AppSettings;
import Artifact.Event.Types;
import Artifact.Service.Project;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Audio.Analyze;
import Audio.Segment;
import Frame.Position;
import Frame.Range;
import Time.Rational;
import Property.Abstract;
import Artifact.Widgets.ArtifactPropertyWidget;
import Artifact.Widgets.ExpressionCopilotWidget;
import Artifact.Widgets.Timeline;
import Artifact.Widgets.Timeline.EasingLab;
import Utils.Id;
import Utils.Path;
import Math.Interpolate;
import UI.ShortcutBindings;
import Undo.UndoManager;

W_OBJECT_IMPL(Artifact::ArtifactAnimationMenu)

namespace Artifact {

namespace {
class ChangeAudioReactiveBindingsCommand final : public UndoCommand {
public:
 ChangeAudioReactiveBindingsCommand(
     ArtifactCompositionWeakPtr composition,
     QVector<CompositionAudioReactiveBinding> before,
     QVector<CompositionAudioReactiveBinding> after,
     QString label)
  : composition_(std::move(composition)), before_(std::move(before)),
    after_(std::move(after)), label_(std::move(label)) {}

 void undo() override { apply(before_); }
 void redo() override { apply(after_); }
 QString label() const override { return label_; }

private:
 void apply(const QVector<CompositionAudioReactiveBinding>& bindings) {
  if (const auto composition = composition_.lock()) {
   composition->setAudioReactiveBindings(bindings);
   if (auto* manager = UndoManager::instance()) {
    manager->notifyAnythingChanged();
   }
  }
 }
 ArtifactCompositionWeakPtr composition_;
 QVector<CompositionAudioReactiveBinding> before_;
 QVector<CompositionAudioReactiveBinding> after_;
 QString label_;
};

class BakeAudioReactiveBindingCommand final : public UndoCommand {
public:
 BakeAudioReactiveBindingCommand(
     ArtifactAbstractLayerWeak layer, QString propertyPath,
     std::vector<ArtifactCore::KeyFrame> before,
     std::vector<ArtifactCore::KeyFrame> after,
     QVariant beforeValue, QVariant afterValue)
  : layer_(std::move(layer)), propertyPath_(std::move(propertyPath)),
    before_(std::move(before)), after_(std::move(after)),
    beforeValue_(std::move(beforeValue)), afterValue_(std::move(afterValue)) {}

 void undo() override { apply(before_, beforeValue_); }
 void redo() override { apply(after_, afterValue_); }
 QString label() const override {
  return QStringLiteral("Bake Audio Reactive Binding");
 }

private:
 void apply(const std::vector<ArtifactCore::KeyFrame>& keyframes,
            const QVariant& value) {
  if (const auto layer = layer_.lock()) {
   const auto property = layer->getProperty(propertyPath_);
   if (!property) {
    return;
   }
   property->clearKeyFrames();
   for (const auto& keyframe : keyframes) {
    property->addKeyFrame(
        keyframe.time, keyframe.value, keyframe.interpolation,
        keyframe.cp1_x, keyframe.cp1_y, keyframe.cp2_x, keyframe.cp2_y,
        keyframe.roving);
    property->setKeyFrameAnchorAt(keyframe.time, keyframe.anchor);
    property->setKeyFrameColorLabelAt(keyframe.time, keyframe.colorLabel);
   }
   property->setValue(value);
   layer->changed();
   if (auto* manager = UndoManager::instance()) {
    manager->notifyAnythingChanged();
   }
  }
 }
 ArtifactAbstractLayerWeak layer_;
 QString propertyPath_;
 std::vector<ArtifactCore::KeyFrame> before_;
 std::vector<ArtifactCore::KeyFrame> after_;
 QVariant beforeValue_;
 QVariant afterValue_;
};

class CommitAudioReactiveRecordingCommand final : public UndoCommand {
public:
 CommitAudioReactiveRecordingCommand(
     ArtifactCompositionWeakPtr composition,
     QVector<LiveControlRecordingPropertyChange> changes)
  : composition_(std::move(composition)), changes_(std::move(changes)) {}

 void undo() override { apply(true); }
 void redo() override { apply(false); }
 QString label() const override {
  return QStringLiteral("Commit Audio Reactive Recording");
 }

private:
 void apply(const bool before) {
  const auto composition = composition_.lock();
  if (!composition) {
   return;
  }
  for (const auto& change : changes_) {
   const auto layer = composition->layerById(change.layerId);
   const auto property = layer ? layer->getProperty(change.propertyPath) : nullptr;
   if (!property) {
    continue;
   }
   const auto& keyframes = before ? change.beforeKeyframes
                                  : change.afterKeyframes;
   property->clearKeyFrames();
   for (const auto& keyframe : keyframes) {
    property->addKeyFrame(
        keyframe.time, keyframe.value, keyframe.interpolation,
        keyframe.cp1_x, keyframe.cp1_y, keyframe.cp2_x, keyframe.cp2_y,
        keyframe.roving);
    property->setKeyFrameAnchorAt(keyframe.time, keyframe.anchor);
    property->setKeyFrameColorLabelAt(keyframe.time, keyframe.colorLabel);
   }
   property->setValue(before ? change.beforeValue : change.afterValue);
   layer->changed();
  }
  if (auto* manager = UndoManager::instance()) {
   manager->notifyAnythingChanged();
  }
 }
 ArtifactCompositionWeakPtr composition_;
 QVector<LiveControlRecordingPropertyChange> changes_;
};

std::vector<std::size_t> reducedLinearSampleIndexes(
    const std::vector<std::pair<qint64, double>>& samples,
    const double tolerance)
{
 if (samples.size() <= 2 || tolerance <= 0.0) {
  std::vector<std::size_t> all(samples.size());
  std::iota(all.begin(), all.end(), std::size_t{0});
  return all;
 }
 std::vector<bool> keep(samples.size(), false);
 keep.front() = true;
 keep.back() = true;
 const auto reduceRange = [&](const auto& self, const std::size_t begin,
                              const std::size_t end) -> void {
  if (end <= begin + 1) {
   return;
  }
  const double startFrame = static_cast<double>(samples[begin].first);
  const double endFrame = static_cast<double>(samples[end].first);
  const double frameSpan = endFrame - startFrame;
  double maxError = -1.0;
  std::size_t maxIndex = begin;
  for (std::size_t index = begin + 1; index < end; ++index) {
   const double alpha = frameSpan == 0.0
       ? 0.0
       : (static_cast<double>(samples[index].first) - startFrame) / frameSpan;
   const double interpolated = samples[begin].second +
       (samples[end].second - samples[begin].second) * alpha;
   const double error = std::abs(samples[index].second - interpolated);
   if (error > maxError) {
    maxError = error;
    maxIndex = index;
   }
  }
  if (maxError > tolerance) {
   keep[maxIndex] = true;
   self(self, begin, maxIndex);
   self(self, maxIndex, end);
  }
 };
 reduceRange(reduceRange, 0, samples.size() - 1);
 std::vector<std::size_t> result;
 for (std::size_t index = 0; index < keep.size(); ++index) {
  if (keep[index]) {
   result.push_back(index);
  }
 }
 return result;
}

ArtifactPropertyWidget* activePropertyWidget(QWidget* root)
{
 if (!root) {
  return nullptr;
 }
 const auto widgets = root->findChildren<ArtifactPropertyWidget*>();
 for (auto* widget : widgets) {
  if (widget && widget->isVisible() && widget->hasActiveExpressionTarget()) {
   return widget;
  }
 }
 return nullptr;
}

bool configureActiveAudioReactiveBinding(QWidget* root)
{
 auto* propertyWidget = activePropertyWidget(root);
 auto* service = ArtifactProjectService::instance();
 const auto composition = service ? service->currentComposition().lock()
                                  : ArtifactCompositionPtr{};
 const auto layer = propertyWidget ? propertyWidget->activePropertyLayer()
                                   : ArtifactAbstractLayerPtr{};
 const QString propertyPath = propertyWidget
  ? propertyWidget->activePropertyPath().trimmed()
  : QString{};
 const auto property = layer && !propertyPath.isEmpty()
  ? layer->getProperty(propertyPath)
  : std::shared_ptr<ArtifactCore::AbstractProperty>{};
 if (!composition || !layer || !property || !property->isAnimatable() ||
     !property->getValue().canConvert<double>()) {
  QMessageBox::information(
      root, QStringLiteral("Audio Reactive Binding"),
      QStringLiteral("Focus an animatable numeric property first."));
  return false;
 }

 auto before = composition->audioReactiveBindings();
 auto existing = std::find_if(
     before.cbegin(), before.cend(), [&layer, &propertyPath](const auto& binding) {
      return binding.layerId == layer->id() &&
             binding.propertyPath == propertyPath;
     });
 CompositionAudioReactiveBinding binding =
     existing == before.cend() ? CompositionAudioReactiveBinding{} : *existing;
 const QStringList sources{QStringLiteral("amplitude"), QStringLiteral("peak"),
                           QStringLiteral("low"), QStringLiteral("mid"),
                           QStringLiteral("high")};
 bool accepted = false;
  const int currentSource = std::max<int>(0, static_cast<int>(sources.indexOf(binding.source)));
 binding.source = QInputDialog::getItem(
     root, QStringLiteral("Audio Reactive Binding"),
     QStringLiteral("Audio source"), sources, currentSource, false,
     &accepted).trimmed().toLower();
 if (!accepted) return false;
 binding.gain = QInputDialog::getDouble(
     root, QStringLiteral("Audio Reactive Binding"), QStringLiteral("Gain"),
     binding.gain, -100000.0, 100000.0, 4, &accepted);
 if (!accepted) return false;
 binding.offset = QInputDialog::getDouble(
     root, QStringLiteral("Audio Reactive Binding"), QStringLiteral("Offset"),
     binding.offset, -100000.0, 100000.0, 4, &accepted);
 if (!accepted) return false;
 binding.smoothing = QInputDialog::getDouble(
     root, QStringLiteral("Audio Reactive Binding"),
     QStringLiteral("Smoothing (0..1)"), binding.smoothing, 0.0, 1.0, 3,
     &accepted);
 if (!accepted) return false;
 binding.attackSeconds = QInputDialog::getDouble(
     root, QStringLiteral("Audio Reactive Binding"),
     QStringLiteral("Attack (seconds)"), binding.attackSeconds, 0.0,
     60.0, 4, &accepted);
 if (!accepted) return false;
 binding.releaseSeconds = QInputDialog::getDouble(
     root, QStringLiteral("Audio Reactive Binding"),
     QStringLiteral("Release (seconds)"), binding.releaseSeconds, 0.0,
     60.0, 4, &accepted);
 if (!accepted) return false;
 const QString clampChoice = QInputDialog::getItem(
     root, QStringLiteral("Audio Reactive Binding"), QStringLiteral("Clamp"),
     {QStringLiteral("Off"), QStringLiteral("On")},
     binding.clampEnabled ? 1 : 0, false, &accepted);
 if (!accepted) return false;
 binding.clampEnabled = clampChoice == QStringLiteral("On");
 if (binding.clampEnabled) {
  binding.clampMinimum = QInputDialog::getDouble(
      root, QStringLiteral("Audio Reactive Binding"),
      QStringLiteral("Clamp minimum"), binding.clampMinimum, -100000.0,
      100000.0, 4, &accepted);
  if (!accepted) return false;
  binding.clampMaximum = QInputDialog::getDouble(
      root, QStringLiteral("Audio Reactive Binding"),
      QStringLiteral("Clamp maximum"), binding.clampMaximum, -100000.0,
      100000.0, 4, &accepted);
  if (!accepted) return false;
 }
 const QString invertChoice = QInputDialog::getItem(
     root, QStringLiteral("Audio Reactive Binding"), QStringLiteral("Invert"),
     {QStringLiteral("Off"), QStringLiteral("On")}, binding.invert ? 1 : 0,
     false, &accepted);
 if (!accepted) return false;
 binding.invert = invertChoice == QStringLiteral("On");
 binding.layerId = layer->id();
 binding.propertyPath = propertyPath;
 binding.enabled = true;

 auto after = before;
 if (existing == before.cend()) {
  after.append(binding);
 } else {
  after[std::distance(before.cbegin(), existing)] = binding;
 }
 if (auto* manager = UndoManager::instance()) {
  manager->push(std::make_unique<ChangeAudioReactiveBindingsCommand>(
      composition, before, after, QStringLiteral("Configure Audio Reactive Binding")));
 } else {
  composition->setAudioReactiveBindings(after);
 }
 return true;
}

bool removeActiveAudioReactiveBinding(QWidget* root)
{
 auto* propertyWidget = activePropertyWidget(root);
 auto* service = ArtifactProjectService::instance();
 const auto composition = service ? service->currentComposition().lock()
                                  : ArtifactCompositionPtr{};
 const auto layer = propertyWidget ? propertyWidget->activePropertyLayer()
                                   : ArtifactAbstractLayerPtr{};
 const QString propertyPath = propertyWidget
  ? propertyWidget->activePropertyPath().trimmed()
  : QString{};
 if (!composition || !layer || propertyPath.isEmpty()) {
  return false;
 }
 const auto before = composition->audioReactiveBindings();
 auto after = before;
 after.erase(std::remove_if(
     after.begin(), after.end(), [&layer, &propertyPath](const auto& binding) {
      return binding.layerId == layer->id() &&
             binding.propertyPath == propertyPath;
     }), after.end());
 if (after.size() == before.size()) {
  return false;
 }
 if (auto* manager = UndoManager::instance()) {
  manager->push(std::make_unique<ChangeAudioReactiveBindingsCommand>(
      composition, before, after, QStringLiteral("Remove Audio Reactive Binding")));
 } else {
  composition->setAudioReactiveBindings(after);
 }
 return true;
}

bool previewActiveAudioReactiveBinding(QWidget* root)
{
 auto* propertyWidget = activePropertyWidget(root);
 auto* service = ArtifactProjectService::instance();
 const auto composition = service ? service->currentComposition().lock()
                                  : ArtifactCompositionPtr{};
 const auto layer = propertyWidget ? propertyWidget->activePropertyLayer()
                                   : ArtifactAbstractLayerPtr{};
 const QString propertyPath = propertyWidget
  ? propertyWidget->activePropertyPath().trimmed()
  : QString{};
 if (!composition || !layer || propertyPath.isEmpty()) {
  return false;
 }
 const auto bindings = composition->audioReactiveBindings();
 const auto binding = std::find_if(
     bindings.cbegin(), bindings.cend(), [&layer, &propertyPath](const auto& item) {
      return item.enabled && item.layerId == layer->id() &&
             item.propertyPath == propertyPath;
     });
 if (binding == bindings.cend()) {
  QMessageBox::information(
      root, QStringLiteral("Audio Reactive Preview"),
      QStringLiteral("The focused property has no Audio Reactive binding."));
  return false;
 }
 const auto property = layer->getProperty(propertyPath);
 if (!property) {
  return false;
 }
 bool accepted = false;
 const double rawValue = QInputDialog::getDouble(
     root, QStringLiteral("Audio Reactive Preview"),
     QStringLiteral("Raw %1 value").arg(binding->source), 0.5, 0.0,
     100000.0, 4, &accepted);
 if (!accepted) {
  return false;
 }
 const QVariant previousValue = property->getValue();
 if (!composition->applyAudioReactiveBindingValue(
         binding->bindingId, rawValue, true)) {
  return false;
 }
 const auto monitor =
     composition->audioReactiveBindingMonitor(binding->bindingId);
 QMessageBox::information(
     root, QStringLiteral("Audio Reactive Preview"),
     QStringLiteral("Source: %1\nRaw: %2\nProcessed: %3\nProperty: %4")
         .arg(binding->source,
              QString::number(monitor.rawValue, 'f', 4),
              QString::number(monitor.processedValue, 'f', 4),
              propertyPath));
 layer->setLayerPropertyValue(propertyPath, previousValue);
 return true;
}

bool bakeActiveAudioReactiveBinding(QWidget* root)
{
 auto* propertyWidget = activePropertyWidget(root);
 auto* service = ArtifactProjectService::instance();
 const auto composition = service ? service->currentComposition().lock()
                                  : ArtifactCompositionPtr{};
 const auto layer = propertyWidget ? propertyWidget->activePropertyLayer()
                                   : ArtifactAbstractLayerPtr{};
 const QString propertyPath = propertyWidget
  ? propertyWidget->activePropertyPath().trimmed()
  : QString{};
 const auto property = layer && !propertyPath.isEmpty()
  ? layer->getProperty(propertyPath)
  : std::shared_ptr<ArtifactCore::AbstractProperty>{};
 if (!composition || !layer || !property || !property->isAnimatable()) {
  return false;
 }
 const auto bindings = composition->audioReactiveBindings();
 const auto binding = std::find_if(
     bindings.cbegin(), bindings.cend(), [&layer, &propertyPath](const auto& item) {
      return item.enabled && item.layerId == layer->id() &&
             item.propertyPath == propertyPath;
     });
 if (binding == bindings.cend()) {
  QMessageBox::information(
      root, QStringLiteral("Audio Reactive Bake"),
      QStringLiteral("The focused property has no Audio Reactive binding."));
  return false;
 }

 bool accepted = false;
 const QString rangeMode = QInputDialog::getItem(
     root, QStringLiteral("Audio Reactive Bake"), QStringLiteral("Bake range"),
     {QStringLiteral("Work Area"), QStringLiteral("Layer Range"),
      QStringLiteral("Custom")}, 0, false, &accepted);
 if (!accepted) return false;
 FrameRange requestedRange = rangeMode == QStringLiteral("Layer Range")
     ? FrameRange(layer->inPoint(), layer->outPoint())
     : composition->workAreaRange();
 if (rangeMode == QStringLiteral("Custom")) {
  const int start = QInputDialog::getInt(
      root, QStringLiteral("Audio Reactive Bake"), QStringLiteral("Start frame"),
      static_cast<int>(requestedRange.start()), -1000000, 1000000, 1,
      &accepted);
  if (!accepted) return false;
  const int end = QInputDialog::getInt(
      root, QStringLiteral("Audio Reactive Bake"), QStringLiteral("End frame"),
      static_cast<int>(requestedRange.end()), -1000000, 1000000, 1,
      &accepted);
  if (!accepted) return false;
  requestedRange = FrameRange(FramePosition(start), FramePosition(end));
 }
 requestedRange = requestedRange.normalized();
 qint64 startFrame = std::max<qint64>(
     requestedRange.start(), layer->inPoint().framePosition());
 qint64 endFrame = std::min<qint64>(
     requestedRange.end(), layer->outPoint().framePosition());
 if (endFrame < startFrame) {
  QMessageBox::warning(
      root, QStringLiteral("Audio Reactive Bake"),
      QStringLiteral("The requested range does not overlap the layer."));
  return false;
 }
 const double tolerance = QInputDialog::getDouble(
     root, QStringLiteral("Audio Reactive Bake"),
     QStringLiteral("Keyframe reduction tolerance"), 0.001, 0.0,
     100000.0, 6, &accepted);
 if (!accepted) return false;

 constexpr int sampleRate = 48000;
 const double fps = std::max(
     1.0, static_cast<double>(composition->frameRate().framerate()));
 const int frameScale = std::max(1, static_cast<int>(std::lround(fps)));
 const int samplesPerFrame =
     std::max(1, static_cast<int>(std::lround(sampleRate / fps)));
 ArtifactCore::AudioAnalyzer analyzer(1024);
 std::vector<std::pair<qint64, double>> samples;
 samples.reserve(static_cast<std::size_t>(endFrame - startFrame + 1));
 for (qint64 frame = startFrame; frame <= endFrame; ++frame) {
  ArtifactCore::AudioSegment segment;
  if (!composition->getAudio(
          segment, FramePosition(frame), samplesPerFrame, sampleRate)) {
   segment.sampleRate = sampleRate;
   segment.channelData.resize(1);
   segment.setFrameCount(samplesPerFrame);
   segment.zero();
  }
  const auto analysis = analyzer.analyze(segment);
  double rawValue = analysis.rms;
  if (binding->source == QStringLiteral("peak")) {
   rawValue = analysis.peak;
  } else if (binding->source == QStringLiteral("low")) {
   rawValue = analysis.lowIntensity;
  } else if (binding->source == QStringLiteral("mid")) {
   rawValue = analysis.midIntensity;
  } else if (binding->source == QStringLiteral("high")) {
   rawValue = analysis.highIntensity;
  }
  const auto monitor = composition->evaluateAudioReactiveBindingValue(
      binding->bindingId, rawValue, frame == startFrame);
  if (!monitor.valid) {
   return false;
  }
  samples.emplace_back(frame, monitor.processedValue);
 }
 if (samples.empty()) {
  return false;
 }
 const auto keptIndexes = reducedLinearSampleIndexes(samples, tolerance);
 std::vector<ArtifactCore::KeyFrame> bakedKeyframes;
 bakedKeyframes.reserve(keptIndexes.size());
 for (const std::size_t index : keptIndexes) {
  ArtifactCore::KeyFrame keyframe;
  keyframe.time = RationalTime(samples[index].first, frameScale);
  keyframe.value = samples[index].second;
  keyframe.interpolation = ArtifactCore::InterpolationType::Linear;
  bakedKeyframes.push_back(std::move(keyframe));
 }
 const auto beforeKeyframes = property->getKeyFrames();
 const QVariant beforeValue = property->getValue();
 const QVariant afterValue = samples.back().second;
 if (auto* manager = UndoManager::instance()) {
  manager->push(std::make_unique<BakeAudioReactiveBindingCommand>(
      layer, propertyPath, beforeKeyframes, bakedKeyframes,
      beforeValue, afterValue));
 } else {
  BakeAudioReactiveBindingCommand command(
      layer, propertyPath, beforeKeyframes, bakedKeyframes,
      beforeValue, afterValue);
  command.redo();
 }
 QMessageBox::information(
     root, QStringLiteral("Audio Reactive Bake"),
     QStringLiteral("Baked %1 frames to %2 keyframes (tolerance %3).")
         .arg(samples.size()).arg(bakedKeyframes.size())
         .arg(QString::number(tolerance, 'g', 6)));
 return true;
}

bool beginActiveAudioReactiveRecording(QWidget* root)
{
 auto* propertyWidget = activePropertyWidget(root);
 auto* service = ArtifactProjectService::instance();
 const auto composition = service ? service->currentComposition().lock()
                                  : ArtifactCompositionPtr{};
 const auto layer = propertyWidget ? propertyWidget->activePropertyLayer()
                                   : ArtifactAbstractLayerPtr{};
 const QString propertyPath = propertyWidget
  ? propertyWidget->activePropertyPath().trimmed()
  : QString{};
 if (!composition || !layer || propertyPath.isEmpty() ||
     composition->isLiveControlRecordingActive()) {
  return false;
 }
 const auto bindings = composition->audioReactiveBindings();
 const auto binding = std::find_if(
     bindings.cbegin(), bindings.cend(), [&layer, &propertyPath](const auto& item) {
      return item.enabled && item.layerId == layer->id() &&
             item.propertyPath == propertyPath;
     });
 if (binding == bindings.cend()) {
  return false;
 }
 bool accepted = false;
 const int sampleStride = QInputDialog::getInt(
     root, QStringLiteral("Audio Reactive Recording"),
     QStringLiteral("Sample every N frames"), 1, 1, 1000, 1, &accepted);
 if (!accepted) return false;
 const double deadZone = QInputDialog::getDouble(
     root, QStringLiteral("Audio Reactive Recording"),
     QStringLiteral("Value dead zone"), 0.001, 0.0, 100000.0, 6,
     &accepted);
 if (!accepted) return false;
 LiveControlRecordingOptions options;
 options.addresses.append(binding->bindingId);
 options.sampleEveryNFrames = sampleStride;
 options.deadZone = deadZone;
 options.restoreOnCancel = true;
 return composition->beginLiveControlRecording(options);
}

bool commitAudioReactiveRecording()
{
 auto* service = ArtifactProjectService::instance();
 const auto composition = service ? service->currentComposition().lock()
                                  : ArtifactCompositionPtr{};
 if (!composition || !composition->isLiveControlRecordingActive()) {
  return false;
 }
 auto changes = composition->commitLiveControlRecording();
 if (!changes.isEmpty()) {
  if (auto* manager = UndoManager::instance()) {
   manager->push(std::make_unique<CommitAudioReactiveRecordingCommand>(
       composition, std::move(changes)));
  }
 }
 return true;
}

bool cancelAudioReactiveRecording()
{
 auto* service = ArtifactProjectService::instance();
 const auto composition = service ? service->currentComposition().lock()
                                  : ArtifactCompositionPtr{};
 if (!composition || !composition->isLiveControlRecordingActive()) {
  return false;
 }
 composition->cancelLiveControlRecording();
 return true;
}

QIcon menuIcon(const QString& path)
{
  return QIcon(resolveIconPath(path));
}

ArtifactTimelineWidget* activeTimelineWidget(QWidget* root)
{
 if (!root) {
  return nullptr;
 }

 const auto widgets = root->findChildren<ArtifactTimelineWidget*>();
 for (auto* widget : widgets) {
  if (widget && widget->hasFocus()) {
   return widget;
  }
 }
 for (auto* widget : widgets) {
  if (widget && widget->isVisible()) {
   return widget;
  }
 }
 return widgets.isEmpty() ? nullptr : widgets.front();
}

bool openActiveExpressionCopilot(QWidget* root)
{
 if (!root) {
  return false;
 }

 const auto propertyWidgets = root->findChildren<ArtifactPropertyWidget*>();
 for (auto* propertyWidget : propertyWidgets) {
  if (propertyWidget && propertyWidget->isVisible() &&
      propertyWidget->hasActiveExpressionTarget() &&
      propertyWidget->openActiveExpressionCopilot()) {
   return true;
  }
 }

 return false;
}

bool openNewExpressionCopilot(QWidget* root)
{
 if (!root) {
  return false;
 }

 auto* copilot = new ArtifactExpressionCopilotWidget(root);
 copilot->setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint | Qt::Tool);
 copilot->setWindowTitle(QStringLiteral("Expression Copilot"));
 copilot->setAttribute(Qt::WA_DeleteOnClose);
 copilot->move(QCursor::pos() - QPoint(150, 200));
 copilot->show();
 return true;
}

bool clearActiveExpression(QWidget* root)
{
 if (!root) {
  return false;
 }

 const auto propertyWidgets = root->findChildren<ArtifactPropertyWidget*>();
 for (auto* propertyWidget : propertyWidgets) {
  if (propertyWidget && propertyWidget->isVisible() &&
      propertyWidget->hasActiveExpressionTarget() &&
      propertyWidget->clearActiveExpression()) {
   return true;
  }
 }
 return false;
}

bool convertActiveExpressionToKeyframes(QWidget* root)
{
 if (!root) {
  return false;
 }

 const auto propertyWidgets = root->findChildren<ArtifactPropertyWidget*>();
 for (auto* propertyWidget : propertyWidgets) {
  if (propertyWidget && propertyWidget->isVisible() &&
      propertyWidget->hasActiveExpressionTarget() &&
      propertyWidget->convertActiveExpressionToKeyframes()) {
   return true;
  }
 }
 return false;
}

bool bakeActivePropertyToKeyframes(QWidget* root)
{
 if (!root) {
  return false;
 }

 const auto propertyWidgets = root->findChildren<ArtifactPropertyWidget*>();
 for (auto* propertyWidget : propertyWidgets) {
  if (propertyWidget && propertyWidget->isVisible() &&
      propertyWidget->hasActiveExpressionTarget() &&
      propertyWidget->bakeActivePropertyToKeyframes()) {
   return true;
  }
 }
 return false;
}

bool saveActiveExpressionPreset(QWidget* root)
{
 if (!root) {
  return false;
 }

 const auto propertyWidgets = root->findChildren<ArtifactPropertyWidget*>();
 for (auto* propertyWidget : propertyWidgets) {
  if (propertyWidget && propertyWidget->isVisible() &&
      propertyWidget->hasActiveExpressionTarget() &&
      propertyWidget->saveActiveExpressionPreset()) {
   return true;
  }
 }
 return false;
}

bool loadActiveExpressionPreset(QWidget* root)
{
 if (!root) {
  return false;
 }

 const auto propertyWidgets = root->findChildren<ArtifactPropertyWidget*>();
 for (auto* propertyWidget : propertyWidgets) {
  if (propertyWidget && propertyWidget->isVisible() &&
      propertyWidget->hasActiveExpressionTarget() &&
      propertyWidget->loadActiveExpressionPreset()) {
   return true;
  }
 }
 return false;
}

bool hasActiveExpressionTarget(QWidget* root)
{
 if (!root) {
  return false;
 }

 const auto propertyWidgets = root->findChildren<ArtifactPropertyWidget*>();
 for (auto* propertyWidget : propertyWidgets) {
  if (propertyWidget && propertyWidget->isVisible() &&
      propertyWidget->hasActiveExpressionTarget()) {
   return true;
  }
 }
 return false;
}
}

 class ArtifactAnimationMenu::Impl {
 public:
  Impl(ArtifactAnimationMenu* menu);
  ~Impl();

  ArtifactAnimationMenu* menu_ = nullptr;
  ArtifactCore::LayerID selectedLayerId_;
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;

  QAction* addKeyframeAction = nullptr;
  QAction* removeKeyframeAction = nullptr;
  QAction* selectAllKeyframesAction = nullptr;
  QAction* reverseSelectedKeyframesAction = nullptr;
  QAction* reverseAllKeyframesInLayerAction = nullptr;
  QAction* reverseAllKeyframesInSelectedLayersAction = nullptr;
  QAction* reverseAllKeyframesInCompositionAction = nullptr;
  QAction* copyKeyframesAction = nullptr;
  QAction* pasteKeyframesAction = nullptr;

  QAction* linearInterpolationAction = nullptr;
  QAction* bezierInterpolationAction = nullptr;
  QAction* holdInterpolationAction = nullptr;
  QAction* easeInAction = nullptr;
  QAction* easeOutAction = nullptr;
  QAction* easeInOutAction = nullptr;

  QAction* showGraphEditorAction = nullptr;
  QAction* toggleValueGraphAction = nullptr;
  QAction* toggleVelocityGraphAction = nullptr;
  QAction* easingLabAction = nullptr;
  QAction* keyPatternAction = nullptr;
  QActionGroup* graphModeGroup = nullptr;

  QAction* goToNextKeyframeAction = nullptr;
  QAction* goToPreviousKeyframeAction = nullptr;
  QAction* goToFirstKeyframeAction = nullptr;
  QAction* goToLastKeyframeAction = nullptr;

  QAction* enableTimeRemapAction = nullptr;
  QAction* freezeFrameAction = nullptr;
  QAction* timeReverseAction = nullptr;

  QAction* addExpressionAction = nullptr;
  QAction* editExpressionAction = nullptr;
  QAction* removeExpressionAction = nullptr;
  QAction* convertToKeyframesAction = nullptr;
  QAction* bakeLiveToKeyframesAction = nullptr;
  QAction* configureAudioReactiveAction = nullptr;
  QAction* removeAudioReactiveAction = nullptr;
  QAction* previewAudioReactiveAction = nullptr;
  QAction* bakeAudioReactiveAction = nullptr;
  QAction* armAudioReactiveAction = nullptr;
  QAction* commitAudioReactiveAction = nullptr;
  QAction* cancelAudioReactiveAction = nullptr;

  QAction* saveAnimationPresetAction = nullptr;
  QAction* loadAnimationPresetAction = nullptr;

  QMenu* interpolationMenu = nullptr;
  QMenu* graphEditorMenu = nullptr;
  QMenu* navigationMenu = nullptr;
  QMenu* timeRemapMenu = nullptr;
  QMenu* expressionMenu = nullptr;
  QMenu* audioReactiveMenu = nullptr;
  QMenu* presetMenu = nullptr;
  QMenu* presetLibraryMenu = nullptr;
  QHash<QAction*, ArtifactCore::KeyframePatternPreset> presetLibraryActions_;

  void refreshEnabledState();
  void requestRefreshEnabledState();
 };

 ArtifactAnimationMenu::Impl::Impl(ArtifactAnimationMenu* menu) : menu_(menu)
 {
  auto& eventBus = ArtifactCore::globalEventBus();
  eventBusSubscriptions_.push_back(
      eventBus.subscribe<LayerSelectionChangedEvent>(
          [this](const LayerSelectionChangedEvent& event) {
            const ArtifactCore::LayerID layerId(event.layerId);
            if (!event.compositionId.isEmpty()) {
              auto* service = ArtifactProjectService::instance();
              if (service) {
                if (const auto comp = service->currentComposition().lock()) {
                  if (comp->id().toString() != event.compositionId) {
                    return;
                  }
                }
              }
            }
            selectedLayerId_ = layerId;
            requestRefreshEnabledState();
          }));
  eventBusSubscriptions_.push_back(
      eventBus.subscribe<LayerChangedEvent>(
          [this](const LayerChangedEvent& event) {
            if (!event.compositionId.isEmpty()) {
              auto* service = ArtifactProjectService::instance();
              if (service) {
                if (const auto comp = service->currentComposition().lock()) {
                  if (comp->id().toString() != event.compositionId) {
                    return;
                  }
                }
              }
            }
            if (event.changeType == LayerChangedEvent::ChangeType::Removed &&
                selectedLayerId_ == ArtifactCore::LayerID(event.layerId)) {
              selectedLayerId_ = {};
            }
            requestRefreshEnabledState();
          }));
  eventBusSubscriptions_.push_back(
      eventBus.subscribe<ProjectChangedEvent>(
          [this](const ProjectChangedEvent&) {
            requestRefreshEnabledState();
          }));
  QObject::connect(menu, &QMenu::aboutToShow, menu, [this]() {
   refreshEnabledState();
  });
 }

 void ArtifactAnimationMenu::Impl::requestRefreshEnabledState()
 {
  if (!menu_) {
   return;
  }
  if (QThread::currentThread() == menu_->thread()) {
   refreshEnabledState();
   return;
  }
  QMetaObject::invokeMethod(menu_, [this]() {
   refreshEnabledState();
  }, Qt::QueuedConnection);
 }

 ArtifactAnimationMenu::Impl::~Impl()
 {
 }

 void ArtifactAnimationMenu::Impl::refreshEnabledState()
 {
  auto* service = ArtifactProjectService::instance();
  bool hasLayer = service && service->hasProject() && static_cast<bool>(service->currentComposition().lock()) && !selectedLayerId_.isNil();

  addKeyframeAction->setEnabled(hasLayer);
  removeKeyframeAction->setEnabled(hasLayer);
  selectAllKeyframesAction->setEnabled(hasLayer);
  copyKeyframesAction->setEnabled(hasLayer);
  pasteKeyframesAction->setEnabled(hasLayer);

 interpolationMenu->setEnabled(hasLayer);
 graphEditorMenu->setEnabled(hasLayer);
 navigationMenu->setEnabled(hasLayer);
 timeRemapMenu->setEnabled(hasLayer);
 expressionMenu->setEnabled(hasLayer);
 presetMenu->setEnabled(hasLayer);
  const bool hasExpressionTarget = hasActiveExpressionTarget(menu_ ? menu_->window() : nullptr);
  if (addExpressionAction) {
   addExpressionAction->setEnabled(hasLayer);
  }
  if (editExpressionAction) {
   editExpressionAction->setEnabled(hasLayer && hasExpressionTarget);
  }
  if (removeExpressionAction) {
   removeExpressionAction->setEnabled(hasLayer && hasExpressionTarget);
  }
  if (convertToKeyframesAction) {
   convertToKeyframesAction->setEnabled(hasLayer && hasExpressionTarget);
  }
  if (bakeLiveToKeyframesAction) {
   bakeLiveToKeyframesAction->setEnabled(hasLayer && hasExpressionTarget);
  }
  if (saveAnimationPresetAction) {
   saveAnimationPresetAction->setEnabled(hasLayer && hasExpressionTarget);
  }
  if (loadAnimationPresetAction) {
   loadAnimationPresetAction->setEnabled(hasLayer && hasExpressionTarget);
  }
  if (keyPatternAction) {
    keyPatternAction->setEnabled(hasLayer);
  }
  if (audioReactiveMenu) {
   audioReactiveMenu->setEnabled(hasLayer);
  }
  const auto currentComposition = service
      ? service->currentComposition().lock()
      : ArtifactCompositionPtr{};
  const bool recordingActive = currentComposition &&
      currentComposition->isLiveControlRecordingActive();
  if (configureAudioReactiveAction) {
   configureAudioReactiveAction->setEnabled(hasLayer && hasExpressionTarget);
  }
  if (removeAudioReactiveAction) {
   removeAudioReactiveAction->setEnabled(hasLayer && hasExpressionTarget);
  }
  if (previewAudioReactiveAction) {
   previewAudioReactiveAction->setEnabled(hasLayer && hasExpressionTarget);
  }
  if (bakeAudioReactiveAction) {
   bakeAudioReactiveAction->setEnabled(hasLayer && hasExpressionTarget);
  }
  if (armAudioReactiveAction) {
   armAudioReactiveAction->setEnabled(
       hasLayer && hasExpressionTarget && !recordingActive);
  }
  if (commitAudioReactiveAction) {
   commitAudioReactiveAction->setEnabled(recordingActive);
  }
  if (cancelAudioReactiveAction) {
   cancelAudioReactiveAction->setEnabled(recordingActive);
  }
  if (presetLibraryMenu) {
   presetLibraryMenu->setEnabled(hasLayer);
  }
}

 ArtifactAnimationMenu::ArtifactAnimationMenu(QWidget* parent)
  : QMenu(parent), impl_(new Impl(this))
 {
  setTitle("アニメーション(&A)");
  setIcon(menuIcon(QStringLiteral("Studio/menubar_animation.svg")));
  setTearOffEnabled(false);

  impl_->addKeyframeAction = addAction("キーフレームを追加");
  impl_->addKeyframeAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_add_circle.svg")));
  impl_->addKeyframeAction->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::AnimationAddKeyframe));
  impl_->removeKeyframeAction = addAction("キーフレームを削除");
  impl_->removeKeyframeAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_remove_circle.svg")));
  impl_->removeKeyframeAction->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::AnimationRemoveKeyframe));

  impl_->selectAllKeyframesAction = addAction("すべてのキーフレームを選択");
  impl_->selectAllKeyframesAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_select_all.svg")));
  impl_->selectAllKeyframesAction->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::AnimationSelectAllKeyframes));

  auto* reverseMenu = addMenu("キーフレーム反転(&R)");
  reverseMenu->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_swap_horiz.svg")));
  impl_->reverseSelectedKeyframesAction = reverseMenu->addAction("選択キーフレームを反転");
  impl_->reverseSelectedKeyframesAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_swap_horiz.svg")));
  impl_->reverseAllKeyframesInLayerAction = reverseMenu->addAction("現在のレイヤーのキーフレームをすべて反転");
  impl_->reverseAllKeyframesInLayerAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_swap_horiz.svg")));
  impl_->reverseAllKeyframesInSelectedLayersAction = reverseMenu->addAction("選択レイヤーのキーフレームをすべて反転");
  impl_->reverseAllKeyframesInSelectedLayersAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_swap_horiz.svg")));
  impl_->reverseAllKeyframesInCompositionAction = reverseMenu->addAction("コンポ全体のキーフレームをすべて反転");
  impl_->reverseAllKeyframesInCompositionAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_swap_horiz.svg")));

  impl_->copyKeyframesAction = addAction("キーフレームをコピー");
  impl_->copyKeyframesAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_content_copy.svg")));
  impl_->copyKeyframesAction->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::AnimationCopyKeyframes));

  impl_->pasteKeyframesAction = addAction("キーフレームをペースト");
  impl_->pasteKeyframesAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_content_paste.svg")));
  impl_->pasteKeyframesAction->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::AnimationPasteKeyframes));

  addSeparator();

  impl_->interpolationMenu = addMenu("キーフレーム補間(&I)");
  impl_->interpolationMenu->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_timeline.svg")));

  impl_->linearInterpolationAction = impl_->interpolationMenu->addAction("リニア");
  impl_->linearInterpolationAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_straighten.svg")));
  impl_->linearInterpolationAction->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::AnimationLinearInterpolation));
  impl_->easeInAction = impl_->interpolationMenu->addAction("イージーイーズイン");
  impl_->easeInAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_trending_up.svg")));
  impl_->easeInAction->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::AnimationEaseIn));
  impl_->easeOutAction = impl_->interpolationMenu->addAction("イージーイーズアウト");
  impl_->easeOutAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_trending_down.svg")));
  impl_->easeOutAction->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::AnimationEaseOut));
  impl_->easeInOutAction = impl_->interpolationMenu->addAction("イージーイーズ");
  impl_->easeInOutAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_show_chart.svg")));
  impl_->easeInOutAction->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::AnimationEaseInOut));
  impl_->holdInterpolationAction = impl_->interpolationMenu->addAction("停止");
  impl_->holdInterpolationAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_pause.svg")));
  impl_->holdInterpolationAction->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::AnimationHoldInterpolation));
  impl_->bezierInterpolationAction = impl_->interpolationMenu->addAction("ベジェ");
  impl_->bezierInterpolationAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_edit.svg")));
  impl_->bezierInterpolationAction->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::AnimationBezierInterpolation));

  impl_->linearInterpolationAction->setCheckable(true);
  impl_->bezierInterpolationAction->setCheckable(true);
  impl_->holdInterpolationAction->setCheckable(true);

  addSeparator();

  impl_->graphEditorMenu = addMenu("カーブエディタ(&G)");
  impl_->graphEditorMenu->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_show_chart.svg")));

  impl_->showGraphEditorAction = impl_->graphEditorMenu->addAction("カーブエディタを表示");
  impl_->showGraphEditorAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_query_stats.svg")));
  impl_->showGraphEditorAction->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::AnimationShowGraphEditor));
  impl_->graphEditorMenu->addSeparator();
  impl_->graphModeGroup = new QActionGroup(impl_->menu_);
  impl_->graphModeGroup->setExclusive(true);
  impl_->toggleValueGraphAction = impl_->graphEditorMenu->addAction("値グラフを表示");
  impl_->toggleValueGraphAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_show_chart.svg")));
  impl_->toggleValueGraphAction->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::AnimationToggleValueGraph));
  impl_->toggleValueGraphAction->setCheckable(true);
  impl_->graphModeGroup->addAction(impl_->toggleValueGraphAction);
  impl_->toggleVelocityGraphAction = impl_->graphEditorMenu->addAction("速度グラフを表示");
  impl_->toggleVelocityGraphAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_speed.svg")));
  impl_->toggleVelocityGraphAction->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::AnimationToggleVelocityGraph));
  impl_->toggleVelocityGraphAction->setCheckable(true);
  impl_->graphModeGroup->addAction(impl_->toggleVelocityGraphAction);
  impl_->easingLabAction = impl_->graphEditorMenu->addAction("EasingLab を開く");
  impl_->easingLabAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_tune.svg")));
  impl_->keyPatternAction = impl_->graphEditorMenu->addAction("Key Pattern Dialog を開く");
  impl_->keyPatternAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_animation.svg")));

  impl_->navigationMenu = addMenu("ナビゲーション(&N)");
  impl_->navigationMenu->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_skip_next.svg")));

  impl_->goToNextKeyframeAction = impl_->navigationMenu->addAction("次のキーフレームに移動");
  impl_->goToNextKeyframeAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_skip_next.svg")));
  impl_->goToNextKeyframeAction->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::AnimationGoToNextKeyframe));
  impl_->goToPreviousKeyframeAction = impl_->navigationMenu->addAction("前のキーフレームに移動");
  impl_->goToPreviousKeyframeAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_skip_previous.svg")));
  impl_->goToPreviousKeyframeAction->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::AnimationGoToPreviousKeyframe));

  impl_->navigationMenu->addSeparator();

  impl_->goToFirstKeyframeAction = impl_->navigationMenu->addAction("最初のキーフレームに移動");
  impl_->goToFirstKeyframeAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_fast_rewind.svg")));
  impl_->goToFirstKeyframeAction->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::AnimationGoToFirstKeyframe));
  impl_->goToLastKeyframeAction = impl_->navigationMenu->addAction("最後のキーフレームに移動");
  impl_->goToLastKeyframeAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_fast_forward.svg")));
  impl_->goToLastKeyframeAction->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::AnimationGoToLastKeyframe));

  addSeparator();

  impl_->timeRemapMenu = addMenu("タイムリマップ(&T)");
  impl_->timeRemapMenu->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_schedule.svg")));

  impl_->enableTimeRemapAction = impl_->timeRemapMenu->addAction("タイムリマップ可能にする");
  impl_->enableTimeRemapAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_schedule.svg")));
  impl_->enableTimeRemapAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_T));
  impl_->freezeFrameAction = impl_->timeRemapMenu->addAction("フレームをフリーズ");
  impl_->freezeFrameAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_pause_circle.svg")));
  impl_->freezeFrameAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_J));
  impl_->timeReverseAction = impl_->timeRemapMenu->addAction("時間反転レイヤー");
  impl_->timeReverseAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_swap_horiz.svg")));

  impl_->expressionMenu = addMenu("エクスプレッション(&E)");
  impl_->expressionMenu->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_functions.svg")));

  impl_->addExpressionAction = impl_->expressionMenu->addAction("エクスプレッションを追加...");
  impl_->addExpressionAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_add.svg")));
  impl_->addExpressionAction->setShortcut(QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_Equal));
  impl_->editExpressionAction = impl_->expressionMenu->addAction("エクスプレッションを編集...");
  impl_->editExpressionAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_edit.svg")));
  impl_->editExpressionAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Equal));
  impl_->removeExpressionAction = impl_->expressionMenu->addAction("エクスプレッションを削除");
  impl_->removeExpressionAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_delete.svg")));

  impl_->expressionMenu->addSeparator();

  impl_->convertToKeyframesAction = impl_->expressionMenu->addAction("エクスプレッションをキーフレームに変換...");
  impl_->convertToKeyframesAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_animation.svg")));
  impl_->bakeLiveToKeyframesAction = impl_->expressionMenu->addAction("現在PropertyをキーフレームにBake...");
  impl_->bakeLiveToKeyframesAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_animation.svg")));
  impl_->audioReactiveMenu = impl_->expressionMenu->addMenu(
      QStringLiteral("Audio Reactive"));
  impl_->audioReactiveMenu->setIcon(
      menuIcon(QStringLiteral("Studio/animationmenu_tune.svg")));
  impl_->configureAudioReactiveAction =
      impl_->audioReactiveMenu->addAction(
          QStringLiteral("Configure focused property binding..."));
  impl_->removeAudioReactiveAction =
      impl_->audioReactiveMenu->addAction(
          QStringLiteral("Remove focused property binding"));
  impl_->previewAudioReactiveAction =
      impl_->audioReactiveMenu->addAction(
          QStringLiteral("Preview focused property binding..."));
  impl_->bakeAudioReactiveAction =
      impl_->audioReactiveMenu->addAction(
          QStringLiteral("Bake focused property binding..."));
  impl_->audioReactiveMenu->addSeparator();
  impl_->armAudioReactiveAction =
      impl_->audioReactiveMenu->addAction(
          QStringLiteral("Arm focused binding recording..."));
  impl_->commitAudioReactiveAction =
      impl_->audioReactiveMenu->addAction(
          QStringLiteral("Commit Audio Reactive recording"));
  impl_->cancelAudioReactiveAction =
      impl_->audioReactiveMenu->addAction(
          QStringLiteral("Cancel Audio Reactive recording"));
  addSeparator();

  impl_->presetMenu = addMenu("アニメーションプリセット(&P)");
  impl_->presetMenu->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_bookmarks.svg")));

  impl_->presetLibraryMenu = impl_->presetMenu->addMenu("プリセットライブラリ");
  impl_->presetLibraryMenu->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_bookmarks.svg")));
  const auto addPresetLibraryAction = [this](const QString& label,
                                             const ArtifactCore::KeyframePatternPreset preset) {
    QAction* action = impl_->presetLibraryMenu->addAction(label);
    action->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_bookmarks.svg")));
    impl_->presetLibraryActions_.insert(action, preset);
    return action;
  };
  addPresetLibraryAction("Ramp", ArtifactCore::KeyframePatternPreset::Ramp);
  addPresetLibraryAction("Pulse", ArtifactCore::KeyframePatternPreset::Pulse);
  addPresetLibraryAction("Bounce", ArtifactCore::KeyframePatternPreset::Bounce);
  addPresetLibraryAction("Shake", ArtifactCore::KeyframePatternPreset::Shake);
  addPresetLibraryAction("Loop", ArtifactCore::KeyframePatternPreset::Loop);
  addPresetLibraryAction("Wave", ArtifactCore::KeyframePatternPreset::Wave);
  addPresetLibraryAction("Overshoot", ArtifactCore::KeyframePatternPreset::Overshoot);
  addPresetLibraryAction("Settle", ArtifactCore::KeyframePatternPreset::Settle);
  addPresetLibraryAction("Beat Sync", ArtifactCore::KeyframePatternPreset::BeatSync);

  impl_->presetMenu->addSeparator();

  impl_->saveAnimationPresetAction = impl_->presetMenu->addAction("アニメーションプリセットを保存...");
  impl_->saveAnimationPresetAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_save.svg")));
  impl_->loadAnimationPresetAction = impl_->presetMenu->addAction("アニメーションプリセットを適用...");
  impl_->loadAnimationPresetAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_folder_open.svg")));
  impl_->presetMenu->addSeparator();

  auto dispatchAction = [this](QAction* action) {
   if (!action) {
    return;
   }
   if (action == impl_->addKeyframeAction) { Q_EMIT addKeyframeRequested(); return; }
   if (action == impl_->removeKeyframeAction) { Q_EMIT removeKeyframeRequested(); return; }
   if (action == impl_->selectAllKeyframesAction) { Q_EMIT selectAllKeyframesRequested(); return; }
   if (action == impl_->reverseSelectedKeyframesAction) { Q_EMIT reverseSelectedKeyframesRequested(); return; }
   if (action == impl_->reverseAllKeyframesInLayerAction) { Q_EMIT reverseAllKeyframesInLayerRequested(); return; }
   if (action == impl_->reverseAllKeyframesInSelectedLayersAction) { Q_EMIT reverseAllKeyframesInSelectedLayersRequested(); return; }
   if (action == impl_->reverseAllKeyframesInCompositionAction) { Q_EMIT reverseAllKeyframesInCompositionRequested(); return; }
   if (action == impl_->copyKeyframesAction) { Q_EMIT copyKeyframesRequested(); return; }
   if (action == impl_->pasteKeyframesAction) { Q_EMIT pasteKeyframesRequested(); return; }
   if (action == impl_->linearInterpolationAction) { Q_EMIT applyInterpolationRequested(ArtifactCore::InterpolationType::Linear); return; }
   if (action == impl_->easeInAction) { Q_EMIT applyInterpolationRequested(ArtifactCore::InterpolationType::EaseIn); return; }
   if (action == impl_->easeOutAction) { Q_EMIT applyInterpolationRequested(ArtifactCore::InterpolationType::EaseOut); return; }
   if (action == impl_->easeInOutAction) { Q_EMIT applyInterpolationRequested(ArtifactCore::InterpolationType::EaseInOut); return; }
   if (action == impl_->holdInterpolationAction) { Q_EMIT applyInterpolationRequested(ArtifactCore::InterpolationType::Constant); return; }
   if (action == impl_->bezierInterpolationAction) { Q_EMIT applyInterpolationRequested(ArtifactCore::InterpolationType::Bezier); return; }
   if (action == impl_->showGraphEditorAction) { Q_EMIT showGraphEditorRequested(); return; }
   if (action == impl_->toggleValueGraphAction) { Q_EMIT toggleValueGraphRequested(); return; }
   if (action == impl_->toggleVelocityGraphAction) { Q_EMIT toggleVelocityGraphRequested(); return; }
  if (action == impl_->easingLabAction) {
    EasingLabDialog dialog(
        this,
        [this](ArtifactCore::InterpolationType type) {
          if (auto* timeline = activeTimelineWidget(impl_ && impl_->menu_ ? impl_->menu_->window() : nullptr)) {
            timeline->applyInterpolationToSelectedKeyframes(type);
          }
        });
    dialog.exec();
    return;
  }
  if (action == impl_->keyPatternAction) {
    if (auto* timeline = activeTimelineWidget(impl_ && impl_->menu_ ? impl_->menu_->window() : nullptr)) {
      timeline->showKeyPatternDialog();
    }
    return;
  }
  if (impl_->presetLibraryActions_.contains(action)) {
    if (auto* timeline = activeTimelineWidget(impl_ && impl_->menu_ ? impl_->menu_->window() : nullptr)) {
      timeline->applyAnimationPreset(impl_->presetLibraryActions_.value(action));
    }
    return;
  }
   if (action == impl_->goToNextKeyframeAction) { Q_EMIT goToNextKeyframeRequested(); return; }
   if (action == impl_->goToPreviousKeyframeAction) { Q_EMIT goToPreviousKeyframeRequested(); return; }
   if (action == impl_->goToFirstKeyframeAction) { Q_EMIT goToFirstKeyframeRequested(); return; }
   if (action == impl_->goToLastKeyframeAction) { Q_EMIT goToLastKeyframeRequested(); return; }
   if (action == impl_->enableTimeRemapAction) { Q_EMIT enableTimeRemapRequested(); return; }
   if (action == impl_->freezeFrameAction) { Q_EMIT freezeFrameRequested(); return; }
   if (action == impl_->timeReverseAction) { Q_EMIT timeReverseRequested(); return; }
   if (action == impl_->addExpressionAction) { openNewExpressionCopilot(impl_ && impl_->menu_ ? impl_->menu_->window() : nullptr); return; }
   if (action == impl_->editExpressionAction) { openActiveExpressionCopilot(impl_ && impl_->menu_ ? impl_->menu_->window() : nullptr); return; }
   if (action == impl_->removeExpressionAction) { clearActiveExpression(impl_ && impl_->menu_ ? impl_->menu_->window() : nullptr); return; }
   if (action == impl_->convertToKeyframesAction) { convertActiveExpressionToKeyframes(impl_ && impl_->menu_ ? impl_->menu_->window() : nullptr); return; }
   if (action == impl_->bakeLiveToKeyframesAction) { bakeActivePropertyToKeyframes(impl_ && impl_->menu_ ? impl_->menu_->window() : nullptr); return; }
   if (action == impl_->configureAudioReactiveAction) { configureActiveAudioReactiveBinding(impl_ && impl_->menu_ ? impl_->menu_->window() : nullptr); return; }
   if (action == impl_->removeAudioReactiveAction) { removeActiveAudioReactiveBinding(impl_ && impl_->menu_ ? impl_->menu_->window() : nullptr); return; }
   if (action == impl_->previewAudioReactiveAction) { previewActiveAudioReactiveBinding(impl_ && impl_->menu_ ? impl_->menu_->window() : nullptr); return; }
   if (action == impl_->bakeAudioReactiveAction) { bakeActiveAudioReactiveBinding(impl_ && impl_->menu_ ? impl_->menu_->window() : nullptr); return; }
   if (action == impl_->armAudioReactiveAction) { beginActiveAudioReactiveRecording(impl_ && impl_->menu_ ? impl_->menu_->window() : nullptr); return; }
   if (action == impl_->commitAudioReactiveAction) { commitAudioReactiveRecording(); return; }
   if (action == impl_->cancelAudioReactiveAction) { cancelAudioReactiveRecording(); return; }
   if (action == impl_->saveAnimationPresetAction) { saveActiveExpressionPreset(impl_ && impl_->menu_ ? impl_->menu_->window() : nullptr); return; }
   if (action == impl_->loadAnimationPresetAction) { loadActiveExpressionPreset(impl_ && impl_->menu_ ? impl_->menu_->window() : nullptr); return; }
  };

  QObject::connect(this, &QMenu::triggered, this, dispatchAction);

  if (auto* settings = ArtifactCore::ArtifactAppSettings::instance()) {
    connect(settings, &ArtifactCore::ArtifactAppSettings::settingsChanged, impl_->menu_, [this]() {
      if (!impl_ || !impl_->toggleValueGraphAction || !impl_->toggleVelocityGraphAction) {
        return;
      }
      if (auto* appSettings = ArtifactCore::ArtifactAppSettings::instance()) {
        const bool speedMode =
            appSettings->timelineGraphEditorModeText().compare(
                QStringLiteral("Speed"), Qt::CaseInsensitive) == 0;
        const QSignalBlocker blockValue(impl_->toggleValueGraphAction);
        const QSignalBlocker blockSpeed(impl_->toggleVelocityGraphAction);
        impl_->toggleValueGraphAction->setChecked(!speedMode);
        impl_->toggleVelocityGraphAction->setChecked(speedMode);
      }
    });
    const bool speedMode =
        settings->timelineGraphEditorModeText().compare(
            QStringLiteral("Speed"), Qt::CaseInsensitive) == 0;
    const QSignalBlocker blockValue(impl_->toggleValueGraphAction);
    const QSignalBlocker blockSpeed(impl_->toggleVelocityGraphAction);
    impl_->toggleValueGraphAction->setChecked(!speedMode);
    impl_->toggleVelocityGraphAction->setChecked(speedMode);
  }

  impl_->refreshEnabledState();
 }

 ArtifactAnimationMenu::~ArtifactAnimationMenu()
 {
  delete impl_;
 }

 QAction* ArtifactAnimationMenu::getAddKeyframeAction() const
 {
  return impl_->addKeyframeAction;
 }

 QAction* ArtifactAnimationMenu::getRemoveKeyframeAction() const
 {
  return impl_->removeKeyframeAction;
 }

 QAction* ArtifactAnimationMenu::getSelectAllKeyframesAction() const
 {
  return impl_->selectAllKeyframesAction;
 }

 QAction* ArtifactAnimationMenu::getCopyKeyframesAction() const
 {
  return impl_->copyKeyframesAction;
 }

 QAction* ArtifactAnimationMenu::getPasteKeyframesAction() const
 {
  return impl_->pasteKeyframesAction;
 }

} // namespace Artifact
