module;
#include <QJsonObject>
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
module Artifact.Composition._2D;



import Artifact.Layer.Abstract;


//import ArtifactCore
import Composition.Settings;
import Artifact.Composition.Abstract;

namespace Artifact {

 using namespace ArtifactCore;

 class ArtifactComposition::Impl
 {
 private:
  //QVector<ArtifactAbstractLayerPtr> layers_;
  //MultiIndexLayerContainer layers_;
  
 public:
  Impl();
  ~Impl();
 };

 ArtifactComposition::Impl::Impl()
 {

 }

 ArtifactComposition::Impl::~Impl()
 {

 }
	
ArtifactComposition::ArtifactComposition(const CompositionID& id, const ArtifactCompositionInitParams& params):ArtifactAbstractComposition(id,params)
{

}

ArtifactComposition::~ArtifactComposition()
{
 delete impl_;
}





};
