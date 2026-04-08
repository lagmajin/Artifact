module;
#include <utility>

#include <QWidget>
#include <wobjectdefs.h>
export module Artifact.Widgets.SoftwareRenderTest;


export namespace Artifact {

class ArtifactSoftwareRenderTestWidget : public QWidget {
    W_OBJECT(ArtifactSoftwareRenderTestWidget)
public:
    explicit ArtifactSoftwareRenderTestWidget(QWidget* parent = nullptr);
    ~ArtifactSoftwareRenderTestWidget() override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    class Impl;
    Impl* impl_ = nullptr;
};

} // namespace Artifact

