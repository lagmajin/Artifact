module;
#include <utility>
#include <QDialog>
#include <QEvent>
#include <QString>
#include <QWidget>

#include <wobjectdefs.h>
export module Artifact.Widgets.ColorSciencePanel;

import Color.ScienceManager;

export namespace Artifact {

class ArtifactLutColorReferencePickerDialog final : public QDialog {
public:
  explicit ArtifactLutColorReferencePickerDialog(
      ArtifactColorScienceManager* manager, QWidget* parent = nullptr);

  QString selectedSource() const;

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;
};

class ArtifactColorSciencePanel : public QWidget {
  W_OBJECT(ArtifactColorSciencePanel)

private:
  class Impl;
  Impl *impl_;

public:
  explicit ArtifactColorSciencePanel(QWidget *parent = nullptr);
  ~ArtifactColorSciencePanel();

  ArtifactColorScienceManager *colorScienceManager() const;
};

} // namespace Artifact
