module;
#include <utility>
#include <algorithm>
#include <cmath>
#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QPointF>
#include <QRectF>
#include <QColor>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QPainterPath>
#include <wobjectimpl.h>

module Widget.CurveEditor;

namespace ArtifactCore {

W_OBJECT_IMPL(ArtifactCurveEditorWidget)

class ArtifactCurveEditorWidget::Impl {
public:
 ArtifactCurveEditorWidget* owner_ = nullptr;

 std::vector<CurveTrack> tracks_;
 int64_t currentFrame_ = 0;

 // View range (data coordinates)
 float xMin_ = 0.0f;
 float xMax_ = 100.0f;
 float yMin_ = -10.0f;
 float yMax_ = 110.0f;

 // Interaction state
 enum class DragMode { None, Pan, MoveKey, MoveHandleIn, MoveHandleOut, ScrubPlayhead };
 DragMode dragMode_ = DragMode::None;
 QPoint dragStart_;
 float dragStartXMin_, dragStartXMax_, dragStartYMin_, dragStartYMax_;
 int dragTrackIndex_ = -1;
 int dragKeyIndex_ = -1;
 int64_t dragOrigFrame_ = 0;
 float dragOrigValue_ = 0.0f;
 float dragOrigInTangent_ = 0.0f;
 float dragOrigOutTangent_ = 0.0f;

 // Selection
 int selectedTrack_ = -1;
 int selectedKey_ = -1;
 bool handlesInteractive_ = true;

 static constexpr int KEY_RADIUS = 5;
 static constexpr int HANDLE_RADIUS = 4;
 static constexpr int MARGIN_LEFT = 50;
 static constexpr int MARGIN_TOP = 20;
 static constexpr int MARGIN_RIGHT = 20;
 static constexpr int MARGIN_BOTTOM = 30;

 Impl(ArtifactCurveEditorWidget* owner) : owner_(owner) {}

 QRectF plotRect() const {
  return QRectF(
   MARGIN_LEFT, MARGIN_TOP,
   owner_->width() - MARGIN_LEFT - MARGIN_RIGHT,
   owner_->height() - MARGIN_TOP - MARGIN_BOTTOM
  );
 }

 // Convert data coords to pixel coords
 QPointF dataToPixel(float frame, float value) const {
  QRectF pr = plotRect();
  float px = pr.left() + (frame - xMin_) / (xMax_ - xMin_) * pr.width();
  float py = pr.top() + (1.0f - (value - yMin_) / (yMax_ - yMin_)) * pr.height();
  return QPointF(px, py);
 }

 // Convert pixel coords to data coords
 QPointF pixelToData(QPointF pixel) const {
  QRectF pr = plotRect();
  float frame = xMin_ + (pixel.x() - pr.left()) / pr.width() * (xMax_ - xMin_);
  float value = yMin_ + (1.0f - (pixel.y() - pr.top()) / pr.height()) * (yMax_ - yMin_);
  return QPointF(frame, value);
 }

 // Cubic bezier interpolation for curve drawing
 static float bezierValue(float t, float p0, float p1, float p2, float p3) {
  float u = 1.0f - t;
  return u*u*u*p0 + 3*u*u*t*p1 + 3*u*t*t*p2 + t*t*t*p3;
 }

 // Estimate tangent for keyframe (slope between neighbors)
 static float estimateTangent(const std::vector<CurveKey>& keys, int index) {
  int n = static_cast<int>(keys.size());
  if (n <= 1) return 0.0f;
  if (index == 0) {
   float df = static_cast<float>(keys[1].frame - keys[0].frame);
   return df > 0 ? (keys[1].value - keys[0].value) / df : 0.0f;
  }
  if (index == n - 1) {
   float df = static_cast<float>(keys[n-1].frame - keys[n-2].frame);
   return df > 0 ? (keys[n-1].value - keys[n-2].value) / df : 0.0f;
  }
  float df = static_cast<float>(keys[index+1].frame - keys[index-1].frame);
  return df > 0 ? (keys[index+1].value - keys[index-1].value) / df : 0.0f;
 }

