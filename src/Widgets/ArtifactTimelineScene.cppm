module;

#include <QBrush>
#include <QColor>
#include <QGraphicsRectItem>
#include <QObject>
#include <QPainter>
#include <QPointF>
#include <wobjectimpl.h>

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <vector>

module Artifact.Widgets.Timeline;

import Artifact.Timeline.Objects;

namespace Artifact
{
 W_OBJECT_IMPL(TimelineScene)

 class TimelineScene::Impl
 {
 public:
  std::vector<double> trackHeights_;
  std::vector<ClipItem*> clips_;
 std::vector<ClipItem*> selectedClips_;
 std::unordered_map<ClipItem*, int> clipTracks_;
  TimelineScene* parent_ = nullptr;
  QGraphicsRectItem* highlightRect_ = nullptr;
  bool rippleEditEnabled_ = true;

  struct DragContext
  {
   bool active = false;
   ClipItem* anchorClip = nullptr;
   int anchorTrack = -1;
   double anchorStartX = 0.0;
   std::vector<ClipItem*> movingClips;
   std::unordered_map<ClipItem*, QPointF> initialPos;
   std::unordered_map<ClipItem*, int> initialTrack;
  } drag_;

  explicit Impl(TimelineScene* parent) : parent_(parent) {}

  ~Impl()
  {
   for (auto* clip : clips_)
   {
    parent_->removeItem(clip);
    destroyClipItem(clip);
   }
   clips_.clear();
   clipTracks_.clear();

   if (highlightRect_)
   {
    parent_->removeItem(highlightRect_);
    delete highlightRect_;
    highlightRect_ = nullptr;
   }
  }

  [[nodiscard]] double getTotalTrackHeight() const
  {
   double total = 0.0;
   for (const double height : trackHeights_)
   {
    total += height;
   }
   return total;
  }

  [[nodiscard]] double trackTopY(const int trackIndex) const
  {
   if (trackIndex <= 0)
   {
    return 0.0;
   }
   double y = 0.0;
   for (int i = 0; i < trackIndex && i < static_cast<int>(trackHeights_.size()); ++i)
   {
    y += trackHeights_[i];
   }
   return y;
  }

  void relayoutClips() const
  {
   for (auto* clip : clips_)
   {
    const auto it = clipTracks_.find(clip);
    if (it == clipTracks_.end())
    {
     continue;
    }
    const int trackIndex = it->second;
    if (trackIndex < 0 || trackIndex >= static_cast<int>(trackHeights_.size()))
    {
     continue;
    }
    clip->setPos(clip->pos().x(), trackTopY(trackIndex));
   }
  }

  void refreshSceneHeight() const
  {
   QRectF sr = parent_->sceneRect();
   sr.setHeight(getTotalTrackHeight());
   parent_->setSceneRect(sr);
  }

  void beginDrag(ClipItem* anchor)
  {
   drag_ = DragContext{};
   drag_.active = true;
   drag_.anchorClip = anchor;
   drag_.anchorTrack = clipTrack(anchor);
   drag_.anchorStartX = anchor ? anchor->pos().x() : 0.0;

   if (!selectedClips_.empty() && std::find(selectedClips_.begin(), selectedClips_.end(), anchor) != selectedClips_.end())
   {
    drag_.movingClips = selectedClips_;
   }
   else if (anchor)
   {
    drag_.movingClips.push_back(anchor);
   }

   for (auto* clip : drag_.movingClips)
   {
    drag_.initialPos[clip] = clip->pos();
    drag_.initialTrack[clip] = clipTrack(clip);
   }
  }

  [[nodiscard]] int clipTrack(ClipItem* clip) const
  {
   const auto it = clipTracks_.find(clip);
   return it == clipTracks_.end() ? -1 : it->second;
  }

