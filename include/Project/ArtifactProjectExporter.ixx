module;
#include <QFile>
export module Artifact.Project.Exporter;

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



import Artifact.Project;
import Utils.String.Like;
import Utils.String.UniString;


export namespace Artifact
{
 using namespace ArtifactCore;

 struct ArtifactProjectExporterResult
 {
  bool success = false;
 };

 class ArtifactProjectExporter {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactProjectExporter();
  ~ArtifactProjectExporter();
  void setProject(ArtifactProjectPtr& ptr);
  ArtifactProjectExporterResult exportProject();
  void exportProject2();
  template <StringLike T>
  void setOutputPath(const T& name);
  void setOutputPath(const QString& path);
  void setFormat();
  // ...
 };

 template <StringLike T>
 void ArtifactProjectExporter::setOutputPath(const T& name)
 {
  //setOutputPath(name);
 }
};

