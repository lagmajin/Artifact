module;
#include <string>
#include <vector>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <limits>
#include <QString>
#include <QVector>
#include <QChar>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
module Artifact.PythonAPI;

import Script.Python.Engine;
import Artifact.Application.Manager;
import Artifact.Service.Project;
import Artifact.Service.Playback;
import Artifact.Project.Manager;
import Artifact.Layer.InitParams;
import Artifact.Service.Effect;
import Artifact.Render.Queue.Service;
import Artifact.Layers.Selection.Manager;
import Artifact.Composition.Abstract;
import Artifact.Project.Settings;
import Property;
import Property.Group;
import Property.Abstract;
import Frame.Position;
import Frame.Range;

namespace Artifact {

namespace {
ArtifactPlaybackService* playbackService()
{
    return ArtifactPlaybackService::instance();
}

bool toBool(const std::string& value)
{
    const QString v = QString::fromStdString(value).trimmed().toLower();
    return v == QStringLiteral("1") || v == QStringLiteral("true") || v == QStringLiteral("yes") ||
           v == QStringLiteral("on");
}

int toInt(const std::string& value, int fallback)
{
    bool ok = false;
    const int parsed = QString::fromStdString(value).trimmed().toInt(&ok);
    return ok ? parsed : fallback;
}

QString toQString(const std::string& value)
{
    return QString::fromStdString(value);
}
}

void ArtifactPythonAPI::registerAll() {
    registerProjectAPI();
    registerLayerAPI();
    registerEffectAPI();
    registerRenderAPI();
    registerUtilityAPI();
}

void ArtifactPythonAPI::registerProjectAPI() {
    auto& py = ArtifactCore::PythonEngine::instance();

    py.registerFunction("_native_project_new", [](const std::vector<std::string>& args) -> std::string {
        auto* service = ArtifactProjectService::instance();
        if (!service) return "ERROR: project service unavailable";
        ArtifactProjectSettings settings;
        settings.setProjectName(args.empty() || toQString(args[0]).trimmed().isEmpty()
                                    ? QStringLiteral("Untitled")
                                    : toQString(args[0]).trimmed());
        service->createProject(settings);
        return service->hasProject() ? "OK" : "ERROR: project creation failed";
    });

    py.registerFunction("_native_project_info", [](const std::vector<std::string>&) -> std::string {
        auto* service = ArtifactProjectService::instance();
        if (!service || !service->hasProject()) return "{}";
        QJsonObject info;
        info.insert(QStringLiteral("name"), service->projectName().toQString());
        if (const auto comp = service->currentComposition().lock()) {
            const auto size = comp->settings().compositionSize();
            info.insert(QStringLiteral("width"), size.width());
            info.insert(QStringLiteral("height"), size.height());
            info.insert(QStringLiteral("fps"), comp->frameRate().framerate());
            info.insert(QStringLiteral("current_comp_id"), comp->id().toString());
        }
        return QJsonDocument(info).toJson(QJsonDocument::Compact).toStdString();
    });

    py.registerFunction("_native_current_comp_id", [](const std::vector<std::string>&) -> std::string {
        auto* service = ArtifactProjectService::instance();
        const auto comp = service ? service->currentComposition().lock()
                                  : ArtifactCompositionPtr{};
        return comp ? comp->id().toString().toStdString() : std::string();
    });

    py.registerFunction("_native_project_open", [](const std::vector<std::string>& args) -> std::string {
        if (args.empty() || toQString(args[0]).trimmed().isEmpty()) {
            return "ERROR: project path is empty";
        }
        auto& manager = ArtifactProjectManager::getInstance();
        return manager.loadFromFile(toQString(args[0]).trimmed())
                   ? "OK" : "ERROR: project load failed";
    });

    py.registerFunction("_native_project_save", [](const std::vector<std::string>& args) -> std::string {
        auto* service = ArtifactProjectService::instance();
        if (!service || !service->hasProject()) return "ERROR: no project";
        auto& manager = ArtifactProjectManager::getInstance();
        QString path = args.empty() ? manager.currentProjectPath() : toQString(args[0]).trimmed();
        if (path.isEmpty()) return "ERROR: project path is empty";
        const auto result = manager.saveToFile(path);
        return result.success ? "OK" : ("ERROR: " + result.errorMessage).toStdString();
    });

    std::string code = R"(
import artifact
import json

# Project API
def _project_new(name="Untitled"):
    return artifact._native_project_new(name) == "OK"

def _project_open(path):
    """Open a project file"""
    return artifact._native_project_open(path) == "OK"

def _project_save(path=None):
    """Save current project"""
    args = [] if path is None else [path]
    return artifact._native_project_save(*args) == "OK"

def _project_info():
    """Get current project information"""
    return json.loads(artifact._native_project_info())

artifact.project_new = _project_new
artifact.project_open = _project_open
artifact.project_save = _project_save
artifact.project_info = _project_info
artifact.project_current_comp_id = lambda: artifact._native_current_comp_id()
)";
    py.execute(code);
}