  [[nodiscard]] double snapX(
   const double x,
   const ClipItem* movingAnchor,
   const int targetTrack,
   const std::vector<ClipItem*>& movingClips) const
  {
   constexpr double kGrid = 10.0;
   constexpr double kTolerance = 6.0;

   double best = std::max(0.0, x);
   double bestDistance = kTolerance + 1.0;

   auto tryCandidate = [&](const double candidate) {
    const double distance = std::abs(candidate - x);
    if (distance <= kTolerance && distance < bestDistance)
    {
     best = std::max(0.0, candidate);
     bestDistance = distance;
    }
   };

   tryCandidate(0.0);
   tryCandidate(std::round(x / kGrid) * kGrid);

   std::unordered_set<const ClipItem*> movingSet;
   movingSet.reserve(movingClips.size());
   for (auto* clip : movingClips)
   {
    movingSet.insert(clip);
   }

   const double movingDuration = movingAnchor ? movingAnchor->getDuration() : 0.0;
   for (auto* clip : clips_)
   {
    if (!clip || movingSet.count(clip) > 0)
    {
     continue;
    }
    if (clipTrack(clip) != targetTrack)
    {
      continue;
    }

    const double start = clip->pos().x();
    const double end = start + clip->getDuration();
    tryCandidate(start);
    tryCandidate(end);
    tryCandidate(start - movingDuration);
    tryCandidate(end - movingDuration);
   }

   return best;
  }

  void resolveOverlapsWithRipple(const int trackIndex, const std::unordered_set<ClipItem*>& pinned)
  {
   std::vector<ClipItem*> trackClips;
   trackClips.reserve(clips_.size());
   for (auto* clip : clips_)
   {
    if (clipTrack(clip) == trackIndex)
    {
     trackClips.push_back(clip);
    }
   }

   std::sort(trackClips.begin(), trackClips.end(), [](const ClipItem* a, const ClipItem* b) {
    return a->pos().x() < b->pos().x();
   });

   double cursor = 0.0;
   for (auto* clip : trackClips)
   {
    const double x = clip->pos().x();
    if (x < cursor)
    {
     // Keep moved clips fixed; ripple other clips forward.
     if (pinned.count(clip) == 0)
     {
      clip->setPos(cursor, clip->pos().y());
     }
    }
    cursor = std::max(cursor, clip->pos().x() + clip->getDuration());
   }
  }
 };

TimelineScene::TimelineScene(QWidget* parent) : QGraphicsScene(parent), impl_(new Impl(this))
{
 setSceneRect(0, 0, 2000, 0);
 QObject::connect(this, &QGraphicsScene::selectionChanged, this, [this]() {
  impl_->selectedClips_.clear();
  for (auto* item : selectedItems())
  {
   if (auto* clip = dynamic_cast<ClipItem*>(item))
   {
    impl_->selectedClips_.push_back(clip);
   }
  }
 });
}

 TimelineScene::~TimelineScene()
 {
  delete impl_;
 }

 void TimelineScene::drawBackground(QPainter* painter, const QRectF& rect)
 {
  painter->fillRect(rect, QColor(30, 30, 30));
 }

 int TimelineScene::addTrack(const double height)
 {
  const int trackIndex = static_cast<int>(impl_->trackHeights_.size());
  impl_->trackHeights_.push_back(std::max(10.0, height));
  impl_->refreshSceneHeight();
  return trackIndex;
 }

 void TimelineScene::removeTrack(const int trackIndex)
 {
  if (trackIndex < 0 || trackIndex >= static_cast<int>(impl_->trackHeights_.size()))
  {
   return;
  }

  std::vector<ClipItem*> clipsToRemove;
  for (auto& [clip, idx] : impl_->clipTracks_)
  {
   if (idx == trackIndex)
   {
    clipsToRemove.push_back(clip);
   }
   else if (idx > trackIndex)
   {
    --idx;
   }
  }

  for (auto* clip : clipsToRemove)
  {
   removeClip(clip);
  }

  impl_->trackHeights_.erase(impl_->trackHeights_.begin() + trackIndex);
  impl_->relayoutClips();
  impl_->refreshSceneHeight();

  if (impl_->highlightRect_)
  {
   clearTrackHighlight();
  }
 }

