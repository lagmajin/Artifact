module;
#include <QDialog>
#include <QStringList>
#include <QWidget>

export module Artifact.Widgets.ImportAssetsDialog;

import File.TypeDetector;

export namespace Artifact {

class ArtifactImportAssetsDialog final : public QDialog {
public:
  explicit ArtifactImportAssetsDialog(const QStringList& files, QWidget* parent = nullptr);

  QStringList selectedPaths() const;
};

} // namespace Artifact
