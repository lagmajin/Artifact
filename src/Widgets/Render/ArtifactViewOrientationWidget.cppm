module;

#include <QColor>
#include <QEnterEvent>
#include <QFont>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>
#include <QPen>
#include <QPolygonF>
#include <QQuaternion>
#include <QRegion>
#include <QTransform>
#include <QVector3D>
#include <QWidget>

#include <algorithm>
#include <array>
#include <functional>
#include <utility>
#include <vector>

export module Artifact.Widgets.ViewOrientationWidget;

import UI.View.Orientation.Navigator;

export namespace Artifact {

class ViewOrientationWidget final : public QWidget {
public:
  explicit ViewOrientationWidget(QWidget *parent = nullptr) : QWidget(parent) {
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
  }

  void setOrientation(ArtifactCore::ViewOrientationHotspot hotspot) {
    if (hotspot_ == hotspot) {
      return;
    }
    hotspot_ = hotspot;
    navigator_.snapTo(hotspot_, true);
    orientation_ = navigator_.currentOrientation();
    update();
  }

  ArtifactCore::ViewOrientationHotspot orientation() const { return hotspot_; }

  void setOrientationQuaternion(const QQuaternion &orientation) {
    orientation_ = orientation.normalized();
    navigator_.setCurrentOrientation(orientation_);
    hotspot_ = navigator_.activeHotspot();
    update();
  }

  void setEnabledState(bool enabled) {
    setEnabled(enabled);
    update();
  }

  void setActivatedCallback(
      std::function<void(ArtifactCore::ViewOrientationHotspot)> callback) {
    activatedCallback_ = std::move(callback);
  }

  void setOrbitChangedCallback(
      std::function<void(const QQuaternion &)> callback) {
    orbitChangedCallback_ = std::move(callback);
  }

  QSize sizeHint() const override { return {124, 132}; }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF panelRect = rect().adjusted(1, 1, -1, -1);
    p.setPen(QPen(QColor(255, 255, 255, isEnabled() ? 42 : 24), 1.0));
    p.setBrush(QColor(14, 18, 26, 156));
    p.drawRoundedRect(panelRect, 9.0, 9.0);
    const auto faces = projectedFaces();
    for (const auto &face : faces) {
      if (!face.visible) {
        continue;
      }
      const bool selected = face.hotspot == hotspot_;
      const bool hovered = face.hotspot == hoverHotspot_;
      QColor fill = face.baseFill;
      QColor border = face.baseBorder;
      QColor text(233, 242, 248, 220);
      if (!isEnabled()) {
        fill.setAlpha(64);
        border.setAlpha(70);
        text.setAlpha(90);
      } else if (selected) {
        fill = fill.lighter(118);
        fill.setAlpha(224);
        border = QColor(198, 226, 248, 225);
      } else if (hovered) {
        fill = fill.lighter(110);
        fill.setAlpha(214);
        border = QColor(184, 214, 238, 212);
      }

      p.setPen(QPen(border, selected ? 2.2 : 1.25));
      p.setBrush(fill);
      p.drawPolygon(face.polygon);

      QFont faceFont = p.font();
      faceFont.setBold(true);
      faceFont.setPointSizeF(12.0);
      QPolygonF insetFace;
      insetFace.reserve(4);
      for (const auto &point : face.polygon) {
        insetFace << face.labelRect.center() +
                         (point - face.labelRect.center()) * 0.72;
      }
      const QPolygonF sourceQuad{
          QPointF(0.0, 0.0), QPointF(100.0, 0.0),
          QPointF(100.0, 100.0), QPointF(0.0, 100.0)};
      QTransform faceTransform;
      if (QTransform::quadToQuad(sourceQuad, insetFace, faceTransform)) {
        p.save();
        p.setClipRegion(QRegion(face.polygon.toPolygon()));
        p.setWorldTransform(faceTransform, true);
        p.setFont(faceFont);
        p.setPen(text);
        p.drawText(QRectF(4.0, 20.0, 92.0, 60.0), Qt::AlignCenter,
                   hotspotLabel(face.hotspot, false));
        p.restore();
      }
    }

