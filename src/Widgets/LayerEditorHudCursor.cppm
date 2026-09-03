module;

#include <QHash>
#include <QPixmap>

module Artifact.Widgets.LayerEditor.HudCursor;

import Artifact.Widgets.TransformGizmo;
import Utils.Path;

namespace Artifact {

const QCursor& layerEditorHudCursor(
    const QString& iconName,
    const Qt::CursorShape fallbackShape,
    const int hotX,
    const int hotY)
{
 static QHash<QString, QCursor> cache;
 const QString key = iconName + QStringLiteral("|%1|%2|%3")
     .arg(static_cast<int>(fallbackShape))
     .arg(hotX)
     .arg(hotY);
 auto it = cache.constFind(key);
 if (it != cache.constEnd()) return it.value();

 QPixmap pixmap(ArtifactCore::resolveIconPath(
     QStringLiteral("Studio/%1").arg(iconName)));
 if (!pixmap.isNull()) {
  pixmap = pixmap.scaled(
      24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  it = cache.insert(key, QCursor(pixmap, hotX, hotY));
  return it.value();
 }
 it = cache.insert(key, QCursor(fallbackShape));
 return it.value();
}

const QCursor& layerEditorHudCursorForTransformHandle(
    const TransformGizmo::HandleType handle,
    const bool dragging)
{
 switch (handle) {
 case TransformGizmo::HandleType::Move:
 case TransformGizmo::HandleType::Scale_Center:
  return layerEditorHudCursor(
      QStringLiteral("hud_cursor_move.svg"),
      dragging ? Qt::ClosedHandCursor : Qt::SizeAllCursor);
 case TransformGizmo::HandleType::Scale_L:
 case TransformGizmo::HandleType::Scale_R:
  return layerEditorHudCursor(
      QStringLiteral("hud_cursor_scale_horizontal.svg"), Qt::SizeHorCursor);
 case TransformGizmo::HandleType::Scale_T:
 case TransformGizmo::HandleType::Scale_B:
  return layerEditorHudCursor(
      QStringLiteral("hud_cursor_scale_vertical.svg"), Qt::SizeVerCursor);
 case TransformGizmo::HandleType::Scale_TL:
 case TransformGizmo::HandleType::Scale_TR:
 case TransformGizmo::HandleType::Scale_BL:
 case TransformGizmo::HandleType::Scale_BR:
  return layerEditorHudCursor(
      QStringLiteral("hud_cursor_scale_uniform.svg"), Qt::SizeFDiagCursor);
 case TransformGizmo::HandleType::Rotate:
  return layerEditorHudCursor(
      QStringLiteral("hud_cursor_rotate_corner.svg"), Qt::CrossCursor);
 case TransformGizmo::HandleType::Anchor:
  return layerEditorHudCursor(
      QStringLiteral("hud_cursor_anchor.svg"), Qt::CrossCursor);
 default:
  return layerEditorHudCursor(
      QStringLiteral("hud_cursor_select.svg"), Qt::ArrowCursor, 2, 2);
 }
}

}
