module;
#include <QObject>
#include <QVector2D>
#include <QVector3D>
#include <QColor>
#include <QImage>
#include <QEasingCurve>
#include <memory>
#include <wobjectdefs.h>

export module Artifact.Effect.Transition;

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




W_REGISTER_ARGTYPE(QVector2D)
W_REGISTER_ARGTYPE(QVector3D)
W_REGISTER_ARGTYPE(QEasingCurve::Type)

export namespace Artifact {

/**
 * @brief Transition types available in the system
 */
enum class TransitionType {
    // Basic transitions
    CrossDissolve,      // フェードイン/アウト
    Fade,               // 黒/白へのフェード
    DipToBlack,         // 黒経由のトランジション
    DipToWhite,         // 白経由のトランジション
    
    // Wipe transitions
    WipeLeft,           // 左からワイプ
    WipeRight,          // 右からワイプ
    WipeUp,             // 上からワイプ
    WipeDown,           // 下からワイプ
    WipeRadial,         // 放射状ワイプ
    WipeClock,          // 時計回りワイプ
    WipeDiamond,        // ダイヤモンドワイプ
    
    // Slide transitions
    SlideLeft,          // 左にスライド
    SlideRight,         // 右にスライド
    SlideUp,            // 上にスライド
    SlideDown,          // 下にスライド
    PushLeft,           // 左にプッシュ
    PushRight,          // 右にプッシュ
    PushUp,             // 上にプッシュ
    PushDown,           // 下にプッシュ
    
    // Zoom transitions
    ZoomIn,             // ズームイン
    ZoomOut,            // ズームアウト
    ZoomRotate,         // ズーム回転
    
    // Creative transitions
    Spin,               // 回転
    SpinZoom,           // 回転ズーム
    FlipX,              // X軸フリップ
    FlipY,              // Y軸フリップ
    CubeRotate,         // キューブ回転
    PageCurl,           // ページカール
    RippleDissolve,     // 波紋ディゾルブ
    Glitch,             // グリッチ
    Pixelate,           // ピクセレート
    LightFlash,         // ライトフラッシュ
    
    // Mask-based
    RadialMask,         // 放射状マスク
    LinearMask,         // 線形マスク
    ShapeMask,          // シェイプマスク
    LumaMatte,         // 輝度マスク
    
    // Custom
    Custom              // カスタムマスク使用
};

/**
 * @brief Base transition parameters
 */
class TransitionParams {
public:
    float duration = 1.0f;              // トランジション時間（秒）
    float position = 0.0f;              // トランジション位置（0-1）
    QEasingCurve::Type easing = QEasingCurve::Type::Linear;
    
    // Border settings
    QColor borderColor = QColor(0, 0, 0);
    float borderWidth = 0.0f;
    float feather = 0.0f;               // エッジのぼかし
    
