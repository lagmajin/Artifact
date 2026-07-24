module;
#include <utility>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <limits>
#include <vector>
#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QInputDialog>
#include <QWheelEvent>
#include <QPointF>
#include <QRectF>
#include <QColor>
#include <QDebug>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QPainterPath>
#include <QVector>
#include <wobjectimpl.h>

module Widget.CurveEditor;

import Frame.Rate;
import Time.TimeRemap;

namespace ArtifactCore {

namespace {

float clampSpeedPercent(float value) {
 return std::clamp(value, -1000.0f, 1000.0f);
}
}

CurveTrack sampleSpeedGraph(const QVector<TimeRemapKeyframe>& keyframes,
                            int64_t startFrame,
                            int64_t endFrame,
                            const FrameRate& frameRate) {
 CurveTrack track;
 track.name = QStringLiteral("Speed (%)");
 track.color = QColor(96, 196, 255);
 track.visible = true;

 float fps = frameRate.framerate();
 if (fps <= 0.0f) {
  fps = 30.0f;
 }

 if (endFrame < startFrame) {
  std::swap(startFrame, endFrame);
 }

 auto frameFromTime = [fps](double timeSeconds) -> int64_t {
  return static_cast<int64_t>(std::llround(timeSeconds * static_cast<double>(fps)));
 };

 if (keyframes.isEmpty()) {
  CurveKey left;
  left.frame = startFrame;
  left.value = 100.0f;
  CurveKey right = left;
  right.frame = endFrame;
  track.keys.push_back(left);
  track.keys.push_back(right);
  return track;
 }

 QVector<TimeRemapKeyframe> sorted = keyframes;
 std::sort(sorted.begin(), sorted.end(), [](const TimeRemapKeyframe& a, const TimeRemapKeyframe& b) {
  if (a.outputTime == b.outputTime) {
   return a.sourceTime < b.sourceTime;
  }
  return a.outputTime < b.outputTime;
 });

 struct SamplePoint {
  int64_t frame = 0;
  float speed = 100.0f;
 };

 std::vector<SamplePoint> samples;
 samples.reserve(static_cast<size_t>(sorted.size()) + 2);

 auto appendSample = [&](int64_t frame, float speed) {
  if (!samples.empty() && samples.back().frame == frame) {
   samples.back().speed = speed;
   return;
  }
  samples.push_back({frame, speed});
 };

 const TimeRemapKeyframe* prev = nullptr;
 float lastSpeed = 100.0f;
 for (const auto& key : sorted) {
  if (prev) {
   const double dt = key.outputTime - prev->outputTime;
   const double ds = key.sourceTime - prev->sourceTime;
   if (std::abs(dt) > 1e-9) {
    lastSpeed = clampSpeedPercent(static_cast<float>((ds / dt) * 100.0));
   }
  }

  appendSample(frameFromTime(key.outputTime), lastSpeed);
  prev = &key;
 }

 if (samples.empty()) {
  CurveKey point;
  point.frame = startFrame;
  point.value = 100.0f;
  track.keys.push_back(point);
  point.frame = endFrame;
  track.keys.push_back(point);
  return track;
 }

 if (samples.front().frame > startFrame) {
  samples.insert(samples.begin(), {startFrame, samples.front().speed});
 }
 if (samples.back().frame < endFrame) {
  samples.push_back({endFrame, samples.back().speed});
 }

 std::sort(samples.begin(), samples.end(), [](const SamplePoint& a, const SamplePoint& b) {
  if (a.frame == b.frame) {
   return a.speed < b.speed;
  }
  return a.frame < b.frame;
 });

 for (size_t i = 0; i < samples.size(); ++i) {
  const auto& sample = samples[i];
  CurveKey key;
  key.frame = sample.frame;
  key.value = sample.speed;
  key.smooth = true;
  if (i > 0) {
   const auto& prevSample = samples[i - 1];
   const float df = static_cast<float>(std::max<int64_t>(1, sample.frame - prevSample.frame));
   key.inTangent = (sample.speed - prevSample.speed) / df;
   key.inHandleFrame = -std::max<int64_t>(1, static_cast<int64_t>(std::round(df * 0.25f)));
   key.inHandleValue = key.inTangent * static_cast<float>(-key.inHandleFrame);
  }
  if (i + 1 < samples.size()) {
   const auto& nextSample = samples[i + 1];
   const float df = static_cast<float>(std::max<int64_t>(1, nextSample.frame - sample.frame));
   key.outTangent = (nextSample.speed - sample.speed) / df;
   key.outHandleFrame = std::max<int64_t>(1, static_cast<int64_t>(std::round(df * 0.25f)));
   key.outHandleValue = key.outTangent * static_cast<float>(key.outHandleFrame);
  }
  track.keys.push_back(key);
 }

 if (track.keys.size() == 1) {
  CurveKey extra = track.keys.front();
  extra.frame = endFrame;
  track.keys.push_back(extra);
 }

 return track;
}

W_OBJECT_IMPL(ArtifactCurveEditorWidget)

class ArtifactCurveEditorWidget::Impl {
public:
 ArtifactCurveEditorWidget* owner_ = nullptr;

 std::vector<CurveTrack> tracks_;
 std::vector<CurveTrack> bufferTracks_;
 bool bufferCurveVisible_ = false;
 std::vector<CurveKey> copiedKeys_;
 int copiedSourceTrack_ = -1;
 bool normalizedView_ = false;
 int64_t currentFrame_ = 0;

 // View range (data coordinates)
 float xMin_ = 0.0f;
 float xMax_ = 100.0f;
 float yMin_ = -10.0f;
 float yMax_ = 110.0f;

 // Interaction state
 enum class DragMode { None, Pan, MoveKey, MoveHandleIn, MoveHandleOut, ScrubPlayhead, Marquee };
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
 // CE-3: multi-key selection. selectedTrack_/selectedKey_ remain the
 // primary selection used for tangent handle display and editing.
 std::set<std::pair<int, int>> selectedKeys_;
 struct DraggedKey {
  int track = -1;
  int key = -1;
  int64_t frame = 0;
  float value = 0.0f;
  int64_t finalFrame = 0;
  float finalValue = 0.0f;
 };
 std::vector<DraggedKey> draggedKeys_;
 bool marqueeAdditive_ = false;
 QRectF marqueeRectData_;  // data-space rect while DragMode::Marquee
 bool handlesInteractive_ = true;
  bool keyEditingEnabled_ = true;

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

  // Baseline at y=0, if it is in view.
  if (yMin_ <= 0.0f && yMax_ >= 0.0f) {
   const QPointF zeroPos = dataToPixel(0.0f, 0.0f);
   p.setPen(QPen(QColor(120, 120, 140), 1));
   p.drawLine(QPointF(pr.left(), zeroPos.y()), QPointF(pr.right(), zeroPos.y()));
  }

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

 float displayValue(const CurveTrack& track, float value) const {
  if (!normalizedView_ || track.keys.empty()) {
   return value;
  }
  float minValue = track.keys.front().value;
  float maxValue = minValue;
  for (const auto& key : track.keys) {
   minValue = std::min(minValue, key.value);
   maxValue = std::max(maxValue, key.value);
  }
  const float span = maxValue - minValue;
  return span > 0.0001f ? -1.0f + 2.0f * (value - minValue) / span : 0.0f;
 }

