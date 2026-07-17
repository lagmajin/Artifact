module;
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cctype>
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

#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

#include <QList>
module Artifact.VST.Host;




import Audio.Segment;
import VST3.Interfaces;

namespace fs = std::filesystem;

namespace Artifact {

namespace {

std::string toLowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

VSTPluginInfo makePluginInfoFromPath(const std::string& path, bool isVST3)
{
    VSTPluginInfo info;
    info.path = path;
    info.name = fs::path(path).stem().string();
    info.name += isVST3 ? " (VST3)" : " (VST2)";
    info.vendor = "Unknown";
    info.numInputs = 2;
    info.numOutputs = 2;
    info.numParameters = 0;
    info.isSynth = false;
    info.hasEditor = true;
    return info;
}

void releaseLibraryHandle(void* handle)
{
    if (!handle) {
        return;
    }
#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}

} // namespace

// 内部実装構造体
struct VSTHost::Impl {
    double sampleRate = 44100.0;
    int blockSize = 512;

    struct LoadedPlugin {
        VSTPluginInfo info;
        std::string path;
        bool isVST3 = false;
        void* handle = nullptr;
        std::unique_ptr<Steinberg::VST3Module> vst3Module;
        std::vector<float> parameters;
        bool isProcessing = false;

        // VST2 関数ポインタ
        void* entryPoint = nullptr;
        int (*main)(void*) = nullptr;
        int (*dispatcher)(int32_t, int, int32_t, void*, float) = nullptr;
        void (*setParameter)(int32_t, float) = nullptr;
        float (*getParameter)(int32_t) = nullptr;
        void (*process)(void**, void**, int32_t) = nullptr;
        void (*processDouble)(double**, double**, int32_t) = nullptr;
    };

