#include <algorithm>
#include <cstdint>
#include <utility>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QVariant>
#include <QString>

import Artifact.Layer.Abstract;

import Audio.Modulation.Router;
import Container.NamedVector;

namespace Artifact {
using namespace ArtifactCore;

QJsonObject serializeLayerModulationRouter(
    const Audio::Modulation::ModulationRouter& router) {
  QJsonObject result;
  QJsonArray sources;
  for (const auto& source : router.sourceDefinitions()) {
    QJsonObject item;
    item[QStringLiteral("id")] = static_cast<double>(source.id);
    item[QStringLiteral("type")] = static_cast<int>(source.type);
    item[QStringLiteral("waveform")] = static_cast<int>(source.waveform);
    item[QStringLiteral("frequency")] = source.frequency;
    item[QStringLiteral("phaseOffset")] = source.phaseOffset;
    item[QStringLiteral("pulseWidth")] = source.pulseWidth;
    item[QStringLiteral("attack")] = source.attack;
    item[QStringLiteral("decay")] = source.decay;
    item[QStringLiteral("sustain")] = source.sustain;
    item[QStringLiteral("release")] = source.release;
    item[QStringLiteral("rate")] = source.rate;
    item[QStringLiteral("smoothing")] = source.smoothing;
    item[QStringLiteral("seed")] = static_cast<double>(source.seed);
    item[QStringLiteral("macroValue")] = source.macroValue;
    item[QStringLiteral("constantValue")] = source.constantValue;
    item[QStringLiteral("stepCount")] = static_cast<double>(source.stepCount);
    item[QStringLiteral("unipolar")] = source.unipolar;
    sources.append(item);
  }
  QJsonArray assignments;
  for (const auto& assignment : router.assignments()) {
    if (assignment.sourceId == 0 || assignment.targetPath.empty()) continue;
    QJsonObject item;
    item[QStringLiteral("sourceId")] = static_cast<double>(assignment.sourceId);
    item[QStringLiteral("targetPath")] =
        QString::fromStdString(assignment.targetPath);
    item[QStringLiteral("depth")] = assignment.depth;
    item[QStringLiteral("enabled")] = assignment.enabled;
    item[QStringLiteral("mode")] = static_cast<int>(assignment.mode);
    assignments.append(item);
  }
  if (!sources.isEmpty()) result[QStringLiteral("sources")] = sources;
  if (!assignments.isEmpty()) result[QStringLiteral("assignments")] = assignments;
  return result;
}

void restoreLayerModulationRouter(
    const QJsonObject& object, Audio::Modulation::ModulationRouter& router) {
  NamedVector<Audio::Modulation::ModulationSourceDefinition> sources{
      ContainerName{"Layer.ModulationSources"}};
  for (const auto& value : object.value(QStringLiteral("sources")).toArray()) {
    if (!value.isObject()) continue;
    const QJsonObject item = value.toObject();
    const int type = item.value(QStringLiteral("type")).toInt(-1);
    if (type < 0 || type > 6) continue;
    Audio::Modulation::ModulationSourceDefinition source;
    source.id = static_cast<std::uint32_t>(
        item.value(QStringLiteral("id")).toVariant().toUInt());
    source.type = static_cast<Audio::Modulation::ModulatorSourceType>(type);
    source.waveform = static_cast<Audio::Modulation::LfoWaveform>(
        std::clamp(item.value(QStringLiteral("waveform")).toInt(0), 0, 4));
    source.frequency = static_cast<float>(
        item.value(QStringLiteral("frequency")).toDouble(1.0));
    source.phaseOffset = static_cast<float>(
        item.value(QStringLiteral("phaseOffset")).toDouble(0.0));
    source.pulseWidth = static_cast<float>(
        item.value(QStringLiteral("pulseWidth")).toDouble(0.5));
    source.attack = static_cast<float>(
        item.value(QStringLiteral("attack")).toDouble(0.01));
    source.decay = static_cast<float>(
        item.value(QStringLiteral("decay")).toDouble(0.1));
    source.sustain = static_cast<float>(
        item.value(QStringLiteral("sustain")).toDouble(0.7));
    source.release = static_cast<float>(
        item.value(QStringLiteral("release")).toDouble(0.2));
    source.rate = static_cast<float>(
        item.value(QStringLiteral("rate")).toDouble(1.0));
    source.smoothing = static_cast<float>(
        item.value(QStringLiteral("smoothing")).toDouble(0.005));
    source.seed = static_cast<std::uint32_t>(
        item.value(QStringLiteral("seed")).toVariant().toUInt());
    source.macroValue = static_cast<float>(
        item.value(QStringLiteral("macroValue")).toDouble(0.0));
    source.constantValue = static_cast<float>(
        item.value(QStringLiteral("constantValue")).toDouble(0.0));
    source.stepCount = static_cast<std::uint32_t>(
        std::clamp(item.value(QStringLiteral("stepCount")).toVariant().toUInt(), 1u, 32u));
    source.unipolar = item.value(QStringLiteral("unipolar")).toBool(false);
    sources.add(std::move(source));
  }
  router.clearAssignments();
  router.restoreSources(sources.toStdVector());
  for (const auto& value : object.value(QStringLiteral("assignments")).toArray()) {
    if (!value.isObject()) continue;
    const QJsonObject item = value.toObject();
    const auto sourceId = static_cast<std::uint32_t>(
        item.value(QStringLiteral("sourceId")).toVariant().toUInt());
    const QString targetPath = item.value(
        QStringLiteral("targetPath")).toString().trimmed();
    const int mode = item.value(QStringLiteral("mode")).toInt(0);
    if (sourceId == 0 || targetPath.isEmpty() || mode < 0 || mode > 1) continue;
    auto assignment = Audio::Modulation::ModulationAssignment::forPropertyPath(
        sourceId, targetPath.toStdString(), static_cast<float>(
            item.value(QStringLiteral("depth")).toDouble(1.0)),
        static_cast<Audio::Modulation::ModulationMixMode>(mode));
    assignment.enabled = item.value(QStringLiteral("enabled")).toBool(true);
    router.addAssignment(assignment);
  }
}

} // namespace Artifact