 // Get bezier control points for a segment
 static void getBezierControls(
  const CurveKey& k0, const CurveKey& k1,
  float& cp0Frame, float& cp0Value,
  float& cp1Frame, float& cp1Value)
 {
  float dt = static_cast<float>(k1.frame - k0.frame);
  if (dt <= 0) dt = 1.0f;

  cp0Frame = k0.frame + k0.outHandleFrame;
  cp0Value = k0.value + k0.outHandleValue;
  cp1Frame = k1.frame + k1.inHandleFrame;
  cp1Value = k1.value + k1.inHandleValue;
 }

 void drawGrid(QPainter& p) {
  QRectF pr = plotRect();

  // Background
  p.fillRect(pr, QColor(30, 30, 30));
  p.setPen(QPen(QColor(50, 50, 50), 1));
  p.drawRect(pr);

  // Grid lines - horizontal
  float yRange = yMax_ - yMin_;
  float yStep = niceStep(yRange, 8);
  float yStart = std::ceil(yMin_ / yStep) * yStep;
  QFont font("Consolas", 8);
  p.setFont(font);

  for (float y = yStart; y <= yMax_; y += yStep) {
   QPointF pos = dataToPixel(0, y);
   if (pos.y() >= pr.top() && pos.y() <= pr.bottom()) {
    p.setPen(QPen(QColor(50, 50, 50), 1));
    p.drawLine(QPointF(pr.left(), pos.y()), QPointF(pr.right(), pos.y()));
    p.setPen(QPen(QColor(120, 120, 120), 1));
    p.drawText(QPointF(4, pos.y() + 4), QString::number(y, 'f', 1));
   }
  }

  // Grid lines - vertical
  float xRange = xMax_ - xMin_;
  float xStep = niceStep(xRange, 10);
  float xStart = std::ceil(xMin_ / xStep) * xStep;

  for (float x = xStart; x <= xMax_; x += xStep) {
   QPointF pos = dataToPixel(x, 0);
   if (pos.x() >= pr.left() && pos.x() <= pr.right()) {
    p.setPen(QPen(QColor(50, 50, 50), 1));
    p.drawLine(QPointF(pos.x(), pr.top()), QPointF(pos.x(), pr.bottom()));
    p.setPen(QPen(QColor(120, 120, 120), 1));
    p.drawText(QPointF(pos.x() + 2, owner_->height() - 8), QString::number(x, 'f', 0));
   }
  }
 }

 static float niceStep(float range, int targetLines) {
  float rough = range / targetLines;
  float mag = std::pow(10.0f, std::floor(std::log10(rough)));
  float normalized = rough / mag;
  if (normalized < 1.5f) return 1.0f * mag;
  if (normalized < 3.5f) return 2.0f * mag;
  if (normalized < 7.5f) return 5.0f * mag;
  return 10.0f * mag;
 }

 void drawCurve(QPainter& p, const CurveTrack& track, int trackIndex) {
  if (!track.visible || track.keys.size() < 2) return;

  QRectF pr = plotRect();
  QPainterPath path;

  const auto& keys = track.keys;
  int n = static_cast<int>(keys.size());

  // Build bezier path through all keyframes
  QPointF startPos = dataToPixel(
   static_cast<float>(keys[0].frame), keys[0].value);
  path.moveTo(startPos);

  for (int i = 0; i < n - 1; ++i) {
   const auto& k0 = keys[i];
   const auto& k1 = keys[i+1];

   float cp0F, cp0V, cp1F, cp1V;
   getBezierControls(k0, k1, cp0F, cp0V, cp1F, cp1V);

   QPointF cp0 = dataToPixel(cp0F, cp0V);
   QPointF cp1 = dataToPixel(cp1F, cp1V);
   QPointF endPos = dataToPixel(static_cast<float>(k1.frame), k1.value);

   path.cubicTo(cp0, cp1, endPos);
  }

  QColor curveColor = track.color;
  p.setPen(QPen(curveColor, 2));
  p.setBrush(Qt::NoBrush);

  // Clip to plot rect
  p.save();
  p.setClipRect(pr);
  p.drawPath(path);
  p.restore();
 }