 void TimelineScene::clearTracks()
 {
  for (auto* clip : impl_->clips_)
  {
   removeItem(clip);
   destroyClipItem(clip);
  }
  impl_->clips_.clear();
  impl_->selectedClips_.clear();
  impl_->clipTracks_.clear();
  impl_->trackHeights_.clear();
  setSceneRect(0, 0, 2000, 0);
  clearTrackHighlight();
 }

 int TimelineScene::trackCount() const
 {
  return static_cast<int>(impl_->trackHeights_.size());
 }

 void TimelineScene::highlightTrack(const int trackIndex)
 {
  if (trackIndex < 0 || trackIndex >= static_cast<int>(impl_->trackHeights_.size()))
  {
   return;
  }

  const double y = getTrackYPosition(trackIndex);
  const double h = impl_->trackHeights_[trackIndex];
  const QRectF rect(sceneRect().left(), y, sceneRect().width(), h);

  if (!impl_->highlightRect_)
  {
   impl_->highlightRect_ = new QGraphicsRectItem(rect);
   impl_->highlightRect_->setBrush(QBrush(QColor(255, 255, 0, 40)));
   impl_->highlightRect_->setPen(Qt::NoPen);
   impl_->highlightRect_->setZValue(500);
   addItem(impl_->highlightRect_);
  }
  else
  {
   impl_->highlightRect_->setRect(rect);
  }
 }

 void TimelineScene::clearTrackHighlight()
 {
  if (!impl_->highlightRect_)
  {
   return;
  }

  removeItem(impl_->highlightRect_);
  delete impl_->highlightRect_;
  impl_->highlightRect_ = nullptr;
 }

 double TimelineScene::trackHeight(const int trackIndex) const
 {
  if (trackIndex < 0 || trackIndex >= static_cast<int>(impl_->trackHeights_.size()))
  {
   return 0.0;
  }
  return impl_->trackHeights_[trackIndex];
 }

 void TimelineScene::setTrackHeight(const int trackIndex, const double height)
 {
  if (trackIndex < 0 || trackIndex >= static_cast<int>(impl_->trackHeights_.size()))
  {
   return;
  }

  impl_->trackHeights_[trackIndex] = std::max(10.0, height);
  impl_->relayoutClips();
  impl_->refreshSceneHeight();
  update();
 }

 double TimelineScene::getTrackYPosition(const int trackIndex) const
 {
  if (trackIndex < 0 || trackIndex >= static_cast<int>(impl_->trackHeights_.size()))
  {
   return 0.0;
  }
  return impl_->trackTopY(trackIndex);
 }

