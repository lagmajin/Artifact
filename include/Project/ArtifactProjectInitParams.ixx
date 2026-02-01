module;
#include <cstdint>
#include <QJsonObject>

export module ArtifactProjectInitParams;

import std;
import Utils.String.UniString;
import Size;
import Frame.Rate;
import Artifact.Composition.InitParams;

export namespace Artifact {

 class ArtifactProjectInitParams {
 private:
  class Impl;
  Impl* impl_;

 public:
  ArtifactProjectInitParams();
  explicit ArtifactProjectInitParams(const UniString& name, const UniString& author = UniString());
  ArtifactProjectInitParams(const ArtifactProjectInitParams& other);
  ArtifactProjectInitParams(ArtifactProjectInitParams&& other) noexcept;
  ArtifactProjectInitParams& operator=(const ArtifactProjectInitParams& other);
  ArtifactProjectInitParams& operator=(ArtifactProjectInitParams&& other) noexcept;
  ~ArtifactProjectInitParams();

  // Project metadata
  UniString projectName() const;
  void setProjectName(const UniString& name);
  UniString author() const;
  void setAuthor(const UniString& author);
  UniString description() const;
  void setDescription(const UniString& description);
  UniString organization() const;
  void setOrganization(const UniString& organization);
  UniString version() const;
  void setVersion(const UniString& version);

  // Default composition preferences
  Size defaultResolution() const;
  void setDefaultResolution(const Size& resolution);
  void setDefaultResolution(int width, int height);
  FrameRate defaultFrameRate() const;
  void setDefaultFrameRate(const FrameRate& rate);
  void setDefaultFrameRate(double fps);

  // Auto-save
  bool autoSaveEnabled() const;
  void setAutoSaveEnabled(bool enabled);
  int autoSaveIntervalSeconds() const;
  void setAutoSaveIntervalSeconds(int intervalSeconds);

  // Tracking & auditing
  bool trackChanges() const;
  void setTrackChanges(bool track);
  int64_t creationTimestamp() const;
  void setCreationTimestamp(int64_t timestampMs);
  int64_t lastModifiedTimestamp() const;
  void setLastModifiedTimestamp(int64_t timestampMs);

  // Default composition template
  ArtifactCompositionInitParams defaultCompositionParams() const;
  void setDefaultCompositionParams(const ArtifactCompositionInitParams& params);

  // Serialization
  QJsonObject toJson() const;
  bool fromJson(const QJsonObject& object);

  // Reset to defaults
  void reset();

  // Presets
  static ArtifactProjectInitParams defaultTemplate();
  static ArtifactProjectInitParams animationTemplate();
  static ArtifactProjectInitParams storyboardTemplate();

  // Validation
  bool isValid() const;
  UniString validationError() const;

  bool operator==(const ArtifactProjectInitParams& other) const;
  bool operator!=(const ArtifactProjectInitParams& other) const;
 };

} // namespace Artifact
