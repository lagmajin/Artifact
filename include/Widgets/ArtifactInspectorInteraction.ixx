module;

#include <functional>
#include <QStringList>

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

} // namespace Artifact