 float displayDeltaToValue(const CurveTrack& track, float delta) const {
  if (!normalizedView_ || track.keys.empty()) return delta;
  float minValue = track.keys.front().value;
  float maxValue = minValue;
  for (const auto& key : track.keys) {
   minValue = std::min(minValue, key.value);
   maxValue = std::max(maxValue, key.value);
  }
  return delta * (maxValue - minValue) * 0.5f;
 }

 float displayToValue(const CurveTrack& track, float display) const {
  if (!normalizedView_ || track.keys.empty()) return display;
  float minValue = track.keys.front().value;
  float maxValue = minValue;
  for (const auto& key : track.keys) {
   minValue = std::min(minValue, key.value);
   maxValue = std::max(maxValue, key.value);
  }
  const float span = maxValue - minValue;
  return span > 0.0001f ? minValue + (display + 1.0f) * span * 0.5f : minValue;
 }

 QPointF trackToPixel(const CurveTrack& track, float frame, float value) const {
  return dataToPixel(frame, displayValue(track, value));
 }

 void drawCurve(QPainter& p, const CurveTrack& track, int trackIndex,
                bool buffer = false) {
  if (!track.visible || track.keys.size() < 2) return;

  QRectF pr = plotRect();
  QPainterPath path;

  const auto& keys = track.keys;
  int n = static_cast<int>(keys.size());

  // Build bezier path through all keyframes
  QPointF startPos = trackToPixel(track,
   static_cast<float>(keys[0].frame), keys[0].value);
  path.moveTo(startPos);

  for (int i = 0; i < n - 1; ++i) {
   const auto& k0 = keys[i];
   const auto& k1 = keys[i+1];

   QPointF endPos = trackToPixel(track, static_cast<float>(k1.frame), k1.value);
   if (k0.constant) {
    path.lineTo(trackToPixel(track, static_cast<float>(k1.frame), k0.value));
    path.lineTo(endPos);
   } else {
    float cp0F, cp0V, cp1F, cp1V;
    getBezierControls(k0, k1, cp0F, cp0V, cp1F, cp1V);
    path.cubicTo(trackToPixel(track, cp0F, cp0V),
                 trackToPixel(track, cp1F, cp1V), endPos);
   }
  }

  QColor curveColor = track.color;
  if (buffer) {
   curveColor.setAlpha(90);
  }
  const bool focusedTrack = (selectedTrack_ == trackIndex);
  if (focusedTrack) {
   curveColor = curveColor.lighter(130);
  }
  p.setPen(QPen(curveColor, focusedTrack ? 3 : 2));
  p.setBrush(Qt::NoBrush);

  // Clip to plot rect
  p.save();
  p.setClipRect(pr);
  // CE-11: non-destructive linear Infinity preview outside the keyed range.
  // The key data remains unchanged; the dashed extensions only communicate
  // the extrapolation direction while the curve editor is in view.
  if (!buffer && keys.size() >= 2) {
   const auto& first = keys.front();
   const auto& second = keys[1];
   const auto& penultimate = keys[keys.size() - 2];
   const auto& last = keys.back();
   const float inFrameSpan = static_cast<float>(second.frame - first.frame);
   const float outFrameSpan = static_cast<float>(last.frame - penultimate.frame);
   const float inSlope = std::abs(inFrameSpan) > 0.0001f
       ? (second.value - first.value) / inFrameSpan : 0.0f;
   const float outSlope = std::abs(outFrameSpan) > 0.0001f
       ? (last.value - penultimate.value) / outFrameSpan : 0.0f;
   const float preFrame = std::min(xMin_, static_cast<float>(first.frame));
   const float postFrame = std::max(xMax_, static_cast<float>(last.frame));
   QPainterPath infinityPath;
   infinityPath.moveTo(trackToPixel(track, preFrame,
       first.value + (preFrame - static_cast<float>(first.frame)) * inSlope));
   infinityPath.lineTo(trackToPixel(track, static_cast<float>(first.frame), first.value));
   infinityPath.moveTo(trackToPixel(track, static_cast<float>(last.frame), last.value));
   infinityPath.lineTo(trackToPixel(track, postFrame,
       last.value + (postFrame - static_cast<float>(last.frame)) * outSlope));
   QColor infinityColor = curveColor;
   infinityColor.setAlpha(115);
   p.setPen(QPen(infinityColor, 1.0, Qt::DashLine));
   p.drawPath(infinityPath);
   p.setPen(QPen(curveColor, focusedTrack ? 3 : 2));

   // Repeat the keyed segment as a cycle (pre/post infinity).  Translation
   // in pixel space preserves the authored curve shape and tangent geometry.
   const float periodFrames = static_cast<float>(last.frame - first.frame);
   if (periodFrames > 0.0f) {
    const float periodPixels =
        dataToPixel(static_cast<float>(first.frame) + periodFrames, 0.0f).x() -
        dataToPixel(static_cast<float>(first.frame), 0.0f).x();
    if (periodPixels > 0.01f) {
     QPen cyclePen(curveColor, 1.0, Qt::DashLine);
     cyclePen.setColor(QColor(curveColor.red(), curveColor.green(),
                              curveColor.blue(), 95));
     p.setPen(cyclePen);
     const float firstPixel = dataToPixel(static_cast<float>(first.frame), 0.0f).x();
     const float lastPixel = dataToPixel(static_cast<float>(last.frame), 0.0f).x();
     for (float shift = -periodPixels;
          firstPixel + shift > pr.left() - periodPixels;
          shift -= periodPixels) {
      if (lastPixel + shift < pr.left()) break;
      p.save();
      p.translate(shift, 0.0);
      p.drawPath(path);
      p.restore();
     }
     for (float shift = periodPixels;
          lastPixel + shift < pr.right() + periodPixels;
          shift += periodPixels) {
      if (firstPixel + shift > pr.right()) break;
      p.save();
      p.translate(shift, 0.0);
      p.drawPath(path);
      p.restore();
     }
     p.setPen(QPen(curveColor, focusedTrack ? 3 : 2));
    }
   }
  }
  p.drawPath(path);
  p.restore();
 }

 void captureBufferCurve() {
  if (bufferTracks_.empty()) {
   bufferTracks_ = tracks_;
  }
  bufferCurveVisible_ = true;
 }

 bool copySelectedKeys() {
  copiedKeys_.clear();
  copiedSourceTrack_ = selectedTrack_;
  if (copiedSourceTrack_ < 0) return false;
  for (const auto& selection : selectedKeys_) {
   if (selection.first != copiedSourceTrack_ ||
       selection.second < 0 ||
       selection.second >= static_cast<int>(tracks_[copiedSourceTrack_].keys.size())) {
    continue;
   }
   copiedKeys_.push_back(tracks_[copiedSourceTrack_].keys[selection.second]);
  }
  if (copiedKeys_.empty() && selectedKey_ >= 0 &&
      selectedKey_ < static_cast<int>(tracks_[copiedSourceTrack_].keys.size())) {
   copiedKeys_.push_back(tracks_[copiedSourceTrack_].keys[selectedKey_]);
  }
  std::sort(copiedKeys_.begin(), copiedKeys_.end(),
            [](const CurveKey& lhs, const CurveKey& rhs) { return lhs.frame < rhs.frame; });
  return !copiedKeys_.empty();
 }

