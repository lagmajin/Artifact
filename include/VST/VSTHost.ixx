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
export module Artifact.VST.Host;




import Audio.Segment;

export namespace Artifact {

// VST プラグイン情報
struct VSTPluginInfo {
    std::string name;           // プラグイン名
    std::string vendor;         // ベンダー名
    std::string path;          // プラグインファイルパス
    int numInputs = 0;         // 入力チャンネル数
    int numOutputs = 0;        // 出力チャンネル数
    int numParameters = 0;     // パラメータ数
    bool isSynth = false;      // シンセか
    bool hasEditor = false;    // GUIエディタがあるか
};

// VST パラメータ情報
struct VSTParameterInfo {
    int index = 0;
    std::string name;
    std::string display;
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float defaultValue = 0.0f;
    bool isDiscrete = false;
    bool isAutomatable = true;
};

// VST ホストクラス
class VSTHost {
public:
    VSTHost();
    ~VSTHost();

    // シングルトンアクセス
    static VSTHost& getInstance();

    // プラグインスキャン
    std::vector<VSTPluginInfo> scanPlugins(const std::string& searchPath);
    void addSearchPath(const std::string& path);
    void clearSearchPaths();

    // プラグイン管理
    bool loadPlugin(const std::string& path);
    void unloadPlugin(int pluginId);
    void unloadAllPlugins();

    // プラグイン情報取得
    VSTPluginInfo getPluginInfo(int pluginId) const;
    std::vector<VSTParameterInfo> getPluginParameters(int pluginId) const;

    // パラメータ操作
    bool setParameter(int pluginId, int paramIndex, float value);
    float getParameter(int pluginId, int paramIndex) const;
    std::string getParameterDisplay(int pluginId, int paramIndex) const;

    // オーディオ処理
    void setSampleRate(double sampleRate);
    void setBlockSize(int blockSize);
    
    // プロセッシング
    void process(int pluginId, const AudioSegment& input, AudioSegment& output);
    void processDouble(int pluginId, const std::vector<double>& input, std::vector<double>& output);
    
    // 状態管理
    void resume(int pluginId);
    void suspend(int pluginId);
    bool isPluginLoaded(int pluginId) const;
    int getLoadedPluginCount() const { return static_cast<int>(loadedPlugins_.size()); }

    // エディタ
    void openEditor(int pluginId, void* window);
    void closeEditor(int pluginId);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    
    std::vector<std::string> searchPaths_;
    std::vector<VSTPluginInfo> scannedPlugins_;
    std::map<int, std::string> loadedPlugins_; // pluginId -> path
    int nextPluginId_ = 0;
};

// VST3 / VST2 互換レイヤー
class VSTPluginLoader {
public:
    static std::unique_ptr<VSTPluginInfo> loadPluginInfo(const std::string& path);
    static bool isVST3Plugin(const std::string& path);
    static bool isVST2Plugin(const std::string& path);
};

};
