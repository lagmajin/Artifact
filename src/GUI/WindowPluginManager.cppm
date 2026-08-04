module;
#include <QVector>
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
module WindowManager;




namespace Artifact
{


class WindowPluginManager::Impl
 {
 public:
	 QVector<WindowTypeInfo> windowTypes_;
	 int findIndex(const QString& name) const {
	   const QString normalized = name.trimmed();
	   for (int i = 0; i < windowTypes_.size(); ++i) {
	     if (windowTypes_.at(i).name.compare(normalized, Qt::CaseInsensitive) == 0) return i;
	   }
	   return -1;
	 }
 	
 public:
  Impl();
  ~Impl();
 };

 WindowPluginManager::Impl::Impl()
 {

 }

 WindowPluginManager::Impl::~Impl()
 {

 }

 WindowPluginManager::~WindowPluginManager()
 {
  delete impl_;
  impl_ = nullptr;
 }

 WindowPluginManager::WindowPluginManager()
  : impl_(new Impl())
 {
 }

 void WindowPluginManager::registerWindowFactory()
 {
   // Kept for compatibility with older callers. Registration is now explicit
   // through registerWindowType(), so invoking this method is idempotent.
 }

 bool WindowPluginManager::registerWindowType(const WindowTypeInfo& info)
 {
   if (!impl_ || info.name.trimmed().isEmpty() || impl_->findIndex(info.name) >= 0) {
     return false;
   }
   WindowTypeInfo normalized = info;
   normalized.name = normalized.name.trimmed();
   impl_->windowTypes_.append(std::move(normalized));
   return true;
 }

 bool WindowPluginManager::unregisterWindowType(const QString& name)
 {
   if (!impl_) return false;
   const int index = impl_->findIndex(name);
   if (index < 0) return false;
   impl_->windowTypes_.removeAt(index);
   return true;
 }

 bool WindowPluginManager::hasWindowType(const QString& name) const
 {
   return impl_ && impl_->findIndex(name) >= 0;
 }

 bool WindowPluginManager::canOpenWindow(const QString& name,
                                         const int openInstanceCount) const
 {
   if (!impl_ || openInstanceCount < 0) return false;
   const int index = impl_->findIndex(name);
   if (index < 0) return false;
   return impl_->windowTypes_.at(index).allowMultiple || openInstanceCount == 0;
 }

 QVector<WindowTypeInfo> WindowPluginManager::windowTypes() const
 {
   return impl_ ? impl_->windowTypes_ : QVector<WindowTypeInfo>{};
 }

};
