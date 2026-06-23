module;
#include <wobjectdefs.h>
#include <QWidget>
#include <QString>

export module Artifact.Widgets.Welcome;

export namespace Artifact
{

class ArtifactWelcomeWidget final : public QWidget
{
    W_OBJECT(ArtifactWelcomeWidget)
public:
    explicit ArtifactWelcomeWidget(QWidget* parent = nullptr);
    ~ArtifactWelcomeWidget() override;

    void refreshRecentProjects();

    void openRecentProject(const QString& path) W_SIGNAL(openRecentProject, path);
    void createNewComposition() W_SIGNAL(createNewComposition);
    void importAsset() W_SIGNAL(importAsset);
    void openProject() W_SIGNAL(openProject);

private:
    class Impl;
    Impl* impl_;
};

}
