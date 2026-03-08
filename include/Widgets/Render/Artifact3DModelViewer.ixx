module;
#include <QWidget>
#include <QVector3D>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPaintEvent>
#include <wobjectdefs.h>

export module Artifact.Widgets.ModelViewer;

import Utils.String.UniString;
import Color.Float;

export namespace Artifact {

/**
 * @brief 3D Model Viewer Widget
 */
class Artifact3DModelViewer : public QWidget {
    W_OBJECT(Artifact3DModelViewer)
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

    void requestUpdate();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
};

}
