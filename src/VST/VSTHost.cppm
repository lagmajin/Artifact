module;
#include <iostream>
#include <fstream>
#include <filesystem>
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

namespace fs = std::filesystem;

namespace Artifact {

// 内部実装構造体
struct VSTHost::Impl {
    double sampleRate = 44100.0;
    int blockSize = 512;
    
    // ロードされたプラグイン管理
    struct LoadedPlugin {
        VSTPluginInfo info;
        void* handle = nullptr;
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
    int nextPluginId = 0;
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
    searchPaths_.push_back(path);
}

void VSTHost::clearSearchPaths() {
    searchPaths_.clear();
}

bool VSTPluginLoader::isVST2Plugin(const std::string& path) {
#ifdef _WIN32
    return path.find(".dll") != std::string::npos;
#elif __APPLE__
    return path.find(".vst") != std::string::npos;
#else
    return path.find(".so") != std::string::npos;
#endif
}

bool VSTPluginLoader::isVST3Plugin(const std::string& path) {
    return path.find(".vst3") != std::string::npos;
}

std::unique_ptr<VSTPluginInfo> VSTPluginLoader::loadPluginInfo(const std::string& path) {
    // シンプルなファイル名からの情報抽出
    auto info = std::make_unique<VSTPluginInfo>();
    info->path = path;
    
    // ファイル名から名前を抽出
    fs::path p(path);
    info->name = p.stem().string();
    
    // 拡張子に応じてVST2/VST3を判定
    if (path.find(".vst3") != std::string::npos) {
        info->name += " (VST3)";
    } else {
        info->name += " (VST2)";
    }
    
    info->vendor = "Unknown";
    info->numInputs = 2;
    info->numOutputs = 2;
    info->numParameters = 0;
    info->isSynth = false;
    info->hasEditor = true;

         return info;
    }

    std::vector<VSTPluginInfo> VSTHost::scanPlugins(const std::string& searchPath) {
        std::vector<VSTPluginInfo> plugins;
    
    if (!searchPath.empty()) {
        addSearchPath(searchPath);
    }
    
    for (const auto& searchPath : searchPaths_) {
        try {
            if (!fs::exists(searchPath)) continue;
            
            for (const auto& entry : fs::recursive_directory_iterator(searchPath)) {
                if (!entry.is_regular_file()) continue;
                
                std::string path = entry.path().string();
                
                if (VSTPluginLoader::isVST2Plugin(path) || VSTPluginLoader::isVST3Plugin(path)) {
                    auto info = VSTPluginLoader::loadPluginInfo(path);
                    if (info) {
                        plugins.push_back(*info);
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error scanning path " << searchPath << ": " << e.what() << std::endl;
        }
    }
    
    scannedPlugins_ = plugins;
    return plugins;
}

bool VSTHost::loadPlugin(const std::string& path) {
    if (!VSTPluginLoader::isVST2Plugin(path)) {
        std::cerr << "VST3 plugins not yet supported in this implementation" << std::endl;
        return false;
    }
    
    // 動的ライブラリをロード
#ifdef _WIN32
    void* handle = LoadLibraryA(path.c_str());
#else
    void* handle = dlopen(path.c_str(), RTLD_NOW);
#endif
    
    if (!handle) {
#ifdef _WIN32
        std::cerr << "Failed to load plugin: " << path << std::endl;
#else
        std::cerr << "Failed to load plugin: " << dlerror() << std::endl;
#endif
        return false;
    }
    
    // VST メインエントリポイントを取得
#ifdef _WIN32
    void* entry = GetProcAddress((HMODULE)handle, "VSTPluginMain");
    if (!entry) entry = GetProcAddress((HMODULE)handle, "main");
#else
    void* entry = dlsym(handle, "VSTPluginMain");
    if (!entry) entry = dlsym(handle, "main");
#endif
    
    if (!entry) {
        std::cerr << "Failed to find VST entry point" << std::endl;
#ifdef _WIN32
        FreeLibrary((HMODULE)handle);
#else
        dlclose(handle);
#endif
        return false;
    }
    
    // プラグインを初期化
    auto plugin = std::make_unique<Impl::LoadedPlugin>();
    plugin->handle = handle;
    plugin->entryPoint = entry;
    plugin->info.path = path;
    
    // ファイル名から名前を抽出
    fs::path p(path);
    plugin->info.name = p.stem().string() + " (VST2)";
    
    // プラグイン情報を取得（可能な範囲で）
    plugin->info.numInputs = 2;
    plugin->info.numOutputs = 2;
    plugin->info.hasEditor = true;

         int pluginId = nextPluginId_++;
         loadedPlugins_[pluginId] = path;

         return true;
         }

    void VSTHost::unloadPlugin(int pluginId) {
        auto it = loadedPlugins_.find(pluginId);
        if (it == loadedPlugins_.end()) return;

        loadedPlugins_.erase(it);
    }

    void VSTHost::unloadAllPlugins() {
    auto pluginIds = std::vector<int>();
    for (const auto& [id, _] : loadedPlugins_) {
        pluginIds.push_back(id);
    }
    
    for (int id : pluginIds) {
        unloadPlugin(id);
    }
}

VSTPluginInfo VSTHost::getPluginInfo(int pluginId) const {
    auto it = loadedPlugins_.find(pluginId);
    if (it != loadedPlugins_.end()) {
        // Load plugin info from path
        auto pluginInfo = VSTPluginLoader::loadPluginInfo(it->second);
        if (pluginInfo) {
            return *pluginInfo;
        }
    }
    return VSTPluginInfo();
}

std::vector<VSTParameterInfo> VSTHost::getPluginParameters(int pluginId) const {
    std::vector<VSTParameterInfo> params;
    return params;
}

bool VSTHost::setParameter(int pluginId, int paramIndex, float value) {
    return false;
}

float VSTHost::getParameter(int pluginId, int paramIndex) const {
    return 0.0f;
}

std::string VSTHost::getParameterDisplay(int pluginId, int paramIndex) const {
    float value = getParameter(pluginId, paramIndex);
    return std::to_string(value);
}

void VSTHost::setSampleRate(double sampleRate) {
    impl_->sampleRate = sampleRate;
}

void VSTHost::setBlockSize(int blockSize) {
    impl_->blockSize = blockSize;
}

void VSTHost::process(int pluginId, const ArtifactCore::AudioSegment& input, ArtifactCore::AudioSegment& output) {
    // VST プラグインがない場合は入力をそのまま出力
    output = input;
}

void VSTHost::processDouble(int pluginId, const std::vector<double>& input, std::vector<double>& output) {
    output = input;
}

void VSTHost::resume(int pluginId) {
    // Stub implementation
}

void VSTHost::suspend(int pluginId) {
    // Stub implementation
}

bool VSTHost::isPluginLoaded(int pluginId) const {
    return loadedPlugins_.find(pluginId) != loadedPlugins_.end();
}

void VSTHost::openEditor(int pluginId, void* window) {
    auto it = loadedPlugins_.find(pluginId);
    if (it != loadedPlugins_.end()) {
        // VST エディタを開く処理
    }
}

void VSTHost::closeEditor(int pluginId) {
    auto it = loadedPlugins_.find(pluginId);
    if (it != loadedPlugins_.end()) {
        // VST エディタを閉じる処理
    }
}

}
