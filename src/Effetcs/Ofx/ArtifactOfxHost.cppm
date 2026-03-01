module;
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <windows.h> // For LoadLibrary / GetProcAddress
#include "ofxCore.h"
#include "ofxImageEffect.h"
#include <iostream>

export module Artifact.Effect.Ofx.Host;

import std;
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
    OfxPlugin* pluginAPI;
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
        // Here we provide the host struct required by plugins
        hostStruct_.host = hostStructPtr();
        hostStruct_.fetchSuite = fetchSuiteCallback;
    }

    // Scan a directory for .ofx bundles and load them
    void scanDirectory(const UniString& path) {
        // Implementation:
        // 1. Iterate over matching files (e.g. *.ofx)
        // 2. LoadLibrary
        // 3. GetProcAddress("OfxGetNumberOfPlugins")
        // 4. GetProcAddress("OfxGetPlugin")
        // 5. Store in plugins_ array
        std::wcout << L"Scanning OFX Directory: " << path.toStdWString() << std::endl;
    }

    const std::vector<OfxPluginDescriptor>& getLoadedPlugins() const {
        return plugins_;
    }

    OfxHost* getOfxHostStruct() {
        return &hostStruct_;
    }

private:
    ArtifactOfxHost() = default;
    ~ArtifactOfxHost() = default;

    std::vector<OfxPluginDescriptor> plugins_;
    OfxHost hostStruct_;

    static void* fetchSuiteCallback(OfxPropertySetHandle host, const char* suiteName, int suiteVersion) {
        // Route requested OFX suites to our implementations
        // e.g. "OfxPropertySuite", "OfxImageEffectSuite", "OfxMemorySuite"
        // Return nullptr for unsupported suites for now
        std::cout << "Plugin requested suite: " << suiteName << " v" << suiteVersion << std::endl;
        return nullptr;
    }

    static OfxPropertySetHandle hostStructPtr() {
        return nullptr; // Later: allocate and return a global host property set
    }
};

} // namespace Ofx
} // namespace Artifact
