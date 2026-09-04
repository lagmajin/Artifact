module;
#include <QtGlobal>

class QAbstractScrollArea;
class QColor;
class QGroupBox;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QString;
class QWidget;

export module Artifact.Widgets.InspectorStyle;

export namespace Artifact {

QColor themeColor(const QString& value, const QColor& fallback);
QColor blendColor(const QColor& a, const QColor& b, qreal t);

void applyInspectorPalette(QWidget* widget, bool elevated = false);
void applyInspectorLabelPalette(QLabel* label, bool prominent = false);
void applyInspectorSectionBox(QGroupBox* box);
void applyInspectorTextEdit(QPlainTextEdit* edit);
void applyInspectorList(QListWidget* list);
void applyInspectorButton(QPushButton* button, bool accent = false);
void applyInspectorComponentStateButton(QPushButton* button, bool active);
void applyInspectorOwnerDrawScrollBars(QAbstractScrollArea* area);

} // namespace Artifact
