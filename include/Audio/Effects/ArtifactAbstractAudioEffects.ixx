module;
export module ArtifactAbstractAudioEffects;

import std;

export namespace Artifact {

// オーディオエフェクトの抽象ベースクラス
class ArtifactAbstractAudioEffects {
public:
    ArtifactAbstractAudioEffects() = default;
    virtual ~ArtifactAbstractAudioEffects() = default;

    // エフェクト名
    virtual std::string name() const = 0;

    // 有効/無効
    virtual void setEnabled(bool enabled) { enabled_ = enabled; }
    virtual bool isEnabled() const { return enabled_; }

    // パラメータ（例: key-value）
    virtual void setParameter(const std::string& key, float value) = 0;
    virtual float getParameter(const std::string& key) const = 0;

    // オーディオ処理（in-place）
    // buffer: float配列, samples: サンプル数, channels: チャンネル数
    virtual void process(float* buffer, int samples, int channels) = 0;

protected:
    bool enabled_ = true;
};

}