    const auto snapTargets = projectedSnapTargets();
    for (const auto &target : snapTargets) {
      if (!target.visible) {
        continue;
      }
      const bool selected = target.hotspot == hotspot_;
      const bool hovered = target.hotspot == hoverHotspot_;
      QColor fill(205, 228, 246, selected ? 228 : hovered ? 206 : 144);
      QColor border(30, 38, 52, selected ? 224 : hovered ? 188 : 132);
      if (!isEnabled()) {
        fill.setAlpha(72);
        border.setAlpha(72);
      }
      QPen edgePen(fill, selected ? target.thickness + 2.0
                                 : hovered ? target.thickness + 1.0
                                           : target.thickness);
      edgePen.setCapStyle(Qt::RoundCap);
      p.setPen(edgePen);
      p.setBrush(Qt::NoBrush);
      p.drawLine(target.start, target.end);
      QPen edgeInsetPen(border, selected ? 1.8 : 1.0);
      edgeInsetPen.setCapStyle(Qt::RoundCap);
      p.setPen(edgeInsetPen);
      p.drawLine(target.start, target.end);
      if (selected || hovered) {
        QFont snapFont = p.font();
        snapFont.setBold(true);
        snapFont.setPointSizeF(std::max(7.0, snapFont.pointSizeF() - 1.5));
        p.setFont(snapFont);
        p.setPen(QColor(236, 244, 250, 235));
        const QRectF textRect(target.center.x() - 18.0, target.center.y() - 22.0,
                              36.0, 12.0);
        p.drawText(textRect, Qt::AlignCenter,
                   hotspotLabel(target.hotspot, true));
      }
    }

    const auto cornerTargets = projectedCornerTargets();
    for (const auto &target : cornerTargets) {
      if (!target.visible) {
        continue;
      }
      const bool selected = target.hotspot == hotspot_;
      const bool hovered = target.hotspot == hoverHotspot_;
      QColor fill(214, 228, 240, selected ? 214 : hovered ? 188 : 92);
      QColor border(186, 210, 230, selected ? 228 : hovered ? 204 : 120);
      if (!isEnabled()) {
        fill.setAlpha(56);
        border.setAlpha(70);
      }
      if (!selected && !hovered) {
        fill.setAlpha(40);
      }
      p.setPen(QPen(border, selected ? 1.5 : hovered ? 1.2 : 0.8));
      p.setBrush(fill);
      p.drawEllipse(target.center, target.radius, target.radius);
    }