    // Advanced
    bool reverse = false;               // 逆再生
    int customWidth = 0;                // カスタム幅
    int customHeight = 0;               // カスタム高さ
};

/**
 * @brief Wipe-specific parameters
 */
class WipeParams : public TransitionParams {
public:
    float angle = 0.0f;                 // 角度（度）
    float softness = 0.1f;              // エッジの柔らかさ
    bool invert = false;                // 方向反転
    QVector2D origin = QVector2D(0.5f, 0.5f);  // 中心点
    float innerRadius = 0.0f;           // 内側半径（放射状用）
};

/**
 * @brief Slide-specific parameters
 */
class SlideParams : public TransitionParams {
public:
    float distance = 1.0f;              // 移動距離（画面幅比）
    bool push = false;                  // プッシュモード
    float scale = 1.0f;                 // スケール
    float rotation = 0.0f;              // 回転角度
    float opacity = 1.0f;               // 不透明度
};

/**
 * @brief Zoom-specific parameters
 */
class ZoomParams : public TransitionParams {
public:
    float startScale = 1.0f;            // 開始スケール
    float endScale = 2.0f;              // 終了スケール
    QVector2D center = QVector2D(0.5f, 0.5f);  // 中心点
    float rotation = 0.0f;              // 追加回転
    bool reverseZoom = false;           // ズーム反転
};

/**
 * @brief Glitch transition parameters
 */
class GlitchParams : public TransitionParams {
public:
    float intensity = 0.5f;             // グリッチ強度
    float blockSize = 16.0f;            // ブロックサイズ
    float colorSeparation = 10.0f;      // 色分離量
    float noiseAmount = 0.3f;           // ノイズ量
    bool horizontalGlitch = true;       // 水平グリッチ
    bool verticalGlitch = false;        // 垂直グリッチ
    int seed = 0;                       // ランダムシード
};

/**
 * @brief Page curl parameters
 */
class PageCurlParams : public TransitionParams {
public:
    float angle = 45.0f;                // カール角度
    float radius = 50.0f;               // カール半径
    bool flipBack = false;              // 裏面を表示
    QColor backColor = QColor(200, 200, 200);  // 裏面色
    float shadow = 0.5f;                // 影の強さ
};

/**
 * @brief Ripple dissolve parameters
 */
class RippleParams : public TransitionParams {
public:
    float amplitude = 20.0f;            // 波の振幅
    float frequency = 10.0f;            // 波の周波数
    float speed = 1.0f;                 // 波の速度
    float distortion = 0.5f;            // 歪み量
    QVector2D center = QVector2D(0.5f, 0.5f);
};

/**
 * @brief Abstract transition base class
 */
class AbstractTransition : public QObject {
    W_OBJECT(AbstractTransition)
protected:
    TransitionParams params_;
    TransitionType type_;
    bool enabled_ = true;
    
public:
    explicit AbstractTransition(TransitionType type, QObject* parent = nullptr);
    virtual ~AbstractTransition();
    
    // Process transition between two frames
    // progress: 0.0 = from frame, 1.0 = to frame
    virtual void process(const QImage& fromFrame,
                        const QImage& toFrame,
                        QImage& output,
                        float progress) = 0;
    
    // GPU variant
    virtual void processGPU(const float* fromPixels,
                           const float* toPixels,
                           float* outputPixels,
                           int width, int height,
                           float progress);
    
    // Parameters
    TransitionParams& params() { return params_; }
    const TransitionParams& params() const { return params_; }
    void setParams(const TransitionParams& p) { params_ = p; emit changed(); }
    
    TransitionType type() const { return type_; }
    
    bool enabled() const { return enabled_; }
    void setEnabled(bool e) { enabled_ = e; emit changed(); }
    
    // Easing
    float applyEasing(float progress) const;
    
signals:
    void changed() W_SIGNAL(changed);
    void progressChanged(float progress) W_SIGNAL(progressChanged, progress);
};

/**
 * @brief Cross dissolve transition
 */
class CrossDissolveTransition : public AbstractTransition {
    W_OBJECT(CrossDissolveTransition)
public:
    explicit CrossDissolveTransition(QObject* parent = nullptr);
    ~CrossDissolveTransition() override;
    
    void process(const QImage& fromFrame,
                const QImage& toFrame,
                QImage& output,
                float progress) override;
};

/**
 * @brief Wipe transition
 */
class WipeTransition : public AbstractTransition {
    W_OBJECT(WipeTransition)
private:
    WipeParams wipeParams_;
    
public:
    explicit WipeTransition(TransitionType type, QObject* parent = nullptr);
    ~WipeTransition() override;
    
    void process(const QImage& fromFrame,
                const QImage& toFrame,
                QImage& output,
                float progress) override;
    
    WipeParams& wipeParams() { return wipeParams_; }
    void setWipeParams(const WipeParams& p) { wipeParams_ = p; emit changed(); }
    
    // Calculate wipe mask
    float calculateWipeMask(float x, float y, float progress) const;
};

/**
 * @brief Slide transition
 */
class SlideTransition : public AbstractTransition {
    W_OBJECT(SlideTransition)
private:
    SlideParams slideParams_;
    
public:
    explicit SlideTransition(TransitionType type, QObject* parent = nullptr);
    ~SlideTransition() override;
    
    void process(const QImage& fromFrame,
                const QImage& toFrame,
                QImage& output,
                float progress) override;
    
    SlideParams& slideParams() { return slideParams_; }
    void setSlideParams(const SlideParams& p) { slideParams_ = p; emit changed(); }
};

/**
 * @brief Zoom transition
 */
class ZoomTransition : public AbstractTransition {
    W_OBJECT(ZoomTransition)
private:
    ZoomParams zoomParams_;
    
public:
    explicit ZoomTransition(TransitionType type, QObject* parent = nullptr);
    ~ZoomTransition() override;
    
