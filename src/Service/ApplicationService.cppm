module;

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
module Artifact.Service.Application;




import Artifact.Application.Manager;
import Artifact.Service.ClipboardManager;

namespace Artifact
{
 class ApplicationService::Impl
 {
 private:;
 	
 public:
  Impl();
  ~Impl()=default;
  static ArtifactApplicationManager* instance();
 };

 ArtifactApplicationManager* ApplicationService::Impl::instance()
 {

  return ArtifactApplicationManager::instance();
 }

 ApplicationService::ApplicationService()
 {

 }

 ApplicationService::~ApplicationService()
 {

 }

};