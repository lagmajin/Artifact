module;
#include <string>
#include <vector>
#include <chrono>
#include <sstream>
module Artifact.PythonAPI;

import Script.Python.Engine;

namespace Artifact {

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

artifact.get_layers = _get_layers
artifact.get_selected_layers = _get_selected_layers
artifact.create_layer = _create_layer
artifact.delete_layer = _delete_layer
artifact.duplicate_layer = _duplicate_layer
artifact.set_layer_property = _set_layer_property
artifact.get_layer_property = _get_layer_property
artifact.move_layer = _move_layer
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
