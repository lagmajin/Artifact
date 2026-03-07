module;
#include <wobjectimpl.h>
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
module Artifact.Test.ProjectManager;




//#include <boost/asio/prefer.hpp>

namespace Artifact
{
 class ArtifactTestProjectManager::Impl
 {
 private:
 	
 	
 public:
  Impl();
  ~Impl();
 };

 ArtifactTestProjectManager::Impl::Impl()
 {
  
 }

 ArtifactTestProjectManager::Impl::~Impl()
 {

 }

 ArtifactTestProjectManager::ArtifactTestProjectManager():impl_(new Impl)
 {

 }

 ArtifactTestProjectManager::~ArtifactTestProjectManager()
 {
  delete impl_;
 }

};