    const QPointF axisOrigin(20.0, height() - 18.0);
    const auto drawAxis = [&p, &axisOrigin](const QPointF &delta,
                                            const QColor &color,
                                            const QString &label) {
      p.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap));
      p.drawLine(axisOrigin, axisOrigin + delta);
      p.setPen(color.lighter(125));
      p.drawText(QRectF(axisOrigin + delta - QPointF(6.0, 7.0),
                        QSizeF(12.0, 14.0)),
                 Qt::AlignCenter, label);
    };
    drawAxis(QPointF(15.0, 0.0), QColor(232, 92, 92), QStringLiteral("X"));
    drawAxis(QPointF(0.0, -15.0), QColor(96, 205, 132), QStringLiteral("Y"));
    drawAxis(QPointF(-8.0, 8.0), QColor(92, 154, 232), QStringLiteral("Z"));
  }

  void mouseMoveEvent(QMouseEvent *event) override {
    if (!isEnabled()) {
      QWidget::mouseMoveEvent(event);
      return;
    }
    if (pressArmed_ && !dragActive_ &&
        (event->position() - dragStartPos_).manhattanLength() >=
            QApplication::startDragDistance()) {
      dragActive_ = true;
    }
    if (dragActive_) {
      const QPointF delta = event->position() - dragStartPos_;
      const float yawDelta = static_cast<float>(delta.x()) * 0.55f;
      const float pitchDelta = static_cast<float>(delta.y()) * 0.55f;
      const QQuaternion yaw =
          QQuaternion::fromAxisAndAngle(0.0f, 1.0f, 0.0f, yawDelta);
      const QVector3D localRight =
          dragStartOrientation_.rotatedVector(QVector3D(1.0f, 0.0f, 0.0f));
      const QQuaternion pitch =
          QQuaternion::fromAxisAndAngle(localRight, pitchDelta);
      orientation_ = (pitch * yaw * dragStartOrientation_).normalized();
      navigator_.setCurrentOrientation(orientation_);
      hotspot_ = navigator_.activeHotspot();
      hoverHotspot_ = hotspotAt(event->position());
      if (orbitChangedCallback_) {
        orbitChangedCallback_(orientation_);
      }
      update();
      event->accept();
      return;
    }
    hoverHotspot_ = hotspotAt(event->position());
    update();
    QWidget::mouseMoveEvent(event);
  }

  void leaveEvent(QEvent *event) override {
    hoverHotspot_ = ArtifactCore::ViewOrientationHotspot::None;
    update();
    QWidget::leaveEvent(event);
  }

  void mousePressEvent(QMouseEvent *event) override {
    if (!isEnabled() || event->button() != Qt::LeftButton) {
      QWidget::mousePressEvent(event);
      return;
    }
    pressedHotspot_ = hotspotAt(event->position());
    if (pressedHotspot_ == ArtifactCore::ViewOrientationHotspot::None) {
      QWidget::mousePressEvent(event);
      return;
    }
    pressArmed_ = true;
    dragActive_ = false;
    dragStartPos_ = event->position();
    dragStartOrientation_ = orientation_;
    event->accept();
  }

  void mouseReleaseEvent(QMouseEvent *event) override {
    if (!isEnabled() || event->button() != Qt::LeftButton) {
      QWidget::mouseReleaseEvent(event);
      return;
    }
    const auto releaseHotspot = hotspotAt(event->position());
    if (dragActive_) {
      dragActive_ = false;
      pressArmed_ = false;
      event->accept();
      return;
    }
    if (pressArmed_ && pressedHotspot_ != ArtifactCore::ViewOrientationHotspot::None &&
        pressedHotspot_ == releaseHotspot) {
      hotspot_ = pressedHotspot_;
      navigator_.snapTo(hotspot_, true);
      orientation_ = navigator_.currentOrientation();
      if (activatedCallback_) {
        activatedCallback_(hotspot_);
      }
      update();
      event->accept();
      pressArmed_ = false;
      return;
    }
    pressArmed_ = false;
    QWidget::mouseReleaseEvent(event);
  }

