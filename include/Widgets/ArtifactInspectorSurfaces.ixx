module;

#include <functional>

class QFrame;
class QLabel;
class QObject;
class QPushButton;
class QString;
class QStyledItemDelegate;
class QWidget;

export module Artifact.Widgets.InspectorSurfaces;

export namespace Artifact {

enum class InspectorChromeLabelRole { Section, Active, Summary };
enum class InspectorEffectPanelRole { Header, Stack, Detail };

QPushButton* createInspectorActionButton(const QString& text,
                                          QWidget* parent = nullptr);
void setInspectorButtonOwnerDrawn(QPushButton* button, bool enabled);
void setInspectorButtonAction(QPushButton* button, std::function<void()> action);
void triggerInspectorButtonAction(QPushButton* button);

QLabel* createInspectorChromeLabel(const QString& text,
                                   InspectorChromeLabelRole role,
                                   QWidget* parent = nullptr);
QWidget* createInspectorCanvasSurface(QWidget* parent = nullptr);
QWidget* createInspectorPropertySurface(QWidget* editor,
                                         QWidget* parent = nullptr);
void setInspectorPropertySurfaceEditor(QWidget* surface, QWidget* editor);
QFrame* createInspectorDivider(QWidget* parent = nullptr);
QStyledItemDelegate* createInspectorComponentStackItemDelegate(QObject* parent);
QWidget* createInspectorEffectPanelSurface(InspectorEffectPanelRole role,
                                            QWidget* parent = nullptr);
QWidget* createInspectorEffectRackSurface(const QString& title,
                                          QWidget* parent = nullptr);
void setInspectorEffectRackSurfaceTitle(QWidget* surface, const QString& title);

} // namespace Artifact
