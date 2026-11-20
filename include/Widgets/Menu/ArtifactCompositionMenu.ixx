module;
#include <utility>

#include <QtCore/QScopedPointer>
#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtWidgets/QtWidgets>
#include <Qtwidgets/QMenu>
#include <wobjectdefs.h>
export module Menu.Composition;

import Artifact.Composition.Abstract;
import Undo.UndoManager;


export namespace Artifact {

QString uniqueCompositionStateId(
    const QVector<CompositionStateVariant>& states,
    const QString& requestedName);

class ChangeCompositionStatesCommand final : public UndoCommand {
public:
  ChangeCompositionStatesCommand(ArtifactCompositionWeakPtr composition,
                                 QVector<CompositionStateVariant> beforeStates,
                                 QString beforeActiveId,
                                 QVector<CompositionStateVariant> afterStates,
                                 QString afterActiveId,
                                 QString beforeComparisonAId,
                                 QString beforeComparisonBId,
                                 QString afterComparisonAId,
                                 QString afterComparisonBId,
                                 QString label);

  void undo() override;
  void redo() override;
  QString label() const override;
  bool lastOperationSucceeded() const override;

private:
  bool apply(const QVector<CompositionStateVariant>& states,
             const QString& activeId, const QString& comparisonAId,
             const QString& comparisonBId);

  ArtifactCompositionWeakPtr composition_;
  QVector<CompositionStateVariant> beforeStates_;
  QString beforeActiveId_;
  QVector<CompositionStateVariant> afterStates_;
  QString afterActiveId_;
  QString beforeComparisonAId_;
  QString beforeComparisonBId_;
  QString afterComparisonAId_;
  QString afterComparisonBId_;
  QString label_;
  bool lastOperationSucceeded_ = true;
};

class ArtifactCompositionMenuPrivate;

class ArtifactCompositionMenu : public QMenu {
 W_OBJECT(ArtifactCompositionMenu)
private:
 class Impl;
 Impl* impl_;
public:
 void rebuildMenu();
 W_SLOT(rebuildMenu, ());

public:
 explicit ArtifactCompositionMenu(QWidget* mainWindow, QWidget* parent);
 ArtifactCompositionMenu(QWidget* parent = nullptr);
 ~ArtifactCompositionMenu();
 void handleCreateCompositionRequested();
 W_SLOT(handleCreateCompositionRequested, ());
};

}
