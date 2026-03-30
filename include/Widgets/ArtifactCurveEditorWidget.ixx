module;
#include <QWidget>
#include <wobjectdefs.h>
#include <vector>

export module Widget.CurveEditor;

import std;

export namespace ArtifactCore {

 struct CurveKey {
  int64_t frame = 0;
  float value = 0.0f;
  float inTangent = 0.0f;   // incoming slope
  float outTangent = 0.0f;  // outgoing slope
  bool smooth = false;      // continuous tangent
  int64_t inHandleFrame = 0;  // bezier in-handle frame offset from key
  int64_t outHandleFrame = 0; // bezier out-handle frame offset from key
  float inHandleValue = 0.0f; // bezier in-handle value offset from key
  float outHandleValue = 0.0f; // bezier out-handle value offset from key
 };

 struct CurveTrack {
  QString name;
  QColor color = QColor(255, 255, 255);
  std::vector<CurveKey> keys;
  bool visible = true;
 };

 class ArtifactCurveEditorWidget : public QWidget {
  W_OBJECT(ArtifactCurveEditorWidget)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactCurveEditorWidget(QWidget* parent = nullptr);
  ~ArtifactCurveEditorWidget();

  void setTracks(const std::vector<CurveTrack>& tracks);
  void setViewRange(float xMin, float xMax, float yMin, float yMax);
  void setCurrentFrame(int64_t frame);
  void fitToContent();

 signals:
  void keyMoved(int trackIndex, int keyIndex, int64_t newFrame, float newValue)
   W_SIGNAL(keyMoved, trackIndex, keyIndex, newFrame, newValue);
  void keySelected(int trackIndex, int keyIndex)
   W_SIGNAL(keySelected, trackIndex, keyIndex);
  void currentFrameChanged(int64_t frame)
   W_SIGNAL(currentFrameChanged, frame);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
 };

};