 void drawHandles(QPainter& p, const CurveTrack& track, int trackIndex) {
  if (!track.visible) return;
  QRectF pr = plotRect();

  p.save();
  p.setClipRect(pr);

  const auto& keys = track.keys;
  int n = static_cast<int>(keys.size());

  for (int i = 0; i < n; ++i) {
   const auto& key = keys[i];
   QPointF kp = dataToPixel(static_cast<float>(key.frame), key.value);

   // Draw tangent handles
   if (i > 0 && (selectedTrack_ == trackIndex && selectedKey_ == i)) {
    float cp1F = static_cast<float>(key.frame + key.inHandleFrame);
    float cp1V = key.value + key.inHandleValue;
    QPointF hp = dataToPixel(cp1F, cp1V);
    p.setPen(QPen(QColor(180, 180, 180), 1));
    p.drawLine(kp, hp);
    p.setPen(QPen(QColor(200, 200, 100), 1));
    p.setBrush(QColor(200, 200, 100));
    p.drawEllipse(hp, HANDLE_RADIUS, HANDLE_RADIUS);
   }

   if (i < n - 1 && (selectedTrack_ == trackIndex && selectedKey_ == i)) {
    float cp0F = static_cast<float>(key.frame + key.outHandleFrame);
    float cp0V = key.value + key.outHandleValue;
    QPointF hp = dataToPixel(cp0F, cp0V);
    p.setPen(QPen(QColor(180, 180, 180), 1));
    p.drawLine(kp, hp);
    p.setPen(QPen(QColor(200, 200, 100), 1));
    p.setBrush(QColor(200, 200, 100));
    p.drawEllipse(hp, HANDLE_RADIUS, HANDLE_RADIUS);
   }

   // Draw keyframe diamond
   bool isSelected = (selectedTrack_ == trackIndex && selectedKey_ == i);
   QColor keyColor = isSelected ? QColor(255, 255, 100) : track.color;
   QColor fillColor = isSelected ? QColor(255, 255, 100, 180) : track.color.darker(150);

   p.setPen(QPen(keyColor, isSelected ? 2 : 1));
   p.setBrush(fillColor);

   // Diamond shape
   QPolygonF diamond;
   diamond << QPointF(kp.x(), kp.y() - KEY_RADIUS)
           << QPointF(kp.x() + KEY_RADIUS, kp.y())
           << QPointF(kp.x(), kp.y() + KEY_RADIUS)
           << QPointF(kp.x() - KEY_RADIUS, kp.y());
   p.drawPolygon(diamond);
  }

  p.restore();
 }

 void drawPlayhead(QPainter& p) {
  QRectF pr = plotRect();
  QPointF pp = dataToPixel(static_cast<float>(currentFrame_), 0);

  if (pp.x() >= pr.left() && pp.x() <= pr.right()) {
   p.setPen(QPen(QColor(255, 80, 80), 2));
   p.drawLine(QPointF(pp.x(), pr.top()), QPointF(pp.x(), pr.bottom()));

   // Triangle at top
   QPolygonF tri;
   tri << QPointF(pp.x() - 5, pr.top())
       << QPointF(pp.x() + 5, pr.top())
       << QPointF(pp.x(), pr.top() + 8);
   p.setBrush(QColor(255, 80, 80));
   p.setPen(Qt::NoPen);
   p.drawPolygon(tri);
  }
 }

