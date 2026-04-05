module;
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QToolBar>
#include <QAction>
#include <QComboBox>
#include <QScreen>
#include <QGuiApplication>
#include <QTimer>
#include <QImage>
#include <QPainter>
#include <QKeyEvent>
#include <QApplication>
#include <QStatusBar>
#include <QStyle>
#include <wobjectdefs.h>

export module Artifact.Widgets.SecondaryPreviewWindow;

export namespace Artifact {

class ArtifactSecondaryPreviewWindow : public QWidget {
    W_OBJECT(ArtifactSecondaryPreviewWindow)
private:
    class Impl;
    Impl* impl_;

public:
    explicit ArtifactSecondaryPreviewWindow(QWidget* parent = nullptr);
    ~ArtifactSecondaryPreviewWindow();

    // 表示する画像を更新
    void updatePreviewImage(const QImage& image);

    // フレーム情報を更新
    void updateFrameInfo(int64_t frame, int64_t totalFrames, const QString& compName);

    // 画面を選択して表示
    void showOnScreen(int screenIndex);

    // フルスクリーンモード切替
    void setFullscreen(bool fullscreen);
    bool isFullscreen() const;

    // 自動更新の有効/無効
    void setAutoUpdate(bool enabled);
    bool autoUpdate() const;

    // 更新レートの設定（fps）
    void setUpdateRate(int fps);
    int updateRate() const;

    signals:
    void closed() W_SIGNAL(closed);
    void fullscreenToggled(bool enabled) W_SIGNAL(fullscreenToggled, enabled);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
};

} // namespace Artifact
