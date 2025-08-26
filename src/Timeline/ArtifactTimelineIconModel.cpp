module;
#include <QIcon>
#include <QStandardItemModel>
module ArtifactTimelineIconModel;

import Utils.Path;

namespace Artifact
{
 using namespace ArtifactCore;

 ArtifactTimelineIconModel::ArtifactTimelineIconModel(QObject* parent /*= nullptr*/) : QStandardItemModel(parent)
 {
  // 列数固定（例: 4列 = 目玉・スピーカー・ソロ・ロック）
  setColumnCount(4);


	
 }



 ArtifactTimelineIconModel::~ArtifactTimelineIconModel()
 {

 }

};