private:
  struct CubeFaceProjection {
    ArtifactCore::ViewOrientationHotspot hotspot =
        ArtifactCore::ViewOrientationHotspot::None;
    QPolygonF polygon;
    QRectF labelRect;
    QColor baseFill;
    QColor baseBorder;
    bool visible = false;
    float depth = 0.0f;
  };

  struct CubeSnapTarget {
    ArtifactCore::ViewOrientationHotspot hotspot =
        ArtifactCore::ViewOrientationHotspot::None;
    QPointF start;
    QPointF end;
    QPointF center;
    qreal thickness = 0.0;
    bool visible = false;
    float depth = 0.0f;
  };

  struct CubeCornerTarget {
    ArtifactCore::ViewOrientationHotspot hotspot =
        ArtifactCore::ViewOrientationHotspot::None;
    QPointF center;
    qreal radius = 0.0;
    bool visible = false;
    float depth = 0.0f;
  };

  static QString hotspotLabel(ArtifactCore::ViewOrientationHotspot hotspot,
                              bool compact = false) {
    switch (hotspot) {
    case ArtifactCore::ViewOrientationHotspot::Top:
      return QStringLiteral("Top");
    case ArtifactCore::ViewOrientationHotspot::Bottom:
      return QStringLiteral("Bottom");
    case ArtifactCore::ViewOrientationHotspot::Left:
      return QStringLiteral("Left");
    case ArtifactCore::ViewOrientationHotspot::Right:
      return QStringLiteral("Right");
    case ArtifactCore::ViewOrientationHotspot::Front:
      return QStringLiteral("Front");
    case ArtifactCore::ViewOrientationHotspot::Back:
      return QStringLiteral("Back");
    case ArtifactCore::ViewOrientationHotspot::FrontTop:
      return compact ? QStringLiteral("F/T") : QStringLiteral("Front Top");
    case ArtifactCore::ViewOrientationHotspot::FrontBottom:
      return compact ? QStringLiteral("F/B") : QStringLiteral("Front Bottom");
    case ArtifactCore::ViewOrientationHotspot::FrontLeft:
      return compact ? QStringLiteral("F/L") : QStringLiteral("Front Left");
    case ArtifactCore::ViewOrientationHotspot::FrontRight:
      return compact ? QStringLiteral("F/R") : QStringLiteral("Front Right");
    case ArtifactCore::ViewOrientationHotspot::BackTop:
      return compact ? QStringLiteral("B/T") : QStringLiteral("Back Top");
    case ArtifactCore::ViewOrientationHotspot::BackBottom:
      return compact ? QStringLiteral("B/B") : QStringLiteral("Back Bottom");
    case ArtifactCore::ViewOrientationHotspot::BackLeft:
      return compact ? QStringLiteral("B/L") : QStringLiteral("Back Left");
    case ArtifactCore::ViewOrientationHotspot::BackRight:
      return compact ? QStringLiteral("B/R") : QStringLiteral("Back Right");
    case ArtifactCore::ViewOrientationHotspot::LeftTop:
      return compact ? QStringLiteral("L/T") : QStringLiteral("Left Top");
    case ArtifactCore::ViewOrientationHotspot::LeftBottom:
      return compact ? QStringLiteral("L/B") : QStringLiteral("Left Bottom");
    case ArtifactCore::ViewOrientationHotspot::RightTop:
      return compact ? QStringLiteral("R/T") : QStringLiteral("Right Top");
    case ArtifactCore::ViewOrientationHotspot::RightBottom:
      return compact ? QStringLiteral("R/B") : QStringLiteral("Right Bottom");
    case ArtifactCore::ViewOrientationHotspot::FrontTopLeft:
      return compact ? QStringLiteral("FTL") : QStringLiteral("Front Top Left");
    case ArtifactCore::ViewOrientationHotspot::FrontTopRight:
      return compact ? QStringLiteral("FTR") : QStringLiteral("Front Top Right");
    case ArtifactCore::ViewOrientationHotspot::FrontBottomLeft:
      return compact ? QStringLiteral("FBL") : QStringLiteral("Front Bottom Left");
    case ArtifactCore::ViewOrientationHotspot::FrontBottomRight:
      return compact ? QStringLiteral("FBR") : QStringLiteral("Front Bottom Right");
    case ArtifactCore::ViewOrientationHotspot::BackTopLeft:
      return compact ? QStringLiteral("BTL") : QStringLiteral("Back Top Left");
    case ArtifactCore::ViewOrientationHotspot::BackTopRight:
      return compact ? QStringLiteral("BTR") : QStringLiteral("Back Top Right");
    case ArtifactCore::ViewOrientationHotspot::BackBottomLeft:
      return compact ? QStringLiteral("BBL") : QStringLiteral("Back Bottom Left");
    case ArtifactCore::ViewOrientationHotspot::BackBottomRight:
      return compact ? QStringLiteral("BBR") : QStringLiteral("Back Bottom Right");
    default:
      return QStringLiteral("-");
    }
  }

  std::array<CubeFaceProjection, 6> projectedFaces() const {
    const QRectF bounds = rect().adjusted(16.0, 14.0, -16.0, -20.0);
    const QPointF center(bounds.center().x(), bounds.center().y());
    const float cubeRadius =
        static_cast<float>(std::max(24.0, std::min(bounds.width(), bounds.height()) * 0.28));
    const auto rotate = [this](float x, float y, float z) {
      return orientation_.rotatedVector(QVector3D(x, y, z));
    };
    const auto project = [center, cubeRadius](const QVector3D &v) {
      const float perspective = 1.0f + v.z() * 0.22f;
      return QPointF(center.x() + v.x() * cubeRadius * perspective,
                     center.y() - v.y() * cubeRadius * perspective);
    };
    const std::array<QVector3D, 8> vertices = {{
        rotate(-1.0f, 1.0f, 1.0f),  rotate(1.0f, 1.0f, 1.0f),
        rotate(1.0f, -1.0f, 1.0f),  rotate(-1.0f, -1.0f, 1.0f),
        rotate(-1.0f, 1.0f, -1.0f), rotate(1.0f, 1.0f, -1.0f),
        rotate(1.0f, -1.0f, -1.0f), rotate(-1.0f, -1.0f, -1.0f),
    }};
    const std::array<QPointF, 8> projected = {{
        project(vertices[0]), project(vertices[1]), project(vertices[2]),
        project(vertices[3]), project(vertices[4]), project(vertices[5]),
        project(vertices[6]), project(vertices[7]),
    }};
    struct FaceDef {
      ArtifactCore::ViewOrientationHotspot hotspot;
      std::array<int, 4> indices;
      QVector3D normal;
      QColor fill;
      QColor border;
    };
    const std::array<FaceDef, 6> defs = {{
        {ArtifactCore::ViewOrientationHotspot::Front, {0, 1, 2, 3},
         QVector3D(0.0f, 0.0f, 1.0f), QColor(76, 120, 168, 210),
         QColor(132, 188, 236, 210)},
        {ArtifactCore::ViewOrientationHotspot::Back, {5, 4, 7, 6},
         QVector3D(0.0f, 0.0f, -1.0f), QColor(42, 56, 78, 190),
         QColor(96, 124, 156, 180)},
        {ArtifactCore::ViewOrientationHotspot::Left, {4, 0, 3, 7},
         QVector3D(-1.0f, 0.0f, 0.0f), QColor(58, 86, 118, 205),
         QColor(118, 166, 214, 190)},
        {ArtifactCore::ViewOrientationHotspot::Right, {1, 5, 6, 2},
         QVector3D(1.0f, 0.0f, 0.0f), QColor(64, 96, 132, 205),
         QColor(125, 176, 226, 196)},
        {ArtifactCore::ViewOrientationHotspot::Top, {4, 5, 1, 0},
         QVector3D(0.0f, 1.0f, 0.0f), QColor(88, 128, 168, 220),
         QColor(148, 204, 248, 205)},
        {ArtifactCore::ViewOrientationHotspot::Bottom, {3, 2, 6, 7},
         QVector3D(0.0f, -1.0f, 0.0f), QColor(36, 48, 66, 185),
         QColor(90, 112, 142, 170)},
    }};

    std::array<CubeFaceProjection, 6> faces{};
    for (int i = 0; i < static_cast<int>(defs.size()); ++i) {
      const auto &def = defs[i];
      auto &face = faces[i];
      face.hotspot = def.hotspot;
      face.baseFill = def.fill;
      face.baseBorder = def.border;
      const QVector3D rotatedNormal = orientation_.rotatedVector(def.normal);
      face.visible = rotatedNormal.z() > 0.0f;
      face.depth = rotatedNormal.z();
      QPolygonF polygon;
      polygon.reserve(4);
      QRectF labelRect;
      QPointF centroid(0.0, 0.0);
      for (const int index : def.indices) {
        polygon << projected[index];
        centroid += projected[index];
      }
      centroid /= 4.0;
      face.polygon = polygon;
      labelRect = QRectF(centroid.x() - 22.0, centroid.y() - 10.0, 44.0, 20.0);
      face.labelRect = labelRect;
    }
    std::sort(faces.begin(), faces.end(),
              [](const CubeFaceProjection &a, const CubeFaceProjection &b) {
                return a.depth < b.depth;
              });
    return faces;
  }

  std::vector<CubeSnapTarget> projectedSnapTargets() const {
    const QRectF bounds = rect().adjusted(16.0, 14.0, -16.0, -20.0);
    const QPointF center(bounds.center().x(), bounds.center().y());
    const float cubeRadius = static_cast<float>(
        std::max(24.0, std::min(bounds.width(), bounds.height()) * 0.28));
    const auto project = [center, cubeRadius](const QVector3D &v) {
      const float perspective = 1.0f + v.z() * 0.22f;
      return QPointF(center.x() + v.x() * cubeRadius * perspective,
                     center.y() - v.y() * cubeRadius * perspective);
    };
    struct EdgeDef {
      ArtifactCore::ViewOrientationHotspot hotspot;
      QVector3D start;
      QVector3D end;
    };
    const std::array<EdgeDef, 12> edges = {{
        {ArtifactCore::ViewOrientationHotspot::FrontTop,
         {-1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}},
        {ArtifactCore::ViewOrientationHotspot::FrontBottom,
         {-1.0f, -1.0f, 1.0f}, {1.0f, -1.0f, 1.0f}},
        {ArtifactCore::ViewOrientationHotspot::FrontLeft,
         {-1.0f, 1.0f, 1.0f}, {-1.0f, -1.0f, 1.0f}},
        {ArtifactCore::ViewOrientationHotspot::FrontRight,
         {1.0f, 1.0f, 1.0f}, {1.0f, -1.0f, 1.0f}},
        {ArtifactCore::ViewOrientationHotspot::BackTop,
         {-1.0f, 1.0f, -1.0f}, {1.0f, 1.0f, -1.0f}},
        {ArtifactCore::ViewOrientationHotspot::BackBottom,
         {-1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, -1.0f}},
        {ArtifactCore::ViewOrientationHotspot::BackLeft,
         {-1.0f, 1.0f, -1.0f}, {-1.0f, -1.0f, -1.0f}},
        {ArtifactCore::ViewOrientationHotspot::BackRight,
         {1.0f, 1.0f, -1.0f}, {1.0f, -1.0f, -1.0f}},
        {ArtifactCore::ViewOrientationHotspot::LeftTop,
         {-1.0f, 1.0f, -1.0f}, {-1.0f, 1.0f, 1.0f}},
        {ArtifactCore::ViewOrientationHotspot::LeftBottom,
         {-1.0f, -1.0f, -1.0f}, {-1.0f, -1.0f, 1.0f}},
        {ArtifactCore::ViewOrientationHotspot::RightTop,
         {1.0f, 1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}},
        {ArtifactCore::ViewOrientationHotspot::RightBottom,
         {1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, 1.0f}},
    }};

    std::vector<CubeSnapTarget> targets;
    targets.reserve(edges.size());
    for (const auto &edge : edges) {
      const QVector3D rotatedStart = orientation_.rotatedVector(edge.start);
      const QVector3D rotatedEnd = orientation_.rotatedVector(edge.end);
      const QVector3D rotatedCenter = (rotatedStart + rotatedEnd) * 0.5f;
      CubeSnapTarget target;
      target.hotspot = edge.hotspot;
      target.start = project(rotatedStart);
      target.end = project(rotatedEnd);
      target.center = project(rotatedCenter);
      target.thickness = 6.0;
      target.depth = rotatedCenter.z();
      target.visible = rotatedCenter.z() > 0.02f;
      targets.push_back(target);
    }

    std::sort(targets.begin(), targets.end(),
              [](const CubeSnapTarget &a, const CubeSnapTarget &b) {
                return a.depth < b.depth;
              });
    return targets;
  }

  std::vector<CubeCornerTarget> projectedCornerTargets() const {
    const QRectF bounds = rect().adjusted(16.0, 14.0, -16.0, -20.0);
    const QPointF center(bounds.center().x(), bounds.center().y());
    const float cubeRadius = static_cast<float>(
        std::max(24.0, std::min(bounds.width(), bounds.height()) * 0.28));
    const auto project = [center, cubeRadius](const QVector3D &v) {
      const float perspective = 1.0f + v.z() * 0.22f;
      return QPointF(center.x() + v.x() * cubeRadius * perspective,
                     center.y() - v.y() * cubeRadius * perspective);
    };
    struct CornerDef {
      ArtifactCore::ViewOrientationHotspot hotspot;
      QVector3D position;
    };
    const std::array<CornerDef, 8> corners = {{
        {ArtifactCore::ViewOrientationHotspot::FrontTopLeft,
         {-1.0f, 1.0f, 1.0f}},
        {ArtifactCore::ViewOrientationHotspot::FrontTopRight,
         {1.0f, 1.0f, 1.0f}},
        {ArtifactCore::ViewOrientationHotspot::FrontBottomLeft,
         {-1.0f, -1.0f, 1.0f}},
        {ArtifactCore::ViewOrientationHotspot::FrontBottomRight,
         {1.0f, -1.0f, 1.0f}},
        {ArtifactCore::ViewOrientationHotspot::BackTopLeft,
         {-1.0f, 1.0f, -1.0f}},
        {ArtifactCore::ViewOrientationHotspot::BackTopRight,
         {1.0f, 1.0f, -1.0f}},
        {ArtifactCore::ViewOrientationHotspot::BackBottomLeft,
         {-1.0f, -1.0f, -1.0f}},
        {ArtifactCore::ViewOrientationHotspot::BackBottomRight,
         {1.0f, -1.0f, -1.0f}},
    }};

    std::vector<CubeCornerTarget> targets;
    targets.reserve(corners.size());
    for (const auto &corner : corners) {
      const QVector3D rotated = orientation_.rotatedVector(corner.position);
      CubeCornerTarget target;
      target.hotspot = corner.hotspot;
      target.center = project(rotated);
      target.radius = rotated.z() > 0.45f ? 4.5 : 3.8;
      target.visible = rotated.z() > 0.0f;
      target.depth = rotated.z();
      targets.push_back(target);
    }

    std::sort(targets.begin(), targets.end(),
              [](const CubeCornerTarget &a, const CubeCornerTarget &b) {
                return a.depth < b.depth;
              });
    return targets;
  }

  ArtifactCore::ViewOrientationHotspot hotspotAt(const QPointF &pos) const {
    const auto cornerTargets = projectedCornerTargets();
    for (auto it = cornerTargets.rbegin(); it != cornerTargets.rend(); ++it) {
      if (!it->visible) {
        continue;
      }
      const QPointF delta = pos - it->center;
      const qreal hitRadius = it->radius + 1.75;
      if ((delta.x() * delta.x()) + (delta.y() * delta.y()) <=
          hitRadius * hitRadius) {
        return it->hotspot;
      }
    }
    const auto snapTargets = projectedSnapTargets();
    for (auto it = snapTargets.rbegin(); it != snapTargets.rend(); ++it) {
      if (!it->visible) {
        continue;
      }
      const QPointF edge = it->end - it->start;
      const qreal edgeLengthSquared =
          edge.x() * edge.x() + edge.y() * edge.y();
      if (edgeLengthSquared <= 0.0) {
        continue;
      }
      const QPointF fromStart = pos - it->start;
      const qreal t = std::clamp(
          (fromStart.x() * edge.x() + fromStart.y() * edge.y()) /
              edgeLengthSquared,
          0.0, 1.0);
      const QPointF nearest = it->start + edge * t;
      const QPointF delta = pos - nearest;
      const qreal hitRadius = it->thickness * 0.5 + 3.0;
      if ((delta.x() * delta.x()) + (delta.y() * delta.y()) <=
          hitRadius * hitRadius) {
        return it->hotspot;
      }
    }
    auto faces = projectedFaces();
    for (auto it = faces.rbegin(); it != faces.rend(); ++it) {
      if (it->visible && it->polygon.containsPoint(pos, Qt::OddEvenFill)) {
        return it->hotspot;
      }
    }
    return ArtifactCore::ViewOrientationHotspot::None;
  }

  ArtifactCore::ViewOrientationNavigator navigator_;
  QQuaternion orientation_;
  ArtifactCore::ViewOrientationHotspot hotspot_ =
      ArtifactCore::ViewOrientationHotspot::Front;
  ArtifactCore::ViewOrientationHotspot hoverHotspot_ =
      ArtifactCore::ViewOrientationHotspot::None;
  std::function<void(ArtifactCore::ViewOrientationHotspot)> activatedCallback_;
  std::function<void(const QQuaternion &)> orbitChangedCallback_;
  ArtifactCore::ViewOrientationHotspot pressedHotspot_ =
      ArtifactCore::ViewOrientationHotspot::None;
  QPointF dragStartPos_;
  QQuaternion dragStartOrientation_;
  bool pressArmed_ = false;
  bool dragActive_ = false;
};

}
