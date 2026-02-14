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
  void setAuthor(const UniString& a) { author_ = a; }
  UniString author() const { return author_; }
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
  name_ = UniString(name).toQString();
 }

 QJsonObject ArtifactProjectSettings::Impl::toJson() const
 {
  QJsonObject result;
  result["name"] = name_;
  result["author"] = author_.toQString();
  result["version"] = "1.0";
  return result;
 }

 void ArtifactProjectSettings::Impl::setFromJson(const QJsonObject& json)
 {
  if (json.contains("name")) {
    name_ = json["name"].toString();
  }
  if (json.contains("author")) {
    author_ = UniString(json["author"].toString());
  }
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
 return impl_->projectName().toQString();
 }

 template <StringLike T>
 void Artifact::ArtifactProjectSettings::setProjectName(const T& name)
 {
 // impl_->setProjectName(name);
 }

 UniString ArtifactProjectSettings::author() const
 {
  return impl_->author();
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

