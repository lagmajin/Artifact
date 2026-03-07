module;
#include <QJsonObject>
export module Composition3D;

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




import Artifact.Composition.Abstract;

export namespace Artifact {

 enum eCompositionType {
  OriginalComposition,
  PreCompose,
 };

 //class ArtifactCompositionPrivate;

 class ArtifactComposition3D :public ArtifactAbstractComposition{
 private: 
  class Impl;
 	
  //std::unique_ptr<ArtifactCompositionPrivate> pImpl_;
 public:
 

 };

}