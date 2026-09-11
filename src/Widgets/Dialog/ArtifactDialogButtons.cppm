module;
#include <QHBoxLayout>
#include <QPushButton>
#include <QString>
#include <QWidget>

module Artifact.Widgets.DialogButtons;

import Application.AppSettings;

namespace Artifact {
namespace {

bool shouldAlignDialogButtonsLeft() {
  if (auto* settings = ArtifactCore::ArtifactAppSettings::instance()) {
    return settings->accessibilityDialogButtonAlignment() ==
           QStringLiteral("left");
  }
  return false;
}

} // namespace

DialogButtonRow createDialogButtonRow(QWidget* parent, const QString& okText,
                                      const QString& cancelText,
                                      const QString& applyText,
                                      bool includeApply) {
  auto* rowWidget = new QWidget(parent);
  auto* layout = new QHBoxLayout(rowWidget);
  layout->setContentsMargins(0, 0, 0, 0);

  const bool alignLeft = shouldAlignDialogButtonsLeft();
  if (!alignLeft) {
    // Platform is the default; on Windows the established native placement is right.
    layout->addStretch(1);
  }

  DialogButtonRow row;
  row.widget = rowWidget;
  if (includeApply) {
    row.applyButton = new QPushButton(applyText, rowWidget);
    layout->addWidget(row.applyButton);
  }

  row.okButton = new QPushButton(okText, rowWidget);
  row.cancelButton = new QPushButton(cancelText, rowWidget);
  row.okButton->setDefault(true);
  row.okButton->setAutoDefault(true);
  row.cancelButton->setAutoDefault(false);
  layout->addWidget(row.okButton);
  layout->addWidget(row.cancelButton);

  if (alignLeft) {
    layout->addStretch(1);
  }
  return row;
}

} // namespace Artifact