    void process(const QImage& fromFrame,
                const QImage& toFrame,
                QImage& output,
                float progress) override;
    
    ZoomParams& zoomParams() { return zoomParams_; }
    void setZoomParams(const ZoomParams& p) { zoomParams_ = p; emit changed(); }
};

/**
 * @brief Glitch transition
 */
class GlitchTransition : public AbstractTransition {
    W_OBJECT(GlitchTransition)
private:
    GlitchParams glitchParams_;
    
public:
    explicit GlitchTransition(QObject* parent = nullptr);
    ~GlitchTransition() override;
    
    void process(const QImage& fromFrame,
                const QImage& toFrame,
                QImage& output,
                float progress) override;
    
    GlitchParams& glitchParams() { return glitchParams_; }
    void setGlitchParams(const GlitchParams& p) { glitchParams_ = p; emit changed(); }
};

/**
 * @brief Page curl transition
 */
class PageCurlTransition : public AbstractTransition {
    W_OBJECT(PageCurlTransition)
private:
    PageCurlParams curlParams_;
    
public:
    explicit PageCurlTransition(QObject* parent = nullptr);
    ~PageCurlTransition() override;
    
    void process(const QImage& fromFrame,
                const QImage& toFrame,
                QImage& output,
                float progress) override;
    
    PageCurlParams& curlParams() { return curlParams_; }
    void setCurlParams(const PageCurlParams& p) { curlParams_ = p; emit changed(); }
};

/**
 * @brief Ripple dissolve transition
 */
class RippleTransition : public AbstractTransition {
    W_OBJECT(RippleTransition)
private:
    RippleParams rippleParams_;
    
public:
    explicit RippleTransition(QObject* parent = nullptr);
    ~RippleTransition() override;
    
    void process(const QImage& fromFrame,
                const QImage& toFrame,
                QImage& output,
                float progress) override;
    
    RippleParams& rippleParams() { return rippleParams_; }
    void setRippleParams(const RippleParams& p) { rippleParams_ = p; emit changed(); }
};

/**
 * @brief Transition factory - creates transitions by type
 */
class TransitionFactory {
public:
    static std::unique_ptr<AbstractTransition> create(TransitionType type);
    static QStringList availableTransitions();
    static QString transitionName(TransitionType type);
    static QString transitionDisplayName(TransitionType type);
};

/**
 * @brief Transition presets
 */
class TransitionPresets {
public:
    // Basic presets
    static CrossDissolveTransition* quickDissolve();
    static CrossDissolveTransition* slowFade();
    static WipeTransition* smoothWipeLeft();
    static WipeTransition* smoothWipeRight();
    static SlideTransition* pushLeft();
    static SlideTransition* pushRight();
    
    // Creative presets
    static ZoomTransition* cinematicZoom();
    static ZoomTransition* spinOut();
    static GlitchTransition* digitalGlitch();
    static PageCurlTransition* documentFlip();
    static RippleTransition* waterRipple();
    
    // Quick presets (short duration)
    static CrossDissolveTransition* quickCut();
    static WipeTransition* quickWipe();
    static SlideTransition* quickSlide();
};

/**
 * @brief Transition manager - handles transition effects for compositions
 */
class TransitionManager : public QObject {
    W_OBJECT(TransitionManager)
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    
public:
    explicit TransitionManager(QObject* parent = nullptr);
    ~TransitionManager();
    
    // Register transition
    void registerTransition(const QString& name, std::unique_ptr<AbstractTransition> transition);
    
    // Get transition
    AbstractTransition* transition(const QString& name) const;
    
    // Apply transition to layer
    void applyTransition(const QString& name,
                        const QImage& fromFrame,
                        const QImage& toFrame,
                        QImage& output,
                        float progress);
    
    // Available transitions
    QStringList availableTransitions() const;
    
    // Default transition
    QString defaultTransition() const;
    void setDefaultTransition(const QString& name);
    
signals:
    void transitionApplied(const QString& name) W_SIGNAL(transitionApplied, name);
    void transitionListChanged() W_SIGNAL(transitionListChanged);
};

} // namespace Artifact