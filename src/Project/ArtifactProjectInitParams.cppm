module;
#include <algorithm>
#include <QDateTime>
#include <QJsonObject>
#include <QVariant>

module ArtifactProjectInitParams;

import std;
import Utils.String.UniString;
import Size;
import Frame.Rate;
import Artifact.Composition.InitParams;

namespace Artifact {

 namespace {
  int64_t currentTimestampMs()
  {
   return QDateTime::currentMSecsSinceEpoch();
  }
 }

 class ArtifactProjectInitParams::Impl {
 public:
  UniString projectName_;
  UniString author_;
  UniString description_;
  UniString organization_;
  UniString version_;
  Size defaultResolution_;
  FrameRate defaultFrameRate_;
  bool autoSaveEnabled_ = true;
  int autoSaveIntervalSeconds_ = 300;
  bool trackChanges_ = true;
  int64_t creationTimestamp_;
  int64_t lastModifiedTimestamp_;
  ArtifactCompositionInitParams defaultCompositionParams_;

  Impl()
   : projectName_(std::string("Untitled Project")),
     version_(std::string("1.0.0")),
     defaultFrameRate_(30.0),
     creationTimestamp_(currentTimestampMs()),
     lastModifiedTimestamp_(currentTimestampMs())
  {
   defaultResolution_.width = 1920;
   defaultResolution_.height = 1080;
  }
 };

 ArtifactProjectInitParams::ArtifactProjectInitParams()
  : impl_(new Impl())
 {
 }

 ArtifactProjectInitParams::ArtifactProjectInitParams(const UniString& name, const UniString& author)
  : impl_(new Impl())
 {
  impl_->projectName_ = name;
  impl_->author_ = author;
 }

 ArtifactProjectInitParams::ArtifactProjectInitParams(const ArtifactProjectInitParams& other)
  : impl_(new Impl(*other.impl_))
 {
 }

 ArtifactProjectInitParams::ArtifactProjectInitParams(ArtifactProjectInitParams&& other) noexcept
  : impl_(other.impl_)
 {
  other.impl_ = nullptr;
 }

 ArtifactProjectInitParams& ArtifactProjectInitParams::operator=(const ArtifactProjectInitParams& other)
 {
  if (this != &other) {
   *impl_ = *other.impl_;
  }
  return *this;
 }

 ArtifactProjectInitParams& ArtifactProjectInitParams::operator=(ArtifactProjectInitParams&& other) noexcept
 {
  if (this != &other) {
   delete impl_;
   impl_ = other.impl_;
   other.impl_ = nullptr;
  }
  return *this;
 }

 ArtifactProjectInitParams::~ArtifactProjectInitParams()
 {
  delete impl_;
 }

 UniString ArtifactProjectInitParams::projectName() const
 {
  return impl_->projectName_;
 }

 void ArtifactProjectInitParams::setProjectName(const UniString& name)
 {
  impl_->projectName_ = name;
 }

 UniString ArtifactProjectInitParams::author() const
 {
  return impl_->author_;
 }

 void ArtifactProjectInitParams::setAuthor(const UniString& author)
 {
  impl_->author_ = author;
 }

 UniString ArtifactProjectInitParams::description() const
 {
  return impl_->description_;
 }

 void ArtifactProjectInitParams::setDescription(const UniString& description)
 {
  impl_->description_ = description;
 }

 UniString ArtifactProjectInitParams::organization() const
 {
  return impl_->organization_;
 }

 void ArtifactProjectInitParams::setOrganization(const UniString& organization)
 {
  impl_->organization_ = organization;
 }

 UniString ArtifactProjectInitParams::version() const
 {
  return impl_->version_;
 }

 void ArtifactProjectInitParams::setVersion(const UniString& version)
 {
  impl_->version_ = version;
 }

 Size ArtifactProjectInitParams::defaultResolution() const
 {
  return impl_->defaultResolution_;
 }

 void ArtifactProjectInitParams::setDefaultResolution(const Size& resolution)
 {
  impl_->defaultResolution_ = resolution;
 }

 void ArtifactProjectInitParams::setDefaultResolution(int width, int height)
 {
  impl_->defaultResolution_.width = width;
  impl_->defaultResolution_.height = height;
 }

