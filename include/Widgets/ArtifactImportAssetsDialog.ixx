module;
#include <QDialog>
#include <QEvent>
#include <QStringList>
#include <QWidget>

export module Artifact.Widgets.ImportAssetsDialog;

import File.TypeDetector;

export namespace Artifact {

class ArtifactMediaImportPickerDialog final : public QDialog {
public:
  explicit ArtifactMediaImportPickerDialog(QWidget* parent = nullptr);
  QStringList selectedPaths() const;

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;
};

class ArtifactProjectOpenPickerDialog final : public QDialog {
public:
  explicit ArtifactProjectOpenPickerDialog(const QStringList& recentProjects,
                                           QWidget* parent = nullptr);
  QString selectedPath() const;

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;
};

class ArtifactImportAssetsDialog final : public QDialog {
public:
  explicit ArtifactImportAssetsDialog(const QStringList& files, QWidget* parent = nullptr);

  QStringList selectedPaths() const;
};

} // namespace Artifact