    std::map<int, LoadedPlugin> loadedPlugins;
};

// VST2 定数
constexpr int32_t VST_MAGIC = 'VstP';
constexpr int32_t effGetVendorVersion = 1;
constexpr int32_t effGetCategory = 2;
constexpr int32_t effSetSampleRate = 10;
constexpr int32_t effSetBlockSize = 11;
constexpr int32_t effMainsChanged = 1;
constexpr int32_t effEditOpen = 2;
constexpr int32_t effEditClose = 3;
constexpr int32_t effGetParameterName = 5;
constexpr int32_t effGetParameterDisplay = 6;
constexpr int32_t effGetParameterLabel = 7;
constexpr int32_t effGetParamMin = 8;
constexpr int32_t effGetParamMax = 9;
constexpr int32_t effGetParamDef = 11;
constexpr int32_t effGetNumParams = 22;
constexpr int32_t effGetNumInputs = 23;
constexpr int32_t effGetNumOutputs = 24;
constexpr int32_t effGetProductString = 21;
constexpr int32_t effGetVendorString = 20;
constexpr int32_t effCanDo = 12;
constexpr int32_t effIdentify = 0xDEADBEEF;

constexpr int32_t kVstAudioInputMask = 1;
constexpr int32_t kVstAudioOutputMask = 2;
constexpr int32_t kVstSynthCategory = 1;

VSTHost::VSTHost() : impl_(new Impl()) {
    // デフォルトの検索パスを追加
#ifdef _WIN32
    addSearchPath("C:/Program Files/VSTPlugins");
    addSearchPath("C:/Program Files/Common Files/VST3");
    addSearchPath("C:/Program Files/Steinberg/VSTPlugins");
#elif __APPLE__
    addSearchPath("/Library/Audio/Plug-Ins/VST");
    addSearchPath("/Library/Audio/Plug-Ins/VST3");
    addSearchPath("~/Library/Audio/Plug-Ins/VST");
#else
    addSearchPath("/usr/lib/vst");
    addSearchPath("/usr/local/lib/vst");
    addSearchPath("~/.vst");
#endif
}

VSTHost::~VSTHost() {
    unloadAllPlugins();
}

VSTHost& VSTHost::getInstance() {
    static VSTHost instance;
    return instance;
}

void VSTHost::addSearchPath(const std::string& path) {
    if (std::find(searchPaths_.begin(), searchPaths_.end(), path) != searchPaths_.end()) {
        return;
    }
    searchPaths_.push_back(path);
}

void VSTHost::clearSearchPaths() {
    searchPaths_.clear();
}

bool VSTPluginLoader::isVST2Plugin(const std::string& path) {
    const std::string ext = toLowerCopy(fs::path(path).extension().string());
    return ext == ".dll" || ext == ".so" || ext == ".vst";
}

bool VSTPluginLoader::isVST3Plugin(const std::string& path) {
    return toLowerCopy(fs::path(path).extension().string()) == ".vst3";
}

std::unique_ptr<VSTPluginInfo> VSTPluginLoader::loadPluginInfo(const std::string& path) {
    if (path.empty() || !fs::exists(path)) {
        return nullptr;
    }

    auto info = std::make_unique<VSTPluginInfo>();
    *info = makePluginInfoFromPath(path, isVST3Plugin(path));
    return info;
}

std::vector<VSTPluginInfo> VSTHost::scanPlugins(const std::string& searchPath) {
    std::vector<VSTPluginInfo> plugins;

    if (!searchPath.empty()) {
        addSearchPath(searchPath);
    }

    for (const auto& candidateRoot : searchPaths_) {
        try {
            if (!fs::exists(candidateRoot)) {
                continue;
            }

            for (const auto& entry : fs::recursive_directory_iterator(candidateRoot)) {
                const std::string path = entry.path().string();
                if (!VSTPluginLoader::isVST2Plugin(path) &&
                    !VSTPluginLoader::isVST3Plugin(path)) {
                    continue;
                }

                auto info = VSTPluginLoader::loadPluginInfo(path);
                if (info) {
                    plugins.push_back(*info);
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error scanning path " << candidateRoot << ": " << e.what()
                      << std::endl;
        }
    }

    scannedPlugins_ = plugins;
    return plugins;
}

bool VSTHost::loadPlugin(const std::string& path) {
    if (path.empty() || !fs::exists(path)) {
        std::cerr << "Failed to load plugin: file not found: " << path << std::endl;
        return false;
    }

    if (getLoadedPluginIdByPath(path) >= 0) {
        return true;
    }

    if (!VSTPluginLoader::isVST2Plugin(path) && !VSTPluginLoader::isVST3Plugin(path)) {
        std::cerr << "Unsupported plugin format: " << path << std::endl;
        return false;
    }

    Impl::LoadedPlugin plugin;
    plugin.path = path;
    plugin.isVST3 = VSTPluginLoader::isVST3Plugin(path);
    plugin.info = makePluginInfoFromPath(path, plugin.isVST3);

    if (plugin.isVST3) {
        plugin.vst3Module = std::make_unique<Steinberg::VST3Module>();
        if (!plugin.vst3Module->load(path)) {
            std::cerr << "Failed to load VST3 plugin: " << path << std::endl;
            return false;
        }
    } else {
#ifdef _WIN32
        plugin.handle = LoadLibraryA(path.c_str());
#else
        plugin.handle = dlopen(path.c_str(), RTLD_NOW);
#endif
        if (!plugin.handle) {
#ifdef _WIN32
            std::cerr << "Failed to load plugin: " << path << std::endl;
#else
            std::cerr << "Failed to load plugin: " << dlerror() << std::endl;
#endif
            return false;
        }

#ifdef _WIN32
        plugin.entryPoint = GetProcAddress(static_cast<HMODULE>(plugin.handle), "VSTPluginMain");
        if (!plugin.entryPoint) {
            plugin.entryPoint = GetProcAddress(static_cast<HMODULE>(plugin.handle), "main");
        }
#else
        plugin.entryPoint = dlsym(plugin.handle, "VSTPluginMain");
        if (!plugin.entryPoint) {
            plugin.entryPoint = dlsym(plugin.handle, "main");
        }
#endif

        if (!plugin.entryPoint) {
            std::cerr << "Failed to find VST entry point: " << path << std::endl;
            releaseLibraryHandle(plugin.handle);
            return false;
        }
    }

    const int pluginId = nextPluginId_++;
    loadedPlugins_[pluginId] = path;
    impl_->loadedPlugins.emplace(pluginId, std::move(plugin));
    return true;
}

void VSTHost::unloadPlugin(int pluginId) {
    auto pathIt = loadedPlugins_.find(pluginId);
    if (pathIt == loadedPlugins_.end()) {
        return;
    }

    auto implIt = impl_->loadedPlugins.find(pluginId);
    if (implIt != impl_->loadedPlugins.end()) {
        if (!implIt->second.isVST3) {
            releaseLibraryHandle(implIt->second.handle);
        }
        impl_->loadedPlugins.erase(implIt);
    }

    loadedPlugins_.erase(pathIt);
}

void VSTHost::unloadAllPlugins() {
    std::vector<int> pluginIds;
    for (const auto& [id, _] : loadedPlugins_) {
        pluginIds.push_back(id);
    }

    for (int id : pluginIds) {
        unloadPlugin(id);
    }
}

VSTPluginInfo VSTHost::getPluginInfo(int pluginId) const {
    const auto implIt = impl_->loadedPlugins.find(pluginId);
    if (implIt != impl_->loadedPlugins.end()) {
        return implIt->second.info;
    }

    const auto pathIt = loadedPlugins_.find(pluginId);
    if (pathIt != loadedPlugins_.end()) {
        auto pluginInfo = VSTPluginLoader::loadPluginInfo(pathIt->second);
        if (pluginInfo) {
            return *pluginInfo;
        }
    }

    return VSTPluginInfo();
}

std::vector<VSTParameterInfo> VSTHost::getPluginParameters(int pluginId) const {
    std::vector<VSTParameterInfo> params;
    const auto implIt = impl_->loadedPlugins.find(pluginId);
    if (implIt == impl_->loadedPlugins.end()) {
        return params;
    }

    const auto& plugin = implIt->second;
    const int count = std::max(plugin.info.numParameters,
                               static_cast<int>(plugin.parameters.size()));
    if (count <= 0) {
        return params;
    }

    params.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        VSTParameterInfo param;
        param.index = i;
        param.name = "Param " + std::to_string(i + 1);
        param.display = (i < static_cast<int>(plugin.parameters.size()))
                            ? std::to_string(plugin.parameters[static_cast<size_t>(i)])
                            : "0";
        param.minValue = 0.0f;
        param.maxValue = 1.0f;
        param.defaultValue = 0.0f;
        param.isDiscrete = false;
        param.isAutomatable = true;
        params.push_back(std::move(param));
    }
    return params;
}

bool VSTHost::setParameter(int pluginId, int paramIndex, float value) {
    auto it = impl_->loadedPlugins.find(pluginId);
    if (it == impl_->loadedPlugins.end() || paramIndex < 0) {
        return false;
    }

    auto& plugin = it->second;
    const size_t index = static_cast<size_t>(paramIndex);
    if (plugin.parameters.size() <= index) {
        plugin.parameters.resize(index + 1, 0.0f);
    }
    plugin.parameters[index] = value;
    return true;
}

float VSTHost::getParameter(int pluginId, int paramIndex) const {
    const auto it = impl_->loadedPlugins.find(pluginId);
    if (it == impl_->loadedPlugins.end() || paramIndex < 0) {
        return 0.0f;
    }

    const auto& params = it->second.parameters;
    const size_t index = static_cast<size_t>(paramIndex);
    if (index >= params.size()) {
        return 0.0f;
    }
    return params[index];
}

std::string VSTHost::getParameterDisplay(int pluginId, int paramIndex) const {
    return std::to_string(getParameter(pluginId, paramIndex));
}

void VSTHost::setSampleRate(double sampleRate) {
    impl_->sampleRate = sampleRate;
}

void VSTHost::setBlockSize(int blockSize) {
    impl_->blockSize = blockSize;
}

void VSTHost::process(int pluginId, const ArtifactCore::AudioSegment& input, ArtifactCore::AudioSegment& output) {
    if (!isPluginLoaded(pluginId)) {
        output = input;
        return;
    }

    // 現時点は状態管理とロード経路の整備を優先し、オーディオは透過させる。
    output = input;
}

void VSTHost::processDouble(int pluginId, const std::vector<double>& input, std::vector<double>& output) {
    (void)pluginId;
    output = input;
}

void VSTHost::resume(int pluginId) {
    auto it = impl_->loadedPlugins.find(pluginId);
    if (it != impl_->loadedPlugins.end()) {
        it->second.isProcessing = true;
    }
}

void VSTHost::suspend(int pluginId) {
    auto it = impl_->loadedPlugins.find(pluginId);
    if (it != impl_->loadedPlugins.end()) {
        it->second.isProcessing = false;
    }
}

bool VSTHost::isPluginLoaded(int pluginId) const {
    return impl_->loadedPlugins.find(pluginId) != impl_->loadedPlugins.end();
}

int VSTHost::getLoadedPluginIdByPath(const std::string& path) const {
    for (const auto& [pluginId, loadedPath] : loadedPlugins_) {
        if (loadedPath == path) {
            return pluginId;
        }
    }
    return -1;
}

void VSTHost::openEditor(int pluginId, void* window) {
    (void)window;
    auto it = impl_->loadedPlugins.find(pluginId);
    if (it != impl_->loadedPlugins.end()) {
        std::cout << "[VSTHost] openEditor requested for " << it->second.info.name
                  << std::endl;
    }
}

void VSTHost::closeEditor(int pluginId) {
    auto it = impl_->loadedPlugins.find(pluginId);
    if (it != impl_->loadedPlugins.end()) {
        std::cout << "[VSTHost] closeEditor requested for " << it->second.info.name
                  << std::endl;
    }
}

}
