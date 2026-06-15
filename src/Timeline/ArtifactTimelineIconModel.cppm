module;
#include <compare>
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
  // QIcon eyeIcon(resolveIconPath("MaterialVS/neutral/visibility.svg"));
  
  setHorizontalHeaderItem(0, new QStandardItem(QIcon(resolveIconPath("MaterialVS/neutral/visibility.svg")), ""));
  setHorizontalHeaderItem(1, new QStandardItem(QIcon(resolveIconPath("MaterialVS/yellow/lock.svg")), ""));
 }



 ArtifactTimelineIconModel::~ArtifactTimelineIconModel()
 {

 }

};
