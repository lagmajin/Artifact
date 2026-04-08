module;
#include <QIcon>
#include <QStandardItemModel>
#include <utility>
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
