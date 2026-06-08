module;
#include <QDialog>
#include <QSize>
#include <QString>
#include <wobjectdefs.h>
export module Artifact.Widgets.ResolutionRemapDialog;

import Geometry.ResolutionRemap;

export namespace Artifact {

class ArtifactResolutionRemapDialog final : public QDialog {
    W_OBJECT(ArtifactResolutionRemapDialog)

public:
    explicit ArtifactResolutionRemapDialog(
        const QSize& oldSize,
        const QSize& newSize,
        const ArtifactCore::RemapImpact& impact,
        QWidget* parent = nullptr);
    ~ArtifactResolutionRemapDialog();

    ArtifactCore::RemapPolicy selectedPolicy() const;
    bool remapRequested() const;

protected:
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void showEvent(QShowEvent*) override;

private:
    class Impl;
    Impl* impl_;
};

}
