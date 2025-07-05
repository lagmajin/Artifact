module;
#include <QString>
#include <QObject>


export module Project.Settings;



export namespace Artifact {

 class ArtifactProjectSetting:QObject {
 private:

 public:
  explicit ArtifactProjectSetting();
  ArtifactProjectSetting(const ArtifactProjectSetting& setting);
  ~ArtifactProjectSetting();
  QString projectName() const;

 };









};