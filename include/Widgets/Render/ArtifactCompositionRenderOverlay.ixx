module;

#include <QFont>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QSize>
#include <QVector>

export module Artifact.Widgets.CompositionRenderOverlay;

import Artifact.Render.IRenderer;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Widgets.PieMenu;

export namespace Artifact {

void drawCompositionRegionOverlay(ArtifactIRenderer *renderer,
                                  const ArtifactCompositionPtr &comp);

void drawAnchorCenterOverlay(ArtifactIRenderer *renderer,
                             const ArtifactAbstractLayerPtr &layer);

void drawSelectionOverlay(ArtifactIRenderer *renderer,
                          const ArtifactAbstractLayerPtr &layer);

void drawCameraSelectionOverlay(ArtifactIRenderer *renderer,
                                const ArtifactAbstractLayerPtr &layer,
                                bool isActiveCamera);

void drawSelectionSummaryOverlay(ArtifactIRenderer *renderer,
                                 const ArtifactAbstractLayerPtr &layer,
                                 int overlayW,
                                 int overlayH);

void drawViewportDropGhostOverlay(ArtifactIRenderer *renderer,
                                  const ArtifactCompositionPtr &comp,
                                  float overlayW,
                                  float overlayH,
                                  const QRectF &ghostRect,
                                  const QString &ghostTitle,
                                  const QString &ghostHint,
                                  const QString &dropCandidateLabel);

void drawViewportInfoOverlay(ArtifactIRenderer *renderer,
                             int overlayW,
                             int overlayH,
                             const QString &title,
                             const QString &detail,
                             const QSize *restoreCanvasSize = nullptr);

void drawViewportStatusChipOverlay(ArtifactIRenderer *renderer,
                                   int overlayW,
                                   int overlayH,
                                   const QString &statusText,
                                   const QSize *restoreCanvasSize = nullptr);

void drawViewportSnapHintOverlay(ArtifactIRenderer *renderer,
                                 int overlayW,
                                 int overlayH,
                                 bool snapBypassed,
                                 const QString &snapTitle,
                                 const QString &snapDetail,
                                 int verticalCount = 0,
                                 int horizontalCount = 0,
                                 const QSize *restoreCanvasSize = nullptr);

void drawViewportCommandPaletteOverlay(ArtifactIRenderer *renderer,
                                       float overlayW,
                                       float overlayH,
                                       const QRectF &panel,
                                       const QString &queryText,
                                       const QStringList &items);

void drawViewportContextMenuOverlay(ArtifactIRenderer *renderer,
                                    float overlayW,
                                    float overlayH,
                                    const QRectF &panel,
                                    const QString &title,
                                    const QString &subtitle,
                                    const QStringList &items,
                                    const QVector<bool> &enabledItems);

void drawViewportPieMenuOverlay(ArtifactIRenderer *renderer,
                                float overlayW,
                                float overlayH,
                                const QRectF &rect,
                                const PieMenuModel &model,
                                int selectedIndex);

} // namespace Artifact