 // Hit test for keyframe points
 int hitTestKey(QPointF pixel, int& outTrackIndex, int& outKeyIndex) const {
  outTrackIndex = -1;
  outKeyIndex = -1;
  float bestDist = (KEY_RADIUS + 3) * (KEY_RADIUS + 3);

  for (int ti = 0; ti < static_cast<int>(tracks_.size()); ++ti) {
   if (!tracks_[ti].visible) continue;
   const auto& keys = tracks_[ti].keys;
   for (int ki = 0; ki < static_cast<int>(keys.size()); ++ki) {
    QPointF kp = dataToPixel(static_cast<float>(keys[ki].frame), keys[ki].value);
    float dx = static_cast<float>(pixel.x() - kp.x());
    float dy = static_cast<float>(pixel.y() - kp.y());
    float dist = dx*dx + dy*dy;
    if (dist < bestDist) {
     bestDist = dist;
     outTrackIndex = ti;
     outKeyIndex = ki;
    }
   }
  }
  return outTrackIndex >= 0 ? 0 : -1;
 }

 // Hit test for tangent handles
 int hitTestHandle(QPointF pixel, int& outTrackIndex, int& outKeyIndex, bool& outInHandle) const {
  if (!handlesInteractive_) {
   outTrackIndex = -1;
   outKeyIndex = -1;
   outInHandle = false;
   return -1;
  }

  outTrackIndex = -1;
  outKeyIndex = -1;
  outInHandle = false;
  float bestDist = (HANDLE_RADIUS + 4) * (HANDLE_RADIUS + 4);

  for (int ti = 0; ti < static_cast<int>(tracks_.size()); ++ti) {
   if (!tracks_[ti].visible) continue;
   if (selectedTrack_ != ti) continue;
   const auto& keys = tracks_[ti].keys;
   for (int ki = 0; ki < static_cast<int>(keys.size()); ++ki) {
    if (selectedTrack_ == ti && selectedKey_ == ki) {
     // Check in-handle
     if (ki > 0) {
      float hF = static_cast<float>(keys[ki].frame + keys[ki].inHandleFrame);
      float hV = keys[ki].value + keys[ki].inHandleValue;
      QPointF hp = dataToPixel(hF, hV);
      float dx = static_cast<float>(pixel.x() - hp.x());
      float dy = static_cast<float>(pixel.y() - hp.y());
      float dist = dx*dx + dy*dy;
      if (dist < bestDist) {
       bestDist = dist;
       outTrackIndex = ti;
       outKeyIndex = ki;
       outInHandle = true;
      }
     }
     // Check out-handle
     if (ki < static_cast<int>(keys.size()) - 1) {
      float hF = static_cast<float>(keys[ki].frame + keys[ki].outHandleFrame);
      float hV = keys[ki].value + keys[ki].outHandleValue;
      QPointF hp = dataToPixel(hF, hV);
      float dx = static_cast<float>(pixel.x() - hp.x());
      float dy = static_cast<float>(pixel.y() - hp.y());
      float dist = dx*dx + dy*dy;
      if (dist < bestDist) {
       bestDist = dist;
       outTrackIndex = ti;
       outKeyIndex = ki;
       outInHandle = false;
      }
     }
    }
   }
  }
  return outTrackIndex >= 0 ? 0 : -1;
 }

