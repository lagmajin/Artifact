module;

module Project.Settings;



namespace Artifact {

 class ArtifactProjectSettings::Impl
 {
 private:
  QString name_;
 public:
  Impl();
  ~Impl();
  QString projectName() const;
  template <StringLike T>
  void setProjectName(const T& name);
 };

 template <StringLike T>
 void Artifact::ArtifactProjectSettings::Impl::setProjectName(const T& name)
 {

 }

 ArtifactProjectSettings::Impl::Impl()
 {

 }

 ArtifactProjectSettings::Impl::~Impl()
 {

 }

 QString ArtifactProjectSettings::Impl::projectName() const
 {
  return name_;
 }


 ArtifactProjectSettings::ArtifactProjectSettings()
 {

 }

 ArtifactProjectSettings::ArtifactProjectSettings(const ArtifactProjectSettings& setting)
 {

 }

 ArtifactProjectSettings::~ArtifactProjectSettings()
 {

 }

 QString ArtifactProjectSettings::projectName() const
 {
  return impl_->projectName();
 }

 template <StringLike T>
 void Artifact::ArtifactProjectSettings::setProjectName(const T& name)
 {
  impl_->setProjectName(name);
 }

 QString ArtifactProjectSettings::author() const
 {
  return QString();
 }

ArtifactProjectSettings& ArtifactProjectSettings::operator=(const ArtifactProjectSettings& settings)
 {

 return *this;
 }

 bool ArtifactProjectSettings::operator==(const ArtifactProjectSettings& other) const
 {


  return false;

 }

 bool ArtifactProjectSettings::operator!=(const ArtifactProjectSettings& other) const
 {
  return !(*this == other);
 }

 


};

