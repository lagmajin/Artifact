module;
#include <cstring>
#include <algorithm>

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
module Artifact.VST.Effect;




import Audio.Segment;
import Artifact.Audio.Effects.Base;
import Artifact.VST.Host;

namespace Artifact {

// 内部実装
class VSTEffect::Impl {
public:
    int pluginId = -1;
    std::string pluginPath;
    std::string pluginName;
    double sampleRate = 44100.0;
    int blockSize = 512;
    bool isLoaded = false;
    
    // パラメータ管理
    std::map<std::string, float> parameters;
};

// ファクトリー関数
std::unique_ptr<ArtifactAbstractAudioEffect> createVSTEffect(const std::string& path) {
    auto effect = std::make_unique<VSTEffect>();
    if (!path.empty()) {
        effect->loadPlugin(path);
    }
    return effect;
}

VSTEffect::VSTEffect() : impl_(new Impl()) {
}

VSTEffect::~VSTEffect() {
    unloadPlugin();
}

bool VSTEffect::loadPlugin(const std::string& path) {
    auto& host = VSTHost::getInstance();
    
    // 既にロードされている場合は閉じる
    if (impl_->isLoaded) {
        unloadPlugin();
    }
    
    // プラグインをロード
    if (!host.loadPlugin(path)) {
        return false;
    }
    
    impl_->pluginPath = path;
    impl_->pluginId = host.getLoadedPluginCount() - 1;
    impl_->pluginName = host.getPluginInfo(impl_->pluginId).name;
    impl_->isLoaded = true;
    
    // サンプルレートを設定
    host.setSampleRate(impl_->sampleRate);
    host.setBlockSize(impl_->blockSize);
    
    // プラグインを再開
    host.resume(impl_->pluginId);
    
    // 名前を更新
    impl_->pluginName = host.getPluginInfo(impl_->pluginId).name;
    
    return true;
}

void VSTEffect::unloadPlugin() {
    if (!impl_->isLoaded) return;
    
    auto& host = VSTHost::getInstance();
    if (impl_->pluginId >= 0) {
        host.unloadPlugin(impl_->pluginId);
    }
    
    impl_->pluginId = -1;
    impl_->pluginPath.clear();
    impl_->pluginName.clear();
    impl_->isLoaded = false;
}

std::string VSTEffect::getPluginName() const {
    return impl_->pluginName;
}

bool VSTEffect::isPluginLoaded() const {
    return impl_->isLoaded;
}

void VSTEffect::process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain) {
    if (!impl_->isLoaded || impl_->pluginId < 0) {
        return;
    }
    
    auto& host = VSTHost::getInstance();
    
    // VST プラグインを通じてオーディオを処理 (インプレース)
    // ※ VSTHost::process がインプレースに対応している必要がある。
    // もし対応していない場合は一時バッファを使う
    ArtifactCore::AudioSegment output = segment; // 暫定的にコピー
    host.process(impl_->pluginId, segment, output);
    segment = output;
}

std::vector<AudioEffectParameter> VSTEffect::getParameters() const {
    std::vector<AudioEffectParameter> params;
    
    if (!impl_->isLoaded || impl_->pluginId < 0) {
        return params;
    }
    
    auto& host = VSTHost::getInstance();
    auto pluginParams = host.getPluginParameters(impl_->pluginId);

    for (const auto& param : pluginParams) {
        AudioEffectParameter effectParam;
        effectParam.name = param.name;
        effectParam.minValue = param.minValue;
        effectParam.maxValue = param.maxValue;
        effectParam.defaultValue = param.defaultValue;

        // 現在値を取得
        // effectParam.value = host.getParameter(impl_->pluginId, param.index);

        params.push_back(effectParam);
    }
    
    return params;
}

void VSTEffect::setParameter(const std::string& name, float value) {
    impl_->parameters[name] = value;
    
    if (!impl_->isLoaded || impl_->pluginId < 0) return;
    
    auto& host = VSTHost::getInstance();
    auto pluginParams = host.getPluginParameters(impl_->pluginId);
    
    for (size_t i = 0; i < pluginParams.size(); i++) {
        if (pluginParams[i].name == name) {
            host.setParameter(impl_->pluginId, static_cast<int>(i), value);
            break;
        }
    }
}

float VSTEffect::getParameter(const std::string& name) const {
    auto it = impl_->parameters.find(name);
    if (it != impl_->parameters.end()) {
        return it->second;
    }
    return 0.0f;
}

void VSTEffect::openEditor(void* parentWindow) {
    if (!impl_->isLoaded || impl_->pluginId < 0) return;
    
    auto& host = VSTHost::getInstance();
    host.openEditor(impl_->pluginId, parentWindow);
}

void VSTEffect::closeEditor() {
    if (!impl_->isLoaded || impl_->pluginId < 0) return;
    
    auto& host = VSTHost::getInstance();
    host.closeEditor(impl_->pluginId);
}

bool VSTEffect::hasEditor() const {
    if (!impl_->isLoaded || impl_->pluginId < 0) return false;
    
    auto& host = VSTHost::getInstance();
    return host.getPluginInfo(impl_->pluginId).hasEditor;
}

void VSTEffect::setSampleRate(int sampleRate) {
    impl_->sampleRate = sampleRate;

    if (impl_->isLoaded && impl_->pluginId >= 0) {
        auto& host = VSTHost::getInstance();
        host.setSampleRate(sampleRate);
    }
}

int VSTEffect::getSampleRate() const {
    return impl_->sampleRate;
}

}
