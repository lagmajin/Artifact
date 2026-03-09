module;
#include <QtWidgets>

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

#include <wobjectdefs.h>
export module Menu.MenuBar;






export namespace Artifact {

 enum class eMenuType {
  File,
  Edit,
  Create,
  Composition,
  Layer,
  Effect,
  Animation,
  Script,
  Render,
  Tool,
  Link,
  Test,
  Window,
  Option,
  Help,

 };

 class ArtifactMainWindow;


 class ArtifactMenuBar :public QMenuBar{
	 W_OBJECT(ArtifactMenuBar)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactMenuBar(ArtifactMainWindow* mainWindow,QWidget*parent=nullptr);
  ~ArtifactMenuBar();
  void setMainWindow(ArtifactMainWindow* window);
 };


};