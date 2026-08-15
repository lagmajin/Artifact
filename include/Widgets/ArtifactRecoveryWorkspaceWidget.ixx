module;
#include <QWidget>
#include <QString>
#include <wobjectdefs.h>

export module Artifact.Widgets.RecoveryWorkspace;

export namespace Artifact {

class ArtifactRecoveryWorkspaceWidget : public QWidget {
  W_OBJECT(ArtifactRecoveryWorkspaceWidget)
public:
  explicit ArtifactRecoveryWorkspaceWidget(const QString& ledgerPath,
                                            QWidget* parent = nullptr);
  ~ArtifactRecoveryWorkspaceWidget() override;

  void refreshLedger();

private:
  class Impl;
  Impl* impl_;
};

}