 bool deleteSelectedKey() {
  if (selectedTrack_ < 0 || selectedTrack_ >= static_cast<int>(tracks_.size()) ||
      selectedKey_ < 0 || selectedKey_ >= static_cast<int>(tracks_[selectedTrack_].keys.size())) {
   return false;
  }
  auto& keys = tracks_[selectedTrack_].keys;
  keys.erase(keys.begin() + selectedKey_);
  if (keys.empty()) {
   selectedTrack_ = -1;
   selectedKey_ = -1;
  } else {
   selectedKey_ = std::min(selectedKey_, static_cast<int>(keys.size()) - 1);
  }
  return true;
 }
};

ArtifactCurveEditorWidget::ArtifactCurveEditorWidget(QWidget* parent)
 : QWidget(parent), impl_(new Impl(this))
{
 setMouseTracking(true);
 setMinimumHeight(120);
 setFocusPolicy(Qt::StrongFocus);
}

ArtifactCurveEditorWidget::~ArtifactCurveEditorWidget() {
 delete impl_;
}

void ArtifactCurveEditorWidget::setTracks(const std::vector<CurveTrack>& tracks) {
 impl_->tracks_ = tracks;
 update();
}

void ArtifactCurveEditorWidget::setViewRange(float xMin, float xMax, float yMin, float yMax) {
 impl_->xMin_ = xMin;
 impl_->xMax_ = xMax;
 impl_->yMin_ = yMin;
 impl_->yMax_ = yMax;
 update();
}

void ArtifactCurveEditorWidget::setCurrentFrame(int64_t frame) {
 impl_->currentFrame_ = frame;
 update();
}

void ArtifactCurveEditorWidget::setHandleEditingEnabled(bool enabled) {
 impl_->handlesInteractive_ = enabled;
}

void ArtifactCurveEditorWidget::fitToContent() {
 float minF = 1e30f, maxF = -1e30f;
 float minV = 1e30f, maxV = -1e30f;
 bool hasKeys = false;

 for (const auto& track : impl_->tracks_) {
  for (const auto& key : track.keys) {
   float f = static_cast<float>(key.frame);
   minF = std::min(minF, f);
   maxF = std::max(maxF, f);
   minV = std::min(minV, key.value);
   maxV = std::max(maxV, key.value);
   hasKeys = true;
  }
 }

 if (!hasKeys) {
  setViewRange(0, 100, -10, 110);
  return;
 }

 float marginF = std::max((maxF - minF) * 0.1f, 5.0f);
 float marginV = std::max((maxV - minV) * 0.1f, 5.0f);
 setViewRange(minF - marginF, maxF + marginF, minV - marginV, maxV + marginV);
}

void ArtifactCurveEditorWidget::focusTrack(int trackIndex) {
 if (trackIndex < 0 || trackIndex >= static_cast<int>(impl_->tracks_.size())) {
  for (auto &track : impl_->tracks_) {
   track.visible = true;
  }
  impl_->selectedTrack_ = -1;
  impl_->selectedKey_ = -1;
  fitToContent();
  return;
 }

 for (int i = 0; i < static_cast<int>(impl_->tracks_.size()); ++i) {
  impl_->tracks_[i].visible = (i == trackIndex);
 }
 impl_->selectedTrack_ = trackIndex;
 impl_->selectedKey_ = impl_->tracks_[trackIndex].keys.empty() ? -1 : 0;

 float minF = 1e30f, maxF = -1e30f;
 float minV = 1e30f, maxV = -1e30f;
 bool hasKeys = false;
 for (const auto &key : impl_->tracks_[trackIndex].keys) {
  const float frame = static_cast<float>(key.frame);
  minF = std::min(minF, frame);
  maxF = std::max(maxF, frame);
  minV = std::min(minV, key.value);
  maxV = std::max(maxV, key.value);
  hasKeys = true;
 }

 if (!hasKeys) {
  update();
  return;
 }

 const float marginF = std::max((maxF - minF) * 0.12f, 5.0f);
 const float marginV = std::max((maxV - minV) * 0.15f, 5.0f);
 setViewRange(minF - marginF, maxF + marginF, minV - marginV, maxV + marginV);
}

void ArtifactCurveEditorWidget::paintEvent(QPaintEvent* /*event*/) {
 QPainter p(this);
 p.setRenderHint(QPainter::Antialiasing);

 // Background
 p.fillRect(rect(), QColor(25, 25, 25));

 // Grid
 impl_->drawGrid(p);

 // Curves
 for (int ti = 0; ti < static_cast<int>(impl_->tracks_.size()); ++ti) {
  impl_->drawCurve(p, impl_->tracks_[ti], ti);
 }

 // Handles (only for selected track)
 for (int ti = 0; ti < static_cast<int>(impl_->tracks_.size()); ++ti) {
  if (impl_->selectedTrack_ == ti) {
   impl_->drawHandles(p, impl_->tracks_[ti], ti);
  }
 }

 // Keyframe points (for all tracks)
 for (int ti = 0; ti < static_cast<int>(impl_->tracks_.size()); ++ti) {
  impl_->drawHandles(p, impl_->tracks_[ti], ti);
 }

 // Playhead
 impl_->drawPlayhead(p);

 // Track names
 QFont font("Consolas", 9);
 p.setFont(font);
 int nameY = 14;
 for (int ti = 0; ti < static_cast<int>(impl_->tracks_.size()); ++ti) {
  if (!impl_->tracks_[ti].visible) continue;
  p.setPen(impl_->tracks_[ti].color);
  p.drawText(QPointF(impl_->MARGIN_LEFT + 4, nameY), impl_->tracks_[ti].name);
  nameY += 14;
 }
}

void ArtifactCurveEditorWidget::mousePressEvent(QMouseEvent* event) {
 QPointF pos = event->position();
 bool startedInteraction = false;

 // Check if clicking on playhead area (bottom margin) for scrubbing
 QRectF pr = impl_->plotRect();
 if (pos.y() > pr.bottom() && pos.y() < pr.bottom() + impl_->MARGIN_BOTTOM) {
  impl_->dragMode_ = Impl::DragMode::ScrubPlayhead;
  startedInteraction = true;
  QPointF data = impl_->pixelToData(pos);
  int64_t frame = static_cast<int64_t>(std::round(data.x()));
  impl_->currentFrame_ = frame;
  Q_EMIT currentFrameChanged(frame);
  if (startedInteraction) {
   Q_EMIT interactionStarted();
  }
  update();
  return;
 }

 // Check for handle hit first
 int ht, hk;
 bool inHandle;
 if (impl_->hitTestHandle(pos, ht, hk, inHandle) == 0) {
  impl_->dragMode_ = inHandle ? Impl::DragMode::MoveHandleIn : Impl::DragMode::MoveHandleOut;
  startedInteraction = true;
  impl_->dragTrackIndex_ = ht;
  impl_->dragKeyIndex_ = hk;
  impl_->dragStart_ = pos.toPoint();
  impl_->selectedTrack_ = ht;
  impl_->selectedKey_ = hk;
  if (startedInteraction) {
   Q_EMIT interactionStarted();
  }
  update();
  return;
 }

 // Check for key hit
 int tk, kk;
 if (impl_->hitTestKey(pos, tk, kk) == 0) {
  impl_->dragMode_ = Impl::DragMode::MoveKey;
  startedInteraction = true;
  impl_->dragTrackIndex_ = tk;
  impl_->dragKeyIndex_ = kk;
  impl_->dragStart_ = pos.toPoint();
  impl_->dragOrigFrame_ = impl_->tracks_[tk].keys[kk].frame;
  impl_->dragOrigValue_ = impl_->tracks_[tk].keys[kk].value;
  impl_->selectedTrack_ = tk;
  impl_->selectedKey_ = kk;
  Q_EMIT keySelected(tk, kk);
  if (startedInteraction) {
   Q_EMIT interactionStarted();
  }
  update();
  return;
 }

 // Pan
 impl_->dragMode_ = Impl::DragMode::Pan;
 impl_->dragStart_ = pos.toPoint();
 impl_->dragStartXMin_ = impl_->xMin_;
 impl_->dragStartXMax_ = impl_->xMax_;
 impl_->dragStartYMin_ = impl_->yMin_;
 impl_->dragStartYMax_ = impl_->yMax_;
 startedInteraction = true;

 // Deselect
 impl_->selectedTrack_ = -1;
 impl_->selectedKey_ = -1;
 if (startedInteraction) {
  Q_EMIT interactionStarted();
 }
 update();
}

void ArtifactCurveEditorWidget::mouseMoveEvent(QMouseEvent* event) {
 QPointF pos = event->position();
 QPoint delta = pos.toPoint() - impl_->dragStart_;

 switch (impl_->dragMode_) {
  case Impl::DragMode::Pan: {
   QRectF pr = impl_->plotRect();
   float dx = static_cast<float>(delta.x()) / pr.width() * (impl_->dragStartXMax_ - impl_->dragStartXMin_);
   float dy = static_cast<float>(delta.y()) / pr.height() * (impl_->dragStartYMax_ - impl_->dragStartYMin_);
   impl_->xMin_ = impl_->dragStartXMin_ - dx;
   impl_->xMax_ = impl_->dragStartXMax_ - dx;
   impl_->yMin_ = impl_->dragStartYMin_ + dy;
   impl_->yMax_ = impl_->dragStartYMax_ + dy;
   update();
   break;
  }

  case Impl::DragMode::MoveKey: {
   QPointF data = impl_->pixelToData(pos);
   QPointF origData = impl_->pixelToData(impl_->dragStart_);
   float frameDelta = static_cast<float>(data.x() - origData.x());
   float valueDelta = static_cast<float>(data.y() - origData.y());

   int64_t newFrame = impl_->dragOrigFrame_ + static_cast<int64_t>(std::round(frameDelta));
   float newValue = impl_->dragOrigValue_ + valueDelta;

   auto& key = impl_->tracks_[impl_->dragTrackIndex_].keys[impl_->dragKeyIndex_];
   key.frame = newFrame;
   key.value = newValue;

   Q_EMIT keyMoved(impl_->dragTrackIndex_, impl_->dragKeyIndex_, newFrame, newValue);
   update();
   break;
  }

  case Impl::DragMode::MoveHandleIn: {
   QPointF data = impl_->pixelToData(pos);
   auto& key = impl_->tracks_[impl_->dragTrackIndex_].keys[impl_->dragKeyIndex_];
   key.inHandleFrame = static_cast<int64_t>(data.x()) - key.frame;
   key.inHandleValue = static_cast<float>(data.y()) - key.value;
   update();
   break;
  }

  case Impl::DragMode::MoveHandleOut: {
   QPointF data = impl_->pixelToData(pos);
   auto& key = impl_->tracks_[impl_->dragTrackIndex_].keys[impl_->dragKeyIndex_];
   key.outHandleFrame = static_cast<int64_t>(data.x()) - key.frame;
   key.outHandleValue = static_cast<float>(data.y()) - key.value;
   update();
   break;
  }

  case Impl::DragMode::ScrubPlayhead: {
   QPointF data = impl_->pixelToData(pos);
   int64_t frame = static_cast<int64_t>(std::round(data.x()));
   impl_->currentFrame_ = frame;
   Q_EMIT currentFrameChanged(frame);
   update();
   break;
  }

  default:
   break;
 }
}

void ArtifactCurveEditorWidget::mouseReleaseEvent(QMouseEvent* /*event*/) {
 const bool hadDrag = impl_->dragMode_ != Impl::DragMode::None;
 if (impl_->dragMode_ == Impl::DragMode::MoveKey) {
  // Re-sort keys after move
  auto& keys = impl_->tracks_[impl_->dragTrackIndex_].keys;
  std::sort(keys.begin(), keys.end(),
   [](const CurveKey& a, const CurveKey& b) { return a.frame < b.frame; });

  // Find new index of moved key
  for (int i = 0; i < static_cast<int>(keys.size()); ++i) {
   if (keys[i].frame == impl_->dragOrigFrame_) {
    impl_->selectedKey_ = i;
    break;
   }
  }
 }

 impl_->dragMode_ = Impl::DragMode::None;
 if (hadDrag) {
  Q_EMIT interactionFinished();
 }
}

void ArtifactCurveEditorWidget::wheelEvent(QWheelEvent* event) {
 QPointF pos = event->position();
 QPointF data = impl_->pixelToData(pos);

 float zoomFactor = event->angleDelta().y() > 0 ? 0.9f : 1.1f;

 QRectF pr = impl_->plotRect();

 if (event->modifiers() & Qt::AltModifier) {
  // Alt + wheel: zoom Y
  float yCenter = static_cast<float>(data.y());
  float yRange = impl_->yMax_ - impl_->yMin_;
  float newYRange = yRange * zoomFactor;
  float ratio = (yCenter - impl_->yMin_) / yRange;
  impl_->yMin_ = yCenter - ratio * newYRange;
  impl_->yMax_ = yCenter + (1.0f - ratio) * newYRange;
 } else if (event->modifiers() & Qt::ShiftModifier) {
  // Shift + wheel: zoom X
  float xCenter = static_cast<float>(data.x());
  float xRange = impl_->xMax_ - impl_->xMin_;
  float newXRange = xRange * zoomFactor;
  float ratio = (xCenter - impl_->xMin_) / xRange;
  impl_->xMin_ = xCenter - ratio * newXRange;
  impl_->xMax_ = xCenter + (1.0f - ratio) * newXRange;
 } else {
  // Wheel: zoom both
  float xCenter = static_cast<float>(data.x());
  float yCenter = static_cast<float>(data.y());
  float xRange = impl_->xMax_ - impl_->xMin_;
  float yRange = impl_->yMax_ - impl_->yMin_;
  float newXRange = xRange * zoomFactor;
  float newYRange = yRange * zoomFactor;
  float xRatio = (xCenter - impl_->xMin_) / xRange;
  float yRatio = (yCenter - impl_->yMin_) / yRange;
  impl_->xMin_ = xCenter - xRatio * newXRange;
  impl_->xMax_ = xCenter + (1.0f - xRatio) * newXRange;
  impl_->yMin_ = yCenter - yRatio * newYRange;
  impl_->yMax_ = yCenter + (1.0f - yRatio) * newYRange;
 }

 update();
}

void ArtifactCurveEditorWidget::mouseDoubleClickEvent(QMouseEvent* event) {
 if (!event) {
  fitToContent();
  return;
 }

 int trackIndex = -1;
 int keyIndex = -1;
 if (impl_->hitTestKey(event->position(), trackIndex, keyIndex) == 0 && trackIndex >= 0) {
  focusTrack(trackIndex);
  event->accept();
  return;
 }

 // Background double-click resets to all tracks and fits.
 focusTrack(-1);
 event->accept();
}

void ArtifactCurveEditorWidget::keyPressEvent(QKeyEvent* event) {
 if (!event || event->isAutoRepeat()) {
  QWidget::keyPressEvent(event);
  return;
 }

 if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
  const int oldTrack = impl_->selectedTrack_;
  const int oldKey = impl_->selectedKey_;
  if (impl_->deleteSelectedKey()) {
   Q_EMIT keyDeleted(oldTrack, oldKey);
   update();
   event->accept();
   return;
  }
 }

 if (event->key() == Qt::Key_F) {
  if (impl_->selectedTrack_ >= 0) {
   focusTrack(impl_->selectedTrack_);
  } else {
   fitToContent();
  }
  event->accept();
  return;
 }

 if (event->key() == Qt::Key_A || event->key() == Qt::Key_Escape) {
  focusTrack(-1);
  event->accept();
  return;
 }

 QWidget::keyPressEvent(event);
}

 // ============================================================
 // Speed Graph Utilities
 // ============================================================

 // Sample speed from TimeRemap keyframes and create a CurveTrack for display.
 // Requires the Time.TimeRemap module (optional dependency).
 // Usage:
 //   auto track = sampleSpeedGraph(keyframes, startFrame, endFrame, frameRate);
 //   curveEditor->setTracks({track});

} // namespace ArtifactCore
