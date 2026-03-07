module;
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
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
export module Artifact.Audio.Effects.Manager;




import Artifact.Audio.Effects.Base;

export namespace Artifact {

class ArtifactAudioEffectManager {
public:
    // シングルトンインスタンス
    static ArtifactAudioEffectManager& instance();

    // エフェクトファクトリーの登録
    void registerEffectFactory(const std::string& effectId, AudioEffectFactory factory);
    
    // エフェクトインスタンスの作成
    std::unique_ptr<ArtifactAbstractAudioEffect> createEffect(const std::string& effectId);
    
    // 登録済みエフェクトの一覧取得
    std::vector<std::string> getAvailableEffects() const;
    
    // エフェクトの存在確認
    bool hasEffect(const std::string& effectId) const;

private:
    ArtifactAudioEffectManager() = default;
    ~ArtifactAudioEffectManager() = default;
    
    std::unordered_map<std::string, AudioEffectFactory> effectFactories_;
};

} // namespace Artifact