void ArtifactPythonAPI::registerLayerAPI() {
    auto& py = ArtifactCore::PythonEngine::instance();

    py.registerFunction("_native_get_layers", [](const std::vector<std::string>&) -> std::string {
        auto* service = ArtifactProjectService::instance();
        const auto comp = service ? service->currentComposition().lock() : ArtifactCompositionPtr{};
        if (!comp) return "[]";

        QJsonArray layers;
        for (const auto& layer : comp->allLayer()) {
            if (!layer) continue;
            QJsonObject item;
            item.insert(QStringLiteral("id"), layer->id().toString());
            item.insert(QStringLiteral("name"), layer->layerName());
            item.insert(QStringLiteral("visible"), layer->isVisible());
            item.insert(QStringLiteral("locked"), layer->isLocked());
            layers.append(item);
        }
        return QJsonDocument(layers).toJson(QJsonDocument::Compact).toStdString();
    });

    py.registerFunction("_native_get_selected_layers", [](const std::vector<std::string>&) -> std::string {
        auto* app = ArtifactApplicationManager::instance();
        auto* selection = app ? app->layerSelectionManager() : nullptr;
        if (!selection) return "[]";

        QJsonArray layers;
        for (const auto& layer : selection->selectedLayersInOrder()) {
            if (!layer) continue;
            QJsonObject item;
            item.insert(QStringLiteral("id"), layer->id().toString());
            item.insert(QStringLiteral("name"), layer->layerName());
            layers.append(item);
        }
        return QJsonDocument(layers).toJson(QJsonDocument::Compact).toStdString();
    });

    py.registerFunction("_native_create_layer", [](const std::vector<std::string>& args) -> std::string {
        auto* service = ArtifactProjectService::instance();
        if (!service || !service->hasProject()) return "ERROR: no project";
        const QString name = args.empty() || toQString(args[0]).trimmed().isEmpty()
                                 ? QStringLiteral("Layer") : toQString(args[0]).trimmed();
        const QString type = args.size() > 1 ? toQString(args[1]).trimmed().toLower()
                                             : QStringLiteral("solid");
        if (type == QStringLiteral("solid")) {
            service->addLayerToCurrentComposition(ArtifactSolidLayerInitParams(name));
        } else if (type == QStringLiteral("text")) {
            service->addLayerToCurrentComposition(ArtifactTextLayerInitParams(name));
        } else if (type == QStringLiteral("null")) {
            service->addLayerToCurrentComposition(ArtifactNullLayerInitParams(name));
        } else {
            return "ERROR: unsupported layer type";
        }
        return "OK";
    });

    py.registerFunction("_native_delete_layer", [](const std::vector<std::string>& args) -> std::string {
        auto* service = ArtifactProjectService::instance();
        const auto comp = service ? service->currentComposition().lock() : ArtifactCompositionPtr{};
        if (!service || !comp || args.empty()) return "ERROR: layer not found";
        for (const auto& layer : comp->allLayer()) {
            if (layer && layer->layerName().compare(toQString(args[0]), Qt::CaseInsensitive) == 0) {
                return service->removeLayerFromComposition(comp->id(), layer->id()) ? "OK"
                                                                                       : "ERROR: delete failed";
            }
        }
        return "ERROR: layer not found";
    });

    py.registerFunction("_native_duplicate_layer", [](const std::vector<std::string>& args) -> std::string {
        auto* service = ArtifactProjectService::instance();
        const auto comp = service ? service->currentComposition().lock() : ArtifactCompositionPtr{};
        if (!service || !comp || args.empty()) return "ERROR: layer not found";
        for (const auto& layer : comp->allLayer()) {
            if (layer && layer->layerName().compare(toQString(args[0]), Qt::CaseInsensitive) == 0) {
                if (!service->duplicateLayerInCurrentComposition(layer->id())) {
                    return "ERROR: duplicate failed";
                }
                if (args.size() > 1 && !toQString(args[1]).trimmed().isEmpty()) {
                    auto* app = ArtifactApplicationManager::instance();
                    auto* selection = app ? app->layerSelectionManager() : nullptr;
                    const auto duplicate = selection ? selection->currentLayer() : ArtifactAbstractLayerPtr{};
                    if (!duplicate || !service->renameLayerInCurrentComposition(
                            duplicate->id(), toQString(args[1]).trimmed())) {
                        return "ERROR: duplicate rename failed";
                    }
                }
                return "OK";
            }
        }
        return "ERROR: layer not found";
    });

    py.registerFunction("_native_move_layer", [](const std::vector<std::string>& args) -> std::string {
        auto* service = ArtifactProjectService::instance();
        const auto comp = service ? service->currentComposition().lock() : ArtifactCompositionPtr{};
        if (!service || !comp || args.size() < 2) return "ERROR: invalid arguments";
        const int index = std::max(0, toInt(args[1], -1));
        if (index < 0) return "ERROR: invalid index";
        for (const auto& layer : comp->allLayer()) {
            if (layer && layer->layerName().compare(toQString(args[0]), Qt::CaseInsensitive) == 0) {
                return service->moveLayerInCurrentComposition(layer->id(), index) ? "OK"
                                                                                    : "ERROR: move failed";
            }
        }
        return "ERROR: layer not found";
    });

    py.registerFunction("_native_set_layer_property", [](const std::vector<std::string>& args) -> std::string {
        auto* service = ArtifactProjectService::instance();
        const auto comp = service ? service->currentComposition().lock() : ArtifactCompositionPtr{};
        if (!service || !comp || args.size() < 3) return "ERROR: invalid arguments";
        for (const auto& layer : comp->allLayer()) {
            if (!layer || layer->layerName().compare(toQString(args[0]), Qt::CaseInsensitive) != 0) continue;
            const QString property = toQString(args[1]).trimmed().toLower();
            const bool value = toBool(args[2]);
            bool ok = false;
            if (property == QStringLiteral("visible")) ok = service->setLayerVisibleInCurrentComposition(layer->id(), value);
            else if (property == QStringLiteral("locked")) ok = service->setLayerLockedInCurrentComposition(layer->id(), value);
            else if (property == QStringLiteral("solo")) ok = service->setLayerSoloInCurrentComposition(layer->id(), value);
            else if (property == QStringLiteral("shy")) ok = service->setLayerShyInCurrentComposition(layer->id(), value);
            else if (property == QStringLiteral("name")) ok = service->renameLayerInCurrentComposition(layer->id(), toQString(args[2]));
            else return "ERROR: unsupported layer property";
            return ok ? "OK" : "ERROR: property update failed";
        }
        return "ERROR: layer not found";
    });

    py.registerFunction("_native_get_layer_property", [](const std::vector<std::string>& args) -> std::string {
        auto* service = ArtifactProjectService::instance();
        const auto comp = service ? service->currentComposition().lock() : ArtifactCompositionPtr{};
        if (!service || !comp || args.size() < 2) return "null";
        for (const auto& layer : comp->allLayer()) {
            if (!layer || layer->layerName().compare(toQString(args[0]), Qt::CaseInsensitive) != 0) continue;
            const QString property = toQString(args[1]).trimmed().toLower();
            if (property == QStringLiteral("visible")) return layer->isVisible() ? "true" : "false";
            if (property == QStringLiteral("locked")) return layer->isLocked() ? "true" : "false";
            if (property == QStringLiteral("solo")) return layer->isSolo() ? "true" : "false";
            if (property == QStringLiteral("shy")) return layer->isShy() ? "true" : "false";
            if (property == QStringLiteral("name")) return QJsonDocument(QJsonObject{{QStringLiteral("value"), layer->layerName()}}).toJson(QJsonDocument::Compact).toStdString();
            return "null";
        }
        return "null";
    });

    py.registerFunction("rename_selected_layers", [](const std::vector<std::string>& args) -> std::string {
        const QString prefix = args.size() > 0 ? toQString(args[0]) : QString();
        const QString baseName = args.size() > 1 ? toQString(args[1]) : QStringLiteral("Layer");
        const QString suffix = args.size() > 2 ? toQString(args[2]) : QString();
        const int startIndex = args.size() > 3 ? std::max(1, toInt(args[3], 1)) : 1;
        const int padding = args.size() > 4 ? std::max(0, toInt(args[4], 0)) : 0;
        const bool renameSelectedOnly = args.size() > 5 ? toBool(args[5]) : true;

        auto* service = ArtifactProjectService::instance();
        auto* selection = ArtifactApplicationManager::instance()
                              ? ArtifactApplicationManager::instance()->layerSelectionManager()
                              : nullptr;
        auto comp = service ? service->currentComposition().lock() : ArtifactCompositionPtr{};
        if (!service || !selection || !comp) {
            return "ERROR: no active composition or selection";
        }

        QVector<ArtifactAbstractLayerPtr> ordered;
        const auto selected = selection->selectedLayers();
        if (selected.isEmpty()) {
            if (auto current = selection->currentLayer()) {
                ordered.push_back(current);
            }
        } else {
            ordered.reserve(selected.size());
            const auto allLayers = comp->allLayer();
            for (const auto& layer : allLayers) {
                if (layer && selected.contains(layer)) {
                    ordered.push_back(layer);
                }
            }
        }

        if (ordered.isEmpty()) {
            return "ERROR: no selected layers";
        }

        int index = startIndex;
        for (const auto& layer : ordered) {
            if (!layer) {
                continue;
            }
            if (renameSelectedOnly && layer->isLocked()) {
                continue;
            }
            const QString number = padding > 0
                                       ? QStringLiteral("%1").arg(index, padding, 10, QChar('0'))
                                       : QString::number(index);
            const QString newName =
                QStringLiteral("%1%2%3%4").arg(prefix, baseName, number, suffix);
            if (!service->renameLayerInCurrentComposition(layer->id(), newName)) {
                return std::string("ERROR: failed to rename layer ") +
                       layer->layerName().toStdString();
            }
            ++index;
        }

        return "OK";
    });

    py.registerFunction("clean_selected_layers", [](const std::vector<std::string>& args) -> std::string {
        const bool clearParent = args.size() > 0 ? toBool(args[0]) : true;
        const bool clearEffects = args.size() > 1 ? toBool(args[1]) : true;
        const bool clearMarkers = args.size() > 2 ? toBool(args[2]) : true;
        const bool clearExpressions = args.size() > 3 ? toBool(args[3]) : true;
        const bool clearLabels = args.size() > 4 ? toBool(args[4]) : true;
        const bool preserveLockedLayers = args.size() > 5 ? toBool(args[5]) : true;

        auto* service = ArtifactProjectService::instance();
        auto* selection = ArtifactApplicationManager::instance()
                              ? ArtifactApplicationManager::instance()->layerSelectionManager()
                              : nullptr;
        auto comp = service ? service->currentComposition().lock() : ArtifactCompositionPtr{};
        if (!service || !selection || !comp) {
            return "ERROR: no active composition or selection";
        }

        const auto selected = selection->selectedLayers();
        QVector<ArtifactAbstractLayerPtr> ordered;
        if (selected.isEmpty()) {
            if (auto current = selection->currentLayer()) {
                ordered.push_back(current);
            }
        } else {
            const auto allLayers = comp->allLayer();
            ordered.reserve(selected.size());
            for (const auto& layer : allLayers) {
                if (layer && selected.contains(layer)) {
                    ordered.push_back(layer);
                }
            }
        }

        if (ordered.isEmpty()) {
            return "ERROR: no selected layers";
        }

        int clearedCount = 0;
        bool markersCleared = false;
        for (const auto& layer : ordered) {
            if (!layer) {
                continue;
            }
            if (preserveLockedLayers && layer->isLocked()) {
                continue;
            }

            bool changed = false;
            if (clearParent && layer->hasParent()) {
                changed |= service->clearLayerParentInCurrentComposition(layer->id());
            }

            if (clearEffects) {
                const auto effects = layer->getEffects();
                for (const auto& effect : effects) {
                    if (!effect) {
                        continue;
                    }
                    changed |= service->removeEffectFromLayerInCurrentComposition(
                        layer->id(), effect->effectID().toQString());
                }
            }

            if (clearMarkers && !markersCleared) {
                if (auto* playback = playbackService()) {
                    playback->clearAllMarkers();
                }
                markersCleared = true;
                changed = true;
            }

            if (clearExpressions) {
                for (const auto& group : layer->getLayerPropertyGroups()) {
                    for (const auto& property : group.allProperties()) {
                        if (property && property->hasExpression()) {
                            property->setExpression(QString{});
                            changed = true;
                        }
                    }
                }
                if (layer->hasScriptBinding()) {
                    layer->clearScriptBinding();
                    changed = true;
                }
            }

            if (clearLabels && layer->labelColorIndex() != 0) {
                layer->setLabelColorIndex(0);
                changed = true;
            }

            if (changed) {
                ++clearedCount;
            }
        }

        return std::string("OK: cleaned ") + std::to_string(clearedCount) + " layer(s)";
    });

    py.registerFunction("trim_comp_to_content", [](const std::vector<std::string>& args) -> std::string {
        const QString trimMode = args.size() > 0 ? toQString(args[0]).trimmed() : QStringLiteral("selectedLayers");
        const int paddingFrames = args.size() > 1 ? std::max(0, toInt(args[1], 0)) : 0;
        const bool setWorkArea = args.size() > 2 ? toBool(args[2]) : true;
        const bool respectLockedLayers = args.size() > 3 ? toBool(args[3]) : true;

        auto* service = ArtifactProjectService::instance();
        auto* selection = ArtifactApplicationManager::instance()
                              ? ArtifactApplicationManager::instance()->layerSelectionManager()
                              : nullptr;
        auto comp = service ? service->currentComposition().lock() : ArtifactCompositionPtr{};
        if (!service || !selection || !comp) {
            return "ERROR: no active composition or selection";
        }

        QVector<ArtifactAbstractLayerPtr> ordered;
        const auto selected = selection->selectedLayers();
        const auto mode = trimMode.isEmpty() ? QStringLiteral("selectedLayers") : trimMode;
        const bool useSelected = mode.compare(QStringLiteral("selectedLayers"), Qt::CaseInsensitive) == 0;
        const bool useVisible = mode.compare(QStringLiteral("visibleLayers"), Qt::CaseInsensitive) == 0;
        if (useSelected) {
            const auto allLayers = comp->allLayer();
            if (!selected.isEmpty()) {
                ordered.reserve(selected.size());
                for (const auto& layer : allLayers) {
                    if (layer && selected.contains(layer)) {
                        ordered.push_back(layer);
                    }
                }
            } else if (auto current = selection->currentLayer()) {
                ordered.push_back(current);
            }
        } else {
            const auto allLayers = comp->allLayer();
            ordered = QVector<ArtifactAbstractLayerPtr>(allLayers.begin(), allLayers.end());
        }

        if (ordered.isEmpty()) {
            return "ERROR: no layers to trim";
        }

        qint64 minIn = std::numeric_limits<qint64>::max();
        qint64 maxOut = std::numeric_limits<qint64>::min();
        for (const auto& layer : ordered) {
            if (!layer) {
                continue;
            }
            if (respectLockedLayers && layer->isLocked()) {
                continue;
            }
            if (useVisible && layer->isShy()) {
                continue;
            }
            minIn = std::min(minIn, layer->inPoint().framePosition());
            maxOut = std::max(maxOut, layer->outPoint().framePosition());
        }

        if (minIn == std::numeric_limits<qint64>::max() ||
            maxOut == std::numeric_limits<qint64>::min()) {
            return "ERROR: failed to resolve layer bounds";
        }

        const qint64 startFrame = std::max<qint64>(0, minIn - paddingFrames);
        const qint64 endFrame = std::max<qint64>(startFrame + 1, maxOut + paddingFrames);
        const FrameRange trimmedRange{FramePosition(startFrame), FramePosition(endFrame)};
        comp->setFrameRange(trimmedRange);
        if (setWorkArea) {
            comp->setWorkAreaRange(trimmedRange);
        }

        return std::string("OK: trimmed to ") + std::to_string(startFrame) + "-" +
               std::to_string(endFrame);
    });

    std::string code = R"(
import artifact
import json

# Layer API
def _get_layers():
    """Get all layer names in current composition"""
    return json.loads(artifact._native_get_layers())

def _get_selected_layers():
    """Get selected layer names"""
    return json.loads(artifact._native_get_selected_layers())

def _create_layer(name, layer_type="solid"):
    """Create a new layer"""
    return artifact._native_create_layer(name, layer_type) == "OK"

def _delete_layer(name):
    """Delete a layer by name"""
    return artifact._native_delete_layer(name) == "OK"

def _duplicate_layer(name, new_name=None):
    """Duplicate a layer"""
    return artifact._native_duplicate_layer(name, "" if new_name is None else new_name) == "OK"

def _set_layer_property(layer_name, property_name, value):
    return artifact._native_set_layer_property(layer_name, property_name, str(value)) == "OK"

def _get_layer_property(layer_name, property_name):
    value = artifact._native_get_layer_property(layer_name, property_name)
    if value in ("true", "false"):
        return value == "true"
    try:
        parsed = json.loads(value)
        return parsed.get("value") if isinstance(parsed, dict) else parsed
    except Exception:
        return None

def _move_layer(layer_name, index):
    return artifact._native_move_layer(layer_name, index) == "OK"

def _clean_selected_layers(clear_parent=True, clear_effects=True, clear_markers=True,
                           clear_expressions=True, clear_labels=True,
                           preserve_locked_layers=True):
    return artifact.clean_selected_layers(clear_parent, clear_effects, clear_markers,
                                          clear_expressions, clear_labels,
                                          preserve_locked_layers)

def _rename_selected_layers(prefix="", base_name="Layer", suffix="", start_index=1, padding=0,
                           rename_selected_only=True):
    return artifact.rename_selected_layers(prefix, base_name, suffix, start_index, padding,
                                           rename_selected_only)

def _trim_comp_to_content(trim_mode="selectedLayers", padding_frames=0, sync_work_area=True,
                          respect_locked_layers=True):
    return artifact.trim_comp_to_content(trim_mode, padding_frames, sync_work_area,
                                         respect_locked_layers)

artifact.get_layers = _get_layers
artifact.get_selected_layers = _get_selected_layers
artifact.create_layer = _create_layer
artifact.delete_layer = _delete_layer
artifact.duplicate_layer = _duplicate_layer
artifact.set_layer_property = _set_layer_property
artifact.get_layer_property = _get_layer_property
artifact.move_layer = _move_layer
artifact.clean_selected_layers = _clean_selected_layers
artifact.rename_selected_layers = _rename_selected_layers
artifact.trim_comp_to_content = _trim_comp_to_content
)";
    py.execute(code);
}

