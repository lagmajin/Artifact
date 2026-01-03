module;

#include <QStandardItemModel>
module Artifact.Project.Model;

import std;
import Artifact.Project;
import Artifact.Service.Project;

namespace Artifact
{

 class ArtifactProjectModel::Impl
 {
 private:
  

 public:
  Impl();
  ~Impl();
  ArtifactProjectWeakPtr projectPtr_;
  void refreshTree();
  static ArtifactProjectService* projectService();
 };

 void ArtifactProjectModel::Impl::refreshTree()
 {
  auto projectService = ArtifactProjectService::instance();
 	
 	 
 	
 }

 ArtifactProjectService* ArtifactProjectModel::Impl::projectService()
 {

  return ArtifactProjectService::instance();
 }

 ArtifactProjectModel::ArtifactProjectModel(QObject* parent/*=nullptr*/) :QAbstractItemModel(parent), impl_(new Impl())
 {

  connect(impl_->projectService(),&ArtifactProjectService::layerCreated, this, [this]() {
   impl_->refreshTree();
   });

  

 }

 ArtifactProjectModel::~ArtifactProjectModel()
 {
  delete impl_;
 }

 QVariant ArtifactProjectModel::data(const QModelIndex& index, int role) const
 {
  if (!index.isValid())
   return QVariant();
  
  switch (role) {
  case Qt::DisplayRole: // 「画面に表示する文字は何？」
   return QVariant();

  case Qt::ToolTipRole: // 「マウスホバーした時の説明は？」
   return QVariant();

  case Qt::DecorationRole: // 「アイコンは何にする？」
   return QVariant(); // QIconを返せる

  case Qt::ForegroundRole: // 「文字の色は何色？」
   return QVariant();
   break;

  case Qt::TextAlignmentRole: // 「文字の配置は？」
   return Qt::AlignCenter;
  }

  return QVariant();
 }

 int ArtifactProjectModel::rowCount(const QModelIndex& parent) const
 {
  return 0;
 }
	

};