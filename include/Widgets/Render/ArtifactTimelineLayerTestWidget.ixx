module;
#include <QWidget>
#include <QKeyEvent>

export module Artifact.Widgets.TimelineLayerTest;

export namespace Artifact {

/// タイムラインのレイヤーを使った合成テストウィジェット
/// 実際のコンポジションのレイヤーの透明度、ブレンドモード、可視状態をテスト可能
class ArtifactTimelineLayerTestWidget : public QWidget {
    W_OBJECT(ArtifactTimelineLayerTestWidget)
private:
    class Impl;
    Impl* impl_;
    
protected:
    void keyPressEvent(QKeyEvent* event) override;
    
public:
    explicit ArtifactTimelineLayerTestWidget(QWidget* parent = nullptr);
    ~ArtifactTimelineLayerTestWidget() override;
};

} // namespace Artifact
