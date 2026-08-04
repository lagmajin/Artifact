module;
#include <QHeaderView>
#include <QMenu>
#include <QTreeView>
#include <QWidget>
#include <utility>

module Artifact.Widgets.Hierarchy;
import Artifact.Layers.Hierarchy.Model;

namespace Artifact {
class ArtifactLayerHierarchyHeaderContextMenu::Impl {
private:
public:
  Impl(ArtifactLayerHierarchyHeaderContextMenu *menu);
  ~Impl();
  void buildMenu();
  QMenu *visibleMenu = nullptr;
  void defaultHandleMousePressEvent(QMouseEvent *event);
};

void ArtifactLayerHierarchyHeaderContextMenu::Impl::buildMenu() {}

void ArtifactLayerHierarchyHeaderContextMenu::Impl::
    defaultHandleMousePressEvent(QMouseEvent *event) {}

ArtifactLayerHierarchyHeaderContextMenu::Impl::Impl(
    ArtifactLayerHierarchyHeaderContextMenu *menu) {
  visibleMenu = new QMenu();
  visibleMenu->setTitle("visible");
  menu->addMenu(visibleMenu);
}

ArtifactLayerHierarchyHeaderContextMenu::Impl::~Impl() {}

ArtifactLayerHierarchyHeaderContextMenu::
    ArtifactLayerHierarchyHeaderContextMenu(QWidget *parent /*= nullptr*/)
    : QMenu(parent), impl_(new Impl(this)) {}

ArtifactLayerHierarchyHeaderContextMenu::
    ~ArtifactLayerHierarchyHeaderContextMenu() {
  delete impl_;
}

class ArtifactLayerHierarchyHeaderView::Impl {
private:
public:
  Impl();
  Impl(ArtifactLayerHierarchyHeaderView *view);
  ~Impl();
  void showContextMenu();
  ArtifactLayerHierarchyHeaderView *view_ = nullptr;
  ArtifactLayerHierarchyHeaderContextMenu *menu = nullptr;
};

ArtifactLayerHierarchyHeaderView::Impl::Impl() {}

ArtifactLayerHierarchyHeaderView::Impl::Impl(
    ArtifactLayerHierarchyHeaderView *view)
    : view_(view) {}

ArtifactLayerHierarchyHeaderView::Impl::~Impl() {
  // if (view_) view_->deleteLater();
  if (menu)
    menu->deleteLater();
}

void ArtifactLayerHierarchyHeaderView::Impl::showContextMenu() {
  if (!view_) {
    return;
  }

  QMenu menu(view_);
  menu.setTitle(QStringLiteral("Visible Columns"));
  const QStringList labels = {
      QStringLiteral("Visibility"), QStringLiteral("Lock"),
      QStringLiteral("Type"), QStringLiteral("Name")};
  for (int section = 0; section < labels.size(); ++section) {
    QAction* action = menu.addAction(labels.at(section));
    action->setCheckable(true);
    action->setChecked(!view_->isSectionHidden(section));
    action->setEnabled(section != 3);
    QObject::connect(action, &QAction::toggled, view_,
                     [this, section](bool visible) {
                       if (view_) {
                         view_->setSectionHidden(section, !visible);
                       }
                     });
  }
  menu.exec(view_->viewport()->mapToGlobal(view_->rect().center()));
}

ArtifactLayerHierarchyHeaderView::ArtifactLayerHierarchyHeaderView(
    QWidget *parent /*= nullptr*/)
    : QHeaderView(Qt::Horizontal, parent), impl_(new Impl(this)) {
  setSectionsMovable(true);
  setDragEnabled(true);
  setAlternatingRowColors(true);
  setDragDropMode(QAbstractItemView::InternalMove);
  setContextMenuPolicy(Qt::CustomContextMenu);

  setSectionResizeMode(QHeaderView::Interactive);

  // setSectionResizeMode(0, QHeaderView::Fixed);
  // setSectionResizeMode(0, QHeaderView::ResizeToContents);

  connect(this, &QHeaderView::customContextMenuRequested, this,
          [this](const QPoint &pos) { impl_->showContextMenu(); });
}

ArtifactLayerHierarchyHeaderView::~ArtifactLayerHierarchyHeaderView() {
  delete impl_;
}

class ArtifactLayerHierarchyView::Impl {
private:
public:
  Impl();
  ~Impl();
};

ArtifactLayerHierarchyView::Impl::Impl() {}

ArtifactLayerHierarchyView::Impl::~Impl() {}

ArtifactLayerHierarchyView::ArtifactLayerHierarchyView(
    QWidget *parent /*= nullptr*/)
    : QTreeView(parent), impl_(new Impl()) {
  auto model = new ArtifactHierarchyModel();

  setModel(model);

  setHeader(new ArtifactLayerHierarchyHeaderView);
  header()->setSectionResizeMode(0, QHeaderView::Fixed);
  header()->resizeSection(0, 24); // Visibility
  header()->setSectionResizeMode(1, QHeaderView::Fixed);
  header()->resizeSection(1, 24); // Lock
  header()->setSectionResizeMode(2, QHeaderView::Fixed);
  header()->resizeSection(2, 24); // Type
  header()->setSectionResizeMode(3, QHeaderView::Stretch); // Name
  header()->setStretchLastSection(true);

  setRootIsDecorated(true);
  setItemsExpandable(true);
  setExpandsOnDoubleClick(true);

  // Enable drag and drop for layer reordering
  setDragEnabled(true);
  setAcceptDrops(true);
  setDragDropMode(QAbstractItemView::InternalMove);
}

ArtifactLayerHierarchyView::~ArtifactLayerHierarchyView() { delete impl_; }

}; // namespace Artifact
