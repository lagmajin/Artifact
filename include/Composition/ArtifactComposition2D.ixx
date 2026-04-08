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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <wobjectdefs.h>
#include <QList>
#include <QString>
#include <QJsonObject>
export module Artifact.Composition._2D;






import Utils.Id;
import Color.Float;
import Artifact.Layers;
import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;


export namespace Artifact {

 using namespace ArtifactCore;

 //class ArtifactComposition2DPrivate;

 class ArtifactComposition :public ArtifactAbstractComposition{
 private:
  class Impl;
  Impl* impl_;
  ArtifactComposition(const ArtifactComposition&) = delete;
  ArtifactComposition& operator=(const ArtifactComposition&) = delete;
 public:
  explicit ArtifactComposition(const CompositionID& id, const ArtifactCompositionInitParams& params);
  ~ArtifactComposition();





 };

 typedef std::shared_ptr<ArtifactComposition> ArtifactComposition2DPtr;





};
