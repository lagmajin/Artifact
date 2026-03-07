module;
#include <QObject>
#include <QString>
export module Artifact.PreviewController;

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



import Frame.Position;

export namespace Artifact
{

 class ArtifactPreviewController :public QObject
 {
 private:
  class Impl;
  Impl* impl_;

 public:
  explicit ArtifactPreviewController();
  ~ArtifactPreviewController();

  void play();
  void stop();
 	
 };






};