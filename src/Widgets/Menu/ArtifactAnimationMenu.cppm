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
#include <QSet>
#include <wobjectimpl.h>

module Menu.Animation;

import Translation.Manager;
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
import Memory.SharedPtr;

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

 void undo() override { lastOperationSucceeded_ = apply(before_); }
 void redo() override { lastOperationSucceeded_ = apply(after_); }
 QString label() const override { return label_; }
 bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }

private:
 bool apply(const QVector<CompositionAudioReactiveBinding>& bindings) {
  const auto composition = composition_.lock();
  if (!composition) return false;
  composition->setAudioReactiveBindings(bindings);
  const auto applied = composition->audioReactiveBindings();
  if (applied.size() != bindings.size()) return false;
  for (int index = 0; index < bindings.size(); ++index) {
   if (applied[index].toJson() != bindings[index].toJson()) return false;
  }
  if (auto* manager = UndoManager::instance()) {
   manager->notifyAnythingChanged();
  }
  return true;
 }
 ArtifactCompositionWeakPtr composition_;
 QVector<CompositionAudioReactiveBinding> before_;
 QVector<CompositionAudioReactiveBinding> after_;
 QString label_;
 bool lastOperationSucceeded_ = true;
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

 void undo() override {
  lastOperationSucceeded_ = apply(before_, beforeValue_, after_, afterValue_);
 }
 void redo() override {
  lastOperationSucceeded_ = apply(after_, afterValue_, before_, beforeValue_);
 }
 bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
 QString label() const override {
  return QStringLiteral("Bake Audio Reactive Binding");
 }

private:
 bool apply(const std::vector<ArtifactCore::KeyFrame>& keyframes,
            const QVariant& value,
            const std::vector<ArtifactCore::KeyFrame>& compensationKeyframes,
            const QVariant& compensationValue) {
  const auto layer = layer_.lock();
  if (!layer) return false;
  const auto property = layer->getProperty(propertyPath_);
  if (!property) return false;
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
  const bool applied = property->keyFrameCount() == keyframes.size() &&
                       property->getValue() == value;
  if (!applied) {
   property->clearKeyFrames();
   for (const auto& keyframe : compensationKeyframes) {
    property->addKeyFrame(
        keyframe.time, keyframe.value, keyframe.interpolation,
        keyframe.cp1_x, keyframe.cp1_y, keyframe.cp2_x, keyframe.cp2_y,
        keyframe.roving);
    property->setKeyFrameAnchorAt(keyframe.time, keyframe.anchor);
    property->setKeyFrameColorLabelAt(keyframe.time, keyframe.colorLabel);
   }
   property->setValue(compensationValue);
   return false;
  }
  layer->changed();
  if (auto* manager = UndoManager::instance()) {
   manager->notifyAnythingChanged();
  }
  return true;
 }
 ArtifactAbstractLayerWeak layer_;
 QString propertyPath_;
 std::vector<ArtifactCore::KeyFrame> before_;
 std::vector<ArtifactCore::KeyFrame> after_;
 QVariant beforeValue_;
 QVariant afterValue_;
 bool lastOperationSucceeded_ = true;
};

class CommitAudioReactiveRecordingCommand final : public UndoCommand {
public:
 CommitAudioReactiveRecordingCommand(
     ArtifactCompositionWeakPtr composition,
     QVector<LiveControlRecordingPropertyChange> changes)
  : composition_(std::move(composition)), changes_(std::move(changes)) {}

 void undo() override { lastOperationSucceeded_ = apply(true); }
 void redo() override { lastOperationSucceeded_ = apply(false); }
 QString label() const override {
  return QStringLiteral("Commit Audio Reactive Recording");
 }
 bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }

private:
 bool apply(const bool before) {
  const auto composition = composition_.lock();
  if (!composition) {
   return false;
  }
  for (const auto& change : changes_) {
   const auto layer = composition->layerById(change.layerId);
   if (!layer || !layer->getProperty(change.propertyPath)) {
    return false;
   }
  }
  for (const auto& change : changes_) {
   const auto layer = composition->layerById(change.layerId);
   const auto property = layer ? layer->getProperty(change.propertyPath) : nullptr;
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
   if (property->keyFrameCount() != keyframes.size() ||
       property->getValue() != (before ? change.beforeValue : change.afterValue)) {
    return false;
   }
   layer->changed();
  }
  if (auto* manager = UndoManager::instance()) {
   manager->notifyAnythingChanged();
  }
  return true;
 }
 ArtifactCompositionWeakPtr composition_;
 QVector<LiveControlRecordingPropertyChange> changes_;
 bool lastOperationSucceeded_ = true;
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
  : ArtifactCore::SharedPtr<ArtifactCore::AbstractProperty>{};
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
  return manager->push(std::make_unique<ChangeAudioReactiveBindingsCommand>(
      composition, before, after,
      QStringLiteral("Configure Audio Reactive Binding")));
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
  return manager->push(std::make_unique<ChangeAudioReactiveBindingsCommand>(
      composition, before, after,
      QStringLiteral("Remove Audio Reactive Binding")));
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
  : ArtifactCore::SharedPtr<ArtifactCore::AbstractProperty>{};
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
 bool applied = false;
 if (auto* manager = UndoManager::instance()) {
  applied = manager->push(std::make_unique<BakeAudioReactiveBindingCommand>(
      layer, propertyPath, beforeKeyframes, bakedKeyframes,
      beforeValue, afterValue));
 } else {
  BakeAudioReactiveBindingCommand command(
      layer, propertyPath, beforeKeyframes, bakedKeyframes,
      beforeValue, afterValue);
  command.redo();
  applied = command.lastOperationSucceeded();
 }
 if (!applied) {
  return false;
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
 bool applied = true;
 if (!changes.isEmpty()) {
  if (auto* manager = UndoManager::instance()) {
   applied = manager->push(std::make_unique<CommitAudioReactiveRecordingCommand>(
       composition, std::move(changes)));
  } else {
   CommitAudioReactiveRecordingCommand command(composition, std::move(changes));
   command.redo();
   applied = command.lastOperationSucceeded();
  }
 }
 return applied;
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

void addTransformKeyframesAtPlayhead(QWidget* root,
                                     const QSet<QString>& propertyPaths)
{
 auto* timeline = activeTimelineWidget(root);
 if (!timeline || propertyPaths.isEmpty()) return;
 const QSet<QString> previous = timeline->selectedPropertyPaths();
 timeline->setSelectedPropertyPaths(propertyPaths);
 timeline->addKeyframeAtPlayhead();
 timeline->setSelectedPropertyPaths(previous);
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
 copilot->setWindowTitle(QStringLiteral("Expression Editor"));
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
  QAction* setPositionKeyframesAction = nullptr;
  QAction* setRotationKeyframesAction = nullptr;
  QAction* setScaleKeyframesAction = nullptr;
  QAction* setAllTransformKeyframesAction = nullptr;
  QAction* nudgeBackwardAction = nullptr;
  QAction* nudgeForwardAction = nullptr;
  QAction* moveLayerStartAction = nullptr;
  QAction* moveLayerEndAction = nullptr;
  QAction* trimLayerInAction = nullptr;
  QAction* trimLayerOutAction = nullptr;
  QAction* deleteLayerAnimationAction = nullptr;
  QAction* distributeKeyframesAction = nullptr;
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
  QAction* catmullRomInterpolationAction = nullptr;
  QAction* hermiteInterpolationAction = nullptr;

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
  QAction* slowHalfAction = nullptr;
  QAction* slowQuarterAction = nullptr;
  QAction* stopMotion12FpsAction = nullptr;
  QAction* stopMotion8FpsAction = nullptr;
  QAction* stopMotion4FpsAction = nullptr;

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
  setPositionKeyframesAction->setEnabled(hasLayer);
  setRotationKeyframesAction->setEnabled(hasLayer);
  setScaleKeyframesAction->setEnabled(hasLayer);
  setAllTransformKeyframesAction->setEnabled(hasLayer);
  nudgeBackwardAction->setEnabled(hasLayer);
  nudgeForwardAction->setEnabled(hasLayer);
  moveLayerStartAction->setEnabled(hasLayer);
  moveLayerEndAction->setEnabled(hasLayer);
  trimLayerInAction->setEnabled(hasLayer);
  trimLayerOutAction->setEnabled(hasLayer);
  deleteLayerAnimationAction->setEnabled(hasLayer);
  distributeKeyframesAction->setEnabled(hasLayer);
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
  setTitle(TranslationManager::instance().tr(QStringLiteral("menu.animation.label"), QStringLiteral("アニメーション(&A)")));
  setIcon(menuIcon(QStringLiteral("Studio/menubar_animation.svg")));
  setTearOffEnabled(false);

  impl_->addKeyframeAction = addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.add_keyframe"), QStringLiteral("キーフレームを追加")));
  impl_->addKeyframeAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_add_circle.svg")));
  impl_->addKeyframeAction->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::AnimationAddKeyframe));
  impl_->addKeyframeAction->setToolTip(QStringLiteral("Add Keyframe"));
  impl_->addKeyframeAction->setStatusTip(QStringLiteral("Add a keyframe at the current time"));
  auto* transformKeyframesMenu = addMenu(TranslationManager::instance().tr(QStringLiteral("menu.animation.transform_keyframes"), QStringLiteral("トランスフォームキーを設定")));
  transformKeyframesMenu->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_animation.svg")));
  transformKeyframesMenu->setAccessibleName(QStringLiteral("Set Transform Keys"));
  transformKeyframesMenu->setAccessibleDescription(QStringLiteral("Add keyframes for transform properties"));
  impl_->setPositionKeyframesAction = transformKeyframesMenu->addAction("Position");
  impl_->setPositionKeyframesAction->setToolTip(QStringLiteral("Set Position Keys"));
  impl_->setPositionKeyframesAction->setStatusTip(QStringLiteral("Add position keyframes"));
  impl_->setRotationKeyframesAction = transformKeyframesMenu->addAction("Rotation");
  impl_->setRotationKeyframesAction->setToolTip(QStringLiteral("Set Rotation Keys"));
  impl_->setRotationKeyframesAction->setStatusTip(QStringLiteral("Add rotation keyframes"));
  impl_->setScaleKeyframesAction = transformKeyframesMenu->addAction("Scale");
  impl_->setScaleKeyframesAction->setToolTip(QStringLiteral("Set Scale Keys"));
  impl_->setScaleKeyframesAction->setStatusTip(QStringLiteral("Add scale keyframes"));
  transformKeyframesMenu->addSeparator();
  impl_->setAllTransformKeyframesAction = transformKeyframesMenu->addAction("All");
  impl_->setAllTransformKeyframesAction->setToolTip(QStringLiteral("Set All Transform Keys"));
  impl_->setAllTransformKeyframesAction->setStatusTip(QStringLiteral("Add keyframes for all transform properties"));
  auto* nudgeMenu = addMenu(TranslationManager::instance().tr(QStringLiteral("menu.animation.nudge_keyframes"), QStringLiteral("キーフレームをナッジ")));
  nudgeMenu->setAccessibleName(QStringLiteral("Nudge Keyframes"));
  nudgeMenu->setAccessibleDescription(QStringLiteral("Move selected keyframes by one frame"));
  impl_->nudgeBackwardAction = nudgeMenu->addAction("Backward");
  impl_->nudgeBackwardAction->setToolTip(QStringLiteral("Nudge Keyframes Backward"));
  impl_->nudgeBackwardAction->setStatusTip(QStringLiteral("Move selected keyframes one frame earlier"));
  impl_->nudgeForwardAction = nudgeMenu->addAction("Forward");
  impl_->nudgeForwardAction->setToolTip(QStringLiteral("Nudge Keyframes Forward"));
  impl_->nudgeForwardAction->setStatusTip(QStringLiteral("Move selected keyframes one frame later"));
  auto* layerTimingMenu = addMenu(TranslationManager::instance().tr(QStringLiteral("menu.animation.layer_timing"), QStringLiteral("レイヤー時間操作")));
  layerTimingMenu->setAccessibleName(QStringLiteral("Layer Timing"));
  layerTimingMenu->setAccessibleDescription(QStringLiteral("Move layer boundaries to the current frame"));
  impl_->moveLayerStartAction = layerTimingMenu->addAction("Move Layer Start to Current Frame");
  impl_->moveLayerStartAction->setToolTip(QStringLiteral("Move Layer Start to Current Frame"));
  impl_->moveLayerStartAction->setStatusTip(QStringLiteral("Move the layer start to the playhead"));
  impl_->moveLayerEndAction = layerTimingMenu->addAction("Move Layer End to Current Frame");
  impl_->moveLayerEndAction->setToolTip(QStringLiteral("Move Layer End to Current Frame"));
  impl_->moveLayerEndAction->setStatusTip(QStringLiteral("Move the layer end to the playhead"));
  impl_->trimLayerInAction = layerTimingMenu->addAction("Set Layer In Point to Current Frame");
  impl_->trimLayerInAction->setToolTip(QStringLiteral("Set Layer In Point to Current Frame"));
  impl_->trimLayerInAction->setStatusTip(QStringLiteral("Trim the layer in point to the playhead"));
  impl_->trimLayerOutAction = layerTimingMenu->addAction("Set Layer Out Point to Current Frame");
  impl_->trimLayerOutAction->setToolTip(QStringLiteral("Set Layer Out Point to Current Frame"));
  impl_->trimLayerOutAction->setStatusTip(QStringLiteral("Trim the layer out point to the playhead"));
  impl_->deleteLayerAnimationAction = addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.delete_animation"), QStringLiteral("アニメーションを削除")));
  impl_->deleteLayerAnimationAction->setToolTip(QStringLiteral("Delete Layer Animation"));
  impl_->deleteLayerAnimationAction->setStatusTip(QStringLiteral("Remove all animation from the selected layer"));
  impl_->distributeKeyframesAction = addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.distribute_selected"), QStringLiteral("選択キーフレームを均等配置")));
  impl_->distributeKeyframesAction->setToolTip(QStringLiteral("Distribute Selected Keyframes"));
  impl_->distributeKeyframesAction->setStatusTip(QStringLiteral("Space the selected keyframes evenly in time"));
  impl_->removeKeyframeAction = addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.remove_keyframe"), QStringLiteral("キーフレームを削除")));
  impl_->removeKeyframeAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_remove_circle.svg")));
  impl_->removeKeyframeAction->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::AnimationRemoveKeyframe));
  impl_->removeKeyframeAction->setToolTip(QStringLiteral("Remove Keyframe"));
  impl_->removeKeyframeAction->setStatusTip(QStringLiteral("Remove keyframes at the current time"));

  impl_->selectAllKeyframesAction = addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.select_all"), QStringLiteral("すべてのキーフレームを選択")));
  impl_->selectAllKeyframesAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_select_all.svg")));
  impl_->selectAllKeyframesAction->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::AnimationSelectAllKeyframes));
  impl_->selectAllKeyframesAction->setToolTip(QStringLiteral("Select All Keyframes"));
  impl_->selectAllKeyframesAction->setStatusTip(QStringLiteral("Select every keyframe in the layer"));

  auto* reverseMenu = addMenu(TranslationManager::instance().tr(QStringLiteral("menu.animation.reverse_menu"), QStringLiteral("キーフレーム反転(&R)")));
  reverseMenu->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_swap_horiz.svg")));
  reverseMenu->setAccessibleName(QStringLiteral("Reverse Keyframes"));
  reverseMenu->setAccessibleDescription(QStringLiteral("Reverse keyframe timing over a scope"));
  impl_->reverseSelectedKeyframesAction = reverseMenu->addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.reverse_selected"), QStringLiteral("選択キーフレームを反転")));
  impl_->reverseSelectedKeyframesAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_swap_horiz.svg")));
  impl_->reverseSelectedKeyframesAction->setToolTip(QStringLiteral("Reverse Selected Keyframes"));
  impl_->reverseSelectedKeyframesAction->setStatusTip(QStringLiteral("Reverse the timing of the selected keyframes"));
  impl_->reverseAllKeyframesInLayerAction = reverseMenu->addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.reverse_layer"), QStringLiteral("現在のレイヤーのキーフレームをすべて反転")));
  impl_->reverseAllKeyframesInLayerAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_swap_horiz.svg")));
  impl_->reverseAllKeyframesInLayerAction->setToolTip(QStringLiteral("Reverse All Keyframes in Layer"));
  impl_->reverseAllKeyframesInLayerAction->setStatusTip(QStringLiteral("Reverse every keyframe in the current layer"));
  impl_->reverseAllKeyframesInSelectedLayersAction = reverseMenu->addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.reverse_selected_layers"), QStringLiteral("選択レイヤーのキーフレームをすべて反転")));
  impl_->reverseAllKeyframesInSelectedLayersAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_swap_horiz.svg")));
  impl_->reverseAllKeyframesInSelectedLayersAction->setToolTip(QStringLiteral("Reverse Keyframes in Selected Layers"));
  impl_->reverseAllKeyframesInSelectedLayersAction->setStatusTip(QStringLiteral("Reverse every keyframe in the selected layers"));
  impl_->reverseAllKeyframesInCompositionAction = reverseMenu->addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.reverse_composition"), QStringLiteral("コンポ全体のキーフレームをすべて反転")));
  impl_->reverseAllKeyframesInCompositionAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_swap_horiz.svg")));
  impl_->reverseAllKeyframesInCompositionAction->setToolTip(QStringLiteral("Reverse All Keyframes in Composition"));
  impl_->reverseAllKeyframesInCompositionAction->setStatusTip(QStringLiteral("Reverse every keyframe in the composition"));

  impl_->copyKeyframesAction = addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.copy_keyframes"), QStringLiteral("キーフレームをコピー")));
  impl_->copyKeyframesAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_content_copy.svg")));
  impl_->copyKeyframesAction->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::AnimationCopyKeyframes));
  impl_->copyKeyframesAction->setToolTip(QStringLiteral("Copy Keyframes"));
  impl_->copyKeyframesAction->setStatusTip(QStringLiteral("Copy the selected keyframes"));

  impl_->pasteKeyframesAction = addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.paste_keyframes"), QStringLiteral("キーフレームをペースト")));
  impl_->pasteKeyframesAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_content_paste.svg")));
  impl_->pasteKeyframesAction->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::AnimationPasteKeyframes));
  impl_->pasteKeyframesAction->setToolTip(QStringLiteral("Paste Keyframes"));
  impl_->pasteKeyframesAction->setStatusTip(QStringLiteral("Paste keyframes at the current time"));

  addSeparator();

  impl_->interpolationMenu = addMenu(TranslationManager::instance().tr(QStringLiteral("menu.animation.interpolation_menu"), QStringLiteral("キーフレーム補間(&I)")));
  impl_->interpolationMenu->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_timeline.svg")));
  impl_->interpolationMenu->setAccessibleName(QStringLiteral("Keyframe Interpolation"));
  impl_->interpolationMenu->setAccessibleDescription(QStringLiteral("Set interpolation for the selected keyframes"));

  impl_->linearInterpolationAction = impl_->interpolationMenu->addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.interp_linear"), QStringLiteral("リニア")));
  impl_->linearInterpolationAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_straighten.svg")));
  impl_->linearInterpolationAction->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::AnimationLinearInterpolation));
  impl_->linearInterpolationAction->setToolTip(QStringLiteral("Linear Interpolation"));
  impl_->linearInterpolationAction->setStatusTip(QStringLiteral("Interpolate with constant speed"));
  impl_->easeInAction = impl_->interpolationMenu->addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.interp_ease_in"), QStringLiteral("イージーイーズイン")));
  impl_->easeInAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_trending_up.svg")));
  impl_->easeInAction->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::AnimationEaseIn));
  impl_->easeInAction->setToolTip(QStringLiteral("Ease In Interpolation"));
  impl_->easeInAction->setStatusTip(QStringLiteral("Start slow and accelerate"));
  impl_->easeOutAction = impl_->interpolationMenu->addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.interp_ease_out"), QStringLiteral("イージーイーズアウト")));
  impl_->easeOutAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_trending_down.svg")));
  impl_->easeOutAction->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::AnimationEaseOut));
  impl_->easeOutAction->setToolTip(QStringLiteral("Ease Out Interpolation"));
  impl_->easeOutAction->setStatusTip(QStringLiteral("Start fast and decelerate"));
  impl_->easeInOutAction = impl_->interpolationMenu->addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.interp_ease_in_out"), QStringLiteral("イージーイーズ")));
  impl_->easeInOutAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_show_chart.svg")));
  impl_->easeInOutAction->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::AnimationEaseInOut));
  impl_->easeInOutAction->setToolTip(QStringLiteral("Ease In-Out Interpolation"));
  impl_->easeInOutAction->setStatusTip(QStringLiteral("Start and end slow with fast middle"));
  impl_->holdInterpolationAction = impl_->interpolationMenu->addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.interp_hold"), QStringLiteral("停止")));
  impl_->holdInterpolationAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_pause.svg")));
  impl_->holdInterpolationAction->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::AnimationHoldInterpolation));
  impl_->holdInterpolationAction->setToolTip(QStringLiteral("Hold Interpolation"));
  impl_->holdInterpolationAction->setStatusTip(QStringLiteral("Hold the value until the next keyframe"));
  impl_->bezierInterpolationAction = impl_->interpolationMenu->addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.interp_bezier"), QStringLiteral("ベジェ")));
  impl_->bezierInterpolationAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_edit.svg")));
  impl_->bezierInterpolationAction->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::AnimationBezierInterpolation));
  impl_->bezierInterpolationAction->setToolTip(QStringLiteral("Bezier Interpolation"));
  impl_->bezierInterpolationAction->setStatusTip(QStringLiteral("Interpolate along editable bezier handles"));
  impl_->catmullRomInterpolationAction = impl_->interpolationMenu->addAction("Catmull-Rom");
  impl_->catmullRomInterpolationAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_show_chart.svg")));
  impl_->catmullRomInterpolationAction->setToolTip(QStringLiteral("Catmull-Rom Interpolation"));
  impl_->catmullRomInterpolationAction->setStatusTip(QStringLiteral("Interpolate with a smooth Catmull-Rom spline"));
  impl_->hermiteInterpolationAction = impl_->interpolationMenu->addAction("Hermite");
  impl_->hermiteInterpolationAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_show_chart.svg")));
  impl_->hermiteInterpolationAction->setToolTip(QStringLiteral("Hermite Interpolation"));
  impl_->hermiteInterpolationAction->setStatusTip(QStringLiteral("Interpolate with a Hermite spline"));

  impl_->linearInterpolationAction->setCheckable(true);
  impl_->bezierInterpolationAction->setCheckable(true);
  impl_->holdInterpolationAction->setCheckable(true);
  impl_->catmullRomInterpolationAction->setCheckable(true);
  impl_->hermiteInterpolationAction->setCheckable(true);

  addSeparator();

  impl_->graphEditorMenu = addMenu(TranslationManager::instance().tr(QStringLiteral("menu.animation.graph_editor_menu"), QStringLiteral("カーブエディタ(&G)")));
  impl_->graphEditorMenu->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_show_chart.svg")));
  impl_->graphEditorMenu->setAccessibleName(QStringLiteral("Curve Editor"));
  impl_->graphEditorMenu->setAccessibleDescription(QStringLiteral("Open graph tools for editing animation curves"));

  impl_->showGraphEditorAction = impl_->graphEditorMenu->addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.show_graph_editor"), QStringLiteral("カーブエディタを表示")));
  impl_->showGraphEditorAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_query_stats.svg")));
  impl_->showGraphEditorAction->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::AnimationShowGraphEditor));
  impl_->showGraphEditorAction->setToolTip(QStringLiteral("Show Curve Editor"));
  impl_->showGraphEditorAction->setStatusTip(QStringLiteral("Open the curve editor panel"));
  impl_->graphEditorMenu->addSeparator();
  impl_->graphModeGroup = new QActionGroup(impl_->menu_);
  impl_->graphModeGroup->setExclusive(true);
  impl_->toggleValueGraphAction = impl_->graphEditorMenu->addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.show_value_graph"), QStringLiteral("値グラフを表示")));
  impl_->toggleValueGraphAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_show_chart.svg")));
  impl_->toggleValueGraphAction->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::AnimationToggleValueGraph));
  impl_->toggleValueGraphAction->setCheckable(true);
  impl_->toggleValueGraphAction->setToolTip(QStringLiteral("Show Value Graph"));
  impl_->toggleValueGraphAction->setStatusTip(QStringLiteral("Display animation as a value graph"));
  impl_->graphModeGroup->addAction(impl_->toggleValueGraphAction);
  impl_->toggleVelocityGraphAction = impl_->graphEditorMenu->addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.show_velocity_graph"), QStringLiteral("速度グラフを表示")));
  impl_->toggleVelocityGraphAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_speed.svg")));
  impl_->toggleVelocityGraphAction->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::AnimationToggleVelocityGraph));
  impl_->toggleVelocityGraphAction->setCheckable(true);
  impl_->toggleVelocityGraphAction->setToolTip(QStringLiteral("Show Velocity Graph"));
  impl_->toggleVelocityGraphAction->setStatusTip(QStringLiteral("Display animation as a velocity graph"));
  impl_->graphModeGroup->addAction(impl_->toggleVelocityGraphAction);
  impl_->easingLabAction = impl_->graphEditorMenu->addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.open_easing_lab"), QStringLiteral("EasingLab を開く")));
  impl_->easingLabAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_tune.svg")));
  impl_->easingLabAction->setToolTip(QStringLiteral("Open Easing Lab"));
  impl_->easingLabAction->setStatusTip(QStringLiteral("Open the easing curve laboratory"));
  impl_->keyPatternAction = impl_->graphEditorMenu->addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.open_key_pattern"), QStringLiteral("Key Pattern Dialog を開く")));
  impl_->keyPatternAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_animation.svg")));
  impl_->keyPatternAction->setToolTip(QStringLiteral("Open Key Pattern Dialog"));
  impl_->keyPatternAction->setStatusTip(QStringLiteral("Open the keyframe pattern dialog"));

  impl_->navigationMenu = addMenu(TranslationManager::instance().tr(QStringLiteral("menu.animation.navigation_menu"), QStringLiteral("ナビゲーション(&N)")));
  impl_->navigationMenu->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_skip_next.svg")));
  impl_->navigationMenu->setAccessibleName(QStringLiteral("Keyframe Navigation"));
  impl_->navigationMenu->setAccessibleDescription(QStringLiteral("Jump between keyframes"));

  impl_->goToNextKeyframeAction = impl_->navigationMenu->addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.next_keyframe"), QStringLiteral("次のキーフレームに移動")));
  impl_->goToNextKeyframeAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_skip_next.svg")));
  impl_->goToNextKeyframeAction->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::AnimationGoToNextKeyframe));
  impl_->goToNextKeyframeAction->setToolTip(QStringLiteral("Go to Next Keyframe"));
  impl_->goToNextKeyframeAction->setStatusTip(QStringLiteral("Move to the next keyframe"));
  impl_->goToPreviousKeyframeAction = impl_->navigationMenu->addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.prev_keyframe"), QStringLiteral("前のキーフレームに移動")));
  impl_->goToPreviousKeyframeAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_skip_previous.svg")));
  impl_->goToPreviousKeyframeAction->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::AnimationGoToPreviousKeyframe));
  impl_->goToPreviousKeyframeAction->setToolTip(QStringLiteral("Go to Previous Keyframe"));
  impl_->goToPreviousKeyframeAction->setStatusTip(QStringLiteral("Move to the previous keyframe"));

  impl_->navigationMenu->addSeparator();

  impl_->goToFirstKeyframeAction = impl_->navigationMenu->addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.first_keyframe"), QStringLiteral("最初のキーフレームに移動")));
  impl_->goToFirstKeyframeAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_fast_rewind.svg")));
  impl_->goToFirstKeyframeAction->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::AnimationGoToFirstKeyframe));
  impl_->goToFirstKeyframeAction->setToolTip(QStringLiteral("Go to First Keyframe"));
  impl_->goToFirstKeyframeAction->setStatusTip(QStringLiteral("Move to the first keyframe"));
  impl_->goToLastKeyframeAction = impl_->navigationMenu->addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.last_keyframe"), QStringLiteral("最後のキーフレームに移動")));
  impl_->goToLastKeyframeAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_fast_forward.svg")));
  impl_->goToLastKeyframeAction->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::AnimationGoToLastKeyframe));
  impl_->goToLastKeyframeAction->setToolTip(QStringLiteral("Go to Last Keyframe"));
  impl_->goToLastKeyframeAction->setStatusTip(QStringLiteral("Move to the last keyframe"));

  addSeparator();

  impl_->timeRemapMenu = addMenu(TranslationManager::instance().tr(QStringLiteral("menu.animation.time_remap_menu"), QStringLiteral("タイムリマップ(&T)")));
  impl_->timeRemapMenu->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_schedule.svg")));
  impl_->timeRemapMenu->setAccessibleName(QStringLiteral("Time Remap"));
  impl_->timeRemapMenu->setAccessibleDescription(QStringLiteral("Retiming tools for the selected layer"));

  impl_->enableTimeRemapAction = impl_->timeRemapMenu->addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.enable_time_remap"), QStringLiteral("タイムリマップ可能にする")));
  impl_->enableTimeRemapAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_schedule.svg")));
  impl_->enableTimeRemapAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_T));
  impl_->enableTimeRemapAction->setToolTip(QStringLiteral("Enable Time Remap"));
  impl_->enableTimeRemapAction->setStatusTip(QStringLiteral("Enable time remapping on the selected layer"));
  impl_->freezeFrameAction = impl_->timeRemapMenu->addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.freeze_frame"), QStringLiteral("フレームをフリーズ")));
  impl_->freezeFrameAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_pause_circle.svg")));
  impl_->freezeFrameAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_J));
  impl_->freezeFrameAction->setToolTip(QStringLiteral("Freeze Frame"));
  impl_->freezeFrameAction->setStatusTip(QStringLiteral("Freeze the current frame"));
  impl_->timeReverseAction = impl_->timeRemapMenu->addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.reverse_time"), QStringLiteral("時間反転レイヤー")));
  impl_->timeReverseAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_swap_horiz.svg")));
  impl_->timeReverseAction->setToolTip(QStringLiteral("Reverse Layer Time"));
  impl_->timeReverseAction->setStatusTip(QStringLiteral("Play the layer time in reverse"));
  impl_->timeRemapMenu->addSeparator();
  impl_->slowHalfAction = impl_->timeRemapMenu->addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.slow_50"), QStringLiteral("スローモーション 50%（レイヤー尺を延長）")));
  impl_->slowHalfAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_schedule.svg")));
  impl_->slowHalfAction->setToolTip(QStringLiteral("Slow Motion 50 Percent"));
  impl_->slowHalfAction->setStatusTip(QStringLiteral("Stretch the layer to half speed"));
  impl_->slowQuarterAction = impl_->timeRemapMenu->addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.slow_25"), QStringLiteral("スローモーション 25%（レイヤー尺を延長）")));
  impl_->slowQuarterAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_schedule.svg")));
  impl_->slowQuarterAction->setToolTip(QStringLiteral("Slow Motion 25 Percent"));
  impl_->slowQuarterAction->setStatusTip(QStringLiteral("Stretch the layer to quarter speed"));
  impl_->timeRemapMenu->addSeparator();
  impl_->stopMotion12FpsAction = impl_->timeRemapMenu->addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.stop_motion_12"), QStringLiteral("コマ撮り 12 fps")));
  impl_->stopMotion12FpsAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_pause_circle.svg")));
  impl_->stopMotion12FpsAction->setToolTip(QStringLiteral("Stop Motion 12 fps"));
  impl_->stopMotion12FpsAction->setStatusTip(QStringLiteral("Quantize motion to 12 frames per second"));
  impl_->stopMotion8FpsAction = impl_->timeRemapMenu->addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.stop_motion_8"), QStringLiteral("コマ撮り 8 fps")));
  impl_->stopMotion8FpsAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_pause_circle.svg")));
  impl_->stopMotion8FpsAction->setToolTip(QStringLiteral("Stop Motion 8 fps"));
  impl_->stopMotion8FpsAction->setStatusTip(QStringLiteral("Quantize motion to 8 frames per second"));
  impl_->stopMotion4FpsAction = impl_->timeRemapMenu->addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.stop_motion_4"), QStringLiteral("コマ撮り 4 fps")));
  impl_->stopMotion4FpsAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_pause_circle.svg")));
  impl_->stopMotion4FpsAction->setToolTip(QStringLiteral("Stop Motion 4 fps"));
  impl_->stopMotion4FpsAction->setStatusTip(QStringLiteral("Quantize motion to 4 frames per second"));

  impl_->expressionMenu = addMenu(TranslationManager::instance().tr(QStringLiteral("menu.animation.expression_menu"), QStringLiteral("エクスプレッション(&E)")));
  impl_->expressionMenu->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_functions.svg")));
  impl_->expressionMenu->setAccessibleName(QStringLiteral("Expressions"));
  impl_->expressionMenu->setAccessibleDescription(QStringLiteral("Add and manage property expressions"));

  impl_->addExpressionAction = impl_->expressionMenu->addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.expr_add"), QStringLiteral("エクスプレッションを追加...")));
  impl_->addExpressionAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_add.svg")));
  impl_->addExpressionAction->setShortcut(QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_Equal));
  impl_->addExpressionAction->setToolTip(QStringLiteral("Add Expression"));
  impl_->addExpressionAction->setStatusTip(QStringLiteral("Add an expression to the focused property"));
  impl_->editExpressionAction = impl_->expressionMenu->addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.expr_edit"), QStringLiteral("エクスプレッションを編集...")));
  impl_->editExpressionAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_edit.svg")));
  impl_->editExpressionAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Equal));
  impl_->editExpressionAction->setToolTip(QStringLiteral("Edit Expression"));
  impl_->editExpressionAction->setStatusTip(QStringLiteral("Edit the expression on the focused property"));
  impl_->removeExpressionAction = impl_->expressionMenu->addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.expr_remove"), QStringLiteral("エクスプレッションを削除")));
  impl_->removeExpressionAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_delete.svg")));
  impl_->removeExpressionAction->setToolTip(QStringLiteral("Remove Expression"));
  impl_->removeExpressionAction->setStatusTip(QStringLiteral("Remove the expression from the focused property"));

  impl_->expressionMenu->addSeparator();

  impl_->convertToKeyframesAction = impl_->expressionMenu->addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.expr_to_keyframes"), QStringLiteral("エクスプレッションをキーフレームに変換...")));
  impl_->convertToKeyframesAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_animation.svg")));
  impl_->convertToKeyframesAction->setToolTip(QStringLiteral("Convert Expression to Keyframes"));
  impl_->convertToKeyframesAction->setStatusTip(QStringLiteral("Bake the expression result into keyframes"));
  impl_->bakeLiveToKeyframesAction = impl_->expressionMenu->addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.bake_live"), QStringLiteral("現在PropertyをキーフレームにBake...")));
  impl_->bakeLiveToKeyframesAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_animation.svg")));
  impl_->bakeLiveToKeyframesAction->setToolTip(QStringLiteral("Bake Property to Keyframes"));
  impl_->bakeLiveToKeyframesAction->setStatusTip(QStringLiteral("Bake the current property value into keyframes"));
  impl_->audioReactiveMenu = impl_->expressionMenu->addMenu(
      QStringLiteral("Audio Reactive"));
  impl_->audioReactiveMenu->setIcon(
      menuIcon(QStringLiteral("Studio/animationmenu_tune.svg")));
  impl_->audioReactiveMenu->setAccessibleName(QStringLiteral("Audio Reactive"));
  impl_->audioReactiveMenu->setAccessibleDescription(QStringLiteral("Drive properties from audio analysis"));
  impl_->configureAudioReactiveAction =
      impl_->audioReactiveMenu->addAction(
          QStringLiteral("Configure focused property binding..."));
  impl_->configureAudioReactiveAction->setToolTip(QStringLiteral("Configure Audio Binding"));
  impl_->configureAudioReactiveAction->setStatusTip(QStringLiteral("Configure audio binding for the focused property"));
  impl_->removeAudioReactiveAction =
      impl_->audioReactiveMenu->addAction(
          QStringLiteral("Remove focused property binding"));
  impl_->removeAudioReactiveAction->setToolTip(QStringLiteral("Remove Audio Binding"));
  impl_->removeAudioReactiveAction->setStatusTip(QStringLiteral("Remove audio binding from the focused property"));
  impl_->previewAudioReactiveAction =
      impl_->audioReactiveMenu->addAction(
          QStringLiteral("Preview focused property binding..."));
  impl_->previewAudioReactiveAction->setToolTip(QStringLiteral("Preview Audio Binding"));
  impl_->previewAudioReactiveAction->setStatusTip(QStringLiteral("Preview the audio-driven property motion"));
  impl_->bakeAudioReactiveAction =
      impl_->audioReactiveMenu->addAction(
          QStringLiteral("Bake focused property binding..."));
  impl_->bakeAudioReactiveAction->setToolTip(QStringLiteral("Bake Audio Binding"));
  impl_->bakeAudioReactiveAction->setStatusTip(QStringLiteral("Bake the audio-driven motion into keyframes"));
  impl_->audioReactiveMenu->addSeparator();
  impl_->armAudioReactiveAction =
      impl_->audioReactiveMenu->addAction(
          QStringLiteral("Arm focused binding recording..."));
  impl_->armAudioReactiveAction->setToolTip(QStringLiteral("Arm Audio Recording"));
  impl_->armAudioReactiveAction->setStatusTip(QStringLiteral("Arm recording for the focused audio binding"));
  impl_->commitAudioReactiveAction =
      impl_->audioReactiveMenu->addAction(
          QStringLiteral("Commit Audio Reactive recording"));
  impl_->commitAudioReactiveAction->setToolTip(QStringLiteral("Commit Audio Recording"));
  impl_->commitAudioReactiveAction->setStatusTip(QStringLiteral("Commit the audio reactive recording"));
  impl_->cancelAudioReactiveAction =
      impl_->audioReactiveMenu->addAction(
          QStringLiteral("Cancel Audio Reactive recording"));
  impl_->cancelAudioReactiveAction->setToolTip(QStringLiteral("Cancel Audio Recording"));
  impl_->cancelAudioReactiveAction->setStatusTip(QStringLiteral("Discard the audio reactive recording"));
  addSeparator();

  impl_->presetMenu = addMenu(TranslationManager::instance().tr(QStringLiteral("menu.animation.preset_menu"), QStringLiteral("アニメーションプリセット(&P)")));
  impl_->presetMenu->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_bookmarks.svg")));
  impl_->presetMenu->setAccessibleName(QStringLiteral("Animation Presets"));
  impl_->presetMenu->setAccessibleDescription(QStringLiteral("Save and apply animation presets"));

  impl_->presetLibraryMenu = impl_->presetMenu->addMenu(TranslationManager::instance().tr(QStringLiteral("menu.animation.preset_library"), QStringLiteral("プリセットライブラリ")));
  impl_->presetLibraryMenu->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_bookmarks.svg")));
  impl_->presetLibraryMenu->setAccessibleName(QStringLiteral("Preset Library"));
  impl_->presetLibraryMenu->setAccessibleDescription(QStringLiteral("Apply a built-in keyframe pattern preset"));
  const auto addPresetLibraryAction = [this](const QString& label,
                                             const ArtifactCore::KeyframePatternPreset preset) {
    QAction* action = impl_->presetLibraryMenu->addAction(label);
    action->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_bookmarks.svg")));
    action->setToolTip(QStringLiteral("Apply preset: %1").arg(label));
    action->setStatusTip(QStringLiteral("Apply the keyframe pattern preset"));
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

  impl_->saveAnimationPresetAction = impl_->presetMenu->addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.preset_save"), QStringLiteral("アニメーションプリセットを保存...")));
  impl_->saveAnimationPresetAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_save.svg")));
  impl_->saveAnimationPresetAction->setToolTip(QStringLiteral("Save Animation Preset"));
  impl_->saveAnimationPresetAction->setStatusTip(QStringLiteral("Save the selected animation as a preset"));
  impl_->loadAnimationPresetAction = impl_->presetMenu->addAction(TranslationManager::instance().tr(QStringLiteral("menu.animation.preset_apply"), QStringLiteral("アニメーションプリセットを適用...")));
  impl_->loadAnimationPresetAction->setIcon(menuIcon(QStringLiteral("Studio/animationmenu_folder_open.svg")));
  impl_->loadAnimationPresetAction->setToolTip(QStringLiteral("Apply Animation Preset"));
  impl_->loadAnimationPresetAction->setStatusTip(QStringLiteral("Apply a saved animation preset"));
  impl_->presetMenu->addSeparator();

  auto dispatchAction = [this](QAction* action) {
   if (!action) {
    return;
   }
   if (action == impl_->addKeyframeAction) {
    ArtifactCore::globalEventBus().publish(
        TimelineKeyframeEditCommandRequestedEvent{
            TimelineKeyframeEditCommandKind::Add});
    return;
   }
   QWidget* root = impl_ && impl_->menu_ ? impl_->menu_->window() : nullptr;
   if (action == impl_->setPositionKeyframesAction) {
    addTransformKeyframesAtPlayhead(
        root, {QStringLiteral("transform.position.x"),
               QStringLiteral("transform.position.y")});
    return;
   }
   if (action == impl_->setRotationKeyframesAction) {
    addTransformKeyframesAtPlayhead(
        root, {QStringLiteral("transform.rotation.z")});
    return;
   }
   if (action == impl_->setScaleKeyframesAction) {
    addTransformKeyframesAtPlayhead(
        root, {QStringLiteral("transform.scale.x"),
               QStringLiteral("transform.scale.y")});
    return;
   }
   if (action == impl_->setAllTransformKeyframesAction) {
    addTransformKeyframesAtPlayhead(
        root, {QStringLiteral("transform.position.x"),
               QStringLiteral("transform.position.y"),
               QStringLiteral("transform.rotation.z"),
               QStringLiteral("transform.scale.x"),
               QStringLiteral("transform.scale.y")});
    return;
   }
   if (action == impl_->nudgeBackwardAction ||
       action == impl_->nudgeForwardAction) {
    if (auto* timeline = activeTimelineWidget(root)) {
      timeline->nudgeSelectedKeyframes(
          action == impl_->nudgeForwardAction ? 1 : -1);
    }
    return;
   }
   if (action == impl_->moveLayerStartAction ||
       action == impl_->moveLayerEndAction) {
    if (auto* timeline = activeTimelineWidget(root)) {
      if (action == impl_->moveLayerStartAction)
        timeline->moveSelectedLayerStartToCurrentFrame();
      else
        timeline->moveSelectedLayerEndToCurrentFrame();
    }
    return;
   }
   if (action == impl_->trimLayerInAction ||
       action == impl_->trimLayerOutAction) {
    if (auto* timeline = activeTimelineWidget(root)) {
      if (action == impl_->trimLayerInAction)
        timeline->trimSelectedLayerInToCurrentFrame();
      else
        timeline->trimSelectedLayerOutToCurrentFrame();
    }
    return;
   }
   if (action == impl_->deleteLayerAnimationAction) {
    if (auto* timeline = activeTimelineWidget(root))
      timeline->deleteSelectedLayerAnimation();
    return;
   }
   if (action == impl_->distributeKeyframesAction) {
    if (auto* timeline = activeTimelineWidget(root))
      timeline->distributeSelectedKeyframesEvenly();
    return;
   }
   if (action == impl_->removeKeyframeAction) {
    ArtifactCore::globalEventBus().publish(
        TimelineKeyframeEditCommandRequestedEvent{
            TimelineKeyframeEditCommandKind::Remove});
    return;
   }
   if (action == impl_->selectAllKeyframesAction) {
    ArtifactCore::globalEventBus().publish(
        TimelineKeyframeEditCommandRequestedEvent{
            TimelineKeyframeEditCommandKind::SelectAll});
    return;
   }
   if (action == impl_->reverseSelectedKeyframesAction) {
    ArtifactCore::globalEventBus().publish(
        TimelineKeyframeEditCommandRequestedEvent{
            TimelineKeyframeEditCommandKind::ReverseSelected});
    return;
   }
   if (action == impl_->reverseAllKeyframesInLayerAction) {
    ArtifactCore::globalEventBus().publish(
        TimelineKeyframeEditCommandRequestedEvent{
            TimelineKeyframeEditCommandKind::ReverseCurrentLayer});
    return;
   }
   if (action == impl_->reverseAllKeyframesInSelectedLayersAction) {
    ArtifactCore::globalEventBus().publish(
        TimelineKeyframeEditCommandRequestedEvent{
            TimelineKeyframeEditCommandKind::ReverseSelectedLayers});
    return;
   }
   if (action == impl_->reverseAllKeyframesInCompositionAction) {
    ArtifactCore::globalEventBus().publish(
        TimelineKeyframeEditCommandRequestedEvent{
            TimelineKeyframeEditCommandKind::ReverseComposition});
    return;
   }
   if (action == impl_->copyKeyframesAction) {
    ArtifactCore::globalEventBus().publish(
        TimelineKeyframeEditCommandRequestedEvent{
            TimelineKeyframeEditCommandKind::Copy});
    return;
   }
   if (action == impl_->pasteKeyframesAction) {
    ArtifactCore::globalEventBus().publish(
        TimelineKeyframeEditCommandRequestedEvent{
            TimelineKeyframeEditCommandKind::Paste});
    return;
   }
   if (action == impl_->linearInterpolationAction) {
    ArtifactCore::globalEventBus().publish(
        TimelineInterpolationCommandRequestedEvent{
            ArtifactCore::InterpolationType::Linear});
    return;
   }
   if (action == impl_->easeInAction) {
    ArtifactCore::globalEventBus().publish(
        TimelineInterpolationCommandRequestedEvent{
            ArtifactCore::InterpolationType::EaseIn});
    return;
   }
   if (action == impl_->easeOutAction) {
    ArtifactCore::globalEventBus().publish(
        TimelineInterpolationCommandRequestedEvent{
            ArtifactCore::InterpolationType::EaseOut});
    return;
   }
   if (action == impl_->easeInOutAction) {
    ArtifactCore::globalEventBus().publish(
        TimelineInterpolationCommandRequestedEvent{
            ArtifactCore::InterpolationType::EaseInOut});
    return;
   }
   if (action == impl_->holdInterpolationAction) {
    ArtifactCore::globalEventBus().publish(
        TimelineInterpolationCommandRequestedEvent{
            ArtifactCore::InterpolationType::Constant});
    return;
   }
   if (action == impl_->bezierInterpolationAction) {
    ArtifactCore::globalEventBus().publish(
        TimelineInterpolationCommandRequestedEvent{
            ArtifactCore::InterpolationType::Bezier});
    return;
   }
   if (action == impl_->catmullRomInterpolationAction) {
    ArtifactCore::globalEventBus().publish(
        TimelineInterpolationCommandRequestedEvent{
            ArtifactCore::InterpolationType::CatmullRom});
    return;
   }
   if (action == impl_->hermiteInterpolationAction) {
    ArtifactCore::globalEventBus().publish(
        TimelineInterpolationCommandRequestedEvent{
            ArtifactCore::InterpolationType::Hermite});
    return;
   }
   if (action == impl_->showGraphEditorAction) {
    ArtifactCore::globalEventBus().publish(
        TimelineGraphCommandRequestedEvent{
            TimelineGraphCommandKind::ShowEditor});
    return;
   }
   if (action == impl_->toggleValueGraphAction) {
    ArtifactCore::globalEventBus().publish(
        TimelineGraphCommandRequestedEvent{
            TimelineGraphCommandKind::ShowValue});
    return;
   }
   if (action == impl_->toggleVelocityGraphAction) {
    ArtifactCore::globalEventBus().publish(
        TimelineGraphCommandRequestedEvent{
            TimelineGraphCommandKind::ShowSpeed});
    return;
   }
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
   if (action == impl_->goToNextKeyframeAction) {
    ArtifactCore::globalEventBus().publish(
        TimelineKeyframeNavigationRequestedEvent{
            TimelineKeyframeNavigationKind::Next});
    return;
   }
   if (action == impl_->goToPreviousKeyframeAction) {
    ArtifactCore::globalEventBus().publish(
        TimelineKeyframeNavigationRequestedEvent{
            TimelineKeyframeNavigationKind::Previous});
    return;
   }
   if (action == impl_->goToFirstKeyframeAction) {
    ArtifactCore::globalEventBus().publish(
        TimelineKeyframeNavigationRequestedEvent{
            TimelineKeyframeNavigationKind::First});
    return;
   }
   if (action == impl_->goToLastKeyframeAction) {
    ArtifactCore::globalEventBus().publish(
        TimelineKeyframeNavigationRequestedEvent{
            TimelineKeyframeNavigationKind::Last});
    return;
   }
   if (action == impl_->enableTimeRemapAction) {
    ArtifactCore::globalEventBus().publish(
        TimelineTimeRemapCommandRequestedEvent{
            TimelineTimeRemapCommandKind::Enable});
    return;
   }
   if (action == impl_->freezeFrameAction) {
    ArtifactCore::globalEventBus().publish(
        TimelineTimeRemapCommandRequestedEvent{
            TimelineTimeRemapCommandKind::Freeze});
    return;
   }
   if (action == impl_->timeReverseAction) {
    ArtifactCore::globalEventBus().publish(
        TimelineTimeRemapCommandRequestedEvent{
            TimelineTimeRemapCommandKind::Reverse});
    return;
   }
   if (action == impl_->slowHalfAction) {
    ArtifactCore::globalEventBus().publish(
        TimelineTimeRemapCommandRequestedEvent{TimelineTimeRemapCommandKind::SlowHalf});
    return;
   }
   if (action == impl_->slowQuarterAction) {
    ArtifactCore::globalEventBus().publish(
        TimelineTimeRemapCommandRequestedEvent{TimelineTimeRemapCommandKind::SlowQuarter});
    return;
   }
   if (action == impl_->stopMotion12FpsAction || action == impl_->stopMotion8FpsAction ||
       action == impl_->stopMotion4FpsAction) {
    const auto kind = action == impl_->stopMotion12FpsAction
        ? TimelineTimeRemapCommandKind::StopMotion12Fps
        : action == impl_->stopMotion8FpsAction
            ? TimelineTimeRemapCommandKind::StopMotion8Fps
            : TimelineTimeRemapCommandKind::StopMotion4Fps;
    ArtifactCore::globalEventBus().publish(TimelineTimeRemapCommandRequestedEvent{kind});
    return;
   }
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
    impl_->eventBusSubscriptions_.push_back(
        ArtifactCore::globalEventBus().subscribe<ArtifactCore::AppSettingsChangedEvent>(
            [this](const ArtifactCore::AppSettingsChangedEvent&) {
              if (!impl_ || !impl_->toggleValueGraphAction ||
                  !impl_->toggleVelocityGraphAction) {
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
            }));
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
