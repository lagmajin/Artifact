module;
#include <QLabel>
#include <QHBoxLayout>
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
module Artifact.Widgets.Timeline.Label;





namespace Artifact
{
	W_OBJECT_IMPL(ArtifactTimelineBottomLabel)
	
 class ArtifactTimelineBottomLabel::Impl
 {
 private:

 public:
  QLabel* frameRenderingLabel = nullptr;
 };

 ArtifactTimelineBottomLabel::ArtifactTimelineBottomLabel(QWidget* parent /*= nullptr*/) :QWidget(parent)
 {


  auto layout = new QHBoxLayout();


  setLayout(layout);

  setFixedHeight(28);
 }

 ArtifactTimelineBottomLabel::~ArtifactTimelineBottomLabel()
 {

 }
	
}