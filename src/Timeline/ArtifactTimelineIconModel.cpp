module;
#include <QIcon>
#include <QStandardItemModel>
module ArtifactTimelineIconModel;

import Utils.Path;


namespace Artifact
{
 using namespace ArtifactCore;

 class ArtifactTimelineIconModel::Impl
 {
 private:

 public:
  Impl();
  ~Impl();
 };

 ArtifactTimelineIconModel::ArtifactTimelineIconModel(QObject* parent /*= nullptr*/) : QStandardItemModel(parent)
 {


  setColumnCount(4);
  //QIcon eyeIcon(resolveIconPath("visibility.png"));
  
  setHorizontalHeaderItem(0, new QStandardItem(QIcon(resolveIconPath("visibility.png")), ""));
  setHorizontalHeaderItem(1, new QStandardItem(QIcon(resolveIconPath("lock.png")), ""));
 }



 ArtifactTimelineIconModel::~ArtifactTimelineIconModel()
 {

 }

};