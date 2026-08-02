module;
#include <QPointF>
#include <QSizeF>
#include <QString>
#include <vector>

export module Artifact.Widgets.Render.ViewportScaleOverlay;

export namespace Artifact {

enum class ViewportRulerTickLevel {
  Major,
  Minor,
  SubMinor
};

struct ViewportTickStep {
  float value = 1.0f;
  float interval = 1.0f;
  float subInterval = 0.0f;
  QString label;
  int subdivisionsPerMajor = 0;
};

struct ViewportRulerTick {
  ViewportRulerTickLevel level = ViewportRulerTickLevel::Major;
  float canvasPos = 0.0f;
  float viewportPos = 0.0f;
  QString label;
};

class ViewportTickCalculator {
public:
  static ViewportTickStep compute(float zoom, float targetPixels,
                                  const QString& unitName = QStringLiteral("px"));
  static float snapToNiceValue(float rawValue);
};

class ViewportRulerData {
public:
  static std::vector<ViewportRulerTick> generateTicks(
      float zoom, const QPointF& viewportOrigin, const QSizeF& viewportSize,
      const QSizeF& canvasSize, bool horizontal = true,
      float targetPixels = 80.0f,
      const QString& unitName = QStringLiteral("px"));
};

struct ViewportScaleBarData {
  float barCanvasWidth = 0.0f;
  float barViewportX = 16.0f;
  float barViewportY = 16.0f;
  float barWidthPx = 0.0f;
  QString label;
};

enum class ViewportRulerOrientation {
  Horizontal,
  Vertical
};

enum class ViewportRulerAnchor {
  Start,
  Center,
  End
};

struct ViewportRulerConfig {
  ViewportRulerOrientation orientation = ViewportRulerOrientation::Horizontal;
  ViewportRulerAnchor anchor = ViewportRulerAnchor::Start;
  float targetPixels = 80.0f;
  QString unitName = QStringLiteral("px");
  bool showLabels = true;
  bool showTicks = true;
};

struct ViewportScaleBarConfig {
  enum class Anchor {
    BottomLeft,
    BottomRight,
    TopLeft,
    TopRight
  };
  Anchor anchor = Anchor::BottomLeft;
  float marginX = 16.0f;
  float marginY = 16.0f;
  float targetPixels = 100.0f;
  QString unitName = QStringLiteral("px");
  bool visible = true;
};

struct ViewportGridLabelConfig {
  float targetPixels = 80.0f;
  QString unitName = QStringLiteral("px");
  QPointF viewportPosition = QPointF(16.0, 16.0);
  bool visible = true;
};

struct ViewportGridLabelData {
  float interval = 0.0f;
  QPointF viewportPosition;
  QString label;
};

struct ViewportCompassConfig {
  QPointF viewportPosition = QPointF(48.0, 48.0);
  float size = 32.0f;
  bool visible = true;
};

struct ViewportCompassData {
  QPointF center;
  QPointF xAxisEnd;
  QPointF yAxisEnd;
  QString xLabel = QStringLiteral("X");
  QString yLabel = QStringLiteral("Y");
};

struct ViewportOverlayFrameData {
  struct Ruler {
    int id = 0;
    std::vector<ViewportRulerTick> ticks;
  };
  struct ScaleBar {
    int id = 0;
    ViewportScaleBarData data;
  };
  struct GridLabel {
    int id = 0;
    ViewportGridLabelData data;
  };
  struct Compass {
    int id = 0;
    ViewportCompassData data;
  };
  std::vector<Ruler> rulers;
  std::vector<ScaleBar> scaleBars;
  std::vector<GridLabel> gridLabels;
  std::vector<Compass> compasses;
};

class ViewportOverlayManager {
public:
  int addRuler(const ViewportRulerConfig& config = {});
  int addScaleBar(const ViewportScaleBarConfig& config = {});
  int addGridLabel(const ViewportGridLabelConfig& config = {});
  int addCompass(const ViewportCompassConfig& config = {});
  bool configureRuler(int id, const ViewportRulerConfig& config);
  bool configureScaleBar(int id, const ViewportScaleBarConfig& config);
  bool configureGridLabel(int id, const ViewportGridLabelConfig& config);
  bool configureCompass(int id, const ViewportCompassConfig& config);
  void remove(int id);
  void clear();
  void invalidateCache();
  void setVisible(int id, bool visible);
  bool isVisible(int id) const;

  std::vector<ViewportRulerTick> generateRulerTicks(
      int id, float zoom, const QPointF& viewportOrigin,
      const QSizeF& viewportSize, const QSizeF& canvasSize) const;
  ViewportScaleBarData generateScaleBarData(
      int id, float zoom, const QSizeF& viewportSize) const;
  ViewportGridLabelData generateGridLabelData(int id, float zoom) const;
  ViewportCompassData generateCompassData(int id, float yawDegrees) const;
  ViewportOverlayFrameData generateAll(
      float zoom, const QPointF& viewportOrigin,
      const QSizeF& viewportSize, const QSizeF& canvasSize,
      float yawDegrees = 0.0f) const;

private:
  struct RulerEntry {
    int id = 0;
    ViewportRulerConfig config;
    bool visible = true;
    mutable bool cacheValid = false;
    mutable float cachedZoom = 0.0f;
    mutable QPointF cachedOrigin;
    mutable QSizeF cachedViewportSize;
    mutable QSizeF cachedCanvasSize;
    mutable std::vector<ViewportRulerTick> cachedTicks;
  };
  struct ScaleBarEntry {
    int id = 0;
    ViewportScaleBarConfig config;
    bool visible = true;
  };
  struct GridLabelEntry {
    int id = 0;
    ViewportGridLabelConfig config;
    bool visible = true;
  };
  struct CompassEntry {
    int id = 0;
    ViewportCompassConfig config;
    bool visible = true;
  };
  int nextId_ = 1;
  std::vector<RulerEntry> rulers_;
  std::vector<ScaleBarEntry> scaleBars_;
  std::vector<GridLabelEntry> gridLabels_;
  std::vector<CompassEntry> compasses_;
};

class ViewportScaleBarDataFactory {
public:
  static ViewportScaleBarData generate(float zoom, const QSizeF& viewportSize,
                                       float targetPixels = 100.0f,
                                       const QString& unitName = QStringLiteral("px"));
};

}
