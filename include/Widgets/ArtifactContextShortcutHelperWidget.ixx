module;
#include <QWidget>
#include <wobjectdefs.h>

export module Artifact.Widgets.ContextShortcutHelperWidget;

import Artifact.Workspace.Modes;

export namespace Artifact {

class ArtifactContextShortcutHelperWidget : public QWidget {
    W_OBJECT(ArtifactContextShortcutHelperWidget)
public:
    explicit ArtifactContextShortcutHelperWidget(QWidget *parent = nullptr);
    ~ArtifactContextShortcutHelperWidget();

    void setWorkspaceMode(WorkspaceMode mode);

private:
    class Impl;
    Impl *impl_;
};

} // namespace Artifact
