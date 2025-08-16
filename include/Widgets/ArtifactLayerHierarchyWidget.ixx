module;
#include <QWidget>
#include <QTreeView>
#include <QHeaderView>
export module Artifact.Widgets.Hierarchy;
import Artifact.Layers.Hierarchy.Model;

export namespace Artifact
{
 enum class LayerDisplayMode {
  LayerName,   // レイヤー名を表示
  SourceName   // ソース名を表示
 };

 class ArtifactLayerHierarchyHeaderView :public QHeaderView
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactLayerHierarchyHeaderView(QWidget* parent = nullptr);
  ~ArtifactLayerHierarchyHeaderView();
  void setDisplayLayerMode(LayerDisplayMode mode);
 };

 class ArtifactLayerHierarchyView :public QTreeView
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactLayerHierarchyView(QWidget* parent = nullptr);
  ~ArtifactLayerHierarchyView();

 };






};


