module;

#include <QMatrix4x4>
#include <QPointF>
#include <QRect>
#include <QVector3D>

export module Artifact.Widgets.ViewportMath;

export namespace Artifact::ViewportMath {

// The public viewport contract is always Qt/widget-style Top-Down pixels.
// Qt's QVector3D project/unproject API uses OpenGL-style Bottom-Up Y values,
// so the conversion stays inside this module instead of leaking to callers.
QPointF toTopDown(const QPointF &openGLPoint, const QRect &viewport);
QPointF toOpenGL(const QPointF &topDownPoint, const QRect &viewport);

QVector3D projectToTopDown(const QVector3D &worldPoint,
                           const QMatrix4x4 &view,
                           const QMatrix4x4 &projection,
                           const QRect &viewport);

QVector3D unprojectFromTopDown(const QVector3D &topDownPoint,
                               const QMatrix4x4 &view,
                               const QMatrix4x4 &projection,
                               const QRect &viewport);

} // namespace Artifact::ViewportMath
