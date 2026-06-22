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
module Artifact.PythonAPI;

import Script.Python.Engine;
import Artifact.Application.Manager;
import Artifact.Service.Project;
import Artifact.Layers.Selection.Manager;
import Artifact.Composition.Abstract;
import Property;
import Property.Group;
import Property.Abstract;
import Frame.Position;
import Frame.Range;

namespace Artifact {

namespace {
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

    // In a production build, these would call back into actual ProjectManager
    std::string code = R"(
import artifact

# Project API
def _project_new(name="Untitled"):
    """Create a new project"""
    pass

def _project_open(path):
    """Open a project file"""
    pass

def _project_save(path=None):
    """Save current project"""
    pass

def _project_info():
    """Get current project information"""
    return {"name": "Untitled", "fps": 30.0, "width": 1920, "height": 1080}

artifact.project_new = _project_new
artifact.project_open = _project_open
artifact.project_save = _project_save
artifact.project_info = _project_info
)";
    py.execute(code);
}

void ArtifactPythonAPI::registerLayerAPI() {
    auto& py = ArtifactCore::PythonEngine::instance();

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
                service->clearAllMarkers();
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
        const FrameRange trimmedRange(FramePosition(startFrame), FramePosition(endFrame));
        comp->setFrameRange(trimmedRange);
        if (setWorkArea) {
            comp->setWorkAreaRange(trimmedRange);
        }

        return std::string("OK: trimmed to ") + std::to_string(startFrame) + "-" +
               std::to_string(endFrame);
    });

    std::string code = R"(
import artifact

# Layer API
def _get_layers():
    """Get all layer names in current composition"""
    return []

def _get_selected_layers():
    """Get selected layer names"""
    return []

def _create_layer(name, layer_type="solid"):
    """Create a new layer"""
    pass

def _delete_layer(name):
    """Delete a layer by name"""
    pass

def _duplicate_layer(name, new_name=None):
    """Duplicate a layer"""
    pass

def _set_layer_property(layer_name, property_name, value):
    pass

def _get_layer_property(layer_name, property_name):
    return None

def _move_layer(layer_name, index):
    pass

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

    std::string code = R"(
import artifact

# Effect API
def _add_effect(layer_name, effect_type):
    pass

def _remove_effect(layer_name, effect_index):
    pass

def _get_effects(layer_name):
    return []

def _set_effect_param(layer_name, effect_index, param_name, value):
    pass

def _get_effect_param(layer_name, effect_index, param_name):
    return None

def _get_available_effects():
    return ["blur", "glow", "reverb", "compressor", "delay", "limiter", "distortion"]

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

    std::string code = R"(
import artifact

# Render API
def _render(start=0, end=None, output_path=None, format="png"):
    pass

def _render_current_frame(output_path=None):
    pass

def _set_render_settings(**kwargs):
    pass

artifact.render = _render
artifact.render_current_frame = _render_current_frame
artifact.set_render_settings = _set_render_settings
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
        # _set_frame(frame)
        callback(frame)

artifact.log = _log
artifact.for_each_frame = _for_each_frame
)";
    py.execute(code);
}

} // namespace Artifact
