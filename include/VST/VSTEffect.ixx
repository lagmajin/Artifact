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
export module Artifact.VST.Effect;




import Audio.Segment;
import Artifact.Audio.Effects.Base;

export namespace Artifact {

// VST プラグインをオーディオエフェクトとしてラップするクラス
class VSTEffect : public ArtifactAbstractAudioEffect {
public:
    VSTEffect();
    ~VSTEffect() override;

    // プラグイン読み込み
    bool loadPlugin(const std::string& path);
    void unloadPlugin();
    
    // プラグイン情報
    std::string getPluginName() const;
    bool isPluginLoaded() const;
    
    // ArtifactAbstractAudioEffect インターフェース
    void process(ArtifactCore::AudioSegment& segment) override;
    std::string getName() const override { return "VST Effect"; }
    std::string getDescription() const override { return "VST plugin effect"; }
    
    std::vector<AudioEffectParameter> getParameters() const override;
    void setParameter(const std::string& name, float value) override;
    float getParameter(const std::string& name) const override;

    // VST 固有メソッド
    void openEditor(void* parentWindow);
    void closeEditor();
    bool hasEditor() const;
    
    // サンプルレート設定
    void setSampleRate(double sampleRate);
    double getSampleRate() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// ファクトリー関数
std::unique_ptr<ArtifactAbstractAudioEffect> createVSTEffect(const std::string& path = "");

};
