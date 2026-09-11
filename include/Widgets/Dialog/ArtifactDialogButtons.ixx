module;
#include <QString>

class QWidget;
class QPushButton;

export module Artifact.Widgets.DialogButtons;

export namespace Artifact {

struct DialogButtonRow {
  QWidget* widget = nullptr;
  QPushButton* okButton = nullptr;
  QPushButton* cancelButton = nullptr;
  QPushButton* applyButton = nullptr;
};

// Keeps the button order and existing click wiring intact while applying the
// persisted placement preference to the complete action row.
DialogButtonRow createDialogButtonRow(QWidget* parent,
                                      const QString& okText = QStringLiteral("OK"),
                                      const QString& cancelText = QStringLiteral("Cancel"),
                                      const QString& applyText = QStringLiteral("Apply"),
                                      bool includeApply = false);

} // namespace Artifact
