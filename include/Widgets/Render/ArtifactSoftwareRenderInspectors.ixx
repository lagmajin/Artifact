module;
#include <QWidget>
#include <wobjectdefs.h>

export module Artifact.Widgets.SoftwareRenderInspectors;

export namespace Artifact {

class ArtifactSoftwareCompositionTestWidget : public QWidget {
    W_OBJECT(ArtifactSoftwareCompositionTestWidget)
public:
    explicit ArtifactSoftwareCompositionTestWidget(QWidget* parent = nullptr);
    ~ArtifactSoftwareCompositionTestWidget() override;

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    class Impl;
    Impl* impl_ = nullptr;
};

class ArtifactSoftwareLayerTestWidget : public QWidget {
    W_OBJECT(ArtifactSoftwareLayerTestWidget)
public:
    explicit ArtifactSoftwareLayerTestWidget(QWidget* parent = nullptr);
    ~ArtifactSoftwareLayerTestWidget() override;

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    class Impl;
    Impl* impl_ = nullptr;
};

} // namespace Artifact
