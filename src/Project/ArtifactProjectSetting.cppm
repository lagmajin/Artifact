module;
#include <wobjectdefs.h>
#include <QList>
#include <QString>
module Artifact.Project.Settings;

import std;
import Utils.String.UniString;


namespace Artifact {

 using namespace ArtifactCore;

 class ArtifactProjectSettings::Impl
 {
 private:
  QString name_;
  UniString author_;
 public:
  Impl();
  ~Impl();
  UniString projectName() const;
  template <StringLike T>
  void setProjectName(const T& name);
  QJsonObject toJson() const;
  void setFromJson(const QJsonObject& json);
 };
	
 ArtifactProjectSettings::Impl::Impl()
 {

 }

 ArtifactProjectSettings::Impl::~Impl()
 {

 }

 UniString ArtifactProjectSettings::Impl::projectName() const
 {
  return name_;
 }
	
 template <StringLike T>
 void ArtifactProjectSettings::Impl::setProjectName(const T& name)
 {

 }

 QJsonObject ArtifactProjectSettings::Impl::toJson() const
 {
  QJsonObject result;
 	
  return result;
 }

 void ArtifactProjectSettings::Impl::setFromJson(const QJsonObject& json)
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

 UniString ArtifactProjectSettings::author() const
 {
  return UniString();
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

 QJsonObject ArtifactProjectSettings::toJson() const
 {
  return impl_->toJson();
 }



};

