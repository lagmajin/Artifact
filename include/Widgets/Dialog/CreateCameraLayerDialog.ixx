module;
#include <utility>

#include <QDialog>
#include <QWidget>
#include <wobjectdefs.h>
export module Artifact.Widgets.CreateCameraLayerDialog;

import Artifact.Layer.Camera;

export namespace Artifact {

/// カメラ設定ダイアログ
/// docs/image/カメラレイヤー.jpeg に基づく UI
class CreateCameraLayerDialog final : public QDialog {
    W_OBJECT(CreateCameraLayerDialog)

public:
    explicit CreateCameraLayerDialog(QWidget* parent = nullptr);
    ~CreateCameraLayerDialog();

    // Returned settings
    QString cameraName() const;
    float   focalLength() const;   // mm
    float   fov() const;           // degrees
    float   zoom() const;          // px
    float   focusDistance() const;
    float   blurAmount() const;    // 0–100 %
    float   apertureF() const;     // F-number (1.4, 2, 2.8, 4, 5.6, 8 ...)
    bool    depthOfFieldEnabled() const;
    bool    motionBlur() const;
    bool    cameraLocked() const;

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    class Impl;
    Impl* impl_;
};

} // namespace Artifact
