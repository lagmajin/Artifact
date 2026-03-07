module;
#include <wobjectimpl.h>
#include <map>
#include <QWidget>
#include <QPointer>
module Artifact.Controller.TimelineViewProvider;

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

namespace Artifact{

 W_OBJECT_IMPL(TimelineViewProvider)
  
  
  class TimelineViewProvider::Impl {
  private:
   std::map<CompositionID, QPointer<ArtifactTimelineWidget>> widgets_;
   TimelineViewProvider *owner_ = nullptr;
  public:
   Impl(TimelineViewProvider *owner);
   ~Impl();
   ArtifactTimelineWidget* getOrCreate(const CompositionID &id, QWidget *parent);
  };


 TimelineViewProvider::Impl::Impl(TimelineViewProvider *owner)
   : owner_(owner)
 {
 }
 TimelineViewProvider::Impl::~Impl()
 {
   // Do not delete widgets here: ownership is managed by Qt parent or external callers.
   widgets_.clear();
 }
 
 ArtifactTimelineWidget* TimelineViewProvider::Impl::getOrCreate(const CompositionID &id, QWidget *parent)
 {
   auto it = widgets_.find(id);
   if (it != widgets_.end() && !it->second.isNull()) {
     it->second->setComposition(id);
     return it->second.data();
   }
   auto *w = new ArtifactTimelineWidget(parent);
   w->setComposition(id);
   // Ensure we remove entry when widget is destroyed
   QObject::connect(w, &QObject::destroyed, [this, id](QObject *) {
     widgets_.erase(id);
   });
   widgets_.emplace(id, QPointer<ArtifactTimelineWidget>(w));
   return w;
 }
 TimelineViewProvider::TimelineViewProvider(QObject* parent /*= nullptr*/) : QObject(parent)
 {
   m_impl = new Impl(this);
 }

 TimelineViewProvider::~TimelineViewProvider()
 {
   delete m_impl;
 }

 ArtifactTimelineWidget *TimelineViewProvider::timelineWidgetForComposition(const CompositionID &id, QWidget *parent)
 {
   return m_impl->getOrCreate(id, parent);
 }


};