module;
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Artifact.PythonAPI;




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
