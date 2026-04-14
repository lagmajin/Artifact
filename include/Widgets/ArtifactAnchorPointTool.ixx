module;
#include <QWidget>
#include <QPushButton>
#include <QButtonGroup>
#include <wobjectdefs.h>
export module Artifact.Widgets.AnchorPointTool;

import Artifact.Layer.Abstract;

export namespace Artifact {

enum class AnchorPoint {
    TopLeft, TopCenter, TopRight,
    MiddleLeft, Center, MiddleRight,
    BottomLeft, BottomCenter, BottomRight
};

class ArtifactAnchorPointTool : public QWidget {
    W_OBJECT(ArtifactAnchorPointTool)

public:
    explicit ArtifactAnchorPointTool(QWidget* parent = nullptr);
    ~ArtifactAnchorPointTool() override;

private slots:
    void onAnchorPointClicked(int id);
    void onApplyToSelected();

private:
    void applyToCurrentSelection(AnchorPoint anchorPoint);
    void moveAnchorPoint(ArtifactAbstractLayerPtr layer, AnchorPoint anchorPoint);

    class Impl;
    Impl* impl_;
};

} // namespace Artifact
