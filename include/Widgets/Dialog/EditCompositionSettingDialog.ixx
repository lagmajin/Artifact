module;
#include <wobjectdefs.h>
#include <QDialog>
export module Artifact.Dialog.EditComposition;

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


import Widgets.Dialog.Abstract;

export namespace Artifact {

 class ArtifactEditCompositionSettingPage:public QWidget
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactEditCompositionSettingPage(QWidget* parent = nullptr);
  ~ArtifactEditCompositionSettingPage();
 };
	
 class ArtifactEditCompositionDialog:public QDialog
 {
 	W_OBJECT(ArtifactEditCompositionDialog)
 private:
  class Impl;
  Impl* impl_;
	
 protected:
 public:
  explicit ArtifactEditCompositionDialog(QWidget* parent = nullptr);
  ~ArtifactEditCompositionDialog();
 public /*signals*/: 
 };












};