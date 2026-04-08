module;
#include <utility>

#include <wobjectdefs.h>
#include <QWidget>
#include <QVector3D>
export module Artifact.Widgets.ModelViewer;


import Utils.String.UniString;
import Color.Float;

export namespace Artifact {

/**
 * @brief 3D Model Viewer Widget
 */
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
};

}
