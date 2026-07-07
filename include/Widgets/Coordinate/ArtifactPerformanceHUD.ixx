module;
#include <utility>

#include <wobjectdefs.h>
#include <QObject>

export module Artifact.Widgets.PerformanceHUD;

export namespace Artifact {

// Forward declaration (imported by .cppm)
class CompositionRenderController;

/// C4D風ビューポート HUD
/// FPS / レイヤー数 / メモリ / ズーム率をビューポート隅に常時表示する。
/// 既存の CompositionRenderController::setInfoOverlayText を利用するため、
/// コントローラの変更は不要。
class ArtifactPerformanceHUD : public QObject
{
    W_OBJECT(ArtifactPerformanceHUD)
private:
    class Impl;
    Impl* impl_;

public:
    explicit ArtifactPerformanceHUD(QObject* parent = nullptr);
    ~ArtifactPerformanceHUD() override;

    /// 対象のレンダーコントローラを設定（必須）
    void setController(CompositionRenderController* controller);

    /// HUD の表示/非表示
    void setEnabled(bool enabled);
    bool isEnabled() const;
};

} // namespace Artifact
