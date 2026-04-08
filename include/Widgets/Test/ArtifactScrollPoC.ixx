module;
#include <utility>

#include <wobjectdefs.h>
#include <QWidget>
#include <QScrollBar>
export module Artifact.Widgets.Test.ScrollPoC;


export namespace Artifact {

/**
 * @brief QAbstractScrollArea を使わない、独自スクロール実装の PoC
 */
class ArtifactScrollPoCWidget : public QWidget {
    W_OBJECT(ArtifactScrollPoCWidget)
public:
    explicit ArtifactScrollPoCWidget(QWidget* parent = nullptr);
    ~ArtifactScrollPoCWidget() override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    QScrollBar* verticalScrollBar_;
    int contentHeight_ = 2000; // ダミーの高さ
};

} // namespace Artifact
