module;
export module Artifact.PythonAPI;

import std;
import Script.Python.Engine;

export namespace Artifact {

/**
 * @brief Registers Artifact Application's Python API into the Core's embedded interpreter.
 * This bridges the generic Python engine with our specific application data 
 * (Compositions, Layers, Effects, etc.).
 */
class ArtifactPythonAPI {
public:
    static void registerAll();

private:
    static void registerProjectAPI();
    static void registerLayerAPI();
    static void registerEffectAPI();
    static void registerRenderAPI();
    static void registerUtilityAPI();
};

} // namespace Artifact
