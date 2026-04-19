module;
#include <vector>
#include <QWidget>
#include <QTreeWidgetItem>
#include <QEvent>
#include <wobjectdefs.h>
export module Artifact.Widgets.ProblemViewWidget;

import Core.Diagnostics.ProjectDiagnostic;
import Artifact.Project.Health;

export namespace Artifact {

class ArtifactProject;

class ArtifactProblemViewWidget : public QWidget {
    W_OBJECT(ArtifactProblemViewWidget)

public:
    explicit ArtifactProblemViewWidget(QWidget* parent = nullptr);
    ~ArtifactProblemViewWidget() override;

    void setProject(ArtifactProject* project);
    void refreshFromCurrentProject();
    void loadDiagnostics(const std::vector<ArtifactCore::ProjectDiagnostic>& diagnostics);
    void loadProjectHealth(const ProjectHealthReport& report);

private slots:
    void onRefresh();
    void onNavigateToProblem(QTreeWidgetItem* item, int column);

private:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void rebuildProblemTree();
    void updateSummary(const std::vector<ArtifactCore::ProjectDiagnostic>& diagnostics);

    class Impl;
    Impl* impl_;
};

} // namespace Artifact
