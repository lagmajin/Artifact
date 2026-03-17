module;
#include <QObject>
#include <QThread>
#include <QImage>
#include <QMutex>
#include <QWaitCondition>
#include <QAtomicInt>
#include <chrono>
#include <atomic>
#include <functional>
#include <wobjectdefs.h>

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

export module Artifact.Playback.Engine;

import Frame.Position;
import Frame.Rate;
import Frame.Range;
import Artifact.Composition.Abstract;

W_REGISTER_ARGTYPE(QImage)
W_REGISTER_ARGTYPE(ArtifactCore::FramePosition)
W_REGISTER_ARGTYPE(ArtifactCore::FrameRange)
W_REGISTER_ARGTYPE(ArtifactCore::FrameRate)

export namespace Artifact {

using namespace ArtifactCore;

/// 再生スレッドエンジン
/// 専用スレッドで高精度な再生制御を行う
class ArtifactPlaybackEngine : public QObject {
    W_OBJECT(ArtifactPlaybackEngine)
private:
    class Impl;
    Impl* impl_;

public:
    explicit ArtifactPlaybackEngine(QObject* parent = nullptr);
    ~ArtifactPlaybackEngine();

    // 設定
    void setFrameRate(const FrameRate& rate);
    FrameRate frameRate() const;

    void setFrameRange(const FrameRange& range);
    FrameRange frameRange() const;

    void setPlaybackSpeed(float speed);  // 1.0=通常，0.5=半速，2.0=倍速，-1.0=逆再生
    float playbackSpeed() const;

    void setLooping(bool loop);
    bool isLooping() const;

    // 再生制御
    void play();
    void pause();
    void stop();
    void togglePlayPause();

    // フレームナビゲーション
    void goToFrame(const FramePosition& position);
    void goToNextFrame();
    void goToPreviousFrame();
    void goToStartFrame();
    void goToEndFrame();

    // マーカー/チャプター移動
    void goToNextMarker();
    void goToPreviousMarker();
    void goToNextChapter();
    void goToPreviousChapter();

    // 状態取得
    bool isPlaying() const;
    bool isPaused() const;
    bool isStopped() const;

    FramePosition currentFrame() const;
    void setCurrentFrame(const FramePosition& position);

    // In/Out Points
    void setInOutPoints(class ArtifactInOutPoints* inOutPoints);
    class ArtifactInOutPoints* inOutPoints() const;

    // オーディオクロック同期
    void setAudioClockProvider(const std::function<double()>& provider);

    // コンプポジション設定
    void setComposition(ArtifactCompositionPtr composition);
    ArtifactCompositionPtr composition() const;

public: // signals
    void frameChanged(const FramePosition& position, const QImage& frame)
        W_SIGNAL(frameChanged, position, frame);
    void playbackStateChanged(bool playing, bool paused, bool stopped)
        W_SIGNAL(playbackStateChanged, playing, paused, stopped);
    void playbackSpeedChanged(float speed)
        W_SIGNAL(playbackSpeedChanged, speed);
    void loopingChanged(bool loop)
        W_SIGNAL(loopingChanged, loop);
    void frameRangeChanged(const FrameRange& range)
        W_SIGNAL(frameRangeChanged, range);
    void droppedFrameDetected(int64_t count)
        W_SIGNAL(droppedFrameDetected, count);
};

} // namespace Artifact
