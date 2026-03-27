module;
#include <wobjectdefs.h>
#include <QWidget>

export module Artifact.Widgets.ConsoleWidget;

export namespace Artifact {

class ArtifactConsoleWidget : public QWidget {
    W_OBJECT(ArtifactConsoleWidget)
public:
    explicit ArtifactConsoleWidget(QWidget* parent = nullptr);
    ~ArtifactConsoleWidget() override;
    int consoleFontPointSize() const;
    void setConsoleFontPointSize(int pointSize);

private:
    class Impl;
    Impl* impl_;
};

} // namespace Artifact