 ClipItem* TimelineScene::addClip(const int trackIndex, const double start, const double duration)
 {
  if (trackIndex < 0 || trackIndex >= static_cast<int>(impl_->trackHeights_.size()))
  {
   return nullptr;
  }

  const double yPos = getTrackYPosition(trackIndex);
  const double height = impl_->trackHeights_[trackIndex];

  auto* clip = createClipItem(start, duration, height);
  clip->setPos(std::max(0.0, start), yPos);
  addItem(clip);
  impl_->clips_.push_back(clip);
  impl_->clipTracks_[clip] = trackIndex;

  QObject::connect(clip, &ClipItem::dragMoved, this, [this](ClipItem*, const double, const double sceneY) {
   const int hoverTrack = getTrackAtPosition(sceneY);
   if (hoverTrack >= 0)
   {
    highlightTrack(hoverTrack);
   }
   else
   {
    clearTrackHighlight();
   }
  });

  QObject::connect(clip, &ClipItem::dragStarted, this, [this, clip](ClipItem*) {
   impl_->beginDrag(clip);
  });

  QObject::connect(clip, &ClipItem::dragEnded, this, [this, clip](ClipItem*, const double sceneX, const double sceneY) {
   if (!impl_->drag_.active)
   {
    impl_->beginDrag(clip);
   }

   int dropTrack = getTrackAtPosition(sceneY);
   if (dropTrack < 0)
   {
    if (impl_->trackHeights_.empty())
    {
      clearTrackHighlight();
      return;
     }
    dropTrack = sceneY < 0.0 ? 0 : static_cast<int>(impl_->trackHeights_.size()) - 1;
   }

   const int trackDelta = dropTrack - impl_->drag_.anchorTrack;
   const double anchorSnapped = impl_->snapX(sceneX, clip, dropTrack, impl_->drag_.movingClips);
   const double deltaX = anchorSnapped - impl_->drag_.anchorStartX;

   std::unordered_set<ClipItem*> moved;
   moved.reserve(impl_->drag_.movingClips.size());
   for (auto* movingClip : impl_->drag_.movingClips)
   {
    if (!movingClip)
    {
     continue;
    }
    const auto posIt = impl_->drag_.initialPos.find(movingClip);
    const auto trackIt = impl_->drag_.initialTrack.find(movingClip);
    if (posIt == impl_->drag_.initialPos.end() || trackIt == impl_->drag_.initialTrack.end())
    {
     continue;
    }

    const int srcTrack = trackIt->second;
    int dstTrack = std::clamp(srcTrack + trackDelta, 0, static_cast<int>(impl_->trackHeights_.size()) - 1);
    impl_->clipTracks_[movingClip] = dstTrack;

    const double dstX = std::max(0.0, posIt->second.x() + deltaX);
    movingClip->setPos(dstX, getTrackYPosition(dstTrack));
    moved.insert(movingClip);
   }

   if (impl_->rippleEditEnabled_)
   {
    std::unordered_set<int> affectedTracks;
    for (auto* movingClip : impl_->drag_.movingClips)
    {
     affectedTracks.insert(impl_->clipTrack(movingClip));
    }
    for (const int affectedTrack : affectedTracks)
    {
     if (affectedTrack >= 0)
     {
      impl_->resolveOverlapsWithRipple(affectedTrack, moved);
     }
    }
   }

   impl_->drag_.active = false;
   impl_->drag_.movingClips.clear();
   impl_->drag_.initialPos.clear();
   impl_->drag_.initialTrack.clear();
   clearTrackHighlight();
  });

  return clip;
 }

 void TimelineScene::removeClip(ClipItem* clip)
 {
  const auto it = std::find(impl_->clips_.begin(), impl_->clips_.end(), clip);
  if (it == impl_->clips_.end())
  {
   return;
  }

  impl_->selectedClips_.erase(std::remove(impl_->selectedClips_.begin(), impl_->selectedClips_.end(), clip),
   impl_->selectedClips_.end());
  impl_->clipTracks_.erase(clip);
  removeItem(clip);
  destroyClipItem(clip);
  impl_->clips_.erase(it);
 }

 const std::vector<ClipItem*>& TimelineScene::getClips() const
 {
  return impl_->clips_;
 }

 int TimelineScene::getTrackAtPosition(const double yPos) const
 {
  double currentY = 0.0;
  for (int i = 0; i < static_cast<int>(impl_->trackHeights_.size()); ++i)
  {
   const double nextY = currentY + impl_->trackHeights_[i];
   if (yPos >= currentY && yPos < nextY)
   {
    return i;
   }
   currentY = nextY;
  }
  return -1;
 }

void TimelineScene::clearSelection()
{
 QGraphicsScene::clearSelection();
 impl_->selectedClips_.clear();
}

 const std::vector<ClipItem*>& TimelineScene::getSelectedClips() const
 {
  return impl_->selectedClips_;
 }
}