 bool pasteCopiedKeys() {
  if (copiedKeys_.empty() || selectedTrack_ < 0 ||
      selectedTrack_ >= static_cast<int>(tracks_.size())) return false;
  const int64_t anchorFrame = selectedKey_ >= 0 &&
      selectedKey_ < static_cast<int>(tracks_[selectedTrack_].keys.size())
      ? tracks_[selectedTrack_].keys[selectedKey_].frame : copiedKeys_.front().frame;
  const int64_t frameOffset = anchorFrame - copiedKeys_.front().frame;
  auto& targetKeys = tracks_[selectedTrack_].keys;
  std::set<int64_t> pastedFrames;
  for (const auto& sourceKey : copiedKeys_) {
   CurveKey pasted = sourceKey;
   pasted.frame += frameOffset;
   auto existing = std::find_if(targetKeys.begin(), targetKeys.end(),
       [&](const CurveKey& key) { return key.frame == pasted.frame; });
   if (existing != targetKeys.end()) {
    *existing = pasted;
   } else {
    targetKeys.push_back(pasted);
   }
   pastedFrames.insert(pasted.frame);
  }
  std::sort(targetKeys.begin(), targetKeys.end(),
            [](const CurveKey& lhs, const CurveKey& rhs) { return lhs.frame < rhs.frame; });
  selectedKeys_.clear();
  for (int index = 0; index < static_cast<int>(targetKeys.size()); ++index) {
   if (pastedFrames.count(targetKeys[index].frame)) {
    selectedKeys_.insert({selectedTrack_, index});
   }
  }
  selectedKey_ = selectedKeys_.empty() ? -1 : selectedKeys_.begin()->second;
  return !selectedKeys_.empty();
 }