 FrameRate ArtifactProjectInitParams::defaultFrameRate() const
 {
  return impl_->defaultFrameRate_;
 }

 void ArtifactProjectInitParams::setDefaultFrameRate(const FrameRate& rate)
 {
  impl_->defaultFrameRate_ = rate;
 }

 void ArtifactProjectInitParams::setDefaultFrameRate(double fps)
 {
  impl_->defaultFrameRate_ = FrameRate(fps);
 }

 bool ArtifactProjectInitParams::autoSaveEnabled() const
 {
  return impl_->autoSaveEnabled_;
 }

 void ArtifactProjectInitParams::setAutoSaveEnabled(bool enabled)
 {
  impl_->autoSaveEnabled_ = enabled;
 }

 int ArtifactProjectInitParams::autoSaveIntervalSeconds() const
 {
  return impl_->autoSaveIntervalSeconds_;
 }

 void ArtifactProjectInitParams::setAutoSaveIntervalSeconds(int intervalSeconds)
 {
  impl_->autoSaveIntervalSeconds_ = std::max(5, intervalSeconds);
 }

 bool ArtifactProjectInitParams::trackChanges() const
 {
  return impl_->trackChanges_;
 }

 void ArtifactProjectInitParams::setTrackChanges(bool track)
 {
  impl_->trackChanges_ = track;
 }

 int64_t ArtifactProjectInitParams::creationTimestamp() const
 {
  return impl_->creationTimestamp_;
 }

 void ArtifactProjectInitParams::setCreationTimestamp(int64_t timestampMs)
 {
  impl_->creationTimestamp_ = timestampMs;
 }

 int64_t ArtifactProjectInitParams::lastModifiedTimestamp() const
 {
  return impl_->lastModifiedTimestamp_;
 }

 void ArtifactProjectInitParams::setLastModifiedTimestamp(int64_t timestampMs)
 {
  impl_->lastModifiedTimestamp_ = timestampMs;
 }

 ArtifactCompositionInitParams ArtifactProjectInitParams::defaultCompositionParams() const
 {
  return impl_->defaultCompositionParams_;
 }

 void ArtifactProjectInitParams::setDefaultCompositionParams(const ArtifactCompositionInitParams& params)
 {
  impl_->defaultCompositionParams_ = params;
 }

 QJsonObject ArtifactProjectInitParams::toJson() const
 {
  QJsonObject result;
  result["projectName"] = impl_->projectName_.toQString();
  result["author"] = impl_->author_.toQString();
  result["description"] = impl_->description_.toQString();
  result["organization"] = impl_->organization_.toQString();
  result["version"] = impl_->version_.toQString();

  QJsonObject resolution;
  resolution["width"] = impl_->defaultResolution_.width;
  resolution["height"] = impl_->defaultResolution_.height;
  result["defaultResolution"] = resolution;
  result["defaultFrameRate"] = impl_->defaultFrameRate_.framerate();

  result["autoSaveEnabled"] = impl_->autoSaveEnabled_;
  result["autoSaveIntervalSeconds"] = impl_->autoSaveIntervalSeconds_;
  result["trackChanges"] = impl_->trackChanges_;
  result["creationTimestamp"] = static_cast<qint64>(impl_->creationTimestamp_);
  result["lastModifiedTimestamp"] = static_cast<qint64>(impl_->lastModifiedTimestamp_);
  return result;
 }

