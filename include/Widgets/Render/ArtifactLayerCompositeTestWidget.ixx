module;
#include <QWidget>
#include <QKeyEvent>
#include <wobjectdefs.h>

export module Artifact.Widgets.LayerCompositeTest;

export namespace Artifact {

class ArtifactLayerCompositeTestWidget : public QWidget {
    W_OBJECT(ArtifactLayerCompositeTestWidget)
private:
    class Impl;
    Impl* impl_;
    
protected:
    void keyPressEvent(QKeyEvent* event) override;
    
public:
    explicit ArtifactLayerCompositeTestWidget(QWidget* parent = nullptr);
    ~ArtifactLayerCompositeTestWidget() override;
};

} // namespace Artifact