 bool cloneSelectedKeys() {
  if (selectedTrack_ < 0 || selectedTrack_ >= static_cast<int>(tracks_.size()) ||
      selectedKeys_.empty()) {
   return false;
  }

  auto& targetKeys = tracks_[selectedTrack_].keys;
  std::vector<CurveKey> sourceKeys;
  for (const auto& selection : selectedKeys_) {
   if (selection.first != selectedTrack_ || selection.second < 0 ||
       selection.second >= static_cast<int>(targetKeys.size())) {
    continue;
   }
   sourceKeys.push_back(targetKeys[selection.second]);
  }
  if (sourceKeys.empty()) {
   return false;
  }

  std::sort(sourceKeys.begin(), sourceKeys.end(),
            [](const CurveKey& lhs, const CurveKey& rhs) {
              return lhs.frame < rhs.frame;
            });
  const int64_t sourceStart = sourceKeys.front().frame;
  const int64_t sourceEnd = sourceKeys.back().frame;
  const int64_t offset = std::max<int64_t>(1, sourceEnd - sourceStart + 1);

  std::set<int64_t> clonedFrames;
  for (const auto& sourceKey : sourceKeys) {
   CurveKey clone = sourceKey;
   clone.frame += offset;
   auto existing = std::find_if(targetKeys.begin(), targetKeys.end(),
       [&](const CurveKey& key) { return key.frame == clone.frame; });
   if (existing != targetKeys.end()) {
    *existing = clone;
   } else {
    targetKeys.push_back(clone);
   }
   clonedFrames.insert(clone.frame);
  }

  std::sort(targetKeys.begin(), targetKeys.end(),
            [](const CurveKey& lhs, const CurveKey& rhs) {
              return lhs.frame < rhs.frame;
            });
  selectedKeys_.clear();
  for (int index = 0; index < static_cast<int>(targetKeys.size()); ++index) {
   if (clonedFrames.count(targetKeys[index].frame)) {
    selectedKeys_.insert({selectedTrack_, index});
   }
  }
  selectedKey_ = selectedKeys_.empty() ? -1 : selectedKeys_.begin()->second;
  return !selectedKeys_.empty();
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
   QPointF kp = trackToPixel(track, static_cast<float>(key.frame), key.value);

   // Draw tangent handles
   const bool isSelected = isKeySelected(trackIndex, i);
   if (i > 0 && isSelected) {
    float cp1F = static_cast<float>(key.frame + key.inHandleFrame);
    float cp1V = key.value + key.inHandleValue;
    QPointF hp = trackToPixel(track, cp1F, cp1V);
    p.setPen(QPen(QColor(245, 245, 245), 2));
    p.drawLine(kp, hp);
    p.setPen(QPen(QColor(255, 230, 120), 2));
    p.setBrush(QColor(255, 230, 120));
    p.drawEllipse(hp, HANDLE_RADIUS + 1, HANDLE_RADIUS + 1);
   }

   if (i < n - 1 && isSelected) {
    float cp0F = static_cast<float>(key.frame + key.outHandleFrame);
    float cp0V = key.value + key.outHandleValue;
    QPointF hp = trackToPixel(track, cp0F, cp0V);
    p.setPen(QPen(QColor(245, 245, 245), 2));
    p.drawLine(kp, hp);
    p.setPen(QPen(QColor(255, 230, 120), 2));
    p.setBrush(QColor(255, 230, 120));
    p.drawEllipse(hp, HANDLE_RADIUS + 1, HANDLE_RADIUS + 1);
   }

   // Draw keyframe diamond
   QColor keyColor = isSelected ? QColor(255, 250, 170) : track.color;
   QColor fillColor = isSelected ? QColor(255, 236, 128, 235) : track.color.darker(150);
   p.setPen(QPen(keyColor, isSelected ? 3 : 1));
   p.setBrush(fillColor);

   // Diamond shape
   QPolygonF diamond;
   diamond << QPointF(kp.x(), kp.y() - KEY_RADIUS)
           << QPointF(kp.x() + KEY_RADIUS, kp.y())
           << QPointF(kp.x(), kp.y() + KEY_RADIUS)
           << QPointF(kp.x() - KEY_RADIUS, kp.y());
   p.drawPolygon(diamond);
   if (isSelected) {
    p.setPen(QPen(QColor(255, 255, 255, 180), 1));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(kp, KEY_RADIUS + 3, KEY_RADIUS + 3);
   }
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

 bool isKeySelected(int trackIndex, int keyIndex) const {
  return selectedKeys_.count({trackIndex, keyIndex}) > 0;
 }

 void clearKeySelection() {
  selectedTrack_ = -1;
  selectedKey_ = -1;
  selectedKeys_.clear();
 }

 void setPrimaryKeySelection(int trackIndex, int keyIndex) {
  selectedTrack_ = trackIndex;
  selectedKey_ = keyIndex;
  selectedKeys_.clear();
  selectedKeys_.insert({trackIndex, keyIndex});
 }

 void collectDraggedKeys() {
  draggedKeys_.clear();
  for (const auto& sel : selectedKeys_) {
   const int t = sel.first;
   const int k = sel.second;
   if (t < 0 || t >= static_cast<int>(tracks_.size())) continue;
   if (k < 0 || k >= static_cast<int>(tracks_[t].keys.size())) continue;
   draggedKeys_.push_back({t, k, tracks_[t].keys[k].frame,
                           tracks_[t].keys[k].value,
                           tracks_[t].keys[k].frame,
                           tracks_[t].keys[k].value});
  }
 }

 // CE-5: insert a key at the given data position. The target track is the
 // selected one when valid, otherwise the track whose curve passes nearest
 // to the insert position, otherwise the first visible track.
 bool insertKeyAt(const QPointF& dataPos) {
  int target = -1;
  if (selectedTrack_ >= 0 && selectedTrack_ < static_cast<int>(tracks_.size()) &&
      tracks_[selectedTrack_].visible) {
   target = selectedTrack_;
  } else {
   float bestDist = std::numeric_limits<float>::max();
   for (int ti = 0; ti < static_cast<int>(tracks_.size()); ++ti) {
    if (!tracks_[ti].visible || tracks_[ti].keys.empty()) continue;
    const auto& keys = tracks_[ti].keys;
    const float x = static_cast<float>(dataPos.x());
    float curveY = keys.front().value;
    if (x <= keys.front().frame) {
     curveY = keys.front().value;
    } else if (x >= keys.back().frame) {
     curveY = keys.back().value;
    } else {
     for (std::size_t i = 0; i + 1 < keys.size(); ++i) {
      if (x >= keys[i].frame && x <= keys[i + 1].frame) {
       const float span = static_cast<float>(std::max<int64_t>(1, keys[i + 1].frame - keys[i].frame));
       const float t = (x - static_cast<float>(keys[i].frame)) / span;
       curveY = keys[i].value + t * (keys[i + 1].value - keys[i].value);
       break;
      }
     }
    }
    const float dist = std::abs(curveY - static_cast<float>(dataPos.y()));
    if (dist < bestDist) {
     bestDist = dist;
     target = ti;
    }
   }
  }
  if (target < 0) {
   for (int ti = 0; ti < static_cast<int>(tracks_.size()); ++ti) {
    if (tracks_[ti].visible) { target = ti; break; }
   }
  }
  if (target < 0) return false;

  auto& keys = tracks_[target].keys;
  CurveKey key;
  key.frame = static_cast<int64_t>(std::llround(dataPos.x()));
  key.value = displayToValue(tracks_[target], static_cast<float>(dataPos.y()));
  key.smooth = true;

  const auto it = std::lower_bound(keys.begin(), keys.end(), key.frame,
    [](const CurveKey& k, int64_t frame) { return k.frame < frame; });
  const int idx = static_cast<int>(it - keys.begin());
  // Auto bezier handles from neighbors (25% rule, same as sampleSpeedGraph).
  if (idx > 0) {
   const auto& prev = keys[idx - 1];
   const float df = static_cast<float>(std::max<int64_t>(1, key.frame - prev.frame));
   key.inTangent = (key.value - prev.value) / df;
   key.inHandleFrame = -std::max<int64_t>(1, static_cast<int64_t>(std::llround(df * 0.25f)));
   key.inHandleValue = key.inTangent * static_cast<float>(-key.inHandleFrame);
  }
  if (idx < static_cast<int>(keys.size())) {
   const auto& next = keys[idx];
   const float df = static_cast<float>(std::max<int64_t>(1, next.frame - key.frame));
   key.outTangent = (next.value - key.value) / df;
   key.outHandleFrame = std::max<int64_t>(1, static_cast<int64_t>(std::llround(df * 0.25f)));
   key.outHandleValue = key.outTangent * static_cast<float>(key.outHandleFrame);
  }
  keys.insert(keys.begin() + idx, key);
  setPrimaryKeySelection(target, idx);
  return true;
 }

 // CE-3: delete every selected key. Returns true when anything was removed.
 bool deleteSelectedKeys() {
  if (selectedKeys_.empty()) {
   return false;
  }
  std::map<int, std::vector<int>> perTrack;
  for (const auto& sel : selectedKeys_) {
   perTrack[sel.first].push_back(sel.second);
  }
  bool removed = false;
  for (auto& entry : perTrack) {
   const int trackIndex = entry.first;
   auto& keyIndices = entry.second;
   if (trackIndex < 0 || trackIndex >= static_cast<int>(tracks_.size())) continue;
   auto& keys = tracks_[trackIndex].keys;
   std::sort(keyIndices.rbegin(), keyIndices.rend());
   for (const int keyIndex : keyIndices) {
    if (keyIndex >= 0 && keyIndex < static_cast<int>(keys.size())) {
     keys.erase(keys.begin() + keyIndex);
     removed = true;
    }
   }
  }
  clearKeySelection();
  return removed;
 }

 void selectKeysInDataRect(const QRectF& rect, bool additive) {
  if (!additive) {
   clearKeySelection();
  }
  const float x1 = static_cast<float>(std::min(rect.left(), rect.right()));
  const float x2 = static_cast<float>(std::max(rect.left(), rect.right()));
  const float y1 = static_cast<float>(std::min(rect.top(), rect.bottom()));
  const float y2 = static_cast<float>(std::max(rect.top(), rect.bottom()));
  for (int ti = 0; ti < static_cast<int>(tracks_.size()); ++ti) {
   if (!tracks_[ti].visible) continue;
   for (int ki = 0; ki < static_cast<int>(tracks_[ti].keys.size()); ++ki) {
    const auto& key = tracks_[ti].keys[ki];
    const float kf = static_cast<float>(key.frame);
    if (kf >= x1 && kf <= x2 && key.value >= y1 && key.value <= y2) {
     selectedKeys_.insert({ti, ki});
    }
   }
  }
  if (!selectedKeys_.empty()) {
   const auto& first = *selectedKeys_.begin();
   selectedTrack_ = first.first;
   selectedKey_ = first.second;
  }
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

 bool selectedKeyBounds(CurveTrack*& outTrack, CurveKey*& outKey,
                        const CurveKey*& outPrev, const CurveKey*& outNext) {
  outTrack = nullptr;
  outKey = nullptr;
  outPrev = nullptr;
  outNext = nullptr;
  if (selectedTrack_ < 0 || selectedTrack_ >= static_cast<int>(tracks_.size())) {
   return false;
  }
  auto& track = tracks_[selectedTrack_];
  if (selectedKey_ < 0 || selectedKey_ >= static_cast<int>(track.keys.size())) {
   return false;
  }
  outTrack = &track;
  outKey = &track.keys[selectedKey_];
  if (selectedKey_ > 0) {
   outPrev = &track.keys[selectedKey_ - 1];
  }
  if (selectedKey_ + 1 < static_cast<int>(track.keys.size())) {
   outNext = &track.keys[selectedKey_ + 1];
  }
  return true;
 }

 bool setSelectedTangentsFlat() {
  CurveTrack* track = nullptr;
  CurveKey* key = nullptr;
  const CurveKey* prev = nullptr;
  const CurveKey* next = nullptr;
  if (!selectedKeyBounds(track, key, prev, next)) {
   return false;
  }
  Q_UNUSED(track);
 if (prev) {
  const int64_t span = std::max<int64_t>(1, (key->frame - prev->frame) / 3);
  key->inHandleFrame = -span;
  key->inHandleValue = 0.0f;
 }
  if (next) {
  const int64_t span = std::max<int64_t>(1, (next->frame - key->frame) / 3);
  key->outHandleFrame = span;
  key->outHandleValue = 0.0f;
 }
 key->brokenTangents = false;
 key->smooth = true;
 return true;
}

 bool setSelectedTangentsAuto() {
  CurveTrack* track = nullptr;
  CurveKey* key = nullptr;
  const CurveKey* prev = nullptr;
  const CurveKey* next = nullptr;
  if (!selectedKeyBounds(track, key, prev, next)) {
   return false;
  }
  Q_UNUSED(track);
  float slope = 0.0f;
  if (prev && next) {
   const float df = static_cast<float>(next->frame - prev->frame);
   slope = df > 0.0f ? (next->value - prev->value) / df : 0.0f;
  } else if (prev) {
   const float df = static_cast<float>(key->frame - prev->frame);
   slope = df > 0.0f ? (key->value - prev->value) / df : 0.0f;
  } else if (next) {
   const float df = static_cast<float>(next->frame - key->frame);
   slope = df > 0.0f ? (next->value - key->value) / df : 0.0f;
  }
  if (prev) {
   const int64_t span = std::max<int64_t>(1, (key->frame - prev->frame) / 3);
   key->inHandleFrame = -span;
   key->inHandleValue = -slope * static_cast<float>(span);
  }
  if (next) {
   const int64_t span = std::max<int64_t>(1, (next->frame - key->frame) / 3);
   key->outHandleFrame = span;
   key->outHandleValue = slope * static_cast<float>(span);
  }
  key->brokenTangents = false;
  key->smooth = true;
  return true;
 }

 bool setSelectedTangentsLinear() {
  CurveTrack* track = nullptr;
  CurveKey* key = nullptr;
  const CurveKey* prev = nullptr;
  const CurveKey* next = nullptr;
  if (!selectedKeyBounds(track, key, prev, next)) {
   return false;
  }
  Q_UNUSED(track);
  if (prev) {
   const int64_t span = std::max<int64_t>(1, (key->frame - prev->frame) / 3);
   const float slope = static_cast<float>((key->value - prev->value) /
      static_cast<float>(std::max<int64_t>(1, key->frame - prev->frame)));
   key->inHandleFrame = -span;
   key->inHandleValue = -slope * static_cast<float>(span);
  }
  if (next) {
   const int64_t span = std::max<int64_t>(1, (next->frame - key->frame) / 3);
   const float slope = static_cast<float>((next->value - key->value) /
      static_cast<float>(std::max<int64_t>(1, next->frame - key->frame)));
   key->outHandleFrame = span;
   key->outHandleValue = slope * static_cast<float>(span);
  }
  key->brokenTangents = false;
  key->smooth = false;
  return true;
 }

 bool setSelectedTangentsBroken() {
  bool changed = false;
  const auto apply = [&](int trackIndex, int keyIndex) {
   if (trackIndex < 0 || trackIndex >= static_cast<int>(tracks_.size()) ||
       keyIndex < 0 || keyIndex >= static_cast<int>(tracks_[trackIndex].keys.size())) return;
   auto& key = tracks_[trackIndex].keys[keyIndex];
   changed |= !key.brokenTangents || key.smooth;
   key.brokenTangents = true;
   key.smooth = false;
  };
  if (selectedKeys_.empty()) apply(selectedTrack_, selectedKey_);
  else for (const auto& selection : selectedKeys_) apply(selection.first, selection.second);
  return changed;
 }

 bool setSelectedTangentsUnified() {
  bool changed = false;
  const auto apply = [&](int trackIndex, int keyIndex) {
   if (trackIndex < 0 || trackIndex >= static_cast<int>(tracks_.size()) ||
       keyIndex < 0 || keyIndex >= static_cast<int>(tracks_[trackIndex].keys.size())) return;
   auto& key = tracks_[trackIndex].keys[keyIndex];
   const int64_t inSpan = std::max<int64_t>(1, -key.inHandleFrame);
   const int64_t outSpan = std::max<int64_t>(1, key.outHandleFrame);
   const float inSlope = key.inHandleValue / static_cast<float>(inSpan);
   const float outSlope = key.outHandleValue / static_cast<float>(outSpan);
   const float slope = 0.5f * (inSlope + outSlope);
   key.inHandleFrame = -inSpan;
   key.outHandleFrame = outSpan;
   key.inHandleValue = -slope * static_cast<float>(inSpan);
   key.outHandleValue = slope * static_cast<float>(outSpan);
   key.inTangent = slope;
   key.outTangent = slope;
   changed |= key.brokenTangents || !key.smooth;
   key.brokenTangents = false;
   key.smooth = true;
  };
  if (selectedKeys_.empty()) apply(selectedTrack_, selectedKey_);
  else for (const auto& selection : selectedKeys_) apply(selection.first, selection.second);
  return changed;
 }

 bool setSelectedConstant(bool enabled) {
  bool changed = false;
  const auto apply = [&](int trackIndex, int keyIndex) {
   if (trackIndex < 0 || trackIndex >= static_cast<int>(tracks_.size()) ||
       keyIndex < 0 || keyIndex >= static_cast<int>(tracks_[trackIndex].keys.size())) return;
   auto& key = tracks_[trackIndex].keys[keyIndex];
   changed |= key.constant != enabled || (enabled ? key.smooth : !key.smooth);
   key.constant = enabled;
   key.smooth = !enabled;
  };
  if (selectedKeys_.empty()) apply(selectedTrack_, selectedKey_);
  else for (const auto& selection : selectedKeys_) apply(selection.first, selection.second);
  return changed;
 }

 template <typename Fn>
 bool applyTangentOperationToSelection(Fn&& operation) {
  if (selectedKeys_.empty()) {
   return operation();
  }
  const auto selections = selectedKeys_;
  bool changed = false;
  for (const auto& selection : selections) {
   selectedTrack_ = selection.first;
   selectedKey_ = selection.second;
   selectedKeys_.clear();
   changed |= operation();
  }
  selectedKeys_ = selections;
  return changed;
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

const std::vector<CurveTrack>& ArtifactCurveEditorWidget::tracks() const {
 return impl_->tracks_;
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

void ArtifactCurveEditorWidget::setSpeedGraph(
    const QVector<TimeRemapKeyframe>& keyframes,
    int64_t startFrame,
    int64_t endFrame,
    const FrameRate& frameRate) {
 std::vector<CurveTrack> tracks;
 tracks.push_back(
     ArtifactCore::sampleSpeedGraph(keyframes, startFrame, endFrame, frameRate));
 setTracks(tracks);
 fitToContent();
}

void ArtifactCurveEditorWidget::setHandleEditingEnabled(bool enabled) {
 impl_->handlesInteractive_ = enabled;
}

void ArtifactCurveEditorWidget::setKeyEditingEnabled(bool enabled) {
 impl_->keyEditingEnabled_ = enabled;
 if (!enabled && impl_->dragMode_ != Impl::DragMode::ScrubPlayhead &&
     impl_->dragMode_ != Impl::DragMode::Pan) {
  impl_->dragMode_ = Impl::DragMode::None;
 }
 update();
}

bool ArtifactCurveEditorWidget::setSelectedKeyAutoTangents() {
 if (!impl_) {
  return false;
 }
 Q_EMIT interactionStarted();
 if (!impl_->applyTangentOperationToSelection(
         [impl = impl_]() { return impl->setSelectedTangentsAuto(); })) {
  Q_EMIT interactionFinished();
  return false;
 }
 update();
 Q_EMIT interactionFinished();
 return true;
}

bool ArtifactCurveEditorWidget::setSelectedKeyFlatTangents() {
 if (!impl_) {
  return false;
 }
 Q_EMIT interactionStarted();
 if (!impl_->applyTangentOperationToSelection(
         [impl = impl_]() { return impl->setSelectedTangentsFlat(); })) {
  Q_EMIT interactionFinished();
  return false;
 }
 update();
 Q_EMIT interactionFinished();
 return true;
}

bool ArtifactCurveEditorWidget::setSelectedKeyLinearTangents() {
 if (!impl_) {
  return false;
 }
 Q_EMIT interactionStarted();
 if (!impl_->applyTangentOperationToSelection(
         [impl = impl_]() { return impl->setSelectedTangentsLinear(); })) {
  Q_EMIT interactionFinished();
  return false;
 }
 update();
 Q_EMIT interactionFinished();
 return true;
}

bool ArtifactCurveEditorWidget::setSelectedKeyBrokenTangents() {
 if (!impl_) {
  return false;
 }
 Q_EMIT interactionStarted();
 if (!impl_->setSelectedTangentsBroken()) {
  Q_EMIT interactionFinished();
  return false;
 }
 update();
 Q_EMIT interactionFinished();
 return true;
}

bool ArtifactCurveEditorWidget::setSelectedKeyUnifiedTangents() {
 if (!impl_) {
  return false;
 }
 Q_EMIT interactionStarted();
 if (!impl_->setSelectedTangentsUnified()) {
  Q_EMIT interactionFinished();
  return false;
 }
 update();
 Q_EMIT interactionFinished();
 return true;
}

bool ArtifactCurveEditorWidget::setSelectedKeyConstant() {
 if (!impl_) return false;
 Q_EMIT interactionStarted();
 if (!impl_->setSelectedConstant(true)) {
  Q_EMIT interactionFinished();
  return false;
 }
 update();
 Q_EMIT interactionFinished();
 return true;
}

bool ArtifactCurveEditorWidget::setSelectedKeyBezier() {
 if (!impl_) return false;
 Q_EMIT interactionStarted();
 if (!impl_->setSelectedConstant(false)) {
  Q_EMIT interactionFinished();
  return false;
 }
 update();
 Q_EMIT interactionFinished();
 return true;
}

bool ArtifactCurveEditorWidget::promptSetSelectedKeyValue() {
 if (!impl_ || !impl_->keyEditingEnabled_) {
  return false;
 }

 const int trackIndex = impl_->selectedTrack_;
 const int keyIndex = impl_->selectedKey_;
 if (trackIndex < 0 || keyIndex < 0 ||
     trackIndex >= static_cast<int>(impl_->tracks_.size()) ||
     keyIndex >= static_cast<int>(impl_->tracks_[trackIndex].keys.size())) {
  return false;
 }

 auto& key = impl_->tracks_[trackIndex].keys[keyIndex];
 bool accepted = false;
 const double nextValue = QInputDialog::getDouble(
     this, QStringLiteral("Set Keyframe Value"), QStringLiteral("Value:"),
     static_cast<double>(key.value), -1000000.0, 1000000.0, 3, &accepted);
 if (!accepted) {
  return false;
 }

 const float valueDelta = static_cast<float>(nextValue) - key.value;
 if (std::abs(static_cast<double>(valueDelta)) < 0.0001) {
  return false;
 }

 std::vector<std::pair<int, int>> keysToMove;
 for (const auto& selection : impl_->selectedKeys_) {
  if (selection.first >= 0 && selection.first < static_cast<int>(impl_->tracks_.size()) &&
      selection.second >= 0 &&
      selection.second < static_cast<int>(impl_->tracks_[selection.first].keys.size())) {
   keysToMove.push_back(selection);
  }
 }
 if (keysToMove.empty()) {
  keysToMove.push_back({trackIndex, keyIndex});
 }

 impl_->captureBufferCurve();
 Q_EMIT interactionStarted();
 for (const auto& selection : keysToMove) {
  auto& selectedKey = impl_->tracks_[selection.first].keys[selection.second];
  selectedKey.value += valueDelta;
  Q_EMIT keyMoved(selection.first, selection.second, selectedKey.frame, selectedKey.value);
 }
 Q_EMIT interactionFinished();
 update();
 return true;
}

bool ArtifactCurveEditorWidget::promptSetSelectedKeyFrame() {
 if (!impl_ || !impl_->keyEditingEnabled_) {
  return false;
 }
 const int trackIndex = impl_->selectedTrack_;
 const int keyIndex = impl_->selectedKey_;
 if (trackIndex < 0 || keyIndex < 0 ||
     trackIndex >= static_cast<int>(impl_->tracks_.size()) ||
     keyIndex >= static_cast<int>(impl_->tracks_[trackIndex].keys.size())) {
  return false;
 }
 auto& key = impl_->tracks_[trackIndex].keys[keyIndex];
 const int64_t primaryFrame = key.frame;
 bool accepted = false;
 const int64_t nextFrame = static_cast<int64_t>(QInputDialog::getInt(
     this, QStringLiteral("Set Keyframe Frame"), QStringLiteral("Frame:"),
     static_cast<int>(std::clamp<int64_t>(key.frame, -1000000000, 1000000000)),
     -1000000000, 1000000000, 1, &accepted));
 const int64_t frameDelta = nextFrame - primaryFrame;
 if (!accepted || frameDelta == 0) {
  return false;
 }
 std::vector<std::pair<int, int>> keysToMove;
 for (const auto& selection : impl_->selectedKeys_) {
  if (selection.first >= 0 && selection.first < static_cast<int>(impl_->tracks_.size()) &&
      selection.second >= 0 &&
      selection.second < static_cast<int>(impl_->tracks_[selection.first].keys.size())) {
   keysToMove.push_back(selection);
  }
 }
 if (keysToMove.empty()) {
  keysToMove.push_back({trackIndex, keyIndex});
 }

 impl_->captureBufferCurve();
 Q_EMIT interactionStarted();
 for (const auto& selection : keysToMove) {
  auto& selectedKey = impl_->tracks_[selection.first].keys[selection.second];
  selectedKey.frame += frameDelta;
  Q_EMIT keyMoved(selection.first, selection.second, selectedKey.frame, selectedKey.value);
 }
 Q_EMIT interactionFinished();
 update();
 return true;
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
  qDebug() << "[CurveEditor] fitToContent"
           << "no keys"
           << "fallbackRange=0..100,-10..110";
  setViewRange(0, 100, -10, 110);
  return;
 }

 float marginF = std::max((maxF - minF) * 0.1f, 5.0f);
 float marginV = std::max((maxV - minV) * 0.1f, 5.0f);
 qDebug() << "[CurveEditor] fitToContent"
          << "frameRange=" << minF << ".." << maxF
          << "valueRange=" << minV << ".." << maxV
          << "margin=" << marginF << marginV;
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
 if (impl_->bufferCurveVisible_) {
  for (int ti = 0; ti < static_cast<int>(impl_->bufferTracks_.size()); ++ti) {
   impl_->drawCurve(p, impl_->bufferTracks_[ti], ti, true);
  }
 }
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

 // CE-3: marquee (box) selection rectangle
 if (impl_->dragMode_ == Impl::DragMode::Marquee && !impl_->marqueeRectData_.isNull()) {
  const QPointF mp0 = impl_->dataToPixel(static_cast<float>(impl_->marqueeRectData_.left()),
                                        static_cast<float>(impl_->marqueeRectData_.top()));
  const QPointF mp1 = impl_->dataToPixel(static_cast<float>(impl_->marqueeRectData_.right()),
                                        static_cast<float>(impl_->marqueeRectData_.bottom()));
  p.setPen(QPen(QColor(120, 170, 255, 200), 1, Qt::DashLine));
  p.setBrush(QColor(120, 170, 255, 40));
  p.drawRect(QRectF(mp0, mp1).normalized());
 }

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

 // CE-5: Ctrl+Click inserts a key at the click position (Blender style).
 if ((event->modifiers() & Qt::ControlModifier) && impl_->keyEditingEnabled_) {
  const QPointF data = impl_->pixelToData(pos);
  Q_EMIT interactionStarted();
  impl_->insertKeyAt(data);
  Q_EMIT interactionFinished();
  update();
  return;
 }

 // Check for handle hit first
 if (!impl_->keyEditingEnabled_) {
  impl_->dragMode_ = Impl::DragMode::Pan;
  impl_->dragStart_ = pos.toPoint();
  impl_->dragStartXMin_ = impl_->xMin_;
  impl_->dragStartXMax_ = impl_->xMax_;
  impl_->dragStartYMin_ = impl_->yMin_;
  impl_->dragStartYMax_ = impl_->yMax_;
  startedInteraction = true;
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
  impl_->setPrimaryKeySelection(ht, hk);
  if (startedInteraction) {
   Q_EMIT interactionStarted();
  }
  update();
  return;
 }

 // Check for key hit
 int tk, kk;
 if (impl_->hitTestKey(pos, tk, kk) == 0) {
  if (event->modifiers() & Qt::ShiftModifier) {
   // CE-3: shift-click toggles membership in the multi-selection.
   const std::pair<int, int> sel{tk, kk};
   if (impl_->selectedKeys_.count(sel)) {
    impl_->selectedKeys_.erase(sel);
    if (impl_->selectedTrack_ == tk && impl_->selectedKey_ == kk) {
     if (!impl_->selectedKeys_.empty()) {
      const auto& first = *impl_->selectedKeys_.begin();
      impl_->selectedTrack_ = first.first;
      impl_->selectedKey_ = first.second;
     } else {
      impl_->selectedTrack_ = -1;
      impl_->selectedKey_ = -1;
     }
    }
   } else {
    impl_->selectedKeys_.insert(sel);
    impl_->selectedTrack_ = tk;
    impl_->selectedKey_ = kk;
   }
   update();
   return;
  }
  impl_->dragMode_ = Impl::DragMode::MoveKey;
  startedInteraction = true;
  impl_->captureBufferCurve();
  impl_->dragTrackIndex_ = tk;
  impl_->dragKeyIndex_ = kk;
  impl_->dragStart_ = pos.toPoint();
  impl_->dragOrigFrame_ = impl_->tracks_[tk].keys[kk].frame;
  impl_->dragOrigValue_ = impl_->tracks_[tk].keys[kk].value;
  if (!impl_->isKeySelected(tk, kk)) {
   impl_->setPrimaryKeySelection(tk, kk);
  }
  impl_->collectDraggedKeys();
  Q_EMIT keySelected(tk, kk);
  if (startedInteraction) {
   Q_EMIT interactionStarted();
  }
  update();
  return;
 }

 // CE-3: shift+drag on empty space starts a marquee (box) selection.
 if (event->modifiers() & Qt::ShiftModifier) {
  impl_->dragMode_ = Impl::DragMode::Marquee;
  impl_->marqueeAdditive_ = false;
  const QPointF data = impl_->pixelToData(pos);
  impl_->marqueeRectData_ = QRectF(data, data);
  impl_->dragStart_ = pos.toPoint();
  startedInteraction = true;
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
 impl_->clearKeySelection();
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

   // CE-12: frame movement is already quantized to integer frames below.
   // Ctrl enables a predictable value-grid snap without adding another
   // shortcut or changing the normal free-form curve editing path.
   if (event->modifiers().testFlag(Qt::ControlModifier)) {
    valueDelta = std::round(valueDelta);
   }

   // CE-4: move every selected key by the same delta.
   if (impl_->draggedKeys_.empty()) {
    impl_->collectDraggedKeys();
   }
   for (auto& dragged : impl_->draggedKeys_) {
    if (dragged.track < 0 || dragged.track >= static_cast<int>(impl_->tracks_.size())) continue;
    auto& keys = impl_->tracks_[dragged.track].keys;
    if (dragged.key < 0 || dragged.key >= static_cast<int>(keys.size())) continue;
    int64_t newFrame = dragged.frame + static_cast<int64_t>(std::llround(frameDelta));
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
     const auto& candidateKeys = impl_->tracks_[dragged.track].keys;
     for (int candidateIndex = 0;
          candidateIndex < static_cast<int>(candidateKeys.size()); ++candidateIndex) {
      if (candidateIndex == dragged.key ||
          impl_->selectedKeys_.count({dragged.track, candidateIndex}) > 0) {
       continue;
      }
      if (std::abs(candidateKeys[candidateIndex].frame - newFrame) <= 2) {
       newFrame = candidateKeys[candidateIndex].frame;
       break;
      }
     }
    }
    const float newValue = dragged.value +
        impl_->displayDeltaToValue(impl_->tracks_[dragged.track], valueDelta);
    keys[dragged.key].frame = newFrame;
    keys[dragged.key].value = newValue;
    dragged.finalFrame = newFrame;
    dragged.finalValue = newValue;
    Q_EMIT keyMoved(dragged.track, dragged.key, newFrame, newValue);
   }
   update();
   break;
  }

  case Impl::DragMode::Marquee: {
   const QPointF data = impl_->pixelToData(pos);
   impl_->marqueeRectData_.setBottomRight(data);
   update();
   break;
  }

  case Impl::DragMode::MoveHandleIn: {
   QPointF data = impl_->pixelToData(pos);
   auto& key = impl_->tracks_[impl_->dragTrackIndex_].keys[impl_->dragKeyIndex_];
   const float rawValue = impl_->displayToValue(impl_->tracks_[impl_->dragTrackIndex_],
                                                static_cast<float>(data.y()));
   const bool wasBroken = key.brokenTangents;
   key.inHandleFrame = static_cast<int64_t>(data.x()) - key.frame;
   key.inHandleValue = rawValue - key.value;
   if (!wasBroken) {
    key.outHandleFrame = -key.inHandleFrame;
    key.outHandleValue = -key.inHandleValue;
   }
   key.brokenTangents = true;
   key.smooth = true;
   update();
   break;
  }

  case Impl::DragMode::MoveHandleOut: {
   QPointF data = impl_->pixelToData(pos);
   auto& key = impl_->tracks_[impl_->dragTrackIndex_].keys[impl_->dragKeyIndex_];
   const float rawValue = impl_->displayToValue(impl_->tracks_[impl_->dragTrackIndex_],
                                                static_cast<float>(data.y()));
   const bool wasBroken = key.brokenTangents;
   key.outHandleFrame = static_cast<int64_t>(data.x()) - key.frame;
   key.outHandleValue = rawValue - key.value;
   if (!wasBroken) {
    key.inHandleFrame = -key.outHandleFrame;
    key.inHandleValue = -key.outHandleValue;
   }
   key.brokenTangents = true;
   key.smooth = true;
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
 const Impl::DragMode finishedMode = impl_->dragMode_;
 const bool hadDrag = finishedMode != Impl::DragMode::None;
 if (finishedMode == Impl::DragMode::MoveKey) {
  // Re-sort every affected track, then rebuild the selection against
  // the new key order using the final dragged positions.
  std::set<int> affectedTracks;
  for (const auto& dragged : impl_->draggedKeys_) {
   affectedTracks.insert(dragged.track);
  }
  for (const int trackIndex : affectedTracks) {
   if (trackIndex < 0 || trackIndex >= static_cast<int>(impl_->tracks_.size())) continue;
   auto& keys = impl_->tracks_[trackIndex].keys;
   std::sort(keys.begin(), keys.end(),
    [](const CurveKey& lhs, const CurveKey& rhs) { return lhs.frame < rhs.frame; });
  }
  std::set<std::pair<int, int>> newSelection;
  for (const auto& dragged : impl_->draggedKeys_) {
   if (dragged.track < 0 || dragged.track >= static_cast<int>(impl_->tracks_.size())) continue;
   const auto& keys = impl_->tracks_[dragged.track].keys;
   for (int i = 0; i < static_cast<int>(keys.size()); ++i) {
    if (keys[i].frame == dragged.finalFrame &&
        std::abs(keys[i].value - dragged.finalValue) < 0.0001f) {
     newSelection.insert({dragged.track, i});
     break;
    }
   }
  }
  impl_->selectedKeys_ = newSelection;
  if (!newSelection.empty()) {
   const auto& first = *newSelection.begin();
   impl_->selectedTrack_ = first.first;
   impl_->selectedKey_ = first.second;
  } else {
   impl_->selectedTrack_ = -1;
   impl_->selectedKey_ = -1;
  }
 }

 if (finishedMode == Impl::DragMode::Marquee) {
  impl_->selectKeysInDataRect(impl_->marqueeRectData_, impl_->marqueeAdditive_);
  impl_->marqueeRectData_ = QRectF();
 }

 impl_->dragMode_ = Impl::DragMode::None;
 impl_->draggedKeys_.clear();
 if (hadDrag) {
  Q_EMIT interactionFinished();
 }
 update();
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

 if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace ||
     event->key() == Qt::Key_X) {
  if (!impl_->keyEditingEnabled_) {
   event->accept();
   return;
  }
  // CE-3: delete every selected key. The removal is written back to
  // the properties at interactionFinished and covered by one undo step.
  if (!impl_->selectedKeys_.empty()) {
   Q_EMIT interactionStarted();
   impl_->deleteSelectedKeys();
   Q_EMIT interactionFinished();
   update();
   event->accept();
   return;
  }
 }

 if (event->key() == Qt::Key_C && impl_->keyEditingEnabled_) {
  if (impl_->copySelectedKeys()) {
   event->accept();
   return;
  }
 }

 if (event->key() == Qt::Key_V && impl_->keyEditingEnabled_) {
  if (!impl_->copiedKeys_.empty() &&
      impl_->selectedTrack_ >= 0 &&
      impl_->selectedTrack_ < static_cast<int>(impl_->tracks_.size())) {
   Q_EMIT interactionStarted();
   if (impl_->pasteCopiedKeys()) {
   Q_EMIT interactionFinished();
   update();
   event->accept();
   return;
   }
  }
 }

 if (event->key() == Qt::Key_D &&
     event->modifiers().testFlag(Qt::ShiftModifier) &&
     impl_->keyEditingEnabled_) {
  if (!impl_->selectedKeys_.empty()) {
   Q_EMIT interactionStarted();
   const bool cloned = impl_->cloneSelectedKeys();
   Q_EMIT interactionFinished();
   if (cloned) {
    update();
    event->accept();
    return;
   }
  }
 }

 if (event->key() == Qt::Key_B) {
  if (impl_->bufferTracks_.empty()) {
   impl_->bufferTracks_ = impl_->tracks_;
  }
  impl_->bufferCurveVisible_ = !impl_->bufferCurveVisible_;
  update();
  event->accept();
  return;
 }

 if (event->key() == Qt::Key_N) {
  impl_->normalizedView_ = !impl_->normalizedView_;
  if (impl_->normalizedView_) {
   impl_->yMin_ = -1.2f;
   impl_->yMax_ = 1.2f;
  } else {
   fitToContent();
  }
  update();
  event->accept();
  return;
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

 if (event->key() == Qt::Key_A) {
  // CE-3: select all keys across visible tracks (toggle to deselect).
  int totalKeys = 0;
  for (const auto& track : impl_->tracks_) {
   if (track.visible) totalKeys += static_cast<int>(track.keys.size());
  }
  if (totalKeys > 0 && static_cast<int>(impl_->selectedKeys_.size()) == totalKeys) {
   impl_->clearKeySelection();
  } else {
   impl_->clearKeySelection();
   for (int ti = 0; ti < static_cast<int>(impl_->tracks_.size()); ++ti) {
    if (!impl_->tracks_[ti].visible) continue;
    for (int ki = 0; ki < static_cast<int>(impl_->tracks_[ti].keys.size()); ++ki) {
     impl_->selectedKeys_.insert({ti, ki});
    }
   }
   if (!impl_->selectedKeys_.empty()) {
    const auto& first = *impl_->selectedKeys_.begin();
    impl_->selectedTrack_ = first.first;
    impl_->selectedKey_ = first.second;
   }
  }
  update();
  event->accept();
  return;
 }

 if (event->key() == Qt::Key_Escape) {
  impl_->clearKeySelection();
  focusTrack(-1);
  event->accept();
  return;
 }

 QWidget::keyPressEvent(event);
}

 // ============================================================
 // Speed Graph Utilities
 // ============================================================

} // namespace ArtifactCore
