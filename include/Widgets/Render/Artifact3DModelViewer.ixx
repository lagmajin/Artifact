module;
#include <utility>

#include <wobjectdefs.h>
#include <QWidget>
#include <QShowEvent>
#include <QHideEvent>
#include <QEvent>
#include <QVector3D>
#include <QPainter>
#include <QPaintEvent>
export module Artifact.Widgets.ModelViewer;


import Utils.String.UniString;
import Color.Float;

export namespace Artifact {

class NavHudLabel;

/**
 * @brief 3D Model Viewer Widget
 */
class NavHudLabel : public QWidget {
    W_OBJECT(NavHudLabel)
public:
    explicit NavHudLabel(QWidget* parent = nullptr) : QWidget(parent) {}
    void setText(const QString& text) { text_ = text; update(); }
    const QString& text() const { return text_; }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        QColor bg(0, 0, 0, 160);
        QColor fg(255, 255, 255);
        p.setBrush(bg);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 4, 4);
        p.setPen(fg);
        QFont f = font();
        f.setPointSize(12);
        f.setBold(true);
        p.setFont(f);
        p.drawText(rect().adjusted(10, 4, -10, -4), Qt::AlignLeft | Qt::AlignVCenter, text_);
    }

private:
    QString text_;
};

class Artifact3DModelViewer : public QWidget {
    W_OBJECT(Artifact3DModelViewer)
public:
    enum class DisplayMode {
        Solid,
        Wireframe,
        SolidWithWire
    };
private:
    class Impl;
    Impl* impl_;

public:
    explicit Artifact3DModelViewer(QWidget* parent = nullptr);
    virtual ~Artifact3DModelViewer();

    void loadModel(const ArtifactCore::UniString& filePath);
    void clearModel();
    void resetView();

    void setBackgroundColor(const ArtifactCore::FloatColor& color);
    void setZoom(float factor);
    void setCameraRotation(float yaw, float pitch);
    void setCameraPosition(const QVector3D& position);
    void setDisplayMode(DisplayMode mode);
    void displayModeChanged(int mode) W_SIGNAL(displayModeChanged, mode);
    DisplayMode displayMode() const;
    bool hasModel() const;
    int vertexCount() const;
    int polygonCount() const;
    QString backendName() const;
    QString lastErrorText() const;
    QVector3D meshExtents() const;
    float zoomFactor() const;
    float cameraYaw() const;
    float cameraPitch() const;
    QVector3D cameraPosition() const;

    void requestUpdate();

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
};

}
