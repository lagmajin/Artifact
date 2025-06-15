module;
#include <QString>
#include <QObject>


export module ArtifactProjectSetting;



export namespace Artifact {

 class ArtifactProjectSetting:QObject {
 private:

 public:
  explicit ArtifactProjectSetting();
  ~ArtifactProjectSetting();
  QString projectName() const;

 };











};