void ArtifactPythonAPI::registerEffectAPI() {
    auto& py = ArtifactCore::PythonEngine::instance();

    py.registerFunction("_native_available_effects", [](const std::vector<std::string>&) -> std::string {
        auto* service = ArtifactEffectService::instance();
        if (!service) return "[]";
        QJsonArray effects;
        for (const auto& info : service->availableEffects()) {
            QJsonObject item;
            item.insert(QStringLiteral("id"), info.id.toString());
            item.insert(QStringLiteral("name"), info.displayName);
            effects.append(item);
        }
        return QJsonDocument(effects).toJson(QJsonDocument::Compact).toStdString();
    });

    py.registerFunction("_native_add_effect", [](const std::vector<std::string>& args) -> std::string {
        auto* project = ArtifactProjectService::instance();
        auto* effects = ArtifactEffectService::instance();
        const auto comp = project ? project->currentComposition().lock() : ArtifactCompositionPtr{};
        if (!project || !effects || !comp || args.size() < 2) return "ERROR: invalid arguments";
        for (const auto& layer : comp->allLayer()) {
            if (layer && layer->layerName().compare(toQString(args[0]), Qt::CaseInsensitive) == 0) {
                const auto result = effects->addEffectToLayer(layer->id(), EffectID(toQString(args[1])));
                return result.success ? "OK" : ("ERROR: " + result.message).toStdString();
            }
        }
        return "ERROR: layer not found";
    });

    py.registerFunction("_native_remove_effect", [](const std::vector<std::string>& args) -> std::string {
        auto* project = ArtifactProjectService::instance();
        auto* effects = ArtifactEffectService::instance();
        const auto comp = project ? project->currentComposition().lock() : ArtifactCompositionPtr{};
        if (!project || !effects || !comp || args.size() < 2) return "ERROR: invalid arguments";
        for (const auto& layer : comp->allLayer()) {
            if (layer && layer->layerName().compare(toQString(args[0]), Qt::CaseInsensitive) == 0) {
                const auto result = effects->removeEffectFromLayer(layer->id(), toQString(args[1]));
                return result.success ? "OK" : ("ERROR: " + result.message).toStdString();
            }
        }
        return "ERROR: layer not found";
    });

    py.registerFunction("_native_get_effects", [](const std::vector<std::string>& args) -> std::string {
        auto* project = ArtifactProjectService::instance();
        const auto comp = project ? project->currentComposition().lock() : ArtifactCompositionPtr{};
        if (!project || !comp || args.empty()) return "[]";
        for (const auto& layer : comp->allLayer()) {
            if (layer && layer->layerName().compare(toQString(args[0]), Qt::CaseInsensitive) == 0) {
                QJsonArray result;
                for (const auto& effect : layer->getEffects()) {
                    if (!effect) continue;
                    QJsonObject item;
                    item.insert(QStringLiteral("id"), effect->effectID().toQString());
                    item.insert(QStringLiteral("name"), effect->displayName().toQString());
                    item.insert(QStringLiteral("enabled"), effect->isEnabled());
                    result.append(item);
                }
                return QJsonDocument(result).toJson(QJsonDocument::Compact).toStdString();
            }
        }
        return "[]";
    });

    py.registerFunction("_native_set_effect_param", [](const std::vector<std::string>& args) -> std::string {
        auto* project = ArtifactProjectService::instance();
        auto* effects = ArtifactEffectService::instance();
        const auto comp = project ? project->currentComposition().lock() : ArtifactCompositionPtr{};
        if (!project || !effects || !comp || args.size() < 4) return "ERROR: invalid arguments";
        for (const auto& layer : comp->allLayer()) {
            if (!layer || layer->layerName().compare(toQString(args[0]), Qt::CaseInsensitive) != 0) continue;
            const auto allEffects = layer->getEffects();
            const int index = toInt(args[1], -1);
            if (index < 0 || index >= static_cast<int>(allEffects.size()) || !allEffects[index]) {
                return "ERROR: effect not found";
            }
            const auto result = effects->setEffectProperty(layer->id(),
                allEffects[index]->effectID().toQString(), toQString(args[2]),
                QVariant(toQString(args[3])));
            return result.success ? "OK" : ("ERROR: " + result.message).toStdString();
        }
        return "ERROR: layer not found";
    });

    py.registerFunction("_native_get_effect_param", [](const std::vector<std::string>& args) -> std::string {
        auto* project = ArtifactProjectService::instance();
        const auto comp = project ? project->currentComposition().lock() : ArtifactCompositionPtr{};
        if (!project || !comp || args.size() < 3) return "null";
        for (const auto& layer : comp->allLayer()) {
            if (!layer || layer->layerName().compare(toQString(args[0]), Qt::CaseInsensitive) != 0) continue;
            const int index = toInt(args[1], -1);
            const auto allEffects = layer->getEffects();
            if (index < 0 || index >= static_cast<int>(allEffects.size()) || !allEffects[index]) return "null";
            const auto property = allEffects[index]->editableProperty(toQString(args[2]));
            if (!property) return "null";
            return QJsonDocument(QJsonObject{{QStringLiteral("value"), QJsonValue::fromVariant(property->getValue())}})
                .toJson(QJsonDocument::Compact).toStdString();
        }
        return "null";
    });

    std::string code = R"(
import artifact
import json

# Effect API
def _add_effect(layer_name, effect_type):
    return artifact._native_add_effect(layer_name, effect_type) == "OK"

def _remove_effect(layer_name, effect_index):
    effects = _get_effects(layer_name)
    if not isinstance(effect_index, int) or effect_index < 0 or effect_index >= len(effects):
        return False
    return artifact._native_remove_effect(layer_name, effects[effect_index]["id"]) == "OK"

def _get_effects(layer_name):
    return json.loads(artifact._native_get_effects(layer_name))

def _set_effect_param(layer_name, effect_index, param_name, value):
    return artifact._native_set_effect_param(layer_name, effect_index, param_name, str(value)) == "OK"

def _get_effect_param(layer_name, effect_index, param_name):
    result = json.loads(artifact._native_get_effect_param(layer_name, effect_index, param_name))
    return result.get("value") if isinstance(result, dict) else None

def _get_available_effects():
    return json.loads(artifact._native_available_effects())

artifact.add_effect = _add_effect
artifact.remove_effect = _remove_effect
artifact.get_effects = _get_effects
artifact.set_effect_param = _set_effect_param
artifact.get_effect_param = _get_effect_param
artifact.get_available_effects = _get_available_effects
)";
    py.execute(code);
}

