module;

#include <functional>
#include <QStringList>
#include <QString>

class QListWidget;
class QListWidgetItem;
class QWidget;

export module Artifact.Widgets.InspectorInteraction;

export namespace Artifact {

QListWidget* createInspectorSelectionList(QWidget* parent = nullptr);
void setInspectorSelectionAction(
    QListWidget* list, std::function<void(QListWidgetItem*)> action);
void setInspectorSelectionActionEnabled(QListWidget* list, bool enabled);
QListWidget* createInspectorEffectRackList(QWidget* parent = nullptr);
void setInspectorEffectRackReorderHandler(
    QListWidget* list, std::function<void(const QStringList&, int)> handler);

// The editor is borrowed; it is never owned by a list item.
void setInspectorEffectRackEditor(QListWidget* list, QWidget* editor,
                                 const QString& effectId);
void setInspectorEffectRackHeaderAction(
    QListWidget* list, std::function<void(QListWidgetItem*, bool)> action);

} // namespace Artifact
