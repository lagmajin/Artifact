module;
#include <wobjectdefs.h>
//
module Artifact.Project.Settings;

import std;

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
	
 template <StringLike T>
 void ArtifactProjectSettings::Impl::setProjectName(const T& name)
 {

 }

 ArtifactProjectSettings::ArtifactProjectSettings():impl_(new Impl())
 {

 }

 ArtifactProjectSettings::ArtifactProjectSettings(const ArtifactProjectSettings& setting) :impl_(new Impl())
 {

 }

 ArtifactProjectSettings::~ArtifactProjectSettings()
 {
  delete impl_;
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

