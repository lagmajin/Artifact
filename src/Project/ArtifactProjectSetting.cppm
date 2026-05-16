module;
#include <utility>
#include <wobjectdefs.h>
#include <QList>
#include <QString>
#include <QRegularExpression>
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
  std::vector<ProjectValidationIssue> validate() const;
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

 std::vector<ProjectValidationIssue> ArtifactProjectSettings::Impl::validate() const
 {
  std::vector<ProjectValidationIssue> issues;

  // Project name validation
  const QString name = name_.trimmed();
  
  if (name.isEmpty()) {
    issues.push_back({
      ProjectValidationIssue::Severity::Warning,
      "projectName",
      "プロジェクト名が空です",
      "プロジェクトに名前を設定してください"
    });
  } else {
    // Check for problematic characters
    static const QRegularExpression invalidChars(R"([<>:"/\\|?*])");
    if (invalidChars.match(name).hasMatch()) {
      issues.push_back({
        ProjectValidationIssue::Severity::Error,
        "projectName",
        "プロジェクト名に使用できない文字が含まされています",
        "次の文字は使用できません: < > : \" / \\ | ? *"
      });
    }

    // Check for leading/trailing whitespace
    if (name != name_.trimmed()) {
      issues.push_back({
        ProjectValidationIssue::Severity::Info,
        "projectName",
        "プロジェクト名の前後に空白があります",
        "空白を削除することを検討してください"
      });
    }

    // Check for very long names
    if (name.length() > 100) {
      issues.push_back({
        ProjectValidationIssue::Severity::Warning,
        "projectName",
        "プロジェクト名が長すぎます（100文字以内を推奨）",
        "短い名前にすることを検討してください"
      });
    }

    // Check for default template names (typo/untouched)
    static const QStringList defaultNames = {
      "Untitled Project",
      "New Artifact Project",
      "Animation Project",
      "Storyboard Project"
    };
    if (defaultNames.contains(name, Qt::CaseInsensitive)) {
      issues.push_back({
        ProjectValidationIssue::Severity::Info,
        "projectName",
        "デフォルトのプロジェクト名が使用されています",
        "プロジェクト固有の名前に変更してください"
      });
    }
  }

  // Author validation
  const QString author = author_.toQString().trimmed();
  if (!author.isEmpty() && author.length() > 200) {
    issues.push_back({
      ProjectValidationIssue::Severity::Warning,
      "author",
      "著者名が長すぎます（200文字以内を推奨）",
      "短い名前にすることを検討してください"
    });
  }

  return issues;
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
 impl_->setProjectName(name);
 }

// Explicit instantiation for QString to ensure the symbol is emitted for linkers
template void Artifact::ArtifactProjectSettings::setProjectName<QString>(const QString&);

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

 std::vector<ProjectValidationIssue> ArtifactProjectSettings::validate() const
 {
  return impl_->validate();
 }



};

