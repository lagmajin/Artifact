module;
#include <QDialog>
#include <QHBoxLayout>
#include <QVBoxLayout>

export module Artifact.Widgets.Dialog.CompositionShell;

export namespace Artifact {
class ArtifactCompositionDialogShell final : public QDialog {
 public:
  explicit ArtifactCompositionDialogShell(QWidget* parent = nullptr);
  QVBoxLayout* contentLayout() const;
  QHBoxLayout* footerLayout() const;
 private:
  QVBoxLayout* content_ = nullptr;
  QHBoxLayout* footer_ = nullptr;
};
}
