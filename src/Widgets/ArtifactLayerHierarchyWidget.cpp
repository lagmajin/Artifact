module;
#include <QWidget>
#include <QHeaderView>
#include <QTreeView>
module Artifact.Widgets.Hierarchy;
import Artifact.Layers.Hierarchy.Model;

namespace Artifact
{
 class ArtifactLayerHierarchyHeaderContextMenu::Impl
 {private:


 public:
  Impl(ArtifactLayerHierarchyHeaderContextMenu* menu);
  ~Impl();
  void buildMenu();
  QMenu* visibleMenu = nullptr;
 };

 void ArtifactLayerHierarchyHeaderContextMenu::Impl::buildMenu()
 {

 }

 ArtifactLayerHierarchyHeaderContextMenu::Impl::Impl(ArtifactLayerHierarchyHeaderContextMenu* menu)
 {
  visibleMenu = new QMenu();
  visibleMenu->setTitle("visible");
  menu->addMenu(visibleMenu);

 }

 ArtifactLayerHierarchyHeaderContextMenu::ArtifactLayerHierarchyHeaderContextMenu(QWidget* parent /*= nullptr*/):QMenu(parent),impl_(new Impl(this))
 {

 }

 ArtifactLayerHierarchyHeaderContextMenu::~ArtifactLayerHierarchyHeaderContextMenu()
 {
  delete impl_;
 }


 class ArtifactLayerHierarchyHeaderView::Impl
 {
 private:


 public:
  Impl();
  Impl(ArtifactLayerHierarchyHeaderView* view);
  ~Impl();
  void showContextMenu();
  ArtifactLayerHierarchyHeaderView* view_ = nullptr;
  ArtifactLayerHierarchyHeaderContextMenu* menu = nullptr;
 };

 ArtifactLayerHierarchyHeaderView::Impl::Impl()
 {

 }

 ArtifactLayerHierarchyHeaderView::Impl::Impl(ArtifactLayerHierarchyHeaderView* view):view_(view)
 {

 }

 ArtifactLayerHierarchyHeaderView::Impl::~Impl()
 {
  //if (view_) view_->deleteLater();
  if (menu) menu->deleteLater();

 }

 void ArtifactLayerHierarchyHeaderView::Impl::showContextMenu()
 {
  qDebug() << "Menu test";
 }

 ArtifactLayerHierarchyHeaderView::ArtifactLayerHierarchyHeaderView(QWidget* parent /*= nullptr*/) :QHeaderView(Qt::Horizontal, parent),impl_(new Impl(this))
 {
  setSectionsMovable(true);
  setDragEnabled(true);
  setDragDropMode(QAbstractItemView::InternalMove);
  setContextMenuPolicy(Qt::CustomContextMenu);

  setSectionResizeMode(QHeaderView::Interactive);

  //setSectionResizeMode(0, QHeaderView::Fixed);
  //setSectionResizeMode(0, QHeaderView::ResizeToContents);

  connect(this, &QHeaderView::customContextMenuRequested,
   this, [this](const QPoint& pos) {
    impl_->showContextMenu();
   });
 }

 ArtifactLayerHierarchyHeaderView::~ArtifactLayerHierarchyHeaderView()
 {
  delete impl_;
 }

 class ArtifactLayerHierarchyView::Impl
 {
 private:

 public:
  Impl();
  ~Impl();
 };

 ArtifactLayerHierarchyView::Impl::Impl()
 {

 }

 ArtifactLayerHierarchyView::Impl::~Impl()
 {

 }

 ArtifactLayerHierarchyView::ArtifactLayerHierarchyView(QWidget* parent /*= nullptr*/) :QTreeView(parent)
 {
  auto model = new ArtifactHierarchyModel();

  setModel(model);

  setHeader(new ArtifactLayerHierarchyHeaderView);
  header()->setSectionResizeMode(0, QHeaderView::Fixed);
  header()->resizeSection(0, 20);
  header()->setStretchLastSection(false);
  //header()->setSectionResizeMode(QHeaderView::Interactive);

  setRootIsDecorated(false);

 }

 ArtifactLayerHierarchyView::~ArtifactLayerHierarchyView()
 {

 }









};
