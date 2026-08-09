module;

#include <QMatrix4x4>
#include <QPointF>
#include <QRect>
#include <QVector3D>

module Artifact.Widgets.ViewportMath;

namespace Artifact::ViewportMath {

QPointF toTopDown(const QPointF &openGLPoint, const QRect &viewport) {
  return QPointF(openGLPoint.x(),
                 static_cast<qreal>(viewport.y() + viewport.height()) -
                     openGLPoint.y());
}

QPointF toOpenGL(const QPointF &topDownPoint, const QRect &viewport) {
  return QPointF(topDownPoint.x(),
                 static_cast<qreal>(viewport.y() + viewport.height()) -
                     topDownPoint.y());
}

QVector3D projectToTopDown(const QVector3D &worldPoint,
                           const QMatrix4x4 &view,
                           const QMatrix4x4 &projection,
                           const QRect &viewport) {
  QVector3D projected = worldPoint.project(view, projection, viewport);
  projected.setY(static_cast<float>(
      toTopDown(QPointF(projected.x(), projected.y()), viewport).y()));
  return projected;
}

QVector3D unprojectFromTopDown(const QVector3D &topDownPoint,
                               const QMatrix4x4 &view,
                               const QMatrix4x4 &projection,
                               const QRect &viewport) {
  QVector3D openGLPoint = topDownPoint;
  openGLPoint.setY(static_cast<float>(
      toOpenGL(QPointF(topDownPoint.x(), topDownPoint.y()), viewport).y()));
  return openGLPoint.unproject(view, projection, viewport);
}

} // namespace Artifact::ViewportMath
