module;
#include <vector>
#include <QWidget>
#include <QTreeWidgetItem>
#include <wobjectdefs.h>
export module Artifact.Widgets.ProblemViewWidget;

import Core.Diagnostics.ProjectDiagnostic;

export namespace Artifact {

class ArtifactProblemViewWidget : public QWidget {
    W_OBJECT(ArtifactProblemViewWidget)

public:
    explicit ArtifactProblemViewWidget(QWidget* parent = nullptr);
    ~ArtifactProblemViewWidget() override;

    void loadDiagnostics(const std::vector<ArtifactCore::ProjectDiagnostic>& diagnostics);

private slots:
    void onRefresh();
    void onNavigateToProblem(QTreeWidgetItem* item, int column);

private:
    void updateSummary(const std::vector<ArtifactCore::ProjectDiagnostic>& diagnostics);

    class Impl;
    Impl* impl_;
};

} // namespace Artifact