 bool ArtifactProjectInitParams::fromJson(const QJsonObject& object)
 {
  if (object.isEmpty()) {
   return false;
  }

  setProjectName(UniString(object.value("projectName").toString()));
  setAuthor(UniString(object.value("author").toString()));
  setDescription(UniString(object.value("description").toString()));
  setOrganization(UniString(object.value("organization").toString()));
  setVersion(UniString(object.value("version").toString()));

  if (object.contains("defaultResolution") && object.value("defaultResolution").isObject()) {
   auto resolutionObject = object.value("defaultResolution").toObject();
   int width = resolutionObject.value("width").toInt(1920);
   int height = resolutionObject.value("height").toInt(1080);
   setDefaultResolution(width, height);
  }

  if (object.contains("defaultFrameRate")) {
   setDefaultFrameRate(object.value("defaultFrameRate").toDouble(impl_->defaultFrameRate_.framerate()));
  }

  setAutoSaveEnabled(object.value("autoSaveEnabled").toBool(impl_->autoSaveEnabled_));
  setAutoSaveIntervalSeconds(object.value("autoSaveIntervalSeconds").toInt(impl_->autoSaveIntervalSeconds_));
  setTrackChanges(object.value("trackChanges").toBool(impl_->trackChanges_));
  setCreationTimestamp(object.value("creationTimestamp").toVariant().toLongLong());
  setLastModifiedTimestamp(object.value("lastModifiedTimestamp").toVariant().toLongLong());

  return true;
 }

 void ArtifactProjectInitParams::reset()
 {
  *impl_ = Impl();
 }

 ArtifactProjectInitParams ArtifactProjectInitParams::defaultTemplate()
 {
  ArtifactProjectInitParams params;
  params.setProjectName(UniString(std::string("New Artifact Project")));
  params.setDefaultResolution(1920, 1080);
  params.setDefaultFrameRate(30.0);
  params.setAutoSaveIntervalSeconds(300);
  params.setTrackChanges(true);
  return params;
 }

 ArtifactProjectInitParams ArtifactProjectInitParams::animationTemplate()
 {
  ArtifactProjectInitParams params = defaultTemplate();
  params.setProjectName(UniString(std::string("Animation Project")));
  params.setDefaultFrameRate(24.0);
  params.setDefaultResolution(2048, 1152);
  return params;
 }

 ArtifactProjectInitParams ArtifactProjectInitParams::storyboardTemplate()
 {
  ArtifactProjectInitParams params = defaultTemplate();
  params.setProjectName(UniString(std::string("Storyboard Project")));
  params.setDefaultResolution(1080, 1920);
  params.setDefaultFrameRate(30.0);
  params.setTrackChanges(false);
  return params;
 }

 bool ArtifactProjectInitParams::isValid() const
 {
  if (impl_->projectName_.length() == 0) {
   return false;
  }
  if (impl_->defaultResolution_.width <= 0 || impl_->defaultResolution_.height <= 0) {
   return false;
  }
  if (impl_->defaultFrameRate_.framerate() <= 0.0) {
   return false;
  }
  return true;
 }

 UniString ArtifactProjectInitParams::validationError() const
 {
  if (impl_->projectName_.length() == 0) {
   return UniString(std::string("Project name is empty"));
  }
  if (impl_->defaultResolution_.width <= 0 || impl_->defaultResolution_.height <= 0) {
   return UniString(std::string("Invalid default resolution"));
  }
  if (impl_->defaultFrameRate_.framerate() <= 0.0) {
   return UniString(std::string("Invalid default frame rate"));
  }
  return UniString();
 }

 bool ArtifactProjectInitParams::operator==(const ArtifactProjectInitParams& other) const
 {
  return impl_->projectName_ == other.impl_->projectName_ &&
         impl_->author_ == other.impl_->author_ &&
         impl_->description_ == other.impl_->description_ &&
         impl_->organization_ == other.impl_->organization_ &&
         impl_->version_ == other.impl_->version_ &&
         impl_->defaultResolution_ == other.impl_->defaultResolution_ &&
         impl_->defaultFrameRate_ == other.impl_->defaultFrameRate_ &&
         impl_->autoSaveEnabled_ == other.impl_->autoSaveEnabled_ &&
         impl_->autoSaveIntervalSeconds_ == other.impl_->autoSaveIntervalSeconds_ &&
         impl_->trackChanges_ == other.impl_->trackChanges_ &&
         impl_->creationTimestamp_ == other.impl_->creationTimestamp_ &&
         impl_->lastModifiedTimestamp_ == other.impl_->lastModifiedTimestamp_ &&
         impl_->defaultCompositionParams_ == other.impl_->defaultCompositionParams_;
 }

 bool ArtifactProjectInitParams::operator!=(const ArtifactProjectInitParams& other) const
 {
  return !(*this == other);
 }

} // namespace Artifact
