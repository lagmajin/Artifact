module;
#include <QString>
#include <QObject>


export module Project.Settings;

import std;


export namespace Artifact {


 class ArtifactProjectSettings:public QObject {
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactProjectSettings();
  ArtifactProjectSettings(const ArtifactProjectSettings& setting);
  ~ArtifactProjectSettings();
  QString projectName() const;

 };









};