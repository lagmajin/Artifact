module;
#include <QString>

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
export module Artifact.Project.Importer;




import Artifact.Project;
import Artifact.Project.Health;
import Utils.String.UniString;

export namespace Artifact
{
 using namespace ArtifactCore;

 struct ArtifactProjectImporterResult
 {
  bool success = false;
  ArtifactProjectPtr project;
  UniString errorMessage;
  int compositionsLoaded = 0;
  int layersLoaded = 0;
  ProjectHealthReport healthReport;
 };

 class ArtifactProjectImporter {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactProjectImporter();
  ~ArtifactProjectImporter();
  void setInputPath(const QString& path);
  ArtifactProjectImporterResult importProject();

  // バリデーション
  bool validateFile(const QString& path);
  UniString getFileFormatVersion(const QString& path);
 };

};
