module;
#include <QString>
#include <QObject>

#include <QJsonObject>

export module Project.Settings;

import std;
import Utils;

export namespace Artifact {

 using namespace ArtifactCore;

 class ArtifactProjectSettings:public QObject {
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactProjectSettings();
  ArtifactProjectSettings(const ArtifactProjectSettings& setting);
  ~ArtifactProjectSettings();
  QString projectName() const;
  template <StringLike T>
  void setProjectName(const T& name);
 
  ArtifactProjectSettings& operator=(const ArtifactProjectSettings& settings);

  bool operator==(const ArtifactProjectSettings& other) const;
  bool operator!=(const ArtifactProjectSettings& other) const;

 };








};