void ArtifactPythonAPI::registerRenderAPI() {
    auto& py = ArtifactCore::PythonEngine::instance();

    py.registerFunction("_native_render", [](const std::vector<std::string>& args) -> std::string {
        auto* project = ArtifactProjectService::instance();
        auto* queue = ArtifactRenderQueueService::instance();
        const auto comp = project ? project->currentComposition().lock() : ArtifactCompositionPtr{};
        if (!project || !queue || !comp) return "ERROR: no active composition";

        const auto range = comp->frameRange();
        const int start = args.size() > 0 ? std::max(0, toInt(args[0], static_cast<int>(range.start())))
                                         : static_cast<int>(range.start());
        const int end = args.size() > 1 && !toQString(args[1]).trimmed().isEmpty()
                            ? std::max(start + 1, toInt(args[1], static_cast<int>(range.end())))
                            : static_cast<int>(range.end());
        const QString outputPath = args.size() > 2 ? toQString(args[2]).trimmed() : QString();
        const QString format = args.size() > 3 && !toQString(args[3]).trimmed().isEmpty()
                                   ? toQString(args[3]).trimmed() : QStringLiteral("png");

        queue->addRenderQueueForComposition(comp->id(), comp->id().toString());
        const int index = queue->jobCount() - 1;
        if (index < 0) return "ERROR: failed to create render queue";
        queue->setJobFrameRangeAt(index, start, end);
        if (!outputPath.isEmpty()) queue->setJobOutputPathAt(index, outputPath);
        const QSize compositionSize = comp->settings().compositionSize();
        queue->setJobOutputSettingsAt(index, format, QString(),
                                      std::max(16, compositionSize.width()),
                                      std::max(16, compositionSize.height()),
                                      comp->frameRate().framerate(), 0);
        queue->startRenderQueueAt(index);
        return "OK";
    });

    py.registerFunction("_native_play", [](const std::vector<std::string>&) -> std::string {
        auto* playback = playbackService();
        if (!playback) return "ERROR: playback service unavailable";
        playback->play();
        return "OK";
    });
    py.registerFunction("_native_pause", [](const std::vector<std::string>&) -> std::string {
        auto* playback = playbackService();
        if (!playback) return "ERROR: playback service unavailable";
        playback->pause();
        return "OK";
    });
    py.registerFunction("_native_stop", [](const std::vector<std::string>&) -> std::string {
        auto* playback = playbackService();
        if (!playback) return "ERROR: playback service unavailable";
        playback->stop();
        return "OK";
    });
    py.registerFunction("_native_current_frame", [](const std::vector<std::string>&) -> std::string {
        auto* playback = playbackService();
        return playback ? std::to_string(playback->currentFrame().framePosition()) : std::string("0");
    });
    py.registerFunction("_native_set_current_frame", [](const std::vector<std::string>& args) -> std::string {
        auto* playback = playbackService();
        if (!playback || args.empty()) return "ERROR: invalid frame";
        const int frame = toInt(args[0], -1);
        if (frame < 0) return "ERROR: invalid frame";
        playback->setCurrentFrame(FramePosition(frame));
        return "OK";
    });

    std::string code = R"(
import artifact

# Render API
_render_settings = {}

def _render(start=0, end=None, output_path=None, format="png"):
    start = _render_settings.get("start", start)
    end = _render_settings.get("end", end)
    output_path = _render_settings.get("output_path", output_path)
    format = _render_settings.get("format", format)
    args = [str(start), "" if end is None else str(end), "" if output_path is None else output_path, format]
    return artifact._native_render(*args) == "OK"

def _render_current_frame(output_path=None):
    frame = artifact.current_frame()
    path = "" if output_path is None else output_path
    format = "png"
    if path:
        suffix = path.lower().rsplit(".", 1)[-1] if "." in path else ""
        if suffix in ("exr", "jpg", "jpeg", "tif", "tiff", "bmp", "webp"):
            format = suffix
    return artifact._native_render(str(frame), str(frame + 1), path, format) == "OK"

def _set_render_settings(**kwargs):
    supported = ("start", "end", "output_path", "format")
    for key, value in kwargs.items():
        if key not in supported:
            return False
        _render_settings[key] = value
    return True

def _play():
    return artifact._native_play() == "OK"

def _pause():
    return artifact._native_pause() == "OK"

def _stop():
    return artifact._native_stop() == "OK"

def _current_frame():
    return int(artifact._native_current_frame())

def _set_current_frame(frame):
    return artifact._native_set_current_frame(frame) == "OK"

artifact.render = _render
artifact.render_current_frame = _render_current_frame
artifact.set_render_settings = _set_render_settings
artifact.play = _play
artifact.pause = _pause
artifact.stop = _stop
artifact.current_frame = _current_frame
artifact.set_current_frame = _set_current_frame
)";
    py.execute(code);
}

void ArtifactPythonAPI::registerUtilityAPI() {
    auto& py = ArtifactCore::PythonEngine::instance();

    std::string code = R"(
import artifact
import time

# Utility API
def _log(message, level="info"):
    prefix = {"info": "[INFO]", "warn": "[WARN]", "error": "[ERROR]", "debug": "[DEBUG]"}
    print(f"{prefix.get(level, '[LOG]')} {message}")

def _for_each_frame(start, end, callback):
    for frame in range(start, end + 1):
        if not artifact.set_current_frame(frame):
            return False
        callback(frame)
    return True

artifact.log = _log
artifact.for_each_frame = _for_each_frame
)";
    py.execute(code);
}

} // namespace Artifact
