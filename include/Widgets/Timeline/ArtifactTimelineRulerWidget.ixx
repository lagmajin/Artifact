module;
#include <wobjectdefs.h>
#include <QWidget>
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
export module Artifact.Timeline.RulerWidget;





export namespace Artifact
{
  class ArtifactTimelineRulerWidget :public QWidget
  {
   W_OBJECT(ArtifactTimelineRulerWidget)
  private:
  class Impl;
  Impl* impl_;
  float start{ 0.0f }; // 0..1
  float end{ 1.0f };
 protected:
  void paintEvent(QPaintEvent*) override;
  void mousePressEvent(QMouseEvent* ev) override;
  void mouseMoveEvent(QMouseEvent* ev) override;
  void mouseReleaseEvent(QMouseEvent*) override;
 public:
  explicit ArtifactTimelineRulerWidget(QWidget* parent = nullptr);
  ~ArtifactTimelineRulerWidget();

  // Property accessors with signals
  float startValue() const { return start; }
  float endValue() const { return end; }
  void setStart(float s);
  void setEnd(float e);

public:
  void startChanged(float value) W_SIGNAL(startChanged, value)
  void endChanged(float value) W_SIGNAL(endChanged, value)
 };


};
