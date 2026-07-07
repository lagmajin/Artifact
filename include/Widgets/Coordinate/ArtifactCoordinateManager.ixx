module;
#include <utility>

#include <wobjectdefs.h>
#include <QWidget>

export module Artifact.Widgets.CoordinateManager;

export namespace Artifact {

/// C4D風 座標マネージャー
/// 選択レイヤーの Position / Rotation / Scale を数値で表示・編集し、
/// World / Local 座標系を切り替えられるコンパクトなフローティングパネル。
class ArtifactCoordinateManagerWidget : public QWidget
{
    W_OBJECT(ArtifactCoordinateManagerWidget)
private:
    class Impl;
    Impl* impl_;

protected:
    void showEvent(QShowEvent* event) override;

public:
    explicit ArtifactCoordinateManagerWidget(QWidget* parent = nullptr);
    ~ArtifactCoordinateManagerWidget() override;

    enum class Space {
        World,
        Local
    };

    void setSpace(Space space);
    Space space() const;
    void toggleSpace();

    /// 現在の選択レイヤーから値を読み直す
    void refreshFromSelection();

public: // signals (no new signal-slot wiring per AGENTS.md; use EventBus)
    // (internal only)
};

} // namespace Artifact
