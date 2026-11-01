module;
#include <cstdint>
#include <utility>
#include <vector>

#include <QWidget>
#include <QKeyEvent>
#include <QPointF>
#include <QRectF>
#include <QVector>
#include <wobjectdefs.h>
export module Widget.CurveEditor;

import Frame.Rate;
import Time.TimeRemap;

export namespace ArtifactCore {

 struct CurveKey {
  int64_t frame = 0;
  float value = 0.0f;
  float inTangent = 0.0f;   // incoming slope
  float outTangent = 0.0f;  // outgoing slope
  bool smooth = false;      // continuous tangent
  bool constant = false;    // step/constant interpolation to the next key
  int64_t inHandleFrame = 0;   // bezier in-handle frame offset from key
  int64_t outHandleFrame = 0;  // bezier out-handle frame offset from key
  float inHandleValue = 0.0f;  // bezier in-handle value offset from key
  float outHandleValue = 0.0f; // bezier out-handle value offset from key
  bool brokenTangents = false; // independent in/out tangents (Maya Break/Unify)
 };

 struct CurveTrack {
  QString name;
  QColor color = QColor(255, 255, 255);
  std::vector<CurveKey> keys;
  bool visible = true;
 };

 CurveTrack sampleSpeedGraph(const QVector<TimeRemapKeyframe>& keyframes,
                             int64_t startFrame,
                             int64_t endFrame,
                             const FrameRate& frameRate);

 // CE-14: the derived Speed graph is the average absolute slope (in value
 // units per second) of the neighbouring non-constant segments. Both the
 // display payload and the write-back solver use these two functions so the
 // number the user drags is exactly the number the solver targets.
 float curveSpeedAt(const std::vector<CurveKey>& keys, int index,
                    double framesPerSecond);

 // Solve for the value of keys[index] that produces targetSpeed while the
 // neighbouring keys, their frames and the constant flags stay fixed.
 // Returns false when the target is unreachable (negative, below the minimum
 // the neighbours allow, or degenerate equal spans with a differing target).
 bool solveCurveKeyValueForSpeed(const std::vector<CurveKey>& keys, int index,
                                 float targetSpeed, double framesPerSecond,
                                 float& outValue);

 class ArtifactCurveEditorWidget : public QWidget {
  W_OBJECT(ArtifactCurveEditorWidget)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactCurveEditorWidget(QWidget* parent = nullptr);
  ~ArtifactCurveEditorWidget();

  void setTracks(const std::vector<CurveTrack>& tracks);
  const std::vector<CurveTrack>& tracks() const;
  void setViewRange(float xMin, float xMax, float yMin, float yMax);
  void setCurrentFrame(int64_t frame);
  int64_t currentFrame() const;
  QRectF visualPlotRect() const;
  QRectF visualViewRange() const;
  QPointF visualPoint(int trackIndex, float frame, float value) const;
  bool visualKeySelected(int trackIndex, int keyIndex) const;
  int visualSelectedTrack() const;
  int visualSelectedKey() const;
  int visualSelectedKeyCount() const;
  bool visualNormalizedView() const;
  bool visualBufferCurveVisible() const;
  bool visualKeyEditingEnabled() const;
  bool visualHandleEditingEnabled() const;
  void setSpeedGraph(const QVector<TimeRemapKeyframe>& keyframes,
                     int64_t startFrame,
                     int64_t endFrame,
                     const FrameRate& frameRate);
  void fitToContent();
  // Fit the view to the selected keys (all visible keys when nothing is
  // selected). In the normalized view the y range stays -1.2..1.2.
  void frameSelection();
  // Visibility and view fit only. The timeline calls this on every refresh, so
  // it must not move the primary key or the selection set.
  void focusTrack(int trackIndex);
  void setHandleEditingEnabled(bool enabled);
  void setKeyEditingEnabled(bool enabled);
  // CE-14: a derived graph (the Speed graph) is computed from other data, so
  // only key moves are allowed. Structural edits (insert/delete/copy/paste/
  // clone) and tangent edits are rejected instead of silently corrupting the
  // source property with derived values.
  void setDerivedGraphMode(bool derived);
  bool derivedGraphMode() const;
  void setNormalizedView(bool normalized);
  void setBufferCurveVisible(bool visible);
  bool setSelectedKeyAutoTangents();
  bool setSelectedKeyFlatTangents();
  bool setSelectedKeyLinearTangents();
  bool setSelectedKeyBrokenTangents();
  bool setSelectedKeyUnifiedTangents();
  bool setSelectedKeyConstant();
  bool setSelectedKeyBezier();
  bool promptSetSelectedKeyFrame();
  bool promptSetSelectedKeyValue();

 public:
  void interactionStarted();
  void interactionFinished();

  void keyMoved(int trackIndex, int keyIndex, int64_t newFrame, float newValue);
  void keyDeleted(int trackIndex, int keyIndex);
  void keySelected(int trackIndex, int keyIndex);
  void currentFrameChanged(int64_t frame);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
 };

};
