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
#include <QWidget>
export module Artifact.Widgets.Render.QueueManager;





export namespace Artifact
{

	
	




 class RenderQueueManagerWidget :public QWidget {
  W_OBJECT(RenderQueueManagerWidget)
 private:
  class Impl;
  Impl* impl_;
 protected:
 	
 public:
  explicit RenderQueueManagerWidget(QWidget* parent = nullptr);
  ~RenderQueueManagerWidget();
  QSize sizeHint() const override;
  void setFloatingMode(bool isFloating);
 protected:
  void showEvent(QShowEvent* event) override;
 };








};
