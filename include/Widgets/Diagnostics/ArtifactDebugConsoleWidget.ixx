module;
#include <wobjectdefs.h>
#include <QWidget>

export module Artifact.Widgets.DebugConsoleWidget;

export namespace Artifact {

class ArtifactDebugConsoleWidget : public QWidget {
    W_OBJECT(ArtifactDebugConsoleWidget)
public:
    explicit ArtifactDebugConsoleWidget(QWidget* parent = nullptr);
    ~ArtifactDebugConsoleWidget() override;
    int debugConsoleFontPointSize() const;
    void setDebugConsoleFontPointSize(int pointSize);

private:
    class Impl;
    Impl* impl_;
};

} // namespace Artifact
