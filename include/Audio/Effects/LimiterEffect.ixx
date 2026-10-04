module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>
#include <QtGlobal>

export module Artifact.Audio.Effects.Limiter;

import Audio.Segment;
import Artifact.Audio.Effects.Base;

export namespace Artifact {

// 先読み brick-wall limiter。
//
// アルゴリズム:
//   入力サンプル n を受け取った時点で、ウィンドウ [n-L, n] の peak 最大値を求め、
//   g[n] = min(1, ceiling / windowMax) とする。出力は入力サンプルを L サンプル
//   遅延させたものを使うので、g[n] の計算に必要な情報はすべて既に手元にある。
//   つまり「先読み=L 分の遅延」を実際に付出了うえで、透過的な減衰が得られる。
//
// 性能:
//   peak のスライド最大値は単調減少 deque で求め、元の O(N*L) を O(N) へ落とす。
//   process() 内の一時確保はゼロ。リングと deque は初期化とレート変更時のみ確保する。
class LimiterEffect : public ArtifactAbstractAudioEffect {
public:
    LimiterEffect();
    ~LimiterEffect() override = default;

    void process(ArtifactCore::AudioSegment& segment,
                 const ArtifactCore::AudioSegment* sideChain = nullptr) override;
    String getName() const override { return "Limiter"; }
    // 保存・復元の識別キー。登録済みの id と一致させる。
    String effectType() const override { return "limiter"; }
    String getDescription() const override {
        return "Brick-wall lookahead limiter with transparent gain reduction";
    }

    std::vector<AudioEffectParameter> getUiParameters() const override;
    void setParameter(const String& name, float value) override;
    float getParameter(const String& name) const override;

    void setSampleRate(int sampleRate) override;
    void reinitOnSampleRate() override;

    // 先読み分の遅延。バスは下流をこの分だけずらす。
    qint64 latencySamples() const override {
        return static_cast<qint64>(lookaheadSamples_);
    }
    // Limiter は tail を持たない。release 係数が 1 未満なので減衰は 0 へ収束し、
    // 無限に鳴り続ける残響は生じない。
    qint64 tailSamples() const override { return 0; }

private:
    static constexpr int kMaxLookahead = 512;
    // 遅延リングは lookaheadSamples_ より 1 大きい必要がある。
    // 入力 n を書くと同時に n-L を読むため、n と n-L が同じスロットを
    // 共有しないことが必要（ringSize_ > L）。
    // deque はウィンドウ幅 L+1 個のエントリを保持しうるので +2 を確保する。
    static constexpr int kQueueCapacity = kMaxLookahead + 2;
    // 遅延リングに確保する最大チャンネル数。
    // 超過分は遅延処理を適用せず素通しになる（下限のバッファ契約）。
    static constexpr int kMaxChannels = 32;

    float ceiling_    = -0.3f;
    float releaseMs_  = 50.0f;
    float inputGain_  = 0.0f;

    int lookaheadSamples_ = 64;
    int ringSize_ = 0;

    // 遅延線。チャンネル優先で [ch * ringSize_ + ringPos] にアクセスする。
    std::vector<float> delayRing_;
    // 累積書き込みサンプル数。リング位置は writePos_ % ringSize_ で求める。
    qint64 writePos_ = 0;

    // 単調減少 deque。values が末尾方向に単調減少하도록保ち、
    // 先頭が常にスライド窓の最大値を与える。
    qint64 queueIndex_[kQueueCapacity] = {};
    float queuePeak_[kQueueCapacity] = {};
    int queueFront_ = 0;
    int queueBack_  = 0;

    float currentGain_ = 1.0f;

    void initializeEngine();
    void pushPeak(qint64 index, float peak);
    float windowPeak(qint64 windowStart);
};

std::unique_ptr<ArtifactAbstractAudioEffect> createLimiterEffect();

} // namespace Artifact
