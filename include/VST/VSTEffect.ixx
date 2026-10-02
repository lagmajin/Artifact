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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <QString>
#include <QtGlobal>
export module Artifact.VST.Effect;




import Audio.Segment;
import Audio.Effect;
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
    void process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain = nullptr) override;
    String getName() const override { return String("VST Effect"); }
    String getDescription() const override { return String("VST plugin effect"); }

    std::vector<AudioEffectParameter> getUiParameters() const override;
    void setParameter(const String& name, float value) override;
    float getParameter(const String& name) const override;

    // ArtifactCore::AudioEffect インターフェース — CLAP 側と同じ経路。
    // シリアライズと latencySamples()/tailSamples() 集約が UI に依らず機能する。
    std::vector<ArtifactCore::EffectParameter> getParameters() const override;
    void setParameterValue(const ArtifactCore::String& id, float value) override;
    float getParameterValue(const ArtifactCore::String& id) const override;
    ArtifactCore::String effectType() const override;

    qint64 latencySamples() const override;
    qint64 tailSamples() const override;

    // VST 固有メソッド
    void openEditor(void* parentWindow);
    bool openEditorWindow(void* nativeParent, void* resizeContext,
                          bool (*resizeCallback)(void*, int, int),
                          int& width, int& height, bool& resizable);
    bool resizeEditor(int width, int height);
    void closeEditor();
    bool hasEditor() const;

    // サンプルレート設定
    void setSampleRate(int sampleRate);
    int getSampleRate() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// ファクトリー関数
std::unique_ptr<ArtifactAbstractAudioEffect> createVSTEffect(const std::string& path = "");

};
