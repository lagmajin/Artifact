module;
#include <QWidget>
#include <QTabWidget>
#include <wobjectdefs.h>
export module Artifact.Widgets.QuickToolBox;

export namespace Artifact {

class ArtifactQuickToolBox : public QWidget {
    W_OBJECT(ArtifactQuickToolBox)

public:
    explicit ArtifactQuickToolBox(QWidget* parent = nullptr);
    ~ArtifactQuickToolBox() override;

private:
    class Impl;
    Impl* impl_;
};

} // namespace Artifact
