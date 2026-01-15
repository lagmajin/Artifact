module;
#include <QVector>
#include <QColor>
export module Artifact.Project.Items;

import std;
import Utils.Id;
import Utils.String.UniString;

export namespace Artifact {

 using namespace ArtifactCore;

 enum class eProjectItemType {
  Unknown,
  Folder,
  Composition,
  Footage,
  Solid
 };



 class ProjectItem {
 public:
  virtual ~ProjectItem() = default;
  virtual eProjectItemType type() const = 0;

  Id id;
  UniString name;
  ProjectItem* parent = nullptr;
  QVector<std::unique_ptr<ProjectItem>> children; // 所有権の明確化
 };

 // 2. 具象クラス（必要なデータのみ保持）
 class FootageItem : public ProjectItem {
  eProjectItemType type() const override { return eProjectItemType::Footage; }
  QString filePath;
  
 };

 class SolidItem : public ProjectItem {
  eProjectItemType type() const override { return eProjectItemType::Solid; }
  QColor color;

 };

};