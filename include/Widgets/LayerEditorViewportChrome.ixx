module;

#include <QRectF>
#include <QPointF>
#include <QSize>
#include <QString>

export module Artifact.Widgets.LayerEditor.ViewportChrome;

import Tool;
import Artifact.Layer.Abstract;

export namespace Artifact {

enum class LayerEditorSurfaceMode { Edit, Inspect, Impact };

QString layerEditorEditModeLabel(EditMode mode);
QString layerEditorDisplayModeLabel(DisplayMode mode);
bool layerEditorEditModeAvailable(const ArtifactAbstractLayerPtr& layer,
                                  EditMode mode);

QRectF layerEditorSurfaceModeRect(float viewportWidth, float viewportHeight);
QRectF layerEditorSurfaceModeItemsRect(float viewportWidth, float viewportHeight);
QRectF layerEditorSurfaceSoloRect(float viewportWidth, float viewportHeight);
QRectF layerEditorEditToolRect(float viewportWidth, float viewportHeight);
QRectF layerEditorDisplayModeRect(float viewportWidth, float viewportHeight);
QRectF layerEditorOrientationRect(float viewportWidth, float viewportHeight);
QRectF layerEditorZoomRect(float viewportWidth, float viewportHeight);
float layerEditorZoomStop(float panelWidth, int stop);
int layerEditorZoomControlIndex(float panelWidth, float relativeX);
QRectF layerEditorStateCardRect(float viewportWidth, float viewportHeight);
QString layerEditorChromeToolTip(int control);
int layerEditorChromeControlAt(const QPointF& position,
                               const QSize& viewportSize,
                               LayerEditorSurfaceMode surfaceMode,
                               bool hasLayer);

}
