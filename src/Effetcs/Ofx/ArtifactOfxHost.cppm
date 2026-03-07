module;
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <windows.h> // For LoadLibrary / GetProcAddress
// #include <ofx/ofxCore.h>
// #include <ofx/ofxImageEffect.h>
#include <iostream>

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
export module Artifact.Effect.Ofx.Host;




import Utils.String.UniString;
import Artifact.Effect.Context;

namespace Artifact {
namespace Ofx {

using namespace ArtifactCore;

// OpenFX Property Set (stub implementation)
struct OfxPropertySet {
    // Actually mapped to real property maps in full implementation
};

// Represents an OpenFX Plugin binary descriptor
export struct OfxPluginDescriptor {
    UniString pluginPath;
    // OfxPlugin* pluginAPI;  // Commented out: OFX headers not available
    UniString identifier;
    UniString version;
    HMODULE libraryHandle;
};

// Represents the global OpenFX Host Application
export class ArtifactOfxHost {
public:
    static ArtifactOfxHost& instance() {
        static ArtifactOfxHost s_instance;
        return s_instance;
    }

    // Initialize host and load standard OFX suite functions
    void initialize() {
        // TODO: Implement after OFX headers are available
        // // Here we provide the host struct required by plugins
        // hostStruct_.host = hostStructPtr();
        // hostStruct_.fetchSuite = fetchSuiteCallback;
    }

    // Scan a directory for .ofx bundles and load them
    void scanDirectory(const UniString& path) {
        // TODO: Implement after OFX headers are available
        // Implementation:
        // 1. Iterate over matching files (e.g. *.ofx)
        // 2. LoadLibrary
        // 3. GetProcAddress("OfxGetNumberOfPlugins")
        // 4. GetProcAddress("OfxGetPlugin")
        // 5. Store in plugins_ array
        // std::wcout << L"Scanning OFX Directory: " << path << std::endl;
    }

    const std::vector<OfxPluginDescriptor>& getLoadedPlugins() const {
        return plugins_;
    }

    void* getOfxHostStruct() {
        // return &hostStruct_;  // Commented out: OFX headers not available
        return nullptr;
    }

private:
    ArtifactOfxHost() = default;
    ~ArtifactOfxHost() = default;

    std::vector<OfxPluginDescriptor> plugins_;
    // OfxHost hostStruct_;  // Commented out: OFX headers not available

    // static void* fetchSuiteCallback(OfxPropertySetHandle host, const char* suiteName, int suiteVersion) {
    //     // Route requested OFX suites to our implementations
    //     // e.g. "OfxPropertySuite", "OfxImageEffectSuite", "OfxMemorySuite"
    //     // Return nullptr for unsupported suites for now
    //     std::cout << "Plugin requested suite: " << suiteName << " v" << suiteVersion << std::endl;
    //     return nullptr;
    // }

    // static OfxPropertySetHandle hostStructPtr() {
    //     return nullptr; // Later: allocate and return a global host property set
    // }
};

} // namespace Ofx
} // namespace Artifact
