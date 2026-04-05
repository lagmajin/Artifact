#pragma once

#include <QHBoxLayout>
#include <QPushButton>
#include <QString>
#include <QWidget>

namespace Artifact
{

struct DialogButtonRow
{
  QWidget* widget = nullptr;
  QPushButton* okButton = nullptr;
  QPushButton* cancelButton = nullptr;
  QPushButton* applyButton = nullptr;
};

inline DialogButtonRow createWindowsDialogButtonRow(QWidget* parent,
                                                    const QString& okText = QStringLiteral("OK"),
                                                    const QString& cancelText = QStringLiteral("Cancel"),
                                                    const QString& applyText = QStringLiteral("Apply"),
                                                    bool includeApply = false)
{
  auto* rowWidget = new QWidget(parent);
  auto* layout = new QHBoxLayout(rowWidget);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addStretch(1);

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

  return row;
}

} // namespace Artifact
