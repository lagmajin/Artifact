module;
#include <QWidget>
#include <QHeaderView>
#include <QTreeView>
module Artifact.Widgets.Hierarchy;
import Artifact.Layers.Hierarchy.Model;

namespace Artifact
{
 class ArtifactLayerHierarchyHeaderView::Impl
 {
 private:


 public:
  Impl();
  ~Impl();

 };

 ArtifactLayerHierarchyHeaderView::Impl::Impl()
 {

 }

 ArtifactLayerHierarchyHeaderView::Impl::~Impl()
 {

 }

 ArtifactLayerHierarchyHeaderView::ArtifactLayerHierarchyHeaderView(QWidget* parent /*= nullptr*/) :QHeaderView(Qt::Horizontal, parent),impl_(new Impl)
 {
  setSectionsMovable(true);
  setDragEnabled(true);
  setDragDropMode(QAbstractItemView::InternalMove);

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

  header()->setStretchLastSection(false);
  header()->setSectionResizeMode(QHeaderView::Interactive);

 }

 ArtifactLayerHierarchyView::~ArtifactLayerHierarchyView()
 {

 }







};
