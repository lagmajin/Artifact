module;
#include <QStandardItemModel>
export module ArtifactTimelineIconModel;

export namespace Artifact
{

 class ArtifactTimelineIconModel:public QStandardItemModel
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactTimelineIconModel(QObject* parent = nullptr);
  ~ArtifactTimelineIconModel();
 };










};