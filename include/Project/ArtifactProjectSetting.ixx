module;
#include <QString>
#include <QObject>

#include <QJsonObject>
#include <wobjectdefs.h>
#include <vulkan/vulkan_core.h>


#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Artifact.Project.Settings;



import std;
import Utils;
import Utils.String.UniString;


export namespace Artifact {

 using namespace ArtifactCore;

 struct ProjectValidationIssue {
  enum class Severity {
   Info,
   Warning,
   Error
  };
  Severity severity;
  QString field;
  QString message;
  QString suggestion;
 };

 class ArtifactProjectSettings:public QObject {
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactProjectSettings();
  ArtifactProjectSettings(const ArtifactProjectSettings& setting);
  ~ArtifactProjectSettings();
  QString projectName() const;
  template <StringLike T>
  void setProjectName(const T& name);
  UniString author() const;
  template <StringLike T>
  void setAuthor(const T& name);
  QJsonObject toJson() const;

  // Validation
  std::vector<ProjectValidationIssue> validate() const;

  ArtifactProjectSettings& operator=(const ArtifactProjectSettings& settings);

  bool operator==(const ArtifactProjectSettings& other) const;
  bool operator!=(const ArtifactProjectSettings& other) const;

 public /*signals*/:
  //void projectSettingChanged();
  //W_SLOT(projectChanged);
 };

 template <StringLike T>
 void Artifact::ArtifactProjectSettings::setAuthor(const T& name)
 {
  //impl_->setAuthor(UniString(name));
 }

 





};
W_REGISTER_ARGTYPE(Artifact::ArtifactProjectSettings)