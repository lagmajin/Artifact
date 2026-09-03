module;
#include <QAction>
#include <QActionGroup>
#include <QClipboard>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QCursor>
#include <QDebug>
#include <QLoggingCategory>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QEvent>
#include <QFrame>
#include <QFileInfo>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QFont>
#include <QPalette>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHash>
#include <QHideEvent>
#include <QIcon>
#include <QInputDialog>
#include <QInputMethodEvent>
#include <QImageReader>
#include <QKeySequence>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTabletEvent>
#include <QPointer>
#include <QPolygonF>
#include <QQuaternion>
#include <QMessageBox>
#include <QResizeEvent>
#include <QRegion>
#include <QSet>
#include <QLineEdit>
#include <QPushButton>
#include <QShortcut>
#include <QSpinBox>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTabWidget>
#include <QStringList>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTransform>
#include <QVBoxLayout>
#include <QVariant>
#include <QLabel>
#include <QVector>
#include <QPair>
#include <QVector3D>
#include <QWheelEvent>
#include <QFileDialog>
#include <QProgressDialog>
#include <QFile>
#include <QApplication>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QWidget>
#include <QWidgetAction>
#include <QtSVG/QSvgRenderer>
#include <algorithm>
#include <cmath>
#include <array>
#include <atomic>
#include <deque>
#include <functional>
#include <chrono>
#include <thread>
#include <utility>
#include <wobjectimpl.h>
#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#endif

module Artifact.Widgets.CompositionEditor;
import Memory.SharedPtr;

import Artifact.Widgets.CompositionRenderController;
import Artifact.Widgets.CompositionEmptyOverlay;
import Artifact.Widgets.CompositionTextEditor;
import Artifact.Widgets.CompositionCleanup;
import Artifact.Widgets.ViewOrientationWidget;
import Artifact.Widgets.PointTrackerGizmo;
import Artifact.Service.Application;
import Tool;
import Artifact.Contents.Viewer;
import Artifact.Widgets.TransformGizmo;
import Artifact.Widgets.Gizmo3D;
import Artifact.Widgets.PieMenu;
import Input.Operator;
import UI.ShortcutBindings;
import UI.View.Orientation.Navigator;
import Math.Interpolate;
import Color.Float;
import Artifact.Render.Config;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Factory;
import Artifact.Layers.Abstract._2D;
import Artifact.Layer.Paint;
import Artifact.Render.Queue.Service;
import Artifact.Layer.Shape;
import Artifact.Layer.Text;
import Artifact.Layer.Svg;
import Artifact.Layer.Image;
import NLE.Core;
import NLE.OTIO;
import Artifact.Layers.SolidImage;
import Artifact.Layers.Noise;
import Artifact.Application.Manager;
import Artifact.Layers.Selection.Manager;
import Artifact.Service.ActiveContext;
import Artifact.Service.Project;
import Artifact.Template.Document;
import Artifact.Service.Playback;

import Artifact.Audio.ScrubController;
import Artifact.Application.ProjectBundleIpc;
import Time.Rational;
import Artifact.Layer.Video;
import Artifact.Layer.Clone;
import Artifact.Layer.Camera;
import Artifact.Layer.ParametricComposition;
import Artifact.Tool.Manager;
import Artifact.Tool.PuppetTool;
import FloatColorPickerDialog;
import Artifact.Widgets.CreateCameraLayerDialog;
import Artifact.Widgets.CreateNoiseLayerDialog;
import Clipboard.ClipboardManager;
import Utils.Path;
import Utils.String.UniString;
import Artifact.Layer.InitParams;
import File.TypeDetector;
import Application.AppSettings;
import Widgets.Utils.CSS;
import Event.Bus;
import Artifact.Event.Types;
import Artifact.Widgets.ProfilerOverlay;
import Artifact.Widgets.ProfilerPanel;
import Artifact.Widgets.EventBusDebugger;
import Artifact.Widget.Dialog.ScreenshotExport;
import Dialog.Composition;
import ArtifactCore.Utils.PerformanceProfiler;
import Image.ImageF32x4_RGBA;
import Artifact.Render.IRenderer;
import IO.ImageExporter;
import Image.ExportOptions;
import VectorScopeWidget;
import WaveformScopeWidget;
import ParadeScopeWidget;
import HistgramWidget;
import Codec.Thumbnail.FFmpeg;
import UI.ShortcutBindings;
import Undo.UndoManager;
import Configuration.LayeredConfigStore;
import Configuration.ConfigLayer;

namespace Artifact {

W_OBJECT_IMPL(ArtifactCompositionEditor)
Q_LOGGING_CATEGORY(compositionViewLog, "artifact.compositionview");

namespace {
double safeCompositionFrameRate(const ArtifactAbstractComposition *composition)
{
  if (!composition) {
    return 30.0;
  }
  const double rawFps = composition->frameRate().framerate();
  if (!std::isfinite(rawFps) || rawFps <= 0.0) {
    return 30.0;
  }
  return std::clamp(rawFps, 1.0, 10000.0);
}

double safeCompositionFrameRate(const ArtifactCompositionPtr& composition)
{
  return safeCompositionFrameRate(composition.get());
}

bool commitAnimationLayerSnapshot(const ArtifactAbstractLayerPtr& layer,
                                  const QJsonObject& before,
                                  const QJsonObject& after)
{
  if (!layer) {
    return false;
  }
  if (auto* manager = UndoManager::instance()) {
    const bool accepted = manager->push(
        std::make_unique<AnimationLayerStackSnapshotCommand>(
            layer, before, after));
    if (!accepted) {
      layer->restoreAnimationLayersSnapshot(before);
      layer->changed();
    }
    return accepted;
  }

  layer->restoreAnimationLayersSnapshot(after);
  layer->changed();
  return layer->animationLayersSnapshot() == after;
}

int compositionFrameRateScale(const ArtifactAbstractComposition *composition)
{
  return std::max(1, static_cast<int>(std::llround(
      safeCompositionFrameRate(composition))));
}

int compositionFrameRateScale(const ArtifactCompositionPtr& composition)
{
  return compositionFrameRateScale(composition.get());
}

QDockWidget* findDockByTitle(QWidget* window, const QString& title)
{
  if (!window) {
    return nullptr;
  }
  const auto docks = window->findChildren<QDockWidget*>();
  for (QDockWidget* dock : docks) {
    if (dock && dock->windowTitle() == title) {
      return dock;
    }
  }
  return nullptr;
}

void activateDock(QWidget* window, const QString& title)
{
  auto* dock = findDockByTitle(window, title);
  if (!dock) {
    return;
  }
  dock->setVisible(true);
  dock->raise();
  dock->activateWindow();
}

void openContentsViewerCompareSurfaceImpl()
{
  ArtifactContentsViewer *viewer = nullptr;
  for (QWidget *widget : QApplication::allWidgets()) {
    viewer = qobject_cast<ArtifactContentsViewer *>(widget);
    if (viewer) {
      break;
    }
  }
  if (!viewer) {
    return;
  }

  for (QWidget *widget : QApplication::topLevelWidgets()) {
    if (auto *mainWindow = qobject_cast<QWidget *>(widget)) {
      activateDock(mainWindow, QStringLiteral("Contents Viewer"));
      break;
    }
  }

  viewer->setViewerMode(ContentsViewerMode::Compare);
  viewer->raise();
  viewer->activateWindow();
  viewer->setFocus(Qt::OtherFocusReason);
}

QCursor makeMaskAddCursor()
{
  static const QCursor cursor = []() {
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QColor darkLine(0, 0, 0, 220);
    const QColor lightLine(255, 255, 255, 245);
    QPen pen(darkLine, 1.8);
    pen.setCapStyle(Qt::SquareCap);
    painter.setPen(pen);
    painter.drawLine(12, 2, 12, 22);
    painter.drawLine(2, 12, 22, 12);

    pen.setColor(lightLine);
    pen.setWidthF(1.0);
    painter.setPen(pen);
    painter.drawLine(12, 4, 12, 20);
    painter.drawLine(4, 12, 20, 12);

    pen.setColor(QColor(255, 255, 255, 255));
    pen.setWidthF(1.6);
    painter.setPen(pen);
    painter.drawLine(9, 12, 15, 12);
    painter.drawLine(12, 9, 12, 15);

    painter.end();
    return QCursor(pixmap, 12, 12);
  }();
  return cursor;
}

void polishEditorMenu(QMenu* menu, QWidget* owner)
{
  if (!menu) {
    return;
  }

  const auto& theme = ArtifactCore::currentDCCTheme();
  QPalette pal = menu->palette();
  pal.setColor(QPalette::Window, QColor(theme.secondaryBackgroundColor));
  pal.setColor(QPalette::Base, QColor(theme.secondaryBackgroundColor));
  pal.setColor(QPalette::Button, QColor(theme.secondaryBackgroundColor));
  pal.setColor(QPalette::Text, QColor(theme.textColor));
  pal.setColor(QPalette::WindowText, QColor(theme.textColor));
  pal.setColor(QPalette::ButtonText, QColor(theme.textColor));
  pal.setColor(QPalette::Highlight, QColor(theme.accentColor));
  pal.setColor(QPalette::HighlightedText, QColor(theme.backgroundColor));
  menu->setPalette(pal);

  QFont font = owner ? owner->font() : menu->font();
  if (font.pointSize() < 11) {
    font.setPointSize(11);
  }
  menu->setFont(font);
}

// CompositionEditor 内部の同期は Qt signal を増やさず、
// ここで定義する deferred event に集約する。
// selection / tool label / fit などの状態変化は postEvent でまとめて反映する。
class CompositionEditorDeferredEvent final : public QEvent {
public:
  enum class Kind {
    SelectionSync,
    ToolLabelSync,
  };

  static QEvent::Type eventType() {
    static const int typeId = QEvent::registerEventType();
    return static_cast<QEvent::Type>(typeId);
  }

  explicit CompositionEditorDeferredEvent(Kind kind)
      : QEvent(eventType()), kind(kind) {}

  Kind kind;
};

QIcon loadIconWithFallback(const QString &fileName) {
  const QString resourcePath = ArtifactCore::resolveIconResourcePath(fileName);
  QIcon icon(resourcePath);
  if (!icon.isNull()) {
    return icon;
  }
  return QIcon(ArtifactCore::resolveIconPath(fileName));
}

QIcon loadEditorMenuIcon(const QString &fileName) {
  return loadIconWithFallback(fileName);
}

QImage selectedLayerDebugImage(const ArtifactAbstractLayerPtr& layer) {
  if (!layer) {
    return {};
  }

  if (const auto video = ArtifactCore::dynamicPointerCast<ArtifactVideoLayer>(layer)) {
    return video->currentFrameToQImage();
  }
  if (const auto text = ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(layer)) {
    return text->toQImage();
  }
  if (const auto image = ArtifactCore::dynamicPointerCast<ArtifactImageLayer>(layer)) {
    return image->toQImage();
  }
  if (const auto solidImage =
          ArtifactCore::dynamicPointerCast<ArtifactSolidImageLayer>(layer)) {
    return solidImage->toQImage();
  }
  if (const auto svg = ArtifactCore::dynamicPointerCast<ArtifactSvgLayer>(layer)) {
    return svg->toQImage();
  }
  if (const auto shape = ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(layer)) {
    return shape->toQImage();
  }
  if (const auto clone = ArtifactCore::dynamicPointerCast<ArtifactCloneLayer>(layer)) {
    return clone->toQImage();
  }
  return {};
}

QString screenshotDefaultExtensionForFilter(const QString& selectedFilter)
{
  const QString filter = selectedFilter.toLower();
  if (filter.contains(QStringLiteral("exr"))) {
    return QStringLiteral("exr");
  }
  if (filter.contains(QStringLiteral("jpg")) || filter.contains(QStringLiteral("jpeg"))) {
    return QStringLiteral("jpg");
  }
  return QStringLiteral("png");
}

QString ensureScreenshotSuffix(QString path, const QString& selectedFilter)
{
  if (path.isEmpty()) {
    return path;
  }
  if (QFileInfo(path).suffix().isEmpty()) {
    path += QStringLiteral(".");
    path += screenshotDefaultExtensionForFilter(selectedFilter);
  }
  return path;
}

QImage captureCompositionScreenshot(CompositionRenderController* controller, QWidget* fallbackWidget)
{
  const auto hasVisiblePixels = [](const QImage& image) {
    if (image.isNull()) return false;
    const int stepX = std::max(1, image.width() / 8);
    const int stepY = std::max(1, image.height() / 8);
    for (int y = 0; y < image.height(); y += stepY) {
      for (int x = 0; x < image.width(); x += stepX) {
        const QColor pixel = image.pixelColor(x, y);
        if (pixel.red() != 0 || pixel.green() != 0 || pixel.blue() != 0 || pixel.alpha() != 255) {
          return true;
        }
      }
    }
    return false;
  };
  if (controller) {
    const QImage frame = controller->captureCurrentFrameImage();
    if (hasVisiblePixels(frame)) {
      return frame;
    }
  }

  if (fallbackWidget) {
    const QImage grabbed = fallbackWidget->grab().toImage();
    if (hasVisiblePixels(grabbed)) {
      return grabbed;
    }
  }

  return QImage();
}

QImage captureScreenshotForOptions(CompositionRenderController* controller,
                                   QWidget* fallbackWidget,
                                   ScreenshotCaptureSource source)
{
  if (source == ScreenshotCaptureSource::WholeWindow && fallbackWidget) {
    return fallbackWidget->grab().toImage();
  }
  return captureCompositionScreenshot(controller, fallbackWidget);
}

bool saveScreenshotImage(const QImage& image, const QString& filePath, const QString& format, int jpegQuality)
{
  if (image.isNull() || filePath.isEmpty()) {
    return false;
  }

  ArtifactCore::ImageExportOptions options;
  options.format = format.toLower();
  options.compressionQuality = static_cast<float>(jpegQuality);
  // EXR は自動で FLOAT に昇格される (ImageExporter 内の resolveWriteType による)

  ArtifactCore::ImageExporter exporter;
  auto result = exporter.write(image, filePath, options);
  if (!result.success) {
    qWarning() << "saveScreenshotImage failed:" << result.errorStage << result.errorMessage;
  }
  return result.success;
}

QString shapeTypeDisplayName(ShapeType type) {
  switch (type) {
  case ShapeType::Rect:
    return QStringLiteral("Rect");
  case ShapeType::Ellipse:
    return QStringLiteral("Ellipse");
  case ShapeType::Star:
    return QStringLiteral("Star");
  case ShapeType::Polygon:
    return QStringLiteral("Polygon");
  case ShapeType::Line:
    return QStringLiteral("Line");
  case ShapeType::Triangle:
    return QStringLiteral("Triangle");
  case ShapeType::Square:
    return QStringLiteral("Square");
  }
  return QStringLiteral("Shape");
}

QString shapeSelectionDetail(const ArtifactCore::SharedPtr<ArtifactShapeLayer> &shape) {
  if (!shape) {
    return {};
  }

  QString detail = QStringLiteral("Shape - %1 - %2x%3")
                       .arg(shapeTypeDisplayName(shape->shapeType()))
                       .arg(shape->shapeWidth())
                       .arg(shape->shapeHeight());

  const auto type = shape->shapeType();
  if (type == ShapeType::Polygon) {
    const int pointCount = static_cast<int>(shape->customPolygonPoints().size());
    if (pointCount > 0) {
      detail += QStringLiteral(" - polygon (%1 pts)").arg(pointCount);
      detail += shape->customPolygonClosed()
                    ? QStringLiteral(", closed")
                    : QStringLiteral(", open");
    }
  } else if (shape->hasCustomPath()) {
    const auto verts = shape->customPathVertices();
    int smoothCount = 0;
    int tangentCount = 0;
    for (const auto& v : verts) {
      if (v.smooth) {
        ++smoothCount;
      }
      if (v.inTangent != QPointF(0, 0) || v.outTangent != QPointF(0, 0)) {
        ++tangentCount;
      }
    }
    detail += QStringLiteral(" - editable path (%1 verts, %2 smooth, %3 tangents)")
                  .arg(static_cast<int>(verts.size()))
                  .arg(smoothCount)
                  .arg(tangentCount);
    detail += shape->customPathClosed()
                  ? QStringLiteral(" - closed")
                  : QStringLiteral(" - open");
  } else if (type == ShapeType::Star) {
    detail += QStringLiteral(" - %1 spikes").arg(shape->starPoints());
  } else if (type == ShapeType::Rect || type == ShapeType::Square) {
    const float radius = shape->cornerRadius();
    if (radius > 0.0f) {
      detail += QStringLiteral(" - r%1").arg(radius, 0, 'f', 1);
    }
  }

  if (shape->shapeOperatorCount() > 0) {
    detail += QStringLiteral(" - %1 ops").arg(shape->shapeOperatorCount());
  }

  if (type != ShapeType::Line) {
    detail += QStringLiteral(" - vertex/segment edit ready");
  }
  return detail;
}

ArtifactCompositionPtr resolvePreferredComposition() {
  // A project with no current composition is authoritative.  ActiveContext and
  // PlaybackService can retain a composition from the previous project during
  // a project transition, but that stale pointer must not turn the empty
  // viewport state into the "no layers" state.
  ArtifactCompositionPtr projectComposition;
  if (auto *service = ArtifactProjectService::instance()) {
    projectComposition = service->currentComposition().lock();
    if (!projectComposition) {
      return {};
    }
  }

  if (auto *active = ArtifactActiveContextService::instance()) {
    if (auto comp = active->activeComposition()) {
      return comp;
    }
  }

  if (auto *playback = ArtifactPlaybackService::instance()) {
    if (auto comp = playback->currentComposition()) {
      return comp;
    }
  }

  return projectComposition;
}

bool isSvgShapeFile(const QString &path) {
  return QFileInfo(path).suffix().compare(QStringLiteral("svg"),
                                          Qt::CaseInsensitive) == 0;
}

QString resolveImportedAssetPathForSource(const QString &sourcePath,
                                          const QStringList &importedPaths) {
  if (sourcePath.isEmpty() || importedPaths.isEmpty()) {
    return sourcePath;
  }

  const QString sourceFileName = QFileInfo(sourcePath).fileName();
  for (const QString &importedPath : importedPaths) {
    if (QFileInfo(importedPath)
            .fileName()
            .compare(sourceFileName, Qt::CaseInsensitive) == 0) {
      return importedPath;
    }
  }

  if (importedPaths.size() == 1) {
    return importedPaths.first();
  }

  return sourcePath;
}

struct PendingDroppedAsset {
  QString originalPath;
  QString importedPath;
  QString layerName;
  ArtifactCore::FileType fileType = ArtifactCore::FileType::Unknown;
  bool svgShapeFile = false;
};

// Invisible input surface for the in-viewport text editing session. The
// widget owns keyboard focus and IME handling while the caret / selection /
// preedit visuals are drawn by the render controller's overlay path.
class TextEditCaretOverlay final : public QPlainTextEdit {
public:
  TextEditCaretOverlay(CompositionRenderController *controller,
                       QWidget *parent)
      : QPlainTextEdit(parent), controller_(controller) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFrameShape(QFrame::NoFrame);
    document()->setDocumentMargin(0.0);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setLineWrapMode(QPlainTextEdit::NoWrap);
    QPalette invisiblePalette = palette();
    invisiblePalette.setColor(QPalette::Base, QColor(0, 0, 0, 0));
    invisiblePalette.setColor(QPalette::Text, QColor(0, 0, 0, 0));
    invisiblePalette.setColor(QPalette::Highlight, QColor(0, 0, 0, 0));
    invisiblePalette.setColor(QPalette::HighlightedText, QColor(0, 0, 0, 0));
    invisiblePalette.setColor(QPalette::Window, QColor(0, 0, 0, 0));
    setPalette(invisiblePalette);
    viewport()->setAutoFillBackground(false);
    setFocusPolicy(Qt::StrongFocus);
  }

  void keyPressEvent(QKeyEvent *event) override {
    if (event->key() == Qt::Key_Escape) {
      if (finishRequested) {
        finishRequested(true);
      }
      event->accept();
      return;
    }
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) &&
        event->modifiers().testFlag(Qt::ControlModifier)) {
      if (finishRequested) {
        finishRequested(false);
      }
      event->accept();
      return;
    }
    QPlainTextEdit::keyPressEvent(event);
    syncToController();
  }

  void inputMethodEvent(QInputMethodEvent *event) override {
    QPlainTextEdit::inputMethodEvent(event);
    syncToController(event->preeditString());
  }

  void focusOutEvent(QFocusEvent *event) override {
    QPlainTextEdit::focusOutEvent(event);
    if (finishRequested) {
      finishRequested(false);
    }
  }

  void syncToController(const QString &preeditText = QString()) {
    if (controller_ && controller_->isTextEditSessionActive()) {
      controller_->textSessionSyncFromEditor(
          toPlainText(), textCursor().position(), textCursor().anchor(),
          preeditText);
    }
  }

  void placeCaretAt(int utf16Pos, bool extendSelection) {
    const QString currentText = toPlainText();
    QTextCursor cursor = textCursor();
    cursor.setPosition(std::clamp(utf16Pos, 0, currentText.length()),
                       extendSelection ? QTextCursor::KeepAnchor
                                       : QTextCursor::MoveAnchor);
    setTextCursor(cursor);
    syncToController();
  }

  void selectAllText() {
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
    cursor.movePosition(QTextCursor::Start, QTextCursor::KeepAnchor);
    setTextCursor(cursor);
  }

  std::function<void(bool)> finishRequested;

private:
  CompositionRenderController *controller_ = nullptr;
};

class CompositionViewport final : public QWidget {
  friend class CompositionOverlayWidget;

public:
  enum class NavigationFeedbackMode {
    None,
    Orbit,
    Pan,
    Zoom,
  };

  explicit CompositionViewport(CompositionRenderController *controller,
                               QWidget *parent = nullptr)
      : QWidget(parent), controller_(controller) {
    setMinimumSize(1, 1);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_DontCreateNativeAncestors);
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_NoSystemBackground);
    setMouseTracking(true);
    setAttribute(Qt::WA_TabletTracking, true);
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName(QStringLiteral("Composition Editor"));
    setAccessibleDescription(QStringLiteral("Edit and preview the active composition"));
    setAcceptDrops(true); // アセットブラウザからのD&Dを受け付ける

    resizeDebounceTimer_ = new QTimer(this);
    resizeDebounceTimer_->setSingleShot(true);
    QObject::connect(resizeDebounceTimer_, &QTimer::timeout, this, [this]() {
      if (!controller_ || !controller_->isInitialized()) {
        resizePending_ = false;
        return;
      }
      const QSize pendingSize =
          pendingResizeSize_.isValid() ? pendingResizeSize_ : size();
      controller_->setViewportSize(static_cast<float>(pendingSize.width()),
                                   static_cast<float>(pendingSize.height()));
      controller_->recreateSwapChain(this);
      controller_->markRenderDirty();
      resizePending_ = false;
      if (pendingInitialFit_) {
        QTimer::singleShot(50, this, [this]() { scheduleInitialFit(); });
      }
    });

    readinessTimer_ = new QTimer(this);
    readinessTimer_->setSingleShot(true);
    QObject::connect(readinessTimer_, &QTimer::timeout, this, [this]() {
      readinessScheduled_ = false;
      ensureViewportReady(pendingReadinessReason_.isEmpty()
                              ? QStringLiteral("timer")
                              : pendingReadinessReason_);
    });
  }

  void requestInitialFit() {
    pendingInitialFit_ = true;
    scheduleInitialFit();
  }

  bool syncPreferredComposition() {
    if (!controller_) {
      return false;
    }

    const auto comp = resolvePreferredComposition();
    if (!comp) {
      return false;
    }

    // Only request an initial fit when the composition actually changes.
    // syncPreferredComposition() is called on every showEvent (including
    // QADS focus cycles) — always fitting would reset the user's zoom.
    const bool compositionChanged = (controller_->composition() != comp);
    controller_->setComposition(comp);
    controller_->start();
    if (compositionChanged) {
      autoStartPending_ = true;
      requestInitialFit();
    }
    return true;
  }

  void schedulePreferredCompositionRetry() {
    const int retryCount =
        property("artifactStartupCompositionRetry").toInt();
    if (retryCount >= 20) {
      return;
    }
    setProperty("artifactStartupCompositionRetry", retryCount + 1);
    QTimer::singleShot(250, this, [this]() {
      if (!controller_ || !isVisible() || window()->isMinimized()) {
        return;
      }
      if (syncPreferredComposition()) {
        setProperty("artifactStartupCompositionRetry", 0);
        return;
      }
      schedulePreferredCompositionRetry();
    });
  }

  void scheduleViewportReadinessCheck(const QString &reason, int delayMs = 16) {
    if (!controller_) {
      return;
    }
    pendingReadinessReason_ = reason;
    if (!readinessTimer_) {
      ensureViewportReady(reason);
      return;
    }
    readinessScheduled_ = true;
    readinessTimer_->start(std::max(0, delayMs));
  }

  void scheduleViewportInitializationRetry() {
    const int retryCount =
        property("artifactStartupViewportInitRetry").toInt();
    if (retryCount >= 20) {
      return;
    }
    qInfo() << "[CompositionEditor][Startup] scheduleViewportInitializationRetry"
            << "retry=" << retryCount
            << "visible=" << isVisible()
            << "minimized=" << window()->isMinimized();
    setProperty("artifactStartupViewportInitRetry", retryCount + 1);
    QTimer::singleShot(250, this, [this]() {
      qInfo() << "[CompositionEditor][Startup] retry fired"
              << "visible=" << isVisible()
              << "minimized=" << window()->isMinimized()
              << "controllerInitialized="
              << (controller_ ? controller_->isInitialized() : false);
      if (!controller_) {
        return;
      }
      if (!isVisible() || window()->isMinimized()) {
        scheduleViewportInitializationRetry();
        return;
      }
      if (!ensureViewportReady(QStringLiteral("startup-retry"))) {
        scheduleViewportInitializationRetry();
      }
    });
  }

  bool ensureViewportReady(const QString &reason) {
    if (!controller_) {
      return false;
    }

    const bool visible = isVisible();
    const bool minimized = window() ? window()->isMinimized() : false;
    const quintptr hostWinId =
        visible ? static_cast<quintptr>(winId()) : quintptr{0};
    const QSize logicalSize(width(), height());
    const float hostDpr = devicePixelRatioF();
    const QSize physicalSize(
        static_cast<int>(std::ceil(static_cast<float>(logicalSize.width()) *
                                   hostDpr)),
        static_cast<int>(std::ceil(static_cast<float>(logicalSize.height()) *
                                   hostDpr)));

    qInfo() << "[CompositionEditor][Readiness]"
            << "reason=" << reason
            << "visible=" << visible
            << "minimized=" << minimized
            << "size=" << logicalSize
            << "physicalSize=" << physicalSize
            << "dpr=" << hostDpr
            << "winId=" << hostWinId
            << "initialized=" << controller_->isInitialized();

    if (!visible || minimized || width() <= 0 || height() <= 0 ||
        hostWinId == 0) {
      return false;
    }

    bool initializedNow = false;
    if (!controller_->isInitialized()) {
      QElapsedTimer initTimer;
      initTimer.start();
      controller_->initialize(this);
      qInfo() << "[CompositionEditor][Readiness] initialize ms="
              << initTimer.elapsed();
      if (!controller_->isInitialized()) {
        return false;
      }
      initializedNow = true;
    }

    controller_->setViewportSize(static_cast<float>(width()),
                                 static_cast<float>(height()));

    auto *renderer = controller_->renderer();
    const bool hostChanged = lastReadyHostWinId_ != hostWinId;
    const bool physicalSizeChanged = lastReadyPhysicalSize_ != physicalSize;
    const bool dprChanged = std::abs(lastReadyDpr_ - hostDpr) > 0.001f;
    const bool needsSwapChain = initializedNow || !renderer ||
                                !renderer->hasSwapChain() || hostChanged ||
                                physicalSizeChanged || dprChanged;
    if (needsSwapChain) {
      QElapsedTimer swapChainTimer;
      swapChainTimer.start();
      controller_->recreateSwapChain(this);
      qInfo() << "[CompositionEditor][Readiness] recreateSwapChain ms="
              << swapChainTimer.elapsed()
              << "hostChanged=" << hostChanged
              << "physicalSizeChanged=" << physicalSizeChanged
              << "dprChanged=" << dprChanged;
      lastReadyHostWinId_ = hostWinId;
      lastReadyPhysicalSize_ = physicalSize;
      lastReadyDpr_ = hostDpr;
    }

    if (syncPreferredComposition()) {
      setProperty("artifactStartupViewportInitRetry", 0);
      setProperty("artifactStartupCompositionRetry", 0);
    } else {
      schedulePreferredCompositionRetry();
    }

    controller_->markRenderDirty();
    return true;
  }

  void setOverlayWidget(QWidget *overlayWidget) {
    overlayWidget_ = overlayWidget;
    if (overlayWidget_) {
      overlayWidget_->setAttribute(Qt::WA_TransparentForMouseEvents);
    }
  }
  void setResizeCallback(std::function<void()> callback) {
    resizeCallback_ = std::move(callback);
  }
  void setActivatedCallback(std::function<void()> callback) {
    activatedCallback_ = std::move(callback);
  }
  void setViewportOrientationChangedCallback(
      std::function<void(const QQuaternion &)> callback) {
    viewportOrientationChangedCallback_ = std::move(callback);
  }
  bool isResizePending() const { return resizePending_; }
  QString navigationFeedbackLabel() const {
    switch (navigationFeedbackMode_) {
    case NavigationFeedbackMode::Orbit:
      return QStringLiteral("ORBIT");
    case NavigationFeedbackMode::Pan:
      return QStringLiteral("PAN");
    case NavigationFeedbackMode::Zoom:
      return QStringLiteral("ZOOM");
    case NavigationFeedbackMode::None:
      break;
    }
    return QStringLiteral("Nav: Alt+LMB Orbit | MMB Pan | Wheel Zoom");
  }
  void setOverlayVisible(bool visible) {
    if (overlayWidget_) {
      overlayWidget_->setVisible(visible);
      if (visible) {
        overlayWidget_->raise();
      }
    }
  }

  void hideViewportOverlay() {
    viewportOverlayActions_.clear();
    viewportOverlayEnabledStates_.clear();
    if (controller_) {
      controller_->hideViewportOverlay();
    }
  }

  bool triggerViewportOverlayItem(const QPointF &viewportPos) {
    if (!controller_ || !controller_->isViewportOverlayVisible()) {
      return false;
    }
    const int index = controller_->viewportOverlayItemAt(viewportPos);
    if (index < 0 || index >= static_cast<int>(viewportOverlayActions_.size())) {
      hideViewportOverlay();
      return true;
    }
    if (index >= static_cast<int>(viewportOverlayEnabledStates_.size()) ||
        !viewportOverlayEnabledStates_.at(index)) {
      return true;
    }
    auto action = viewportOverlayActions_.at(index);
    hideViewportOverlay();
    if (action) {
      action();
    }
    return true;
  }

  void showCommandPalette() {
    if (!controller_) {
      return;
    }
    const auto comp = currentComposition();
    auto *service = ArtifactProjectService::instance();
    auto *selection = ArtifactLayerSelectionManager::instance();
    const auto selectedLayers = selection ? selection->selectedLayers()
                                          : QSet<ArtifactAbstractLayerPtr>{};
    const int selectedCount = static_cast<int>(selectedLayers.size());
    QVector<ArtifactAbstractLayerPtr> orderedSelectedLayers;
    if (comp && !selectedLayers.isEmpty()) {
      for (const auto &layer : comp->allLayer()) {
        if (layer && selectedLayers.contains(layer)) {
          orderedSelectedLayers.push_back(layer);
        }
      }
    }
    QStringList items;
    QVector<std::function<void()>> actions;
    const auto add = [&](const QString &label, std::function<void()> action,
                         bool repeatable = false) {
      items.push_back(label);
      actions.push_back([this, label, action = std::move(action), repeatable]() {
        if (repeatable) {
          lastRepeatableAction_ = action;
          lastRepeatableActionLabel_ = label;
          lastRecipeDescriptor_ = QJsonObject{};
        }
        action();
        auto& config = ArtifactCore::LayeredConfigStore::instance();
        const QString usageKey = QStringLiteral("automation/commandUsage/%1")
                                     .arg(QString::number(static_cast<qulonglong>(qHash(label)), 16));
        config.setValue(usageKey, config.value(usageKey, 0).toInt() + 1);
        QStringList recent = config.value(
            QStringLiteral("automation/recentCommands")).toStringList();
        recent.removeAll(label);
        recent.prepend(label);
        while (recent.size() > 12) recent.removeLast();
        config.setValue(QStringLiteral("automation/recentCommands"), recent);
      });
    };
    if (lastRepeatableAction_) {
      add(QStringLiteral("Repeat Last Action: %1").arg(lastRepeatableActionLabel_),
          [this]() { if (lastRepeatableAction_) lastRepeatableAction_(); });
      add(QStringLiteral("Recipe: Save Last Action..."), [this]() {
        if (!lastRepeatableAction_) return;
        bool accepted = false;
        const QString name = QInputDialog::getText(
            this, QStringLiteral("Save Recipe"), QStringLiteral("Recipe name"),
            QLineEdit::Normal, lastRepeatableActionLabel_, &accepted).trimmed();
        if (!accepted || name.isEmpty()) return;
        actionRecipes_.insert(name, lastRepeatableAction_);
        if (!lastRecipeDescriptor_.isEmpty()) {
          auto& config = ArtifactCore::LayeredConfigStore::instance();
          const QByteArray stored = config.value(
              QStringLiteral("automation/parameterRecipes")).toByteArray();
          QJsonArray recipes = QJsonDocument::fromJson(stored).array();
          QJsonObject descriptor = lastRecipeDescriptor_;
          descriptor.insert(QStringLiteral("name"), name);
          bool replaced = false;
          for (int i = 0; i < recipes.size(); ++i) {
            if (recipes.at(i).toObject().value(QStringLiteral("name")).toString()
                    .compare(name, Qt::CaseInsensitive) == 0) {
              recipes[i] = descriptor;
              replaced = true;
              break;
            }
          }
          if (!replaced) recipes.append(descriptor);
          config.setValue(QStringLiteral("automation/parameterRecipes"),
                          QJsonDocument(recipes).toJson(QJsonDocument::Compact));
        }
      });
    }
    for (auto it = actionRecipes_.cbegin(); it != actionRecipes_.cend(); ++it) {
      const QString recipeName = it.key();
      const auto recipeAction = it.value();
      add(QStringLiteral("Recipe: %1").arg(recipeName), recipeAction, true);
    }
    {
      auto& config = ArtifactCore::LayeredConfigStore::instance();
      const QJsonArray recipes = QJsonDocument::fromJson(
          config.value(QStringLiteral("automation/parameterRecipes")).toByteArray()).array();
      for (const auto &value : recipes) {
        const QJsonObject descriptor = value.toObject();
        const QString name = descriptor.value(QStringLiteral("name")).toString();
        if (name.isEmpty() || actionRecipes_.contains(name)) continue;
        if (descriptor.value(QStringLiteral("actionId")).toString() ==
            QStringLiteral("batchRename")) {
          const QString baseName = descriptor.value(QStringLiteral("baseName")).toString();
          add(QStringLiteral("Recipe: %1 [Persistent]").arg(name),
              [baseName]() {
                auto *service = ArtifactProjectService::instance();
                auto *selection = ArtifactLayerSelectionManager::instance();
                const auto comp = service ? service->currentComposition().lock()
                                          : ArtifactCompositionPtr{};
                if (!service || !selection || !comp || baseName.isEmpty()) return;
                const auto selected = selection->selectedLayers();
                int index = 1;
                for (const auto &layer : comp->allLayer()) {
                  if (!layer || !selected.contains(layer) || layer->isLocked()) continue;
                  service->renameLayerInCurrentComposition(
                      layer->id(), QStringLiteral("%1 %2").arg(baseName).arg(index++));
                }
              }, true);
        }
      }
    }
    add(QStringLiteral("View: Reset View"), [this]() {
      if (controller_) controller_->resetView();
    });
    add(QStringLiteral("View: Zoom Fit"), [this]() {
      if (controller_) controller_->zoomFit();
    });
    add(QStringLiteral("View: Zoom Fit Selection"), [this]() {
      if (controller_) controller_->zoomFitSelection();
    });
    add(QStringLiteral("View: Zoom Fit Visible"), [this]() {
      if (controller_) controller_->zoomFitVisible();
    });
    add(QStringLiteral("View: Zoom Fit Work Area"), [this]() {
      if (controller_) controller_->zoomFitWorkArea();
    });
    add(QStringLiteral("View: Zoom 100%"), [this]() {
      if (controller_) controller_->zoom100();
    });
    add(QStringLiteral("Selection: Focus Selected Layer"), [this]() {
      if (controller_) controller_->focusSelectedLayer();
    });
    add(QStringLiteral("Tool: Move"), [this]() {
      if (controller_) controller_->setGizmoMode(TransformGizmo::Mode::Move);
    });
    add(QStringLiteral("Tool: Rotate"), [this]() {
      if (controller_) controller_->setGizmoMode(TransformGizmo::Mode::Rotate);
    });
    add(QStringLiteral("Tool: Scale"), [this]() {
      if (controller_) controller_->setGizmoMode(TransformGizmo::Mode::Scale);
    });
    if (comp && selection && service) {
      add(QStringLiteral("Smart Select: All Layers"), [comp, selection]() {
        selection->clearSelection();
        for (const auto &layer : comp->allLayer()) {
          if (layer) selection->addToSelection(layer);
        }
      });
      add(QStringLiteral("Smart Select: Visible Layers"),
          [comp, selection, service]() {
            selection->clearSelection();
            for (const auto &layer : comp->allLayer()) {
              if (layer && service->isLayerVisibleInCurrentComposition(layer->id())) {
                selection->addToSelection(layer);
              }
            }
          });
      add(QStringLiteral("Smart Select: Locked Layers"),
          [comp, selection, service]() {
            selection->clearSelection();
            for (const auto &layer : comp->allLayer()) {
              if (layer && service->isLayerLockedInCurrentComposition(layer->id())) {
                selection->addToSelection(layer);
              }
            }
          });
      add(QStringLiteral("Smart Select: Name Contains..."),
          [this, comp, selection]() {
            bool accepted = false;
            const QString query = QInputDialog::getText(
                this, QStringLiteral("Smart Select"),
                QStringLiteral("Layer name contains"), QLineEdit::Normal,
                QString(), &accepted).trimmed();
            if (!accepted || query.isEmpty()) return;
            selection->clearSelection();
            for (const auto &layer : comp->allLayer()) {
              if (layer && layer->layerName().contains(query, Qt::CaseInsensitive)) {
                selection->addToSelection(layer);
              }
            }
          });
      if (selectedCount > 0 && !orderedSelectedLayers.isEmpty()) {
        add(QStringLiteral("Find Similar / Select Related..."),
            [this, comp, selection, orderedSelectedLayers]() {
              const auto anchor = orderedSelectedLayers.front();
              if (!anchor) return;
              const QStringList criteria{
                  QStringLiteral("Same Layer Type"),
                  QStringLiteral("Same Source Media"),
                  QStringLiteral("Same Parent"),
                  QStringLiteral("Same Effect Set"),
                  QStringLiteral("Same Font")};
              bool accepted = false;
              const QString criterion = QInputDialog::getItem(
                  this, QStringLiteral("Find Similar"),
                  QStringLiteral("Match criterion"), criteria, 0, false,
                  &accepted);
              if (!accepted) return;
              const auto sourcePath = [](const ArtifactAbstractLayerPtr &layer) {
                if (!layer) return QString();
                const QJsonObject json = layer->toJson();
                const QStringList keys{
                    QStringLiteral("video.sourcePath"), QStringLiteral("image.sourcePath"),
                    QStringLiteral("svg.sourcePath"), QStringLiteral("audio.sourcePath"),
                    QStringLiteral("sourcePath")};
                for (const QString &key : keys) {
                  const QString path = json.value(key).toString().trimmed();
                  if (!path.isEmpty()) {
                    const QFileInfo info(path);
                    const QString resolved = info.canonicalFilePath();
                    return (resolved.isEmpty() ? info.absoluteFilePath() : resolved).toCaseFolded();
                  }
                }
                return QString();
              };
              const auto effectIds = [](const ArtifactAbstractLayerPtr &layer) {
                QSet<QString> ids;
                if (!layer) return ids;
                for (const auto &effect : layer->getEffects()) {
                  if (effect) ids.insert(effect->effectID().toQString());
                }
                return ids;
              };
              const QString anchorSource = sourcePath(anchor);
              const auto anchorParent = anchor->parentLayer();
              const QSet<QString> anchorEffects = effectIds(anchor);
              const auto anchorText = ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(anchor);
              const QString anchorFont = anchorText
                  ? anchorText->fontFamily().toQString().trimmed().toCaseFolded()
                  : QString();
              selection->clearSelection();
              int matched = 0;
              for (const auto &candidate : comp->allLayer()) {
                if (!candidate) continue;
                bool match = false;
                if (criterion == criteria.at(0)) {
                  match = candidate->type_index() == anchor->type_index();
                } else if (criterion == criteria.at(1)) {
                  match = !anchorSource.isEmpty() && sourcePath(candidate) == anchorSource;
                } else if (criterion == criteria.at(2)) {
                  const auto candidateParent = candidate->parentLayer();
                  match = (!anchorParent && !candidateParent) ||
                          (anchorParent && candidateParent &&
                           anchorParent->id() == candidateParent->id());
                } else if (criterion == criteria.at(3)) {
                  match = effectIds(candidate) == anchorEffects;
                } else if (criterion == criteria.at(4)) {
                  const auto text = ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(candidate);
                  match = text && !anchorFont.isEmpty() &&
                          text->fontFamily().toQString().trimmed().toCaseFolded() == anchorFont;
                }
                if (match) {
                  selection->addToSelection(candidate);
                  ++matched;
                }
              }
              if (matched == 0) selection->addToSelection(anchor);
              QMessageBox::information(
                  this, QStringLiteral("Find Similar"),
                  QStringLiteral("Selected %1 related layer(s) by %2.")
                      .arg(std::max(1, matched)).arg(criterion));
            });
        add(QStringLiteral("Property Link: Copy Stable Reference..."),
            [this, comp, orderedSelectedLayers]() {
              const auto layer = orderedSelectedLayers.front();
              if (!layer || !comp) return;
              QStringList labels;
              QVector<QJsonObject> references;
              for (const auto &group : layer->getLayerPropertyGroups()) {
                for (const auto &property : group.allProperties()) {
                  if (!property || property->getName().trimmed().isEmpty()) continue;
                  labels.push_back(QStringLiteral("%1 / %2")
                                       .arg(group.name(), property->getName()));
                  QJsonObject reference;
                  reference.insert(QStringLiteral("schema"),
                                   QStringLiteral("artifact.property-reference.v1"));
                  reference.insert(QStringLiteral("compositionId"), comp->id().toString());
                  reference.insert(QStringLiteral("layerId"), layer->id().toString());
                  reference.insert(QStringLiteral("propertyPath"), property->getName());
                  reference.insert(QStringLiteral("propertyType"),
                                   static_cast<int>(property->getType()));
                  references.push_back(reference);
                }
              }
              if (labels.isEmpty()) {
                QMessageBox::information(this, QStringLiteral("Property Reference"),
                                         QStringLiteral("This layer exposes no referenceable properties."));
                return;
              }
              bool accepted = false;
              const QString selected = QInputDialog::getItem(
                  this, QStringLiteral("Copy Stable Property Reference"),
                  QStringLiteral("Property"), labels, 0, false, &accepted);
              const int index = labels.indexOf(selected);
              if (!accepted || index < 0) return;
              const QByteArray encoded = QJsonDocument(references.at(index))
                                             .toJson(QJsonDocument::Compact);
              if (auto *clipboard = QGuiApplication::clipboard()) {
                clipboard->setText(QString::fromUtf8(encoded));
              }
              QMessageBox::information(
                  this, QStringLiteral("Property Reference"),
                  QStringLiteral("Copied stable reference for %1.").arg(selected));
            });
        add(QStringLiteral("Property Link: Resolve Reference from Clipboard"),
            [this, comp, selection]() {
              auto *clipboard = QGuiApplication::clipboard();
              if (!clipboard || !comp) return;
              QJsonParseError error;
              const QJsonDocument document = QJsonDocument::fromJson(
                  clipboard->text().toUtf8(), &error);
              const QJsonObject reference = document.object();
              if (error.error != QJsonParseError::NoError ||
                  reference.value(QStringLiteral("schema")).toString() !=
                      QStringLiteral("artifact.property-reference.v1")) {
                QMessageBox::warning(this, QStringLiteral("Property Reference"),
                                     QStringLiteral("Clipboard does not contain a valid property reference."));
                return;
              }
              if (reference.value(QStringLiteral("compositionId")).toString() !=
                  comp->id().toString()) {
                QMessageBox::warning(this, QStringLiteral("Property Reference"),
                                     QStringLiteral("The reference belongs to another composition."));
                return;
              }
              const auto layer = comp->layerById(
                  LayerID(reference.value(QStringLiteral("layerId")).toString()));
              const QString propertyPath =
                  reference.value(QStringLiteral("propertyPath")).toString();
              bool found = false;
              if (layer) {
                for (const auto &group : layer->getLayerPropertyGroups()) {
                  for (const auto &property : group.allProperties()) {
                    if (property && property->getName() == propertyPath &&
                        static_cast<int>(property->getType()) ==
                            reference.value(QStringLiteral("propertyType")).toInt()) {
                      found = true;
                      break;
                    }
                  }
                  if (found) break;
                }
              }
              if (!layer || !found) {
                QMessageBox::warning(this, QStringLiteral("Property Reference"),
                                     QStringLiteral("The referenced layer or property no longer exists."));
                return;
              }
              selection->clearSelection();
              selection->addToSelection(layer);
              if (controller_) controller_->setSelectedLayerId(layer->id());
              QMessageBox::information(
                  this, QStringLiteral("Property Reference"),
                  QStringLiteral("Resolved %1 on %2.")
                      .arg(propertyPath, layer->layerName()));
            });
      }
      add(QStringLiteral("QA: Inspect Active Composition"),
          [this, comp, selection]() {
            QStringList issues;
            ArtifactAbstractLayerPtr firstProblemLayer;
            QHash<QString, int> nameCounts;
            const FrameRange compRange = comp->frameRange();
            const auto layers = comp->allLayer();
            if (layers.isEmpty()) {
              issues.push_back(QStringLiteral("Composition has no layers."));
            }
            for (const auto &layer : layers) {
              if (!layer) continue;
              const QString name = layer->layerName().trimmed();
              if (name.isEmpty()) {
                issues.push_back(QStringLiteral("Unnamed layer: %1").arg(layer->id().toString()));
                if (!firstProblemLayer) firstProblemLayer = layer;
              } else {
                nameCounts[name.toCaseFolded()] += 1;
              }
              const qint64 inFrame = layer->inPoint().framePosition();
              const qint64 outFrame = layer->outPoint().framePosition();
              if (outFrame <= inFrame) {
                issues.push_back(QStringLiteral("%1 has an empty or reversed timeline range.")
                                     .arg(name.isEmpty() ? layer->id().toString() : name));
                if (!firstProblemLayer) firstProblemLayer = layer;
              } else if (outFrame <= compRange.start() || inFrame >= compRange.end()) {
                issues.push_back(QStringLiteral("%1 is entirely outside the composition range.")
                                     .arg(name.isEmpty() ? layer->id().toString() : name));
                if (!firstProblemLayer) firstProblemLayer = layer;
              }
            }
            for (auto it = nameCounts.cbegin(); it != nameCounts.cend(); ++it) {
              if (it.value() > 1) {
                issues.push_back(QStringLiteral("Duplicate layer name: %1 (%2 layers)")
                                     .arg(it.key()).arg(it.value()));
              }
            }
            if (firstProblemLayer) {
              selection->clearSelection();
              selection->addToSelection(firstProblemLayer);
              if (controller_) controller_->setSelectedLayerId(firstProblemLayer->id());
            }
            QMessageBox::information(
                this, QStringLiteral("Composition QA"),
                issues.isEmpty()
                    ? QStringLiteral("No basic composition issues found.")
                    : QStringLiteral("%1 issue(s) found:\n\n%2")
                          .arg(issues.size()).arg(issues.join(QStringLiteral("\n"))));
          });
    }
    if (comp && service && selectedCount == 1 && *selectedLayers.cbegin()) {
      const LayerID selectedId = (*selectedLayers.cbegin())->id();
      add(QStringLiteral("Selected Layer: Duplicate"), [service, selectedId]() {
        service->duplicateLayerInCurrentComposition(selectedId);
      });
      add(QStringLiteral("Selected Layer: Split at Playhead"),
          [service, comp, selectedId]() {
            service->splitLayerWithUndo(comp->id(), selectedId);
          });
    }
    if (selectedCount > 0) {
      add(QStringLiteral("Selection: Focus"), [this]() {
        if (controller_) controller_->focusSelectedLayer();
      });
      add(QStringLiteral("Batch: Rename Selected Layers..."),
          [this, service, orderedSelectedLayers]() {
            if (!service || orderedSelectedLayers.isEmpty()) return;
            bool accepted = false;
            const QString baseName = QInputDialog::getText(
                this, QStringLiteral("Batch Rename"),
                QStringLiteral("Base name"), QLineEdit::Normal,
                QStringLiteral("Layer"), &accepted).trimmed();
            if (!accepted || baseName.isEmpty()) return;
            lastRecipeDescriptor_ = QJsonObject{
                {QStringLiteral("schema"), QStringLiteral("artifact.parameter-recipe.v1")},
                {QStringLiteral("actionId"), QStringLiteral("batchRename")},
                {QStringLiteral("baseName"), baseName}};
            int index = 1;
            for (const auto &layer : orderedSelectedLayers) {
              if (!layer || service->isLayerLockedInCurrentComposition(layer->id())) {
                continue;
              }
              service->renameLayerInCurrentComposition(
                  layer->id(), QStringLiteral("%1 %2").arg(baseName).arg(index++));
            }
          }, true);
      add(QStringLiteral("Batch: Duplicate Selected Layers"),
          [service, orderedSelectedLayers]() {
            if (!service) return;
            for (const auto &layer : orderedSelectedLayers) {
              if (layer) service->duplicateLayerInCurrentComposition(layer->id());
            }
          }, true);
      add(QStringLiteral("Batch: Trim Composition to Selection"),
          [this, comp, orderedSelectedLayers]() {
            if (!comp || orderedSelectedLayers.isEmpty()) return;
            if (QMessageBox::question(
                    this, QStringLiteral("Trim Composition"),
                    QStringLiteral("Trim the composition and work area to the selected layers?"))
                != QMessageBox::Yes) return;
            qint64 minIn = std::numeric_limits<qint64>::max();
            qint64 maxOut = std::numeric_limits<qint64>::min();
            for (const auto &layer : orderedSelectedLayers) {
              if (!layer || layer->isLocked()) continue;
              minIn = std::min(minIn, layer->inPoint().framePosition());
              maxOut = std::max(maxOut, layer->outPoint().framePosition());
            }
            if (minIn == std::numeric_limits<qint64>::max() ||
                maxOut == std::numeric_limits<qint64>::min()) return;
            const FrameRange range(std::max<qint64>(0, minIn),
                                   std::max<qint64>(minIn + 1, maxOut));
            comp->setFrameRange(range);
            comp->setWorkAreaRange(range);
          }, true);
      auto &clipboard = ArtifactCore::ClipboardManager::instance();
      clipboard.syncFromSystemClipboard();
      if (clipboard.hasPropertyValue()) {
        const QString propertyPath = clipboard.pastePropertyPath();
        const QVariant propertyValue = clipboard.pastePropertyValue();
        add(QStringLiteral("Paste Special: Property Value (%1)").arg(propertyPath),
            [comp, orderedSelectedLayers, propertyPath, propertyValue]() {
              if (!comp || propertyPath.trimmed().isEmpty()) return;
              bool changed = false;
              for (const auto &layer : orderedSelectedLayers) {
                if (!layer || layer->isLocked()) continue;
                if (layer->setLayerPropertyValue(propertyPath, propertyValue)) {
                  layer->changed();
                  changed = true;
                }
              }
              if (changed) comp->changed();
            }, true);
      }
    }
    if (selectedCount > 1) {
      add(QStringLiteral("Auto Stagger..."),
          [this, comp, orderedSelectedLayers]() {
            if (!comp || orderedSelectedLayers.size() < 2) return;
            bool accepted = false;
            const QStringList modes{
                QStringLiteral("Layer Order"),
                QStringLiteral("Reverse Layer Order"),
                QStringLiteral("Center Out"),
                QStringLiteral("Deterministic Random")};
            const QString mode = QInputDialog::getItem(
                this, QStringLiteral("Auto Stagger"), QStringLiteral("Order"),
                modes, 0, false, &accepted);
            if (!accepted) return;
            const QStringList placementModes{
                QStringLiteral("Start Interval"),
                QStringLiteral("End Interval"),
                QStringLiteral("Overlap by Frames")};
            const QString placementMode = QInputDialog::getItem(
                this, QStringLiteral("Auto Stagger"),
                QStringLiteral("Placement"), placementModes, 0, false,
                &accepted);
            if (!accepted) return;
            const QStringList anchorModes{
                QStringLiteral("Earliest Selected In"),
                QStringLiteral("Current Playhead"),
                QStringLiteral("First in Applied Order")};
            const QString anchorMode = QInputDialog::getItem(
                this, QStringLiteral("Auto Stagger"), QStringLiteral("Anchor"),
                anchorModes, 0, false, &accepted);
            if (!accepted) return;
            const int step = QInputDialog::getInt(
                this, QStringLiteral("Auto Stagger"),
                placementMode == placementModes.at(2)
                    ? QStringLiteral("Overlap frames")
                    : QStringLiteral("Frame interval"),
                4, placementMode == placementModes.at(2) ? 0 : -100000,
                100000, 1,
                &accepted);
            if (!accepted) return;
            QVector<ArtifactAbstractLayerPtr> layers = orderedSelectedLayers;
            if (mode == modes.at(1)) {
              std::reverse(layers.begin(), layers.end());
            } else if (mode == modes.at(2)) {
              QVector<ArtifactAbstractLayerPtr> centerOut;
              centerOut.reserve(layers.size());
              int left = (layers.size() - 1) / 2;
              int right = left + 1;
              centerOut.push_back(layers.at(left--));
              while (left >= 0 || right < layers.size()) {
                if (right < layers.size()) centerOut.push_back(layers.at(right++));
                if (left >= 0) centerOut.push_back(layers.at(left--));
              }
              layers = std::move(centerOut);
            } else if (mode == modes.at(3)) {
              std::sort(layers.begin(), layers.end(),
                        [comp](const auto &lhs, const auto &rhs) {
                          const QString seed = comp->id().toString();
                          const uint lhsHash = qHash(seed + (lhs ? lhs->id().toString() : QString()));
                          const uint rhsHash = qHash(seed + (rhs ? rhs->id().toString() : QString()));
                          return lhsHash == rhsHash
                              ? (lhs && rhs && lhs->id().toString() < rhs->id().toString())
                              : lhsHash < rhsHash;
                        });
            }
            qint64 anchorIn = layers.front()->inPoint().framePosition();
            if (anchorMode == anchorModes.at(0)) {
              for (const auto &layer : layers) {
                if (layer) anchorIn = std::min(anchorIn, layer->inPoint().framePosition());
              }
            } else if (anchorMode == anchorModes.at(1)) {
              if (auto *playback = ArtifactPlaybackService::instance()) {
                anchorIn = playback->currentFrame().framePosition();
              }
            }
            const qint64 firstDuration = std::max<qint64>(
                1, layers.front()->outPoint().framePosition() -
                       layers.front()->inPoint().framePosition());
            const qint64 anchorOut = anchorIn + firstDuration;
            qint64 overlapCursor = anchorIn;
            int position = 0;
            QJsonArray beforeRecords;
            QJsonArray afterRecords;
            QStringList staggerPreview;
            QSet<qint64> targetStarts;
            bool targetCollision = false;
            for (const auto &layer : layers) {
              if (!layer || layer->isLocked() || layer->isTimingLocked()) continue;
              const qint64 duration = std::max<qint64>(
                  1, layer->outPoint().framePosition() -
                         layer->inPoint().framePosition());
              qint64 target = anchorIn;
              if (placementMode == placementModes.at(1)) {
                target = anchorOut + static_cast<qint64>(position) * step - duration;
              } else if (placementMode == placementModes.at(2)) {
                target = overlapCursor;
                overlapCursor += std::max<qint64>(1, duration - step);
              } else {
                target = anchorIn + static_cast<qint64>(position) * step;
              }
              ++position;
              target = std::max<qint64>(0, target);
              if (targetStarts.contains(target)) targetCollision = true;
              targetStarts.insert(target);
              const qint64 delta = target - layer->inPoint().framePosition();
              if (delta == 0) continue;
              QJsonObject before;
              before.insert(QStringLiteral("id"), layer->id().toString());
              before.insert(QStringLiteral("in"), layer->inPoint().framePosition());
              before.insert(QStringLiteral("out"), layer->outPoint().framePosition());
              before.insert(QStringLiteral("start"), layer->startTime().framePosition());
              beforeRecords.append(before);
              QJsonObject after = before;
              after.insert(QStringLiteral("in"), before.value(QStringLiteral("in")).toVariant().toLongLong() + delta);
              after.insert(QStringLiteral("out"), before.value(QStringLiteral("out")).toVariant().toLongLong() + delta);
              after.insert(QStringLiteral("start"), before.value(QStringLiteral("start")).toVariant().toLongLong() + delta);
              afterRecords.append(after);
              staggerPreview.push_back(
                  QStringLiteral("%1  %2 -> %3")
                      .arg(layer->layerName())
                      .arg(before.value(QStringLiteral("in")).toVariant().toLongLong())
                      .arg(target));
            }
            if (afterRecords.isEmpty()) {
              QMessageBox::information(this, QStringLiteral("Auto Stagger"),
                                       QStringLiteral("No layer timing changes are required."));
              return;
            }
            QString staggerPreviewText = staggerPreview.mid(0, 12).join(QStringLiteral("\n"));
            if (staggerPreview.size() > 12) {
              staggerPreviewText += QStringLiteral("\n... and %1 more")
                                        .arg(staggerPreview.size() - 12);
            }
            const QString collisionWarning = targetCollision
                ? QStringLiteral("\n\nWarning: multiple layers resolve to the same start frame.")
                : QString();
            if (QMessageBox::question(
                    this, QStringLiteral("Confirm Auto Stagger"),
                    QStringLiteral("Apply these timing changes?\n\n%1%2")
                        .arg(staggerPreviewText, collisionWarning)) != QMessageBox::Yes) {
              return;
            }
            const ArtifactCompositionWeakPtr weakComp(comp);
            const auto restore = [weakComp](const QByteArray &state) {
              const auto targetComp = weakComp.lock();
              if (!targetComp) return false;
              const QJsonArray records = QJsonDocument::fromJson(state).array();
              for (const auto &value : records) {
                const QJsonObject record = value.toObject();
                const auto layer = targetComp->layerById(
                    LayerID(record.value(QStringLiteral("id")).toString()));
                if (!layer) continue;
                layer->setTimelineWindow(
                    FramePosition(record.value(QStringLiteral("in")).toVariant().toLongLong()),
                    FramePosition(record.value(QStringLiteral("out")).toVariant().toLongLong()));
                layer->setStartTime(FramePosition(
                    record.value(QStringLiteral("start")).toVariant().toLongLong()));
                layer->changed();
              }
              targetComp->changed();
              return true;
            };
            const QByteArray beforeState =
                QJsonDocument(beforeRecords).toJson(QJsonDocument::Compact);
            const QByteArray afterState =
                QJsonDocument(afterRecords).toJson(QJsonDocument::Compact);
            bool applied = false;
            if (auto* manager = UndoManager::instance()) {
              applied = manager->push(std::make_unique<LayoutSnapshotCommand>(
                  QStringLiteral("Auto Stagger"), beforeState, afterState,
                  restore));
            } else {
              applied = restore(afterState);
            }
            if (!applied) {
              QMessageBox::warning(
                  this, QStringLiteral("Auto Stagger"),
                  QStringLiteral("The timing changes could not be applied."));
              return;
            }
          }, true);
      add(QStringLiteral("Batch: Sequence Layers End-to-End"),
          [this, comp, orderedSelectedLayers]() {
            if (!comp || orderedSelectedLayers.size() < 2) return;
            if (QMessageBox::question(
                    this, QStringLiteral("Sequence Layers"),
                    QStringLiteral("Place selected layers end-to-end in layer order?"))
                != QMessageBox::Yes) return;
            QJsonArray beforeRecords;
            for (const auto &layer : orderedSelectedLayers) {
              if (!layer || layer->isLocked()) continue;
              beforeRecords.append(QJsonObject{
                  {QStringLiteral("id"), layer->id().toString()},
                  {QStringLiteral("in"), layer->inPoint().framePosition()},
                  {QStringLiteral("out"), layer->outPoint().framePosition()},
                  {QStringLiteral("start"), layer->startTime().framePosition()}});
            }
            qint64 cursor = orderedSelectedLayers.front()->outPoint().framePosition();
            for (int i = 1; i < orderedSelectedLayers.size(); ++i) {
              const auto &layer = orderedSelectedLayers[i];
              if (!layer || layer->isLocked()) continue;
              const qint64 delta = cursor - layer->inPoint().framePosition();
              layer->slideTimingBy(delta);
              layer->changed();
              cursor = layer->outPoint().framePosition();
            }
            QJsonArray afterRecords;
            for (const auto &layer : orderedSelectedLayers) {
              if (!layer || layer->isLocked()) continue;
              afterRecords.append(QJsonObject{
                  {QStringLiteral("id"), layer->id().toString()},
                  {QStringLiteral("in"), layer->inPoint().framePosition()},
                  {QStringLiteral("out"), layer->outPoint().framePosition()},
                  {QStringLiteral("start"), layer->startTime().framePosition()}});
            }
            if (beforeRecords == afterRecords) return;
            const ArtifactCompositionWeakPtr weakComp(comp);
            const auto restore = [weakComp](const QByteArray &state) {
              const auto targetComp = weakComp.lock();
              if (!targetComp) return false;
              for (const auto &value : QJsonDocument::fromJson(state).array()) {
                const QJsonObject record = value.toObject();
                const auto layer = targetComp->layerById(
                    LayerID(record.value(QStringLiteral("id")).toString()));
                if (!layer) continue;
                layer->setTimelineWindow(
                    FramePosition(record.value(QStringLiteral("in")).toVariant().toLongLong()),
                    FramePosition(record.value(QStringLiteral("out")).toVariant().toLongLong()));
                layer->setStartTime(FramePosition(
                    record.value(QStringLiteral("start")).toVariant().toLongLong()));
                layer->changed();
              }
              targetComp->changed();
              return true;
            };
            const QByteArray beforeState =
                QJsonDocument(beforeRecords).toJson(QJsonDocument::Compact);
            const QByteArray afterState =
                QJsonDocument(afterRecords).toJson(QJsonDocument::Compact);
            bool applied = false;
            if (auto* manager = UndoManager::instance()) {
              applied = manager->push(std::make_unique<LayoutSnapshotCommand>(
                  QStringLiteral("Sequence Layers End-to-End"), beforeState,
                  afterState, restore));
            } else {
              applied = restore(afterState);
            }
            if (!applied) {
              restore(beforeState);
            }
          }, true);
      add(QStringLiteral("Batch: Match Duration to First Layer"),
          [this, comp, orderedSelectedLayers]() {
            if (!comp || orderedSelectedLayers.size() < 2 ||
                !orderedSelectedLayers.front()) return;
            if (QMessageBox::question(
                    this, QStringLiteral("Match Layer Duration"),
                    QStringLiteral("Match every selected layer to the first layer's duration?"))
                != QMessageBox::Yes) return;
            const qint64 duration = std::max<qint64>(
                1, orderedSelectedLayers.front()->outPoint().framePosition() -
                       orderedSelectedLayers.front()->inPoint().framePosition());
            QJsonArray beforeRecords;
            for (const auto &layer : orderedSelectedLayers) {
              if (!layer || layer->isLocked()) continue;
              beforeRecords.append(QJsonObject{
                  {QStringLiteral("id"), layer->id().toString()},
                  {QStringLiteral("in"), layer->inPoint().framePosition()},
                  {QStringLiteral("out"), layer->outPoint().framePosition()},
                  {QStringLiteral("start"), layer->startTime().framePosition()}});
            }
            for (int i = 1; i < orderedSelectedLayers.size(); ++i) {
              const auto &layer = orderedSelectedLayers[i];
              if (!layer || layer->isLocked()) continue;
              layer->setOutPoint(FramePosition(
                  layer->inPoint().framePosition() + duration));
              layer->changed();
            }
            QJsonArray afterRecords;
            for (const auto &layer : orderedSelectedLayers) {
              if (!layer || layer->isLocked()) continue;
              afterRecords.append(QJsonObject{
                  {QStringLiteral("id"), layer->id().toString()},
                  {QStringLiteral("in"), layer->inPoint().framePosition()},
                  {QStringLiteral("out"), layer->outPoint().framePosition()},
                  {QStringLiteral("start"), layer->startTime().framePosition()}});
            }
            if (beforeRecords == afterRecords) return;
            const ArtifactCompositionWeakPtr weakComp(comp);
            const auto restore = [weakComp](const QByteArray &state) {
              const auto targetComp = weakComp.lock();
              if (!targetComp) return false;
              for (const auto &value : QJsonDocument::fromJson(state).array()) {
                const QJsonObject record = value.toObject();
                const auto layer = targetComp->layerById(
                    LayerID(record.value(QStringLiteral("id")).toString()));
                if (!layer) continue;
                layer->setTimelineWindow(
                    FramePosition(record.value(QStringLiteral("in")).toVariant().toLongLong()),
                    FramePosition(record.value(QStringLiteral("out")).toVariant().toLongLong()));
                layer->setStartTime(FramePosition(
                    record.value(QStringLiteral("start")).toVariant().toLongLong()));
                layer->changed();
              }
              targetComp->changed();
              return true;
            };
            const QByteArray beforeState =
                QJsonDocument(beforeRecords).toJson(QJsonDocument::Compact);
            const QByteArray afterState =
                QJsonDocument(afterRecords).toJson(QJsonDocument::Compact);
            bool applied = false;
            if (auto* manager = UndoManager::instance()) {
              applied = manager->push(std::make_unique<LayoutSnapshotCommand>(
                  QStringLiteral("Match Layer Duration"), beforeState,
                  afterState, restore));
            } else {
              applied = restore(afterState);
            }
            if (!applied) {
              restore(beforeState);
            }
          }, true);
    }
    if (selectedCount > 0) {
      add(QStringLiteral("Safety: Inspect and Delete Selected Layers..."),
          [this, service, comp, selection, orderedSelectedLayers]() {
            if (!service || !comp || orderedSelectedLayers.isEmpty()) return;
            QSet<LayerID> selectedIds;
            int selectedEffectCount = 0;
            for (const auto &layer : orderedSelectedLayers) {
              if (!layer) continue;
              selectedIds.insert(layer->id());
              selectedEffectCount += layer->effectCount();
            }
            QStringList dependencies;
            for (const auto &candidate : comp->allLayer()) {
              if (!candidate || selectedIds.contains(candidate->id())) continue;
              if (selectedIds.contains(candidate->parentLayerId())) {
                dependencies.push_back(QStringLiteral("Parent: %1 depends on a selected layer")
                                           .arg(candidate->layerName()));
              }
              for (const auto &matte : candidate->matteReferences()) {
                if (!matte.enabled || matte.sourceLayerId.isNil()) continue;
                if (selectedIds.contains(matte.sourceLayerId)) {
                  dependencies.push_back(QStringLiteral("Matte: %1 uses a selected layer")
                                             .arg(candidate->layerName()));
                  break;
                }
              }
              if (const auto parametric =
                      ArtifactCore::dynamicPointerCast<ArtifactParametricCompositionLayer>(candidate)) {
                for (const auto &binding : parametric->parametricInstance().inputBindings()) {
                  if (selectedIds.contains(binding.sourceLayerId)) {
                    dependencies.push_back(
                        QStringLiteral("Published/Input Control: %1 uses a selected source layer")
                            .arg(candidate->layerName()));
                    break;
                  }
                }
              }
              for (const auto &group : candidate->getLayerPropertyGroups()) {
                for (const auto &property : group.allProperties()) {
                  if (!property || property->getExpression().trimmed().isEmpty()) continue;
                  const QString expression = property->getExpression();
                  bool referencesSelection = false;
                  for (const auto &selectedLayer : orderedSelectedLayers) {
                    if (selectedLayer &&
                        (expression.contains(selectedLayer->id().toString()) ||
                         expression.contains(selectedLayer->layerName()))) {
                      referencesSelection = true;
                      break;
                    }
                  }
                  if (referencesSelection) {
                    dependencies.push_back(
                        QStringLiteral("Expression: %1 / %2 references the selection")
                            .arg(candidate->layerName(), property->getName()));
                  }
                }
              }
            }
            QString dependencyText = dependencies.mid(0, 16).join(QStringLiteral("\n"));
            if (dependencies.size() > 16) {
              dependencyText += QStringLiteral("\n... and %1 more")
                                    .arg(dependencies.size() - 16);
            }
            if (dependencyText.isEmpty()) {
              dependencyText = QStringLiteral("No external parent, matte, or expression references found.");
            }
            const QString warning = QStringLiteral(
                "Selected layers: %1\nEffects removed with selection: %2\n\n%3\n\n"
                "Delete the selected layers? The operation will be added to Undo history.")
                    .arg(selectedIds.size()).arg(selectedEffectCount).arg(dependencyText);
            if (QMessageBox::warning(
                    this, QStringLiteral("Safe Delete Review"), warning,
                    QMessageBox::Yes | QMessageBox::Cancel,
                    QMessageBox::Cancel)
                != QMessageBox::Yes) return;
            auto macro = std::make_unique<MacroUndoCommand>(
                QStringLiteral("Safe Delete Layers"));
            int removed = 0;
            for (const auto &layer : orderedSelectedLayers) {
              if (!layer) continue;
              macro->addChild(std::make_unique<RemoveLayerCommand>(comp, layer));
              ++removed;
            }
            auto* manager = UndoManager::instance();
            bool applied = false;
            if (removed > 0) {
              if (manager) {
                applied = manager->push(std::move(macro));
              } else {
                macro->redo();
                applied = macro->lastOperationSucceeded();
              }
            }
            if (removed > 0 && !applied) {
              return;
            }
            selection->clearSelection();
            QMessageBox::information(this, QStringLiteral("Safe Delete"),
                                     QStringLiteral("Removed %1 layer(s).").arg(removed));
          });
      add(QStringLiteral("Keyframe Cleanup: Remove Redundant Keys"),
          [this, orderedSelectedLayers]() {
            auto macro = std::make_unique<MacroUndoCommand>(
                QStringLiteral("Clean Redundant Keyframes"));
            int removedTotal = 0;
            for (const auto &layer : orderedSelectedLayers) {
              if (!layer || layer->isLocked()) continue;
              for (const auto &group : layer->getLayerPropertyGroups()) {
                for (const auto &property : group.allProperties()) {
                  if (!property || !property->isAnimatable()) continue;
                  const auto before = property->getKeyFrames();
                  if (before.size() < 3) continue;
                  std::vector<ArtifactCore::KeyFrame> after;
                  after.reserve(before.size());
                  after.push_back(before.front());
                  const auto approximatelyEqual = [](const QVariant &lhs,
                                                     const QVariant &rhs) {
                    if (!lhs.isValid() || !rhs.isValid()) return lhs == rhs;
                    bool lhsNumeric = false;
                    bool rhsNumeric = false;
                    const double a = lhs.toDouble(&lhsNumeric);
                    const double b = rhs.toDouble(&rhsNumeric);
                    if (lhsNumeric && rhsNumeric) {
                      const double scale = std::max({1.0, std::abs(a), std::abs(b)});
                      return std::abs(a - b) <= 0.0001 * scale;
                    }
                    return lhs == rhs;
                  };
                  for (size_t i = 1; i + 1 < before.size(); ++i) {
                    const QVariant previous = after.back().value.isValid()
                        ? after.back().value : property->getValue();
                    const QVariant current = before[i].value.isValid()
                        ? before[i].value : property->getValue();
                    const QVariant next = before[i + 1].value.isValid()
                        ? before[i + 1].value : property->getValue();
                    if (approximatelyEqual(previous, current) &&
                        approximatelyEqual(current, next)) {
                      ++removedTotal;
                      continue;
                    }
                    after.push_back(before[i]);
                  }
                  after.push_back(before.back());
                  if (after.size() == before.size()) continue;
                  macro->addChild(std::make_unique<SetLayerPropertyKeyframesCommand>(
                      layer, property->getName(), before, after,
                      QStringLiteral("Clean %1").arg(property->getName())));
                }
              }
            }
            if (removedTotal == 0) {
              QMessageBox::information(this, QStringLiteral("Keyframe Cleanup"),
                                       QStringLiteral("No redundant keyframes were found."));
              return;
            }
            if (QMessageBox::question(
                    this, QStringLiteral("Keyframe Cleanup"),
                    QStringLiteral("Remove %1 redundant keyframe(s)?")
                        .arg(removedTotal)) != QMessageBox::Yes) return;
            auto* manager = UndoManager::instance();
            bool applied = false;
            if (manager) {
              applied = manager->push(std::move(macro));
            } else {
              macro->redo();
              applied = macro->lastOperationSucceeded();
            }
            if (!applied) {
              QMessageBox::warning(
                  this, QStringLiteral("Keyframe Cleanup"),
                  QStringLiteral("The keyframe cleanup could not be applied."));
              return;
            }
          }, true);
      add(QStringLiteral("Adaptive Text Fit..."),
          [this, comp, orderedSelectedLayers]() {
            if (!comp) return;
            QVector<ArtifactCore::SharedPtr<ArtifactTextLayer>> textLayers;
            for (const auto &layer : orderedSelectedLayers) {
              if (auto text = ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(layer);
                  text && !text->isLocked()) {
                textLayers.push_back(text);
              }
            }
            if (textLayers.isEmpty()) {
              QMessageBox::information(this, QStringLiteral("Adaptive Text Fit"),
                                       QStringLiteral("No editable text layers are selected."));
              return;
            }
            bool accepted = false;
            const double minimumSize = QInputDialog::getDouble(
                this, QStringLiteral("Adaptive Text Fit"),
                QStringLiteral("Minimum font size"), 12.0, 1.0, 512.0, 1,
                &accepted);
            if (!accepted) return;
            const QSize compSize = comp->effectiveCompositionSize();
            QJsonArray beforeRecords;
            QJsonArray afterRecords;
            QStringList preview;
            for (const auto &text : textLayers) {
              const QString content = text->text().toQString();
              if (content.isEmpty()) continue;
              const qreal targetWidth = text->maxWidth() > 0.0f
                  ? text->maxWidth() : std::max(1.0, compSize.width() * 0.8);
              const qreal targetHeight = text->boxHeight() > 0.0f
                  ? text->boxHeight() : std::max(1.0, compSize.height() * 0.8);
              const double oldSize = text->fontSize();
              double fittedSize = oldSize;
              while (fittedSize > minimumSize) {
                QFont font(text->fontFamily().toQString());
                font.setPointSizeF(fittedSize);
                const QFontMetricsF metrics(font);
                const QRectF measured = metrics.boundingRect(
                    QRectF(0.0, 0.0, targetWidth, targetHeight * 4.0),
                    Qt::TextWordWrap, content);
                if (measured.width() <= targetWidth &&
                    measured.height() <= targetHeight) break;
                fittedSize = std::max(minimumSize, fittedSize - 1.0);
              }
              if (std::abs(fittedSize - oldSize) <= 0.001) continue;
              QJsonObject before;
              before.insert(QStringLiteral("id"), text->id().toString());
              before.insert(QStringLiteral("size"), oldSize);
              beforeRecords.append(before);
              QJsonObject after = before;
              after.insert(QStringLiteral("size"), fittedSize);
              afterRecords.append(after);
              preview.push_back(QStringLiteral("%1  %2 -> %3 pt")
                                    .arg(text->layerName())
                                    .arg(oldSize, 0, 'f', 1)
                                    .arg(fittedSize, 0, 'f', 1));
            }
            if (afterRecords.isEmpty()) {
              QMessageBox::information(this, QStringLiteral("Adaptive Text Fit"),
                                       QStringLiteral("All selected text already fits."));
              return;
            }
            if (QMessageBox::question(
                    this, QStringLiteral("Adaptive Text Fit"),
                    QStringLiteral("Apply font-size fitting?\n\n%1")
                        .arg(preview.mid(0, 12).join(QStringLiteral("\n"))))
                != QMessageBox::Yes) return;
            const ArtifactCompositionWeakPtr weakComp(comp);
            const auto restore = [weakComp](const QByteArray &state) {
              const auto targetComp = weakComp.lock();
              if (!targetComp) return false;
              const QJsonArray records = QJsonDocument::fromJson(state).array();
              for (const auto &value : records) {
                const QJsonObject record = value.toObject();
                const auto text = ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(
                    targetComp->layerById(LayerID(record.value(QStringLiteral("id")).toString())));
                if (!text) continue;
                text->setFontSize(static_cast<float>(
                    record.value(QStringLiteral("size")).toDouble()));
                text->changed();
              }
              targetComp->changed();
              return true;
            };
            const QByteArray beforeState =
                QJsonDocument(beforeRecords).toJson(QJsonDocument::Compact);
            const QByteArray afterState =
                QJsonDocument(afterRecords).toJson(QJsonDocument::Compact);
            bool applied = false;
            if (auto* manager = UndoManager::instance()) {
              applied = manager->push(std::make_unique<LayoutSnapshotCommand>(
                  QStringLiteral("Adaptive Text Fit"), beforeState, afterState,
                  restore));
            } else {
              applied = restore(afterState);
            }
            if (!applied) {
              QMessageBox::warning(
                  this, QStringLiteral("Adaptive Text Fit"),
                  QStringLiteral("The font-size changes could not be applied."));
              return;
            }
          }, true);
      add(QStringLiteral("Quick Replace Selected Sources..."),
          [this, service, comp, orderedSelectedLayers]() {
            if (!service || !comp || orderedSelectedLayers.isEmpty()) return;
            QString currentPath;
            const QJsonObject firstJson = orderedSelectedLayers.front()->toJson();
            const QStringList sourceKeys{
                QStringLiteral("video.sourcePath"),
                QStringLiteral("image.sourcePath"),
                QStringLiteral("svg.sourcePath"),
                QStringLiteral("audio.sourcePath"),
                QStringLiteral("sourcePath")};
            for (const QString &key : sourceKeys) {
              currentPath = firstJson.value(key).toString().trimmed();
              if (!currentPath.isEmpty()) break;
            }
            const QStringList paths = QFileDialog::getOpenFileNames(
                this, QStringLiteral("Quick Replace Selected Sources"),
                currentPath.isEmpty() ? QString()
                                      : QFileInfo(currentPath).absolutePath(),
                QStringLiteral("Media Files (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.webp *.exr *.svg *.mp4 *.mov *.mkv *.avi *.webm *.m4v *.mpg *.mpeg *.mxf *.gif *.wav *.mp3 *.aac *.flac *.ogg);;All Files (*.*)"));
            if (paths.isEmpty()) return;
            QString mappingMode = QStringLiteral("One Source for All");
            if (paths.size() > 1) {
              const QStringList mappingModes{
                  QStringLiteral("Layer Order"),
                  QStringLiteral("Match Layer Name to Filename")};
              bool mappingAccepted = false;
              mappingMode = QInputDialog::getItem(
                  this, QStringLiteral("Quick Replace"),
                  QStringLiteral("File assignment"), mappingModes, 0, false,
                  &mappingAccepted);
              if (!mappingAccepted) return;
            }
            int replaced = 0;
            int skipped = 0;
            QJsonArray beforeRecords;
            QJsonArray afterRecords;
            QSet<QString> assignedPaths;
            QStringList assignmentPreview;
            const auto normalizedMatchName = [](const QString &text) {
              QString normalized;
              const QString folded = text.toCaseFolded();
              normalized.reserve(folded.size());
              for (const QChar ch : folded) {
                if (ch.isLetterOrNumber()) normalized.append(ch);
              }
              return normalized;
            };
            for (int i = 0; i < orderedSelectedLayers.size(); ++i) {
              const auto &layer = orderedSelectedLayers.at(i);
              if (!layer || layer->isLocked()) {
                ++skipped;
                continue;
              }
              QString path;
              if (paths.size() == 1) {
                path = paths.front();
              } else if (mappingMode == QStringLiteral("Layer Order")) {
                if (i < paths.size()) path = paths.at(i);
              } else {
                const QString layerName = layer->layerName().trimmed();
                const QString normalizedLayerName = normalizedMatchName(layerName);
                for (const QString &candidate : paths) {
                  const QString fileName = QFileInfo(candidate).completeBaseName().trimmed();
                  const QString normalizedFileName = normalizedMatchName(fileName);
                  if (!assignedPaths.contains(candidate) &&
                      !normalizedLayerName.isEmpty() && !normalizedFileName.isEmpty() &&
                      (normalizedLayerName == normalizedFileName ||
                       normalizedLayerName.contains(normalizedFileName) ||
                       normalizedFileName.contains(normalizedLayerName))) {
                    path = candidate;
                    break;
                  }
                }
              }
              if (path.isEmpty()) {
                ++skipped;
                continue;
              }
              const QJsonObject layerJson = layer->toJson();
              QString sourceKey;
              QString oldPath;
              for (const QString &key : sourceKeys) {
                if (!layerJson.contains(key)) continue;
                sourceKey = key;
                oldPath = layerJson.value(key).toString();
                break;
              }
              if (sourceKey.isEmpty()) {
                ++skipped;
                continue;
              }
              const QString suffix = QFileInfo(path).suffix().toLower();
              const QSet<QString> imageExtensions{
                  QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
                  QStringLiteral("bmp"), QStringLiteral("tif"), QStringLiteral("tiff"),
                  QStringLiteral("webp"), QStringLiteral("exr"), QStringLiteral("gif")};
              const QSet<QString> videoExtensions{
                  QStringLiteral("mp4"), QStringLiteral("mov"), QStringLiteral("mkv"),
                  QStringLiteral("avi"), QStringLiteral("webm"), QStringLiteral("m4v"),
                  QStringLiteral("mpg"), QStringLiteral("mpeg"), QStringLiteral("mxf"),
                  QStringLiteral("gif")};
              const QSet<QString> audioExtensions{
                  QStringLiteral("wav"), QStringLiteral("mp3"), QStringLiteral("aac"),
                  QStringLiteral("flac"), QStringLiteral("ogg")};
              const bool compatible =
                  (sourceKey.startsWith(QStringLiteral("image.")) && imageExtensions.contains(suffix)) ||
                  (sourceKey.startsWith(QStringLiteral("video.")) && videoExtensions.contains(suffix)) ||
                  (sourceKey.startsWith(QStringLiteral("audio.")) && audioExtensions.contains(suffix)) ||
                  (sourceKey.startsWith(QStringLiteral("svg.")) && suffix == QStringLiteral("svg")) ||
                  (sourceKey == QStringLiteral("sourcePath"));
              if (!compatible) {
                ++skipped;
                continue;
              }
              if (paths.size() > 1 &&
                  mappingMode == QStringLiteral("Match Layer Name to Filename")) {
                assignedPaths.insert(path);
              }
              QJsonObject before;
              before.insert(QStringLiteral("id"), layer->id().toString());
              before.insert(QStringLiteral("key"), sourceKey);
              before.insert(QStringLiteral("path"), oldPath);
              beforeRecords.append(before);
              QJsonObject after = before;
              after.insert(QStringLiteral("path"), path);
              afterRecords.append(after);
              assignmentPreview.push_back(
                  QStringLiteral("%1  <-  %2")
                      .arg(layer->layerName(), QFileInfo(path).fileName()));
              ++replaced;
            }
            const ArtifactCompositionWeakPtr weakComp(comp);
            const auto restore = [weakComp](const QByteArray &state) {
              const auto targetComp = weakComp.lock();
              if (!targetComp) return false;
              bool allSucceeded = true;
              const QJsonArray records = QJsonDocument::fromJson(state).array();
              for (const auto &value : records) {
                const QJsonObject record = value.toObject();
                const auto layer = targetComp->layerById(
                    LayerID(record.value(QStringLiteral("id")).toString()));
                if (!layer || !layer->setLayerPropertyValue(
                                  record.value(QStringLiteral("key")).toString(),
                                  record.value(QStringLiteral("path")).toString())) {
                  allSucceeded = false;
                  continue;
                }
                layer->changed();
              }
              targetComp->changed();
              return allSucceeded;
            };
            if (!afterRecords.isEmpty()) {
              QString previewText = assignmentPreview.mid(0, 12).join(QStringLiteral("\n"));
              if (assignmentPreview.size() > 12) {
                previewText += QStringLiteral("\n... and %1 more")
                                   .arg(assignmentPreview.size() - 12);
              }
              if (QMessageBox::question(
                      this, QStringLiteral("Confirm Quick Replace"),
                      QStringLiteral("Apply these source replacements?\n\n%1")
                          .arg(previewText)) != QMessageBox::Yes) {
                return;
              }
              const QByteArray beforeState =
                  QJsonDocument(beforeRecords).toJson(QJsonDocument::Compact);
              const QByteArray afterState =
                  QJsonDocument(afterRecords).toJson(QJsonDocument::Compact);
              bool applied = false;
              if (auto* manager = UndoManager::instance()) {
                applied = manager->push(std::make_unique<LayoutSnapshotCommand>(
                    QStringLiteral("Quick Replace Sources"), beforeState,
                    afterState, restore));
              } else {
                applied = restore(afterState);
              }
              if (!applied) {
                QMessageBox::warning(
                    this, QStringLiteral("Quick Replace"),
                    QStringLiteral("The source replacements could not be applied."));
                return;
              }
            }
            QMessageBox::information(
                this, QStringLiteral("Quick Replace"),
                QStringLiteral("Attempted replacement for %1 layer(s). Skipped %2 layer(s).\n"
                               "%3\n"
                               "Transform, timing, masks, effects, and parenting were preserved.")
                    .arg(replaced).arg(skipped)
                    .arg(paths.size() == 1
                             ? QStringLiteral("One source was applied to every compatible layer.")
                             : mappingMode == QStringLiteral("Layer Order")
                                   ? QStringLiteral("Sources were assigned in layer order.")
                                   : QStringLiteral("Sources were matched by layer and filename.")));
          }, true);
      add(QStringLiteral("Auto Precompose Package"),
          [this, service, comp, orderedSelectedLayers]() {
            if (!service || !comp || orderedSelectedLayers.isEmpty()) return;
            bool accepted = false;
            const QString defaultName = orderedSelectedLayers.front()
                ? orderedSelectedLayers.front()->layerName() + QStringLiteral(" Package")
                : QStringLiteral("Precomp Package");
            const QString name = QInputDialog::getText(
                this, QStringLiteral("Auto Precompose Package"),
                QStringLiteral("Composition name"), QLineEdit::Normal,
                defaultName, &accepted).trimmed();
            if (!accepted || name.isEmpty()) return;
            QVector<LayerID> ids;
            for (const auto &layer : orderedSelectedLayers) {
              if (layer && !layer->isLocked()) ids.push_back(layer->id());
            }
            if (ids.isEmpty() || !service->precomposeLayersWithUndo(
                    ids, UniString(name), false, true,
                    PrecomposeMode::MoveSelected)) {
              QMessageBox::warning(this, QStringLiteral("Auto Precompose Package"),
                                   QStringLiteral("Precompose failed."));
            }
          }, true);
    }
    if (selectedCount == 1 && !orderedSelectedLayers.isEmpty()) {
      const auto parametricLayer = ArtifactCore::dynamicPointerCast<ArtifactParametricCompositionLayer>(
          orderedSelectedLayers.front());
      if (parametricLayer && parametricLayer->definition() &&
          !parametricLayer->definition()->publishedControls().isEmpty()) {
        add(QStringLiteral("Published Controls: Edit Override..."),
            [this, parametricLayer]() {
              const auto definition = parametricLayer->definition();
              if (!definition) return;
              QStringList labels;
              for (const auto &control : definition->publishedControls()) {
                labels.push_back(control.displayName.trimmed().isEmpty()
                                     ? control.controlId : control.displayName);
              }
              bool accepted = false;
              const QString selected = QInputDialog::getItem(
                  this, QStringLiteral("Published Controls"),
                  QStringLiteral("Control"), labels, 0, false, &accepted);
              const int index = labels.indexOf(selected);
              if (!accepted || index < 0) return;
              const auto control = definition->publishedControls().at(index);
              const QString valueText = QInputDialog::getText(
                  this, QStringLiteral("Published Controls"),
                  selected, QLineEdit::Normal,
                  control.defaultValue.toString(), &accepted);
              if (!accepted) return;
              QVariant value = valueText;
              if (control.defaultValue.metaType().id() == QMetaType::Bool) {
                value = QVariant(valueText.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0 ||
                                 valueText == QStringLiteral("1"));
              } else if (control.defaultValue.canConvert<double>()) {
                bool numeric = false;
                const double number = valueText.toDouble(&numeric);
                if (numeric) value = number;
              }
              parametricLayer->setPublishedControlOverride(control.controlId, value);
              parametricLayer->changed();
            }, true);
      }
    }
    if (comp && !comp->responsiveLayoutVariants().isEmpty()) {
      add(QStringLiteral("Responsive Preview Matrix..."), [this, comp]() {
        const auto variants = comp->responsiveLayoutVariants();
        QStringList labels;
        for (const auto &variant : variants) {
          labels.push_back(QStringLiteral("%1 — %2x%3")
                               .arg(variant.displayName.trimmed().isEmpty()
                                        ? variant.variantId : variant.displayName)
                               .arg(variant.baseSize.width())
                               .arg(variant.baseSize.height()));
        }
        bool accepted = false;
        const QString selected = QInputDialog::getItem(
            this, QStringLiteral("Responsive Preview Matrix"),
            QStringLiteral("Preview variant"), labels, 0, false, &accepted);
        const int index = labels.indexOf(selected);
        if (!accepted || index < 0) return;
        comp->setActiveResponsiveLayoutVariantId(variants.at(index).variantId);
      }, true);
    }
    const QStringList pinnableCommands = items;
    add(QStringLiteral("Palette: Pin or Unpin Command..."),
        [this, pinnableCommands]() {
          if (pinnableCommands.isEmpty()) return;
          auto& config = ArtifactCore::LayeredConfigStore::instance();
          QStringList favorites = config.value(
              QStringLiteral("automation/favoriteCommands")).toStringList();
          QStringList choices;
          choices.reserve(pinnableCommands.size());
          for (const QString &command : pinnableCommands) {
            choices.push_back(QStringLiteral("%1 %2")
                                  .arg(favorites.contains(command)
                                           ? QStringLiteral("[Pinned]")
                                           : QStringLiteral("[ ]"),
                                       command));
          }
          bool accepted = false;
          const QString selected = QInputDialog::getItem(
              this, QStringLiteral("Command Palette Favorites"),
              QStringLiteral("Toggle command"), choices, 0, false, &accepted);
          const int index = choices.indexOf(selected);
          if (!accepted || index < 0) return;
          const QString command = pinnableCommands.at(index);
          if (favorites.contains(command)) favorites.removeAll(command);
          else favorites.prepend(command);
          config.setValue(QStringLiteral("automation/favoriteCommands"), favorites);
        });
    add(QStringLiteral("Palette: Reset Usage Ranking"), [this]() {
      if (QMessageBox::question(
              this, QStringLiteral("Reset Command Ranking"),
              QStringLiteral("Clear command usage and recent-command history?"))
          != QMessageBox::Yes) return;
      auto& config = ArtifactCore::LayeredConfigStore::instance();
      for (const auto& key : config.allKeys()) {
        if (key.startsWith(QStringLiteral("automation/commandUsage/"))) {
          config.removeValue(ArtifactCore::ConfigLayer::Project, key.toStdString());
          config.removeValue(ArtifactCore::ConfigLayer::User, key.toStdString());
        }
      }
      config.removeValue(ArtifactCore::ConfigLayer::Project,
                         "automation/recentCommands");
      config.removeValue(ArtifactCore::ConfigLayer::User,
                         "automation/recentCommands");
    });

    QVector<int> rankedIndices;
    rankedIndices.reserve(items.size());
    for (int i = 0; i < items.size(); ++i) rankedIndices.push_back(i);
    auto& usageSettings = ArtifactCore::LayeredConfigStore::instance();
    const QStringList recentCommands = usageSettings.value(
        QStringLiteral("automation/recentCommands")).toStringList();
    const QStringList favoriteCommands = usageSettings.value(
        QStringLiteral("automation/favoriteCommands")).toStringList();
    const auto commandScore = [&](const QString &label) {
      const QString usageKey = QStringLiteral("automation/commandUsage/%1")
                                   .arg(QString::number(static_cast<qulonglong>(qHash(label)), 16));
      const int usage = usageSettings.value(usageKey, 0).toInt();
      const int recentIndex = recentCommands.indexOf(label);
      const int recentBoost = recentIndex >= 0 ? std::max(0, 12 - recentIndex) : 0;
      const int favoriteBoost = favoriteCommands.contains(label) ? 1000000 : 0;
      return favoriteBoost + usage * 100 + recentBoost;
    };
    std::stable_sort(rankedIndices.begin(), rankedIndices.end(),
                     [&](int lhs, int rhs) {
                       return commandScore(items.at(lhs)) > commandScore(items.at(rhs));
                     });
    QStringList rankedItems;
    QVector<std::function<void()>> rankedActions;
    rankedItems.reserve(items.size());
    rankedActions.reserve(actions.size());
    for (const int index : rankedIndices) {
      rankedItems.push_back(items.at(index));
      rankedActions.push_back(actions.at(index));
    }
    viewportOverlayActions_ = rankedActions;
    controller_->showCommandPaletteOverlay(QString(), rankedItems);
  }

  void showViewportContextMenu(const QPointF &viewportPos) {
    if (!controller_) {
      return;
    }
    QStringList items;
    QVector<std::function<void()>> actions;
    QVector<bool> enabledStates;
    QString title;
    QString subtitle;
    const auto add = [&](const QString &label, std::function<void()> action,
                         bool enabled = true) {
      items.push_back(label);
      actions.push_back(std::move(action));
      enabledStates.push_back(enabled);
    };
    const auto addSeparator = [&]() {
      if (!items.isEmpty() && !items.last().trimmed().isEmpty()) {
        items.push_back(QString());
        actions.push_back([]() {});
        enabledStates.push_back(false);
      }
    };

    const LayerID layerId = controller_->layerAtViewportPos(viewportPos);
    const auto comp = currentComposition();
    const auto layer =
        (!layerId.isNil() && comp) ? comp->layerById(layerId)
                                   : ArtifactAbstractLayerPtr{};
    auto *svc = ArtifactProjectService::instance();
    auto *selection = ArtifactLayerSelectionManager::instance();
    const int selectedCount =
        selection ? static_cast<int>(selection->selectedLayers().size()) : 0;
    const bool clipboardHasLayerData =
        ArtifactCore::ClipboardManager::instance().hasLayerData();
    const auto *toolManager = ArtifactApplicationManager::instance()
                                  ? ArtifactApplicationManager::instance()->toolManager()
                                  : nullptr;
    const bool eraserToolActive = toolManager &&
                                  toolManager->activeTool() == ToolType::Eraser;
    const auto paintLayer = layer
                                ? ArtifactCore::dynamicPointerCast<ArtifactPaintLayer>(layer)
                                : ArtifactCore::SharedPtr<ArtifactPaintLayer>{};
    if (layer && comp && ArtifactRenderQueueService::instance()) {
      addSeparator();
      const auto addLayerRenderQueueAction =
          [this, layer, comp, &add](const QString &label, const int frameRangeMode) {
        add(label, [this, layer, comp, frameRangeMode]() {
            auto *renderQueue = ArtifactRenderQueueService::instance();
            if (!renderQueue || !layer || !comp) {
              return;
            }
            renderQueue->addRenderQueueForComposition(
                comp->id(), comp->settings().compositionName().toQString());
            const int jobIndex = renderQueue->jobCount() - 1;
            if (jobIndex < 0) {
              return;
            }
            QVariantMap selective = renderQueue->jobSelectiveSettingsAt(jobIndex);
            selective.insert(QStringLiteral("layerFilterMode"), 4);
            selective.insert(QStringLiteral("layerWhitelist"),
                             QStringList{layer->id().toString()});
            selective.insert(QStringLiteral("layerBlacklist"), QStringList{});
            selective.insert(QStringLiteral("frameRangeMode"), frameRangeMode);
            if (frameRangeMode == 4) {
              const int frame = static_cast<int>(comp->framePosition().framePosition());
              renderQueue->setJobFrameRangeAt(jobIndex, frame, frame);
            } else if (frameRangeMode == 1) {
              const auto range = comp->workAreaRange();
              renderQueue->setJobFrameRangeAt(
                  jobIndex, static_cast<int>(range.start()),
                  static_cast<int>(range.end()));
            }
            renderQueue->setJobSelectiveSettingsAt(jobIndex, selective);
            if (controller_) {
              controller_->setInfoOverlayText(
                  QStringLiteral("Render Queue"),
                  QStringLiteral("Selected layer queued for rendering"));
            }
          });
      };
      addLayerRenderQueueAction(QStringLiteral("このレイヤーだけをレンダーキューに追加"), 0);
      addLayerRenderQueueAction(QStringLiteral("このレイヤーを現在フレームだけレンダー"), 4);
      addLayerRenderQueueAction(QStringLiteral("このレイヤーをワークエリアだけレンダー"), 1);
    }
    if (eraserToolActive && paintLayer) {
      addSeparator();
      add(QStringLiteral("Paintレイヤー全体を消去…"),
          [this, paintLayer, comp]() {
            QWidget *parent = this;
            const auto answer = QMessageBox::question(
                parent, QStringLiteral("Paintレイヤーを消去"),
                QStringLiteral("このPaintレイヤーの全フレームを消去しますか？"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (answer != QMessageBox::Yes || !paintLayer) {
              return;
            }
            paintLayer->clearAllFrames();
            if (comp) {
              ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
                  LayerChangedEvent{comp->id().toString(), paintLayer->id().toString(),
                                    LayerChangedEvent::ChangeType::Modified});
            }
            if (controller_) {
              controller_->markRenderDirty();
              controller_->setInfoOverlayText(
                  QStringLiteral("Eraser"),
                  QStringLiteral("All Paint frames cleared"));
            }
          });
    }
    const auto rigLayer = layer
                              ? dynamic_cast<ArtifactAbstract2DLayer *>(layer.get())
                              : nullptr;
    const bool hasRig = rigLayer &&
                        (rigLayer->rig2D().rootBone() != nullptr ||
                         !rigLayer->rig2D().controls().isEmpty() ||
                         rigLayer->rig2D().skinMesh() != nullptr);
    if (hasRig) {
      addSeparator();
      add(QStringLiteral("Rig Overlay: %1")
              .arg(controller_->isShowRigOverlay()
                       ? QStringLiteral("Hide")
                       : QStringLiteral("Show")),
          [this]() {
            if (!controller_) {
              return;
            }
            controller_->setShowRigOverlay(!controller_->isShowRigOverlay());
            controller_->setInfoOverlayText(
                QStringLiteral("Rig Overlay"),
                controller_->isShowRigOverlay()
                    ? QStringLiteral("Bones, controls and mesh overlay enabled")
                    : QStringLiteral("Rig overlay hidden"));
          });
      add(QStringLiteral("Normalize Rig Weights"),
          [this]() {
            if (controller_ && controller_->normalizeRigWeights()) {
              controller_->setInfoOverlayText(
                  QStringLiteral("Rig Weights"),
                  QStringLiteral("All vertex weights normalized"));
            }
          },
          rigLayer->rig2D().skinMesh() != nullptr);
      add(QStringLiteral("Smooth Rig Weights"),
          [this]() {
            if (controller_ && controller_->smoothRigWeights()) {
              controller_->setInfoOverlayText(
                  QStringLiteral("Rig Weights"),
                  QStringLiteral("Selected bone weights smoothed"));
            }
          },
          rigLayer->rig2D().skinMesh() != nullptr);
      add(QStringLiteral("Mirror Rig Weights"),
          [this]() {
            if (controller_ && controller_->mirrorRigWeights()) {
              controller_->setInfoOverlayText(
                  QStringLiteral("Rig Weights"),
                  QStringLiteral("Selected bone weights mirrored"));
            }
          },
          rigLayer->rig2D().skinMesh() != nullptr);
      add(QStringLiteral("Capture Rig Pose"),
          [this]() {
            if (controller_ && controller_->captureRigPose()) {
              controller_->setInfoOverlayText(
                  QStringLiteral("Rig Pose"),
                  QStringLiteral("Current pose captured to clipboard"));
            }
          },
          rigLayer->rig2D().rootBone() != nullptr ||
              !rigLayer->rig2D().controls().isEmpty());
      add(QStringLiteral("Apply Captured Rig Pose (50%)"),
          [this]() {
            if (controller_ && controller_->applyCapturedRigPose(0.5f)) {
              controller_->setInfoOverlayText(
                  QStringLiteral("Rig Pose"),
                  QStringLiteral("Captured pose blended at 50%"));
            }
          },
          rigLayer->rig2D().rootBone() != nullptr ||
              !rigLayer->rig2D().controls().isEmpty());
      add(QStringLiteral("Save Rig Pose Slot 1"),
          [this]() {
            if (controller_ && controller_->saveRigPoseSlot(1)) {
              controller_->setInfoOverlayText(
                  QStringLiteral("Rig Pose"),
                  QStringLiteral("Pose saved to slot 1"));
            }
          },
          rigLayer->rig2D().rootBone() != nullptr ||
              !rigLayer->rig2D().controls().isEmpty());
      add(QStringLiteral("Apply Rig Pose Slot 1 (50%)"),
          [this]() {
            if (controller_ && controller_->applyRigPoseSlot(1, 0.5f)) {
              controller_->setInfoOverlayText(
                  QStringLiteral("Rig Pose"),
                  QStringLiteral("Slot 1 blended at 50%"));
            }
          },
          rigLayer->rig2D().rootBone() != nullptr ||
              !rigLayer->rig2D().controls().isEmpty());
      add(QStringLiteral("Clear Rig Pose Slots"),
          [this]() {
            if (controller_) {
              controller_->clearRigPoseSlots();
              controller_->setInfoOverlayText(
                  QStringLiteral("Rig Pose"),
                  QStringLiteral("Pose slots and clipboard cleared"));
            }
          });
    }
    add(QStringLiteral("Place Work Cursor Here"),
        [this, viewportPos]() {
          if (controller_) {
            controller_->placeWorkCursorAtViewportPos(viewportPos);
            controller_->setWorkCursorLabel(
                QStringLiteral("Placed in %1").arg(QStringLiteral("Active View")));
            controller_->setInfoOverlayText(
                QStringLiteral("Work Cursor"),
                QStringLiteral("Placed at the active viewport position"));
          }
        });
    add(QStringLiteral("Center Work Cursor"),
        [this, comp]() {
          if (!controller_ || !comp) {
            return;
          }
          const QSize size = comp->settings().compositionSize();
          controller_->setWorkCursorCanvasPosition(
              QPointF(size.width() * 0.5, size.height() * 0.5));
          controller_->setWorkCursorLabel(
              QStringLiteral("Centered in %1").arg(QStringLiteral("Active View")));
          controller_->setInfoOverlayText(
              QStringLiteral("Work Cursor"),
              QStringLiteral("Centered in the composition"));
        },
        comp != nullptr);
    add(QStringLiteral("Cursor to Selection"),
        [this]() {
          if (controller_ && controller_->moveWorkCursorToSelection()) {
            controller_->setWorkCursorLabel(QStringLiteral("Selection"));
            controller_->setInfoOverlayText(
                QStringLiteral("3D Cursor"),
                QStringLiteral("Moved to selection center"));
          }
        },
        selectedCount > 0);
    add(QStringLiteral("Cursor to World Origin"),
        [this]() {
          if (controller_) {
            controller_->moveWorkCursorToWorldOrigin();
            controller_->setWorkCursorLabel(QStringLiteral("World Origin"));
            controller_->setInfoOverlayText(
                QStringLiteral("3D Cursor"),
                QStringLiteral("Moved to world origin"));
          }
        });
    add(QStringLiteral("Clear Work Cursor"),
        [this]() {
          if (controller_) {
            controller_->clearWorkCursor();
            controller_->clearInfoOverlayText();
          }
        },
        controller_->isWorkCursorVisible());
    addSeparator();
    const auto describeLayerMenuTitle = [&](const ArtifactAbstractLayerPtr &targetLayer) {
      if (!targetLayer) {
        return QStringLiteral("Layer");
      }
      QString typeText = QStringLiteral("Layer");
      const QString className = targetLayer->className().toQString();
      if (targetLayer->isNullLayer() ||
          className.contains(QStringLiteral("Null"), Qt::CaseInsensitive)) {
        typeText = QStringLiteral("Null Layer");
      } else if (targetLayer->isAdjustmentLayer() ||
                 className.contains(QStringLiteral("Adjust"), Qt::CaseInsensitive)) {
        typeText = QStringLiteral("Adjustment Layer");
      } else if (targetLayer->isGroupLayer() ||
                 className.contains(QStringLiteral("Group"), Qt::CaseInsensitive)) {
        typeText = targetLayer->hasExclusiveChildSelection()
            ? QStringLiteral("Multiplexer Group")
            : QStringLiteral("Group Layer");
      } else if (targetLayer->isCloneLayer() ||
                 className.contains(QStringLiteral("Clone"), Qt::CaseInsensitive)) {
        typeText = QStringLiteral("Clone Layer");
      } else if (className.contains(QStringLiteral("Text"), Qt::CaseInsensitive)) {
        typeText = QStringLiteral("Text Layer");
      } else if (ArtifactCore::dynamicPointerCast<ArtifactNoiseLayer>(targetLayer)) {
        typeText = QStringLiteral("Noise Layer");
      } else if (className.contains(QStringLiteral("Image"), Qt::CaseInsensitive)) {
        typeText = QStringLiteral("Image Layer");
      } else if (className.contains(QStringLiteral("Paint"), Qt::CaseInsensitive)) {
        typeText = QStringLiteral("Paint Layer");
      } else if (className.contains(QStringLiteral("Svg"), Qt::CaseInsensitive) ||
                 className.contains(QStringLiteral("Shape"), Qt::CaseInsensitive)) {
        typeText = QStringLiteral("SVG Layer");
      } else if (className.contains(QStringLiteral("Audio"), Qt::CaseInsensitive)) {
        typeText = QStringLiteral("Audio Layer");
      } else if (className.contains(QStringLiteral("Video"), Qt::CaseInsensitive)) {
        typeText = QStringLiteral("Video Layer");
      } else if (className.contains(QStringLiteral("Camera"), Qt::CaseInsensitive)) {
        typeText = QStringLiteral("Camera Layer");
      } else if (className.contains(QStringLiteral("Light"), Qt::CaseInsensitive)) {
        typeText = QStringLiteral("Light Layer");
      } else if (className.contains(QStringLiteral("Composition"), Qt::CaseInsensitive)) {
        typeText = QStringLiteral("Precomp Layer");
      } else if (targetLayer->is3D()) {
        typeText = QStringLiteral("3D Model Layer");
      }
      QString statePrefix;
      if (svc && svc->isLayerLockedInCurrentComposition(layerId)) {
        statePrefix = QStringLiteral("Locked");
      } else if (svc && !svc->isLayerVisibleInCurrentComposition(layerId)) {
        statePrefix = QStringLiteral("Hidden");
      } else if (svc && svc->isLayerSoloInCurrentComposition(layerId)) {
        statePrefix = QStringLiteral("Solo");
      } else if (svc && svc->isLayerShyInCurrentComposition(layerId)) {
        statePrefix = QStringLiteral("Shy");
      }
      const QString nameText = targetLayer->layerName().trimmed().isEmpty()
                                   ? QStringLiteral("Layer")
                                   : targetLayer->layerName().trimmed();
      if (statePrefix.isEmpty()) {
        return QStringLiteral("%1 • %2").arg(typeText, nameText);
      }
      return QStringLiteral("%1 %2 • %3").arg(statePrefix, typeText, nameText);
    };
    const auto describeContextSubtitle = [&](bool layerContext) {
      QStringList parts;
      parts.push_back(layerContext ? QStringLiteral("Layer actions")
                                   : QStringLiteral("Viewport actions"));
      if (layerContext && selectedCount > 1) {
        parts.push_back(QStringLiteral("%1 selected").arg(selectedCount));
      }
      if (!layerContext && clipboardHasLayerData) {
        parts.push_back(QStringLiteral("Clipboard ready"));
      }
      return parts.join(QStringLiteral(" • "));
    };
    const auto selectedLayersInComposition = [&]() {
      QVector<ArtifactAbstractLayerPtr> selected;
      if (!selection || !comp) {
        return selected;
      }
      const auto selectedSet = selection->selectedLayers();
      if (selectedSet.isEmpty()) {
        return selected;
      }
      const auto &orderedLayers = comp->allLayerRef();
      for (const auto &orderedLayer : orderedLayers) {
        if (orderedLayer && selectedSet.contains(orderedLayer)) {
          selected.push_back(orderedLayer);
        }
      }
      return selected;
    };
    const auto pasteLayersHere = [this]() {
      auto *service = ArtifactProjectService::instance();
      if (!service) {
        return;
      }
      const auto compNow = currentComposition();
      if (!compNow) {
        return;
      }
      const QJsonArray layersArray =
          ArtifactCore::ClipboardManager::instance().pasteLayers();
      if (layersArray.isEmpty()) {
        return;
      }
      auto *selectionManager = ArtifactLayerSelectionManager::instance();
      ArtifactAbstractLayerPtr anchorLayer;
      int anchorIndex = -1;
      if (selectionManager) {
        anchorLayer = selectionManager->currentLayer();
        if (anchorLayer) {
          const auto &layers = compNow->allLayerRef();
          for (int i = 0; i < layers.size(); ++i) {
            if (layers[i] && layers[i]->id() == anchorLayer->id()) {
              anchorIndex = i;
              break;
            }
          }
        }
      }
      if (selectionManager) {
        selectionManager->clearSelection();
      }
      int pasted = 0;
      QVector<ArtifactAbstractLayerPtr> pastedLayers;
      QHash<QString, LayerID> pastedLayerIdMap;
      for (const auto &val : layersArray) {
        if (!val.isObject()) {
          continue;
        }
        const QJsonObject sourceLayerObject = val.toObject();
        const QString sourceLayerId =
            sourceLayerObject.value(QStringLiteral("id")).toString().trimmed();
        QJsonObject layerObject = sourceLayerObject;
        layerObject.remove(QStringLiteral("id"));
        auto pastedLayer = ArtifactLayerFactory::createFromJson(layerObject);
        if (!pastedLayer) {
          continue;
        }
        pastedLayer->setLayerName(pastedLayer->layerName() +
                                  QStringLiteral(" (Copy)"));
        auto result = compNow->appendLayerTop(pastedLayer);
        if (!result.success) {
          continue;
        }
        if (anchorIndex >= 0) {
          const auto &layers = compNow->allLayerRef();
          int pastedIndex = -1;
          for (int i = 0; i < layers.size(); ++i) {
            if (layers[i] && layers[i]->id() == pastedLayer->id()) {
              pastedIndex = i;
              break;
            }
          }
          const int targetIndex = std::clamp(
              anchorIndex + pasted, 0,
              std::max(0, static_cast<int>(layers.size()) - 1));
          if (pastedIndex >= 0 && pastedIndex != targetIndex) {
            compNow->moveLayerToIndex(pastedLayer->id(), targetIndex);
          }
        }
        if (selectionManager) {
          selectionManager->addToSelection(pastedLayer);
        }
        pastedLayers.push_back(pastedLayer);
        if (!sourceLayerId.isEmpty() && !LayerID(sourceLayerId).isNil()) {
          pastedLayerIdMap.insert(sourceLayerId, pastedLayer->id());
        }
        ++pasted;
      }
      if (pasted == 0) {
        QMessageBox::warning(this, QStringLiteral("Paste Layers"),
                             QStringLiteral("No layers could be pasted."));
        return;
      }

      for (const auto &pastedLayer : pastedLayers) {
        if (!pastedLayer) {
          continue;
        }
        const auto parentId = pastedLayer->parentLayerId();
        const auto parentIt = pastedLayerIdMap.constFind(parentId.toString());
        if (parentIt != pastedLayerIdMap.constEnd()) {
          pastedLayer->setParentById(parentIt.value());
        }
        if (auto *cloneLayer = dynamic_cast<ArtifactCloneLayer *>(
                pastedLayer.get())) {
          auto cloneSettings = cloneLayer->cloneSettings();
          const auto sourceIt = pastedLayerIdMap.constFind(
              cloneSettings.sourceLayerId.toString());
          if (sourceIt != pastedLayerIdMap.constEnd()) {
            cloneSettings.sourceLayerId = sourceIt.value();
            cloneLayer->setCloneSettings(cloneSettings);
          }
        }
        auto matteReferences = pastedLayer->matteReferences();
        bool matteReferencesChanged = false;
        for (auto &matteReference : matteReferences) {
          const auto sourceIt = pastedLayerIdMap.constFind(
              matteReference.sourceLayerId.toString());
          if (sourceIt != pastedLayerIdMap.constEnd()) {
            matteReference.sourceLayerId = sourceIt.value();
            matteReferencesChanged = true;
          }
        }
        if (matteReferencesChanged) {
          pastedLayer->setMatteReferences(matteReferences);
        }
      }

      // Repackage the already-created layers as one undoable operation. The
      // layers are temporarily removed so the existing Add/Move commands can
      // replay the exact insertion order without introducing a new command type.
      auto transaction = std::make_unique<MacroUndoCommand>(
          QStringLiteral("Paste Layers"));
      struct PastedUndoEntry {
        ArtifactAbstractLayerPtr layer;
        int index = -1;
        std::unique_ptr<AddLayerCommand> addCommand;
      };
      std::vector<PastedUndoEntry> undoEntries;
      for (const auto &pastedLayer : pastedLayers) {
        if (!pastedLayer) continue;
        const auto currentLayers = compNow->allLayerRef();
        const int currentIndex = currentLayers.indexOf(pastedLayer);
        if (currentIndex < 0) continue;
        undoEntries.push_back(PastedUndoEntry{
            pastedLayer, currentIndex,
            std::make_unique<AddLayerCommand>(compNow, pastedLayer)});
      }
      for (auto &entry : undoEntries) {
        if (!entry.layer) continue;
        compNow->removeLayer(entry.layer->id());
        transaction->addChild(std::move(entry.addCommand));
        transaction->addChild(std::make_unique<MoveLayerIndexCommand>(
            compNow, entry.layer, 0, entry.index));
      }
      bool pushed = false;
      if (auto* manager = UndoManager::instance()) {
        pushed = manager->push(std::move(transaction));
      }
      if (!pushed) {
        for (const auto &entry : undoEntries) {
          if (!entry.layer || compNow->containsLayerById(entry.layer->id())) {
            continue;
          }
          compNow->appendLayerTop(entry.layer);
        }
        for (const auto &entry : undoEntries) {
          if (!entry.layer || entry.index < 0 ||
              !compNow->containsLayerById(entry.layer->id())) {
            continue;
          }
          compNow->moveLayerToIndex(entry.layer->id(), entry.index);
        }
        compNow->changed();
      }
    };
    const auto copyLayerBundle = [this, svc, comp, layer, selectedLayersInComposition]() {
      QVector<ArtifactAbstractLayerPtr> layersToCopy = selectedLayersInComposition();
      if (layersToCopy.isEmpty() && layer) {
        layersToCopy.push_back(layer);
      }
      if (layersToCopy.isEmpty()) {
        return;
      }

      QJsonArray layerJsonArray;
      for (const auto &copyLayer : layersToCopy) {
        if (copyLayer) {
          layerJsonArray.append(copyLayer->toJson());
        }
      }
      if (layerJsonArray.isEmpty()) {
        return;
      }

      QJsonObject metadata;
      metadata[QStringLiteral("sourceProjectName")] = svc ? svc->projectName().toQString() : QString();
      metadata[QStringLiteral("sourceCompositionName")] =
          comp ? comp->settings().compositionName().toQString() : QString();
      if (layersToCopy.size() == 1 && layersToCopy.first()) {
        metadata[QStringLiteral("sourceLayerId")] = layersToCopy.first()->id().toString();
        metadata[QStringLiteral("sourceLayerName")] = layersToCopy.first()->layerName();
      } else {
        QJsonArray sourceLayerIds;
        QJsonArray sourceLayerNames;
        for (const auto &copyLayer : layersToCopy) {
          if (!copyLayer) {
            continue;
          }
          sourceLayerIds.append(copyLayer->id().toString());
          sourceLayerNames.append(copyLayer->layerName());
        }
        metadata[QStringLiteral("sourceLayerIds")] = sourceLayerIds;
        metadata[QStringLiteral("sourceLayerNames")] = sourceLayerNames;
      }

      const QString bundleTitle = layersToCopy.size() == 1 && layersToCopy.first()
                                      ? layersToCopy.first()->layerName()
                                      : QStringLiteral("%1 layer(s)").arg(layerJsonArray.size());
      ArtifactCore::ClipboardManager::instance().copyLayerBundle(layerJsonArray, bundleTitle, metadata);
    };
    if (layer) {
      title = describeLayerMenuTitle(layer);
      subtitle = describeContextSubtitle(true);
      const bool selected = controller_->selectedLayerId() == layerId;
      if (!selected) {
        add(QStringLiteral("Select Layer"), [this, layerId]() {
          if (controller_) controller_->setSelectedLayerId(layerId);
        });
      }
      add(QStringLiteral("Focus Layer"), [this, layerId]() {
        if (controller_) {
          controller_->setSelectedLayerId(layerId);
          controller_->focusSelectedLayer();
        }
      });
      if (ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(layer)) {
        add(QStringLiteral("Edit Text"), [this, layer]() {
          editTextLayerInline(this, layer, controller_);
        });
      }
      add(QStringLiteral("Copy Layer"), [layer]() {
        ArtifactCore::ClipboardManager::instance().copyLayer(layer->toJson(),
                                                             layer->layerName());
      });
      add(QStringLiteral("Add Animation Layer"), [layer]() {
        if (!layer) return;
        const QJsonObject before = layer->animationLayersSnapshot();
        const std::size_t layerIndex = layer->animationLayers().addLayer();
        layer->animationLayers().layer(layerIndex).values.setCurrent(layer->opacity());
        const QJsonObject after = layer->animationLayersSnapshot();
        layer->restoreAnimationLayersSnapshot(before);
        commitAnimationLayerSnapshot(layer, before, after);
      });
      add(QStringLiteral("Add Transform Animation Layers"), [layer]() {
        if (!layer) return;
        const QJsonObject before = layer->animationLayersSnapshot();
        static const QStringList paths{
            QStringLiteral("transform.position.x"),
            QStringLiteral("transform.position.y"),
            QStringLiteral("transform.rotation"),
            QStringLiteral("transform.scale.x"),
            QStringLiteral("transform.scale.y"),
            QStringLiteral("transform.anchor.x"),
            QStringLiteral("transform.anchor.y")};
        for (const auto &path : paths) {
          auto &stack = layer->animationLayerStack(path);
          const std::size_t layerIndex = stack.addLayer();
          stack.layer(layerIndex).values.setCurrent(
              path.contains(QStringLiteral("scale")) ? 1.0f : 0.0f);
        }
        const QJsonObject after = layer->animationLayersSnapshot();
        layer->restoreAnimationLayersSnapshot(before);
        commitAnimationLayerSnapshot(layer, before, after);
      });
      add(QStringLiteral("Remove Transform Animation Layers"), [layer]() {
        if (!layer) return;
        const QJsonObject before = layer->animationLayersSnapshot();
        static const QStringList paths{
            QStringLiteral("transform.position.x"),
            QStringLiteral("transform.position.y"),
            QStringLiteral("transform.rotation"),
            QStringLiteral("transform.scale.x"),
            QStringLiteral("transform.scale.y"),
            QStringLiteral("transform.anchor.x"),
            QStringLiteral("transform.anchor.y")};
        for (const auto &path : paths) {
          auto &stack = layer->animationLayerStack(path);
          stack.clear();
        }
        const QJsonObject after = layer->animationLayersSnapshot();
        layer->restoreAnimationLayersSnapshot(before);
        commitAnimationLayerSnapshot(layer, before, after);
      });
      add(QStringLiteral("Bake Animation Layers at Current Frame"), [layer]() {
        if (!layer) return;
        const QJsonObject before = layer->animationLayersSnapshot();
        layer->bakeAnimationLayersAtCurrentFrame();
        const QJsonObject after = layer->animationLayersSnapshot();
        layer->restoreAnimationLayersSnapshot(before);
        commitAnimationLayerSnapshot(layer, before, after);
      });
      add(QStringLiteral("Bake Animation Layers Over Work Area"), [layer, comp]() {
        if (!layer || !comp) return;
        const auto range = comp->workAreaRange();
        if (!range.isValid() || range.isEmpty()) return;
        const QJsonObject before = layer->animationLayersSnapshot();
        layer->bakeAnimationLayersOverRange(range.start(), range.end());
        const QJsonObject after = layer->animationLayersSnapshot();
        layer->restoreAnimationLayersSnapshot(before);
        commitAnimationLayerSnapshot(layer, before, after);
      });
      if (layer->animationLayers().layerCount() > 0) {
        add(QStringLiteral("Remove Top Animation Layer"), [layer]() {
          if (!layer || layer->animationLayers().layerCount() == 0) return;
          const QJsonObject before = layer->animationLayersSnapshot();
          layer->animationLayers().removeLayer(
              layer->animationLayers().layerCount() - 1);
          const QJsonObject after = layer->animationLayersSnapshot();
          layer->restoreAnimationLayersSnapshot(before);
          commitAnimationLayerSnapshot(layer, before, after);
        });
      }
      add(selectedCount > 1 ? QStringLiteral("Copy Selected Layers as Bundle")
                            : QStringLiteral("Copy Layer as Bundle"),
          copyLayerBundle);
      add(selectedCount > 1 ? QStringLiteral("Send Selected Layers to Main Project")
                            : QStringLiteral("Send Layer to Main Project"),
          [this, layer, svc, comp, selectedLayersInComposition]() {
            QVector<ArtifactAbstractLayerPtr> layersToSend = selectedLayersInComposition();
            if (layersToSend.isEmpty() && layer) {
              layersToSend.push_back(layer);
            }
            if (layersToSend.isEmpty()) {
              return;
            }

            QJsonArray layerJsonArray;
            for (const auto &sendLayer : layersToSend) {
              if (sendLayer) {
                layerJsonArray.append(sendLayer->toJson());
              }
            }
            if (layerJsonArray.isEmpty()) {
              return;
            }

            QJsonObject bundle;
            bundle[QStringLiteral("bundleKind")] = QStringLiteral("layer");
            bundle[QStringLiteral("bundleTitle")] =
                layersToSend.size() == 1 && layersToSend.first()
                    ? layersToSend.first()->layerName()
                    : QStringLiteral("%1 layer(s)").arg(layerJsonArray.size());
            bundle[QStringLiteral("layers")] = layerJsonArray;
            bundle[QStringLiteral("sourceProjectName")] =
                svc ? svc->projectName().toQString() : QString();
            bundle[QStringLiteral("sourceCompositionName")] =
                comp ? comp->settings().compositionName().toQString() : QString();
            if (layersToSend.size() == 1 && layersToSend.first()) {
              bundle[QStringLiteral("sourceLayerId")] =
                  layersToSend.first()->id().toString();
              bundle[QStringLiteral("sourceLayerName")] =
                  layersToSend.first()->layerName();
            }
            QString error;
            if (!sendProjectBundleToMainProject(bundle, &error)) {
              QMessageBox::warning(this, QStringLiteral("Send Bundle"),
                  error.isEmpty() ? QStringLiteral("Failed to send layer bundle to the main project.")
                                  : error);
            }
          });
      add(QStringLiteral("Paste Layers Here"), pasteLayersHere,
          clipboardHasLayerData);
      addSeparator();
      const bool visible =
          svc ? svc->isLayerVisibleInCurrentComposition(layerId) : true;
      add(visible ? QStringLiteral("Hide Layer")
                  : QStringLiteral("Show Layer"),
          [layerId, visible]() {
            auto *svc = ArtifactProjectService::instance();
            if (!svc) return;
            auto comp = svc->currentComposition().lock();
            if (!comp) return;
            auto l = comp->layerById(layerId);
            if (!l) return;
            auto command = std::make_unique<SetLayerVisibilityCommand>(l, !visible);
            bool applied = false;
            if (auto* manager = UndoManager::instance()) {
              applied = manager->push(std::move(command));
            } else {
              command->redo();
              applied = command->lastOperationSucceeded();
            }
            if (!applied) return;
          });
      const bool locked =
          svc ? svc->isLayerLockedInCurrentComposition(layerId) : false;
      add(locked ? QStringLiteral("Unlock Layer")
                 : QStringLiteral("Lock Layer"),
          [layerId, locked]() {
            auto *svc = ArtifactProjectService::instance();
            if (!svc) return;
            auto comp = svc->currentComposition().lock();
            if (!comp) return;
            auto l = comp->layerById(layerId);
            if (!l) return;
            auto command = std::make_unique<SetLayerLockCommand>(l, !locked);
            bool applied = false;
            if (auto* manager = UndoManager::instance()) {
              applied = manager->push(std::move(command));
            } else {
              command->redo();
              applied = command->lastOperationSucceeded();
            }
            if (!applied) return;
          });
      const bool solo =
          svc ? svc->isLayerSoloInCurrentComposition(layerId) : false;
      add(solo ? QStringLiteral("Unsolo Layer")
               : QStringLiteral("Solo Layer"),
          [layerId, solo]() {
            auto *svc = ArtifactProjectService::instance();
            if (!svc) return;
            auto comp = svc->currentComposition().lock();
            if (!comp) return;
            auto l = comp->layerById(layerId);
            if (!l) return;
            auto command = std::make_unique<SetLayerSoloCommand>(l, !solo);
            bool applied = false;
            if (auto* manager = UndoManager::instance()) {
              applied = manager->push(std::move(command));
            } else {
              command->redo();
              applied = command->lastOperationSucceeded();
            }
            if (!applied) return;
          });
      const bool shy = svc ? svc->isLayerShyInCurrentComposition(layerId) : false;
      add(shy ? QStringLiteral("Unshy Layer")
              : QStringLiteral("Shy Layer"),
          [layerId, shy]() {
            auto *svc = ArtifactProjectService::instance();
            if (!svc) return;
            auto comp = svc->currentComposition().lock();
            if (!comp) return;
            auto l = comp->layerById(layerId);
            if (!l) return;
            auto command = std::make_unique<SetLayerShyCommand>(l, !shy);
            bool applied = false;
            if (auto* manager = UndoManager::instance()) {
              applied = manager->push(std::move(command));
            } else {
              command->redo();
              applied = command->lastOperationSucceeded();
            }
            if (!applied) return;
          });
      add(QStringLiteral("Center Layer"), [this, layerId]() {
        auto *service = ArtifactProjectService::instance();
        const auto compNow = currentComposition();
        const auto clickedLayer =
            compNow ? compNow->layerById(layerId) : ArtifactAbstractLayerPtr{};
        if (!service || !compNow || !clickedLayer) {
          return;
        }
        const QSize compSize = compNow->settings().compositionSize();
        const float compCenterX =
            static_cast<float>(compSize.width() > 0 ? compSize.width() : 1920) *
            0.5f;
        const float compCenterY =
            static_cast<float>(compSize.height() > 0 ? compSize.height() : 1080) *
            0.5f;
        const QVector3D current = clickedLayer->position3D();
        const float deltaX = compCenterX - current.x();
        const float deltaY = compCenterY - current.y();
        if (std::abs(deltaX) <= 0.0001f && std::abs(deltaY) <= 0.0001f) {
          return;
        }
        const auto playback = ArtifactPlaybackService::instance();
        const auto currentFrame = playback
            ? playback->currentFrame()
            : compNow->framePosition();
        auto command = std::make_unique<MoveLayerCommand>(
            clickedLayer, deltaX, deltaY, currentFrame.framePosition());
        if (auto* manager = UndoManager::instance()) {
          if (!manager->push(std::move(command))) return;
        } else {
          command->redo();
          if (!command->lastOperationSucceeded()) return;
        }
      });
      if (selected && layer->is3D()) {
        add(QStringLiteral("Reset 3D Transform"), [this]() {
          if (controller_) {
            controller_->resetSelected3DTransform();
          }
        });
        add(QStringLiteral("Edit 3D Transform Numerically…"), [this, layer]() {
          if (!controller_ || !layer || !layer->is3D()) return;
          const auto currentPosition = layer->position3D();
          const auto currentRotation = layer->rotation3D();
          const QVector3D currentScale(layer->transform3D().scaleX(),
                                        layer->transform3D().scaleY(), 1.0f);
          bool accepted = false;
          const auto read = [this, &accepted](const QString& label,
                                               double value,
                                               double minimum,
                                               double maximum) {
            return QInputDialog::getDouble(this, QStringLiteral("3D Transform"),
                                           label, value, minimum, maximum, 3,
                                           &accepted);
          };
          const double px = read(QStringLiteral("Position X"), currentPosition.x(),
                                 -1000000.0, 1000000.0);
          if (!accepted) return;
          const double py = read(QStringLiteral("Position Y"), currentPosition.y(),
                                 -1000000.0, 1000000.0);
          if (!accepted) return;
          const double pz = read(QStringLiteral("Position Z"), currentPosition.z(),
                                 -1000000.0, 1000000.0);
          if (!accepted) return;
          const double rx = read(QStringLiteral("Rotation X"), currentRotation.x(),
                                 -36000.0, 36000.0);
          if (!accepted) return;
          const double ry = read(QStringLiteral("Rotation Y"), currentRotation.y(),
                                 -36000.0, 36000.0);
          if (!accepted) return;
          const double rz = read(QStringLiteral("Rotation Z"), currentRotation.z(),
                                 -36000.0, 36000.0);
          if (!accepted) return;
          const double sx = read(QStringLiteral("Scale X"), currentScale.x(),
                                 0.001, 10000.0);
          if (!accepted) return;
          const double sy = read(QStringLiteral("Scale Y"), currentScale.y(),
                                 0.001, 10000.0);
          if (!accepted) return;
          const QVector3D position(static_cast<float>(px), static_cast<float>(py),
                                   static_cast<float>(pz));
          const QVector3D rotation(static_cast<float>(rx), static_cast<float>(ry),
                                   static_cast<float>(rz));
          const QVector3D scale(static_cast<float>(sx), static_cast<float>(sy), 1.0f);
          controller_->setSelected3DTransform(position, rotation, scale);
        });
        add(QStringLiteral("Copy 3D Transform"), [layer]() {
          if (!layer || !layer->is3D()) return;
          const auto position = layer->position3D();
          const auto rotation = layer->rotation3D();
          const auto scale = QVector3D(layer->transform3D().scaleX(),
                                       layer->transform3D().scaleY(), 1.0f);
          ArtifactCore::LayeredConfigStore::instance().setValue(
              QStringLiteral("Viewport/3DTransformClipboard"),
              QVariantList{
                  position.x(), position.y(), position.z(), rotation.x(),
                  rotation.y(), rotation.z(), scale.x(), scale.y(), scale.z()});
        });
        const QVariantList transformClipboard =
            ArtifactCore::LayeredConfigStore::instance()
                .value(QStringLiteral("Viewport/3DTransformClipboard"))
                .toList();
        add(QStringLiteral("Paste 3D Transform"), [this, transformClipboard]() {
          if (!controller_ || transformClipboard.size() < 8) return;
          const auto finite = [](float value) {
            return std::isfinite(static_cast<double>(value));
          };
          const QVector3D position(transformClipboard.at(0).toFloat(),
                                   transformClipboard.at(1).toFloat(),
                                   transformClipboard.at(2).toFloat());
          const QVector3D rotation(transformClipboard.at(3).toFloat(),
                                   transformClipboard.at(4).toFloat(),
                                   transformClipboard.at(5).toFloat());
          const QVector3D scale(transformClipboard.at(6).toFloat(),
                                transformClipboard.at(7).toFloat(),
                                transformClipboard.size() > 8
                                    ? transformClipboard.at(8).toFloat()
                                    : 1.0f);
          if (!finite(position.x()) || !finite(position.y()) ||
              !finite(position.z()) || !finite(rotation.x()) ||
              !finite(rotation.y()) || !finite(rotation.z()) ||
              !finite(scale.x()) || !finite(scale.y()) || !finite(scale.z())) {
            return;
          }
          controller_->setSelected3DTransform(position, rotation, scale);
        }, transformClipboard.size() >= 8);
        add(QStringLiteral("Clear 3D Transform Clipboard"), []() {
          auto& config = ArtifactCore::LayeredConfigStore::instance();
          config.removeValue(ArtifactCore::ConfigLayer::Project,
                             std::string_view("Viewport/3DTransformClipboard"));
          config.removeValue(ArtifactCore::ConfigLayer::User,
                             std::string_view("Viewport/3DTransformClipboard"));
        }, transformClipboard.size() >= 8);
      }
      if (controller_->isShowMotionPathOverlay() && selected) {
        const auto playback = ArtifactPlaybackService::instance();
        const auto currentFrame =
            playback ? playback->currentFrame()
                     : (comp ? comp->framePosition() : FramePosition(0));
        const RationalTime time(currentFrame.framePosition(), 24);
        const bool hasMotionPathKey =
            layer->transform3D().hasPositionKeyFrameAt(time);
        const auto currentMotionPathInterpolation =
            layer->transform3D().positionXKeyFrameInterpolationAt(time);
        addSeparator();
        add(QStringLiteral("Set Motion Path Keyframe Here"),
            [this]() {
              if (controller_) {
                controller_->setSelectedLayerMotionPathKeyframeAtCurrentFrame();
              }
            },
            !hasMotionPathKey);
        add(QStringLiteral("Remove Motion Path Keyframe Here"),
            [this]() {
              if (controller_) {
                controller_->removeSelectedLayerMotionPathKeyframeAtCurrentFrame();
              }
            },
            hasMotionPathKey);
        addSeparator();
        add(QStringLiteral("Motion Path: Hold"), [this]() {
              if (controller_) {
                controller_->setSelectedLayerMotionPathInterpolationAtCurrentFrame(
                    static_cast<int>(ArtifactCore::InterpolationType::Constant));
              }
            },
            hasMotionPathKey &&
                currentMotionPathInterpolation != ArtifactCore::InterpolationType::Constant);
        add(QStringLiteral("Motion Path: Linear"), [this]() {
              if (controller_) {
                controller_->setSelectedLayerMotionPathInterpolationAtCurrentFrame(
                    static_cast<int>(ArtifactCore::InterpolationType::Linear));
              }
            },
            hasMotionPathKey &&
                currentMotionPathInterpolation != ArtifactCore::InterpolationType::Linear);
        add(QStringLiteral("Motion Path: Ease In"), [this]() {
              if (controller_) {
                controller_->setSelectedLayerMotionPathInterpolationAtCurrentFrame(
                    static_cast<int>(ArtifactCore::InterpolationType::EaseIn));
              }
            },
            hasMotionPathKey &&
                currentMotionPathInterpolation != ArtifactCore::InterpolationType::EaseIn);
        add(QStringLiteral("Motion Path: Ease Out"), [this]() {
              if (controller_) {
                controller_->setSelectedLayerMotionPathInterpolationAtCurrentFrame(
                    static_cast<int>(ArtifactCore::InterpolationType::EaseOut));
              }
            },
            hasMotionPathKey &&
                currentMotionPathInterpolation != ArtifactCore::InterpolationType::EaseOut);
        add(QStringLiteral("Motion Path: Ease In-Out"), [this]() {
              if (controller_) {
                controller_->setSelectedLayerMotionPathInterpolationAtCurrentFrame(
                    static_cast<int>(ArtifactCore::InterpolationType::EaseInOut));
              }
            },
            hasMotionPathKey &&
                currentMotionPathInterpolation != ArtifactCore::InterpolationType::EaseInOut);
        add(QStringLiteral("Motion Path: Bezier"), [this]() {
              if (controller_) {
                controller_->setSelectedLayerMotionPathInterpolationAtCurrentFrame(
                    static_cast<int>(ArtifactCore::InterpolationType::Bezier));
              }
            },
            hasMotionPathKey &&
                currentMotionPathInterpolation != ArtifactCore::InterpolationType::Bezier);
        add(QStringLiteral("Motion Path: Back"), [this]() {
              if (controller_) {
                controller_->setSelectedLayerMotionPathInterpolationAtCurrentFrame(
                    static_cast<int>(ArtifactCore::InterpolationType::BackOut));
              }
            },
            hasMotionPathKey &&
                currentMotionPathInterpolation != ArtifactCore::InterpolationType::BackOut);
        add(QStringLiteral("Motion Path: Expo"), [this]() {
              if (controller_) {
                controller_->setSelectedLayerMotionPathInterpolationAtCurrentFrame(
                    static_cast<int>(ArtifactCore::InterpolationType::Exponential));
              }
            },
            hasMotionPathKey &&
                currentMotionPathInterpolation != ArtifactCore::InterpolationType::Exponential);
      }
      addSeparator();
      const auto parentId =
          svc ? svc->layerParentIdInCurrentComposition(layerId) : LayerID{};
      if (!parentId.isNil()) {
        add(QStringLiteral("Select Parent Layer"), [this, parentId]() {
          auto *service = ArtifactProjectService::instance();
          if (!service) {
            return;
          }
          service->selectLayer(parentId);
        });
        add(QStringLiteral("Unparent Layer"), [layerId]() {
          auto *service = ArtifactProjectService::instance();
          if (!service) {
            return;
          }
          service->clearLayerParentInCurrentComposition(layerId);
        });
      }
      if (comp) {
        const auto &layers = comp->allLayerRef();
        int currentIndex = -1;
        for (int i = 0; i < layers.size(); ++i) {
          if (layers[i] && layers[i]->id() == layerId) {
            currentIndex = i;
            break;
          }
        }
        if (currentIndex >= 0) {
          const int lastIndex = layers.isEmpty() ? 0 : layers.size() - 1;
          if (currentIndex > 0) {
            add(QStringLiteral("Bring Forward"), [layerId, currentIndex]() {
              auto *service = ArtifactProjectService::instance();
              if (!service) {
                return;
              }
              service->moveLayerInCurrentComposition(layerId, currentIndex - 1);
            });
            add(QStringLiteral("Bring to Front"), [this, layerId]() {
              auto *service = ArtifactProjectService::instance();
              const auto compNow = currentComposition();
              if (!service || !compNow) {
                return;
              }
              const auto &layers = compNow->allLayerRef();
              service->moveLayerInCurrentComposition(
                  layerId, std::max(0, static_cast<int>(layers.size()) - 1));
            });
          }
          if (currentIndex < lastIndex) {
            add(QStringLiteral("Send Backward"), [layerId, currentIndex]() {
              auto *service = ArtifactProjectService::instance();
              if (!service) {
                return;
              }
              service->moveLayerInCurrentComposition(layerId, currentIndex + 1);
            });
            add(QStringLiteral("Send to Back"), [layerId]() {
              auto *service = ArtifactProjectService::instance();
              if (!service) {
                return;
              }
              service->moveLayerInCurrentComposition(layerId, 0);
            });
          }
        }
      }
      add(QStringLiteral("Group Selected Layers"), [this]() {
          auto *service = ArtifactProjectService::instance();
          if (!service) return;
          if (!service->groupSelectedLayersWithUndo()) {
            QMessageBox::warning(this, QStringLiteral("Group Layers"),
                                 QStringLiteral("Could not group selected layers."));
          }
        }, selectedCount > 1);
      addSeparator();
      add(QStringLiteral("Ungroup Selected Group"), [this]() {
          auto *service = ArtifactProjectService::instance();
          if (!service) return;
          if (!service->ungroupSelectedGroupWithUndo()) {
            QMessageBox::warning(this, QStringLiteral("Ungroup Layers"),
                                 QStringLiteral("Could not ungroup the selected group."));
          }
        }, layer->isGroupLayer());
      addSeparator();
      add(QStringLiteral("Duplicate Layer"), [this, layerId]() {
        auto *service = ArtifactProjectService::instance();
        if (!service) {
          return;
        }
        if (!service->duplicateLayerInCurrentComposition(layerId)) {
          QMessageBox::warning(this, QStringLiteral("Duplicate Layer"),
                               QStringLiteral("Layer duplication failed."));
        }
      });
      add(QStringLiteral("Rename Layer"), [this, layerId]() {
        auto *service = ArtifactProjectService::instance();
        const auto compNow = currentComposition();
        if (!service || !compNow) {
          return;
        }
        const QString currentName = service->layerNameInCurrentComposition(layerId);
        bool ok = false;
        const QString newName = QInputDialog::getText(
            this, QStringLiteral("Rename Layer"), QStringLiteral("Layer name:"),
            QLineEdit::Normal, currentName, &ok);
        if (!ok) {
          return;
        }
        const QString trimmed = newName.trimmed();
        if (trimmed.isEmpty()) {
          return;
        }
        if (!service->renameLayerInCurrentComposition(layerId, trimmed)) {
          QMessageBox::warning(this, QStringLiteral("Rename Layer"),
                               QStringLiteral("Layer rename failed."));
        }
      });
      add(QStringLiteral("Delete Layer"), [this, layerId]() {
        auto *service = ArtifactProjectService::instance();
        const auto compNow = currentComposition();
        if (!service || !compNow) {
          return;
        }
        const QString message =
            service->layerRemovalConfirmationMessage(compNow->id(), layerId);
        const auto response = QMessageBox::question(
            this, QStringLiteral("Delete Layer"), message,
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (response != QMessageBox::Yes) {
          return;
        }
        if (!service->removeLayerFromComposition(compNow->id(), layerId)) {
          QMessageBox::warning(this, QStringLiteral("Delete Layer"),
                               QStringLiteral("Layer deletion failed."));
        }
      });
    } else {
      title = QStringLiteral("View");
      subtitle = describeContextSubtitle(false);
      add(QStringLiteral("New Text Layer"), [this]() {
        auto *service = ArtifactProjectService::instance();
        if (!service) {
          return;
        }
        ArtifactTextLayerInitParams params(QStringLiteral("Text 1"));
        service->addLayerToCurrentComposition(params);
      });
      add(QStringLiteral("Import SRT/WebVTT as Text Keyframes..."), [this]() {
        auto *service = ArtifactProjectService::instance();
        const auto compNow = currentComposition();
        if (!service || !compNow) {
          return;
        }
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("Import SRT/WebVTT as Text Keyframes"), QString(),
            QStringLiteral("Subtitles (*.srt *.vtt);;SubRip subtitles (*.srt);;WebVTT (*.vtt);;All Files (*.*)"));
        if (path.isEmpty()) {
          return;
        }
        QVector<QString> warnings;
        if (!QFile::exists(path)) {
          QMessageBox::warning(this, QStringLiteral("Subtitle Import"),
                               QStringLiteral("Subtitle file does not exist."));
          return;
        }
        QFile subtitleFile(path);
        if (!subtitleFile.open(QIODevice::ReadOnly)) {
          QMessageBox::warning(this, QStringLiteral("Subtitle Import"),
                               QStringLiteral("Could not open subtitle file."));
          return;
        }
        const auto subtitleText = QString::fromUtf8(subtitleFile.readAll());
        const auto timeBase = ArtifactCore::NLE::TimeBase{
            1, static_cast<int32_t>(compositionFrameRateScale(compNow)), false};
        const auto cues = QFileInfo(path).suffix().compare(QStringLiteral("vtt"),
                                                            Qt::CaseInsensitive) == 0
                              ? ArtifactCore::NLE::OtioAdapter::importWebVtt(
                                    subtitleText, timeBase, &warnings)
                              : ArtifactCore::NLE::OtioAdapter::importSrt(
                                    subtitleText, timeBase, &warnings);
        if (cues.isEmpty()) {
          QMessageBox::warning(this, QStringLiteral("Subtitle Import"),
                               warnings.isEmpty()
                                   ? QStringLiteral("No subtitle cues were found.")
                                   : warnings.join(QStringLiteral("\n")));
          return;
        }
        ArtifactTextLayerInitParams params(QStringLiteral("Imported Subtitles"));
        service->addLayerToCurrentComposition(params, true);
        const auto importedComp = currentComposition();
        if (!importedComp || importedComp->allLayerRef().empty()) {
          return;
        }
        const auto textLayer = ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(
            importedComp->allLayerRef().back());
        if (!textLayer) {
          return;
        }
        const auto textProperty = textLayer->getProperty(QStringLiteral("text.value"));
        if (!textProperty) {
          return;
        }
        textProperty->clearKeyFrames();
        const double fps = safeCompositionFrameRate(compNow);
        for (const auto &cue : cues) {
          textProperty->addKeyFrame(
              ArtifactCore::RationalTime(cue.range.start(), fps),
              QVariant(cue.text), ArtifactCore::InterpolationType::Constant);
        }
        const auto &lastCue = cues.constLast();
        textProperty->addKeyFrame(
            ArtifactCore::RationalTime(
                lastCue.range.start() + lastCue.range.duration(), fps),
            QVariant(QString()), ArtifactCore::InterpolationType::Constant);
        textLayer->setText(ArtifactCore::UniString::fromQString(cues.front().text));
        if (!warnings.isEmpty()) {
          QMessageBox::information(this, QStringLiteral("Subtitle Import"),
                                   warnings.join(QStringLiteral("\n")));
        }
      });
      add(QStringLiteral("Export Selected Text Keyframes as SRT/WebVTT..."), [this]() {
        const auto compNow = currentComposition();
        if (!compNow || !controller_ || controller_->selectedLayerId().isNil()) {
          QMessageBox::information(this, QStringLiteral("Subtitle Export"),
                                   QStringLiteral("Select a Text Layer first."));
          return;
        }
        const auto textLayer = ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(
            compNow->layerById(controller_->selectedLayerId()));
        if (!textLayer) {
          QMessageBox::information(this, QStringLiteral("Subtitle Export"),
                                   QStringLiteral("The selected layer is not a Text Layer."));
          return;
        }
        const auto frames = textLayer->sourceTextKeyframeFrames();
        if (frames.isEmpty()) {
          QMessageBox::information(this, QStringLiteral("Subtitle Export"),
                                   QStringLiteral("The selected Text Layer has no Source Text keyframes."));
          return;
        }
        const QString path = QFileDialog::getSaveFileName(
            this, QStringLiteral("Export Text Keyframes"), QString(),
            QStringLiteral("SubRip subtitles (*.srt);;WebVTT (*.vtt)"));
        if (path.isEmpty()) {
          return;
        }
        const double fps = safeCompositionFrameRate(compNow);
        const auto timeBase = ArtifactCore::NLE::TimeBase{
            1, static_cast<int32_t>(std::llround(fps)), false};
        QVector<ArtifactCore::NLE::SubtitleCue> cues;
        for (qsizetype index = 0; index < frames.size(); ++index) {
          const qint64 start = frames.at(index);
          const qint64 next = index + 1 < frames.size()
                                  ? frames.at(index + 1)
                                  : start + std::max<qint64>(1, static_cast<qint64>(std::ceil(fps)));
          const QString text = textLayer->sourceTextAtFrame(start).trimmed();
          if (text.isEmpty() || next <= start) {
            continue;
          }
          ArtifactCore::NLE::SubtitleCue cue;
          cue.range = ArtifactCore::FrameRange::fromDuration(start, next - start);
          cue.text = text;
          cues.append(std::move(cue));
        }
        const bool webVtt = QFileInfo(path).suffix().compare(QStringLiteral("vtt"),
                                                              Qt::CaseInsensitive) == 0;
        const QString output = webVtt
                                   ? ArtifactCore::NLE::OtioAdapter::exportWebVtt(cues, timeBase)
                                   : ArtifactCore::NLE::OtioAdapter::exportSrt(cues, timeBase);
        QFile outputFile(path);
        if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
            outputFile.write(output.toUtf8()) != output.toUtf8().size()) {
          QMessageBox::warning(this, QStringLiteral("Subtitle Export"),
                               QStringLiteral("Could not write the subtitle file."));
        }
      });
      add(QStringLiteral("New Null Layer"), [this]() {
        auto *service = ArtifactProjectService::instance();
        const auto compNow = currentComposition();
        if (!service || !compNow) {
          return;
        }
        ArtifactNullLayerInitParams params(QStringLiteral("Null 1"));
        const QSize compSize = compNow->settings().compositionSize();
        params.setWidth(compSize.width());
        params.setHeight(compSize.height());
        service->addLayerToCurrentComposition(params);
      });
      add(QStringLiteral("New Adjustment Layer"), [this]() {
        auto *service = ArtifactProjectService::instance();
        if (!service) {
          return;
        }
        ArtifactLayerInitParams params(QStringLiteral("Adjustment Layer 1"),
                                       LayerType::Adjustment);
        service->addLayerToCurrentComposition(params);
      });
      add(QStringLiteral("New Noise Layer..."), [this]() {
        auto *service = ArtifactProjectService::instance();
        const auto compNow = currentComposition();
        if (!service || !compNow) {
          return;
        }
        CreateNoiseLayerDialog dialog(this);
        const QSize compSize = compNow->settings().compositionSize();
        dialog.setCompositionSize(compSize.width(), compSize.height());
        if (dialog.exec() != QDialog::Accepted) {
          return;
        }
        ArtifactNoiseLayerInitParams params(dialog.layerName().isEmpty()
                                                ? QStringLiteral("Noise Layer 1")
                                                : dialog.layerName());
        params.setWidth(dialog.width());
        params.setHeight(dialog.height());
        params.setSeed(dialog.seed());
        params.setKind(dialog.kind());
        service->addLayerToCurrentComposition(params);
      });
      const auto addNoisePreset = [this, &add](const QString& label,
                                         ArtifactCore::ProceduralTexturePreset preset) {
        add(label, [this, preset]() {
          auto *service = ArtifactProjectService::instance();
          const auto compNow = currentComposition();
          if (!service || !compNow) {
            return;
          }
          ArtifactNoiseLayerInitParams params(QStringLiteral("Noise Layer 1"));
          const QSize compSize = compNow->settings().compositionSize();
          params.setWidth(compSize.width());
          params.setHeight(compSize.height());
          params.setPreset(preset);
          service->addLayerToCurrentComposition(params);
        });
      };
      addNoisePreset(QStringLiteral("Noise: Marble"), ArtifactCore::ProceduralTexturePreset::Marble);
      addNoisePreset(QStringLiteral("Noise: Clouds"), ArtifactCore::ProceduralTexturePreset::Clouds);
      addNoisePreset(QStringLiteral("Noise: Cellular"), ArtifactCore::ProceduralTexturePreset::Cellular);
      addNoisePreset(QStringLiteral("Noise: Fabric"), ArtifactCore::ProceduralTexturePreset::Fabric);
      addNoisePreset(QStringLiteral("Noise: Terrain"), ArtifactCore::ProceduralTexturePreset::Terrain);
      addNoisePreset(QStringLiteral("Noise: Metal"), ArtifactCore::ProceduralTexturePreset::Metal);
      add(QStringLiteral("New Camera Layer"), [this]() {
        auto *service = ArtifactProjectService::instance();
        if (!service) {
          return;
        }
        CreateCameraLayerDialog dialog(this);
        dialog.setModal(true);
        if (dialog.exec() != QDialog::Accepted) {
          return;
        }

        ArtifactCameraLayerInitParams params;
        params.setName(UniString(dialog.cameraName().trimmed().isEmpty()
                                     ? QStringLiteral("Camera 1")
                                     : dialog.cameraName()));

        service->addLayerToCurrentComposition(params);

        auto* selectionManager = ArtifactLayerSelectionManager::instance();
        const ArtifactCameraLayerPtr camera =
            selectionManager
                ? ArtifactCore::dynamicPointerCast<ArtifactCameraLayer>(
                      selectionManager->currentLayer())
                : ArtifactCameraLayerPtr{};
        if (!camera) {
          return;
        }

        camera->setZoom(dialog.zoom());
        camera->setFocusDistance(dialog.focusDistance());
        camera->setAperture(dialog.apertureF());
        camera->setDepthOfField(dialog.depthOfFieldEnabled());
        camera->setMotionBlur(dialog.motionBlur());
        camera->setBlurAmount(dialog.blurAmount());
        camera->setUseManualFov(true);
        camera->setFov(dialog.fov());
        camera->setLocked(dialog.cameraLocked());
      });
      add(QStringLiteral("New Light Layer"), [this]() {
        auto *service = ArtifactProjectService::instance();
        if (!service) {
          return;
        }
        ArtifactLayerInitParams params(QStringLiteral("Light 1"),
                                       LayerType::Light);
        service->addLayerToCurrentComposition(params);
      });
      add(QStringLiteral("New 3D Plane Layer"), [this]() {
        auto *service = ArtifactProjectService::instance();
        if (!service) {
          return;
        }
        ArtifactFixedGeometry3DLayerInitParams params(QStringLiteral("3D Plane 1"),
                                                      FixedGeometry3D::Plane);
        service->addLayerToCurrentComposition(params);
        if (controller_) {
          controller_->markRenderDirty();
        }
      });
      add(QStringLiteral("New 3D Box Layer"), [this]() {
        auto *service = ArtifactProjectService::instance();
        if (!service) {
          return;
        }
        ArtifactFixedGeometry3DLayerInitParams params(QStringLiteral("3D Box 1"),
                                                      FixedGeometry3D::Cube);
        service->addLayerToCurrentComposition(params);
        if (controller_) {
          controller_->markRenderDirty();
        }
      });
      add(QStringLiteral("New 3D Sphere Layer"), [this]() {
        auto *service = ArtifactProjectService::instance();
        if (!service) {
          return;
        }
        ArtifactFixedGeometry3DLayerInitParams params(QStringLiteral("3D Sphere 1"),
                                                      FixedGeometry3D::Sphere);
        service->addLayerToCurrentComposition(params);
        if (controller_) {
          controller_->markRenderDirty();
        }
      });
      add(QStringLiteral("New 3D Cylinder Layer"), [this]() {
        auto *service = ArtifactProjectService::instance();
        if (!service) {
          return;
        }
        ArtifactFixedGeometry3DLayerInitParams params(QStringLiteral("3D Cylinder 1"),
                                                      FixedGeometry3D::Cylinder);
        service->addLayerToCurrentComposition(params);
        if (controller_) {
          controller_->markRenderDirty();
        }
      });
      add(QStringLiteral("New 3D Cone Layer"), [this]() {
        auto *service = ArtifactProjectService::instance();
        if (!service) {
          return;
        }
        ArtifactFixedGeometry3DLayerInitParams params(QStringLiteral("3D Cone 1"),
                                                      FixedGeometry3D::Cone);
        service->addLayerToCurrentComposition(params);
        if (controller_) {
          controller_->markRenderDirty();
        }
      });
      add(QStringLiteral("New 3D Torus Layer"), [this]() {
        auto *service = ArtifactProjectService::instance();
        if (!service) {
          return;
        }
        ArtifactFixedGeometry3DLayerInitParams params(QStringLiteral("3D Torus 1"),
                                                      FixedGeometry3D::Torus);
        service->addLayerToCurrentComposition(params);
        if (controller_) {
          controller_->markRenderDirty();
        }
      });
      add(QStringLiteral("New 3D Capsule Layer"), [this]() {
        auto *service = ArtifactProjectService::instance();
        if (!service) {
          return;
        }
        ArtifactFixedGeometry3DLayerInitParams params(QStringLiteral("3D Capsule 1"),
                                                      FixedGeometry3D::Capsule);
        service->addLayerToCurrentComposition(params);
        if (controller_) {
          controller_->markRenderDirty();
        }
      });
      add(QStringLiteral("New 3D Pyramid Layer"), [this]() {
        auto *service = ArtifactProjectService::instance();
        if (!service) {
          return;
        }
        ArtifactFixedGeometry3DLayerInitParams params(QStringLiteral("3D Pyramid 1"),
                                                      FixedGeometry3D::Pyramid);
        service->addLayerToCurrentComposition(params);
        if (controller_) {
          controller_->markRenderDirty();
        }
      });
      add(QStringLiteral("New SVG Layer..."), [this]() {
        auto *service = ArtifactProjectService::instance();
        if (!service) {
          return;
        }
        const QString filePath = QFileDialog::getOpenFileName(
            this, QStringLiteral("SVGを選択"), QString(),
            QStringLiteral("SVG (*.svg);;All Files (*.*)"));
        if (filePath.isEmpty()) {
          return;
        }
        if (!filePath.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive)) {
          QMessageBox::warning(this, QStringLiteral("Layer"),
                               QStringLiteral("SVG ファイルを選択してください。"));
          return;
        }
        QSvgRenderer validator(filePath);
        if (!validator.isValid()) {
          QMessageBox::warning(this, QStringLiteral("Layer"),
                               QStringLiteral("SVG を読み込めませんでした。"));
          return;
        }
        const QString layerName =
            QFileInfo(filePath).completeBaseName().isEmpty()
                ? QStringLiteral("SVG 1")
                : QFileInfo(filePath).completeBaseName();
        service->importAssetsFromPathsAsync(QStringList{filePath},
                                           [this, service, layerName, filePath](QStringList importedPaths) {
                                             if (!service || importedPaths.isEmpty()) {
                                               return;
                                             }
                                             ArtifactSvgInitParams params(layerName);
                                             params.setSvgPath(importedPaths.first());
                                             service->addLayerToCurrentComposition(params);
                                             if (controller_) {
                                               controller_->markRenderDirty();
                                             }
                                           });
      });
      add(QStringLiteral("New Image Layer..."), [this]() {
        auto *service = ArtifactProjectService::instance();
        if (!service) {
          return;
        }
        const QString filePath = QFileDialog::getOpenFileName(
            this, QStringLiteral("画像を選択"), QString(),
            QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.tif *.tiff);;All Files (*.*)"));
        if (filePath.isEmpty()) {
          return;
        }
        QImageReader reader(filePath);
        if (!reader.canRead()) {
          QMessageBox::warning(this, QStringLiteral("Layer"),
                               QStringLiteral("画像を読み込めませんでした。"));
          return;
        }
        const QString layerName =
            QFileInfo(filePath).completeBaseName().isEmpty()
                ? QStringLiteral("Image 1")
                : QFileInfo(filePath).completeBaseName();
        service->importAssetsFromPathsAsync(QStringList{filePath},
                                           [this, service, layerName, filePath](QStringList importedPaths) {
                                             if (!service || importedPaths.isEmpty()) {
                                               return;
                                             }
                                             ArtifactImageInitParams params(layerName);
                                             params.setImagePath(importedPaths.first());
                                             service->addLayerToCurrentComposition(params);
                                             if (controller_) {
                                               controller_->markRenderDirty();
                                             }
                                           });
      });
      addSeparator();
      const bool hasSelectedLayer = !controller_->selectedLayerId().isNil();
      add(QStringLiteral("Focus Selected Layer"), [this]() {
          if (controller_) {
            controller_->focusSelectedLayer();
          }
        }, hasSelectedLayer);
      addSeparator();
      add(QStringLiteral("Reset View"), [this]() {
        if (controller_) controller_->resetView();
      });
      add(QStringLiteral("Zoom Fit"), [this]() {
        if (controller_) controller_->zoomFit();
      });
      add(QStringLiteral("Zoom Fit Selection"), [this]() {
        if (controller_) controller_->zoomFitSelection();
      }, hasSelectedLayer);
      add(QStringLiteral("Zoom Fit Visible"), [this]() {
        if (controller_) controller_->zoomFitVisible();
      });
      add(QStringLiteral("Zoom Fit Work Area"), [this]() {
        if (controller_) controller_->zoomFitWorkArea();
      });
      add(QStringLiteral("Zoom 100%"), [this]() {
        if (controller_) controller_->zoom100();
      });
      addSeparator();
      add(QStringLiteral("Paste Layers Here"), pasteLayersHere,
          clipboardHasLayerData);
      addSeparator();
      add(QStringLiteral("Command Palette"), [this]() { showCommandPalette(); });
    }

    // Tracker context menu (when TrackPoint tool is active)
    {
      const auto *app = ArtifactApplicationManager::instance();
      const auto *tm = app ? app->toolManager() : nullptr;
      if (tm && tm->activeTool() == ToolType::TrackPoint) {
        addSeparator();
        add(QStringLiteral("Track Forward"),
            [this, ctrl = controller_]() {
              if (ctrl) ctrl->trackerTrackForward();
            });
        add(QStringLiteral("Track Backward"),
            [this, ctrl = controller_]() {
              if (ctrl) ctrl->trackerTrackBackward();
            });
        add(QStringLiteral("Track All"),
            [this, ctrl = controller_]() {
              if (ctrl) ctrl->trackerTrackAll();
            });
        add(QStringLiteral("Use Planar Tracker"),
            [this, ctrl = controller_]() {
              if (ctrl) ctrl->trackerUsePlanarMode();
            });
        addSeparator();
        add(QStringLiteral("Apply to Layer Position"),
            [this, ctrl = controller_]() {
              if (ctrl) ctrl->trackerApplyToPosition();
            });
        add(QStringLiteral("Apply to Layer Anchor"),
            [this, ctrl = controller_]() {
              if (ctrl) ctrl->trackerApplyToAnchor();
            });
        add(QStringLiteral("Apply All Points to Null Layers"),
            [this, ctrl = controller_]() {
              if (ctrl) ctrl->trackerApplyAllPoints();
            });
        add(QStringLiteral("Apply Planar Track as Corner Pin"),
            [this, ctrl = controller_]() {
              if (ctrl) ctrl->trackerApplyPlanarCornerPin();
            });
        addSeparator();
        add(QStringLiteral("Delete Tracker"),
            [this, ctrl = controller_]() {
              if (ctrl) ctrl->trackerDelete();
            });
      }
    }

    viewportOverlayActions_ = actions;
    viewportOverlayEnabledStates_ = enabledStates;
    controller_->showContextMenuOverlay(viewportPos, items, title, subtitle,
                                        enabledStates);
  }

  void updateViewportCursor(const QPointF &pos) {
    if (!controller_ || spacePressed_) {
      return;
    }
    auto *app = ArtifactApplicationManager::instance();
    auto *toolManager = app ? app->toolManager() : nullptr;
    const ToolType activeTool =
        toolManager ? toolManager->activeTool() : ToolType::Selection;
    const Qt::CursorShape cursorShape =
        controller_->cursorShapeForViewportPos(pos);
    if (activeTool == ToolType::Pen && cursorShape == Qt::CrossCursor) {
      setCursor(makeMaskAddCursor());
      return;
    }
    setCursor(cursorShape);
  }

  void enqueueDroppedAssets(const QStringList &paths,
                            const QStringList &importedPaths) {
    ArtifactCore::FileTypeDetector detector;
    // インポート済みリストは連番シーケンスが代表パスへ集約済みのため、
    // そちらを正として列挙する（元パス列挙だとシーケンスが再展開される）。
    const QStringList &effectivePaths =
        importedPaths.isEmpty() ? paths : importedPaths;
    for (const QString &path : effectivePaths) {
      const QFileInfo fi(path);
      PendingDroppedAsset asset;
      asset.originalPath = path;
      asset.layerName = fi.completeBaseName();
      asset.svgShapeFile = isSvgShapeFile(path);
      asset.importedPath =
          resolveImportedAssetPathForSource(path, importedPaths);
      asset.fileType = asset.svgShapeFile ? ArtifactCore::FileType::Image
                                          : detector.detectByExtension(path);
      pendingDroppedAssets_.push_back(asset);
    }

    if (!pendingDropTimer_) {
      pendingDropTimer_ = new QTimer(this);
      pendingDropTimer_->setSingleShot(true);
      QObject::connect(pendingDropTimer_, &QTimer::timeout, this,
                       [this]() { processPendingDroppedAssets(); });
    }
    if (!pendingDropTimer_->isActive()) {
      pendingDropTimer_->start(0);
    }
  }

  void importDroppedPaths(const QStringList &paths) {
    auto *svc = ArtifactProjectService::instance();
    if (!svc || paths.isEmpty()) {
      return;
    }
    QPointer<CompositionViewport> self(this);
    svc->importAssetsFromPathsAsync(
        paths, [self, paths](const QStringList &importedPaths) mutable {
          if (self) {
            self->enqueueDroppedAssets(paths, importedPaths);
          }
        });
  }

  void importDroppedTemplate(const QString &fileName) {
    if (fileName.isEmpty()) return;
    auto *service = ArtifactProjectService::instance();
    const auto composition = service ? service->currentComposition().lock()
                                     : ArtifactCompositionPtr{};
    if (!composition) return;
    ArtifactTemplateLibrary library;
    QString error;
    const auto document = library.load(fileName, &error);
    if (document.name.isEmpty() && document.layerSnapshots.isEmpty()) return;
    auto *undoManager = UndoManager::instance();
    if (!undoManager) {
      document.appendToComposition(*composition);
      return;
    }
    const auto layers = document.instantiateLayers();
    auto macro = std::make_unique<MacroUndoCommand>(
        QStringLiteral("Import Template"));
    bool hasImportableLayers = false;
    for (auto it = layers.crbegin(); it != layers.crend(); ++it) {
      if (*it) {
        hasImportableLayers = true;
        macro->addChild(std::make_unique<AddLayerCommand>(
            composition, *it, true));
      }
    }
    if (!hasImportableLayers) {
      return;
    }
    if (!undoManager->push(std::move(macro))) {
      return;
    }
  }

  void processPendingDroppedAssets() {
    if (processingDroppedAssets_) {
      return;
    }

    auto *svc = ArtifactProjectService::instance();
    if (!svc || pendingDroppedAssets_.empty()) {
      pendingDroppedAssets_.clear();
      processingDroppedAssets_ = false;
      return;
    }

    processingDroppedAssets_ = true;
    constexpr int kAssetsPerTick = 2;
    int processed = 0;

    while (processed < kAssetsPerTick && !pendingDroppedAssets_.empty()) {
      const PendingDroppedAsset asset = pendingDroppedAssets_.front();
      pendingDroppedAssets_.pop_front();
      ++processed;
      const bool shouldSelectThisLayer = pendingDroppedAssets_.empty();

      using FT = ArtifactCore::FileType;
      if (asset.svgShapeFile) {
        ArtifactSvgInitParams params(asset.layerName);
        params.setSvgPath(asset.importedPath);
        svc->addLayerToCurrentComposition(params, shouldSelectThisLayer);
      } else if (asset.fileType == FT::Image) {
        ArtifactImageInitParams params(asset.layerName);
        params.setImagePath(asset.importedPath);
        svc->addLayerToCurrentComposition(params, shouldSelectThisLayer);
      } else if (asset.fileType == FT::Audio) {
        ArtifactAudioInitParams params(asset.layerName);
        params.setAudioPath(asset.importedPath);
        svc->addLayerToCurrentComposition(params, shouldSelectThisLayer);
      } else if (asset.fileType == FT::Video) {
        ArtifactVideoInitParams params(asset.layerName);
        params.setVideoPath(asset.importedPath);
        svc->addLayerToCurrentComposition(params, shouldSelectThisLayer);
      } else if (asset.fileType == FT::Model3D) {
        ArtifactModel3DLayerInitParams params(asset.layerName);
        params.setModelPath(asset.importedPath);
        svc->addLayerToCurrentComposition(params, shouldSelectThisLayer);
      } else {
        ArtifactImageInitParams params(asset.layerName);
        params.setImagePath(asset.importedPath);
        svc->addLayerToCurrentComposition(params, shouldSelectThisLayer);
      }
    }

    processingDroppedAssets_ = false;
    if (!pendingDroppedAssets_.empty() && pendingDropTimer_) {
      pendingDropTimer_->start(0);
    }
  }

protected:
  void showEvent(QShowEvent *event) override {
    QWidget::showEvent(event);
    qInfo() << "[CompositionEditor][ShowEvent]"
            << "visible=" << isVisible()
            << "minimized=" << window()->isMinimized()
            << "winId=" << static_cast<quintptr>(winId())
            << "controller=" << controller_;
    if (controller_) {
      scheduleViewportReadinessCheck(QStringLiteral("show-event"), 16);
      if (!controller_->isInitialized()) {
        scheduleViewportInitializationRetry();
      }
    }
  }

  void paintEvent(QPaintEvent *) override {
    // Rendering is driven by QTimer in the controller.
    // With WA_PaintOnScreen the backing store is bypassed.
    // Ghost overlays are now composited in the Diligent render pass.
  }

  // --- D&D: アセットブラウザ → コンポジションエディタ ---
  void dragEnterEvent(QDragEnterEvent *event) override {
    if (event->mimeData()->hasFormat("application/x-artifact-template")) {
      event->acceptProposedAction();
      return;
    }
    if (event->mimeData()->hasUrls()) {
      const auto urls = event->mimeData()->urls();
      // フォルダは弾く
      for (const auto &url : urls) {
        if (url.isLocalFile() && !QFileInfo(url.toLocalFile()).isDir()) {
          event->acceptProposedAction();
          dropOverlayVisible_ = true;
          updateDropLabel(urls);
          updateDropPreview(urls, event->position());
          return;
        }
      }
    }
    event->ignore();
  }

  void dragMoveEvent(QDragMoveEvent *event) override {
    if (event->mimeData()->hasFormat("application/x-artifact-template")) {
      event->acceptProposedAction();
      return;
    }
    if (dropOverlayVisible_) {
      updateDropPreview(event->mimeData()->urls(), event->position());
      event->acceptProposedAction();
    } else {
      event->ignore();
    }
  }

  void dragLeaveEvent(QDragLeaveEvent *event) override {
    clearDropPreview();
    QWidget::dragLeaveEvent(event);
  }

  void dropEvent(QDropEvent *event) override {
    clearDropPreview();

    if (event->mimeData()->hasFormat("application/x-artifact-template")) {
      importDroppedTemplate(QString::fromUtf8(
          event->mimeData()->data("application/x-artifact-template")));
      event->acceptProposedAction();
      return;
    }

    if (!event->mimeData()->hasUrls()) {
      event->ignore();
      return;
    }

    const auto urls = event->mimeData()->urls();
    QStringList paths;
    for (const auto &url : urls) {
      if (!url.isLocalFile())
        continue;
      const QString path = url.toLocalFile();
      const QFileInfo fi(path);
      if (!fi.exists() || fi.isDir())
        continue;
      paths.append(path);
    }

    if (paths.isEmpty()) {
      event->ignore();
      return;
    }

    importDroppedPaths(paths);
    event->acceptProposedAction();
  }

  void hideEvent(QHideEvent *event) override {
    restoreTemporarySolo();
    restoreTemporaryPlayback();
    const bool controllerDragActive =
        isSpatialGizmoDragging() || nativeControllerDragActive_;
    isPanning_ = false;
    isPanningWithMiddle_ = false;
    isAltOrbiting_ = false;
    isAltZooming_ = false;
    nativePointerCaptureActive_ = false;
    nativeControllerDragActive_ = false;
    panMomentumActive_ = false;
    panVelocityPerMs_ = {};
    if (controller_) {
      const bool gizmoCancelled = controller_->cancelGizmoInteraction();
      if (controllerDragActive && !gizmoCancelled) {
        controller_->handleMouseRelease();
      }
      controller_->finishViewportInteraction();
    }
    clearNavigationFeedback();
    if (controller_) {
      controller_->stop();
    }
    QWidget::hideEvent(event);
  }

  void focusOutEvent(QFocusEvent *event) override {
    restoreTemporarySolo();
    restoreTemporaryPlayback();
    const bool controllerDragActive =
        isSpatialGizmoDragging() || nativeControllerDragActive_;
    isPanning_ = false;
    isPanningWithMiddle_ = false;
    isAltOrbiting_ = false;
    isAltZooming_ = false;
    nativePointerCaptureActive_ = false;
    nativeControllerDragActive_ = false;
    panMomentumActive_ = false;
    panVelocityPerMs_ = {};
    if (controller_) {
      const bool gizmoCancelled = controller_->cancelGizmoInteraction();
      if (controllerDragActive && !gizmoCancelled) {
        controller_->handleMouseRelease();
      }
      controller_->finishViewportInteraction();
    }
    clearNavigationFeedback();
    QWidget::focusOutEvent(event);
  }

  void resizeEvent(QResizeEvent *event) override {
    QWidget::resizeEvent(event);
    if (resizeCallback_) {
      resizeCallback_();
    }
    if (!controller_) {
      return;
    }

    if (!controller_->isInitialized()) {
      pendingResizeSize_ = event->size();
      resizePending_ = true;
      scheduleViewportReadinessCheck(QStringLiteral("resize-uninitialized"), 32);
      return;
    }

    if (controller_->isInitialized()) {
      controller_->setViewportSize(static_cast<float>(event->size().width()),
                                   static_cast<float>(event->size().height()));
      pendingResizeSize_ = event->size();
      resizePending_ = true;
      if (resizeDebounceTimer_) {
        resizeDebounceTimer_->stop();
        resizeDebounceTimer_->start(32);
      }
      // Render is deferred to the debounce timer — no immediate
      // renderOneFrame() to avoid redundant work during continuous resize.
    }
  }

  bool event(QEvent *event) override {
    if (event &&
        (event->type() == QEvent::WindowDeactivate ||
         event->type() == QEvent::ApplicationDeactivate ||
         event->type() == QEvent::Hide ||
         event->type() == QEvent::UngrabMouse) &&
        (isPanning_ || isAltOrbiting_ || isAltZooming_ ||
         isSpatialGizmoDragging() || nativePointerCaptureActive_ ||
         nativeControllerDragActive_)) {
      const bool controllerDragActive =
          isSpatialGizmoDragging() || nativeControllerDragActive_;
      isPanning_ = false;
      isPanningWithMiddle_ = false;
      isAltOrbiting_ = false;
      isAltZooming_ = false;
      nativePointerCaptureActive_ = false;
      nativeControllerDragActive_ = false;
      panMomentumActive_ = false;
      panVelocityPerMs_ = {};
      clearNavigationFeedback();
      if (didSpacePan_) {
        ArtifactAudioScrubController::instance().stopScrub();
        didSpacePan_ = false;
      }
      if (controller_) {
        const bool gizmoCancelled = controller_->cancelGizmoInteraction();
        if (controllerDragActive && !gizmoCancelled) {
          controller_->handleMouseRelease();
        }
        controller_->finishViewportInteraction();
      }
      if (event->type() != QEvent::UngrabMouse &&
          QWidget::mouseGrabber() == this) {
        releaseMouse();
      }
#ifdef Q_OS_WIN
      if (GetCapture() == reinterpret_cast<HWND>(winId())) {
        ReleaseCapture();
      }
#endif
      unsetCursor();
    }
    const bool handled = QWidget::event(event);

    if (!controller_ || !event) {
      return handled;
    }

    switch (event->type()) {
    case QEvent::Show:
      scheduleViewportReadinessCheck(QStringLiteral("event-show"), 16);
      break;
    case QEvent::WinIdChange:
      resetSwapChainReadinessTracking();
      scheduleViewportReadinessCheck(QStringLiteral("event-winid-change"), 0);
      break;
    case QEvent::PlatformSurface:
      resetSwapChainReadinessTracking();
      scheduleViewportReadinessCheck(QStringLiteral("event-platform-surface"),
                                     16);
      break;
    case QEvent::ActivationChange:
      if (window() && window()->isActiveWindow()) {
        scheduleViewportReadinessCheck(QStringLiteral("event-activation"), 0);
      }
      break;
    case QEvent::WindowStateChange:
      if (!window() || !window()->isMinimized()) {
        scheduleViewportReadinessCheck(QStringLiteral("event-window-state"), 16);
      }
      break;
    default:
      break;
    }

    return handled;
  }

  void enterEvent(QEnterEvent *event) override {
    QWidget::enterEvent(event);
  }

  void focusInEvent(QFocusEvent *event) override {
    if (activatedCallback_) {
      activatedCallback_();
    }
    QWidget::focusInEvent(event);
  }

  void wheelEvent(QWheelEvent *event) override {
    if (controller_ && controller_->isPieMenuOverlayVisible())
      return; // Block while menu open

    if (!controller_) {
      return;
    }

    panMomentumActive_ = false;
    panVelocityPerMs_ = {};

    if (activatedCallback_) {
      activatedCallback_();
    }

    controller_->notifyViewportInteractionActivity();

    const auto modifiers = event->modifiers();
    const QPointF angleDelta = event->angleDelta();

    if (angleDelta.y() != 0.0 && !modifiers.testFlag(Qt::ShiftModifier) &&
        !modifiers.testFlag(Qt::AltModifier) &&
        !modifiers.testFlag(Qt::ControlModifier)) {
      if (auto *toolManager = ArtifactApplicationManager::instance()
                                  ? ArtifactApplicationManager::instance()->toolManager()
                                  : nullptr;
          toolManager && toolManager->activeTool() == ToolType::Rectangle) {
        const float delta = angleDelta.y() > 0.0 ? 2.0f : -2.0f;
        if (controller_->adjustRectangleToolRoundness(delta)) {
          event->accept();
          return;
        }
        if (toolManager &&
            (toolManager->activeTool() == ToolType::Rectangle ||
             toolManager->activeTool() == ToolType::Shape) &&
            controller_->adjustSelectedShapeCornerRadius(delta)) {
          event->accept();
          return;
        }
      }
    }

    const auto *toolManager = ArtifactApplicationManager::instance()
                                  ? ArtifactApplicationManager::instance()
                                        ->toolManager()
                                  : nullptr;
    if (toolManager && toolManager->activeTool() == ToolType::Puppet &&
        !modifiers.testFlag(Qt::ShiftModifier) &&
        !modifiers.testFlag(Qt::AltModifier) &&
        !modifiers.testFlag(Qt::ControlModifier) && angleDelta.y() != 0.0) {
      const float weightDelta = angleDelta.y() > 0.0 ? 0.05f : -0.05f;
      if (controller_->adjustSelectedPuppetPinWeightAt(event->position(),
                                                        weightDelta)) {
        event->accept();
        return;
      }
      const float depthDelta = angleDelta.y() > 0.0 ? 0.05f : -0.05f;
      if (controller_->adjustSelectedPuppetPinDepthAt(event->position(),
                                                       depthDelta)) {
        event->accept();
        return;
      }
    }

    if (modifiers.testFlag(Qt::AltModifier) ||
        modifiers.testFlag(Qt::ControlModifier)) {
      // AE Style: Alt/Ctrl + Wheel = Zoom
      if (angleDelta.y() > 0) {
        controller_->zoomInAt(event->position());
      } else if (angleDelta.y() < 0) {
        controller_->zoomOutAt(event->position());
      }
    } else if (modifiers.testFlag(Qt::ShiftModifier)) {
      // AE Style: Shift + Wheel = Horizontal Pan
      // Prefer a real horizontal tilt delta; use vertical wheel input only
      // as a fallback for mice without a horizontal wheel axis.
      const float deltaX = angleDelta.x() != 0.0
          ? static_cast<float>(angleDelta.x())
          : static_cast<float>(angleDelta.y());
      controller_->panBy(QPointF(deltaX, 0));
    } else {
      // Wheel without modifier = Zoom (industry-standard default).
      // Previously this panned vertically (AE style), but users expect
      // plain scroll to zoom in composition editors.
      if (angleDelta.y() > 0) {
        controller_->zoomInAt(event->position());
      } else if (angleDelta.y() < 0) {
        controller_->zoomOutAt(event->position());
      }
    }

    setNavigationFeedback(
        modifiers.testFlag(Qt::ShiftModifier)
            ? NavigationFeedbackMode::Pan
            : NavigationFeedbackMode::Zoom,
        true);
    event->accept();
  }

  void mouseDoubleClickEvent(QMouseEvent *event) override {
    if (controller_) {
      if (controller_->resetHoveredMaskTangent()) {
        event->accept();
        return;
      }
      if (controller_->resetProjectedFrameHandleAt(event->position())) {
        event->accept();
        return;
      }
      const auto *toolManager = ArtifactApplicationManager::instance()
                                    ? ArtifactApplicationManager::instance()
                                          ->toolManager()
                                    : nullptr;
      if (toolManager &&
          (toolManager->activeTool() == ToolType::Rectangle ||
           toolManager->activeTool() == ToolType::Shape) &&
          controller_->resetSelectedShapeCornerRadius()) {
        event->accept();
        return;
      }
      if (toolManager && toolManager->activeTool() == ToolType::Puppet &&
          controller_->resetSelectedPuppetPinRotation()) {
        event->accept();
        return;
      }
      const bool anchorToolActive =
          toolManager && toolManager->activeTool() == ToolType::AnchorPoint;
      if (event->modifiers().testFlag(Qt::ControlModifier) &&
          anchorToolActive &&
          (controller_->resetSelected3DAnchorToCenter() ||
           controller_->resetSelected2DAnchorToCenter())) {
        event->accept();
        return;
      }
      const auto layerId = controller_->layerAtViewportPos(event->position());
      if (!layerId.isNil()) {
        if (const auto comp = currentComposition()) {
          if (auto layer = comp->layerById(layerId)) {
            if (ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(layer)) {
              editTextLayerInline(this, layer, controller_);
              event->accept();
              return;
            }
          }
        }
      } else {
        controller_->resetView();
        event->accept();
        return;
      }
    }
    event->accept();
  }

  void contextMenuEvent(QContextMenuEvent *event) override {
    if (controller_) {
      controller_->handleMouseMove(event->pos());
      QMenu maskMenu(this);
      maskMenu.setTitle(QStringLiteral("Mask Mode"));
      const std::array<std::pair<const char *, int>, 4> modes = {{
          {"Add", 0}, {"Subtract", 1}, {"Intersect", 2}, {"Difference", 3}}};
      bool added = controller_->hasHoveredMaskPath();
      const bool hasVertex = controller_->hasHoveredMaskVertex();
      if (added) for (const auto &[label, mode] : modes) {
        QAction *action = maskMenu.addAction(QString::fromUtf8(label));
        action->setProperty("artifactMaskMode", mode);
      }
      if (added) {
        maskMenu.addSeparator();
        if (hasVertex) {
          QMenu *vertexMenu = maskMenu.addMenu(QStringLiteral("Vertex Type"));
          const std::array<std::pair<const char *, int>, 4> vertexTypes = {{
              {"Corner", 0}, {"Smooth", 1}, {"Continuous", 2},
              {"Broken", 3}}};
          for (const auto &[label, type] : vertexTypes) {
            QAction *action = vertexMenu->addAction(QString::fromUtf8(label));
            action->setProperty("artifactMaskVertexType", type);
          }
        }
        QAction *toggle = maskMenu.addAction(QStringLiteral("Toggle Enabled"));
        toggle->setProperty("artifactMaskAction", QStringLiteral("toggle"));
        QAction *toggleSelected = maskMenu.addAction(
            QStringLiteral("Toggle Enabled on Selected Layers"));
        toggleSelected->setProperty("artifactMaskAction",
                                    QStringLiteral("toggleSelected"));
        QAction *remove = maskMenu.addAction(QStringLiteral("Delete Mask"));
        remove->setProperty("artifactMaskAction", QStringLiteral("delete"));
        QAction *removeSelected = maskMenu.addAction(
            QStringLiteral("Delete Mask on Selected Layers"));
        removeSelected->setProperty("artifactMaskAction",
                                    QStringLiteral("deleteSelected"));
        QAction *lock = maskMenu.addAction(QStringLiteral("Toggle Locked"));
        lock->setProperty("artifactMaskAction", QStringLiteral("lock"));
        QAction *colorPicker = maskMenu.addAction(QStringLiteral("Change Mask Color…"));
        colorPicker->setProperty("artifactMaskAction", QStringLiteral("colorPicker"));
        QAction *duplicate = maskMenu.addAction(QStringLiteral("Duplicate Mask"));
        duplicate->setProperty("artifactMaskAction", QStringLiteral("duplicate"));
        QAction *duplicateSelected = maskMenu.addAction(
            QStringLiteral("Duplicate Mask on Selected Layers"));
        duplicateSelected->setProperty("artifactMaskAction",
                                       QStringLiteral("duplicateSelected"));
        QAction *moveUp = maskMenu.addAction(QStringLiteral("Move Up"));
        moveUp->setProperty("artifactMaskAction", QStringLiteral("up"));
        QAction *moveUpSelected = maskMenu.addAction(
            QStringLiteral("Move Up on Selected Layers"));
        moveUpSelected->setProperty("artifactMaskAction",
                                    QStringLiteral("upSelected"));
        QAction *moveDown = maskMenu.addAction(QStringLiteral("Move Down"));
        moveDown->setProperty("artifactMaskAction", QStringLiteral("down"));
        QAction *moveDownSelected = maskMenu.addAction(
            QStringLiteral("Move Down on Selected Layers"));
        moveDownSelected->setProperty("artifactMaskAction",
                                      QStringLiteral("downSelected"));
        QAction *copy = maskMenu.addAction(QStringLiteral("Copy Mask"));
        copy->setProperty("artifactMaskAction", QStringLiteral("copy"));
        QAction *paste = maskMenu.addAction(QStringLiteral("Paste Mask"));
        paste->setProperty("artifactMaskAction", QStringLiteral("paste"));
        QAction *invert = maskMenu.addAction(QStringLiteral("Toggle Inverted"));
        invert->setProperty("artifactMaskAction", QStringLiteral("invert"));
        QAction *invertSelected = maskMenu.addAction(
            QStringLiteral("Toggle Inverted on Selected Layers"));
        invertSelected->setProperty("artifactMaskAction",
                                    QStringLiteral("invertSelected"));
        QAction *featherIn = maskMenu.addAction(QStringLiteral("Increase Feather"));
        featherIn->setProperty("artifactMaskAction", QStringLiteral("featherIn"));
        QAction *featherInSelected = maskMenu.addAction(
            QStringLiteral("Increase Feather on Selected Layers"));
        featherInSelected->setProperty("artifactMaskAction",
                                       QStringLiteral("featherInSelected"));
        QAction *featherOut = maskMenu.addAction(QStringLiteral("Decrease Feather"));
        featherOut->setProperty("artifactMaskAction", QStringLiteral("featherOut"));
        QAction *featherOutSelected = maskMenu.addAction(
            QStringLiteral("Decrease Feather on Selected Layers"));
        featherOutSelected->setProperty("artifactMaskAction",
                                        QStringLiteral("featherOutSelected"));
        QAction *expand = maskMenu.addAction(QStringLiteral("Expand Mask"));
        expand->setProperty("artifactMaskAction", QStringLiteral("expand"));
        QAction *expandSelected = maskMenu.addAction(
            QStringLiteral("Expand Mask on Selected Layers"));
        expandSelected->setProperty("artifactMaskAction",
                                    QStringLiteral("expandSelected"));
        QAction *contract = maskMenu.addAction(QStringLiteral("Contract Mask"));
        contract->setProperty("artifactMaskAction", QStringLiteral("contract"));
        QAction *contractSelected = maskMenu.addAction(
            QStringLiteral("Contract Mask on Selected Layers"));
        contractSelected->setProperty("artifactMaskAction",
                                     QStringLiteral("contractSelected"));
        QAction *opacityIn = maskMenu.addAction(QStringLiteral("Increase Opacity"));
        opacityIn->setProperty("artifactMaskAction", QStringLiteral("opacityIn"));
        QAction *opacityInSelected = maskMenu.addAction(
            QStringLiteral("Increase Opacity on Selected Layers"));
        opacityInSelected->setProperty("artifactMaskAction",
                                       QStringLiteral("opacityInSelected"));
        QAction *opacityOut = maskMenu.addAction(QStringLiteral("Decrease Opacity"));
        opacityOut->setProperty("artifactMaskAction", QStringLiteral("opacityOut"));
        QAction *opacityOutSelected = maskMenu.addAction(
            QStringLiteral("Decrease Opacity on Selected Layers"));
        opacityOutSelected->setProperty("artifactMaskAction",
                                        QStringLiteral("opacityOutSelected"));
      }
      if (added) {
        QAction *chosen = maskMenu.exec(event->globalPos());
        if (chosen && controller_) {
          if (chosen->property("artifactMaskMode").isValid()) {
            controller_->setHoveredMaskMode(
                chosen->property("artifactMaskMode").toInt());
          } else if (chosen->property("artifactMaskVertexType").isValid()) {
            controller_->setHoveredMaskVertexType(
                chosen->property("artifactMaskVertexType").toInt());
          } else {
            const QString action =
                chosen->property("artifactMaskAction").toString();
            if (action == QStringLiteral("toggle")) {
              controller_->toggleHoveredMaskEnabled();
            } else if (action == QStringLiteral("toggleSelected")) {
              controller_->toggleHoveredMaskEnabledForSelectedLayers();
            } else if (action == QStringLiteral("delete")) {
              controller_->deleteHoveredMask();
            } else if (action == QStringLiteral("deleteSelected")) {
              controller_->deleteHoveredMaskForSelectedLayers();
            } else if (action == QStringLiteral("lock")) {
              controller_->toggleHoveredMaskLocked();
            } else if (action == QStringLiteral("colorPicker")) {
              if (auto *dialog = new ArtifactWidgets::FloatColorPicker(this)) {
                dialog->setWindowTitle(QStringLiteral("Mask Color"));
                dialog->exec();
                const auto c = dialog->getColor();
                controller_->setHoveredMaskColor(c);
                dialog->deleteLater();
              }
            } else if (action == QStringLiteral("duplicate")) {
              controller_->duplicateHoveredMask();
            } else if (action == QStringLiteral("duplicateSelected")) {
              controller_->duplicateHoveredMaskForSelectedLayers();
            } else if (action == QStringLiteral("up")) {
              controller_->moveHoveredMask(-1);
            } else if (action == QStringLiteral("upSelected")) {
              controller_->moveHoveredMaskForSelectedLayers(-1);
            } else if (action == QStringLiteral("down")) {
              controller_->moveHoveredMask(1);
            } else if (action == QStringLiteral("downSelected")) {
              controller_->moveHoveredMaskForSelectedLayers(1);
            } else if (action == QStringLiteral("copy")) {
              controller_->copyHoveredMask();
            } else if (action == QStringLiteral("paste")) {
              controller_->pasteMask();
            } else if (action == QStringLiteral("invert")) {
              controller_->toggleHoveredMaskInverted();
            } else if (action == QStringLiteral("invertSelected")) {
              controller_->toggleHoveredMaskInvertedForSelectedLayers();
            } else if (action == QStringLiteral("featherIn")) {
              controller_->adjustHoveredMaskGeometry(5.0f, 0.0f);
            } else if (action == QStringLiteral("featherInSelected")) {
              controller_->adjustHoveredMaskGeometryForSelectedLayers(5.0f, 0.0f);
            } else if (action == QStringLiteral("featherOut")) {
              controller_->adjustHoveredMaskGeometry(-5.0f, 0.0f);
            } else if (action == QStringLiteral("featherOutSelected")) {
              controller_->adjustHoveredMaskGeometryForSelectedLayers(-5.0f, 0.0f);
            } else if (action == QStringLiteral("expand")) {
              controller_->adjustHoveredMaskGeometry(0.0f, 5.0f);
            } else if (action == QStringLiteral("expandSelected")) {
              controller_->adjustHoveredMaskGeometryForSelectedLayers(0.0f, 5.0f);
            } else if (action == QStringLiteral("contract")) {
              controller_->adjustHoveredMaskGeometry(0.0f, -5.0f);
            } else if (action == QStringLiteral("contractSelected")) {
              controller_->adjustHoveredMaskGeometryForSelectedLayers(0.0f, -5.0f);
            } else if (action == QStringLiteral("opacityIn")) {
              controller_->adjustHoveredMaskOpacity(0.1f);
            } else if (action == QStringLiteral("opacityInSelected")) {
              controller_->adjustHoveredMaskOpacityForSelectedLayers(0.1f);
            } else if (action == QStringLiteral("opacityOut")) {
              controller_->adjustHoveredMaskOpacity(-0.1f);
            } else if (action == QStringLiteral("opacityOutSelected")) {
              controller_->adjustHoveredMaskOpacityForSelectedLayers(-0.1f);
            }
          }
        }
        event->accept();
        return;
      }
    }
    showViewportContextMenu(event->pos());
    event->accept();
  }

  void mousePressEvent(QMouseEvent *event) override {
    if (event && event->button() == Qt::LeftButton && controller_ &&
        !controller_->isModalGizmoInteractionActive()) {
      const int frameDimension =
          controller_->beginFrameSizeBadgeInput(event->position());
      if (frameDimension >= 0) {
        modalTransformNumericInput_ =
            frameDimension == 0 ? QStringLiteral("w") : QStringLiteral("h");
        updateViewportCursor(event->position());
        event->accept();
        return;
      }
    }
    if (event && event->button() == Qt::LeftButton && controller_ &&
        controller_->isModalGizmoInteractionActive()) {
      controller_->handleMouseMove(event->position());
      controller_->commitModalGizmoInteraction();
      modalTransformNumericInput_.clear();
      updateViewportCursor(event->position());
      event->accept();
      return;
    }
    if (event && event->button() == Qt::RightButton && controller_ &&
        controller_->cancelGizmoInteraction()) {
      modalTransformNumericInput_.clear();
      if (QWidget::mouseGrabber() == this) {
        releaseMouse();
      }
      clearNavigationFeedback();
      controller_->finishViewportInteraction();
      updateViewportCursor(event->position());
      event->accept();
      return;
    }
    if (activatedCallback_) {
      activatedCallback_();
    }
    // Any fresh pointer gesture takes ownership of the viewport and stops
    // residual momentum from a previous pan.
    panMomentumActive_ = false;
    panVelocityPerMs_ = {};

    if (controller_ && controller_->isPieMenuOverlayVisible()) {
      if (event->button() == Qt::LeftButton) {
        controller_->confirmPieMenuOverlaySelection();
      } else {
        controller_->cancelPieMenuOverlay();
      }
      event->accept();
      return;
    }

    if (controller_ && controller_->isViewportOverlayVisible()) {
      if (event->button() == Qt::LeftButton) {
        triggerViewportOverlayItem(event->position());
        event->accept();
        return;
      }
      if (event->button() == Qt::RightButton) {
        showViewportContextMenu(event->position());
        event->accept();
        return;
      }
      hideViewportOverlay();
    }

    qDebug() << "[VP] mousePressEvent button=" << event->button()
             << "middle=" << (event->button() == Qt::MiddleButton)
             << "space=" << spacePressed_
             << "alt=" << event->modifiers().testFlag(Qt::AltModifier)
             << "pos=" << event->position();

    if (event->button() == Qt::RightButton &&
        event->modifiers().testFlag(Qt::AltModifier)) {
      isAltZooming_ = true;
      setNavigationFeedback(NavigationFeedbackMode::Zoom);
      lastMousePos_ = event->position();
      panMomentumActive_ = false;
      panVelocityPerMs_ = {};
      lastPanSampleAt_ = std::chrono::steady_clock::now();
      grabMouse();
      if (controller_) {
        controller_->notifyViewportInteractionActivity();
      }
      setCursor(Qt::SizeVerCursor);
      event->accept();
      return;
    }

    if (event->button() == Qt::LeftButton &&
        event->modifiers().testFlag(Qt::AltModifier) &&
        !event->modifiers().testFlag(Qt::ControlModifier) && controller_ &&
        !controller_->isTransformGizmoHovered(event->position()) &&
        !maskNavigationLocked() &&
        !(ArtifactApplicationManager::instance() &&
          ArtifactApplicationManager::instance()->toolManager() &&
          ArtifactApplicationManager::instance()->toolManager()->activeTool() ==
              ToolType::Clone)) {
      isAltOrbiting_ = true;
      setNavigationFeedback(NavigationFeedbackMode::Orbit);
      orbitDragStartPos_ = event->position();
      orbitDragStartOrientation_ =
          controller_->viewportOrientationQuaternion();
      grabMouse();
      controller_->notifyViewportInteractionActivity();
      setCursor(Qt::SizeAllCursor);
      event->accept();
      return;
    }

    if (event->button() == Qt::LeftButton &&
        event->modifiers().testFlag(Qt::ControlModifier) &&
        event->modifiers().testFlag(Qt::AltModifier) && controller_) {
      controller_->placeWorkCursorAtViewportPos(event->position());
      event->accept();
      return;
    }

    if (event->button() == Qt::MiddleButton ||
        (event->button() == Qt::LeftButton && spacePressed_)) {
      isPanning_ = true;
      setNavigationFeedback(NavigationFeedbackMode::Pan);
      isPanningWithMiddle_ = (event->button() == Qt::MiddleButton);
      if (spacePressed_) {
        didSpacePan_ = true;
      }
      lastMousePos_ = event->position();
      if (controller_) {
        controller_->notifyViewportInteractionActivity();
      }
      setCursor(Qt::ClosedHandCursor);
      qDebug() << "[VP] panning started, isPanning_=" << isPanning_;

      if (didSpacePan_) {
        auto& scrubCtrl = ArtifactAudioScrubController::instance();
        scrubCtrl.setEnabled(true);
        if (auto* playback = ArtifactPlaybackService::instance()) {
          scrubCtrl.setComposition(
              static_cast<ArtifactCompositionPtr>(playback->currentComposition().lock()));
        }
        scrubCtrl.startScrub();
      }

      event->accept();
      return;
    }

    if (controller_ && !spacePressed_) {
      controller_->handleMousePress(event);
      if (isSpatialGizmoDragging()) {
        if (QWidget::mouseGrabber() != this) {
          grabMouse();
        }
        updateViewportCursor(event->position());
        if (overlayWidget_) {
          overlayWidget_->update();
        }
        event->accept();
        return;
      }
    }
    QWidget::mousePressEvent(event);
  }

  void mouseMoveEvent(QMouseEvent *event) override {
    if (controller_ && controller_->isPieMenuOverlayVisible()) {
      controller_->updatePieMenuOverlayMousePos(event->position());
      event->accept();
      return;
    }
    if (controller_ && controller_->isContextMenuOverlayVisible()) {
      controller_->updateContextMenuOverlayMousePos(event->position());
      event->accept();
      return;
    }

    // Recover isPanning_ state if grabMouse() didn't work on WA_NativeWindow
    if (!isPanning_ && (event->buttons() & Qt::MiddleButton) && controller_) {
      qDebug() << "[VP] mouseMoveEvent recovering pan, buttons=" << event->buttons();
      isPanning_ = true;
      setNavigationFeedback(NavigationFeedbackMode::Pan);
      isPanningWithMiddle_ = true;
      lastMousePos_ = event->position();
    }
    if (isAltOrbiting_ && controller_) {
      const QPointF delta = event->position() - orbitDragStartPos_;
      controller_->notifyViewportInteractionActivity();
      const float yawDelta = static_cast<float>(delta.x()) * 0.55f;
      const float pitchDelta = static_cast<float>(-delta.y()) * 0.55f;
      const QQuaternion yaw =
          QQuaternion::fromAxisAndAngle(0.0f, 1.0f, 0.0f, yawDelta);
      const QVector3D localRight =
          orbitDragStartOrientation_.rotatedVector(QVector3D(1.0f, 0.0f, 0.0f));
      const QQuaternion pitch =
          QQuaternion::fromAxisAndAngle(localRight, pitchDelta);
      controller_->setViewportOrientationQuaternion(
          (pitch * yaw * orbitDragStartOrientation_).normalized());
      if (viewportOrientationChangedCallback_) {
        viewportOrientationChangedCallback_(
            controller_->viewportOrientationQuaternion());
      }
      if (overlayWidget_) {
        overlayWidget_->update();
      }
      event->accept();
      return;
    }
    if (isAltZooming_ && controller_) {
      const QPointF delta = event->position() - lastMousePos_;
      lastMousePos_ = event->position();
      controller_->notifyViewportInteractionActivity();
      const float zoomDelta = static_cast<float>(-delta.y()) * 0.01f;
      const float factor = std::exp(std::clamp(zoomDelta, -0.35f, 0.35f));
      controller_->zoomAtFactor(event->position(), factor);
      event->accept();
      return;
    }
    if (isPanning_ && controller_) {
      const QPointF delta = event->position() - lastMousePos_;
      lastMousePos_ = event->position();
      const auto now = std::chrono::steady_clock::now();
      const auto elapsedMs = std::max<int64_t>(
          1, std::chrono::duration_cast<std::chrono::milliseconds>(
                 now - lastPanSampleAt_).count());
      panVelocityPerMs_ = delta / static_cast<double>(elapsedMs);
      panVelocityPerMs_.setX(std::clamp(panVelocityPerMs_.x(), -3.0, 3.0));
      panVelocityPerMs_.setY(std::clamp(panVelocityPerMs_.y(), -3.0, 3.0));
      lastPanSampleAt_ = now;
      controller_->notifyViewportInteractionActivity();
      controller_->panBy(delta);
      qDebug() << "[VP] panning, delta=" << delta;

      if (didSpacePan_) {
        if (auto* playback = ArtifactPlaybackService::instance()) {
          ArtifactAudioScrubController::instance().updateScrubPosition(
              playback->currentFrame());
        }
      }

      event->accept();
      return;
    }

    if (controller_) {
      controller_->handleMouseMove(event->position());
      if (isSpatialGizmoDragging()) {
        // Phase 3: Use fixed-rate render tick instead of singleShot(16) + renderOneFrame().
        controller_->markRenderDirty();
        updateViewportCursor(event->position());
        event->accept();
        return;
      }
      if (spacePressed_) {
        setCursor(Qt::OpenHandCursor);
      } else {
        updateViewportCursor(event->position());
      }
    }

    QWidget::mouseMoveEvent(event);
  }

  void mouseReleaseEvent(QMouseEvent *event) override {
    if (controller_ && controller_->isPieMenuOverlayVisible()) {
      if (event->button() != Qt::LeftButton) {
        controller_->cancelPieMenuOverlay();
      }
      event->accept();
      return;
    }

    if (isPanning_ &&
        ((isPanningWithMiddle_ && event->button() == Qt::MiddleButton) ||
         (!isPanningWithMiddle_ && event->button() == Qt::LeftButton))) {
      isPanning_ = false;
      isPanningWithMiddle_ = false;
      if (std::hypot(panVelocityPerMs_.x(), panVelocityPerMs_.y()) > 0.05) {
        panMomentumActive_ = true;
        auto momentumStep = std::make_shared<std::function<void()>>();
        const std::weak_ptr<std::function<void()>> weakMomentum = momentumStep;
        *momentumStep = [this, weakMomentum]() {
          if (!controller_ || !panMomentumActive_) return;
          controller_->notifyViewportInteractionActivity();
          controller_->panBy(panVelocityPerMs_ * 16.0);
          panVelocityPerMs_ *= 0.86;
          if (std::hypot(panVelocityPerMs_.x(), panVelocityPerMs_.y()) < 0.015) {
            panVelocityPerMs_ = {};
            panMomentumActive_ = false;
            clearNavigationFeedback();
            controller_->finishViewportInteraction();
            return;
          }
          if (const auto next = weakMomentum.lock()) {
            QTimer::singleShot(16, this, [next]() { (*next)(); });
          }
        };
        QTimer::singleShot(16, this, [momentumStep]() { (*momentumStep)(); });
      }
      if (!panMomentumActive_) {
        clearNavigationFeedback();
      }
      if (controller_ && !panMomentumActive_) {
        controller_->finishViewportInteraction();
      }
      if (spacePressed_) {
        setCursor(Qt::OpenHandCursor);
      } else {
        unsetCursor();
      }
      if (didSpacePan_) {
        ArtifactAudioScrubController::instance().stopScrub();
      }
      event->accept();
      return;
    }

    if (isAltOrbiting_ && event->button() == Qt::LeftButton) {
      isAltOrbiting_ = false;
      clearNavigationFeedback();
      releaseMouse();
      if (controller_) {
        controller_->finishViewportInteraction();
      }
      unsetCursor();
      event->accept();
      return;
    }

    if (isAltZooming_ && event->button() == Qt::RightButton) {
      isAltZooming_ = false;
      clearNavigationFeedback();
      releaseMouse();
      if (controller_) {
        controller_->finishViewportInteraction();
      }
      unsetCursor();
      event->accept();
      return;
    }

    if (controller_) {
      const bool wasScaleDrag = isScaleDragActive();
      const bool wasSpatialGizmoDragging = isSpatialGizmoDragging();
      controller_->handleMouseRelease();
      if (auto *toolManager = ArtifactApplicationManager::instance()
                                  ? ArtifactApplicationManager::instance()->toolManager()
                                  : nullptr;
          toolManager && toolManager->activeTool() == ToolType::Text) {
        auto *selection = ArtifactApplicationManager::instance()
                              ? ArtifactApplicationManager::instance()
                                    ->layerSelectionManager()
                              : nullptr;
        if (selection) {
          if (const auto layer = selection->currentLayer();
              layer && ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(layer)) {
            editTextLayerInline(this, layer, controller_);
          }
        }
      }
      if (wasSpatialGizmoDragging && QWidget::mouseGrabber() == this) {
        releaseMouse();
      }
      if (wasScaleDrag) {
        controller_->markRenderDirty();
      }
      if (wasScaleDrag) {
        update();
      }
      if (!wasSpatialGizmoDragging) {
        controller_->finishViewportInteraction();
      }
      if (!spacePressed_) {
        updateViewportCursor(event->position());
      }
      if (wasSpatialGizmoDragging) {
        controller_->finishViewportInteraction();
        event->accept();
        return;
      }
    }

    QWidget::mouseReleaseEvent(event);
  }

  void tabletEvent(QTabletEvent *event) override {
    if (!event || !controller_) {
      QWidget::tabletEvent(event);
      return;
    }
    const float pressure = std::clamp(static_cast<float>(event->pressure()),
                                      0.0f, 1.0f);
    controller_->setPointerPressure(pressure);
    controller_->setPointerTilt(static_cast<float>(event->xTilt()),
                                 static_cast<float>(event->yTilt()));
    if (event->type() == QEvent::TabletPress) {
      panMomentumActive_ = false;
      panVelocityPerMs_ = {};
      if (activatedCallback_) {
        activatedCallback_();
      }
      controller_->notifyViewportInteractionActivity();
      QMouseEvent synth(QEvent::MouseButtonPress, event->position(),
                        event->globalPosition(), Qt::LeftButton,
                        Qt::LeftButton, event->modifiers());
      controller_->handleMousePress(&synth);
    } else if (event->type() == QEvent::TabletMove) {
      controller_->notifyViewportInteractionActivity();
      controller_->handleMouseMove(event->position());
    } else if (event->type() == QEvent::TabletRelease) {
      controller_->handleMouseRelease();
      controller_->finishViewportInteraction();
      controller_->setPointerPressure(1.0f);
      controller_->setPointerTilt(0.0f, 0.0f);
    }
    event->accept();
  }

  void leaveEvent(QEvent *event) override {
    if (!isPanning_ && !isAltOrbiting_ &&
        !isSpatialGizmoDragging()) {
      unsetCursor();
    }
    QWidget::leaveEvent(event);
  }

#ifdef Q_OS_WIN
  bool nativeEvent(const QByteArray &eventType, void *message,
                   qintptr *result) override {
    Q_UNUSED(result);
    if (eventType != "windows_generic_MSG")
      return QWidget::nativeEvent(eventType, message, result);

    auto *msg = static_cast<MSG *>(message);
    // WA_PaintOnScreen + WA_NativeWindow bypasses Qt's mouse event delivery.
    // All mouse input is therefore handled here via Win32 messages.
    // SetCapture ensures drag events arrive even when the cursor leaves the
    // client area.
    const double dpr = devicePixelRatioF();
    const QPointF physPos(GET_X_LPARAM(msg->lParam),
                          GET_Y_LPARAM(msg->lParam));
    const QPointF logPos = physPos / dpr;
    const bool altDown = (GetKeyState(VK_MENU) & 0x8000) != 0;
    const auto cancelNativePointerInteraction = [&]() {
      const bool finishControllerDrag = nativeControllerDragActive_;
      nativePointerCaptureActive_ = false;
      nativeControllerDragActive_ = false;
      isPanning_ = false;
      isPanningWithMiddle_ = false;
      isAltOrbiting_ = false;
      isAltZooming_ = false;
      clearNavigationFeedback();
      if (controller_) {
        const bool gizmoCancelled = controller_->cancelGizmoInteraction();
        if (finishControllerDrag && !gizmoCancelled) {
          controller_->handleMouseRelease();
        }
        controller_->finishViewportInteraction();
      }
      unsetCursor();
      if (GetCapture() == msg->hwnd) {
        ReleaseCapture();
      }
    };

    switch (msg->message) {
    case WM_RBUTTONDOWN:
      if (controller_ && controller_->cancelGizmoInteraction()) {
        nativePointerCaptureActive_ = false;
        nativeControllerDragActive_ = false;
        if (GetCapture() == msg->hwnd) {
          ReleaseCapture();
        }
        clearNavigationFeedback();
        controller_->finishViewportInteraction();
        updateViewportCursor(logPos);
        return true;
      }
      if (controller_ && controller_->isPieMenuOverlayVisible()) {
        controller_->cancelPieMenuOverlay();
        return true;
      }
      if (altDown) {
        isAltZooming_ = true;
        setNavigationFeedback(NavigationFeedbackMode::Zoom);
        lastMousePos_ = logPos;
        SetCapture(msg->hwnd);
        nativePointerCaptureActive_ = true;
        setCursor(Qt::SizeVerCursor);
        if (controller_) {
          controller_->notifyViewportInteractionActivity();
        }
        return true;
      }
      showViewportContextMenu(logPos);
      return true;

    case WM_LBUTTONDOWN:
      if (controller_ && controller_->isPieMenuOverlayVisible()) {
        controller_->confirmPieMenuOverlaySelection();
        return true;
      }
      if (controller_ && controller_->isViewportOverlayVisible()) {
        triggerViewportOverlayItem(logPos);
        return true;
      }
      if (spacePressed_) {
        isPanning_ = true;
        setNavigationFeedback(NavigationFeedbackMode::Pan);
        isPanningWithMiddle_ = false;
        lastMousePos_ = logPos;
        SetCapture(msg->hwnd);
        nativePointerCaptureActive_ = true;
        setCursor(Qt::ClosedHandCursor);
        if (controller_)
          controller_->notifyViewportInteractionActivity();
      } else if (altDown && (GetKeyState(VK_CONTROL) & 0x8000) == 0 &&
                 controller_ &&
                 !controller_->isTransformGizmoHovered(logPos) &&
                 !maskNavigationLocked()) {
        isAltOrbiting_ = true;
        setNavigationFeedback(NavigationFeedbackMode::Orbit);
        orbitDragStartPos_ = logPos;
        orbitDragStartOrientation_ =
            controller_->viewportOrientationQuaternion();
        SetCapture(msg->hwnd);
        nativePointerCaptureActive_ = true;
        setCursor(Qt::SizeAllCursor);
        controller_->notifyViewportInteractionActivity();
      } else if (altDown && (GetKeyState(VK_CONTROL) & 0x8000) != 0 &&
                 controller_) {
        controller_->placeWorkCursorAtViewportPos(logPos);
      } else if (controller_) {
        QMouseEvent synth(QEvent::MouseButtonPress, logPos,
                          mapToGlobal(logPos), Qt::LeftButton,
                          Qt::LeftButton, Qt::NoModifier);
        controller_->handleMousePress(&synth);
        if (isSpatialGizmoDragging()) {
          SetCapture(msg->hwnd);
          nativePointerCaptureActive_ = true;
          nativeControllerDragActive_ = true;
          updateViewportCursor(logPos);
          if (overlayWidget_)
            overlayWidget_->update();
        }
      }
      return true;

    case WM_LBUTTONUP:
      if (isAltOrbiting_) {
        isAltOrbiting_ = false;
        clearNavigationFeedback();
        if (controller_)
          controller_->finishViewportInteraction();
        unsetCursor();
      } else if (isPanning_ && !isPanningWithMiddle_) {
        isPanning_ = false;
        clearNavigationFeedback();
        if (controller_)
          controller_->finishViewportInteraction();
        if (!spacePressed_)
          unsetCursor();
      } else if (controller_) {
        controller_->handleMouseRelease();
        if (overlayWidget_)
          overlayWidget_->update();
        controller_->markRenderDirty();
        controller_->finishViewportInteraction();
      }
      nativeControllerDragActive_ = false;
      nativePointerCaptureActive_ = false;
      if (GetCapture() == msg->hwnd) {
        ReleaseCapture();
      }
      return true;

    case WM_MBUTTONDOWN:
      isPanning_ = true;
      setNavigationFeedback(NavigationFeedbackMode::Pan);
      isPanningWithMiddle_ = true;
      lastMousePos_ = logPos;
      SetCapture(msg->hwnd);
      nativePointerCaptureActive_ = true;
      setCursor(Qt::ClosedHandCursor);
      if (controller_)
        controller_->notifyViewportInteractionActivity();
      return true;

    case WM_MBUTTONUP:
      if (isPanning_ && isPanningWithMiddle_) {
        isPanning_ = false;
        isPanningWithMiddle_ = false;
        clearNavigationFeedback();
        nativePointerCaptureActive_ = false;
        if (GetCapture() == msg->hwnd)
          ReleaseCapture();
        if (controller_)
          controller_->finishViewportInteraction();
        if (!spacePressed_)
          unsetCursor();
      }
      return true;

    case WM_RBUTTONUP:
      if (isAltZooming_) {
        isAltZooming_ = false;
        clearNavigationFeedback();
        nativePointerCaptureActive_ = false;
        if (GetCapture() == msg->hwnd)
          ReleaseCapture();
        if (controller_) {
          controller_->finishViewportInteraction();
        }
        unsetCursor();
        return true;
      }
      break;

    case WM_MOUSEMOVE:
      if (controller_ && controller_->isPieMenuOverlayVisible()) {
        controller_->updatePieMenuOverlayMousePos(logPos);
        return true;
      }
      if (isAltOrbiting_ && controller_) {
        const QPointF delta = logPos - orbitDragStartPos_;
        controller_->notifyViewportInteractionActivity();
        const float yawDelta = static_cast<float>(delta.x()) * 0.55f;
        const float pitchDelta = static_cast<float>(-delta.y()) * 0.55f;
        const QQuaternion yaw =
            QQuaternion::fromAxisAndAngle(0.0f, 1.0f, 0.0f, yawDelta);
        const QVector3D localRight =
            orbitDragStartOrientation_.rotatedVector(
                QVector3D(1.0f, 0.0f, 0.0f));
        const QQuaternion pitch =
            QQuaternion::fromAxisAndAngle(localRight, pitchDelta);
        controller_->setViewportOrientationQuaternion(
            (pitch * yaw * orbitDragStartOrientation_).normalized());
        if (viewportOrientationChangedCallback_) {
          viewportOrientationChangedCallback_(
              controller_->viewportOrientationQuaternion());
        }
        if (overlayWidget_)
          overlayWidget_->update();
        return true;
      }
      if (isAltZooming_ && controller_) {
        const QPointF delta = logPos - lastMousePos_;
        lastMousePos_ = logPos;
        controller_->notifyViewportInteractionActivity();
        const float zoomDelta = static_cast<float>(-delta.y()) * 0.01f;
        const float factor = std::exp(std::clamp(zoomDelta, -0.35f, 0.35f));
        controller_->zoomAtFactor(logPos, factor);
        return true;
      }
      if (isPanning_ && controller_) {
        const QPointF delta = logPos - lastMousePos_;
        lastMousePos_ = logPos;
        controller_->notifyViewportInteractionActivity();
        controller_->panBy(delta);
        return true;
      }
      if (((msg->wParam & MK_LBUTTON) || isSpatialGizmoDragging()) &&
          controller_) {
        if ((msg->wParam & MK_LBUTTON) && GetCapture() != msg->hwnd) {
          SetCapture(msg->hwnd);
          nativePointerCaptureActive_ = true;
          nativeControllerDragActive_ = true;
        }
        controller_->handleMouseMove(logPos);
        if (isSpatialGizmoDragging()) {
          // Phase 3: Use fixed-rate render tick instead of singleShot(16) + renderOneFrame().
          controller_->markRenderDirty();
          updateViewportCursor(logPos);
          return true;
        }
        updateViewportCursor(logPos);
      }
      break;

    case WM_CANCELMODE:
    case WM_KILLFOCUS:
      if (nativePointerCaptureActive_ || nativeControllerDragActive_) {
        cancelNativePointerInteraction();
        return true;
      }
      break;

    case WM_CAPTURECHANGED:
      if ((nativePointerCaptureActive_ || nativeControllerDragActive_) &&
          reinterpret_cast<HWND>(msg->lParam) != msg->hwnd) {
        cancelNativePointerInteraction();
      }
      break;

    default:
      break;
    }
    return QWidget::nativeEvent(eventType, message, result);
  }
#endif

  void keyPressEvent(QKeyEvent *event) override {
    if (event && event->key() == Qt::Key_Escape && !event->isAutoRepeat()) {
      const bool hadPanInteraction =
          isPanning_ || panMomentumActive_ ||
          std::hypot(panVelocityPerMs_.x(), panVelocityPerMs_.y()) > 0.0;
      panMomentumActive_ = false;
      panVelocityPerMs_ = {};
      if (hadPanInteraction) {
        isPanning_ = false;
        isPanningWithMiddle_ = false;
        clearNavigationFeedback();
        if (controller_) {
          controller_->finishViewportInteraction();
        }
      }
    }
    if (auto *owner = qobject_cast<ArtifactCompositionEditor *>(parentWidget())) {
      if (owner->handleImportPlacementKeyPress(event)) {
        return;
      }
    }
    // Blender-style transform semantics: Esc restores the drag-start
    // transform instead of committing the in-progress frame gizmo edit.
    if (event->key() == Qt::Key_Escape && !event->isAutoRepeat() &&
        controller_) {
      controller_->clearMotionPathSelection();
    }
    if (event->key() == Qt::Key_Escape && !event->isAutoRepeat() &&
        controller_ && controller_->cancelGizmoInteraction()) {
      modalTransformNumericInput_.clear();
      nativePointerCaptureActive_ = false;
      nativeControllerDragActive_ = false;
      if (QWidget::mouseGrabber() == this) {
        releaseMouse();
      }
#ifdef Q_OS_WIN
      if (GetCapture() == reinterpret_cast<HWND>(winId())) {
        ReleaseCapture();
      }
#endif
      clearNavigationFeedback();
      controller_->finishViewportInteraction();
      updateViewportCursor(mapFromGlobal(QCursor::pos()));
      event->accept();
      return;
    }
    if (!event->isAutoRepeat() && controller_ &&
        controller_->isModalGizmoInteractionActive()) {
      const auto applyModalNumericInput = [&]() {
        QString numericToken = modalTransformNumericInput_;
        int frameDimension = -1;
        if (numericToken.startsWith(QLatin1Char('w'), Qt::CaseInsensitive)) {
          frameDimension = 0;
          numericToken.remove(0, 1);
        } else if (numericToken.startsWith(QLatin1Char('h'),
                                           Qt::CaseInsensitive)) {
          frameDimension = 1;
          numericToken.remove(0, 1);
        }
        bool numericOk = false;
        const float numericValue = numericToken.toFloat(&numericOk);
        if (!numericOk) return false;
        const QPointF pointer = mapFromGlobal(QCursor::pos());
        return frameDimension >= 0
            ? controller_->setModalGizmoFrameDimension(
                  frameDimension, numericValue, pointer)
            : controller_->setModalGizmoNumericInput(numericValue, pointer);
      };
      if ((event->key() == Qt::Key_Return ||
           event->key() == Qt::Key_Enter) &&
          event->modifiers() == Qt::NoModifier) {
        controller_->commitModalGizmoInteraction();
        modalTransformNumericInput_.clear();
        updateViewportCursor(mapFromGlobal(QCursor::pos()));
        event->accept();
        return;
      }
      int constraintAxis = -1;
      if (event->modifiers() == Qt::NoModifier &&
          controller_->gizmoMode() == TransformGizmo::Mode::Scale &&
          modalTransformNumericInput_.isEmpty() &&
          (event->key() == Qt::Key_W || event->key() == Qt::Key_H)) {
        modalTransformNumericInput_ =
            event->key() == Qt::Key_W ? QStringLiteral("w")
                                      : QStringLiteral("h");
        event->accept();
        return;
      }
      if (event->key() == Qt::Key_X) constraintAxis = 0;
      if (event->key() == Qt::Key_Y) constraintAxis = 1;
      if (event->key() == Qt::Key_Z) constraintAxis = 2;
      if (constraintAxis >= 0 && event->modifiers() == Qt::NoModifier &&
          controller_->constrainModalGizmoInteraction(
              constraintAxis, mapFromGlobal(QCursor::pos()))) {
        if (!modalTransformNumericInput_.isEmpty() &&
            modalTransformNumericInput_ != QStringLiteral("-")) {
          applyModalNumericInput();
        }
        event->accept();
        return;
      }
      if (event->key() == Qt::Key_Backspace &&
          event->modifiers() == Qt::NoModifier) {
        if (!modalTransformNumericInput_.isEmpty()) {
          modalTransformNumericInput_.chop(1);
        }
        if (modalTransformNumericInput_.isEmpty() ||
            modalTransformNumericInput_ == QStringLiteral("-") ||
            modalTransformNumericInput_.compare(QStringLiteral("w"),
                                                Qt::CaseInsensitive) == 0 ||
            modalTransformNumericInput_.compare(QStringLiteral("h"),
                                                Qt::CaseInsensitive) == 0) {
          controller_->clearModalGizmoNumericInput();
          controller_->handleMouseMove(mapFromGlobal(QCursor::pos()));
        } else {
          applyModalNumericInput();
        }
        event->accept();
        return;
      }
      const QString typed = event->text();
      const bool plainNumericModifier =
          event->modifiers() == Qt::NoModifier ||
          event->modifiers() == Qt::KeypadModifier;
      if (plainNumericModifier && typed.size() == 1) {
        const QChar character = typed.front();
        bool acceptedNumericCharacter = character.isDigit();
        if ((character == QLatin1Char('.') || character == QLatin1Char(',')) &&
            !modalTransformNumericInput_.contains(QLatin1Char('.'))) {
          if (modalTransformNumericInput_.isEmpty() ||
              modalTransformNumericInput_ == QStringLiteral("-") ||
              modalTransformNumericInput_.compare(QStringLiteral("w"),
                                                  Qt::CaseInsensitive) == 0 ||
              modalTransformNumericInput_.compare(QStringLiteral("h"),
                                                  Qt::CaseInsensitive) == 0) {
            modalTransformNumericInput_ += QLatin1Char('0');
          }
          modalTransformNumericInput_ += QLatin1Char('.');
          acceptedNumericCharacter = true;
        } else if (character == QLatin1Char('-') &&
                   !modalTransformNumericInput_.startsWith(
                       QLatin1Char('w'), Qt::CaseInsensitive) &&
                   !modalTransformNumericInput_.startsWith(
                       QLatin1Char('h'), Qt::CaseInsensitive)) {
          if (modalTransformNumericInput_.startsWith(QLatin1Char('-'))) {
            modalTransformNumericInput_.remove(0, 1);
          } else {
            modalTransformNumericInput_.prepend(QLatin1Char('-'));
          }
          acceptedNumericCharacter = true;
        } else if (character.isDigit()) {
          modalTransformNumericInput_ += character;
        }
        if (acceptedNumericCharacter) {
          applyModalNumericInput();
          event->accept();
          return;
        }
      }
      if (event->modifiers() == Qt::NoModifier &&
          (event->key() == Qt::Key_G || event->key() == Qt::Key_R ||
           event->key() == Qt::Key_S)) {
        // Switching modal operation starts again from the pre-transform
        // snapshot, matching Blender's non-destructive mode switch behavior.
        controller_->cancelGizmoInteraction();
        modalTransformNumericInput_.clear();
      }
    }
    if (!event->isAutoRepeat() &&
        (event->key() == Qt::Key_Return ||
         event->key() == Qt::Key_Enter) &&
        event->modifiers() == Qt::NoModifier && controller_ &&
        controller_->hasPendingShapePathCreation() &&
        controller_->finalizePendingShapePathCreation()) {
      clearNavigationFeedback();
      event->accept();
      return;
    }
    if (event->key() == Qt::Key_Escape && !event->isAutoRepeat() &&
        controller_ && controller_->hasPendingShapePathCreation()) {
      controller_->cancelPendingShapePathCreation();
      clearNavigationFeedback();
      controller_->finishViewportInteraction();
      event->accept();
      return;
    }
    if (event->key() == Qt::Key_Escape && !event->isAutoRepeat() &&
        controller_ && controller_->hasPendingMaskEdit()) {
      controller_->cancelMaskInteraction();
      clearNavigationFeedback();
      controller_->finishViewportInteraction();
      event->accept();
      return;
    }
    if (event->key() == Qt::Key_Escape && !event->isAutoRepeat() &&
        controller_ && controller_->cancelTextToolInteraction()) {
      clearNavigationFeedback();
      controller_->finishViewportInteraction();
      event->accept();
      return;
    }
    if (event->key() == Qt::Key_Escape && !event->isAutoRepeat() &&
        controller_ && controller_->clearRigSelection()) {
      event->accept();
      return;
    }
    if (!event->isAutoRepeat() && event->key() == Qt::Key_Tab &&
        event->modifiers().testFlag(Qt::ControlModifier)) {
      if (auto *toolManager = ArtifactApplicationManager::instance()
                                  ? ArtifactApplicationManager::instance()->toolManager()
                                  : nullptr) {
        const bool rigMode = toolManager->activeTool() == ToolType::RigSelect ||
            toolManager->activeTool() == ToolType::RigWeight;
        toolManager->setActiveTool(rigMode ? ToolType::Selection
                                           : ToolType::RigSelect);
        if (controller_) {
          if (rigMode) controller_->clearRigSelection();
          controller_->setInfoOverlayText(
              QStringLiteral("Rig Mode"),
              rigMode ? QStringLiteral("Rig editing exited")
                      : QStringLiteral("Rig Select active"));
        }
        event->accept();
        return;
      }
    }
    if (!event->isAutoRepeat() &&
        (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) &&
        controller_ &&
        ArtifactApplicationManager::instance() &&
        ArtifactApplicationManager::instance()->toolManager() &&
        ArtifactApplicationManager::instance()->toolManager()->activeTool() ==
            ToolType::Puppet && controller_->deleteSelectedPuppetPin()) {
      event->accept();
      return;
    }
    if (!event->isAutoRepeat() && controller_ &&
        ArtifactApplicationManager::instance() &&
        ArtifactApplicationManager::instance()->toolManager() &&
        ArtifactApplicationManager::instance()->toolManager()->activeTool() ==
            ToolType::Puppet) {
      auto *puppet = ArtifactApplicationManager::instance()->puppetTool();
      if (puppet && (event->key() == Qt::Key_O || event->key() == Qt::Key_BracketLeft ||
                     event->key() == Qt::Key_BracketRight)) {
        if (event->key() == Qt::Key_O) {
          puppet->setProportionalEditingEnabled(
              !puppet->isProportionalEditingEnabled());
        } else {
          const float direction = event->key() == Qt::Key_BracketRight ? 1.25f : 0.8f;
          puppet->setProportionalEditRadius(puppet->proportionalEditRadius() * direction);
        }
        if (controller_) {
          controller_->setInfoOverlayText(
              QStringLiteral("Puppet Proportional Editing"),
              puppet->isProportionalEditingEnabled()
                  ? QStringLiteral("On · radius %1").arg(puppet->proportionalEditRadius(), 0, 'f', 1)
                  : QStringLiteral("Off"));
        }
        event->accept();
        return;
      }
    }
    if (!event->isAutoRepeat() && controller_ &&
        ArtifactApplicationManager::instance() &&
        ArtifactApplicationManager::instance()->toolManager() &&
        ArtifactApplicationManager::instance()->toolManager()->activeTool() ==
            ToolType::Pen &&
        (event->key() == Qt::Key_O || event->key() == Qt::Key_BracketLeft ||
         event->key() == Qt::Key_BracketRight)) {
      if (event->key() == Qt::Key_O) {
        controller_->setMaskProportionalEditingEnabled(
            !controller_->isMaskProportionalEditingEnabled());
      } else {
        const float direction = event->key() == Qt::Key_BracketRight ? 1.25f : 0.8f;
        controller_->setMaskProportionalEditRadius(
            controller_->maskProportionalEditRadius() * direction);
      }
      controller_->setInfoOverlayText(
          QStringLiteral("Mask Proportional Editing"),
          controller_->isMaskProportionalEditingEnabled()
              ? QStringLiteral("On · radius %1").arg(
                    controller_->maskProportionalEditRadius(), 0, 'f', 1)
              : QStringLiteral("Off"));
      event->accept();
      return;
    }
    if (!event->isAutoRepeat() && event->key() == Qt::Key_Backspace &&
        event->modifiers() == Qt::NoModifier && controller_) {
      if (auto *toolManager = ArtifactApplicationManager::instance()
                                  ? ArtifactApplicationManager::instance()->toolManager()
                                  : nullptr;
          toolManager && toolManager->activeTool() == ToolType::Pen &&
          controller_->removeLastPendingShapePathVertex()) {
        event->accept();
        return;
      }
      if (toolManager && toolManager->activeTool() == ToolType::Pen &&
          controller_->removeLastPendingMaskVertex()) {
        event->accept();
        return;
      }
    }
    if (!event->isAutoRepeat() &&
        (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) &&
        controller_ && (controller_->deleteSelectedMaskVertices() ||
                        controller_->deleteHoveredMaskVertex())) {
      event->accept();
      return;
    }
    if (!event->isAutoRepeat() && event->key() == Qt::Key_A &&
        event->modifiers().testFlag(Qt::ControlModifier) && controller_) {
      if (auto *toolManager = ArtifactApplicationManager::instance()
                                  ? ArtifactApplicationManager::instance()->toolManager()
                                  : nullptr;
          toolManager && toolManager->activeTool() == ToolType::Pen) {
        controller_->selectAllMaskVertices();
        event->accept();
        return;
      }
    }
    if (!event->isAutoRepeat() && event->key() == Qt::Key_D &&
        event->modifiers().testFlag(Qt::ControlModifier) && controller_ &&
        controller_->duplicateHoveredMask()) {
      event->accept();
      return;
    }
    if (!event->isAutoRepeat() && event->key() == Qt::Key_Z &&
        event->modifiers() == (Qt::ControlModifier | Qt::AltModifier)) {
      if (auto *toolManager = ArtifactApplicationManager::instance()
                                  ? ArtifactApplicationManager::instance()->toolManager()
                                  : nullptr;
          toolManager && toolManager->activeTool() == ToolType::Eraser) {
        const auto comp = currentComposition();
        const auto layer = comp && controller_
            ? comp->layerById(controller_->selectedLayerId())
            : ArtifactAbstractLayerPtr{};
        if (const auto paintLayer = layer
                                        ? ArtifactCore::dynamicPointerCast<ArtifactPaintLayer>(layer)
                                        : ArtifactCore::SharedPtr<ArtifactPaintLayer>{}) {
          if (paintLayer->canUndo()) {
            paintLayer->undoLastStroke();
            if (comp) {
              ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
                  LayerChangedEvent{comp->id().toString(), paintLayer->id().toString(),
                                    LayerChangedEvent::ChangeType::Modified});
            }
            if (controller_) {
              controller_->setInfoOverlayText(
                  QStringLiteral("Eraser"),
                  QStringLiteral("Last paint stroke removed"));
              controller_->markRenderDirty();
            }
            event->accept();
            return;
          }
        }
      }
    }
    if (!event->isAutoRepeat() && event->key() == Qt::Key_C &&
        event->modifiers().testFlag(Qt::ControlModifier) && controller_ &&
        controller_->copyHoveredMask()) {
      event->accept();
      return;
    }
    if (!event->isAutoRepeat() && event->key() == Qt::Key_V &&
        event->modifiers().testFlag(Qt::ControlModifier) && controller_ &&
        controller_->pasteMask()) {
      event->accept();
      return;
    }
    auto *toolManager = ArtifactApplicationManager::instance()
                            ? ArtifactApplicationManager::instance()->toolManager()
                            : nullptr;
    // Blender-compatible duplicate-and-move. This lives in the focused
    // viewport so it does not shadow Ctrl+D (the application's ordinary
    // duplicate command) in other panels.
    if (!event->isAutoRepeat() && event->key() == Qt::Key_D &&
        event->modifiers() == Qt::ShiftModifier && controller_) {
      const auto comp = currentComposition();
      QVector<LayerID> sourceIds;
      if (auto *selection = ArtifactApplicationManager::instance()
                                ? ArtifactApplicationManager::instance()
                                      ->layerSelectionManager()
                                : nullptr) {
        for (const auto &layer : selection->selectedLayers()) {
          if (layer && !layer->isLocked() && !layer->isSelectionLocked()) {
            sourceIds.push_back(layer->id());
          }
        }
      }
      if (sourceIds.isEmpty() && !controller_->selectedLayerId().isNil()) {
        sourceIds.push_back(controller_->selectedLayerId());
      }
      if (comp && !sourceIds.isEmpty()) {
        QSet<LayerID> beforeIds;
        for (const auto &layer : comp->allLayer()) {
          if (layer) beforeIds.insert(layer->id());
        }
        if (auto *service = ArtifactProjectService::instance(); service) {
          bool duplicatedAny = false;
          for (const LayerID &sourceId : sourceIds) {
            duplicatedAny =
                service->duplicateLayerInCurrentComposition(sourceId) ||
                duplicatedAny;
          }
          QVector<ArtifactAbstractLayerPtr> duplicates;
          for (const auto &layer : comp->allLayer()) {
            if (layer && !beforeIds.contains(layer->id())) {
              duplicates.push_back(layer);
            }
          }
          if (duplicatedAny && !duplicates.isEmpty()) {
            if (auto *selection = ArtifactApplicationManager::instance()
                                      ? ArtifactApplicationManager::instance()
                                            ->layerSelectionManager()
                                      : nullptr) {
              selection->clearSelection();
              for (const auto &duplicate : duplicates) {
                selection->addToSelection(duplicate);
              }
            }
            controller_->setSelectedLayerId(duplicates.front()->id());
            if (toolManager) toolManager->setActiveTool(ToolType::Move);
            QPointF modalStart = mapFromGlobal(QCursor::pos());
            if (!rect().contains(modalStart.toPoint())) {
              modalStart = QPointF(rect().center()) + QPointF(80.0, 0.0);
            }
            controller_->beginModalGizmoInteraction(
                TransformGizmo::Mode::Move, modalStart);
            modalTransformNumericInput_.clear();
            controller_->setInfoOverlayText(
                QStringLiteral("Duplicate"),
                QStringLiteral("Duplicate created — move, then Enter to confirm"));
            event->accept();
            return;
          }
        }
      }
    }
    // Preserve Blender's clear-transform grammar without claiming the plain
    // G/R/S keys from the active transform modal.
    if (!event->isAutoRepeat() && controller_ &&
        event->modifiers() == Qt::AltModifier &&
        (event->key() == Qt::Key_G || event->key() == Qt::Key_R ||
         event->key() == Qt::Key_S)) {
      const int component = event->key() == Qt::Key_G
          ? 0 : event->key() == Qt::Key_R ? 1 : 2;
      if (controller_->resetSelectedTransformComponent(component)) {
        controller_->setInfoOverlayText(
            QStringLiteral("Clear Transform"),
            component == 0 ? QStringLiteral("Location reset")
                           : component == 1 ? QStringLiteral("Rotation reset")
                                            : QStringLiteral("Scale reset"));
        event->accept();
        return;
      }
    }
    if (!event->isAutoRepeat() && controller_ &&
        (event->modifiers() == Qt::ShiftModifier ||
         event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) &&
        (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right ||
         event->key() == Qt::Key_Up || event->key() == Qt::Key_Down)) {
      if (auto *toolManager = ArtifactApplicationManager::instance()
                                  ? ArtifactApplicationManager::instance()->toolManager()
                                  : nullptr;
          toolManager && toolManager->activeTool() == ToolType::RigSelect) {
        const float step = event->modifiers().testFlag(Qt::ControlModifier)
            ? 10.0f : 1.0f;
        QVector2D delta;
        if (event->key() == Qt::Key_Left) delta = QVector2D(-step, 0.0f);
        if (event->key() == Qt::Key_Right) delta = QVector2D(step, 0.0f);
        if (event->key() == Qt::Key_Up) delta = QVector2D(0.0f, -step);
        if (event->key() == Qt::Key_Down) delta = QVector2D(0.0f, step);
        if (controller_->nudgeSelectedRigControl(delta)) {
          event->accept();
          return;
        }
      }
    }
    if (!event->isAutoRepeat() && controller_ &&
        event->key() == Qt::Key_Up && controller_->moveHoveredMask(-1)) {
      event->accept();
      return;
    }
    if (!event->isAutoRepeat() && controller_ &&
        event->key() == Qt::Key_Down && controller_->moveHoveredMask(1)) {
      event->accept();
      return;
    }
    if (!event->isAutoRepeat() && event->key() == Qt::Key_N && controller_) {
      if (auto *toolManager = ArtifactApplicationManager::instance()
                                  ? ArtifactApplicationManager::instance()->toolManager()
                                  : nullptr;
          toolManager && toolManager->activeTool() == ToolType::RigWeight) {
        if (controller_->normalizeRigWeights()) {
          controller_->setInfoOverlayText(
              QStringLiteral("Rig Weights"),
              QStringLiteral("Normalized all vertex weights"));
        }
        event->accept();
        return;
      }
    }
    if (!event->isAutoRepeat() && controller_ &&
        event->key() == Qt::Key_E &&
        event->modifiers() == Qt::NoModifier) {
      if (auto *toolManager = ArtifactApplicationManager::instance()
                                  ? ArtifactApplicationManager::instance()->toolManager()
                                  : nullptr;
          toolManager && toolManager->activeTool() == ToolType::RigSelect &&
          controller_->nudgeSelectedRigBoneRotation(15.0f)) {
        event->accept();
        return;
      }
    }
    if (!event->isAutoRepeat() && controller_ &&
        event->key() == Qt::Key_E &&
        event->modifiers() == Qt::ShiftModifier) {
      if (auto *toolManager = ArtifactApplicationManager::instance()
                                  ? ArtifactApplicationManager::instance()->toolManager()
                                  : nullptr;
          toolManager && toolManager->activeTool() == ToolType::RigSelect &&
          controller_->nudgeSelectedRigBoneRotation(-15.0f)) {
        event->accept();
        return;
      }
    }
    if (!event->isAutoRepeat() && event->key() == Qt::Key_S && controller_) {
      if (auto *toolManager = ArtifactApplicationManager::instance()
                                  ? ArtifactApplicationManager::instance()->toolManager()
                                  : nullptr;
          toolManager && toolManager->activeTool() == ToolType::RigWeight) {
        if (controller_->smoothRigWeights()) {
          controller_->setInfoOverlayText(
              QStringLiteral("Rig Weights"),
              QStringLiteral("Smoothed selected bone weights"));
        }
        event->accept();
        return;
      }
    }
    if (!event->isAutoRepeat() && event->key() == Qt::Key_M && controller_) {
      if (auto *toolManager = ArtifactApplicationManager::instance()
                                  ? ArtifactApplicationManager::instance()->toolManager()
                                  : nullptr;
          toolManager && toolManager->activeTool() == ToolType::RigWeight) {
        if (controller_->mirrorRigWeights()) {
          controller_->setInfoOverlayText(
              QStringLiteral("Rig Weights"),
              QStringLiteral("Mirrored selected bone weights"));
        }
        event->accept();
        return;
      }
    }
    if (!event->isAutoRepeat() && controller_ &&
        event->modifiers().testFlag(Qt::ControlModifier) &&
        event->modifiers().testFlag(Qt::ShiftModifier) &&
        (event->key() == Qt::Key_C || event->key() == Qt::Key_V ||
         event->key() == Qt::Key_B)) {
      if (auto *toolManager = ArtifactApplicationManager::instance()
                                  ? ArtifactApplicationManager::instance()->toolManager()
                                  : nullptr;
          toolManager && (toolManager->activeTool() == ToolType::RigSelect ||
                          toolManager->activeTool() == ToolType::RigWeight)) {
        const bool handled = event->key() == Qt::Key_C
            ? controller_->captureRigPose()
            : controller_->applyCapturedRigPose(
                  event->key() == Qt::Key_B ? 0.5f : 1.0f);
        if (handled) {
          controller_->setInfoOverlayText(
              QStringLiteral("Rig Pose"),
              event->key() == Qt::Key_C
                  ? QStringLiteral("Pose captured")
                  : event->key() == Qt::Key_B
                      ? QStringLiteral("Pose blended 50%")
                      : QStringLiteral("Pose applied"));
        }
        event->accept();
        return;
      }
    }
    if (!event->isAutoRepeat() && controller_ &&
        event->key() == Qt::Key_Backspace &&
        event->modifiers().testFlag(Qt::ControlModifier) &&
        event->modifiers().testFlag(Qt::ShiftModifier) &&
        event->modifiers().testFlag(Qt::AltModifier)) {
      if (auto *toolManager = ArtifactApplicationManager::instance()
                                  ? ArtifactApplicationManager::instance()->toolManager()
                                  : nullptr;
          toolManager && (toolManager->activeTool() == ToolType::RigSelect ||
                          toolManager->activeTool() == ToolType::RigWeight)) {
        controller_->clearRigPoseSlots();
        controller_->setInfoOverlayText(
            QStringLiteral("Rig Pose"), QStringLiteral("All pose slots cleared"));
        event->accept();
        return;
      }
    }
    if (!event->isAutoRepeat() && controller_ &&
        event->modifiers().testFlag(Qt::ControlModifier) &&
        event->modifiers().testFlag(Qt::ShiftModifier) &&
        (event->key() >= Qt::Key_1 && event->key() <= Qt::Key_9)) {
      if (auto *toolManager = ArtifactApplicationManager::instance()
                                  ? ArtifactApplicationManager::instance()->toolManager()
                                  : nullptr;
          toolManager && (toolManager->activeTool() == ToolType::RigSelect ||
                          toolManager->activeTool() == ToolType::RigWeight)) {
        const int slot = static_cast<int>(event->key() - Qt::Key_0);
        const bool load = event->modifiers().testFlag(Qt::AltModifier);
        const bool handled = load
            ? controller_->applyRigPoseSlot(slot)
            : controller_->saveRigPoseSlot(slot);
        if (handled) {
          controller_->setInfoOverlayText(
              QStringLiteral("Rig Pose Slot %1").arg(slot),
              load ? QStringLiteral("Pose loaded")
                   : QStringLiteral("Pose saved"));
        }
        event->accept();
        return;
      }
    }
    if (!event->isAutoRepeat() && controller_ &&
        (event->key() == Qt::Key_BracketLeft ||
         event->key() == Qt::Key_BracketRight)) {
      if (toolManager && toolManager->activeTool() == ToolType::Pen) {
        const float delta = event->key() == Qt::Key_BracketRight ? 0.1f : -0.1f;
        if (controller_->adjustHoveredMaskOpacity(delta)) {
          event->accept();
          return;
        }
      }
      if (toolManager &&
          (toolManager->activeTool() == ToolType::Brush ||
           toolManager->activeTool() == ToolType::Eraser)) {
        if (auto *brush = ArtifactApplicationManager::instance()
                              ? ArtifactApplicationManager::instance()->brushTool()
                              : nullptr) {
          const float direction =
              event->key() == Qt::Key_BracketRight ? 1.0f : -1.0f;
          const auto modifiers = event->modifiers();
          const bool shift = modifiers.testFlag(Qt::ShiftModifier);
          const bool control = modifiers.testFlag(Qt::ControlModifier);
          const bool alt = modifiers.testFlag(Qt::AltModifier);
          if (shift && control && alt) {
            brush->setSpacing(brush->spacing() + direction * 0.05f);
          } else if (shift && alt) {
            brush->setAngle(brush->angle() + direction * 15.0f);
          } else if (control && alt) {
            brush->setRoundness(brush->roundness() + direction * 0.05f);
          } else if (alt) {
            brush->setHardness(brush->hardness() + direction * 0.05f);
          } else if (shift) {
            brush->setFlow(brush->flow() + direction * 0.05f);
          } else if (control) {
            brush->setOpacity(brush->opacity() + direction * 0.05f);
          } else {
            brush->setRadius(brush->radius() + direction * 2.0f);
          }
          event->accept();
          return;
        }
      }
      if (toolManager && toolManager->activeTool() == ToolType::RigWeight) {
        const float direction =
            event->key() == Qt::Key_BracketRight ? 1.0f : -1.0f;
        if (event->modifiers().testFlag(Qt::ShiftModifier)) {
          controller_->adjustRigWeightBrushFlow(direction * 0.05f);
        } else if (event->modifiers().testFlag(Qt::ControlModifier)) {
          controller_->adjustRigWeightBrush(0.0f, direction * 0.05f);
        } else {
          controller_->adjustRigWeightBrush(direction * 3.0f);
        }
        event->accept();
        return;
      }
      if (toolManager && toolManager->activeTool() == ToolType::TrackPoint) {
        if (auto *gizmo = controller_->trackerGizmo()) {
          auto state = gizmo->state();
          const float delta = event->key() == Qt::Key_BracketRight ? 2.0f : -2.0f;
          const bool searchSize = event->modifiers().testFlag(Qt::ShiftModifier);
          if (searchSize) {
            state.outerHalfW = std::max(state.innerHalfW + 2.0f,
                                        state.outerHalfW + delta);
            state.outerHalfH = std::max(state.innerHalfH + 2.0f,
                                        state.outerHalfH + delta);
          } else {
            state.innerHalfW = std::clamp(
                state.innerHalfW + delta, 2.0f,
                std::max(2.0f, state.outerHalfW - 2.0f));
            state.innerHalfH = std::clamp(
                state.innerHalfH + delta, 2.0f,
                std::max(2.0f, state.outerHalfH - 2.0f));
          }
          gizmo->setState(state);
          auto &config = ArtifactCore::LayeredConfigStore::instance();
          config.setValue(QStringLiteral("Viewport/TrackPoint/FeatureWidth"),
                          state.innerHalfW * 2.0f);
          config.setValue(QStringLiteral("Viewport/TrackPoint/FeatureHeight"),
                          state.innerHalfH * 2.0f);
          config.setValue(QStringLiteral("Viewport/TrackPoint/SearchWidth"),
                          state.outerHalfW * 2.0f);
          config.setValue(QStringLiteral("Viewport/TrackPoint/SearchHeight"),
                          state.outerHalfH * 2.0f);
          controller_->markRenderDirty();
          event->accept();
          return;
        }
      }
      if (toolManager && toolManager->activeTool() == ToolType::MotionSketch) {
        if (auto *motionSketch = ArtifactApplicationManager::instance()
                                     ? ArtifactApplicationManager::instance()
                                           ->motionSketchTool()
                                     : nullptr) {
          const bool increase = event->key() == Qt::Key_BracketRight;
          if (event->modifiers().testFlag(Qt::ShiftModifier)) {
            const float delta = increase ? 5.0f : -5.0f;
            motionSketch->setSampleRate(motionSketch->sampleRate() + delta);
            ArtifactCore::LayeredConfigStore::instance().setValue(
                QStringLiteral("Viewport/MotionSketch/SampleRate"),
                motionSketch->sampleRate());
          } else {
            const float delta = increase ? 0.1f : -0.1f;
            motionSketch->setSmoothing(motionSketch->smoothing() + delta);
            ArtifactCore::LayeredConfigStore::instance().setValue(
                QStringLiteral("Viewport/MotionSketch/Smoothing"),
                motionSketch->smoothing() * 100.0f);
          }
          event->accept();
          return;
        }
      }
    }
    if (!event->isAutoRepeat() && event->key() == Qt::Key_B &&
        event->modifiers() == Qt::NoModifier) {
      if (auto *toolManager = ArtifactApplicationManager::instance()
                                  ? ArtifactApplicationManager::instance()->toolManager()
                                  : nullptr;
          toolManager && toolManager->activeTool() == ToolType::RigSelect) {
        toolManager->setActiveTool(ToolType::RigWeight);
        if (controller_) {
          controller_->setInfoOverlayText(
              QStringLiteral("Weight Paint"),
              QStringLiteral("RigWeight tool enabled"));
        }
        event->accept();
        return;
      }
    }
    if (!event->isAutoRepeat() && event->key() == Qt::Key_W &&
        event->modifiers() == Qt::NoModifier) {
      if (auto *toolManager = ArtifactApplicationManager::instance()
                                  ? ArtifactApplicationManager::instance()->toolManager()
                                  : nullptr;
          toolManager && toolManager->activeTool() == ToolType::MotionSketch) {
        if (auto *motionSketch = ArtifactApplicationManager::instance()
                                     ? ArtifactApplicationManager::instance()
                                           ->motionSketchTool()
                                     : nullptr) {
          motionSketch->setShowWireframe(!motionSketch->showWireframe());
          ArtifactCore::LayeredConfigStore::instance().setValue(
              QStringLiteral("Viewport/MotionSketch/ShowWireframe"),
              motionSketch->showWireframe());
          event->accept();
          return;
        }
      }
    }
    if (!event->isAutoRepeat() && event->key() == Qt::Key_B &&
        event->modifiers() == Qt::NoModifier) {
      if (auto *toolManager = ArtifactApplicationManager::instance()
                                  ? ArtifactApplicationManager::instance()->toolManager()
                                  : nullptr;
          toolManager && toolManager->activeTool() == ToolType::MotionSketch) {
        if (auto *motionSketch = ArtifactApplicationManager::instance()
                                     ? ArtifactApplicationManager::instance()
                                           ->motionSketchTool()
                                     : nullptr) {
          motionSketch->setShowBackground(!motionSketch->showBackground());
          ArtifactCore::LayeredConfigStore::instance().setValue(
              QStringLiteral("Viewport/MotionSketch/ShowBackground"),
              motionSketch->showBackground());
          event->accept();
          return;
        }
      }
    }
    if (event->key() == Qt::Key_Escape && !event->isAutoRepeat() &&
        controller_ && controller_->isPieMenuOverlayVisible()) {
      controller_->cancelPieMenuOverlay();
      event->accept();
      return;
    }
    if (event->key() == Qt::Key_Escape && !event->isAutoRepeat() &&
        controller_ && controller_->isViewportOverlayVisible()) {
      hideViewportOverlay();
      event->accept();
      return;
    }
    if (event->key() == Qt::Key_Escape && !event->isAutoRepeat() &&
        !controller_->isPieMenuOverlayVisible() &&
        !controller_->isViewportOverlayVisible()) {
      if (controller_) {
        auto* app = ArtifactApplicationManager::instance();
        if (controller_ && controller_->cancelCloneStamp()) {
          event->accept();
          return;
        }
        if (controller_ && controller_->cancelBrushStroke()) {
          event->accept();
          return;
        }
        if (app && app->motionSketchTool() && app->motionSketchTool()->isSketching()) {
          controller_->cancelMotionSketch();
        }
        event->accept();
        return;
      }
    }
    if (!event->isAutoRepeat() && event->key() == Qt::Key_K &&
        event->modifiers().testFlag(Qt::ControlModifier)) {
      showCommandPalette();
      event->accept();
      return;
    }

    if (event->key() == Qt::Key_Tab && !event->isAutoRepeat() &&
        event->modifiers().testFlag(Qt::ShiftModifier)) {
      if (auto *owner = qobject_cast<ArtifactCompositionEditor *>(parentWidget())) {
        owner->toggleViewportToolboxes();
      }
      event->accept();
      return;
    }

    if (event->key() == Qt::Key_Tab && !event->isAutoRepeat()) {
      showPieMenu();
      event->accept();
      return;
    }

    if (!event->isAutoRepeat() &&
        ArtifactCore::ShortcutBindings::instance().matches(
            event, ArtifactCore::ShortcutId::PlaybackToggle)) {
      spacePressed_ = true;
      didSpacePan_ = false;
      setCursor(Qt::OpenHandCursor);
      event->accept();
      return;
    }
    if (event->key() == Qt::Key_F12) {
      if (controller_) {
        saveCurrentFrame(controller_);
      }
      event->accept();
      return;
    }
    if (!event->isAutoRepeat() &&
        (event->key() == Qt::Key_BracketLeft ||
         event->key() == Qt::Key_BracketRight)) {
      auto *selection = ArtifactLayerSelectionManager::instance();
      const auto comp = currentComposition();
      if (selection && comp) {
        const auto layers = comp->allLayerRef();
        if (!layers.isEmpty()) {
          const bool reverse = event->key() == Qt::Key_BracketLeft;
          const auto current = selection->currentLayer();
          int currentIndex = -1;
          if (current) {
            for (int i = 0; i < layers.size(); ++i) {
              if (layers[i] == current) {
                currentIndex = i;
                break;
              }
            }
          }
          if (currentIndex < 0) {
            currentIndex = reverse ? layers.size() : -1;
          }
          const int step = reverse ? -1 : 1;
          int nextIndex = currentIndex + step;
          if (nextIndex < 0) {
            nextIndex = layers.size() - 1;
          } else if (nextIndex >= layers.size()) {
            nextIndex = 0;
          }
          if (nextIndex >= 0 && nextIndex < layers.size() && layers[nextIndex]) {
            selection->selectLayer(layers[nextIndex]);
            event->accept();
            return;
          }
        }
      }
    }
    if (!event->isAutoRepeat() && event->key() == Qt::Key_P) {
      beginTemporaryPlayback();
      event->accept();
      return;
    }
    if (!event->isAutoRepeat() && event->key() == Qt::Key_M &&
        event->modifiers().testFlag(Qt::ControlModifier) &&
        event->modifiers().testFlag(Qt::ShiftModifier) &&
        event->modifiers().testFlag(Qt::AltModifier)) {
      const auto layer = currentLayer();
      auto *editor = qobject_cast<ArtifactCompositionEditor *>(parentWidget());
      if (controller_ && layer &&
          controller_->cyclePresetLayerMaskForLayer(layer, true)) {
        if (editor && editor->renderController()) {
          editor->renderController()->setGizmoMode(TransformGizmo::Mode::Move);
          editor->renderController()->setLineDebugKindVisible(LineDebugKind::MaskPath, true);
          editor->renderController()->setLineDebugKindVisible(LineDebugKind::MaskHandle, true);
        }
      }
      event->accept();
      return;
    }
    if (!event->isAutoRepeat() && event->key() == Qt::Key_M &&
        event->modifiers().testFlag(Qt::ControlModifier) &&
        event->modifiers().testFlag(Qt::ShiftModifier)) {
      const auto layer = currentLayer();
      auto *editor = qobject_cast<ArtifactCompositionEditor *>(parentWidget());
      if (controller_ && layer &&
          controller_->createFullLayerMaskForLayer(layer)) {
        if (editor && editor->renderController()) {
          editor->renderController()->setGizmoMode(TransformGizmo::Mode::Move);
          editor->renderController()->setLineDebugKindVisible(LineDebugKind::MaskPath, true);
          editor->renderController()->setLineDebugKindVisible(LineDebugKind::MaskHandle, true);
        }
      }
      event->accept();
      return;
    }
    if (!event->isAutoRepeat() && event->key() == Qt::Key_M) {
      const auto now = std::chrono::steady_clock::now();
      const bool isDoublePress =
          lastMaskShortcutPressValid_ &&
          (now - lastMaskShortcutPressTime_) <= std::chrono::milliseconds(450);
      lastMaskShortcutPressTime_ = now;
      lastMaskShortcutPressValid_ = true;

      auto *editor = qobject_cast<ArtifactCompositionEditor *>(parentWidget());
      if (editor && editor->renderController()) {
        editor->renderController()->setGizmoMode(TransformGizmo::Mode::Move);
      }
      if (controller_) {
        if (editor && editor->renderController()) {
          editor->renderController()->setLineDebugKindVisible(LineDebugKind::MaskPath, true);
          editor->renderController()->setLineDebugKindVisible(LineDebugKind::MaskHandle, true);
        }
        if (isDoublePress) {
          controller_->setLineDebugKindVisible(LineDebugKind::MaskHandle, true);
        }
      }
      event->accept();
      return;
    }
    if (event->key() == Qt::Key_F && !event->isAutoRepeat()) {
      auto *editor = qobject_cast<ArtifactCompositionEditor *>(parentWidget());
      const bool isMaskTool = editor && editor->renderController() &&
                               editor->renderController()->gizmoMode() == TransformGizmo::Mode::Move;
      if (controller_ && isMaskTool) {
        const bool nextVisible =
            !controller_->isLineDebugKindVisible(LineDebugKind::MaskHandle);
        controller_->setLineDebugKindVisible(LineDebugKind::MaskHandle,
                                             nextVisible);
        if (nextVisible) {
          controller_->setLineDebugKindVisible(LineDebugKind::MaskPath, true);
        }
        event->accept();
        return;
      }
      if (controller_) {
        controller_->focusSelectedLayer();
      }
      event->accept();
      return;
    }
    // Viewport.Composition owns Blender-style transform bindings. Keep these
    // out of toolbar QAction shortcuts so G/R/S are resolved by the focused
    // viewport instead of shadowing other panels and modal tools.
    if (!event->isAutoRepeat() && event->modifiers() == Qt::NoModifier &&
        (event->key() == Qt::Key_G || event->key() == Qt::Key_R ||
         event->key() == Qt::Key_S)) {
      if (controller_) {
        ToolType transformTool = ToolType::Move;
        if (event->key() == Qt::Key_G) {
          controller_->setGizmoMode(TransformGizmo::Mode::Move);
        } else if (event->key() == Qt::Key_R) {
          transformTool = ToolType::Rotation;
          controller_->setGizmoMode(TransformGizmo::Mode::Rotate);
        } else {
          transformTool = ToolType::Scale;
          controller_->setGizmoMode(TransformGizmo::Mode::Scale);
        }
        if (auto *toolManager = ArtifactApplicationManager::instance()
                                    ? ArtifactApplicationManager::instance()
                                          ->toolManager()
                                    : nullptr) {
          toolManager->setActiveTool(transformTool);
        }
        QPointF modalStart = mapFromGlobal(QCursor::pos());
        if (!rect().contains(modalStart.toPoint())) {
          modalStart = QPointF(rect().center()) + QPointF(80.0, 0.0);
        }
        controller_->beginModalGizmoInteraction(controller_->gizmoMode(),
                                                modalStart);
        modalTransformNumericInput_.clear();
      }
      event->accept();
      return;
    }
    if (!event->isAutoRepeat() && (event->key() == Qt::Key_QuoteLeft ||
                                   event->key() == Qt::Key_AsciiTilde)) {
      beginTemporarySolo();
      event->accept();
      return;
    }
    if (!event->isAutoRepeat() && event->key() == Qt::Key_H) {
      if (event->modifiers().testFlag(Qt::ShiftModifier)) {
        soloCurrentLayer();
      } else {
        toggleCurrentLayerVisibility();
      }
      event->accept();
      return;
    }
    if (!event->isAutoRepeat() && event->key() == Qt::Key_S &&
        event->modifiers().testFlag(Qt::ShiftModifier)) {
      soloCurrentLayer();
      event->accept();
      return;
    }
    if (!event->isAutoRepeat() && event->key() == Qt::Key_C &&
        event->modifiers().testFlag(Qt::ControlModifier) &&
        event->modifiers().testFlag(Qt::AltModifier)) {
      centerCurrentLayer();
      event->accept();
      return;
    }
    if (!event->isAutoRepeat() &&
        (ArtifactCore::ShortcutBindings::instance().matches(
             event, ArtifactCore::ShortcutId::LayerDeleteSelected) ||
         event->key() == Qt::Key_Delete ||
         event->key() == Qt::Key_Backspace)) {
      auto *svc = ArtifactProjectService::instance();
      auto *active = ArtifactActiveContextService::instance();
      auto *selection = ArtifactLayerSelectionManager::instance();
      const auto selectedLayers = selection ? selection->selectedLayers()
                                            : QSet<ArtifactAbstractLayerPtr>{};
      const auto currentComp =
          active ? active->activeComposition() : ArtifactCompositionPtr{};
      if (svc && currentComp && !selectedLayers.isEmpty()) {
        if (selectedLayers.size() > 1) {
          for (const auto &layer : selectedLayers) {
            if (layer) {
              svc->removeLayerFromComposition(currentComp->id(), layer->id());
            }
          }
        } else if (const auto currentLayer = selection
                                                 ? selection->currentLayer()
                                                 : ArtifactAbstractLayerPtr{};
                   currentLayer) {
          svc->removeLayerFromComposition(currentComp->id(),
                                          currentLayer->id());
        }
        event->accept();
        return;
      }
    }
    QWidget::keyPressEvent(event);
  }

   void keyReleaseEvent(QKeyEvent *event) override {
     if (event->key() == Qt::Key_Escape && !event->isAutoRepeat() &&
         controller_ && controller_->isPieMenuOverlayVisible()) {
       controller_->cancelPieMenuOverlay();
       event->accept();
       return;
     }
     if (event->key() == Qt::Key_Escape && !event->isAutoRepeat() &&
         controller_ && controller_->isViewportOverlayVisible()) {
       hideViewportOverlay();
       event->accept();
       return;
     }

     if (event->key() == Qt::Key_Tab && !event->isAutoRepeat()) {
        if (controller_ && controller_->isPieMenuOverlayVisible()) {
          controller_->confirmPieMenuOverlaySelection();
        }
        event->accept();
        return;
      }

      if (!event->isAutoRepeat() &&
          ArtifactCore::ShortcutBindings::instance().matches(
              event, ArtifactCore::ShortcutId::PlaybackToggle)) {
        const bool receivedPlaybackPress = spacePressed_;
        spacePressed_ = false;
        // A global shortcut may consume Space press before this widget sees it.
        // In that case this release must not issue a second toggle and pause
        // playback that the global action just started.
        const bool shouldTogglePlayback = receivedPlaybackPress && !didSpacePan_;
        didSpacePan_ = false;
        if (!isPanningWithMiddle_) {
          isPanning_ = false;
          clearNavigationFeedback();
        }
        if (controller_) {
          controller_->finishViewportInteraction();
        }
        ArtifactAudioScrubController::instance().stopScrub();
        unsetCursor();
       if (controller_) {
         setCursor(controller_->cursorShapeForViewportPos(
             mapFromGlobal(QCursor::pos())));
       }
       if (shouldTogglePlayback) {
         executePlaybackToggleAction();
       }
       event->accept();
       return;
     }
     if (!event->isAutoRepeat() && (event->key() == Qt::Key_QuoteLeft ||
                                    event->key() == Qt::Key_AsciiTilde)) {
       restoreTemporarySolo();
       event->accept();
       return;
     }
     if (!event->isAutoRepeat() && event->key() == Qt::Key_P) {
       restoreTemporaryPlayback();
       event->accept();
       return;
     }
     QWidget::keyReleaseEvent(event);
   }

   // ============================================================
   // Debug: Event filter to trace mouse events
   // ============================================================
   bool eventFilter(QObject *obj, QEvent *event) override {
     static const QEvent::Type mouseTypes[] = {
         QEvent::MouseButtonPress,
         QEvent::MouseButtonRelease,
         QEvent::MouseMove,
         QEvent::Wheel,
         QEvent::HoverMove,
         QEvent::HoverEnter,
         QEvent::HoverLeave
     };
     for (QEvent::Type t : mouseTypes) {
       if (event->type() == t) {
         QString objName = obj ? obj->objectName() : QString("<null>");
         if (objName.isEmpty()) {
           objName = obj ? obj->metaObject()->className() : QString("<null>");
         }
         qCDebug(compositionViewLog)
             << "[EVENT]" << event->type()
             << "obj:" << objName
             << "visible:" << (obj && obj->isWidgetType() ? qobject_cast<QWidget*>(obj)->isVisible() : "n/a");
         break;
       }
     }
     return QWidget::eventFilter(obj, event);
   }

 private:
  struct TemporarySoloState {
    LayerID layerId;
    bool solo = false;
  };

  ArtifactAbstractLayerPtr currentLayer() const {
    auto *selection = ArtifactLayerSelectionManager::instance();
    return selection ? selection->currentLayer() : ArtifactAbstractLayerPtr{};
  }

  ArtifactCompositionPtr currentComposition() const {
    auto *active = ArtifactActiveContextService::instance();
    return active ? active->activeComposition() : ArtifactCompositionPtr{};
  }

  void beginTemporarySolo() {
    if (temporarySoloActive_) {
      return;
    }
    auto *svc = ArtifactProjectService::instance();
    const auto comp = currentComposition();
    const auto layer = currentLayer();
    if (!svc || !comp || !layer) {
      return;
    }

    temporarySoloStates_.clear();
    const auto &layers = comp->allLayerRef();
    temporarySoloStates_.reserve(layers.size());
    for (const auto &candidate : layers) {
      if (!candidate) {
        continue;
      }
      temporarySoloStates_.push_back({candidate->id(), candidate->isSolo()});
    }

    temporarySoloActive_ = true;
    svc->smartSoloOnlyLayerInCurrentComposition(layer->id());
  }

  void beginTemporaryPlayback() {
    if (temporaryPlaybackActive_) {
      return;
    }
    auto *playback = ArtifactPlaybackService::instance();
    if (!playback || playback->isPlaying()) {
      return;
    }
    temporaryPlaybackActive_ = true;
    playback->play();
    if (controller_) {
      controller_->start();
    }
  }

  void togglePlaybackPreview() {
    auto *playback = ArtifactPlaybackService::instance();
    if (!playback) {
      qWarning() << "[PlaybackUI] toggle rejected: playback service unavailable";
      return;
    }
    const auto composition = currentComposition();
    qInfo() << "[PlaybackUI] togglePlaybackPreview"
            << "isPlaying=" << playback->isPlaying()
            << "state=" << static_cast<int>(playback->state())
            << "currentFrame=" << playback->currentFrame().framePosition()
            << "composition="
            << (composition ? composition->id().toString()
                            : QStringLiteral("null"))
            << "controller=" << (controller_ ? "available" : "null");
    if (playback->isPlaying()) {
      playback->pause();
      return;
    }
    playback->play();
    if (controller_) {
      controller_->start();
    }
  }

  void executePlaybackToggleAction() {
    auto *actions = ArtifactCore::ActionManager::instance();
    if (actions &&
        actions->getAction(QStringLiteral("playback.play_pause"))) {
      actions->executeAction(QStringLiteral("playback.play_pause"));
      return;
    }
    togglePlaybackPreview();
  }

  void restoreTemporarySolo() {
    if (!temporarySoloActive_) {
      return;
    }
    auto *svc = ArtifactProjectService::instance();
    const auto comp = currentComposition();
    if (!svc || !comp) {
      temporarySoloActive_ = false;
      temporarySoloStates_.clear();
      return;
    }

    for (const auto &state : temporarySoloStates_) {
      if (state.layerId.isNil()) {
        continue;
      }
      svc->setLayerSoloInCurrentComposition(state.layerId, state.solo);
    }
    temporarySoloActive_ = false;
    temporarySoloStates_.clear();
  }

  void restoreTemporaryPlayback() {
    if (!temporaryPlaybackActive_) {
      return;
    }
    auto *playback = ArtifactPlaybackService::instance();
    if (playback && playback->isPlaying()) {
      playback->stop();
    }
    if (controller_) {
      controller_->stop();
    }
    temporaryPlaybackActive_ = false;
  }

  void toggleCurrentLayerVisibility() {
    const auto layer = currentLayer();
    if (!layer) return;
    auto command = std::make_unique<SetLayerVisibilityCommand>(
        layer, !layer->isVisible());
    bool applied = false;
    if (auto* manager = UndoManager::instance()) {
      applied = manager->push(std::move(command));
    } else {
      command->redo();
      applied = command->lastOperationSucceeded();
    }
    if (!applied) {
      return;
    }
  }

  void soloCurrentLayer() {
    auto *svc = ArtifactProjectService::instance();
    const auto layer = currentLayer();
    if (!svc || !layer) {
      return;
    }
    svc->smartSoloOnlyLayerInCurrentComposition(layer->id());
  }

  void centerCurrentLayer() {
    auto *svc = ArtifactProjectService::instance();
    const auto comp = currentComposition();
    const auto layer = currentLayer();
    if (!svc || !comp || !layer) {
      return;
    }

    const QSize compSize = comp->settings().compositionSize();
    const float compCenterX =
        static_cast<float>(compSize.width() > 0 ? compSize.width() : 1920) *
        0.5f;
    const float compCenterY =
        static_cast<float>(compSize.height() > 0 ? compSize.height() : 1080) *
        0.5f;

    const QVector3D current = layer->position3D();
    const float deltaX = compCenterX - current.x();
    const float deltaY = compCenterY - current.y();
    if (std::abs(deltaX) <= 0.0001f && std::abs(deltaY) <= 0.0001f) {
      return;
    }
    const auto playback = ArtifactPlaybackService::instance();
    const auto currentFrame = playback
        ? playback->currentFrame()
        : comp->framePosition();
    auto* manager = UndoManager::instance();
    auto command = std::make_unique<MoveLayerCommand>(
        layer, deltaX, deltaY, currentFrame.framePosition());
    bool applied = false;
    if (manager) {
      applied = manager->push(std::move(command));
    } else {
      command->redo();
      applied = command->lastOperationSucceeded();
    }
    if (!applied) {
      return;
    }
  }

  QVector<TemporarySoloState> temporarySoloStates_;
  bool temporarySoloActive_ = false;
  bool temporaryPlaybackActive_ = false;
  void scheduleInitialFit() {
    if (!pendingInitialFit_) {
      return;
    }
    if (resizePending_) {
      QTimer::singleShot(50, this, [this]() { scheduleInitialFit(); });
      return;
    }
    QTimer::singleShot(0, this, [this]() {
      if (!pendingInitialFit_ || !controller_ || !isVisible() ||
          !controller_->isInitialized()) {
        if (pendingInitialFit_) {
          QTimer::singleShot(50, this, [this]() { scheduleInitialFit(); });
        }
        return;
      }
      if (width() <= 64 || height() <= 64) {
        QTimer::singleShot(50, this, [this]() { scheduleInitialFit(); });
        return;
      }
      controller_->zoomFill();
      pendingInitialFit_ = false;
      // Fill完了後にrenderingスタート
      controller_->markRenderDirty();
      if (autoStartPending_) {
        autoStartPending_ = false;
        controller_->start();
      }
    });
  }

  bool autoStartPending_ = false;

  void showPieMenu() {
    if (!controller_)
      return;

    PieMenuModel model;
    model.title = "View Controls";

    auto *toolManager =
        ArtifactApplicationManager::instance()
            ? ArtifactApplicationManager::instance()->toolManager()
            : nullptr;

    // Selection Tool
    model.items.push_back(
        {"Select", loadIconWithFallback("MaterialVS/neutral/select.svg"),
         "tool.select", true, false, [toolManager]() {
           if (toolManager)
             toolManager->setActiveTool(ToolType::Selection);
         }});

    // Hand Tool
    model.items.push_back({"Hand",
                           loadIconWithFallback("MaterialVS/neutral/hand.svg"),
                           "tool.hand", true, false, [toolManager]() {
                             if (toolManager)
                               toolManager->setActiveTool(ToolType::Hand);
                           }});

    // Mask Tool
    model.items.push_back({"Mask",
                           loadIconWithFallback("MaterialVS/neutral/draw.svg"),
                           "tool.mask", true, false, [this, toolManager]() {
                             if (toolManager)
                               toolManager->setActiveTool(ToolType::Pen);
                             controller_->setLineDebugKindVisible(
                                 LineDebugKind::MaskPath, true);
                             controller_->setLineDebugKindVisible(
                                 LineDebugKind::MaskHandle, true);
                             controller_->markRenderDirty();
                           }});

    // Zoom Fit
    model.items.push_back(
        {"Fit", loadIconWithFallback("MaterialVS/neutral/fit.svg"), "view.fit",
         true, false, [this]() { controller_->zoomFit(); }});

    // Zoom 100%
    model.items.push_back(
        {"100%", loadIconWithFallback("MaterialVS/neutral/zoom_100.svg"),
         "view.100", true, false, [this]() { controller_->zoom100(); }});

    // Reset View
    model.items.push_back(
        {"Reset", loadIconWithFallback("MaterialVS/neutral/reset.svg"),
         "view.reset", true, false, [this]() { controller_->resetView(); }});

    if (auto *gizmo3D = controller_->gizmo3D()) {
      model.items.push_back({"3D Move", QIcon(), "gizmo3d.move", true,
                             gizmo3D->mode() == GizmoMode::Move,
                             [this]() {
                               controller_->setGizmoMode(TransformGizmo::Mode::Move);
                             }});
      model.items.push_back({"3D Rotate", QIcon(), "gizmo3d.rotate", true,
                             gizmo3D->mode() == GizmoMode::Rotate,
                             [this]() {
                               controller_->setGizmoMode(TransformGizmo::Mode::Rotate);
                             }});
      model.items.push_back({"3D Scale", QIcon(), "gizmo3d.scale", true,
                             gizmo3D->mode() == GizmoMode::Scale,
                             [this]() {
                               controller_->setGizmoMode(TransformGizmo::Mode::Scale);
                             }});
      model.items.push_back({"3D World", QIcon(), "gizmo3d.world", true,
                             gizmo3D->space() == GizmoSpace::World,
                             [this, gizmo3D]() {
                               gizmo3D->setSpace(GizmoSpace::World);
                               controller_->markRenderDirty();
                             }});
      model.items.push_back({"3D Local", QIcon(), "gizmo3d.local", true,
                              gizmo3D->space() == GizmoSpace::Local,
                              [this, gizmo3D]() {
                                gizmo3D->setSpace(GizmoSpace::Local);
                                controller_->markRenderDirty();
                              }});
      model.items.push_back({"3D View", QIcon(), "gizmo3d.view", true,
                             gizmo3D->space() == GizmoSpace::View,
                             [this, gizmo3D]() {
                               gizmo3D->setSpace(GizmoSpace::View);
                               controller_->markRenderDirty();
                             }});
    }

    // Grid Toggle
    model.items.push_back(
        {"Grid", loadIconWithFallback("MaterialVS/neutral/grid.svg"),
         "display.grid", true, controller_->isShowGrid(),
         [this]() {
           const bool next = !controller_->isShowGrid();
           controller_->setShowGrid(next);
           if (auto *settings = ArtifactCore::ArtifactAppSettings::instance()) {
             settings->setCompositionShowGrid(next);
           }
         }});

    // Safe Area Toggle
    model.items.push_back(
        {"Safe Area", loadIconWithFallback("MaterialVS/neutral/safe_area.svg"),
         "display.safeArea", true, controller_->isShowSafeMargins(), [this]() {
           const bool next = !controller_->isShowSafeMargins();
           controller_->setShowSafeMargins(next);
           if (auto *settings = ArtifactCore::ArtifactAppSettings::instance()) {
             settings->setCompositionShowSafeMargins(next);
           }
         }});

    controller_->showPieMenuOverlay(model, mapFromGlobal(QCursor::pos()));
  }

  void saveCurrentFrame(CompositionRenderController *controller) {
    auto comp = controller->composition();
    if (!comp)
      return;

    auto *svc = ArtifactProjectService::instance();
    if (!svc)
      return;

    const auto selection = ArtifactLayerSelectionManager::instance();
    const ArtifactAbstractLayerPtr selectedLayer =
        selection ? selection->currentLayer() : ArtifactAbstractLayerPtr{};
    const ArtifactAbstractLayerPtr controllerLayer =
        !controller->selectedLayerId().isNil() ? comp->layerById(controller->selectedLayerId())
                                               : ArtifactAbstractLayerPtr{};
    const ArtifactAbstractLayerPtr targetLayer =
        selectedLayer ? selectedLayer : controllerLayer;

    qDebug() << "Debug: F12 pressed. Attempting to save selected layer..."
             << (targetLayer ? targetLayer->id().toString() : QStringLiteral("<none>"));

    QDir dir(QStringLiteral("test"));
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
      qWarning() << "Failed to create test directory.";
      return;
    }

    QImage img = selectedLayerDebugImage(targetLayer);
    if (img.isNull()) {
      img = captureCompositionScreenshot(controller, nullptr);
    }
    if (img.isNull()) {
      qWarning() << "No debug image available for selected layer.";
      return;
    }

    const QString layerTag = targetLayer
                                 ? targetLayer->id().toString()
                                 : QStringLiteral("composition");
    const QString path = QStringLiteral("test/frame_%1_%2.png")
                             .arg(layerTag)
                             .arg(comp->framePosition().framePosition());
    if (img.save(path)) {
      qDebug() << "Successfully saved debug frame to:" << path;
    } else {
      qWarning() << "Failed to save image to:" << path;
    }
  }

  bool maskNavigationLocked() const {
    const auto *app = ArtifactApplicationManager::instance();
    const auto *toolManager = app ? app->toolManager() : nullptr;
    if (toolManager && toolManager->activeTool() == ToolType::Pen) {
      return true;
    }
    return controller_ && controller_->hasPendingMaskEdit();
  }

  CompositionRenderController *controller_ = nullptr;
  std::function<void()> resizeCallback_;
  bool isPanning_ = false;
  bool isPanningWithMiddle_ = false;
  bool panMomentumActive_ = false;
  QPointF panVelocityPerMs_;
  std::chrono::steady_clock::time_point lastPanSampleAt_{};
  bool isAltOrbiting_ = false;
  bool isAltZooming_ = false;
  bool nativePointerCaptureActive_ = false;
  bool nativeControllerDragActive_ = false;
  bool spacePressed_ = false;
  bool didSpacePan_ = false;
  NavigationFeedbackMode navigationFeedbackMode_ =
      NavigationFeedbackMode::None;
  quint64 navigationFeedbackGeneration_ = 0;
  std::chrono::steady_clock::time_point lastMaskShortcutPressTime_{};
  bool lastMaskShortcutPressValid_ = false;
  QString modalTransformNumericInput_;
  bool pendingInitialFit_ = true;
  QTimer *resizeDebounceTimer_ = nullptr;
  QSize pendingResizeSize_;
  bool resizePending_ = false;
  QPointF lastMousePos_;
  QPointF orbitDragStartPos_;
  QQuaternion orbitDragStartOrientation_;
  // D&D オーバーレイ
  bool dropOverlayVisible_ = false;
  QString dropCandidateLabel_;
  QRectF dropGhostRect_;
  QString dropGhostTitle_;
  QString dropGhostHint_;
  QTimer *pendingDropTimer_ = nullptr;
  std::deque<PendingDroppedAsset> pendingDroppedAssets_;
  bool processingDroppedAssets_ = false;
  QWidget *overlayWidget_ = nullptr;
  std::function<void()> activatedCallback_;
  std::function<void(const QQuaternion &)> viewportOrientationChangedCallback_;
  QVector<std::function<void()>> viewportOverlayActions_;
  QVector<bool> viewportOverlayEnabledStates_;
  std::function<void()> lastRepeatableAction_;
  QString lastRepeatableActionLabel_;
  QJsonObject lastRecipeDescriptor_;
  QHash<QString, std::function<void()>> actionRecipes_;
  // 動画ファイルのキャンバスサイズキャッシュ（非同期取得）
  QHash<QString, QSize> videoDimensionCache_;
  QHash<QString, ArtifactCore::FileType> dragFileTypeCache_;
  QString lastDragPath_;
  QPointF lastDragPos_;

  static QString kindLabelForFileType(ArtifactCore::FileType type) {
    switch (type) {
    case ArtifactCore::FileType::Image:
      return QStringLiteral("Image layer");
    case ArtifactCore::FileType::Video:
      return QStringLiteral("Video layer");
    case ArtifactCore::FileType::Audio:
      return QStringLiteral("Audio layer");
    case ArtifactCore::FileType::Model3D:
      return QStringLiteral("3D model layer");
    default:
      return QStringLiteral("Imported layer");
    }
  }

  // キャンバス座標系でのゴーストサイズ（コンポジションピクセル単位）を返す。
  // updateDropPreview 内で renderer->canvasToViewport を通してビューポート座標に変換する。
  QSizeF ghostSizeForFile(const QString &path,
                          ArtifactCore::FileType type) const {
    // コンポジションサイズをフォールバックに使用
    const auto comp = currentComposition();
    const QSize compSz = comp ? comp->settings().compositionSize()
                              : QSize(1920, 1080);
    const double cw = compSz.width() > 0 ? compSz.width() : 1920.0;
    const double ch = compSz.height() > 0 ? compSz.height() : 1080.0;

    if (isSvgShapeFile(path)) {
      return QSizeF(cw * 0.3, ch * 0.3);
    }
    switch (type) {
    case ArtifactCore::FileType::Image: {
      QImageReader reader(path);
      const QSize imageSize = reader.size();
      if (imageSize.isValid()) {
        return QSizeF(imageSize.width(), imageSize.height());
      }
      return QSizeF(cw, ch);
    }
    case ArtifactCore::FileType::Video: {
      // キャッシュがあればそのまま使用
      auto it = videoDimensionCache_.find(path);
      if (it != videoDimensionCache_.end() && it.value().isValid()) {
        return QSizeF(it.value().width(), it.value().height());
      }
      // コンポジションサイズをフォールバックとして返す（非同期で実寸を取得）
      return QSizeF(cw, ch);
    }
    case ArtifactCore::FileType::Audio:
      return QSizeF(cw, ch * 0.08);
    case ArtifactCore::FileType::Model3D:
      return QSizeF(cw * 0.42, ch * 0.42);
    default:
      return QSizeF(cw * 0.4, ch * 0.4);
    }
  }

  // 動画ファイルの実寸を非同期で取得してキャッシュに登録する
  void startVideoDimensionLoad(const QString &path) {
    if (videoDimensionCache_.contains(path)) {
      return; // already cached or loading
    }
    // placeholder to prevent double-launch
    videoDimensionCache_[path] = QSize();
    const QString capturePath = path;
    QPointer<CompositionViewport> self = this;
    std::thread([capturePath, self]() {
      ArtifactCore::FFmpegThumbnailExtractor extractor;
      const auto result =
          extractor.extractThumbnail(ArtifactCore::UniString(capturePath));
      if (!result.success || result.image.isNull()) {
        return;
      }
      const QSize dims = result.image.size();
      // メインスレッドへのコールバックは qApp 経由で安全にポスト
      QMetaObject::invokeMethod(
          qApp,
          [self, capturePath, dims]() {
            if (!self) {
              return;
            }
            self->videoDimensionCache_[capturePath] = dims;
            // ドラッグ中なら即座にゴースト更新
            if (self->lastDragPath_ == capturePath &&
                !self->lastDragPos_.isNull()) {
              self->updateDropPreview(
                  {QUrl::fromLocalFile(capturePath)},
                  self->lastDragPos_);
            }
          },
          Qt::QueuedConnection);
    }).detach();
  }

  void clearDropPreview() {
    dropOverlayVisible_ = false;
    dropCandidateLabel_.clear();
    dropGhostRect_ = QRectF();
    dropGhostTitle_.clear();
    dropGhostHint_.clear();
    lastDragPath_.clear();
    lastDragPos_ = QPointF();
    if (controller_) {
      controller_->clearDropGhostPreview();
    }
  }

  bool isSpatialGizmoDragging() const {
    if (!controller_) {
      return false;
    }
    const auto *gizmo2D = controller_->gizmo();
    const auto *gizmo3D = controller_->gizmo3D();
    return (gizmo2D && gizmo2D->isDragging()) ||
           (gizmo3D && gizmo3D->isDragging());
  }

  bool isScaleDragActive() const {
    if (!controller_) {
      return false;
    }
    auto *gizmo = controller_->gizmo();
    if (!gizmo || !gizmo->isDragging()) {
      return false;
    }
    switch (gizmo->activeHandle()) {
    case TransformGizmo::HandleType::Scale_TL:
    case TransformGizmo::HandleType::Scale_TR:
    case TransformGizmo::HandleType::Scale_BL:
    case TransformGizmo::HandleType::Scale_BR:
    case TransformGizmo::HandleType::Scale_T:
    case TransformGizmo::HandleType::Scale_B:
    case TransformGizmo::HandleType::Scale_L:
    case TransformGizmo::HandleType::Scale_R:
      return true;
    default:
      return false;
    }
  }

  bool isScaleGhostVisible() const {
    if (!isScaleDragActive()) {
      return false;
    }
    const auto comp = currentComposition();
    if (!comp || !controller_ || !controller_->renderer()) {
      return false;
    }
    const auto layerId = controller_->selectedLayerId();
    return !layerId.isNil() && comp->layerById(layerId) != nullptr;
  }

  void drawScaleGhost(QPainter &p) {
    if (!isScaleGhostVisible()) {
      return;
    }

    const auto comp = currentComposition();
    const auto layerId = controller_->selectedLayerId();
    const auto layer =
        comp ? comp->layerById(layerId) : ArtifactAbstractLayerPtr{};
    if (!layer || !controller_ || !controller_->renderer()) {
      return;
    }

    const QRectF bbox = layer->transformedBoundingBox();
    if (!bbox.isValid() || bbox.isEmpty()) {
      return;
    }

    const auto *renderer = controller_->renderer();
    const auto tl = renderer->canvasToViewport(
        {static_cast<float>(bbox.left()), static_cast<float>(bbox.top())});
    const auto tr = renderer->canvasToViewport(
        {static_cast<float>(bbox.right()), static_cast<float>(bbox.top())});
    const auto bl = renderer->canvasToViewport(
        {static_cast<float>(bbox.left()), static_cast<float>(bbox.bottom())});
    const QRectF viewRect(QPointF(qMin(tl.x, tr.x), qMin(tl.y, bl.y)),
                          QPointF(qMax(tr.x, tl.x), qMax(bl.y, tl.y)));

    const auto &t3 = layer->transform3D();
    const QString text =
        QStringLiteral("Scale  %1%%  x  %2%%")
            .arg(QString::number(t3.scaleX() * 100.0f, 'f', 0))
            .arg(QString::number(t3.scaleY() * 100.0f, 'f', 0));
    const QFontMetrics fm(font());
    const QSize textSize = fm.size(Qt::TextSingleLine, text);
    QRect labelRect(static_cast<int>(viewRect.right()) + 12,
                    static_cast<int>(viewRect.top()) - textSize.height() - 14,
                    textSize.width() + 22, textSize.height() + 12);
    if (labelRect.right() > width() - 8) {
      labelRect.moveRight(width() - 8);
    }
    if (labelRect.left() < 8) {
      labelRect.moveLeft(8);
    }
    if (labelRect.top() < 8) {
      labelRect.moveTop(8);
    }

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(12, 14, 17, 220));
    p.drawRoundedRect(labelRect, 6, 6);
    p.setPen(QColor(230, 235, 240));
    p.drawText(labelRect.adjusted(10, 6, -10, -6),
               Qt::AlignLeft | Qt::AlignVCenter,
               fm.elidedText(text, Qt::ElideRight, labelRect.width() - 20));
  }

  void updateDropPreview(const QList<QUrl> &urls, const QPointF &pos) {
    QString path;
    for (const auto &url : urls) {
      if (!url.isLocalFile()) {
        continue;
      }
      const QString candidate = url.toLocalFile();
      if (QFileInfo(candidate).isDir()) {
        continue;
      }
      path = candidate;
      break;
    }
    if (path.isEmpty()) {
      dropOverlayVisible_ = false;
      dropGhostRect_ = QRectF();
      dropGhostTitle_.clear();
      dropGhostHint_.clear();
      if (controller_) {
        controller_->clearDropGhostPreview();
      }
      return;
    }

    lastDragPath_ = path;
    lastDragPos_ = pos;

    QFileInfo fi(path);
    const bool svgShapeFile = isSvgShapeFile(path);
    ArtifactCore::FileType fileType = ArtifactCore::FileType::Image;
    if (svgShapeFile) {
      fileType = ArtifactCore::FileType::Image;
    } else {
      auto it = dragFileTypeCache_.find(path);
      if (it != dragFileTypeCache_.end()) {
        fileType = it.value();
      } else {
        ArtifactCore::FileTypeDetector detector;
        fileType = detector.detectByExtension(path);
        dragFileTypeCache_.insert(path, fileType);
      }
    }

    const QSizeF canvasSize = ghostSizeForFile(path, fileType);

    const auto *renderer =
        (controller_ && controller_->renderer()) ? controller_->renderer()
                                                  : nullptr;
    if (renderer) {
      const auto cc = renderer->viewportToCanvas(
          {static_cast<float>(pos.x()), static_cast<float>(pos.y())});
      const float hw = static_cast<float>(canvasSize.width() * 0.5);
      const float hh = static_cast<float>(canvasSize.height() * 0.5);
      const auto vpTL = renderer->canvasToViewport({cc.x - hw, cc.y - hh});
      const auto vpBR = renderer->canvasToViewport({cc.x + hw, cc.y + hh});
      const float vpW = std::max(40.0f, vpBR.x - vpTL.x);
      const float vpH = std::max(40.0f, vpBR.y - vpTL.y);
      const float vpCx = (vpTL.x + vpBR.x) * 0.5f;
      const float vpCy = (vpTL.y + vpBR.y) * 0.5f;
      dropGhostRect_ = QRectF(vpCx - vpW * 0.5f, vpCy - vpH * 0.5f,
                               vpW, vpH);
    } else {
      constexpr double kFallbackW = 220.0, kFallbackH = 140.0;
      dropGhostRect_ =
          QRectF(pos.x() - kFallbackW * 0.5, pos.y() - kFallbackH * 0.5,
                 kFallbackW, kFallbackH);
    }

    dropGhostTitle_ =
        fi.fileName().isEmpty() ? fi.completeBaseName() : fi.fileName();
    dropGhostHint_ = svgShapeFile ? QStringLiteral("Shape layer")
                                   : kindLabelForFileType(fileType);
    if (controller_) {
      controller_->setDropGhostPreview(dropGhostRect_, dropGhostTitle_,
                                       dropGhostHint_, dropCandidateLabel_);
    }
  }

  void updateDropLabel(const QList<QUrl> &urls) {
    QStringList names;
    for (const auto &url : urls) {
      if (url.isLocalFile()) {
        names.append(QFileInfo(url.toLocalFile()).fileName());
      }
    }
    if (names.size() == 1) {
      dropCandidateLabel_ = names.first();
    } else if (names.size() > 1) {
      dropCandidateLabel_ = QStringLiteral("%1 files").arg(names.size());
    } else {
      dropCandidateLabel_.clear();
    }
  }

private:
  QTimer *readinessTimer_ = nullptr;
  QString pendingReadinessReason_;
  bool readinessScheduled_ = false;
  quintptr lastReadyHostWinId_ = 0;
  QSize lastReadyPhysicalSize_;
  float lastReadyDpr_ = 0.0f;

  void setNavigationFeedback(NavigationFeedbackMode mode,
                             bool transient = false) {
    navigationFeedbackMode_ = mode;
    const quint64 generation = ++navigationFeedbackGeneration_;
    if (overlayWidget_) {
      overlayWidget_->update();
    }
    if (!transient) {
      return;
    }
    QPointer<CompositionViewport> self(this);
    QTimer::singleShot(650, this, [self, generation]() {
      if (!self || self->navigationFeedbackGeneration_ != generation) {
        return;
      }
      self->clearNavigationFeedback();
    });
  }

  void clearNavigationFeedback() {
    navigationFeedbackMode_ = NavigationFeedbackMode::None;
    ++navigationFeedbackGeneration_;
    if (overlayWidget_) {
      overlayWidget_->update();
    }
  }

  void resetSwapChainReadinessTracking() {
    lastReadyHostWinId_ = 0;
    lastReadyPhysicalSize_ = QSize();
    lastReadyDpr_ = 0.0f;
  }
};

class ViewportHudGrip final : public QWidget {
public:
  ViewportHudGrip(QToolBar *hud, QString settingsKey)
      : QWidget(hud), hud_(hud), settingsKey_(std::move(settingsKey)) {
    setFixedWidth(12);
    setCursor(Qt::SizeAllCursor);
    setToolTip(QStringLiteral("Drag to move this viewport toolbar"));
  }

protected:
  void mousePressEvent(QMouseEvent *event) override {
    if (event && event->button() == Qt::LeftButton && hud_) {
      dragging_ = true;
      dragStartGlobal_ = event->globalPosition().toPoint();
      dragStartPosition_ = hud_->pos();
      event->accept();
      return;
    }
    QWidget::mousePressEvent(event);
  }

  void mouseMoveEvent(QMouseEvent *event) override {
    if (!dragging_ || !event || !hud_) {
      QWidget::mouseMoveEvent(event);
      return;
    }
    QPoint position = dragStartPosition_ +
                      event->globalPosition().toPoint() - dragStartGlobal_;
    const QRect bounds = hud_->property("artifactHudViewportBounds").toRect();
    if (bounds.isValid()) {
      position.setX(std::clamp(position.x(), bounds.left(),
                               std::max(bounds.left(), bounds.right() - hud_->width() + 1)));
      position.setY(std::clamp(position.y(), bounds.top(),
                               std::max(bounds.top(), bounds.bottom() - hud_->height() + 1)));
    }
    hud_->move(position);
    event->accept();
  }

  void mouseReleaseEvent(QMouseEvent *event) override {
    if (dragging_ && event && event->button() == Qt::LeftButton && hud_) {
      dragging_ = false;
      const QRect bounds = hud_->property("artifactHudViewportBounds").toRect();
      const QPoint offset = hud_->pos() - bounds.topLeft();
      hud_->setProperty("artifactHudOffset", offset);
      ArtifactCore::LayeredConfigStore::instance().setValue(settingsKey_, offset);
      event->accept();
      return;
    }
    QWidget::mouseReleaseEvent(event);
  }

private:
  QToolBar *hud_ = nullptr;
  QString settingsKey_;
  QPoint dragStartGlobal_;
  QPoint dragStartPosition_;
  bool dragging_ = false;
};

class CompositionOverlayWidget final : public QWidget {
public:
  explicit CompositionOverlayWidget(CompositionViewport *viewport,
                                    QWidget *parent = nullptr)
      : QWidget(parent), viewport_(viewport) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);

    navigationLabel_ = new QLabel(this);
    navigationLabel_->setAttribute(Qt::WA_TransparentForMouseEvents);
    navigationLabel_->setAlignment(Qt::AlignCenter);
    navigationLabel_->setContentsMargins(14, 4, 14, 4);
    navigationLabel_->setAutoFillBackground(true);
    QPalette navigationPalette = navigationLabel_->palette();
    navigationPalette.setColor(QPalette::Window, QColor(12, 16, 22, 220));
    navigationPalette.setColor(QPalette::WindowText,
                               QColor(226, 241, 252, 236));
    navigationLabel_->setPalette(navigationPalette);
    QFont navigationFont = navigationLabel_->font();
    navigationFont.setBold(true);
    navigationLabel_->setFont(navigationFont);
    navigationLabel_->hide();

    previewLabel_ = new QLabel(this);
    previewLabel_->setAttribute(Qt::WA_TransparentForMouseEvents);
    previewLabel_->setAlignment(Qt::AlignCenter);
    previewLabel_->setContentsMargins(12, 4, 12, 4);
    previewLabel_->setAutoFillBackground(true);
    QPalette previewPalette = previewLabel_->palette();
    previewPalette.setColor(QPalette::Window, QColor(56, 34, 12, 214));
    previewPalette.setColor(QPalette::WindowText, QColor(255, 229, 197, 240));
    previewLabel_->setPalette(previewPalette);
    QFont previewFont = previewLabel_->font();
    previewFont.setBold(true);
    previewLabel_->setFont(previewFont);
    previewLabel_->hide();

    resizeFrameTimer_ = new QTimer(this);
    resizeFrameTimer_->setInterval(90);
    QObject::connect(resizeFrameTimer_, &QTimer::timeout, this, [this]() {
      resizeFramePhase_ = (resizeFramePhase_ + 1) % 8;
      update();
    });
  }

  void syncToViewport() {
    if (!viewport_) {
      hide();
      return;
    }
    setGeometry(viewport_->geometry());
    raise();
    show();
    update();
  }

  void setActivePaneIndicatorProvider(
      std::function<std::optional<std::pair<QRect, QString>>()> provider) {
    activePaneIndicatorProvider_ = std::move(provider);
    update();
  }

  void setNavigationFeedbackProvider(std::function<QString()> provider) {
    navigationFeedbackProvider_ = std::move(provider);
    update();
  }

  void setPreviewBadgeProvider(std::function<QString()> provider) {
    previewBadgeProvider_ = std::move(provider);
    update();
  }

  void setResizeIndicatorProvider(std::function<bool()> provider) {
    resizeIndicatorProvider_ = std::move(provider);
    updateResizeIndicatorAnimation();
    update();
  }

protected:
  void paintEvent(QPaintEvent *) override {
    Q_UNUSED(viewport_);
    refreshNavigationFeedback();
    refreshPreviewBadge();
    updateResizeIndicatorAnimation();
    if (!activePaneIndicatorProvider_) {
      return;
    }
    const auto indicator = activePaneIndicatorProvider_();
    if (!indicator.has_value()) {
      return;
    }

    const QRect paneRect = indicator->first;
    const QString label = indicator->second;
    if (!paneRect.isValid()) {
      return;
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRect frameRect = paneRect.adjusted(1, 1, -1, -1);
    QPen framePen(QColor(124, 214, 255, 245), 3.0);
    if (resizeIndicatorProvider_ && resizeIndicatorProvider_()) {
      framePen.setStyle(Qt::DashLine);
      framePen.setDashPattern({5.0, 3.0});
      framePen.setDashOffset(static_cast<qreal>(resizeFramePhase_) * -1.2);
    }
    p.setPen(framePen);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(frameRect, 8.0, 8.0);

    if (!label.isEmpty()) {
      QFont font = p.font();
      font.setPointSizeF(std::max(8.5, font.pointSizeF() - 0.5));
      font.setBold(true);
      p.setFont(font);
      const QFontMetrics fm(font);
      const int chipW = std::max(84, fm.horizontalAdvance(label) + 24);
      const QRect chipRect(frameRect.left() + 10, frameRect.top() + 10, chipW, 24);
      p.setPen(Qt::NoPen);
      p.setBrush(QColor(14, 18, 26, 212));
      p.drawRoundedRect(chipRect, 12.0, 12.0);
      p.setPen(QColor(226, 241, 252, 236));
      p.drawText(chipRect, Qt::AlignCenter, label);
    }
  }

private:
  void updateResizeIndicatorAnimation() {
    const bool animate =
        resizeIndicatorProvider_ && resizeIndicatorProvider_();
    if (animate) {
      if (!resizeFrameTimer_->isActive()) {
        resizeFrameTimer_->start();
      }
      return;
    }
    if (resizeFrameTimer_->isActive()) {
      resizeFrameTimer_->stop();
    }
    resizeFramePhase_ = 0;
  }

  void refreshNavigationFeedback() {
    if (!navigationLabel_) {
      return;
    }
    const QString label =
        navigationFeedbackProvider_ ? navigationFeedbackProvider_() : QString{};
    if (label.isEmpty()) {
      navigationLabel_->hide();
      return;
    }
    navigationLabel_->setText(label);
    navigationLabel_->adjustSize();
    navigationLabel_->resize(std::max(76, navigationLabel_->width()), 28);
    navigationLabel_->move(
        std::max(12, (width() - navigationLabel_->width()) / 2),
        std::max(12, height() - navigationLabel_->height() - 18));
    navigationLabel_->show();
    navigationLabel_->raise();
  }

  void refreshPreviewBadge() {
    if (!previewLabel_) {
      return;
    }
    const QString label =
        previewBadgeProvider_ ? previewBadgeProvider_() : QString{};
    if (label.isEmpty()) {
      previewLabel_->hide();
      return;
    }
    previewLabel_->setText(label);
    previewLabel_->adjustSize();
    previewLabel_->resize(std::max(84, previewLabel_->width()), 28);
    previewLabel_->move(12, 12);
    previewLabel_->show();
    previewLabel_->raise();
  }

  CompositionViewport *viewport_ = nullptr;
  QLabel *navigationLabel_ = nullptr;
  QLabel *previewLabel_ = nullptr;
  QTimer *resizeFrameTimer_ = nullptr;
  int resizeFramePhase_ = 0;
  std::function<std::optional<std::pair<QRect, QString>>()> activePaneIndicatorProvider_;
  std::function<QString()> navigationFeedbackProvider_;
  std::function<QString()> previewBadgeProvider_;
  std::function<bool()> resizeIndicatorProvider_;
};

} // namespace

void openContentsViewerCompareSurface() {
  openContentsViewerCompareSurfaceImpl();
}

class ArtifactCompositionEditor::Impl {
public:
  enum class ImportPlacementSizeMode {
    Original = 0,
    Fit = 1,
    Fill = 2,
    Stretch = 3,
  };

  struct ImportPlacementSession {
    bool active = false;
    bool sizeAdjustSupported = true;
    ArtifactAbstractLayerPtr targetLayer;
    QString sourcePath;
    QString layerName;
    QSize sourceSize;
    QSize compositionSize;
    QPointF placementCenter;
    QRectF originalBounds;
    ImportPlacementSizeMode sizeMode = ImportPlacementSizeMode::Original;
    bool committed = false;
  };

  enum class ViewportLayoutMode {
    Single = 1,
    TwoUp = 2,
    FourUp = 4,
  };

  enum class WorkspaceMode {
    Design,
    Animate,
  };

  static constexpr int kViewportPaneCount = 4;

  struct PaneState {
    int paneId = 0;
    QRect rect;
    CompositionViewport *view = nullptr;
    CompositionRenderController *controller = nullptr;
    bool visible = false;
  };

  CompositionViewport *compositionView_ = nullptr;
  QWidget *viewportHost_ = nullptr;
  QSplitter *viewportRowsSplitter_ = nullptr;
  QSplitter *viewportTopSplitter_ = nullptr;
  QSplitter *viewportBottomSplitter_ = nullptr;
  std::array<PaneState, kViewportPaneCount> panes_{{
      {0, QRect(), nullptr, nullptr, false},
      {1, QRect(), nullptr, nullptr, false},
      {2, QRect(), nullptr, nullptr, false},
      {3, QRect(), nullptr, nullptr, false},
  }};
  CompositionOverlayWidget *overlayView_ = nullptr;
  std::array<EmptyCompositionOverlayWidget *, kViewportPaneCount>
      emptyStateOverlays_{{nullptr, nullptr, nullptr, nullptr}};
  ViewOrientationWidget *viewOrientationWidget_ = nullptr;
  CompositionRenderController *renderController_ = nullptr;
  ViewportLayoutButton *viewportLayoutButton_ = nullptr;
  ViewportLayoutButton *compositionCleanupButton_ = nullptr;
  ViewportLayoutButton *workspaceModeButton_ = nullptr;
  WorkspaceMode workspaceMode_ = WorkspaceMode::Animate;
  ViewportLayoutMode viewportLayoutMode_ = ViewportLayoutMode::Single;
  int activePaneId_ = 0;
  // Top Toolbar (Zoom/View controls)
  QToolBar *topToolbar_ = nullptr;
  QToolBar *toolHud_ = nullptr;
  QToolBar *zoomHud_ = nullptr;
  bool viewportToolboxesVisible_ = true;
  QFrame *chromeStrip_ = nullptr;
  QLabel *chromeTitleLabel_ = nullptr;
  QLabel *chromeDetailLabel_ = nullptr;
  QLabel *chromeMetaLabel_ = nullptr;
  QAction *resetAction_ = nullptr;
  QAction *zoomInAction_ = nullptr;
  QAction *zoomOutAction_ = nullptr;
  QAction *zoomFitAction_ = nullptr;
  QAction *zoom100Action_ = nullptr;
  QAction *editTextAction_ = nullptr;
  QToolButton *screenshotButton_ = nullptr;
  QToolButton *viewportRenderOutputButton_ = nullptr;
  QAction *quickScreenshotAction_ = nullptr;
  QAction *advancedScreenshotAction_ = nullptr;
  QAction *viewportRenderOutputAction_ = nullptr;
  QAction *compareAction_ = nullptr;
  QAction *motionPathAction_ = nullptr;
  QAction *effectHitboxAction_ = nullptr;
  QAction *layerChromeAction_ = nullptr;
  QAction *lockViewAction_ = nullptr;
  QAction *autoFourUpAction_ = nullptr;
  QAction *densityHeatmapAction_ = nullptr;
  QAction *gizmoVisibleAction_ = nullptr;
  QAction *xRayAction_ = nullptr;
  QAction *isolationAction_ = nullptr;
  QToolButton *shadingButton_ = nullptr;
  QToolButton *viewPresetButton_ = nullptr;
  QToolButton *viewportBookmarkButton_ = nullptr;
  QAction *renderSuspendAction_ = nullptr;
  QAction *previewOrbitAction_ = nullptr;
  QAction *vectorScopeAction_ = nullptr;
  QPointer<QDialog> vectorScopeDialog_;
  QToolButton *toolModeButton_ = nullptr;
  QToolButton *gizmoModeButton_ = nullptr;
  QToolButton *pivotModeButton_ = nullptr;
  QAction *immersiveAction_ = nullptr;
  bool immersiveMode_ = false;
  struct PreviewOrbitSnapshot {
    QQuaternion orientation;
    QPointF pan;
    float zoom = 1.0f;
  };
  QHash<CompositionRenderController *, PreviewOrbitSnapshot>
      previewOrbitSnapshots_;
  bool previewOrbitMode_ = false;
  ViewportChannelDisplayMode viewportChannelDisplayMode_ =
      ViewportChannelDisplayMode::Color;
  DisplayMode lastToolServiceDisplayMode_ = DisplayMode::Color;
  std::optional<QQuaternion> pendingViewCubeOrientation_;
  bool viewCubeUpdateQueued_ = false;

  PaneState *pane(int paneId) {
    if (paneId < 0 || paneId >= kViewportPaneCount) {
      return nullptr;
    }
    return &panes_[paneId];
  }

  const PaneState *pane(int paneId) const {
    if (paneId < 0 || paneId >= kViewportPaneCount) {
      return nullptr;
    }
    return &panes_[paneId];
  }

  PaneState *activePane() { return pane(activePaneId_); }

  const PaneState *activePane() const { return pane(activePaneId_); }

  CompositionViewport *activeViewport() const {
    if (const auto *paneState = activePane()) {
      if (paneState->view) {
        return paneState->view;
      }
    }
    return compositionView_;
  }

  CompositionRenderController *activeRenderController() const {
    if (const auto *paneState = activePane()) {
      if (paneState->controller) {
        return paneState->controller;
      }
    }
    return renderController_;
  }

  void setActivePane(ArtifactCompositionEditor *owner, int paneId) {
    const int paneCount = activeViewportPaneCount();
    const int clampedPaneId =
        std::clamp(paneId, 0, std::max(0, paneCount - 1));
    if (activePaneId_ == clampedPaneId) {
      return;
    }
    activePaneId_ = clampedPaneId;
    if (const auto *toolManager =
            ArtifactApplicationManager::instance()
                ? ArtifactApplicationManager::instance()->toolManager()
                : nullptr;
        toolManager && (toolManager->activeTool() == ToolType::Pen ||
                        toolManager->activeTool() == ToolType::Shape)) {
      forceFrontForPlanarEditingTool(toolManager->activeTool());
    }
    syncOverlayGeometry(owner);
    if (overlayView_) {
      overlayView_->update();
    }
  }

  int activeViewportPaneCount() const {
    switch (viewportLayoutMode_) {
    case ViewportLayoutMode::Single:
      return 1;
    case ViewportLayoutMode::TwoUp:
      return 2;
    case ViewportLayoutMode::FourUp:
      return 4;
    }
    return 1;
  }

  QString viewportLayoutLabel() const {
    switch (viewportLayoutMode_) {
    case ViewportLayoutMode::Single:
      return QStringLiteral("1 View");
    case ViewportLayoutMode::TwoUp:
      return QStringLiteral("2-Up");
    case ViewportLayoutMode::FourUp:
      return QStringLiteral("4-Up");
    }
    return QStringLiteral("1 View");
  }

  QString activePaneViewLabel() const {
    auto *controller = activeRenderController();
    const int paneCount = activeViewportPaneCount();
    const int paneIndex = std::clamp(activePaneId_ + 1, 1, std::max(1, paneCount));
    const QString panePrefix = paneCount > 1
                                   ? QStringLiteral("Pane %1/%2 · ")
                                         .arg(paneIndex)
                                         .arg(paneCount)
                                   : QString{};
    if (!controller) {
      return panePrefix.isEmpty() ? QStringLiteral("Active View")
                                  : QStringLiteral("%1Active View").arg(panePrefix);
    }
    const auto makeLabel = [&panePrefix](const QString &label) {
      return panePrefix.isEmpty() ? label : QStringLiteral("%1%2").arg(panePrefix, label);
    };
    switch (controller->viewportOrientation()) {
    case ArtifactCore::ViewOrientationHotspot::Top:
      return makeLabel(QStringLiteral("Active: Top"));
    case ArtifactCore::ViewOrientationHotspot::Bottom:
      return makeLabel(QStringLiteral("Active: Bottom"));
    case ArtifactCore::ViewOrientationHotspot::Left:
      return makeLabel(QStringLiteral("Active: Left"));
    case ArtifactCore::ViewOrientationHotspot::Right:
      return makeLabel(QStringLiteral("Active: Right"));
    case ArtifactCore::ViewOrientationHotspot::Back:
      return makeLabel(QStringLiteral("Active: Back"));
    case ArtifactCore::ViewOrientationHotspot::Front:
      return makeLabel(QStringLiteral("Active: Front"));
    case ArtifactCore::ViewOrientationHotspot::FrontTop:
      return makeLabel(QStringLiteral("Active: Front Top"));
    case ArtifactCore::ViewOrientationHotspot::FrontBottom:
      return makeLabel(QStringLiteral("Active: Front Bottom"));
    case ArtifactCore::ViewOrientationHotspot::FrontLeft:
      return makeLabel(QStringLiteral("Active: Front Left"));
    case ArtifactCore::ViewOrientationHotspot::FrontRight:
      return makeLabel(QStringLiteral("Active: Front Right"));
    case ArtifactCore::ViewOrientationHotspot::BackTop:
      return makeLabel(QStringLiteral("Active: Back Top"));
    case ArtifactCore::ViewOrientationHotspot::BackBottom:
      return makeLabel(QStringLiteral("Active: Back Bottom"));
    case ArtifactCore::ViewOrientationHotspot::BackLeft:
      return makeLabel(QStringLiteral("Active: Back Left"));
    case ArtifactCore::ViewOrientationHotspot::BackRight:
      return makeLabel(QStringLiteral("Active: Back Right"));
    case ArtifactCore::ViewOrientationHotspot::LeftTop:
      return makeLabel(QStringLiteral("Active: Left Top"));
    case ArtifactCore::ViewOrientationHotspot::LeftBottom:
      return makeLabel(QStringLiteral("Active: Left Bottom"));
    case ArtifactCore::ViewOrientationHotspot::RightTop:
      return makeLabel(QStringLiteral("Active: Right Top"));
    case ArtifactCore::ViewOrientationHotspot::RightBottom:
      return makeLabel(QStringLiteral("Active: Right Bottom"));
    case ArtifactCore::ViewOrientationHotspot::FrontTopLeft:
      return makeLabel(QStringLiteral("Active: Front Top Left"));
    case ArtifactCore::ViewOrientationHotspot::FrontTopRight:
      return makeLabel(QStringLiteral("Active: Front Top Right"));
    case ArtifactCore::ViewOrientationHotspot::FrontBottomLeft:
      return makeLabel(QStringLiteral("Active: Front Bottom Left"));
    case ArtifactCore::ViewOrientationHotspot::FrontBottomRight:
      return makeLabel(QStringLiteral("Active: Front Bottom Right"));
    case ArtifactCore::ViewOrientationHotspot::BackTopLeft:
      return makeLabel(QStringLiteral("Active: Back Top Left"));
    case ArtifactCore::ViewOrientationHotspot::BackTopRight:
      return makeLabel(QStringLiteral("Active: Back Top Right"));
    case ArtifactCore::ViewOrientationHotspot::BackBottomLeft:
      return makeLabel(QStringLiteral("Active: Back Bottom Left"));
    case ArtifactCore::ViewOrientationHotspot::BackBottomRight:
      return makeLabel(QStringLiteral("Active: Back Bottom Right"));
    case ArtifactCore::ViewOrientationHotspot::None:
      return makeLabel(QStringLiteral("Active: Perspective"));
    }
    return makeLabel(QStringLiteral("Active View"));
  }

  bool activePaneResizePending() const {
    const auto *paneState = activePane();
    return paneState && paneState->view && paneState->view->isResizePending();
  }

  std::optional<std::pair<QRect, QString>> activePaneIndicator() const {
    if (activeViewportPaneCount() <= 1) {
      return std::nullopt;
    }
    const auto *paneState = activePane();
    if (!paneState || !paneState->view || !paneState->view->isVisible()) {
      return std::nullopt;
    }
    return std::make_pair(QRect(QPoint(0, 0), paneState->view->size()),
                          activePaneViewLabel());
  }

  QString activeNavigationFeedbackLabel() const {
    if (const auto *viewport = activeViewport()) {
      return viewport->navigationFeedbackLabel();
    }
    return {};
  }

  ViewportLayoutMode nextViewportLayoutMode() const {
    switch (viewportLayoutMode_) {
    case ViewportLayoutMode::Single:
      return ViewportLayoutMode::TwoUp;
    case ViewportLayoutMode::TwoUp:
      return ViewportLayoutMode::FourUp;
    case ViewportLayoutMode::FourUp:
      return ViewportLayoutMode::Single;
    }
    return ViewportLayoutMode::Single;
  }

  void forEachRenderController(const std::function<void(CompositionRenderController *)> &fn) {
    for (const auto &paneState : panes_) {
      if (paneState.controller) {
        fn(paneState.controller);
      }
    }
  }

  void forEachActiveSecondaryController(
      const std::function<void(CompositionRenderController *)> &fn) {
    for (int i = 1; i < activeViewportPaneCount(); ++i) {
      if (const auto *paneState = pane(i); paneState && paneState->controller) {
        fn(paneState->controller);
      }
    }
  }

  void forEachSecondaryController(
      const std::function<void(CompositionRenderController *)> &fn) {
    for (int i = 1; i < kViewportPaneCount; ++i) {
      if (const auto *paneState = pane(i); paneState && paneState->controller) {
        fn(paneState->controller);
      }
    }
  }

  void forEachActiveViewport(const std::function<void(CompositionViewport *, int)> &fn) {
    const int paneCount = activeViewportPaneCount();
    for (int i = 0; i < paneCount; ++i) {
      if (const auto *paneState = pane(i); paneState && paneState->view) {
        fn(paneState->view, i);
      }
    }
  }

  std::array<QRect, kViewportPaneCount> computePaneRects(const QRect &hostRect) const {
    std::array<QRect, kViewportPaneCount> rects{};
    if (!hostRect.isValid()) {
      return rects;
    }

    switch (viewportLayoutMode_) {
    case ViewportLayoutMode::Single:
      rects[0] = hostRect;
      break;
    case ViewportLayoutMode::TwoUp: {
      const int leftWidth = hostRect.width() / 2;
      rects[0] = QRect(hostRect.left(), hostRect.top(), leftWidth, hostRect.height());
      rects[1] = QRect(hostRect.left() + leftWidth, hostRect.top(),
                       hostRect.width() - leftWidth, hostRect.height());
      break;
    }
    case ViewportLayoutMode::FourUp: {
      const int topHeight = hostRect.height() / 2;
      const int leftWidth = hostRect.width() / 2;
      rects[0] = QRect(hostRect.left(), hostRect.top(), leftWidth, topHeight);
      rects[1] = QRect(hostRect.left() + leftWidth, hostRect.top(),
                       hostRect.width() - leftWidth, topHeight);
      rects[2] = QRect(hostRect.left(), hostRect.top() + topHeight,
                       leftWidth, hostRect.height() - topHeight);
      rects[3] = QRect(hostRect.left() + leftWidth, hostRect.top() + topHeight,
                       hostRect.width() - leftWidth, hostRect.height() - topHeight);
      break;
    }
    }

    return rects;
  }

  void applyViewportLayout() {
    const int paneCount = activeViewportPaneCount();
    for (int i = 0; i < kViewportPaneCount; ++i) {
      if (auto *paneState = pane(i)) {
        paneState->visible = i < paneCount;
        if (paneState->view) {
          paneState->view->setVisible(paneState->visible);
        }
      }
    }
    if (viewportHost_) {
      const auto rects = computePaneRects(viewportHost_->rect());
      for (int i = 0; i < kViewportPaneCount; ++i) {
        if (auto *paneState = pane(i)) {
          paneState->rect = rects[i];
        }
      }
    }
    activePaneId_ = std::clamp(activePaneId_, 0, std::max(0, paneCount - 1));
    if (viewportLayoutButton_) {
      viewportLayoutButton_->setText(viewportLayoutLabel());
    }
    if (viewportTopSplitter_) {
      if (paneCount <= 1) {
        viewportTopSplitter_->setSizes({1, 0});
      } else {
        const int paneWidth = std::max(1, viewportTopSplitter_->width() / 2);
        viewportTopSplitter_->setSizes({paneWidth, paneWidth});
      }
    }
    if (viewportBottomSplitter_) {
      const bool showBottomRow = paneCount >= 4;
      viewportBottomSplitter_->setVisible(showBottomRow);
      if (showBottomRow) {
        const int paneWidth = std::max(1, viewportBottomSplitter_->width() / 2);
        viewportBottomSplitter_->setSizes({paneWidth, paneWidth});
      }
    }
    if (viewportRowsSplitter_) {
      if (paneCount >= 4) {
        const int paneHeight = std::max(1, viewportRowsSplitter_->height() / 2);
        viewportRowsSplitter_->setSizes({paneHeight, paneHeight});
      } else {
        viewportRowsSplitter_->setSizes({1, 0});
      }
    }
  }

  // Bottom Viewer Controls
  QWidget *bottomBar_ = nullptr;
  QComboBox *resolutionCombo_ = nullptr;
  QToolButton *fastPreviewBtn_ = nullptr;
  QToolButton *hdrDisplayBtn_ = nullptr;
  QToolButton *displayOptionsBtn_ = nullptr;
  bool compactViewportControls_ = false;
  bool layerChromeVisible_ = true;
  bool lockViewToSelection_ = false;
  bool autoAssignFourUpViews_ = true;
  bool screenshotExportInProgress_ = false;
  quint64 viewportLayoutGeneration_ = 0;

  void refreshHDRDisplayState() {
    if (!hdrDisplayBtn_) {
      return;
    }
    const bool hdr = Artifact::RenderConfig::hdrDisplayEnabled();
    hdrDisplayBtn_->setText(hdr ? QStringLiteral("HDR")
                               : QStringLiteral("SDR"));
    hdrDisplayBtn_->setToolTip(
        hdr ? QStringLiteral("HDR display (scRGB)")
            : QStringLiteral("SDR display (sRGB)"));
  }

  bool selectionSyncQueued_ = false;
  bool toolLabelSyncQueued_ = false;
  std::chrono::steady_clock::time_point lastMaskShortcutPressTime_{};
  bool lastMaskShortcutPressValid_ = false;
  ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;
  ProfilerOverlayWidget *profilerOverlay_ = nullptr;
  ProfilerPanelWidget *profilerPanel_ = nullptr;
  EventBusDebuggerWidget *eventBusDebugger_ = nullptr;
  int startupCompositionRetryCount_ = 0;
  ImportPlacementSession importPlacementSession_;

  // 外部 signal から即時に widget を書き換えず、イベントループの次 tick
  // にまとめて反映する。
  void queueSelectionSync(ArtifactCompositionEditor *owner) {
    if (!owner || selectionSyncQueued_) {
      return;
    }
    selectionSyncQueued_ = true;
    QCoreApplication::postEvent(
        owner, new CompositionEditorDeferredEvent(
                   CompositionEditorDeferredEvent::Kind::SelectionSync));
  }

  void queueToolLabelSync(ArtifactCompositionEditor *owner) {
    if (!owner || toolLabelSyncQueued_) {
      return;
    }
    toolLabelSyncQueued_ = true;
    QCoreApplication::postEvent(
        owner, new CompositionEditorDeferredEvent(
                   CompositionEditorDeferredEvent::Kind::ToolLabelSync));
  }

  void activateMaskEditingTool() {
    if (auto *toolManager =
            ArtifactApplicationManager::instance()
                ? ArtifactApplicationManager::instance()->toolManager()
                : nullptr) {
      toolManager->setActiveTool(ToolType::Pen);
    }
  }

  bool isMaskNavigationLocked() const {
    const auto *app = ArtifactApplicationManager::instance();
    const auto *toolManager = app ? app->toolManager() : nullptr;
    if (toolManager && toolManager->activeTool() == ToolType::Pen) {
      return true;
    }
    const auto *controller = activeRenderController();
    return controller && controller->hasPendingMaskEdit();
  }

  void forceFrontForPlanarEditingTool(ToolType toolType) {
    if (toolType != ToolType::Pen && toolType != ToolType::Shape) {
      return;
    }
    auto *controller = activeRenderController();
    if (!controller) {
      return;
    }
    controller->setViewportOrientation(
        ArtifactCore::ViewOrientationHotspot::Front);
    if (viewOrientationWidget_) {
      viewOrientationWidget_->setOrientationQuaternion(
          controller->viewportOrientationQuaternion());
    }
    if (overlayView_) {
      overlayView_->update();
    }
  }

  bool isMaskEditingToolActive() const {
    auto *toolManager =
        ArtifactApplicationManager::instance()
            ? ArtifactApplicationManager::instance()->toolManager()
            : nullptr;
    return toolManager && toolManager->activeTool() == ToolType::Pen;
  }

  void showMaskEditingGuides() {
    if (!renderController_) {
      return;
    }
    renderController_->setLineDebugKindVisible(LineDebugKind::MaskPath, true);
    renderController_->setLineDebugKindVisible(LineDebugKind::MaskHandle, true);
  }

  void syncChromeSummary(ArtifactCompositionEditor *owner) {
    Q_UNUSED(owner);
    if (!chromeStrip_ || !chromeTitleLabel_ || !chromeDetailLabel_ ||
        !chromeMetaLabel_) {
      return;
    }

    const auto comp = renderController_ ? renderController_->composition()
                                        : ArtifactCompositionPtr{};
    auto *selection = ArtifactLayerSelectionManager::instance();
    const auto current =
        selection ? selection->currentLayer() : ArtifactAbstractLayerPtr{};
    const int selectedCount =
        selection ? selection->selectedLayers().size() : 0;
    const QString compName =
        comp ? comp->settings().compositionName().toQString()
             : QStringLiteral("<no composition>");
    const QString layerName = current
                                  ? (current->layerName().trimmed().isEmpty()
                                         ? current->id().toString()
                                         : current->layerName().trimmed())
                                  : QStringLiteral("<none>");
    chromeTitleLabel_->setText(QStringLiteral("Composition: %1").arg(compName));
    chromeDetailLabel_->setText(
        QStringLiteral("Layer: %1  |  Selection: %2")
            .arg(layerName)
            .arg(selectedCount));
    chromeMetaLabel_->hide();
  }

  void openCreateCompositionDialog(ArtifactCompositionEditor *owner) {
    QWidget *dialogParent = owner ? owner->window() : nullptr;
    CreateCompositionDialog dialog(dialogParent);
    if (dialog.exec() == QDialog::Accepted) {
      const ArtifactCompositionInitParams params = dialog.acceptedInitParams();
      QTimer::singleShot(0, dialogParent ? dialogParent : owner, [params]() {
        if (auto *service = ArtifactProjectService::instance()) {
          service->createComposition(params);
        }
      });
    }
  }

  QString importPlacementModeLabel() const {
    switch (importPlacementSession_.sizeMode) {
    case ImportPlacementSizeMode::Original:
      return QStringLiteral("Original");
    case ImportPlacementSizeMode::Fit:
      return QStringLiteral("Fit");
    case ImportPlacementSizeMode::Fill:
      return QStringLiteral("Fill");
    case ImportPlacementSizeMode::Stretch:
      return QStringLiteral("Stretch");
    }
    return QStringLiteral("Original");
  }

  QRectF importPlacementBounds(ImportPlacementSizeMode mode) const {
    if (!importPlacementSession_.sizeAdjustSupported) {
      return QRectF();
    }
    if (!importPlacementSession_.compositionSize.isValid() ||
        !importPlacementSession_.sourceSize.isValid() ||
        importPlacementSession_.sourceSize.width() <= 0 ||
        importPlacementSession_.sourceSize.height() <= 0) {
      return QRectF();
    }
    const double compW = std::max<double>(1.0, static_cast<double>(importPlacementSession_.compositionSize.width()));
    const double compH = std::max<double>(1.0, static_cast<double>(importPlacementSession_.compositionSize.height()));
    const double srcW = std::max<double>(1.0, static_cast<double>(importPlacementSession_.sourceSize.width()));
    const double srcH = std::max<double>(1.0, static_cast<double>(importPlacementSession_.sourceSize.height()));
    double w = srcW;
    double h = srcH;
    switch (mode) {
    case ImportPlacementSizeMode::Original:
      break;
    case ImportPlacementSizeMode::Fit: {
      const double scale = std::min(compW / srcW, compH / srcH);
      w = srcW * scale;
      h = srcH * scale;
      break;
    }
    case ImportPlacementSizeMode::Fill: {
      const double scale = std::max<double>(compW / srcW, compH / srcH);
      w = srcW * scale;
      h = srcH * scale;
      break;
    }
    case ImportPlacementSizeMode::Stretch:
      w = compW;
      h = compH;
      break;
    }
    const QPointF center = importPlacementSession_.placementCenter;
    return QRectF(center.x() - w * 0.5, center.y() - h * 0.5, w, h);
  }

  void applyImportPlacementMode() {
    auto layer = importPlacementSession_.targetLayer;
    if (!renderController_ || !layer) {
      return;
    }
    const QRectF bounds = importPlacementBounds(importPlacementSession_.sizeMode);
    if (!bounds.isValid()) {
      return;
    }
    const auto comp = ArtifactProjectService::instance()
                          ? ArtifactProjectService::instance()->currentComposition().lock()
                          : ArtifactCompositionPtr{};
    const ArtifactCore::RationalTime time(
        comp ? comp->framePosition().framePosition() : 0, 30000);
    auto& t3 = layer->transform3D();
    t3.setPosition(time, static_cast<float>(bounds.center().x()),
                   static_cast<float>(bounds.center().y()));
    t3.setScale(time,
                static_cast<float>(bounds.width() /
                                   std::max<double>(1.0, static_cast<double>(importPlacementSession_.sourceSize.width()))),
                static_cast<float>(bounds.height() /
                                   std::max<double>(1.0, static_cast<double>(importPlacementSession_.sourceSize.height()))));
    layer->setDirty(LayerDirtyFlag::Transform);
    layer->changed();
  }

  void refreshImportPlacementOverlay(ArtifactCompositionEditor *owner) {
    if (!renderController_) {
      return;
    }
    if (!importPlacementSession_.active) {
      renderController_->clearInfoOverlayText();
      syncSelectionState(owner);
      return;
    }
    const auto &shortcuts = ArtifactCore::ShortcutBindings::instance();
    const QString detail =
        importPlacementSession_.sizeAdjustSupported
            ? QStringLiteral("Mode: %1 | S:%2  Shift+S:%3  Enter:%4  Esc:%5  R:%6")
                  .arg(importPlacementModeLabel(),
                       shortcuts.shortcutText(ArtifactCore::ShortcutId::ImportPlacementNextSizeMode),
                       shortcuts.shortcutText(ArtifactCore::ShortcutId::ImportPlacementPreviousSizeMode),
                       shortcuts.shortcutText(ArtifactCore::ShortcutId::ImportPlacementConfirm),
                       shortcuts.shortcutText(ArtifactCore::ShortcutId::ImportPlacementCancel),
                       shortcuts.shortcutText(ArtifactCore::ShortcutId::ImportPlacementReset))
            : QStringLiteral("Mode: Placement | Enter:%1  Esc:%2")
                  .arg(shortcuts.shortcutText(ArtifactCore::ShortcutId::ImportPlacementConfirm),
                       shortcuts.shortcutText(ArtifactCore::ShortcutId::ImportPlacementCancel));
    renderController_->setInfoOverlayText(QStringLiteral("Smart Import Placement"), detail);
    syncOverlayGeometry(owner);
  }

  void startImportPlacementSession(ArtifactCompositionEditor *owner,
                                   const QString &sourcePath,
                                   const QString &layerName,
                                   const QSize &sourceSize,
                                   const ArtifactAbstractLayerPtr &targetLayer,
                                   bool sizeAdjustSupported = true) {
    auto *svc = ArtifactProjectService::instance();
    auto comp = svc ? svc->currentComposition().lock() : ArtifactCompositionPtr{};
    if (!svc || !comp || !targetLayer) {
      return;
    }
    importPlacementSession_ = {};
    importPlacementSession_.active = true;
    importPlacementSession_.targetLayer = targetLayer;
    importPlacementSession_.sizeAdjustSupported = sizeAdjustSupported;
    importPlacementSession_.sourcePath = sourcePath;
    importPlacementSession_.layerName = layerName;
    importPlacementSession_.sourceSize = sourceSize;
    importPlacementSession_.compositionSize = comp->settings().compositionSize();
    importPlacementSession_.placementCenter = QPointF(
        importPlacementSession_.compositionSize.width() * 0.5,
        importPlacementSession_.compositionSize.height() * 0.5);
    importPlacementSession_.originalBounds = targetLayer->transformedBoundingBox();
    applyImportPlacementMode();
    refreshImportPlacementOverlay(owner);
  }

  void finishImportPlacementSession(ArtifactCompositionEditor *owner, bool commit) {
    if (!importPlacementSession_.active) {
      return;
    }
    if (!commit && importPlacementSession_.targetLayer) {
      if (auto *svc = ArtifactProjectService::instance()) {
        const auto comp = svc->currentComposition().lock();
        if (comp) {
          svc->removeLayerFromComposition(comp->id(),
                                          importPlacementSession_.targetLayer->id());
        }
      }
    }
    importPlacementSession_ = {};
    if (renderController_) {
      renderController_->clearInfoOverlayText();
    }
    syncSelectionState(owner);
  }

  bool handleImportPlacementKeyPress(ArtifactCompositionEditor *owner,
                                     QKeyEvent *event) {
    if (!event || !importPlacementSession_.active) {
      return false;
    }
    auto &shortcuts = ArtifactCore::ShortcutBindings::instance();
    if (shortcuts.matches(event, ArtifactCore::ShortcutId::ImportPlacementNextSizeMode)) {
      if (!importPlacementSession_.sizeAdjustSupported) {
        return false;
      }
      importPlacementSession_.sizeMode = static_cast<ImportPlacementSizeMode>(
          (static_cast<int>(importPlacementSession_.sizeMode) + 1) % 4);
      applyImportPlacementMode();
      refreshImportPlacementOverlay(owner);
      event->accept();
      return true;
    }
    if (shortcuts.matches(event, ArtifactCore::ShortcutId::ImportPlacementPreviousSizeMode)) {
      if (!importPlacementSession_.sizeAdjustSupported) {
        return false;
      }
      importPlacementSession_.sizeMode = static_cast<ImportPlacementSizeMode>(
          (static_cast<int>(importPlacementSession_.sizeMode) + 3) % 4);
      applyImportPlacementMode();
      refreshImportPlacementOverlay(owner);
      event->accept();
      return true;
    }
    if (shortcuts.matches(event, ArtifactCore::ShortcutId::ImportPlacementReset)) {
      if (!importPlacementSession_.sizeAdjustSupported) {
        return false;
      }
      importPlacementSession_.sizeMode = ImportPlacementSizeMode::Original;
      applyImportPlacementMode();
      refreshImportPlacementOverlay(owner);
      event->accept();
      return true;
    }
    if (shortcuts.matches(event, ArtifactCore::ShortcutId::ImportPlacementConfirm)) {
      finishImportPlacementSession(owner, true);
      event->accept();
      return true;
    }
    if (shortcuts.matches(event, ArtifactCore::ShortcutId::ImportPlacementCancel)) {
      finishImportPlacementSession(owner, false);
      event->accept();
      return true;
    }
    return false;
  }

  void syncSelectionState(ArtifactCompositionEditor *owner) {
    if (!owner || !renderController_) {
      return;
    }
    const auto comp = renderController_->composition();
    auto *selection = ArtifactLayerSelectionManager::instance();
    const auto currentSelection =
        selection ? selection->currentLayer() : ArtifactAbstractLayerPtr{};
    const auto current =
        currentSelection && comp ? comp->layerById(currentSelection->id())
                                 : ArtifactAbstractLayerPtr{};
    const int selectedCount =
        selection ? selection->selectedLayers().size() : 0;
    auto *playback = ArtifactPlaybackService::instance();
    const QString statusText =
        playback && playback->isPlaying() ? QStringLiteral("Status: playback")
                                          : QStringLiteral("Status: idle");

    // Guard: if the selection manager reports no current layer, but the
    // previously-selected layer still exists in the composition, preserve the
    // gizmo. This prevents spurious selection clears triggered by deferred
    // syncSelectionState() calls that race with property-edit notifications.
    if (!current) {
      const auto prevId = renderController_->selectedLayerId();
      if (!prevId.isNil()) {
        if (comp && comp->layerById(prevId)) {
          selectionSyncQueued_ = false;
          return;
        }
      }
    }

    renderController_->setSelectedLayerId(
        current ? current->id() : ArtifactCore::LayerID::Nil());
    if (current) {
      const QString layerName = current->layerName().trimmed();
      const QString title =
          layerName.isEmpty() ? current->id().toString() : layerName;
      const auto *toolManager =
          ArtifactApplicationManager::instance()
              ? ArtifactApplicationManager::instance()->toolManager()
              : nullptr;
      const QString activeToolLabel =
          toolManager && toolManager->activeTool() == ToolType::Pen
              ? QStringLiteral("Mask editing")
              : toolManager && toolManager->activeTool() == ToolType::Shape
                    ? QStringLiteral("Shape modeling")
              : QStringLiteral("Transform/Select");
      const auto compareMode = renderController_->compareMode();
      const QString compareLabel =
          compareMode == CompositionCompareMode::Off
              ? QStringLiteral("Compare: Off")
              : QStringLiteral("Compare: %1")
                    .arg(compareMode == CompositionCompareMode::A
                             ? QStringLiteral("A")
                             : compareMode == CompositionCompareMode::B
                                   ? QStringLiteral("B")
                                   : QStringLiteral("Diff"));
      const QString referenceLabel =
          renderController_->isReferencePinned()
              ? QStringLiteral("Ref: pinned @ frame %1")
                    .arg(renderController_->referenceFrame())
              : QStringLiteral("Ref: free");
      QString detail =
          selectedCount <= 1
              ? QStringLiteral("Selection: 1 layer")
              : QStringLiteral("Selection: %1 layers").arg(selectedCount);
      if (selectedCount == 1) {
        if (const auto shape =
                ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(current)) {
          detail = QStringLiteral("Selection: 1 layer | %1")
                       .arg(shapeSelectionDetail(shape));
        }
      }
      renderController_->setInfoOverlayText(
          QStringLiteral("Current: %1").arg(title),
          QStringLiteral("%1 | %2 | %3 | %4 | %5")
              .arg(detail, activeToolLabel, compareLabel, referenceLabel,
                   statusText));
    } else {
      renderController_->setInfoOverlayText(
          QStringLiteral("Current: Composition Editor"),
          selectedCount <= 0
              ? QStringLiteral("Selection: 0 layers | Tool: Mask editing | Compare: Off | Ref: free | Status: idle | Open a composition")
              : QStringLiteral("Selection: %1 layers | Tool: Mask editing | Compare: Off | Ref: free | %2").arg(selectedCount).arg(statusText));
    }
    if (editTextAction_) {
      editTextAction_->setEnabled(
          selectedCount == 1 && current &&
          ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(current));
    }
    syncChromeSummary(owner);
    syncOverlayGeometry(owner);
  }

  void syncToolLabel(ArtifactCompositionEditor *owner) {
    if (!owner || !toolModeButton_) {
      return;
    }
    auto *app = ArtifactApplicationManager::instance();
    auto *toolManager = app ? app->toolManager() : nullptr;
    const auto type =
        toolManager ? toolManager->activeTool() : ToolType::Selection;
    forceFrontForPlanarEditingTool(type);
    switch (type) {
    case ToolType::Selection:
      toolModeButton_->setText(QStringLiteral("Select"));
      break;
    case ToolType::Hand:
      toolModeButton_->setText(QStringLiteral("Hand"));
      break;
    case ToolType::Pen:
      toolModeButton_->setText(QStringLiteral("Mask"));
      break;
    case ToolType::Shape:
      toolModeButton_->setText(QStringLiteral("Shape"));
      break;
    case ToolType::AnchorPoint:
      toolModeButton_->setText(QStringLiteral("Anchor"));
      break;
    case ToolType::TrackPoint:
      toolModeButton_->setText(QStringLiteral("Track Point"));
      break;
    case ToolType::Brush:
      toolModeButton_->setText(QStringLiteral("Brush"));
      break;
    case ToolType::Clone:
      toolModeButton_->setText(QStringLiteral("Clone"));
      break;
    case ToolType::Eraser:
      toolModeButton_->setText(QStringLiteral("Eraser"));
      break;
    case ToolType::ScrubPreview:
      toolModeButton_->setText(QStringLiteral("Scrub"));
      break;
    case ToolType::RigSelect:
      toolModeButton_->setText(QStringLiteral("Rig Select"));
      break;
    case ToolType::RigWeight:
      toolModeButton_->setText(QStringLiteral("Rig Weight"));
      break;
    case ToolType::Puppet:
      toolModeButton_->setText(QStringLiteral("Puppet"));
      break;
    case ToolType::MotionSketch:
      toolModeButton_->setText(QStringLiteral("Motion Sketch"));
      break;
    case ToolType::Move:
      toolModeButton_->setText(QStringLiteral("Move"));
      break;
    case ToolType::Scale:
      toolModeButton_->setText(QStringLiteral("Scale"));
      break;
    case ToolType::Rotation:
      toolModeButton_->setText(QStringLiteral("Rotate"));
      break;
    case ToolType::Zoom:
      toolModeButton_->setText(QStringLiteral("Zoom"));
      break;
    case ToolType::Ripple:
      toolModeButton_->setText(QStringLiteral("Ripple"));
      break;
    case ToolType::Rolling:
      toolModeButton_->setText(QStringLiteral("Rolling"));
      break;
    case ToolType::Slip:
      toolModeButton_->setText(QStringLiteral("Slip"));
      break;
    case ToolType::Slide:
      toolModeButton_->setText(QStringLiteral("Slide"));
      break;
    default:
      toolModeButton_->setText(QStringLiteral("Tool"));
      break;
    }
  }

  bool syncPreferredComposition(ArtifactCompositionEditor *owner) {
    if (!owner || !renderController_) {
      return false;
    }

    const auto comp = resolvePreferredComposition();
    if (!comp) {
      return false;
    }

    renderController_->setComposition(comp);
    if (compositionView_) {
      compositionView_->requestInitialFit();
    }
    renderController_->markRenderDirty();
    startupCompositionRetryCount_ = 0;
    syncOverlayGeometry(owner);
    return true;
  }

  void syncOverlayGeometry(ArtifactCompositionEditor *owner) {
    if (!owner || !compositionView_) {
      return;
    }
    const auto comp =
        renderController_ ? renderController_->composition()
                          : ArtifactCompositionPtr{};
    const bool hasComposition = static_cast<bool>(comp);
    const bool hasLayers = comp && !comp->allLayerRef().isEmpty();
    syncToolbarVisibility(hasComposition, hasLayers);
    CompositionViewport *overlayViewport = activeViewport();
    if (!overlayViewport) {
      overlayViewport = compositionView_;
    }
    const QPoint viewportTopLeft =
        overlayViewport->mapTo(owner, QPoint(0, 0));
    const QRect viewportGeometry(viewportTopLeft, overlayViewport->size());
    if (viewportGeometry.isEmpty()) {
      return;
    }
    const int overlayInset = 14;
    const int overlayWidth = std::max(0, viewportGeometry.width() -
                                          overlayInset * 2);

    // The bottom controls duplicate commands already available in the View
    // menu.  Keep the full set while it fits, then retain one compact entry
    // point instead of allowing an overlay to extend past the viewport.
    const bool compactControls = overlayWidth < 360;
    const bool controlDensityChanged =
        compactViewportControls_ != compactControls;
    if (controlDensityChanged) {
      compactViewportControls_ = compactControls;
    }
    if (resolutionCombo_) {
      resolutionCombo_->setVisible(!compactControls);
    }
    if (fastPreviewBtn_) {
      fastPreviewBtn_->setVisible(!compactControls);
    }
    if (displayOptionsBtn_) {
      displayOptionsBtn_->setVisible(!compactControls);
    }
    if (controlDensityChanged) {
      refreshViewportStateLabels();
    }

    const auto placeHud = [&](QWidget *hud, const QPoint &defaultPosition) {
      if (!hud) {
        return;
      }
      hud->adjustSize();
      hud->setProperty("artifactHudViewportBounds", viewportGeometry);
      const QVariant savedOffset = hud->property("artifactHudOffset");
      QPoint position = savedOffset.isValid()
                            ? viewportGeometry.topLeft() + savedOffset.toPoint()
                            : defaultPosition;
      position.setX(std::clamp(
          position.x(), viewportGeometry.left(),
          std::max(viewportGeometry.left(),
                   viewportGeometry.right() - hud->width() + 1)));
      position.setY(std::clamp(
          position.y(), viewportGeometry.top(),
          std::max(viewportGeometry.top(),
                   viewportGeometry.bottom() - hud->height() + 1)));
      hud->move(position);
      const bool shouldShow = hasComposition && viewportToolboxesVisible_;
      if (hud->isVisible() != shouldShow) {
        hud->setVisible(shouldShow);
      }
      if (hud->isVisible()) {
        hud->raise();
      }
    };
    if (toolHud_) {
      toolHud_->adjustSize();
      placeHud(toolHud_, QPoint(viewportGeometry.left() + 42,
                                viewportGeometry.top() + 34));
    }
    if (zoomHud_) {
      zoomHud_->adjustSize();
      placeHud(zoomHud_, QPoint(viewportGeometry.center().x() -
                                    zoomHud_->width() / 2,
                                viewportGeometry.top() + 34));
    }

    if (overlayView_) {
      overlayView_->setGeometry(viewportGeometry);
      // A QWidget layered over the native swap-chain surface can occlude the
      // viewport. Keep this legacy full-surface overlay dormant.
      overlayView_->hide();
    }

    const bool showEmptyState = !hasComposition || !hasLayers;
    for (int i = 0; i < kViewportPaneCount; ++i) {
      auto *emptyStateOverlay = emptyStateOverlays_[i];
      const auto *paneState = pane(i);
      // Empty-state guidance belongs to the primary viewport only. Secondary
      // panes remain unobstructed so their native viewport background stays
      // visible in two-up and four-up layouts.
      const bool showInPane =
          showEmptyState && i == 0 && i < activeViewportPaneCount() && paneState &&
          paneState->view && paneState->view->isVisible();
      if (!emptyStateOverlay) {
        continue;
      }
      if (showInPane) {
        const QPoint paneTopLeft =
            paneState->view->mapTo(owner, QPoint(0, 0));
        emptyStateOverlay->setCompositionAvailable(hasComposition);
        const QSize overlaySize =
            emptyStateOverlay->preferredOverlaySize(paneState->view->size());
        const QPoint overlayTopLeft(
            paneTopLeft.x() + (paneState->view->width() - overlaySize.width()) / 2,
            paneTopLeft.y() + (paneState->view->height() - overlaySize.height()) / 2);
        // Keep the transparent native-window overlap limited to the message
        // card itself. The rest of the pane remains the real GPU viewport.
        emptyStateOverlay->setGeometry(QRect(overlayTopLeft, overlaySize));
        if (!emptyStateOverlay->isVisible()) {
          emptyStateOverlay->show();
        }
        emptyStateOverlay->raise();
      } else if (emptyStateOverlay->isVisible()) {
        emptyStateOverlay->hide();
      }
    }
    if (viewOrientationWidget_) {
      const QSize sz = viewOrientationWidget_->sizeHint();
      const int x = std::max(viewportGeometry.left() + overlayInset,
                             viewportGeometry.right() - sz.width() -
                                 overlayInset + 1);
      const int y = viewportGeometry.top() + 24;
      viewOrientationWidget_->setGeometry(x, y, sz.width(), sz.height());
      if (auto *controller = activeRenderController()) {
        viewOrientationWidget_->setOrientationQuaternion(
            controller->viewportOrientationQuaternion());
      }
      // The cube is a convenience control; orientation presets remain
      // available from the View menu when the viewport cannot contain it.
      const bool showNavigator = hasComposition && overlayWidth >= sz.width();
      if (viewOrientationWidget_->isVisible() != showNavigator) {
        viewOrientationWidget_->setVisible(showNavigator);
      }
      viewOrientationWidget_->setEnabledState(
          showNavigator && !isMaskNavigationLocked());
      if (viewOrientationWidget_->isVisible()) {
        viewOrientationWidget_->raise();
      }
    }
    if (chromeStrip_) {
      chromeStrip_->adjustSize();
      const int width = std::min({440, chromeStrip_->sizeHint().width(),
                                  overlayWidth});
      chromeStrip_->setGeometry(viewportGeometry.left() + overlayInset,
                                viewportGeometry.bottom() -
                                    chromeStrip_->sizeHint().height() - overlayInset + 1,
                                width, chromeStrip_->sizeHint().height());
      if (chromeStrip_->isVisible() != hasComposition) {
        chromeStrip_->setVisible(hasComposition);
      }
      if (chromeStrip_->isVisible()) {
        chromeStrip_->raise();
      }
    }
    if (bottomBar_) {
      bottomBar_->adjustSize();
      const QSize sz = bottomBar_->sizeHint();
      const int width = std::min(sz.width(), overlayWidth);
      bottomBar_->setGeometry(viewportGeometry.right() - width - overlayInset + 1,
                              viewportGeometry.bottom() - sz.height() - overlayInset + 1,
                              width, sz.height());
      const bool showBottomBar = hasComposition && width >= 96;
      if (bottomBar_->isVisible() != showBottomBar) {
        bottomBar_->setVisible(showBottomBar);
      }
      if (bottomBar_->isVisible()) {
        bottomBar_->raise();
      }
    }
    if (profilerOverlay_) {
      const QRect vg = viewportGeometry;
      const QSize ps = profilerOverlay_->size();
      profilerOverlay_->move(vg.left() + 8, vg.top() + 8);
      if (profilerOverlay_->isVisible()) {
        profilerOverlay_->raise();
      }
      Q_UNUSED(ps);
    }
  }

  QString viewportChannelDisplayLabel() const {
    QStringList tags;
    switch (viewportChannelDisplayMode_) {
    case ViewportChannelDisplayMode::Color:
      tags << QStringLiteral("RGB");
      break;
    case ViewportChannelDisplayMode::Alpha:
      tags << QStringLiteral("Alpha");
      break;
    case ViewportChannelDisplayMode::ColorAlpha:
      tags << QStringLiteral("RGBA");
      break;
    case ViewportChannelDisplayMode::Red:
      tags << QStringLiteral("R");
      break;
    case ViewportChannelDisplayMode::Green:
      tags << QStringLiteral("G");
      break;
    case ViewportChannelDisplayMode::Blue:
      tags << QStringLiteral("B");
      break;
    case ViewportChannelDisplayMode::Depth:
      tags << QStringLiteral("Depth+");
      break;
    case ViewportChannelDisplayMode::Emission:
      tags << QStringLiteral("Emission");
      break;
    case ViewportChannelDisplayMode::ObjectId:
      tags << QStringLiteral("Object ID+");
      break;
    case ViewportChannelDisplayMode::MaterialId:
      tags << QStringLiteral("Material ID+");
      break;
    case ViewportChannelDisplayMode::Albedo:
      tags << QStringLiteral("Albedo");
      break;
    case ViewportChannelDisplayMode::AlbedoR:
      tags << QStringLiteral("Alb R");
      break;
    case ViewportChannelDisplayMode::AlbedoG:
      tags << QStringLiteral("Alb G");
      break;
    case ViewportChannelDisplayMode::AlbedoB:
      tags << QStringLiteral("Alb B");
      break;
    case ViewportChannelDisplayMode::Normal:
      tags << QStringLiteral("Normal");
      break;
    case ViewportChannelDisplayMode::NormalX:
      tags << QStringLiteral("Nrm X");
      break;
    case ViewportChannelDisplayMode::NormalY:
      tags << QStringLiteral("Nrm Y");
      break;
    case ViewportChannelDisplayMode::NormalZ:
      tags << QStringLiteral("Nrm Z");
      break;
    case ViewportChannelDisplayMode::Velocity:
      tags << QStringLiteral("Velocity");
      break;
    case ViewportChannelDisplayMode::VelocityX:
      tags << QStringLiteral("Vel X");
      break;
    case ViewportChannelDisplayMode::VelocityY:
      tags << QStringLiteral("Vel Y");
      break;
    }
    if (xRayAction_ && xRayAction_->isChecked()) {
      tags << QStringLiteral("X-Ray");
    }
    if (isolationAction_ && isolationAction_->isChecked()) {
      tags << QStringLiteral("Isolate");
    }
    if (gizmoVisibleAction_ && !gizmoVisibleAction_->isChecked()) {
      tags << QStringLiteral("No Gizmo");
    }
    return tags.isEmpty()
               ? QStringLiteral("Shading")
               : QStringLiteral("Shading: %1").arg(tags.join(QStringLiteral(" + ")));
  }

  QString gizmoButtonLabel() const {
    const bool visible = !gizmoVisibleAction_ || gizmoVisibleAction_->isChecked();
    if (!visible) {
      return QStringLiteral("Gizmo: OFF");
    }
    QString modeLabel = QStringLiteral("All");
    if (renderController_) {
      switch (renderController_->gizmoMode()) {
      case TransformGizmo::Mode::Translation:
        modeLabel = QStringLiteral("Move");
        break;
      case TransformGizmo::Mode::Rotation:
        modeLabel = QStringLiteral("Rotate");
        break;
      case TransformGizmo::Mode::Scale:
        modeLabel = QStringLiteral("Scale");
        break;
      default:
        break;
      }
    }
    return QStringLiteral("Gizmo: %1").arg(modeLabel);
  }

  QString shadingButtonTooltip() const {
    QStringList lines;
    lines << QStringLiteral("Viewport shading and channel display");
    lines << QStringLiteral("Current: %1").arg(viewportChannelDisplayLabel());
    lines << QStringLiteral("Alt+2 RGB, Alt+3 Alpha, Alt+4 RGBA");
    return lines.join(QChar::LineFeed);
  }

  QString gizmoButtonTooltip() const {
    QStringList lines;
    lines << QStringLiteral("Transform gizmo mode and visibility");
    lines << QStringLiteral("Current: %1").arg(gizmoButtonLabel());
    lines << QStringLiteral("W = Move, R = Rotate, S = Scale");
    lines << QStringLiteral("All = Move + Rotate + Scale");
    return lines.join(QChar::LineFeed);
  }

  QString layerChromeButtonLabel() const {
    return layerChromeVisible_ ? QStringLiteral("Layer Controls: ON")
                               : QStringLiteral("Layer Controls: OFF");
  }

  QString lockViewButtonLabel() const {
    return lockViewToSelection_ ? QStringLiteral("Lock View: ON")
                               : QStringLiteral("Lock View: OFF");
  }

  QString viewportToggleLabel(const QString &name, bool enabled) const {
    return QStringLiteral("%1: %2")
        .arg(name, enabled ? QStringLiteral("ON") : QStringLiteral("OFF"));
  }

  QString previewOrbitButtonLabel() const {
    return previewOrbitMode_ ? QStringLiteral("Preview: ON")
                             : QStringLiteral("Preview: OFF");
  }

  QString previewOrbitButtonTooltip() const {
    QStringList lines;
    lines << QStringLiteral("Temporary view-only navigation session");
    lines << QStringLiteral("Current: %1").arg(previewOrbitButtonLabel());
    lines << QStringLiteral("Alt+Left = Orbit, Middle = Pan, Wheel = Zoom");
    lines << QStringLiteral("Restores the saved orientation / pan / zoom when turned off");
    return lines.join(QChar::LineFeed);
  }

  QString renderSuspendButtonLabel() const {
    return renderSuspendAction_ && renderSuspendAction_->isChecked()
               ? QStringLiteral("Render Hold: ON")
               : QStringLiteral("Render Hold: OFF");
  }

  QString renderSuspendButtonTooltip() const {
    QStringList lines;
    lines << QStringLiteral("Suspend viewport cache invalidation");
    lines << QStringLiteral("Current: %1").arg(renderSuspendButtonLabel());
    lines << QStringLiteral("Useful while inspecting a frozen viewport state");
    return lines.join(QChar::LineFeed);
  }

  void refreshViewportStateLabels() {
    if (shadingButton_) {
      shadingButton_->setText(compactViewportControls_
                                  ? QStringLiteral("View")
                                  : viewportChannelDisplayLabel());
      shadingButton_->setToolTip(shadingButtonTooltip());
    }
    if (gizmoModeButton_) {
      gizmoModeButton_->setText(gizmoButtonLabel());
      gizmoModeButton_->setToolTip(gizmoButtonTooltip());
    }
    if (previewOrbitAction_) {
      previewOrbitAction_->setText(previewOrbitButtonLabel());
      previewOrbitAction_->setToolTip(previewOrbitButtonTooltip());
      previewOrbitAction_->setChecked(previewOrbitMode_);
    }
    if (renderSuspendAction_) {
      renderSuspendAction_->setText(renderSuspendButtonLabel());
      renderSuspendAction_->setToolTip(renderSuspendButtonTooltip());
      const bool checked = renderSuspendAction_->isChecked();
      if (renderController_) {
        renderController_->setRenderQueueActive(checked);
      }
      forEachActiveSecondaryController(
        [checked](CompositionRenderController *controller) {
          controller->setRenderQueueActive(checked);
        });
    }
    if (autoFourUpAction_) {
      autoFourUpAction_->setText(fourUpAutoAssignButtonLabel());
      autoFourUpAction_->setChecked(autoAssignFourUpViews_);
    }
  }

  void setPreviewOrbitMode(ArtifactCompositionEditor *owner, bool enabled) {
    Q_UNUSED(owner);
    if (enabled && isMaskNavigationLocked()) {
      if (previewOrbitAction_ && previewOrbitAction_->isChecked()) {
        const QSignalBlocker blocker(previewOrbitAction_);
        previewOrbitAction_->setChecked(false);
      }
      return;
    }
    if (previewOrbitMode_ == enabled) {
      return;
    }
    if (enabled) {
      previewOrbitSnapshots_.clear();
      forEachRenderController([this](CompositionRenderController *controller) {
        if (!controller) {
          return;
        }
        PreviewOrbitSnapshot snapshot;
        snapshot.orientation = controller->viewportOrientationQuaternion();
        if (auto *renderer = controller->renderer()) {
          float panX = 0.0f;
          float panY = 0.0f;
          renderer->getPan(panX, panY);
          snapshot.pan = QPointF(panX, panY);
          snapshot.zoom = std::max(0.001f, renderer->getZoom());
        }
        previewOrbitSnapshots_.insert(controller, snapshot);
      });
      previewOrbitMode_ = true;
    } else {
      for (auto it = previewOrbitSnapshots_.cbegin();
           it != previewOrbitSnapshots_.cend(); ++it) {
        if (!it.key()) {
          continue;
        }
        auto *controller = it.key();
        const PreviewOrbitSnapshot snapshot = it.value();
        controller->setViewportOrientationQuaternion(snapshot.orientation);
        if (auto *renderer = controller->renderer()) {
          renderer->setZoom(snapshot.zoom);
          renderer->setPan(static_cast<float>(snapshot.pan.x()),
                           static_cast<float>(snapshot.pan.y()));
        }
      }
      previewOrbitSnapshots_.clear();
      previewOrbitMode_ = false;
    }
    refreshViewportStateLabels();
    if (overlayView_) {
      overlayView_->update();
    }
  }

  void setViewportChannelDisplayMode(ArtifactCompositionEditor *owner,
                                     ViewportChannelDisplayMode mode) {
    Q_UNUSED(owner);
    viewportChannelDisplayMode_ = mode;
    forEachRenderController([mode](CompositionRenderController *controller) {
      controller->setViewportChannelDisplayMode(mode);
    });
    refreshViewportStateLabels();
  }

  bool saveRendererMultiChannelImage(ArtifactCompositionEditor *owner,
                                     const QString &filePath,
                                     const QString &format,
                                     const QString &dialogTitle) {
    if (!owner || !renderController_) {
      return false;
    }

    auto *renderer = renderController_->renderer();
    if (!renderer) {
      QMessageBox::warning(owner, dialogTitle, QStringLiteral("Renderer not available."));
      return false;
    }

    struct RendererStateGuard {
      ArtifactIRenderer *renderer = nullptr;
      bool previousMultiChannel = false;
      bool previousDepth = false;
      bool previousNormalX = false;
      bool previousNormalY = false;
      bool previousNormalZ = false;
      bool previousObjectId = false;

      ~RendererStateGuard() {
        if (!renderer) {
          return;
        }
        renderer->setChannelEnabled(ArtifactIRenderer::ChannelType::Depth, previousDepth);
        renderer->setChannelEnabled(ArtifactIRenderer::ChannelType::NormalX, previousNormalX);
        renderer->setChannelEnabled(ArtifactIRenderer::ChannelType::NormalY, previousNormalY);
        renderer->setChannelEnabled(ArtifactIRenderer::ChannelType::NormalZ, previousNormalZ);
        renderer->setChannelEnabled(ArtifactIRenderer::ChannelType::ObjectId, previousObjectId);
        renderer->setMultiChannelEnabled(previousMultiChannel);
      }
    } stateGuard{
        renderer, renderer->isMultiChannelEnabled(),
        renderer->isChannelEnabled(ArtifactIRenderer::ChannelType::Depth),
        renderer->isChannelEnabled(ArtifactIRenderer::ChannelType::NormalX),
        renderer->isChannelEnabled(ArtifactIRenderer::ChannelType::NormalY),
        renderer->isChannelEnabled(ArtifactIRenderer::ChannelType::NormalZ),
        renderer->isChannelEnabled(ArtifactIRenderer::ChannelType::ObjectId)};

    renderer->setMultiChannelEnabled(true);
    renderer->setChannelEnabled(ArtifactIRenderer::ChannelType::Depth, true);
    renderer->setChannelEnabled(ArtifactIRenderer::ChannelType::NormalX, true);
    renderer->setChannelEnabled(ArtifactIRenderer::ChannelType::NormalY, true);
    renderer->setChannelEnabled(ArtifactIRenderer::ChannelType::NormalZ, true);
    renderer->setChannelEnabled(ArtifactIRenderer::ChannelType::ObjectId, true);
    renderer->clear();
    if (auto comp = renderController_->composition()) {
      const auto pos = comp->framePosition();
      const auto &layers = comp->allLayerRef();
      for (const auto &layer : layers) {
        if (layer && layer->isVisible() && layer->isActiveAt(pos)) {
          layer->draw(renderer);
        }
      }
    }
    renderer->flush();

    ArtifactCore::MultiChannelImage multiFrame = renderer->readbackToMultiChannelImage();
    if (multiFrame.isEmpty()) {
      QMessageBox::warning(owner, dialogTitle,
                           QStringLiteral("Failed to capture multi-channel image."));
      return false;
    }

    ArtifactCore::ImageExportOptions exportOpts;
    exportOpts.format = format;
    ArtifactCore::ImageExporter exporter;
    auto result = exporter.writeMultiChannel(multiFrame, filePath, exportOpts);
    if (!result.success) {
      QMessageBox::warning(owner, dialogTitle,
                           QStringLiteral("Save failed: %1 - %2")
                               .arg(result.errorStage, result.errorMessage));
      return false;
    }

    QMessageBox::information(owner, dialogTitle,
                             QStringLiteral("保存しました:\n%1").arg(filePath));
    return true;
  }

  void saveViewportRenderOutput(ArtifactCompositionEditor *owner) {
    if (!owner || !renderController_) {
      return;
    }

    ArtifactScreenshotExportDialog dialog(owner);
    dialog.setWindowTitle(QStringLiteral("Viewport Render Output"));
    dialog.setFilePath(QStringLiteral("viewport_render.exr"));
    dialog.setFormat(QStringLiteral("exr"));
    dialog.setCaptureSource(ScreenshotCaptureSource::Renderer);
    dialog.setMultiChannelEnabled(true);
    if (dialog.exec() != QDialog::Accepted) {
      return;
    }

    const ScreenshotExportOptions options = dialog.options();
    auto *renderer = renderController_->renderer();
    if (!renderer) {
      QMessageBox::warning(owner, QStringLiteral("Viewport Render Output"),
                           QStringLiteral("Renderer not available."));
      return;
    }
    if (options.multiChannel) {
      saveRendererMultiChannelImage(owner, options.filePath, options.format,
                                    QStringLiteral("Viewport Render Output"));
      return;
    }

    saveRendererImageAsync(owner, renderer, options.filePath, options.format,
                           options.jpegQuality,
                           QStringLiteral("Viewport Render Output"));
  }

  void applyFourUpDefaultOrientations() {
    if (viewportLayoutMode_ != ViewportLayoutMode::FourUp) {
      return;
    }
    static constexpr std::array<ArtifactCore::ViewOrientationHotspot, 3>
        secondaryHotspots{ArtifactCore::ViewOrientationHotspot::Top,
                          ArtifactCore::ViewOrientationHotspot::Front,
                          ArtifactCore::ViewOrientationHotspot::Right};
    for (int i = 1; i < 4; ++i) {
      if (auto *paneState = pane(i);
          paneState && paneState->controller && paneState->visible) {
        paneState->controller->setViewportOrientation(secondaryHotspots[i - 1]);
      }
    }
    if (overlayView_) {
      overlayView_->update();
    }
  }

  QString fourUpAutoAssignButtonLabel() const {
    return autoAssignFourUpViews_
               ? QStringLiteral("Auto-Assign Four-Up Views: ON")
               : QStringLiteral("Auto-Assign Four-Up Views: OFF");
  }

  void setToolbarActionVisible(QAction *action, bool visible) {
    if (!topToolbar_ || !action) {
      return;
    }
    if (QWidget *widget = topToolbar_->widgetForAction(action)) {
      widget->setVisible(visible);
    }
    action->setVisible(visible);
  }

  void syncToolbarVisibility(bool hasComposition, bool hasLayers) {
    if (!topToolbar_ || !viewportLayoutButton_) {
      return;
    }

    const bool showEditingChrome = hasComposition;
    const bool showLayerChrome = hasComposition && hasLayers && layerChromeVisible_;

    viewportLayoutButton_->setVisible(true);
    setToolbarActionVisible(resetAction_, showEditingChrome);
    setToolbarActionVisible(zoomInAction_, showEditingChrome);
    setToolbarActionVisible(zoomOutAction_, showEditingChrome);
    setToolbarActionVisible(zoomFitAction_, showEditingChrome);
    setToolbarActionVisible(zoom100Action_, showEditingChrome);
    setToolbarActionVisible(editTextAction_, showLayerChrome);
    if (screenshotButton_) {
      screenshotButton_->setVisible(showEditingChrome);
    }
    if (viewportRenderOutputButton_) {
      viewportRenderOutputButton_->setVisible(showEditingChrome);
    }
    setToolbarActionVisible(compareAction_, showLayerChrome);
    setToolbarActionVisible(motionPathAction_, showLayerChrome);
    setToolbarActionVisible(effectHitboxAction_, showLayerChrome);
    if (toolModeButton_) {
      toolModeButton_->setVisible(showEditingChrome);
    }
    if (gizmoModeButton_) {
      gizmoModeButton_->setVisible(showLayerChrome);
    }
    if (pivotModeButton_) {
      pivotModeButton_->setVisible(showLayerChrome);
    }
    if (layerChromeAction_) {
      layerChromeAction_->setText(layerChromeButtonLabel());
      layerChromeAction_->setChecked(layerChromeVisible_);
      layerChromeAction_->setEnabled(hasComposition && hasLayers);
    }
    if (lockViewAction_) {
      lockViewAction_->setText(lockViewButtonLabel());
      lockViewAction_->setChecked(lockViewToSelection_);
      lockViewAction_->setEnabled(hasComposition && hasLayers);
    }
  }

  void toggleImmersiveMode(ArtifactCompositionEditor *owner, bool immersive) {
    if (!owner) {
      return;
    }
    immersiveMode_ = immersive;
    if (auto *topLevel = owner->window()) {
      if (immersive) {
        topLevel->showFullScreen();
      } else {
        topLevel->showNormal();
      }
    }
    if (immersiveAction_) {
      immersiveAction_->setChecked(immersive);
      immersiveAction_->setText(immersive ? QStringLiteral("Exit Immersive")
                                          : QStringLiteral("Immersive"));
    }
  }

  void toggleViewportToolboxes(ArtifactCompositionEditor *owner) {
    viewportToolboxesVisible_ = !viewportToolboxesVisible_;
    ArtifactCore::LayeredConfigStore::instance().setValue(
        QStringLiteral("Viewport/Hud/Visible"), viewportToolboxesVisible_);
    syncOverlayGeometry(owner);
  }

  bool saveRendererImageAsync(ArtifactCompositionEditor *owner,
                              ArtifactIRenderer *renderer,
                              const QString &filePath,
                              const QString &format,
                              int jpegQuality,
                              const QString &dialogTitle) {
    if (!owner || !renderer || filePath.isEmpty() || screenshotExportInProgress_) {
      return false;
    }

    screenshotExportInProgress_ = true;
    auto *progress = new QProgressDialog(
        QStringLiteral("Rendering and saving %1...").arg(QFileInfo(filePath).fileName()),
        QStringLiteral("Cancel"), 0, 0, owner);
    progress->setWindowTitle(dialogTitle);
    progress->setWindowModality(Qt::WindowModal);
    progress->setAutoClose(false);
    progress->setAutoReset(false);
    progress->show();

    QPointer<ArtifactCompositionEditor> safeOwner(owner);
    QPointer<QProgressDialog> safeProgress(progress);
    renderer->readbackToImageAsync(
        [this, safeOwner, safeProgress, filePath, format, jpegQuality,
         dialogTitle](const QImage &image) {
          if (!safeOwner) {
            return;
          }
          QMetaObject::invokeMethod(
              safeOwner,
              [this, safeOwner, safeProgress, filePath, format, jpegQuality,
               dialogTitle, image]() {
                const bool cancelled = !safeProgress || safeProgress->wasCanceled();
                if (safeProgress) {
                  safeProgress->close();
                  safeProgress->deleteLater();
                }
                screenshotExportInProgress_ = false;
                if (cancelled) {
                  return;
                }
                if (image.isNull()) {
                  QMessageBox::warning(
                      safeOwner, dialogTitle,
                      QStringLiteral("現在のフレームを取得できませんでした。\nStage: readback"));
                  return;
                }
                if (safeProgress) {
                  safeProgress->setLabelText(
                      QStringLiteral("Saving %1...").arg(QFileInfo(filePath).fileName()));
                }
                if (!saveScreenshotImage(image, filePath, format, jpegQuality)) {
                  QMessageBox::warning(
                      safeOwner, dialogTitle,
                      QStringLiteral("保存に失敗しました:\n%1\nStage: encode/write")
                          .arg(filePath));
                  return;
                }
                QMessageBox::information(
                    safeOwner, dialogTitle,
                    QStringLiteral("保存しました:\n%1").arg(filePath));
              },
              Qt::QueuedConnection);
        });
    return true;
  }

  bool saveQuickScreenshot(ArtifactCompositionEditor* owner) {
    if (!owner || !renderController_) {
      return false;
    }

    const QString selectedFilterDefault = QStringLiteral("PNG Image (*.png)");
    QString selectedFilter = selectedFilterDefault;
    const QString rawPath = QFileDialog::getSaveFileName(
        owner,
        QStringLiteral("スクリーンショットを保存"),
        QStringLiteral("composition_screenshot.png"),
        QStringLiteral("PNG Image (*.png);;JPEG Image (*.jpg *.jpeg);;OpenEXR (*.exr);;All Files (*.*)"),
        &selectedFilter);
    if (rawPath.isEmpty()) {
      return false;
    }

    const QString filePath = ensureScreenshotSuffix(rawPath, selectedFilter);
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    return saveRendererImageAsync(owner, renderController_->renderer(), filePath,
                                  suffix, 95, QStringLiteral("スクリーンショット"));
  }

  bool saveAdvancedScreenshot(ArtifactCompositionEditor* owner) {
    if (!owner || !renderController_) {
      return false;
    }

    const QString defaultPath = QStringLiteral("composition_screenshot.png");
    ArtifactScreenshotExportDialog dialog(owner);
    dialog.setFilePath(defaultPath);
    dialog.setFormat(QStringLiteral("png"));
    if (dialog.exec() != QDialog::Accepted) {
      return false;
    }

    const ScreenshotExportOptions options = dialog.options();

    const QString filePath = options.filePath;

    if (options.multiChannel) {
      return saveRendererMultiChannelImage(owner, filePath, options.format,
                                           QStringLiteral("Advanced Screenshot"));
    } else {
      if (options.captureSource == ScreenshotCaptureSource::WholeWindow) {
        const QImage screenshot = captureScreenshotForOptions(
            renderController_, owner, options.captureSource);
        if (screenshot.isNull()) {
          QMessageBox::warning(owner, QStringLiteral("Advanced Screenshot"),
                               QStringLiteral("現在のフレームを取得できませんでした。"));
          return false;
        }
        if (!saveScreenshotImage(screenshot, filePath, options.format, options.jpegQuality)) {
          QMessageBox::warning(owner, QStringLiteral("Advanced Screenshot"),
                               QStringLiteral("保存に失敗しました:\n%1").arg(filePath));
          return false;
        }
        QMessageBox::information(owner, QStringLiteral("Advanced Screenshot"),
                                 QStringLiteral("保存しました:\n%1").arg(filePath));
        return true;
      }
      return saveRendererImageAsync(owner, renderController_->renderer(), filePath,
                                    options.format, options.jpegQuality,
                                    QStringLiteral("Advanced Screenshot"));
    }
  }
};

ArtifactCompositionEditor::ArtifactCompositionEditor(QWidget *parent)
    : QWidget(parent), impl_(new Impl()) {
  QElapsedTimer ctorTimer;
  ctorTimer.start();
  setMinimumSize(0, 0);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setAutoFillBackground(true);
  impl_->viewportToolboxesVisible_ =
      ArtifactCore::LayeredConfigStore::instance().valueBool(
          QStringLiteral("Viewport/Hud/Visible"), true);

  const auto theme = ArtifactCore::currentDCCTheme();
  QPalette editorPalette = palette();
  editorPalette.setColor(QPalette::Window, QColor(theme.backgroundColor));
  editorPalette.setColor(QPalette::WindowText, QColor(theme.textColor));
  setPalette(editorPalette);

  if (auto *active = ArtifactActiveContextService::instance()) {
    active->setHandler(this);
  }

  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  impl_->renderController_ = new CompositionRenderController(this);
  impl_->panes_[0].controller = impl_->renderController_;
  for (int i = 1; i < ArtifactCompositionEditor::Impl::kViewportPaneCount; ++i) {
    impl_->panes_[i].controller = new CompositionRenderController(this);
  }
  {
    const QColor clear(28, 40, 56);
    impl_->forEachRenderController([&clear](CompositionRenderController *controller) {
      controller->setClearColor(
          {clear.redF(), clear.greenF(), clear.blueF(), 1.0f});
    });
  }
  if (auto *settings = ArtifactCore::ArtifactAppSettings::instance()) {
    const auto applySettings = [settings](CompositionRenderController *controller) {
      if (!controller) {
        return;
      }
      controller->setCompositionBackgroundMode(
          settings->compositionBackgroundMode());
      controller->setCheckerboardSize(
          settings->compositionCheckerboardSize());
      controller->setGridSettings(settings->compositionGridSettings());
      controller->setShowGrid(settings->compositionShowGrid());
      controller->setShowGuides(settings->compositionShowGuides());
      controller->setShowSafeMargins(
          settings->compositionShowSafeMargins());
      controller->setShowAnchorCenterOverlay(
          settings->compositionShowAnchorCenterOverlay());
      controller->setShowCameraFrustumOverlay(
          settings->compositionShowCameraFrustumOverlay());
      controller->setShowMotionPathOverlay(
          settings->compositionShowMotionPathOverlay());
      controller->setShowDensityHeatmapOverlay(
          settings->compositionShowDensityHeatmapOverlay());
    };
    impl_->forEachRenderController(applySettings);
  }
  if (ArtifactCore::ArtifactAppSettings::instance()) {
    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<ArtifactCore::AppSettingsChangedEvent>(
            [this](const ArtifactCore::AppSettingsChangedEvent&) {
              if (!impl_ || !impl_->renderController_) {
                return;
              }
              if (auto *settings = ArtifactCore::ArtifactAppSettings::instance()) {
                impl_->renderController_->setShowMotionPathOverlay(
                    settings->compositionShowMotionPathOverlay());
                impl_->forEachActiveSecondaryController(
                    [settings](CompositionRenderController *controller) {
                      controller->setShowMotionPathOverlay(
                          settings->compositionShowMotionPathOverlay());
                    });
                if (impl_->motionPathAction_) {
                  const QSignalBlocker blocker(impl_->motionPathAction_);
                  impl_->motionPathAction_->setChecked(
                      impl_->renderController_->isShowMotionPathOverlay());
                }
                impl_->renderController_->setShowDensityHeatmapOverlay(
                    settings->compositionShowDensityHeatmapOverlay());
                impl_->forEachActiveSecondaryController(
                    [settings](CompositionRenderController *controller) {
                      controller->setShowDensityHeatmapOverlay(
                          settings->compositionShowDensityHeatmapOverlay());
                    });
                if (impl_->densityHeatmapAction_) {
                  const QSignalBlocker blocker(impl_->densityHeatmapAction_);
                  impl_->densityHeatmapAction_->setChecked(
                      impl_->renderController_->isShowDensityHeatmapOverlay());
                }
              }
            }));
  }

  impl_->compositionView_ =
      new CompositionViewport(impl_->renderController_, this);
  impl_->panes_[0].view = impl_->compositionView_;
  for (int i = 1; i < ArtifactCompositionEditor::Impl::kViewportPaneCount; ++i) {
    impl_->panes_[i].view =
        new CompositionViewport(impl_->panes_[i].controller, this);
    impl_->panes_[i].view->hide();
  }
  impl_->viewportHost_ = new QWidget(this);
  impl_->viewportHost_->setObjectName(QStringLiteral("compositionViewportHost"));
  auto *viewportHostLayout = new QVBoxLayout(impl_->viewportHost_);
  viewportHostLayout->setContentsMargins(0, 0, 0, 0);
  viewportHostLayout->setSpacing(0);
  impl_->viewportRowsSplitter_ = new QSplitter(Qt::Vertical, impl_->viewportHost_);
  impl_->viewportRowsSplitter_->setObjectName(QStringLiteral("compositionViewportRowsSplitter"));
  impl_->viewportRowsSplitter_->setChildrenCollapsible(false);
  impl_->viewportTopSplitter_ = new QSplitter(Qt::Horizontal, impl_->viewportRowsSplitter_);
  impl_->viewportTopSplitter_->setObjectName(QStringLiteral("compositionViewportTopSplitter"));
  impl_->viewportTopSplitter_->setChildrenCollapsible(false);
  impl_->viewportBottomSplitter_ =
      new QSplitter(Qt::Horizontal, impl_->viewportRowsSplitter_);
  impl_->viewportBottomSplitter_->setObjectName(QStringLiteral("compositionViewportBottomSplitter"));
  impl_->viewportBottomSplitter_->setChildrenCollapsible(false);
  impl_->viewportTopSplitter_->addWidget(impl_->panes_[0].view);
  impl_->viewportTopSplitter_->addWidget(impl_->panes_[1].view);
  impl_->viewportBottomSplitter_->addWidget(impl_->panes_[2].view);
  impl_->viewportBottomSplitter_->addWidget(impl_->panes_[3].view);
  viewportHostLayout->addWidget(impl_->viewportRowsSplitter_);
  impl_->viewportBottomSplitter_->hide();
  impl_->compositionView_->setResizeCallback([this]() {
    if (impl_) {
      impl_->syncOverlayGeometry(this);
    }
  });
  for (int i = 1; i < ArtifactCompositionEditor::Impl::kViewportPaneCount; ++i) {
    if (auto *view = impl_->panes_[i].view) {
      view->setResizeCallback([this]() {
        if (impl_) {
          impl_->syncOverlayGeometry(this);
        }
      });
    }
  }
  impl_->overlayView_ =
      new CompositionOverlayWidget(impl_->compositionView_, this);
  impl_->overlayView_->setActivePaneIndicatorProvider([this]() {
    if (!impl_) {
      return std::optional<std::pair<QRect, QString>>{};
    }
    return impl_->activePaneIndicator();
  });
  impl_->overlayView_->setNavigationFeedbackProvider([this]() {
    return impl_ ? impl_->activeNavigationFeedbackLabel() : QString{};
  });
  impl_->overlayView_->setPreviewBadgeProvider([this]() {
    return impl_ && impl_->previewOrbitMode_ ? QStringLiteral("Preview On")
                                             : QString{};
  });
  impl_->overlayView_->setResizeIndicatorProvider([this]() {
    return impl_ && impl_->activePaneResizePending();
  });
  for (int i = 0; i < ArtifactCompositionEditor::Impl::kViewportPaneCount; ++i) {
    if (auto *view = impl_->panes_[i].view) {
      view->setOverlayWidget(impl_->overlayView_);
      view->setViewportOrientationChangedCallback(
          [this](const QQuaternion &orientation) {
            if (impl_ && impl_->viewOrientationWidget_) {
              impl_->viewOrientationWidget_->setOrientationQuaternion(
                  orientation);
            }
          });
    }
  }
  impl_->overlayView_->hide();
  for (int i = 0; i < ArtifactCompositionEditor::Impl::kViewportPaneCount; ++i) {
    impl_->emptyStateOverlays_[i] = new EmptyCompositionOverlayWidget(
        this, [this]() {
          if (impl_) {
            impl_->openCreateCompositionDialog(this);
          }
        }, [this, i](const QStringList &paths) {
          if (!impl_ || i < 0 ||
              i >= ArtifactCompositionEditor::Impl::kViewportPaneCount) {
            return;
          }
          if (auto *view = impl_->panes_[i].view) {
            view->importDroppedPaths(paths);
          }
        });
    impl_->emptyStateOverlays_[i]->hide();
  }
  impl_->viewOrientationWidget_ = new ViewOrientationWidget(this);
  impl_->viewOrientationWidget_->setActivatedCallback(
      [this](ArtifactCore::ViewOrientationHotspot hotspot) {
        if (impl_) {
          if (auto *controller = impl_->activeRenderController()) {
            controller->setViewportOrientation(hotspot);
            impl_->viewOrientationWidget_->setOrientationQuaternion(
                controller->viewportOrientationQuaternion());
            if (impl_->overlayView_) {
              impl_->overlayView_->update();
            }
          }
        }
      });
  impl_->viewOrientationWidget_->setOrbitChangedCallback(
      [this](const QQuaternion &orientation) {
        if (!impl_) {
          return;
        }
        impl_->pendingViewCubeOrientation_ = orientation;
        if (impl_->viewCubeUpdateQueued_) {
          return;
        }
        impl_->viewCubeUpdateQueued_ = true;
        QTimer::singleShot(0, this, [this]() {
          if (!impl_) {
            return;
          }
          impl_->viewCubeUpdateQueued_ = false;
          const auto pending = impl_->pendingViewCubeOrientation_;
          impl_->pendingViewCubeOrientation_.reset();
          if (!pending) {
            return;
          }
          if (auto *controller = impl_->activeRenderController()) {
            if (impl_->isMaskNavigationLocked()) {
              impl_->viewOrientationWidget_->setOrientationQuaternion(
                  controller->viewportOrientationQuaternion());
              return;
            }
            controller->setViewportOrientationQuaternion(*pending);
            if (impl_->overlayView_) {
              impl_->overlayView_->update();
            }
          }
        });
      });
  impl_->viewOrientationWidget_->show();
  for (int i = 0; i < ArtifactCompositionEditor::Impl::kViewportPaneCount; ++i) {
    if (auto *view = impl_->panes_[i].view) {
      view->setActivatedCallback([this, i]() {
        if (impl_) {
          impl_->setActivePane(this, i);
        }
      });
    }
  }

  // Top Toolbar
  impl_->topToolbar_ = new QToolBar(this);
  impl_->topToolbar_->setObjectName(QStringLiteral("compositionTopToolbar"));
  impl_->topToolbar_->setMovable(false);
  impl_->topToolbar_->setToolButtonStyle(Qt::ToolButtonTextOnly);
  impl_->topToolbar_->setIconSize(QSize(18, 18));
  impl_->topToolbar_->setFixedHeight(38);
  {
    QPalette pal = impl_->topToolbar_->palette();
    pal.setColor(QPalette::Window, QColor(theme.secondaryBackgroundColor));
    pal.setColor(QPalette::Button, QColor(theme.secondaryBackgroundColor));
    pal.setColor(QPalette::WindowText, QColor(theme.textColor));
    impl_->topToolbar_->setPalette(pal);
  }

  impl_->viewportLayoutButton_ = new ViewportLayoutButton(impl_->topToolbar_);
  impl_->viewportLayoutButton_->setText(impl_->viewportLayoutLabel());
  impl_->viewportLayoutButton_->setFixedWidth(72);
  impl_->viewportLayoutButton_->setAutoRaise(true);
  impl_->viewportLayoutButton_->setFocusPolicy(Qt::NoFocus);
  impl_->viewportLayoutButton_->setToolButtonStyle(Qt::ToolButtonTextOnly);
  impl_->viewportLayoutButton_->setSizePolicy(QSizePolicy::Fixed,
                                               QSizePolicy::Preferred);
  impl_->viewportLayoutButton_->setToolTip(
      QStringLiteral("Cycle the viewport layout between 1, 2, and 4 views"));
  impl_->topToolbar_->addWidget(impl_->viewportLayoutButton_);
  impl_->topToolbar_->addSeparator();
  auto setViewportLayout = [this](ArtifactCompositionEditor::Impl::ViewportLayoutMode mode) {
    if (!impl_) {
      return;
    }
    impl_->viewportLayoutMode_ = mode;
    const quint64 layoutGeneration = ++impl_->viewportLayoutGeneration_;
    const qint64 deferredStartMs = QDateTime::currentMSecsSinceEpoch();
    const auto composition = impl_->renderController_
                                 ? impl_->renderController_->composition()
                                 : ArtifactCompositionPtr{};
    // Only active panes need a composition transition.  Updating all three
    // secondary render controllers on every layout change made Four-Up entry
    // pay the full renderer setup cost even for panes that were not visible.
    impl_->forEachActiveSecondaryController(
        [&composition](CompositionRenderController *controller) {
          controller->stop();
          controller->setComposition(composition);
        });
    impl_->applyViewportLayout();
    if (impl_->autoAssignFourUpViews_) {
      impl_->applyFourUpDefaultOrientations();
    }
    if (composition) {
      // Starting several renderer/controller pairs synchronously makes Four-Up
      // entry block the editor. Stagger secondary starts across event-loop
      // turns so the primary viewport remains responsive while they warm up.
      const int activeCount = impl_->activeViewportPaneCount();
      for (int i = 1; i < activeCount; ++i) {
        QTimer::singleShot((i - 1) * 16, this, [this, i, layoutGeneration,
                                                deferredStartMs]() {
          if (!impl_ || layoutGeneration != impl_->viewportLayoutGeneration_ ||
              i >= impl_->activeViewportPaneCount()) {
            return;
          }
          if (auto *paneState = impl_->pane(i);
              paneState && paneState->controller) {
            paneState->controller->start();
            qInfo() << "[CompositionEditor][ViewportLayout] deferred pane start"
                    << i << "latencyMs="
                    << (QDateTime::currentMSecsSinceEpoch() - deferredStartMs);
            if (paneState->view) {
              paneState->view->requestInitialFit();
            }
          }
        });
      }
    }
    impl_->syncOverlayGeometry(this);
  };
  impl_->viewportLayoutButton_->setActivatedCallback([this, setViewportLayout]() {
    if (!impl_) {
      return;
    }
    setViewportLayout(impl_->nextViewportLayoutMode());
  });

  impl_->compositionCleanupButton_ = new ViewportLayoutButton(impl_->topToolbar_);
  impl_->compositionCleanupButton_->setText(QStringLiteral("Cleanup"));
  impl_->compositionCleanupButton_->setAutoRaise(true);
  impl_->compositionCleanupButton_->setFocusPolicy(Qt::NoFocus);
  impl_->compositionCleanupButton_->setToolButtonStyle(Qt::ToolButtonTextOnly);
  impl_->compositionCleanupButton_->setToolTip(
      QStringLiteral("Analyze composition spacing, edge margins, and near-center placement"));
  impl_->compositionCleanupButton_->setActivatedCallback([this]() {
    if (!impl_) {
      return;
    }
    showCompositionCleanupDialog(
        this, impl_->activeRenderController(), impl_->activeRenderController()
                  ? impl_->activeRenderController()->composition()
                  : ArtifactCompositionPtr{});
  });
  impl_->topToolbar_->addWidget(impl_->compositionCleanupButton_);

  auto *viewPresetMenu = new QMenu(this);
  polishEditorMenu(viewPresetMenu, this);
  auto *viewValuesHost = new QWidget(viewPresetMenu);
  viewValuesHost->setObjectName(QStringLiteral("compositionViewPresetValuesHost"));
  auto *viewValuesLayout = new QHBoxLayout(viewValuesHost);
  viewValuesLayout->setContentsMargins(10, 8, 10, 8);
  viewValuesLayout->setSpacing(6);
  auto *yawLabel = new QLabel(QStringLiteral("Yaw"), viewValuesHost);
  auto *yawSpin = new QDoubleSpinBox(viewValuesHost);
  yawSpin->setRange(-180.0, 180.0);
  yawSpin->setDecimals(1);
  yawSpin->setSingleStep(5.0);
  yawSpin->setFixedWidth(72);
  auto *pitchLabel = new QLabel(QStringLiteral("Pitch"), viewValuesHost);
  auto *pitchSpin = new QDoubleSpinBox(viewValuesHost);
  pitchSpin->setRange(-180.0, 180.0);
  pitchSpin->setDecimals(1);
  pitchSpin->setSingleStep(5.0);
  pitchSpin->setFixedWidth(72);
  auto *zoomLabel = new QLabel(QStringLiteral("Zoom"), viewValuesHost);
  auto *zoomSpin = new QDoubleSpinBox(viewValuesHost);
  zoomSpin->setRange(0.05, 64.0);
  zoomSpin->setDecimals(2);
  zoomSpin->setSingleStep(0.25);
  zoomSpin->setFixedWidth(72);
  auto *syncViewButton = new QPushButton(QStringLiteral("Sync"), viewValuesHost);
  auto *applyViewButton = new QPushButton(QStringLiteral("Apply"), viewValuesHost);
  viewValuesLayout->addWidget(yawLabel);
  viewValuesLayout->addWidget(yawSpin);
  viewValuesLayout->addWidget(pitchLabel);
  viewValuesLayout->addWidget(pitchSpin);
  viewValuesLayout->addWidget(zoomLabel);
  viewValuesLayout->addWidget(zoomSpin);
  viewValuesLayout->addWidget(syncViewButton);
  viewValuesLayout->addWidget(applyViewButton);
  auto *viewValuesAction = new QWidgetAction(viewPresetMenu);
  viewValuesAction->setDefaultWidget(viewValuesHost);
  viewPresetMenu->addAction(viewValuesAction);
  viewPresetMenu->addSeparator();
  const auto syncViewValueEditors = [this, yawSpin, pitchSpin, zoomSpin]() {
    if (!impl_) {
      return;
    }
    if (auto *controller = impl_->activeRenderController()) {
      const QVector3D euler =
          controller->viewportOrientationQuaternion().toEulerAngles();
      const QSignalBlocker yawBlocker(yawSpin);
      const QSignalBlocker pitchBlocker(pitchSpin);
      const QSignalBlocker zoomBlocker(zoomSpin);
      yawSpin->setValue(euler.y());
      pitchSpin->setValue(euler.x());
      zoomSpin->setValue(controller->renderer()
                             ? static_cast<double>(controller->renderer()->getZoom())
                             : 1.0);
    }
  };
  QObject::connect(syncViewButton, &QPushButton::clicked, this,
                   [syncViewValueEditors]() { syncViewValueEditors(); });
  QObject::connect(applyViewButton, &QPushButton::clicked, this,
                   [this, yawSpin, pitchSpin, zoomSpin]() {
                     if (!impl_) {
                       return;
                     }
                     if (auto *controller = impl_->activeRenderController()) {
                       controller->setViewportOrientationQuaternion(
                           QQuaternion::fromEulerAngles(
                               static_cast<float>(pitchSpin->value()),
                               static_cast<float>(yawSpin->value()), 0.0f));
                       if (auto *renderer = controller->renderer()) {
                         renderer->setZoom(static_cast<float>(zoomSpin->value()));
                       }
                       controller->markRenderDirty();
                       if (impl_->viewOrientationWidget_) {
                         impl_->viewOrientationWidget_->setOrientationQuaternion(
                             controller->viewportOrientationQuaternion());
                       }
                       if (impl_->overlayView_) {
                         impl_->overlayView_->update();
                       }
                     }
                   });
  QAction *frameSelectedAct =
      viewPresetMenu->addAction(QStringLiteral("Frame Selected"));
  frameSelectedAct->setToolTip(
      QStringLiteral("Focus the active viewport on the selected layer"));
  QObject::connect(frameSelectedAct, &QAction::triggered, this, [this]() {
    if (!impl_) {
      return;
    }
    if (auto *controller = impl_->activeRenderController()) {
      controller->focusSelectedLayer();
      controller->setInfoOverlayText(QStringLiteral("Frame Selected"),
                                     impl_->activePaneViewLabel());
    }
  });
  QAction *frameAllAct = viewPresetMenu->addAction(QStringLiteral("Frame All"));
  frameAllAct->setToolTip(
      QStringLiteral("Reset the active viewport to the full composition view"));
  QObject::connect(frameAllAct, &QAction::triggered, this, [this]() {
    if (!impl_) {
      return;
    }
    if (auto *controller = impl_->activeRenderController()) {
      controller->resetView();
      controller->setInfoOverlayText(QStringLiteral("Frame All"),
                                     impl_->activePaneViewLabel());
    }
  });
  viewPresetMenu->addSeparator();
  const auto addViewPresetAction =
      [this, viewPresetMenu](const QString &text,
                             ArtifactCore::ViewOrientationHotspot hotspot) {
        QAction *action = viewPresetMenu->addAction(text);
        QObject::connect(action, &QAction::triggered, this, [this, hotspot]() {
          if (!impl_) {
            return;
          }
          if (auto *controller = impl_->activeRenderController()) {
            controller->setViewportOrientation(hotspot);
            if (impl_->viewOrientationWidget_) {
              impl_->viewOrientationWidget_->setOrientationQuaternion(
                  controller->viewportOrientationQuaternion());
            }
            if (impl_->overlayView_) {
              impl_->overlayView_->update();
            }
          }
        });
        return action;
      };
  addViewPresetAction(QStringLiteral("Front"), ArtifactCore::ViewOrientationHotspot::Front);
  addViewPresetAction(QStringLiteral("Back"), ArtifactCore::ViewOrientationHotspot::Back);
  addViewPresetAction(QStringLiteral("Left"), ArtifactCore::ViewOrientationHotspot::Left);
  addViewPresetAction(QStringLiteral("Right"), ArtifactCore::ViewOrientationHotspot::Right);
  addViewPresetAction(QStringLiteral("Top"), ArtifactCore::ViewOrientationHotspot::Top);
  addViewPresetAction(QStringLiteral("Bottom"), ArtifactCore::ViewOrientationHotspot::Bottom);
  viewPresetMenu->addSeparator();
  QAction *quadAssignAct =
      viewPresetMenu->addAction(QStringLiteral("Apply Four-Up Preset"));
  QAction *autoQuadAssignAct =
      viewPresetMenu->addAction(QStringLiteral("Auto-Assign Four-Up Views"));
  autoQuadAssignAct->setCheckable(true);
  autoQuadAssignAct->setChecked(impl_->autoAssignFourUpViews_);
  autoQuadAssignAct->setToolTip(
      QStringLiteral("Automatically apply Top / Front / Right when Four-Up layout is enabled"));
  impl_->autoFourUpAction_ = autoQuadAssignAct;
  QObject::connect(quadAssignAct, &QAction::triggered, this, [this]() {
    if (!impl_) {
      return;
    }
    impl_->applyFourUpDefaultOrientations();
    impl_->syncOverlayGeometry(this);
  });
  QObject::connect(autoQuadAssignAct, &QAction::toggled, this, [this](bool checked) {
    if (!impl_) {
      return;
    }
    impl_->autoAssignFourUpViews_ = checked;
    if (checked && impl_->viewportLayoutMode_ ==
                       ArtifactCompositionEditor::Impl::ViewportLayoutMode::FourUp) {
      impl_->applyFourUpDefaultOrientations();
    }
     this->refreshEnabledState();
  });
  impl_->viewPresetButton_ = new QToolButton(impl_->topToolbar_);
  impl_->viewPresetButton_->setText(QStringLiteral("View"));
  impl_->viewPresetButton_->setMenu(viewPresetMenu);
  impl_->viewPresetButton_->setPopupMode(QToolButton::InstantPopup);
  impl_->viewPresetButton_->setToolButtonStyle(Qt::ToolButtonTextOnly);
  impl_->viewPresetButton_->setToolTip(
      QStringLiteral("Viewport orientation presets"));
  impl_->viewPresetButton_->setAccessibleName(QStringLiteral("View preset"));
  impl_->viewPresetButton_->setAccessibleDescription(QStringLiteral("Choose a viewport orientation preset"));
  QObject::connect(viewPresetMenu, &QMenu::aboutToShow, this,
                   [syncViewValueEditors]() { syncViewValueEditors(); });
  impl_->topToolbar_->addWidget(impl_->viewPresetButton_);
  impl_->viewportBookmarkButton_ = new QToolButton(impl_->topToolbar_);
  impl_->viewportBookmarkButton_->setText(QStringLiteral("Bookmark"));
  impl_->viewportBookmarkButton_->setIcon(QIcon(resolveIconPath("Studio/viewmenu_bookmarks.svg")));
  impl_->viewportBookmarkButton_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  impl_->viewportBookmarkButton_->setToolTip(
      QStringLiteral("Open the Camera ブックマーク menu from the main View menu"));
  impl_->viewportBookmarkButton_->setAccessibleName(QStringLiteral("Viewport bookmarks"));
  impl_->viewportBookmarkButton_->setAccessibleDescription(QStringLiteral("Open saved viewport camera bookmarks"));
  QObject::connect(impl_->viewportBookmarkButton_, &QToolButton::clicked, this,
                   [this]() {
                      if (!impl_) {
                        return;
                      }
                      auto *hostWindow = this->window();
                      QMenu *bookmarkMenu = nullptr;
                      if (hostWindow) {
                        if (auto *namedMenu = hostWindow->findChild<QMenu *>(
                                QStringLiteral("viewportBookmarkMenu"))) {
                          bookmarkMenu = namedMenu;
                        } else {
                          const auto menus = hostWindow->findChildren<QMenu *>();
                          for (QMenu *menu : menus) {
                            if (menu && menu->title().contains(QStringLiteral("Camera ブックマーク"))) {
                              bookmarkMenu = menu;
                              break;
                            }
                          }
                        }
                      }
                      if (!bookmarkMenu) {
                       return;
                     }
                     bookmarkMenu->popup(
                         impl_->viewportBookmarkButton_->mapToGlobal(
                             QPoint(0, impl_->viewportBookmarkButton_->height())));
                   });
  impl_->topToolbar_->addWidget(impl_->viewportBookmarkButton_);
  const auto applyViewPreset =
      [this](ArtifactCore::ViewOrientationHotspot hotspot) {
        if (!impl_) {
          return;
        }
        if (auto *controller = impl_->activeRenderController()) {
          controller->setViewportOrientation(hotspot);
          if (impl_->viewOrientationWidget_) {
            impl_->viewOrientationWidget_->setOrientationQuaternion(
                controller->viewportOrientationQuaternion());
          }
          if (impl_->overlayView_) {
            impl_->overlayView_->update();
          }
          impl_->refreshViewportStateLabels();
        }
      };
  impl_->topToolbar_->addSeparator();

  const auto addQuickViewPresetButton =
      [this, applyViewPreset](const QString &text,
                              ArtifactCore::ViewOrientationHotspot hotspot) {
        QAction *action = impl_->topToolbar_->addAction(text);
        action->setToolTip(QStringLiteral("Quick preset: %1 view").arg(text));
        QObject::connect(action, &QAction::triggered, this,
                         [applyViewPreset, hotspot]() { applyViewPreset(hotspot); });
        return action;
      };
  addQuickViewPresetButton(QStringLiteral("Front"),
                           ArtifactCore::ViewOrientationHotspot::Front);
  addQuickViewPresetButton(QStringLiteral("Top"),
                           ArtifactCore::ViewOrientationHotspot::Top);
  addQuickViewPresetButton(QStringLiteral("Right"),
                           ArtifactCore::ViewOrientationHotspot::Right);
  addQuickViewPresetButton(QStringLiteral("Back"),
                           ArtifactCore::ViewOrientationHotspot::Back);
  addQuickViewPresetButton(QStringLiteral("Left"),
                           ArtifactCore::ViewOrientationHotspot::Left);
  addQuickViewPresetButton(QStringLiteral("Bottom"),
                           ArtifactCore::ViewOrientationHotspot::Bottom);
  impl_->renderSuspendAction_ =
      impl_->topToolbar_->addAction(QStringLiteral("Render Hold Off"));
  impl_->renderSuspendAction_->setCheckable(true);
  impl_->renderSuspendAction_->setChecked(false);
  impl_->renderSuspendAction_->setToolTip(
      QStringLiteral("Suspend viewport cache invalidation"));
  QObject::connect(impl_->renderSuspendAction_, &QAction::toggled, this,
                   [this](bool checked) {
                     if (!impl_) {
                       return;
                     }
                     impl_->renderSuspendAction_->setChecked(checked);
                     impl_->refreshViewportStateLabels();
                   });
  impl_->previewOrbitAction_ = impl_->topToolbar_->addAction(QStringLiteral("Preview Off"));
  impl_->previewOrbitAction_->setCheckable(true);
  impl_->previewOrbitAction_->setChecked(false);
  impl_->previewOrbitAction_->setToolTip(
      QStringLiteral("Temporary view-only navigation session"));
  QObject::connect(impl_->previewOrbitAction_, &QAction::toggled, this,
                   [this](bool checked) {
                     if (!impl_) {
                       return;
                     }
                     impl_->setPreviewOrbitMode(this, checked);
                   });
  impl_->vectorScopeAction_ = impl_->topToolbar_->addAction(QStringLiteral("Vectorscope"));
  impl_->vectorScopeAction_->setToolTip(
      QStringLiteral("Show a vectorscope following the current composition preview frame"));
  QObject::connect(impl_->vectorScopeAction_, &QAction::triggered, this,
                   [this]() {
                     if (!impl_) {
                       return;
                     }
                     if (impl_->vectorScopeDialog_) {
                       impl_->vectorScopeDialog_->raise();
                       impl_->vectorScopeDialog_->activateWindow();
                       return;
                     }
                     auto *dialog = new QDialog(this);
                     dialog->setAttribute(Qt::WA_DeleteOnClose);
                     dialog->setWindowTitle(QStringLiteral("Preview Vectorscope"));
                     dialog->resize(360, 380);
                     auto *layout = new QVBoxLayout(dialog);
                     auto *tabs = new QTabWidget(dialog);
                     auto *vectorScope = new ArtifactWidgets::VectorScopeWidget(tabs);
                     vectorScope->setMode(ArtifactWidgets::VectorScopeMode::Standard);
                     auto *waveformScope = new ArtifactWidgets::WaveformScopeWidget(tabs);
                     waveformScope->setMode(ArtifactWidgets::WaveformMode::Luma);
                     auto *paradeScope = new ArtifactWidgets::ParadeScopeWidget(tabs);
                     paradeScope->setMode(ArtifactWidgets::ParadeMode::RGB);
                     auto *histogramWidget = new ArtifactWidgets::HistogramWidget(tabs);
                     histogramWidget->setMode(ArtifactWidgets::HistogramMode::Combined);
                     histogramWidget->setLogScale(true);
                     tabs->addTab(vectorScope, QStringLiteral("Vectorscope"));
                     tabs->addTab(waveformScope, QStringLiteral("Waveform"));
                     tabs->addTab(paradeScope, QStringLiteral("RGB Parade"));
                     tabs->addTab(histogramWidget, QStringLiteral("Histogram"));
                     layout->addWidget(tabs);
                     auto *timer = new QTimer(dialog);
                     QObject::connect(timer, &QTimer::timeout, dialog,
                                      [this, vectorScope, waveformScope, paradeScope,
                                       histogramWidget]() {
                                        if (!impl_ || !impl_->renderController_ || !vectorScope ||
                                            !waveformScope || !paradeScope || !histogramWidget) {
                                          return;
                                        }
                                        const auto frame =
                                            impl_->renderController_->captureCurrentFrameImage();
                                        vectorScope->updateFrame(frame);
                                        waveformScope->updateFrame(frame);
                                        paradeScope->updateFrame(frame);
                                        histogramWidget->updateFrame(frame);
                                      });
                     QObject::connect(dialog, &QDialog::finished, this,
                                      [this]() {
                                        if (impl_) {
                                          impl_->vectorScopeDialog_.clear();
                                        }
                                      });
                     impl_->vectorScopeDialog_ = dialog;
                     dialog->show();
                     timer->start(150);
                     QTimer::singleShot(0, dialog,
                                        [this, vectorScope, waveformScope, paradeScope,
                                         histogramWidget]() {
                       if (impl_ && impl_->renderController_ && vectorScope && waveformScope &&
                           paradeScope && histogramWidget) {
                         const auto frame =
                             impl_->renderController_->captureCurrentFrameImage();
                         vectorScope->updateFrame(frame);
                         waveformScope->updateFrame(frame);
                         paradeScope->updateFrame(frame);
                         histogramWidget->updateFrame(frame);
                       }
                     });
                   });
  impl_->topToolbar_->addSeparator();

  impl_->resetAction_ = impl_->topToolbar_->addAction("Reset");
  impl_->topToolbar_->addSeparator();
  impl_->zoomInAction_ = impl_->topToolbar_->addAction("Zoom+");
  impl_->zoomOutAction_ = impl_->topToolbar_->addAction("Zoom-");
  impl_->zoomFitAction_ = impl_->topToolbar_->addAction("Fill");
  impl_->zoom100Action_ = impl_->topToolbar_->addAction("100%");
  impl_->editTextAction_ = impl_->topToolbar_->addAction("Edit Text");
  impl_->editTextAction_->setToolTip(QStringLiteral("Edit current text layer"));
  impl_->editTextAction_->setShortcut(QKeySequence(Qt::Key_F2));

  auto* screenshotMenu = new QMenu(this);
  polishEditorMenu(screenshotMenu, this);
  impl_->quickScreenshotAction_ = screenshotMenu->addAction(QStringLiteral("Quick Screenshot"));
  impl_->quickScreenshotAction_->setToolTip(
      QStringLiteral("Save the current composition view with minimal options"));
  impl_->advancedScreenshotAction_ = screenshotMenu->addAction(QStringLiteral("Advanced Screenshot..."));
  impl_->advancedScreenshotAction_->setToolTip(
      QStringLiteral("Open the custom screenshot dialog"));
  screenshotMenu->addSeparator();
  impl_->viewportRenderOutputAction_ =
      screenshotMenu->addAction(QStringLiteral("Viewport Render Output..."));
  impl_->viewportRenderOutputAction_->setToolTip(
      QStringLiteral("Save the rendered viewport frame through the renderer pipeline\n"
                     "Supports single-frame export and multi-channel EXR"));
  impl_->viewportRenderOutputAction_->setShortcut(
      QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_S));
  impl_->viewportRenderOutputAction_->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  impl_->screenshotButton_ = new QToolButton(impl_->topToolbar_);
  impl_->screenshotButton_->setText(QStringLiteral("Screenshot"));
  impl_->screenshotButton_->setMenu(screenshotMenu);
  impl_->screenshotButton_->setPopupMode(QToolButton::InstantPopup);
  impl_->screenshotButton_->setAccessibleName(QStringLiteral("Screenshot"));
  impl_->screenshotButton_->setAccessibleDescription(QStringLiteral("Choose a composition screenshot action"));
  impl_->topToolbar_->addWidget(impl_->screenshotButton_);
  impl_->viewportRenderOutputButton_ = new QToolButton(impl_->topToolbar_);
  impl_->viewportRenderOutputButton_->setText(QStringLiteral("Render Output"));
  impl_->viewportRenderOutputButton_->setToolTip(
      QStringLiteral("Open the viewport render output export dialog"));
  impl_->viewportRenderOutputButton_->setAccessibleName(QStringLiteral("Render output"));
  impl_->viewportRenderOutputButton_->setAccessibleDescription(
      QStringLiteral("Open the viewport render output export dialog"));
  QObject::connect(impl_->viewportRenderOutputButton_, &QToolButton::clicked, this,
                   [this]() {
                     if (impl_) {
                       impl_->saveViewportRenderOutput(this);
                     }
                   });
  impl_->topToolbar_->addWidget(impl_->viewportRenderOutputButton_);

  impl_->compareAction_ = impl_->topToolbar_->addAction("A/B");
  impl_->compareAction_->setToolTip(
      QStringLiteral("Open the A/B compare surface in Contents Viewer"));
  impl_->compareAction_->setShortcut(
      QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_B));

  impl_->motionPathAction_ = impl_->topToolbar_->addAction("Motion Path");
  impl_->motionPathAction_->setCheckable(true);
  impl_->motionPathAction_->setToolTip(
      QStringLiteral("Show motion path overlay for the selected layer"));
  impl_->motionPathAction_->setShortcut(
      QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_M));
  impl_->effectHitboxAction_ = impl_->topToolbar_->addAction("Hitbox");
  impl_->effectHitboxAction_->setCheckable(true);
  impl_->effectHitboxAction_->setToolTip(
      QStringLiteral("Show selected layer bounds, masks, and matte source hitboxes"));
  impl_->effectHitboxAction_->setShortcut(
      QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_H));

  auto *toolMenu = new QMenu(this);
  polishEditorMenu(toolMenu, this);
  auto *toolGroup = new QActionGroup(this);
  toolGroup->setExclusive(true);
  const auto addToolAction = [&](const QString &text, const QString &iconName,
                                 ToolType toolType, bool checked) {
    QAction *action = toolMenu->addAction(loadIconWithFallback(iconName), text);
    action->setCheckable(true);
    action->setChecked(checked);
    if (toolType == ToolType::Pen) {
      action->setToolTip(QStringLiteral("Enter Mask editing; Roto fields appear in property panels where supported."));
    } else if (toolType == ToolType::Shape) {
      action->setToolTip(QStringLiteral("Enter Shape modeling; edit vertices and paths directly in the viewport."));
    } else if (toolType == ToolType::AnchorPoint) {
      action->setToolTip(QStringLiteral("Enter Anchor editing; drag the anchor point directly in the viewport."));
    }
    toolGroup->addAction(action);
    connect(action, &QAction::triggered, this,
            [this, toolType, text, iconName]() {
      if (auto *toolManager =
              ArtifactApplicationManager::instance()
                  ? ArtifactApplicationManager::instance()->toolManager()
                  : nullptr) {
        toolManager->setActiveTool(toolType);
      }
      impl_->forceFrontForPlanarEditingTool(toolType);
      if (toolType == ToolType::Pen && impl_->renderController_) {
        impl_->showMaskEditingGuides();
        impl_->renderController_->markRenderDirty();
      }
      if (impl_->toolModeButton_) {
        impl_->toolModeButton_->setText(text);
        impl_->toolModeButton_->setIcon(loadIconWithFallback(iconName));
      }
    });
  };
  addToolAction(QStringLiteral("Select"),
                QStringLiteral("Studio/toolbar_tool_select.svg"),
                ToolType::Selection, true);
  addToolAction(QStringLiteral("Hand"),
                QStringLiteral("Studio/toolbar_tool_hand.svg"), ToolType::Hand,
                false);
  addToolAction(QStringLiteral("Mask"),
                QStringLiteral("Studio/toolbar_tool_pen.svg"), ToolType::Pen,
                false);
  addToolAction(QStringLiteral("Shape"),
                QStringLiteral("Studio/toolbar_tool_shape.svg"), ToolType::Shape,
                false);
  addToolAction(QStringLiteral("Anchor"),
                QStringLiteral("Studio/toolbar_tool_anchor.svg"),
                ToolType::AnchorPoint, false);
  impl_->workspaceModeButton_ = new ViewportLayoutButton(impl_->topToolbar_);
  impl_->workspaceModeButton_->setObjectName(QStringLiteral("compositionWorkspaceModeButton"));
  impl_->workspaceModeButton_->setText(QStringLiteral("Animate"));
  impl_->workspaceModeButton_->setToolButtonStyle(Qt::ToolButtonTextOnly);
  impl_->workspaceModeButton_->setToolTip(
      QStringLiteral("Animate: AE-style transform and timeline editing. Click to switch to Design."));
  impl_->workspaceModeButton_->setActivatedCallback([this]() {
    if (!impl_ || !impl_->workspaceModeButton_) {
      return;
    }
    const bool enterDesign =
        impl_->workspaceMode_ == ArtifactCompositionEditor::Impl::WorkspaceMode::Animate;
    impl_->workspaceMode_ = enterDesign
                                ? ArtifactCompositionEditor::Impl::WorkspaceMode::Design
                                : ArtifactCompositionEditor::Impl::WorkspaceMode::Animate;
    const QString modeName = enterDesign ? QStringLiteral("Design")
                                         : QStringLiteral("Animate");
    impl_->workspaceModeButton_->setText(modeName);
    impl_->workspaceModeButton_->setToolTip(
        enterDesign
            ? QStringLiteral("Design: Figma-style structure and layout editing. Click to switch to Animate.")
            : QStringLiteral("Animate: AE-style transform and timeline editing. Click to switch to Design."));
    setProperty("artifactWorkspaceMode", modeName);
    for (auto &pane : impl_->panes_) {
      if (pane.view) {
        pane.view->setProperty("artifactWorkspaceMode", modeName);
      }
      if (pane.controller) {
        pane.controller->setInfoOverlayText(
            modeName,
            enterDesign ? QStringLiteral("Layout and structure editing")
                        : QStringLiteral("Transform and timeline editing"));
      }
    }
    impl_->refreshViewportStateLabels();
  });
  setProperty("artifactWorkspaceMode", QStringLiteral("Animate"));
  for (auto &pane : impl_->panes_) {
    if (pane.view) {
      pane.view->setProperty("artifactWorkspaceMode", QStringLiteral("Animate"));
    }
  }
  impl_->topToolbar_->addWidget(impl_->workspaceModeButton_);

  impl_->toolModeButton_ = new QToolButton(impl_->topToolbar_);
  impl_->toolModeButton_->setText(QStringLiteral("Select"));
  impl_->toolModeButton_->setIcon(
      loadIconWithFallback(QStringLiteral("Studio/toolbar_tool_select.svg")));
  impl_->toolModeButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
  impl_->toolModeButton_->setMenu(toolMenu);
  impl_->toolModeButton_->setPopupMode(QToolButton::InstantPopup);
  impl_->toolModeButton_->setToolTip(QStringLiteral("Select current editing tool. Mask opens mask editing. Anchor opens pivot editing."));
  impl_->toolModeButton_->setAccessibleName(QStringLiteral("Editing tool"));
  impl_->toolModeButton_->setAccessibleDescription(QStringLiteral("Choose the current composition editing tool"));
  impl_->topToolbar_->addWidget(impl_->toolModeButton_);

  auto *gizmoMenu = new QMenu(this);
  polishEditorMenu(gizmoMenu, this);
  gizmoMenu->setIcon(
      loadEditorMenuIcon(QStringLiteral("MaterialVS/neutral/transform.svg")));
  auto *gizmoGroup = new QActionGroup(this);
  gizmoGroup->setExclusive(true);
  const auto addGizmoAction = [&](const QString &text, const QString &iconPath,
                                  TransformGizmo::Mode mode, bool checked) {
    QAction *action = gizmoMenu->addAction(text);
    action->setCheckable(true);
    action->setChecked(checked);
    action->setIcon(loadEditorMenuIcon(iconPath));
    gizmoGroup->addAction(action);
    connect(action, &QAction::triggered, this, [this, mode]() {
      if (impl_->renderController_) {
        impl_->renderController_->setGizmoMode(mode);
      }
      impl_->refreshViewportStateLabels();
    });
  };
  addGizmoAction(QStringLiteral("Gizmo: All (W/R/S)"),
                 QStringLiteral("MaterialVS/neutral/view_sidebar.svg"),
                 TransformGizmo::Mode::All, true);
  addGizmoAction(QStringLiteral("Gizmo: Move (W)"),
                 QStringLiteral("MaterialVS/neutral/transform.svg"),
                 TransformGizmo::Mode::Move, false);
  addGizmoAction(QStringLiteral("Gizmo: Rotate (R)"),
                 QStringLiteral("Material/redo.svg"),
                 TransformGizmo::Mode::Rotate, false);
  addGizmoAction(QStringLiteral("Gizmo: Scale (S)"),
                 QStringLiteral("MaterialVS/neutral/crop.svg"),
                 TransformGizmo::Mode::Scale, false);
  gizmoMenu->addSeparator();
  impl_->gizmoVisibleAction_ = gizmoMenu->addAction(QStringLiteral("Show Gizmo"));
  impl_->gizmoVisibleAction_->setCheckable(true);
  impl_->gizmoVisibleAction_->setChecked(true);
  QObject::connect(impl_->gizmoVisibleAction_, &QAction::toggled, this,
                   [this](bool checked) {
                     if (!impl_) {
                       return;
                     }
                     if (auto *controller = impl_->renderController_) {
                       controller->setShowGizmoOverlay(checked);
                     }
                     impl_->forEachActiveSecondaryController(
                         [checked](CompositionRenderController *controller) {
                           controller->setShowGizmoOverlay(checked);
                         });
                     impl_->refreshViewportStateLabels();
                   });
  impl_->gizmoModeButton_ = new QToolButton(impl_->topToolbar_);
  impl_->gizmoModeButton_->setText(impl_->gizmoButtonLabel());
  impl_->gizmoModeButton_->setMenu(gizmoMenu);
  impl_->gizmoModeButton_->setIcon(
      loadEditorMenuIcon(QStringLiteral("MaterialVS/neutral/transform.svg")));
  impl_->gizmoModeButton_->setToolTip(impl_->gizmoButtonTooltip());
  impl_->gizmoModeButton_->setAccessibleName(QStringLiteral("Transform gizmo"));
  impl_->gizmoModeButton_->setAccessibleDescription(QStringLiteral("Choose transform gizmo mode and visibility"));
  impl_->gizmoModeButton_->setPopupMode(QToolButton::InstantPopup);
  impl_->topToolbar_->addWidget(impl_->gizmoModeButton_);

  impl_->chromeStrip_ = new QFrame(this);
  impl_->chromeStrip_->setObjectName(QStringLiteral("compositionChromeStrip"));
  impl_->chromeStrip_->setFrameShape(QFrame::StyledPanel);
  impl_->chromeStrip_->setFrameShadow(QFrame::Plain);
  impl_->chromeStrip_->setAutoFillBackground(true);
  impl_->chromeStrip_->setMinimumHeight(40);
  impl_->chromeStrip_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  {
    QPalette pal = impl_->chromeStrip_->palette();
    pal.setColor(QPalette::Window, QColor(theme.secondaryBackgroundColor));
    pal.setColor(QPalette::WindowText, QColor(theme.textColor));
    impl_->chromeStrip_->setPalette(pal);
  }
  auto *chromeLayout = new QHBoxLayout(impl_->chromeStrip_);
  chromeLayout->setContentsMargins(12, 6, 12, 6);
  chromeLayout->setSpacing(12);
  auto *chromeTextColumn = new QVBoxLayout();
  chromeTextColumn->setContentsMargins(0, 0, 0, 0);
  chromeTextColumn->setSpacing(1);
  impl_->chromeTitleLabel_ =
      new QLabel(QStringLiteral("Composition: <none>"), impl_->chromeStrip_);
  impl_->chromeDetailLabel_ = new QLabel(
      QStringLiteral("Layer: <none>  |  Selection: 0  |  Idle"),
      impl_->chromeStrip_);
  impl_->chromeMetaLabel_ =
      new QLabel(QStringLiteral("Render paused  |  No focus"),
                 impl_->chromeStrip_);
  impl_->chromeMetaLabel_->hide();
  QFont titleFont = impl_->chromeTitleLabel_->font();
  titleFont.setBold(true);
  titleFont.setPointSize(std::max(8, titleFont.pointSize()));
  impl_->chromeTitleLabel_->setFont(titleFont);
  chromeTextColumn->addWidget(impl_->chromeTitleLabel_);
  chromeTextColumn->addWidget(impl_->chromeDetailLabel_);
  chromeLayout->addLayout(chromeTextColumn, 1);
  chromeLayout->addWidget(impl_->chromeMetaLabel_, 0,
                          Qt::AlignRight | Qt::AlignVCenter);

  auto *pivotMenu = new QMenu(this);
  polishEditorMenu(pivotMenu, this);
  auto *pivotGroup = new QActionGroup(this);
  pivotGroup->setExclusive(true);
  const auto applyPivotPreset = [this](const bool useCenter) {
    auto *selection = ArtifactLayerSelectionManager::instance();
    const auto comp = resolvePreferredComposition();
    const auto layer =
        selection ? selection->currentLayer() : ArtifactAbstractLayerPtr{};
    if (!layer || !impl_ || !impl_->renderController_ || !comp) {
      return;
    }

    const QRectF localBounds = layer->localBounds();
    if (!localBounds.isValid() || localBounds.width() <= 0.0 ||
        localBounds.height() <= 0.0) {
      return;
    }

    const QPointF targetAnchor =
        useCenter ? localBounds.center() : localBounds.topLeft();

    auto &t3d = layer->transform3D();
    const RationalTime time(layer->currentFrame(), 30);
    const QPointF currentAnchor(t3d.anchorX(), t3d.anchorY());
    const QPointF delta = targetAnchor - currentAnchor;
    const double radians = t3d.rotation() * 3.14159265358979323846 / 180.0;
    const double cosA = std::cos(radians);
    const double sinA = std::sin(radians);
    const QPointF compensation(
        delta.x() * t3d.scaleX() * cosA - delta.y() * t3d.scaleY() * sinA,
        delta.x() * t3d.scaleX() * sinA + delta.y() * t3d.scaleY() * cosA);

    t3d.setAnchor(time, static_cast<float>(targetAnchor.x()),
                  static_cast<float>(targetAnchor.y()), t3d.anchorZ());
    t3d.setPosition(time,
                    t3d.positionX() + static_cast<float>(compensation.x()),
                    t3d.positionY() + static_cast<float>(compensation.y()));
    layer->setDirty(LayerDirtyFlag::Transform);
    layer->addDirtyReason(LayerDirtyReason::UserEdit);
    ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
        LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                          LayerChangedEvent::ChangeType::Modified});
    impl_->renderController_->markRenderDirty();
  };
  const auto addPivotAction = [&](const QString &text, bool useCenter,
                                  bool checked) {
    QAction *action = pivotMenu->addAction(text);
    action->setCheckable(true);
    action->setChecked(checked);
    pivotGroup->addAction(action);
    connect(action, &QAction::triggered, this,
            [applyPivotPreset, useCenter]() { applyPivotPreset(useCenter); });
  };
  addPivotAction(QStringLiteral("Pivot: Center"), true, false);
  addPivotAction(QStringLiteral("Pivot: Top Left"), false, false);
  impl_->pivotModeButton_ = new QToolButton(impl_->topToolbar_);
  impl_->pivotModeButton_->setText(QStringLiteral("Pivot"));
  impl_->pivotModeButton_->setMenu(pivotMenu);
  impl_->pivotModeButton_->setPopupMode(QToolButton::InstantPopup);
  impl_->pivotModeButton_->setAccessibleName(QStringLiteral("Pivot mode"));
  impl_->pivotModeButton_->setAccessibleDescription(QStringLiteral("Choose the transform pivot position"));
  impl_->topToolbar_->addWidget(impl_->pivotModeButton_);

  impl_->immersiveAction_ =
      impl_->topToolbar_->addAction(QStringLiteral("Immersive"));
  impl_->immersiveAction_->setCheckable(true);
  impl_->immersiveAction_->setShortcut(QKeySequence(Qt::Key_F11));
  impl_->immersiveAction_->setToolTip(
      QStringLiteral("Toggle immersive fullscreen mode"));
  QObject::connect(impl_->immersiveAction_, &QAction::toggled, this,
                   [this](bool checked) {
                     if (impl_) {
                       impl_->toggleImmersiveMode(this, checked);
                     }
                   });
  QObject::connect(impl_->quickScreenshotAction_, &QAction::triggered, this,
                   [this]() {
                     if (impl_) {
                       impl_->saveQuickScreenshot(this);
                     }
                   });
  QObject::connect(impl_->advancedScreenshotAction_, &QAction::triggered, this,
                   [this]() {
                     if (impl_) {
                       impl_->saveAdvancedScreenshot(this);
                     }
                   });
  QObject::connect(impl_->viewportRenderOutputAction_, &QAction::triggered, this,
                   [this]() {
                     if (impl_) {
                       impl_->saveViewportRenderOutput(this);
                     }
                   });

  const auto configureHud = [&theme](QToolBar *hud) {
    hud->setMovable(false);
    hud->setFloatable(false);
    hud->setIconSize(QSize(20, 20));
    hud->setToolButtonStyle(Qt::ToolButtonIconOnly);
    hud->setAutoFillBackground(true);
    QPalette pal = hud->palette();
    pal.setColor(QPalette::Window, QColor(theme.secondaryBackgroundColor));
    pal.setColor(QPalette::Button, QColor(theme.secondaryBackgroundColor));
    pal.setColor(QPalette::WindowText, QColor(theme.textColor));
    pal.setColor(QPalette::ButtonText, QColor(theme.textColor));
    hud->setPalette(pal);
    hud->hide();
  };
  const auto moveToolbarWidget = [](QToolBar *from, QToolBar *to,
                                    QWidget *widget) {
    if (!from || !to || !widget) {
      return;
    }
    const auto actions = from->actions();
    for (QAction *action : actions) {
      if (from->widgetForAction(action) == widget) {
        from->removeAction(action);
        to->addAction(action);
        return;
      }
    }
  };

  impl_->toolHud_ = new QToolBar(this);
  impl_->toolHud_->setObjectName(QStringLiteral("compositionToolHud"));
  configureHud(impl_->toolHud_);
  impl_->toolHud_->setProperty(
      "artifactHudOffset",
      ArtifactCore::LayeredConfigStore::instance().value(
          QStringLiteral("Viewport/Hud/ToolOffset")));
  impl_->toolHud_->addWidget(new ViewportHudGrip(
      impl_->toolHud_, QStringLiteral("Viewport/Hud/ToolOffset")));
  moveToolbarWidget(impl_->topToolbar_, impl_->toolHud_, impl_->toolModeButton_);
  moveToolbarWidget(impl_->topToolbar_, impl_->toolHud_, impl_->gizmoModeButton_);
  moveToolbarWidget(impl_->topToolbar_, impl_->toolHud_, impl_->pivotModeButton_);
  impl_->gizmoModeButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
  impl_->gizmoModeButton_->setIcon(
      loadIconWithFallback(QStringLiteral("Studio/toolbar_tool_move.svg")));
  impl_->pivotModeButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
  impl_->pivotModeButton_->setIcon(
      loadIconWithFallback(QStringLiteral("Studio/toolbar_tool_anchor.svg")));
  impl_->editTextAction_->setIcon(
      loadIconWithFallback(QStringLiteral("Studio/toolbar_tool_text.svg")));
  impl_->motionPathAction_->setIcon(
      loadIconWithFallback(QStringLiteral("Studio/toolbar_tool_pen.svg")));
  impl_->toolHud_->addAction(impl_->editTextAction_);
  impl_->toolHud_->addAction(impl_->motionPathAction_);

  impl_->zoomHud_ = new QToolBar(this);
  impl_->zoomHud_->setObjectName(QStringLiteral("compositionZoomHud"));
  configureHud(impl_->zoomHud_);
  impl_->zoomHud_->setProperty(
      "artifactHudOffset",
      ArtifactCore::LayeredConfigStore::instance().value(
          QStringLiteral("Viewport/Hud/ZoomOffset")));
  impl_->zoomHud_->addWidget(new ViewportHudGrip(
      impl_->zoomHud_, QStringLiteral("Viewport/Hud/ZoomOffset")));
  impl_->resetAction_->setIcon(
      loadIconWithFallback(QStringLiteral("Studio/toolbar_home_surface.svg")));
  impl_->zoomInAction_->setIcon(
      loadIconWithFallback(QStringLiteral("Studio/toolbar_zoom_in.svg")));
  impl_->zoomOutAction_->setIcon(
      loadIconWithFallback(QStringLiteral("Studio/toolbar_zoom_out.svg")));
  impl_->zoomFitAction_->setIcon(
      loadIconWithFallback(QStringLiteral("Studio/toolbar_zoom_fit.svg")));
  impl_->zoom100Action_->setIcon(
      loadIconWithFallback(QStringLiteral("Studio/toolbar_zoom_100.svg")));
  impl_->immersiveAction_->setIcon(
      loadIconWithFallback(QStringLiteral("Studio/fit_screen.svg")));
  for (QAction *action : {impl_->resetAction_, impl_->zoomOutAction_,
                          impl_->zoomInAction_, impl_->zoomFitAction_,
                          impl_->zoom100Action_, impl_->immersiveAction_}) {
    impl_->topToolbar_->removeAction(action);
    impl_->zoomHud_->addAction(action);
  }

  const auto topActions = impl_->topToolbar_->actions();
  for (QAction *action : topActions) {
    QWidget *widget = impl_->topToolbar_->widgetForAction(action);
    const bool keep = action == impl_->previewOrbitAction_ ||
                      widget == impl_->viewportLayoutButton_ ||
                      widget == impl_->viewPresetButton_ ||
                      widget == impl_->workspaceModeButton_ ||
                      widget == impl_->screenshotButton_ ||
                      widget == impl_->viewportRenderOutputButton_;
    if (!keep) {
      impl_->topToolbar_->removeAction(action);
    }
  }
  impl_->viewportRenderOutputButton_->setText(QStringLiteral("Render"));

  // Bottom Bar (Viewer Controls)
  impl_->bottomBar_ = new QWidget(this);
  impl_->bottomBar_->setObjectName(QStringLiteral("compositionBottomBar"));
  impl_->bottomBar_->setMinimumHeight(28);
  impl_->bottomBar_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  impl_->bottomBar_->setAutoFillBackground(true);
  {
    QPalette pal = impl_->bottomBar_->palette();
    pal.setColor(QPalette::Window, QColor(theme.secondaryBackgroundColor));
    pal.setColor(QPalette::WindowText, QColor(theme.textColor));
    impl_->bottomBar_->setPalette(pal);
  }

  auto *bottomLayout = new QHBoxLayout(impl_->bottomBar_);
  bottomLayout->setContentsMargins(6, 0, 6, 0);
  bottomLayout->setSpacing(8);

  // Resolution Dropdown — wired to PreviewQualityPreset
  impl_->resolutionCombo_ = new QComboBox(impl_->bottomBar_);
  impl_->resolutionCombo_->addItem("Full", QVariant::fromValue(static_cast<int>(
                                               PreviewQualityPreset::Final)));
  impl_->resolutionCombo_->addItem("Half", QVariant::fromValue(static_cast<int>(
                                               PreviewQualityPreset::Preview)));
  impl_->resolutionCombo_->addItem(
      "Quarter",
      QVariant::fromValue(static_cast<int>(PreviewQualityPreset::Draft)));
  impl_->resolutionCombo_->setMinimumWidth(70);
  impl_->resolutionCombo_->setAccessibleName(QStringLiteral("Preview quality"));
  impl_->resolutionCombo_->setAccessibleDescription(QStringLiteral("Choose the composition preview resolution"));
  {
    QPalette pal = impl_->resolutionCombo_->palette();
    pal.setColor(QPalette::Base, QColor(theme.backgroundColor));
    pal.setColor(QPalette::Button, QColor(theme.secondaryBackgroundColor));
    pal.setColor(QPalette::Text, QColor(theme.textColor));
    impl_->resolutionCombo_->setPalette(pal);
  }

  // Fast Preview Button (Lightning)
  impl_->fastPreviewBtn_ = new QToolButton(impl_->bottomBar_);
  impl_->fastPreviewBtn_->setIcon(
      loadIconWithFallback(QStringLiteral("Studio/quality_preview.svg")));
  impl_->fastPreviewBtn_->setToolTip("Fast Preview (Lightning)");
  impl_->fastPreviewBtn_->setAccessibleName(QStringLiteral("Fast Preview"));
  impl_->fastPreviewBtn_->setAccessibleDescription(QStringLiteral("Choose a fast preview quality preset"));
  impl_->fastPreviewBtn_->setPopupMode(QToolButton::InstantPopup);
  {
    QPalette pal = impl_->fastPreviewBtn_->palette();
    pal.setColor(QPalette::ButtonText, QColor(theme.textColor));
    impl_->fastPreviewBtn_->setPalette(pal);
  }

  auto *fastPreviewMenu = new QMenu(this);
  polishEditorMenu(fastPreviewMenu, this);
  QAction *fpOff = fastPreviewMenu->addAction("Off");
  QAction *fpAdaptive = fastPreviewMenu->addAction("Adaptive Resolution");
  QAction *fpDraft = fastPreviewMenu->addAction("Fast Draft");
  fpOff->setCheckable(true);
  fpAdaptive->setCheckable(true);
  fpDraft->setCheckable(true);
  fpOff->setChecked(true);
  auto *fpGroup = new QActionGroup(fastPreviewMenu);
  fpGroup->setExclusive(true);
  fpGroup->addAction(fpOff);
  fpGroup->addAction(fpAdaptive);
  fpGroup->addAction(fpDraft);

  QObject::connect(fpOff, &QAction::triggered, this, [this]() {
    if (auto *svc = ArtifactProjectService::instance())
      svc->setPreviewQualityPreset(PreviewQualityPreset::Final);
  });
  QObject::connect(fpAdaptive, &QAction::triggered, this, [this]() {
    if (auto *svc = ArtifactProjectService::instance())
      svc->setPreviewQualityPreset(PreviewQualityPreset::Preview);
  });
  QObject::connect(fpDraft, &QAction::triggered, this, [this]() {
    if (auto *svc = ArtifactProjectService::instance())
      svc->setPreviewQualityPreset(PreviewQualityPreset::Draft);
  });

  impl_->fastPreviewBtn_->setMenu(fastPreviewMenu);

  // HDR / SDR display mode.  The renderer reads this setting when the swap
  // chain is created; changing it here recreates the active VP target.
  impl_->hdrDisplayBtn_ = new QToolButton(impl_->bottomBar_);
  impl_->hdrDisplayBtn_->setPopupMode(QToolButton::InstantPopup);
  impl_->hdrDisplayBtn_->setAccessibleName(QStringLiteral("HDR display mode"));
  auto *hdrMenu = new QMenu(this);
  polishEditorMenu(hdrMenu, this);
  QAction *sdrAct = hdrMenu->addAction(QStringLiteral("SDR (sRGB)"));
  QAction *hdrAct = hdrMenu->addAction(QStringLiteral("HDR (scRGB)"));
  sdrAct->setCheckable(true);
  hdrAct->setCheckable(true);
  auto *hdrGroup = new QActionGroup(hdrMenu);
  hdrGroup->setExclusive(true);
  hdrGroup->addAction(sdrAct);
  hdrGroup->addAction(hdrAct);
  QObject::connect(sdrAct, &QAction::triggered, this, [this]() {
    Artifact::RenderConfig::setHDRDisplayEnabled(false);
    impl_->refreshHDRDisplayState();
    if (impl_->renderController_) {
      impl_->renderController_->recreateSwapChain(this);
    }
  });
  QObject::connect(hdrAct, &QAction::triggered, this, [this]() {
    Artifact::RenderConfig::setHDRDisplayEnabled(true);
    impl_->refreshHDRDisplayState();
    if (impl_->renderController_) {
      impl_->renderController_->recreateSwapChain(this);
    }
  });
  impl_->hdrDisplayBtn_->setMenu(hdrMenu);
  const bool hdrInitiallyEnabled = Artifact::RenderConfig::hdrDisplayEnabled();
  sdrAct->setChecked(!hdrInitiallyEnabled);
  hdrAct->setChecked(hdrInitiallyEnabled);
  impl_->refreshHDRDisplayState();

  // Display Options Button (Background / Grid / Guides)
  impl_->displayOptionsBtn_ = new QToolButton(impl_->bottomBar_);
  impl_->displayOptionsBtn_->setIcon(
      loadIconWithFallback(QStringLiteral("Studio/viewmenu_panels.svg")));
  impl_->displayOptionsBtn_->setToolTip(
      "Choose background, grid, and guide options");
  impl_->displayOptionsBtn_->setAccessibleName(QStringLiteral("Display options"));
  impl_->displayOptionsBtn_->setAccessibleDescription(QStringLiteral("Choose background, grid, and guide options"));
  impl_->displayOptionsBtn_->setPopupMode(QToolButton::InstantPopup);
  {
    QPalette pal = impl_->displayOptionsBtn_->palette();
    pal.setColor(QPalette::ButtonText, QColor(theme.textColor));
    impl_->displayOptionsBtn_->setPalette(pal);
  }

  auto *displayMenu = new QMenu(this);
  polishEditorMenu(displayMenu, this);
  QAction *solidBgAct = displayMenu->addAction("Solid");
  QAction *solidColorAct = displayMenu->addAction("Solid Color...");
  QAction *checkerboardAct = displayMenu->addAction("Checkerboard");
  auto *checkerboardSizeMenu = displayMenu->addMenu("Checkerboard Size");
  polishEditorMenu(checkerboardSizeMenu, this);
  QAction *mayaBgAct = displayMenu->addAction("Maya Gradient");
  auto *bgGroup = new QActionGroup(displayMenu);
  bgGroup->setExclusive(true);
  bgGroup->addAction(solidBgAct);
  bgGroup->addAction(checkerboardAct);
  bgGroup->addAction(mayaBgAct);
  QAction *gridAct = displayMenu->addAction("Grid");
  QAction *guidesAct = displayMenu->addAction("Guides");
  QAction *safeMarginsAct = displayMenu->addAction("Safe Area");
  QAction *originAct = displayMenu->addAction("Origin");
  QAction *anchorCenterAct = displayMenu->addAction("Anchor / Center");
  QAction *cameraOverlayAct = displayMenu->addAction("Camera Frustum");
  QAction *densityHeatmapAct = displayMenu->addAction("Density Heatmap");
  QAction *layerChromeAct = displayMenu->addAction("Layer Controls");
  QAction *lockViewAct = displayMenu->addAction("Lock View to Selected");
  auto *onionMenu = displayMenu->addMenu("Onion Skin");
  polishEditorMenu(onionMenu, this);
  QAction *onionEnableAct = onionMenu->addAction("Enable");
  auto *onionFrameMenu = onionMenu->addMenu("Frame Count");
  polishEditorMenu(onionFrameMenu, this);
  auto *onionOpacityMenu = onionMenu->addMenu("Opacity");
  polishEditorMenu(onionOpacityMenu, this);
  displayMenu->addSeparator();
  QAction *loadReferenceImageAct =
      displayMenu->addAction("Load Reference Image...");
  QAction *showReferenceImageAct =
      displayMenu->addAction("Show Reference Image");
  QAction *clearReferenceImageAct =
      displayMenu->addAction("Clear Reference Image");
  QAction *colorSamplerAct =
      displayMenu->addAction("Color Sampler (Info Palette)");
  QAction *autoColorPaletteAct =
      displayMenu->addAction("Auto Color Palette");
  displayMenu->addSeparator();
  QAction *gpuBlendAct = displayMenu->addAction("GPU Blend Path");
  impl_->densityHeatmapAction_ = densityHeatmapAct;
  const std::array<float, 8> checkerboardSizes{
      8.0f, 12.0f, 16.0f, 24.0f, 32.0f, 48.0f, 64.0f, 96.0f};
  auto *checkerboardSizeGroup = new QActionGroup(displayMenu);
  checkerboardSizeGroup->setExclusive(true);
  checkerboardAct->setCheckable(true);
  gridAct->setCheckable(true);
  guidesAct->setCheckable(true);
  safeMarginsAct->setCheckable(true);
  originAct->setCheckable(true);
  anchorCenterAct->setCheckable(true);
  cameraOverlayAct->setCheckable(true);
  cameraOverlayAct->setShortcut(
      ArtifactCore::ShortcutBindings::instance().shortcut(
          ArtifactCore::ShortcutId::ViewToggleCameraFrustum));
  cameraOverlayAct->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  cameraOverlayAct->setToolTip(
      QStringLiteral("Toggle the camera frustum overlay (%1)")
          .arg(ArtifactCore::ShortcutBindings::instance().shortcutText(
              ArtifactCore::ShortcutId::ViewToggleCameraFrustum)));
  densityHeatmapAct->setCheckable(true);
  layerChromeAct->setCheckable(true);
  layerChromeAct->setChecked(impl_->layerChromeVisible_);
  impl_->layerChromeAction_ = layerChromeAct;
  lockViewAct->setCheckable(true);
  lockViewAct->setChecked(impl_->lockViewToSelection_);
  impl_->lockViewAction_ = lockViewAct;
  onionEnableAct->setCheckable(true);
  showReferenceImageAct->setCheckable(true);
  colorSamplerAct->setCheckable(true);
  autoColorPaletteAct->setCheckable(true);
  gpuBlendAct->setCheckable(true);
  anchorCenterAct->setToolTip(
      QStringLiteral("Show the selected layer anchor point and center point"));
  densityHeatmapAct->setToolTip(
      QStringLiteral("Show a grid-based visual density heatmap on the composition"));
  layerChromeAct->setToolTip(
      QStringLiteral("Show or hide the layer-specific controls in the top chrome"));
  lockViewAct->setToolTip(
      QStringLiteral("Keep the viewport centered on the selected layer"));
  onionEnableAct->setToolTip(
      QStringLiteral("Overlay captured previous frames over the current viewport"));
  showReferenceImageAct->setToolTip(
      QStringLiteral("Show the loaded reference image over the viewport"));
  clearReferenceImageAct->setToolTip(
      QStringLiteral("Remove the current viewport reference image"));
  colorSamplerAct->setToolTip(
      QStringLiteral("Show the cursor-under color as RGB / HSL / HEX / coordinates"));
  autoColorPaletteAct->setToolTip(
      QStringLiteral("Extract dominant colors from the reference image and show a generated harmony palette"));
  gpuBlendAct->setToolTip(
      QStringLiteral("Enable the compute-shader blend path when the composition needs masks, non-normal blending, or rasterizer effects"));
  gpuBlendAct->setStatusTip(
      QStringLiteral("Toggle the experimental GPU blend path"));
  onionEnableAct->setCheckable(true);
  onionEnableAct->setChecked(false);
  onionFrameMenu->clear();
  onionOpacityMenu->clear();
  auto *onionFrameGroup = new QActionGroup(onionFrameMenu);
  onionFrameGroup->setExclusive(true);
  for (int count = 1; count <= 5; ++count) {
    QAction *countAct = onionFrameMenu->addAction(QStringLiteral("%1 frame%2")
                                                      .arg(count)
                                                      .arg(count == 1 ? "" : "s"));
    countAct->setCheckable(true);
    countAct->setData(count);
    onionFrameGroup->addAction(countAct);
  }
  auto *onionOpacityGroup = new QActionGroup(onionOpacityMenu);
  onionOpacityGroup->setExclusive(true);
  for (int opacity : {10, 20, 30, 40, 50, 60, 70, 80}) {
    QAction *opacityAct =
        onionOpacityMenu->addAction(QStringLiteral("%1%").arg(opacity));
    opacityAct->setCheckable(true);
    opacityAct->setData(opacity);
    onionOpacityGroup->addAction(opacityAct);
  }
  showReferenceImageAct->setEnabled(false);
  clearReferenceImageAct->setEnabled(false);
  autoColorPaletteAct->setEnabled(false);
  for (const float size : checkerboardSizes) {
    QAction *sizeAct = checkerboardSizeMenu->addAction(
        QStringLiteral("%1 px").arg(QString::number(size, 'f', 0)));
    sizeAct->setCheckable(true);
    sizeAct->setData(size);
    checkerboardSizeGroup->addAction(sizeAct);
    QObject::connect(sizeAct, &QAction::triggered, this, [this, size]() {
      if (impl_->renderController_) {
        impl_->renderController_->setCheckerboardSize(size);
        impl_->forEachActiveSecondaryController(
            [size](CompositionRenderController *controller) {
              controller->setCheckerboardSize(size);
            });
        if (auto *settings = ArtifactCore::ArtifactAppSettings::instance()) {
          settings->setCompositionCheckerboardSize(size);
        }
      }
    });
  }
  impl_->displayOptionsBtn_->setMenu(displayMenu);

  // Connect actions
  QObject::connect(solidBgAct, &QAction::triggered, this, [this]() {
    if (impl_->renderController_) {
      impl_->renderController_->setCompositionBackgroundMode(
          static_cast<int>(CompositionBackgroundMode::Solid));
      impl_->forEachActiveSecondaryController(
          [](CompositionRenderController *controller) {
            controller->setCompositionBackgroundMode(
                static_cast<int>(CompositionBackgroundMode::Solid));
          });
      if (auto *settings = ArtifactCore::ArtifactAppSettings::instance()) {
        settings->setCompositionBackgroundMode(
            static_cast<int>(CompositionBackgroundMode::Solid));
      }
    }
  });
  QObject::connect(solidColorAct, &QAction::triggered, this, [this]() {
    if (!impl_->renderController_) {
      return;
    }
    const FloatColor initial = impl_->renderController_->clearColor();
    ArtifactWidgets::FloatColorPicker picker(this);
    picker.setColor(initial);
    picker.setInitialColor(initial);
    if (picker.exec() == QDialog::Accepted) {
      const FloatColor chosen = picker.getColor();
      impl_->renderController_->setClearColor(chosen);
      impl_->renderController_->setCompositionBackgroundMode(
          static_cast<int>(CompositionBackgroundMode::Solid));
      impl_->forEachActiveSecondaryController(
          [&chosen](CompositionRenderController *controller) {
            controller->setClearColor(chosen);
            controller->setCompositionBackgroundMode(
                static_cast<int>(CompositionBackgroundMode::Solid));
          });
      if (auto *settings = ArtifactCore::ArtifactAppSettings::instance()) {
        settings->setCompositionBackgroundMode(
            static_cast<int>(CompositionBackgroundMode::Solid));
      }
    }
  });
  QObject::connect(checkerboardAct, &QAction::triggered, this, [this]() {
    if (impl_->renderController_) {
      impl_->renderController_->setCompositionBackgroundMode(
          static_cast<int>(CompositionBackgroundMode::Checkerboard));
      impl_->forEachActiveSecondaryController(
          [](CompositionRenderController *controller) {
            controller->setCompositionBackgroundMode(
                static_cast<int>(CompositionBackgroundMode::Checkerboard));
          });
      if (auto *settings = ArtifactCore::ArtifactAppSettings::instance()) {
        settings->setCompositionBackgroundMode(
            static_cast<int>(CompositionBackgroundMode::Checkerboard));
      }
    }
  });
  QObject::connect(mayaBgAct, &QAction::triggered, this, [this]() {
    if (impl_->renderController_) {
      impl_->renderController_->setCompositionBackgroundMode(
          static_cast<int>(CompositionBackgroundMode::MayaGradient));
      impl_->forEachActiveSecondaryController(
          [](CompositionRenderController *controller) {
            controller->setCompositionBackgroundMode(
                static_cast<int>(CompositionBackgroundMode::MayaGradient));
          });
      if (auto *settings = ArtifactCore::ArtifactAppSettings::instance()) {
        settings->setCompositionBackgroundMode(
            static_cast<int>(CompositionBackgroundMode::MayaGradient));
      }
    }
  });
  QObject::connect(gridAct, &QAction::toggled, this, [this](bool checked) {
    if (impl_->renderController_) {
      impl_->renderController_->setShowGrid(checked);
      impl_->forEachActiveSecondaryController(
          [checked](CompositionRenderController *controller) {
            controller->setShowGrid(checked);
          });
      if (auto *settings = ArtifactCore::ArtifactAppSettings::instance()) {
        settings->setCompositionShowGrid(checked);
      }
    }
  });
  QObject::connect(guidesAct, &QAction::toggled, this, [this](bool checked) {
    if (impl_->renderController_) {
      impl_->renderController_->setShowGuides(checked);
      impl_->forEachActiveSecondaryController(
          [checked](CompositionRenderController *controller) {
            controller->setShowGuides(checked);
          });
      if (auto *settings = ArtifactCore::ArtifactAppSettings::instance()) {
        settings->setCompositionShowGuides(checked);
      }
    }
  });
  QObject::connect(safeMarginsAct, &QAction::toggled, this,
                   [this](bool checked) {
                     if (impl_->renderController_) {
                       impl_->renderController_->setShowSafeMargins(checked);
                       impl_->forEachActiveSecondaryController(
                           [checked](CompositionRenderController *controller) {
                             controller->setShowSafeMargins(checked);
                           });
                       if (auto *settings =
                               ArtifactCore::ArtifactAppSettings::instance()) {
                         settings->setCompositionShowSafeMargins(checked);
                       }
                     }
                   });
  QObject::connect(originAct, &QAction::toggled, this,
                   [this](bool checked) {
                     if (impl_->renderController_) {
                       impl_->renderController_->setShowOriginOverlay(checked);
                       impl_->forEachActiveSecondaryController(
                           [checked](CompositionRenderController *controller) {
                             controller->setShowOriginOverlay(checked);
                           });
                     }
                   });
  QObject::connect(anchorCenterAct, &QAction::toggled, this,
                   [this](bool checked) {
                     if (impl_->renderController_) {
                       impl_->renderController_->setShowAnchorCenterOverlay(checked);
                       impl_->forEachActiveSecondaryController(
                           [checked](CompositionRenderController *controller) {
                             controller->setShowAnchorCenterOverlay(checked);
                           });
                       if (auto *settings =
                               ArtifactCore::ArtifactAppSettings::instance()) {
                         settings->setCompositionShowAnchorCenterOverlay(
                             checked);
                       }
                     }
                   });
  QObject::connect(cameraOverlayAct, &QAction::toggled, this,
                   [this](bool checked) {
                     if (impl_->renderController_) {
                       impl_->renderController_->setShowCameraFrustumOverlay(checked);
                       impl_->forEachActiveSecondaryController(
                           [checked](CompositionRenderController *controller) {
                             controller->setShowCameraFrustumOverlay(checked);
                           });
                       if (auto *settings =
                               ArtifactCore::ArtifactAppSettings::instance()) {
                         settings->setCompositionShowCameraFrustumOverlay(
                             checked);
                       }
                     }
                   });
  QObject::connect(densityHeatmapAct, &QAction::toggled, this,
                   [this](bool checked) {
                     if (impl_->renderController_) {
                       impl_->renderController_->setShowDensityHeatmapOverlay(
                           checked);
                       impl_->forEachActiveSecondaryController(
                           [checked](CompositionRenderController *controller) {
                             controller->setShowDensityHeatmapOverlay(checked);
                           });
                       if (auto *settings =
                               ArtifactCore::ArtifactAppSettings::instance()) {
                         settings->setCompositionShowDensityHeatmapOverlay(
                             checked);
                       }
                     }
                   });
  QObject::connect(layerChromeAct, &QAction::toggled, this,
                   [this](bool checked) {
                     if (!impl_) {
                       return;
                     }
                     impl_->layerChromeVisible_ = checked;
    this->refreshEnabledState();
                   });
  QObject::connect(lockViewAct, &QAction::toggled, this,
                   [this](bool checked) {
                     if (!impl_) {
                       return;
                     }
                     impl_->lockViewToSelection_ = checked;
                     if (checked) {
                       if (auto *controller = impl_->activeRenderController()) {
                         controller->focusSelectedLayer();
                       }
                      }
                      this->refreshEnabledState();
                    });
  QObject::connect(onionEnableAct, &QAction::toggled, this, [this](bool checked) {
    if (!impl_) {
      return;
    }
    if (auto *controller = impl_->renderController_) {
      controller->setShowOnionSkin(checked);
    }
    impl_->forEachActiveSecondaryController(
        [checked](CompositionRenderController *controller) {
          controller->setShowOnionSkin(checked);
        });
                       this->refreshEnabledState();
  });
  for (QAction *action : onionFrameMenu->actions()) {
    QObject::connect(action, &QAction::triggered, this, [this, action]() {
      if (!impl_) {
        return;
      }
      const int count = action->data().toInt();
      if (auto *controller = impl_->renderController_) {
        controller->setOnionSkinFrameCount(count);
      }
      impl_->forEachActiveSecondaryController(
          [count](CompositionRenderController *controller) {
            controller->setOnionSkinFrameCount(count);
          });
       this->refreshEnabledState();
    });
  }
  for (QAction *action : onionOpacityMenu->actions()) {
    QObject::connect(action, &QAction::triggered, this, [this, action]() {
      if (!impl_) {
        return;
      }
      const int opacity = action->data().toInt();
      if (auto *controller = impl_->renderController_) {
        controller->setOnionSkinOpacity(opacity);
      }
      impl_->forEachActiveSecondaryController(
          [opacity](CompositionRenderController *controller) {
            controller->setOnionSkinOpacity(opacity);
          });
       this->refreshEnabledState();
    });
  }
  QObject::connect(loadReferenceImageAct, &QAction::triggered, this,
                   [this, showReferenceImageAct, clearReferenceImageAct,
                    autoColorPaletteAct]() {
                     if (!impl_->renderController_) {
                       return;
                     }
                     const QString filePath = QFileDialog::getOpenFileName(
                         this, QStringLiteral("Reference Image を選択"),
                         QString(),
                         QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.tif *.tiff);;All Files (*.*)"));
                     if (filePath.isEmpty()) {
                       return;
                     }
                     QImageReader reader(filePath);
                     if (!reader.canRead()) {
                       QMessageBox::warning(
                           this, QStringLiteral("Reference Image"),
                           QStringLiteral("画像を読み込めませんでした。"));
                       return;
                     }
                     const QImage image = reader.read();
                     if (image.isNull()) {
                       QMessageBox::warning(
                           this, QStringLiteral("Reference Image"),
                           QStringLiteral("画像を読み込めませんでした。"));
                       return;
                     }
                     impl_->renderController_->setReferenceOverlayImage(image);
                     impl_->forEachActiveSecondaryController(
                         [&image](CompositionRenderController *controller) {
                           controller->setReferenceOverlayImage(image);
                         });
                     showReferenceImageAct->setEnabled(true);
                     clearReferenceImageAct->setEnabled(true);
                     autoColorPaletteAct->setEnabled(true);
                     showReferenceImageAct->setChecked(true);
                   });
  QObject::connect(showReferenceImageAct, &QAction::toggled, this,
                   [this](bool checked) {
                     if (impl_->renderController_) {
                       impl_->renderController_->setShowReferenceOverlay(
                           checked);
                       impl_->forEachActiveSecondaryController(
                           [checked](CompositionRenderController *controller) {
                           controller->setShowReferenceOverlay(checked);
                         });
                     }
                   });
  QObject::connect(colorSamplerAct, &QAction::toggled, this,
                   [this](bool checked) {
                     if (impl_->renderController_) {
                       impl_->renderController_->setShowColorSamplerOverlay(
                           checked);
                       impl_->forEachActiveSecondaryController(
                           [checked](CompositionRenderController *controller) {
                             controller->setShowColorSamplerOverlay(checked);
                           });
                     }
                   });
  QObject::connect(autoColorPaletteAct, &QAction::toggled, this,
                   [this](bool checked) {
                     if (impl_->renderController_) {
                       impl_->renderController_->setShowAutoColorPaletteOverlay(
                           checked);
                       impl_->forEachActiveSecondaryController(
                           [checked](CompositionRenderController *controller) {
                             controller->setShowAutoColorPaletteOverlay(
                                 checked);
                           });
                       if (checked &&
                           !impl_->renderController_->hasReferenceOverlayImage()) {
                         QMessageBox::information(
                             this, QStringLiteral("Auto Color Palette"),
                             QStringLiteral("Reference Image Overlay を読み込むと、支配色抽出と調和パレット生成を表示できます。"));
                       }
                     }
                   });
  QObject::connect(clearReferenceImageAct, &QAction::triggered, this,
                   [this, showReferenceImageAct, clearReferenceImageAct,
                    autoColorPaletteAct]() {
                     if (!impl_->renderController_) {
                       return;
                     }
                     impl_->renderController_->clearReferenceOverlayImage();
                     impl_->forEachActiveSecondaryController(
                         [](CompositionRenderController *controller) {
                           controller->clearReferenceOverlayImage();
                         });
                     const QSignalBlocker blocker(showReferenceImageAct);
                     showReferenceImageAct->setChecked(false);
                     showReferenceImageAct->setEnabled(false);
                     clearReferenceImageAct->setEnabled(false);
                     {
                       const QSignalBlocker paletteBlocker(autoColorPaletteAct);
                       autoColorPaletteAct->setChecked(false);
                       autoColorPaletteAct->setEnabled(false);
                     }
                   });
  QObject::connect(gpuBlendAct, &QAction::toggled, this, [this](bool checked) {
    if (impl_->renderController_) {
      impl_->renderController_->setGpuBlendEnabled(checked);
      impl_->forEachActiveSecondaryController(
          [checked](CompositionRenderController *controller) {
            controller->setGpuBlendEnabled(checked);
          });
    }
  });

  impl_->shadingButton_ = new QToolButton(impl_->bottomBar_);
  impl_->shadingButton_->setText(impl_->viewportChannelDisplayLabel());
  impl_->shadingButton_->setToolTip(impl_->shadingButtonTooltip());
  impl_->shadingButton_->setAccessibleName(QStringLiteral("Viewport shading"));
  impl_->shadingButton_->setAccessibleDescription(QStringLiteral("Choose viewport channel and shading display"));
  impl_->shadingButton_->setPopupMode(QToolButton::InstantPopup);
  {
    QPalette pal = impl_->shadingButton_->palette();
    pal.setColor(QPalette::ButtonText, QColor(theme.textColor));
    impl_->shadingButton_->setPalette(pal);
  }
  auto *shadingMenu = new QMenu(this);
  polishEditorMenu(shadingMenu, this);
  auto *channelGroup = new QActionGroup(shadingMenu);
  channelGroup->setExclusive(true);
  const auto addChannelAction = [&](const QString &text,
                                    ViewportChannelDisplayMode mode,
                                    bool checked) {
    QAction *action = shadingMenu->addAction(text);
    action->setCheckable(true);
    action->setChecked(checked);
    channelGroup->addAction(action);
    QObject::connect(action, &QAction::triggered, this, [this, mode]() {
      if (impl_) {
        impl_->setViewportChannelDisplayMode(this, mode);
      }
      if (mode == ViewportChannelDisplayMode::Color ||
          mode == ViewportChannelDisplayMode::Alpha) {
        if (auto *app = Artifact::ApplicationService::instance()) {
          if (auto *toolService = app->toolService()) {
            const DisplayMode serviceMode =
                mode == ViewportChannelDisplayMode::Alpha
                    ? DisplayMode::Alpha
                    : DisplayMode::Color;
            toolService->setDisplayMode(serviceMode);
            impl_->lastToolServiceDisplayMode_ = serviceMode;
          }
        }
      }
    });
    return action;
  };
  QAction *channelColorAct =
      addChannelAction(QStringLiteral("Color"), ViewportChannelDisplayMode::Color, true);
  QAction *channelAlphaAct =
      addChannelAction(QStringLiteral("Alpha"), ViewportChannelDisplayMode::Alpha, false);
  QAction *channelColorAlphaAct =
      addChannelAction(QStringLiteral("RGB + Alpha"), ViewportChannelDisplayMode::ColorAlpha, false);
  QAction *channelRedAct =
      addChannelAction(QStringLiteral("Red"), ViewportChannelDisplayMode::Red, false);
  QAction *channelGreenAct =
      addChannelAction(QStringLiteral("Green"), ViewportChannelDisplayMode::Green, false);
  QAction *channelBlueAct =
      addChannelAction(QStringLiteral("Blue"), ViewportChannelDisplayMode::Blue, false);
  channelColorAct->setShortcut(QKeySequence(Qt::ALT | Qt::Key_2));
  channelAlphaAct->setShortcut(QKeySequence(Qt::ALT | Qt::Key_3));
  channelColorAlphaAct->setShortcut(QKeySequence(Qt::ALT | Qt::Key_4));
  channelRedAct->setShortcut(QKeySequence(Qt::ALT | Qt::Key_5));
  channelGreenAct->setShortcut(QKeySequence(Qt::ALT | Qt::Key_6));
  channelBlueAct->setShortcut(QKeySequence(Qt::ALT | Qt::Key_7));
  for (QAction *action :
       {channelColorAct, channelAlphaAct, channelColorAlphaAct,
        channelRedAct, channelGreenAct, channelBlueAct}) {
    action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  }
  shadingMenu->addSeparator();
  QAction *channelDepthAct =
      addChannelAction(QStringLiteral("Depth"), ViewportChannelDisplayMode::Depth, false);
  QAction *channelEmissionAct =
      addChannelAction(QStringLiteral("Emission"), ViewportChannelDisplayMode::Emission, false);
  QAction *channelObjectIdAct =
      addChannelAction(QStringLiteral("Object ID"), ViewportChannelDisplayMode::ObjectId, false);
  QAction *channelMaterialIdAct =
      addChannelAction(QStringLiteral("Material ID"), ViewportChannelDisplayMode::MaterialId, false);
  QAction *channelAlbedoAct =
      addChannelAction(QStringLiteral("Albedo"), ViewportChannelDisplayMode::Albedo, false);
  QAction *channelAlbedoRAct =
      addChannelAction(QStringLiteral("Albedo R"), ViewportChannelDisplayMode::AlbedoR, false);
  QAction *channelAlbedoGAct =
      addChannelAction(QStringLiteral("Albedo G"), ViewportChannelDisplayMode::AlbedoG, false);
  QAction *channelAlbedoBAct =
      addChannelAction(QStringLiteral("Albedo B"), ViewportChannelDisplayMode::AlbedoB, false);
  QAction *channelNormalAct =
      addChannelAction(QStringLiteral("Normal"), ViewportChannelDisplayMode::Normal, false);
  QAction *channelNormalXAct =
      addChannelAction(QStringLiteral("Normal X"), ViewportChannelDisplayMode::NormalX, false);
  QAction *channelNormalYAct =
      addChannelAction(QStringLiteral("Normal Y"), ViewportChannelDisplayMode::NormalY, false);
  QAction *channelNormalZAct =
      addChannelAction(QStringLiteral("Normal Z"), ViewportChannelDisplayMode::NormalZ, false);
  QAction *channelVelocityAct =
      addChannelAction(QStringLiteral("Velocity"), ViewportChannelDisplayMode::Velocity, false);
  QAction *channelVelocityXAct =
      addChannelAction(QStringLiteral("Velocity X"), ViewportChannelDisplayMode::VelocityX, false);
  QAction *channelVelocityYAct =
      addChannelAction(QStringLiteral("Velocity Y"), ViewportChannelDisplayMode::VelocityY, false);
  shadingMenu->addSeparator();
  auto *qualityGroup = new QActionGroup(shadingMenu);
  qualityGroup->setExclusive(true);
  const auto addQualityAction = [&](const QString &text,
                                    PreviewQualityPreset preset,
                                    bool checked) {
    QAction *action = shadingMenu->addAction(text);
    action->setCheckable(true);
    action->setChecked(checked);
    qualityGroup->addAction(action);
    QObject::connect(action, &QAction::triggered, this, [preset]() {
      if (auto *svc = ArtifactProjectService::instance()) {
        svc->setPreviewQualityPreset(preset);
      }
    });
    return action;
  };
  QAction *qualityFullAct =
      addQualityAction(QStringLiteral("Full"), PreviewQualityPreset::Final, true);
  QAction *qualityHalfAct =
      addQualityAction(QStringLiteral("Half"), PreviewQualityPreset::Preview, false);
  QAction *qualityQuarterAct =
      addQualityAction(QStringLiteral("Quarter"), PreviewQualityPreset::Draft, false);
  shadingMenu->addSeparator();
  QAction *shadeGridAct = shadingMenu->addAction(QStringLiteral("Grid"));
  QAction *shadeGuidesAct = shadingMenu->addAction(QStringLiteral("Guides"));
  QAction *shadeSafeAreaAct = shadingMenu->addAction(QStringLiteral("Safe Area"));
  QAction *shadeMotionPathAct = shadingMenu->addAction(QStringLiteral("Motion Path"));
  QAction *shadeEffectHitboxAct =
      shadingMenu->addAction(QStringLiteral("Effect Hitbox"));
  QAction *shadeAnchorCenterAct =
      shadingMenu->addAction(QStringLiteral("Anchor / Center"));
  QAction *shadeCameraFrustumAct =
      shadingMenu->addAction(QStringLiteral("Camera Frustum"));
  QAction *shadeDensityHeatmapAct =
      shadingMenu->addAction(QStringLiteral("Density Heatmap"));
  QAction *shadeGizmoAct = shadingMenu->addAction(QStringLiteral("Gizmo"));
  QAction *shadeXRayAct = shadingMenu->addAction(QStringLiteral("X-Ray"));
  QAction *shadeIsolationAct =
      shadingMenu->addAction(QStringLiteral("Isolate Selected"));
  for (QAction *action : {shadeGridAct, shadeGuidesAct, shadeSafeAreaAct,
                          shadeMotionPathAct, shadeEffectHitboxAct,
                          shadeAnchorCenterAct, shadeCameraFrustumAct,
                          shadeDensityHeatmapAct, shadeGizmoAct, shadeXRayAct,
                          shadeIsolationAct}) {
    action->setCheckable(true);
  }
  impl_->xRayAction_ = shadeXRayAct;
  impl_->isolationAction_ = shadeIsolationAct;
  QObject::connect(shadeGridAct, &QAction::toggled, gridAct, &QAction::setChecked);
  QObject::connect(shadeGuidesAct, &QAction::toggled, guidesAct, &QAction::setChecked);
  QObject::connect(shadeSafeAreaAct, &QAction::toggled, safeMarginsAct,
                   &QAction::setChecked);
  QObject::connect(shadeMotionPathAct, &QAction::toggled, impl_->motionPathAction_,
                   &QAction::setChecked);
  QObject::connect(shadeEffectHitboxAct, &QAction::toggled,
                   impl_->effectHitboxAction_, &QAction::setChecked);
  QObject::connect(shadeAnchorCenterAct, &QAction::toggled, anchorCenterAct,
                   &QAction::setChecked);
  QObject::connect(shadeCameraFrustumAct, &QAction::toggled, cameraOverlayAct,
                   &QAction::setChecked);
  QObject::connect(shadeDensityHeatmapAct, &QAction::toggled, densityHeatmapAct,
                   &QAction::setChecked);
  QObject::connect(shadeGizmoAct, &QAction::toggled, impl_->gizmoVisibleAction_,
                   &QAction::setChecked);
  QObject::connect(shadeXRayAct, &QAction::toggled, this, [this](bool checked) {
    if (!impl_) {
      return;
    }
    if (auto *controller = impl_->renderController_) {
      controller->setShowXRayOverlay(checked);
    }
    impl_->forEachActiveSecondaryController(
        [checked](CompositionRenderController *controller) {
          controller->setShowXRayOverlay(checked);
    });
    impl_->refreshViewportStateLabels();
  });
  shadeXRayAct->setShortcut(
      ArtifactCore::ShortcutBindings::instance().shortcut(
          ArtifactCore::ShortcutId::ViewToggleXRay));
  shadeXRayAct->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  shadeXRayAct->setToolTip(QStringLiteral("Toggle the X-Ray overlay for the selected layer (%1)")
                               .arg(ArtifactCore::ShortcutBindings::instance().shortcutText(
                                   ArtifactCore::ShortcutId::ViewToggleXRay)));
  QObject::connect(shadeIsolationAct, &QAction::toggled, this,
                   [this](bool checked) {
                     if (!impl_) {
                       return;
                     }
                     if (auto *controller = impl_->renderController_) {
                       controller->setShowIsolationOverlay(checked);
                     }
                     impl_->forEachActiveSecondaryController(
                         [checked](CompositionRenderController *controller) {
                           controller->setShowIsolationOverlay(checked);
                      });
                      impl_->refreshViewportStateLabels();
                    });
  shadeIsolationAct->setShortcut(
      ArtifactCore::ShortcutBindings::instance().shortcut(
          ArtifactCore::ShortcutId::ViewToggleIsolation));
  shadeIsolationAct->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  shadeIsolationAct->setToolTip(
      QStringLiteral("Toggle isolation mode to show only the selected layer (%1)")
          .arg(ArtifactCore::ShortcutBindings::instance().shortcutText(
              ArtifactCore::ShortcutId::ViewToggleIsolation)));
  QObject::connect(gridAct, &QAction::toggled, shadeGridAct, &QAction::setChecked);
  QObject::connect(guidesAct, &QAction::toggled, shadeGuidesAct,
                   &QAction::setChecked);
  QObject::connect(safeMarginsAct, &QAction::toggled, shadeSafeAreaAct,
                   &QAction::setChecked);
  QObject::connect(impl_->motionPathAction_, &QAction::toggled, shadeMotionPathAct,
                   &QAction::setChecked);
  QObject::connect(impl_->effectHitboxAction_, &QAction::toggled,
                   shadeEffectHitboxAct, &QAction::setChecked);
  QObject::connect(anchorCenterAct, &QAction::toggled, shadeAnchorCenterAct,
                   &QAction::setChecked);
  QObject::connect(cameraOverlayAct, &QAction::toggled, shadeCameraFrustumAct,
                   &QAction::setChecked);
  QObject::connect(densityHeatmapAct, &QAction::toggled, shadeDensityHeatmapAct,
                   &QAction::setChecked);
  QObject::connect(impl_->gizmoVisibleAction_, &QAction::toggled, shadeGizmoAct,
                   &QAction::setChecked);
  impl_->shadingButton_->setMenu(shadingMenu);

  // Initialize checked state
  if (impl_->renderController_) {
    switch (static_cast<CompositionBackgroundMode>(
        impl_->renderController_->compositionBackgroundMode())) {
    case CompositionBackgroundMode::Solid:
      solidBgAct->setChecked(true);
      break;
    case CompositionBackgroundMode::Checkerboard:
      checkerboardAct->setChecked(true);
      break;
    case CompositionBackgroundMode::MayaGradient:
      mayaBgAct->setChecked(true);
      break;
    }
    gridAct->setChecked(impl_->renderController_->isShowGrid());
    guidesAct->setChecked(impl_->renderController_->isShowGuides());
    safeMarginsAct->setChecked(impl_->renderController_->isShowSafeMargins());
    originAct->setChecked(impl_->renderController_->isShowOriginOverlay());
    anchorCenterAct->setChecked(impl_->renderController_->isShowAnchorCenterOverlay());
    cameraOverlayAct->setChecked(impl_->renderController_->isShowCameraFrustumOverlay());
    densityHeatmapAct->setChecked(
        impl_->renderController_->isShowDensityHeatmapOverlay());
    if (impl_->layerChromeAction_) {
      impl_->layerChromeAction_->setText(impl_->layerChromeButtonLabel());
      impl_->layerChromeAction_->setChecked(impl_->layerChromeVisible_);
    }
    layerChromeAct->setChecked(impl_->layerChromeVisible_);
    if (impl_->lockViewAction_) {
      impl_->lockViewAction_->setText(impl_->lockViewButtonLabel());
      impl_->lockViewAction_->setChecked(impl_->lockViewToSelection_);
    }
    lockViewAct->setChecked(impl_->lockViewToSelection_);
    showReferenceImageAct->setChecked(
        impl_->renderController_->isShowReferenceOverlay());
    showReferenceImageAct->setEnabled(
        impl_->renderController_->hasReferenceOverlayImage());
    clearReferenceImageAct->setEnabled(
        impl_->renderController_->hasReferenceOverlayImage());
    colorSamplerAct->setChecked(
        impl_->renderController_->isShowColorSamplerOverlay());
    autoColorPaletteAct->setChecked(
        impl_->renderController_->isShowAutoColorPaletteOverlay());
    autoColorPaletteAct->setEnabled(
        impl_->renderController_->hasReferenceOverlayImage());
    gpuBlendAct->setChecked(impl_->renderController_->isGpuBlendEnabled());
    impl_->motionPathAction_->setChecked(
        impl_->renderController_->isShowMotionPathOverlay());
    impl_->effectHitboxAction_->setChecked(
        impl_->renderController_->isShowEffectHitboxOverlay());
    onionEnableAct->setChecked(impl_->renderController_->isShowOnionSkin());
    for (QAction *action : onionFrameMenu->actions()) {
      action->setChecked(action->data().toInt() ==
                         impl_->renderController_->onionSkinFrameCount());
    }
    for (QAction *action : onionOpacityMenu->actions()) {
      action->setChecked(action->data().toInt() ==
                         impl_->renderController_->onionSkinOpacity());
    }
    const float checkerboardSize = impl_->renderController_->checkerboardSize();
    for (QAction *action : checkerboardSizeMenu->actions()) {
      const float size = action->data().toFloat();
      action->setChecked(std::abs(size - checkerboardSize) <= 0.5f);
    }
    const auto channelMode = impl_->renderController_->viewportChannelDisplayMode();
    impl_->viewportChannelDisplayMode_ = channelMode;
    channelColorAct->setChecked(channelMode == ViewportChannelDisplayMode::Color);
    channelAlphaAct->setChecked(channelMode == ViewportChannelDisplayMode::Alpha);
    channelColorAlphaAct->setChecked(
        channelMode == ViewportChannelDisplayMode::ColorAlpha);
    channelRedAct->setChecked(channelMode == ViewportChannelDisplayMode::Red);
    channelGreenAct->setChecked(channelMode == ViewportChannelDisplayMode::Green);
    channelBlueAct->setChecked(channelMode == ViewportChannelDisplayMode::Blue);
    channelDepthAct->setChecked(channelMode == ViewportChannelDisplayMode::Depth);
    channelEmissionAct->setChecked(channelMode == ViewportChannelDisplayMode::Emission);
    channelObjectIdAct->setChecked(channelMode == ViewportChannelDisplayMode::ObjectId);
    channelMaterialIdAct->setChecked(channelMode == ViewportChannelDisplayMode::MaterialId);
    channelAlbedoAct->setChecked(channelMode == ViewportChannelDisplayMode::Albedo);
    channelAlbedoRAct->setChecked(channelMode == ViewportChannelDisplayMode::AlbedoR);
    channelAlbedoGAct->setChecked(channelMode == ViewportChannelDisplayMode::AlbedoG);
    channelAlbedoBAct->setChecked(channelMode == ViewportChannelDisplayMode::AlbedoB);
    channelNormalAct->setChecked(channelMode == ViewportChannelDisplayMode::Normal);
    channelNormalXAct->setChecked(channelMode == ViewportChannelDisplayMode::NormalX);
    channelNormalYAct->setChecked(channelMode == ViewportChannelDisplayMode::NormalY);
    channelNormalZAct->setChecked(channelMode == ViewportChannelDisplayMode::NormalZ);
    channelVelocityAct->setChecked(channelMode == ViewportChannelDisplayMode::Velocity);
    channelVelocityXAct->setChecked(channelMode == ViewportChannelDisplayMode::VelocityX);
    channelVelocityYAct->setChecked(channelMode == ViewportChannelDisplayMode::VelocityY);
    impl_->refreshViewportStateLabels();
    shadeGridAct->setChecked(gridAct->isChecked());
    shadeGuidesAct->setChecked(guidesAct->isChecked());
    shadeSafeAreaAct->setChecked(safeMarginsAct->isChecked());
    shadeMotionPathAct->setChecked(impl_->motionPathAction_->isChecked());
    shadeEffectHitboxAct->setChecked(impl_->effectHitboxAction_->isChecked());
    shadeAnchorCenterAct->setChecked(anchorCenterAct->isChecked());
    shadeCameraFrustumAct->setChecked(cameraOverlayAct->isChecked());
    shadeDensityHeatmapAct->setChecked(densityHeatmapAct->isChecked());
    if (impl_->gizmoVisibleAction_) {
      impl_->gizmoVisibleAction_->setChecked(
          impl_->renderController_->isShowGizmoOverlay());
      shadeGizmoAct->setChecked(impl_->gizmoVisibleAction_->isChecked());
    }
    shadeXRayAct->setChecked(impl_->renderController_->isShowXRayOverlay());
    shadeIsolationAct->setChecked(
        impl_->renderController_->isShowIsolationOverlay());
    impl_->refreshViewportStateLabels();
  }

  bottomLayout->addWidget(impl_->resolutionCombo_);
  bottomLayout->addWidget(impl_->fastPreviewBtn_);
  bottomLayout->addWidget(impl_->hdrDisplayBtn_);
  bottomLayout->addWidget(impl_->shadingButton_);
  bottomLayout->addWidget(impl_->displayOptionsBtn_);
  bottomLayout->addStretch();

  // Assembly
  mainLayout->addWidget(impl_->topToolbar_);
  mainLayout->addWidget(impl_->viewportHost_, 1);
  impl_->topToolbar_->setAutoFillBackground(true);
  QPalette topPalette = impl_->topToolbar_->palette();
  topPalette.setColor(QPalette::Window, QColor(theme.secondaryBackgroundColor));
  topPalette.setColor(QPalette::Button, QColor(theme.secondaryBackgroundColor));
  topPalette.setColor(QPalette::WindowText, QColor(theme.textColor));
  impl_->topToolbar_->setPalette(topPalette);
  impl_->bottomBar_->setAutoFillBackground(true);
  QPalette bottomPalette = impl_->bottomBar_->palette();
  bottomPalette.setColor(QPalette::Window,
                         QColor(theme.secondaryBackgroundColor));
  bottomPalette.setColor(QPalette::WindowText, QColor(theme.textColor));
  impl_->bottomBar_->setPalette(bottomPalette);
  impl_->syncChromeSummary(this);
  impl_->applyViewportLayout();
  impl_->syncOverlayGeometry(this);
  QTimer::singleShot(0, this, [this]() {
    if (impl_) {
      impl_->syncOverlayGeometry(this);
      // Dock layouts settle after the editor constructor returns.  Re-run the
      // native viewport readiness pass at that point so the first swap chain
      // uses the actual center-pane dimensions rather than a transient size.
      impl_->forEachActiveViewport([](CompositionViewport *view, int) {
        if (view) {
          view->scheduleViewportReadinessCheck(
              QStringLiteral("editor-initial-layout"), 0);
        }
      });
    }
  });

  // Connections
  QObject::connect(impl_->resetAction_, &QAction::triggered, this,
                   &ArtifactCompositionEditor::resetView);
  QObject::connect(impl_->zoomInAction_, &QAction::triggered, this,
                   &ArtifactCompositionEditor::zoomIn);
  QObject::connect(impl_->zoomOutAction_, &QAction::triggered, this,
                   &ArtifactCompositionEditor::zoomOut);
  QObject::connect(impl_->zoomFitAction_, &QAction::triggered, this,
                   &ArtifactCompositionEditor::zoomFill);
  QObject::connect(impl_->zoom100Action_, &QAction::triggered, this,
                   &ArtifactCompositionEditor::zoom100);
  QObject::connect(impl_->editTextAction_, &QAction::triggered, this, [this]() {
    auto *selection = ArtifactLayerSelectionManager::instance();
    const auto layer =
        selection ? selection->currentLayer() : ArtifactAbstractLayerPtr{};
    if (!layer || !ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(layer)) {
      return;
    }
    auto *view = impl_ ? impl_->activeViewport() : nullptr;
    auto *controller = impl_ ? impl_->activeRenderController() : nullptr;
    if (view && controller && editTextLayerInline(view, layer, controller)) {
      controller->markRenderDirty();
    }
  });
  QObject::connect(impl_->compareAction_, &QAction::triggered, this, [this]() {
    openContentsViewerCompareSurfaceImpl();
  });
  QObject::connect(
      impl_->motionPathAction_, &QAction::toggled, this, [this](bool checked) {
        if (!impl_) {
          return;
        }
        if (auto *settings = ArtifactCore::ArtifactAppSettings::instance()) {
          settings->setCompositionShowMotionPathOverlay(checked);
        }
        if (auto *controller = impl_->renderController_) {
          controller->setShowMotionPathOverlay(checked);
        }
        impl_->forEachActiveSecondaryController(
            [checked](CompositionRenderController *controller) {
              controller->setShowMotionPathOverlay(checked);
            });
      });
  QObject::connect(
      impl_->effectHitboxAction_, &QAction::toggled, this,
      [this](bool checked) {
        if (!impl_) {
          return;
        }
        if (auto *controller = impl_->renderController_) {
          controller->setShowEffectHitboxOverlay(checked);
        }
        impl_->forEachActiveSecondaryController(
            [checked](CompositionRenderController *controller) {
              controller->setShowEffectHitboxOverlay(checked);
            });
      });
  auto *toggleMotionPathShortcut =
      new QShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_M), this);
  toggleMotionPathShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  QObject::connect(toggleMotionPathShortcut, &QShortcut::activated, this,
                   [this]() {
                     if (impl_->motionPathAction_) {
                       impl_->motionPathAction_->toggle();
                     }
                   });
  auto *toggleHitboxShortcut =
      new QShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_H), this);
  toggleHitboxShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  QObject::connect(toggleHitboxShortcut, &QShortcut::activated, this,
                   [this]() {
                     if (impl_->effectHitboxAction_) {
                       impl_->effectHitboxAction_->toggle();
                     }
                   });
  auto *focusSelectedLayerShortcut =
      new QShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_F), this);
  focusSelectedLayerShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  QObject::connect(focusSelectedLayerShortcut, &QShortcut::activated, this,
                   [this]() {
                     if (impl_) {
                       if (auto *controller = impl_->activeRenderController()) {
                         controller->focusSelectedLayer();
                       }
                     }
                   });
  auto *frameSelectedShortcut =
      new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F), this);
  frameSelectedShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  QObject::connect(frameSelectedShortcut, &QShortcut::activated, this,
                   [this]() {
                     if (impl_) {
                       if (auto *controller = impl_->activeRenderController()) {
                         controller->focusSelectedLayer();
                       }
                     }
                   });
  auto *frameAllShortcut =
      new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_H), this);
  frameAllShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  QObject::connect(frameAllShortcut, &QShortcut::activated, this, [this]() {
    if (impl_) {
      if (auto *controller = impl_->activeRenderController()) {
        controller->resetView();
      }
    }
  });
  auto *compareSurfaceShortcut =
      new QShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_V), this);
  compareSurfaceShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  QObject::connect(compareSurfaceShortcut, &QShortcut::activated, this,
                    []() { openContentsViewerCompareSurfaceImpl(); });
  auto *compareModeAShortcut =
      new QShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_A), this);
  compareModeAShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  QObject::connect(compareModeAShortcut, &QShortcut::activated, this,
                   [this]() {
                     if (impl_) {
                       if (auto *controller = impl_->activeRenderController()) {
                         controller->setCompareMode(
                           CompositionCompareMode::A);
                       }
                     }
                   });
  auto *compareModeBShortcut =
      new QShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_B), this);
  compareModeBShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  QObject::connect(compareModeBShortcut, &QShortcut::activated, this,
                   [this]() {
                     if (impl_) {
                       if (auto *controller = impl_->activeRenderController()) {
                         controller->setCompareMode(
                           CompositionCompareMode::B);
                       }
                     }
                   });
  auto *compareModeOffShortcut =
      new QShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_O), this);
  compareModeOffShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  QObject::connect(compareModeOffShortcut, &QShortcut::activated, this,
                   [this]() {
                     if (impl_) {
                       if (auto *controller = impl_->activeRenderController()) {
                         controller->setCompareMode(
                           CompositionCompareMode::Off);
                       }
                     }
                   });
  auto *compareReferenceShortcut =
      new QShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_P), this);
  compareReferenceShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  QObject::connect(compareReferenceShortcut, &QShortcut::activated, this,
                   [this]() {
                     if (impl_) {
                       auto *controller = impl_->activeRenderController();
                       if (!controller) {
                         return;
                       }
                       const bool pinned = !controller->isReferencePinned();
                       controller->setReferencePinned(pinned);
                       if (pinned) {
                        if (auto *playback = ArtifactPlaybackService::instance()) {
                          controller->setReferenceFrame(
                              static_cast<int>(
                                  playback->currentFrame().framePosition()));
                        }
                      }
                    }
                  });
  auto *compareDiffShortcut =
      new QShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_D), this);
  compareDiffShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  QObject::connect(compareDiffShortcut, &QShortcut::activated, this,
                   [this]() {
                     if (impl_) {
                       if (auto *controller = impl_->activeRenderController()) {
                         controller->setCompareMode(
                           CompositionCompareMode::Diff);
                       }
                     }
                   });
  auto *toggleViewportLayoutShortcut =
      new QShortcut(QKeySequence(Qt::Key_F9), this);
  toggleViewportLayoutShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  QObject::connect(toggleViewportLayoutShortcut, &QShortcut::activated, this,
                   [this, setViewportLayout]() {
                     if (!impl_) {
                       return;
                     }
                     setViewportLayout(impl_->nextViewportLayoutMode());
                   });
  auto *immersiveExitShortcut =
      new QShortcut(QKeySequence(Qt::Key_Escape), this);
  QObject::connect(
      immersiveExitShortcut, &QShortcut::activated, this, [this]() {
        if (impl_ && impl_->immersiveMode_ && impl_->immersiveAction_) {
          impl_->immersiveAction_->setChecked(false);
        }
      });

  // Resolution dropdown connection
  QObject::connect(impl_->resolutionCombo_,
                   QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                   [this](int index) {
                     auto preset = static_cast<PreviewQualityPreset>(
                         impl_->resolutionCombo_->itemData(index).toInt());
                     if (auto *svc = ArtifactProjectService::instance()) {
                       svc->setPreviewQualityPreset(preset);
                     }
                   });

  if (auto *service = ArtifactProjectService::instance()) {
    const auto syncResolutionCombo = [this](PreviewQualityPreset preset) {
      if (!impl_ || !impl_->resolutionCombo_) {
        return;
      }
      const int targetIndex =
          std::clamp(impl_->resolutionCombo_->findData(
                         QVariant::fromValue(static_cast<int>(preset))),
                     0, std::max(0, impl_->resolutionCombo_->count() - 1));
      QSignalBlocker blocker(impl_->resolutionCombo_);
      impl_->resolutionCombo_->setCurrentIndex(targetIndex);
    };
    const auto syncShadingQualityActions = [qualityFullAct, qualityHalfAct,
                                            qualityQuarterAct](
                                               PreviewQualityPreset preset) {
      const QSignalBlocker fullBlocker(qualityFullAct);
      const QSignalBlocker halfBlocker(qualityHalfAct);
      const QSignalBlocker quarterBlocker(qualityQuarterAct);
      qualityFullAct->setChecked(preset == PreviewQualityPreset::Final);
      qualityHalfAct->setChecked(preset == PreviewQualityPreset::Preview);
      qualityQuarterAct->setChecked(preset == PreviewQualityPreset::Draft);
    };
    syncResolutionCombo(service->previewQualityPreset());
    syncShadingQualityActions(service->previewQualityPreset());
    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<PreviewQualityPresetChangedEvent>(
            [this, syncResolutionCombo, syncShadingQualityActions](
                const PreviewQualityPresetChangedEvent &event) {
              const auto preset =
                  static_cast<PreviewQualityPreset>(event.preset);
              syncResolutionCombo(preset);
              syncShadingQualityActions(preset);
            }));
  }

  if (auto *app = ArtifactApplicationManager::instance()) {
    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<ToolChangedEvent>(
            [this](const ToolChangedEvent &event) {
              if (impl_) {
                impl_->forceFrontForPlanarEditingTool(event.toolType);
                impl_->queueToolLabelSync(this);
              }
            }));
    if (impl_) {
      impl_->queueToolLabelSync(this);
      impl_->queueSelectionSync(this);
    }
  }

  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<ProjectChangedEvent>(
          [this](const ProjectChangedEvent &) {
            if (!impl_ || !impl_->renderController_) {
              return;
            }
            const auto next = resolvePreferredComposition();
            const auto current = impl_->renderController_->composition();
            if (current && next && current->id() == next->id()) {
              impl_->queueSelectionSync(this);
              return;
            }
            setComposition(next);
            impl_->queueSelectionSync(this);
          }));

  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<CurrentCompositionChangedEvent>(
          [this](const CurrentCompositionChangedEvent &event) {
            if (!impl_ || !impl_->renderController_) {
              return;
            }
            if (event.compositionId.trimmed().isEmpty()) {
              setComposition(nullptr);
              return;
            }
            auto *service = ArtifactProjectService::instance();
            if (!service) {
              setComposition(nullptr);
              return;
            }
            auto result =
                service->findComposition(CompositionID(event.compositionId));
            if (result.success) {
              const auto next = result.ptr.lock();
              const auto current = impl_->renderController_->composition();
              if (!current || !next || current->id() != next->id()) {
                setComposition(next);
              }
              return;
            }
            setComposition(nullptr);
          }));

  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<SelectionChangedEvent>(
          [this](const SelectionChangedEvent &) {
            if (impl_) {
              impl_->queueSelectionSync(this);
            }
          }));

  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<LayerSelectionChangedEvent>(
          [this](const LayerSelectionChangedEvent &) {
            if (impl_) {
              impl_->queueSelectionSync(this);
              if (impl_->lockViewToSelection_) {
                if (auto *controller = impl_->activeRenderController()) {
                  controller->focusSelectedLayer();
                }
              }
            }
          }));

  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<PlaybackCompositionChangedEvent>(
          [this](const PlaybackCompositionChangedEvent &event) {
            if (event.compositionId.trimmed().isEmpty()) {
              if (impl_ && impl_->renderController_ &&
                  !impl_->renderController_->composition()) {
                return;
              }
              setComposition(nullptr);
              return;
            }
            if (auto *service = ArtifactProjectService::instance()) {
              auto result =
                  service->findComposition(CompositionID(event.compositionId));
              if (result.success) {
                setComposition(result.ptr.lock());
                return;
              }
            }
            setComposition(nullptr);
          }));

  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<CompositionViewCommandRequestedEvent>(
          [this](const CompositionViewCommandRequestedEvent &event) {
            if (!impl_) {
              return;
            }
            switch (event.kind) {
            case CompositionViewCommandKind::Reset:
              resetView();
              break;
            case CompositionViewCommandKind::ZoomIn:
              zoomIn();
              break;
            case CompositionViewCommandKind::ZoomOut:
              zoomOut();
              break;
            case CompositionViewCommandKind::ZoomFit:
              zoomFit();
              break;
            case CompositionViewCommandKind::Zoom100:
              zoom100();
              break;
            case CompositionViewCommandKind::SetGridVisible:
              if (impl_->renderController_) {
                impl_->renderController_->setShowGrid(event.visible);
                impl_->forEachActiveSecondaryController(
                    [visible = event.visible](CompositionRenderController *controller) {
                      controller->setShowGrid(visible);
                    });
              }
              if (auto *settings = ArtifactCore::ArtifactAppSettings::instance()) {
                settings->setCompositionShowGrid(event.visible);
              }
              break;
            case CompositionViewCommandKind::SetGuidesVisible:
              if (impl_->renderController_) {
                impl_->renderController_->setShowGuides(event.visible);
                impl_->forEachActiveSecondaryController(
                    [visible = event.visible](CompositionRenderController *controller) {
                      controller->setShowGuides(visible);
                    });
              }
              if (auto *settings = ArtifactCore::ArtifactAppSettings::instance()) {
                settings->setCompositionShowGuides(event.visible);
              }
              break;
            }
          }));

  QTimer::singleShot(0, this, [this]() {
    if (!impl_) {
      return;
    }

    // --- Profiler overlay (Ctrl+Shift+P to toggle) ---
    impl_->profilerOverlay_ = new ProfilerOverlayWidget(this);
    impl_->profilerOverlay_->hide();

    auto *profilerShortcut =
        new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P), this);
    QObject::connect(profilerShortcut, &QShortcut::activated, this, [this]() {
      if (!impl_ || !impl_->profilerOverlay_)
        return;
      const bool willShow = !impl_->profilerOverlay_->isVisible();
      ArtifactCore::Profiler::instance().setEnabled(willShow);
      impl_->profilerOverlay_->setVisible(willShow);
      if (willShow) {
        impl_->syncOverlayGeometry(this);
      }
    });

    // Ctrl+Shift+C: copy diagnostic report to clipboard
    auto *profilerCopyShortcut =
        new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C), this);
    QObject::connect(profilerCopyShortcut, &QShortcut::activated, this,
                     [this]() {
                       if (!ArtifactCore::Profiler::instance().isEnabled())
                         return;
                       const auto report =
                           ArtifactCore::Profiler::instance()
                               .generateDiagnosticReport(60);
                       QGuiApplication::clipboard()->setText(
                           QString::fromStdString(
                               ArtifactCore::toStdString(report)));
                     });

    // --- Profiler panel (Ctrl+Shift+D to toggle) ---
    impl_->profilerPanel_ = new ProfilerPanelWidget(nullptr);
    impl_->profilerPanel_->hide();

    auto *profilerPanelShortcut =
        new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D), this);
    QObject::connect(
        profilerPanelShortcut, &QShortcut::activated, this, [this]() {
          if (!impl_ || !impl_->profilerPanel_)
            return;
          const bool willShow = !impl_->profilerPanel_->isVisible();
          ArtifactCore::Profiler::instance().setEnabled(
              willShow || impl_->profilerOverlay_->isVisible());
          impl_->profilerPanel_->setVisible(willShow);
          if (willShow) {
            // Position panel to the right of the composite editor
            const QRect geom = frameGeometry();
            impl_->profilerPanel_->move(geom.right() + 8, geom.top());
            impl_->profilerPanel_->raise();
          }
        });

    // --- EventBus Debugger (Ctrl+Shift+E to toggle) ---
    impl_->eventBusDebugger_ = new EventBusDebuggerWidget(nullptr);
    impl_->eventBusDebugger_->hide();

    auto *eventBusDebuggerShortcut =
        new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E), this);
    QObject::connect(
        eventBusDebuggerShortcut, &QShortcut::activated, this, [this]() {
          if (!impl_ || !impl_->eventBusDebugger_)
            return;
          const bool willShow = !impl_->eventBusDebugger_->isVisible();
          impl_->eventBusDebugger_->setVisible(willShow);
          if (willShow) {
            const QRect geom = frameGeometry();
            impl_->eventBusDebugger_->move(geom.right() + 8, geom.top() + 60);
            impl_->eventBusDebugger_->raise();
          }
        });

    auto *placeWorkCursorShortcut =
        new QShortcut(QKeySequence(ArtifactCore::ShortcutBindings::instance()
                                       .shortcut(ArtifactCore::ShortcutId::WorkCursorPlace)),
                      this);
    QObject::connect(placeWorkCursorShortcut, &QShortcut::activated, this,
                     [this]() {
                       if (!impl_ || !impl_->activeRenderController())
                         return;
                       auto *controller = impl_->activeRenderController();
                       const auto view = impl_->activeViewport();
                       if (!view) {
                         return;
                       }
                       controller->placeWorkCursorAtViewportPos(
                           QPointF(view->width() * 0.5, view->height() * 0.5));
                       controller->setWorkCursorLabel(
                           QStringLiteral("Placed in %1")
                               .arg(impl_->activePaneViewLabel()));
                       controller->setInfoOverlayText(
                           QStringLiteral("Work Cursor"),
                           QStringLiteral("Placed at the active viewport center"));
                     });

    auto *centerWorkCursorShortcut =
        new QShortcut(QKeySequence(ArtifactCore::ShortcutBindings::instance()
                                       .shortcut(ArtifactCore::ShortcutId::WorkCursorCenter)),
                      this);
    QObject::connect(centerWorkCursorShortcut, &QShortcut::activated, this,
                     [this]() {
                       if (!impl_ || !impl_->activeRenderController())
                         return;
                       if (auto *controller = impl_->activeRenderController()) {
                         const auto comp = controller->composition();
                         if (!comp) {
                           return;
                         }
                         const QSize size = comp->settings().compositionSize();
                         controller->setWorkCursorCanvasPosition(QPointF(
                             size.width() * 0.5, size.height() * 0.5));
                         controller->setWorkCursorLabel(
                             QStringLiteral("Centered in %1")
                                 .arg(impl_->activePaneViewLabel()));
                         controller->setInfoOverlayText(
                             QStringLiteral("Work Cursor"),
                             QStringLiteral("Centered in the composition"));
                       }
                     });

    auto *clearWorkCursorShortcut =
        new QShortcut(QKeySequence(ArtifactCore::ShortcutBindings::instance()
                                       .shortcut(ArtifactCore::ShortcutId::WorkCursorClear)),
                      this);
    QObject::connect(clearWorkCursorShortcut, &QShortcut::activated, this,
                     [this]() {
                       if (!impl_ || !impl_->activeRenderController())
                         return;
                       auto *controller = impl_->activeRenderController();
                       controller->clearWorkCursor();
                       controller->clearInfoOverlayText();
                     });

    auto *viewUndoShortcut =
        new QShortcut(QKeySequence(ArtifactCore::ShortcutBindings::instance()
                                       .shortcut(ArtifactCore::ShortcutId::ViewUndo)),
                      this);
    QObject::connect(viewUndoShortcut, &QShortcut::activated, this, [this]() {
      if (auto *controller = impl_ ? impl_->activeRenderController() : nullptr) {
        controller->undoView();
        controller->setInfoOverlayText(QStringLiteral("View Undo"),
                                       impl_->activePaneViewLabel());
      }
    });

    auto *viewRedoShortcut =
        new QShortcut(QKeySequence(ArtifactCore::ShortcutBindings::instance()
                                       .shortcut(ArtifactCore::ShortcutId::ViewRedo)),
                      this);
    QObject::connect(viewRedoShortcut, &QShortcut::activated, this, [this]() {
      if (auto *controller = impl_ ? impl_->activeRenderController() : nullptr) {
        controller->redoView();
        controller->setInfoOverlayText(QStringLiteral("View Redo"),
                                       impl_->activePaneViewLabel());
      }
    });
  });

  qInfo() << "[CompositionEditor][Ctor] total ms=" << ctorTimer.elapsed();
}

void ArtifactCompositionEditor::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  if (impl_) {
    impl_->syncOverlayGeometry(this);
  }

  // Debug: event filter to trace mouse events
  installEventFilter(this);
}

ArtifactCompositionEditor::~ArtifactCompositionEditor() {
  // QToolBar::addWidget() creates QWidgetAction wrappers.  Tear those wrappers
  // down while Impl and every toolbar-owned widget are still valid instead of
  // leaving their release order to QWidget's generic child cleanup.
  if (impl_) {
    delete impl_->toolHud_;
    impl_->toolHud_ = nullptr;
    delete impl_->zoomHud_;
    impl_->zoomHud_ = nullptr;
    delete impl_->topToolbar_;
    impl_->topToolbar_ = nullptr;
  }
  delete impl_;
  impl_ = nullptr;
}

bool ArtifactCompositionEditor::event(QEvent *event) {
  if (event && impl_ && event->type() == QEvent::Show) {
    // The first QMainWindow/QADS layout pass can complete after child show
    // events. Re-check on the next turn with the final center-pane geometry.
    QTimer::singleShot(0, this, [this]() {
      if (!impl_) {
        return;
      }
      impl_->forEachActiveViewport([](CompositionViewport *view, int) {
        if (view) {
          view->scheduleViewportReadinessCheck(
              QStringLiteral("editor-show-layout"), 0);
        }
      });
      impl_->syncOverlayGeometry(this);
    });
  }
  if (event && impl_ &&
      (event->type() == QEvent::Show || event->type() == QEvent::FocusIn ||
       event->type() == QEvent::WindowActivate)) {
    if (auto *app = Artifact::ApplicationService::instance()) {
      if (auto *toolService = app->toolService()) {
        const DisplayMode serviceMode = toolService->displayMode();
        if (serviceMode != impl_->lastToolServiceDisplayMode_) {
          impl_->lastToolServiceDisplayMode_ = serviceMode;
          if (serviceMode == DisplayMode::Color ||
              serviceMode == DisplayMode::Alpha) {
            impl_->setViewportChannelDisplayMode(
                this, serviceMode == DisplayMode::Alpha
                          ? ViewportChannelDisplayMode::Alpha
                          : ViewportChannelDisplayMode::Color);
          }
        }
      }
    }
  }
  // internal event を正規経路にして、Qt signal/slot 直結へ戻しにくくする。
  if (event && impl_ &&
      event->type() == CompositionEditorDeferredEvent::eventType()) {
    auto *deferred = static_cast<CompositionEditorDeferredEvent *>(event);
    switch (deferred->kind) {
    case CompositionEditorDeferredEvent::Kind::SelectionSync:
      impl_->selectionSyncQueued_ = false;
      impl_->syncSelectionState(this);
      impl_->syncChromeSummary(this);
      return true;
    case CompositionEditorDeferredEvent::Kind::ToolLabelSync:
      impl_->toolLabelSyncQueued_ = false;
      impl_->syncToolLabel(this);
      impl_->syncChromeSummary(this);
      return true;
    }
  }
  return QWidget::event(event);
}

QSize ArtifactCompositionEditor::sizeHint() const { return QSize(1024, 720); }

void ArtifactCompositionEditor::setComposition(
    ArtifactCompositionPtr composition) {
  const auto previousComposition =
      impl_->renderController_ ? impl_->renderController_->composition()
                               : ArtifactCompositionPtr{};
  const bool sameCompositionPointer = previousComposition == composition;
  const bool sameCompositionId =
      previousComposition && composition &&
      previousComposition->id() == composition->id();

  if (impl_->renderController_) {
    impl_->renderController_->setComposition(composition);
    if (composition) {
      impl_->renderController_->start();
    }
  }
  impl_->forEachSecondaryController(
      [&composition](CompositionRenderController *controller) {
        controller->setComposition(composition);
        controller->stop();
      });
  if (composition) {
    impl_->forEachActiveSecondaryController(
        [](CompositionRenderController *controller) { controller->start(); });
  }
  if (auto *playback = ArtifactPlaybackService::instance()) {
    playback->setCurrentComposition(composition);
  }
  ArtifactAudioScrubController::instance().setComposition(composition);
  if (impl_->compositionView_ && !sameCompositionPointer && !sameCompositionId) {
    impl_->forEachActiveViewport([](CompositionViewport *view, int) {
      view->requestInitialFit();
    });
  }
  if (impl_) {
    impl_->queueSelectionSync(this);
    impl_->syncChromeSummary(this);
    impl_->syncOverlayGeometry(this);
  }
}

void ArtifactCompositionEditor::setClearColor(const FloatColor &color) {
  if (impl_->renderController_) {
    impl_->renderController_->setClearColor(color);
  }
  impl_->forEachSecondaryController(
      [&color](CompositionRenderController *controller) {
        controller->setClearColor(color);
      });
}

void ArtifactCompositionEditor::refreshEnabledState() {
  if (!impl_) {
    return;
  }

  const auto composition = impl_->renderController_
                                ? impl_->renderController_->composition()
                                : ArtifactCompositionPtr();
  const bool hasComposition = static_cast<bool>(composition);
  const bool hasLayers = hasComposition && composition->layerCount() > 0;
  const bool enabled = hasComposition && hasLayers;

  if (impl_->layerChromeAction_) {
    impl_->layerChromeAction_->setText(impl_->layerChromeButtonLabel());
    impl_->layerChromeAction_->setChecked(impl_->layerChromeVisible_);
    impl_->layerChromeAction_->setEnabled(enabled);
  }
  if (impl_->lockViewAction_) {
    impl_->lockViewAction_->setText(impl_->lockViewButtonLabel());
    impl_->lockViewAction_->setChecked(impl_->lockViewToSelection_);
    impl_->lockViewAction_->setEnabled(enabled);
  }
}

CompositionRenderController* ArtifactCompositionEditor::renderController() const {
  return impl_ ? impl_->renderController_ : nullptr;
}

void ArtifactCompositionEditor::play() {
  if (auto *playback = ArtifactPlaybackService::instance()) {
    playback->play();
  }
  if (impl_->renderController_) {
    impl_->renderController_->start();
  }
  impl_->forEachActiveSecondaryController(
      [](CompositionRenderController *controller) { controller->start(); });
}

void ArtifactCompositionEditor::pause() {
  if (auto *playback = ArtifactPlaybackService::instance()) {
    playback->pause();
  }
}

void ArtifactCompositionEditor::togglePlayPause() {
  if (auto *playback = ArtifactPlaybackService::instance()) {
    if (playback->isPlaying()) {
      pause();
    } else {
      play();
    }
  }
}

void ArtifactCompositionEditor::stop() {
  if (auto *playback = ArtifactPlaybackService::instance()) {
    playback->stop();
  }
  if (impl_->renderController_) {
    impl_->renderController_->stop();
  }
  impl_->forEachSecondaryController(
      [](CompositionRenderController *controller) { controller->stop(); });
}

void ArtifactCompositionEditor::resetView() {
  if (auto *controller = impl_ ? impl_->activeRenderController() : nullptr) {
    controller->resetView();
  }
}

void ArtifactCompositionEditor::zoomIn() {
  auto *controller = impl_ ? impl_->activeRenderController() : nullptr;
  auto *view = impl_ ? impl_->activeViewport() : nullptr;
  if (controller && view) {
    controller->zoomInAt(
        QPointF(view->width() * 0.5, view->height() * 0.5));
  }
}

void ArtifactCompositionEditor::zoomOut() {
  auto *controller = impl_ ? impl_->activeRenderController() : nullptr;
  auto *view = impl_ ? impl_->activeViewport() : nullptr;
  if (controller && view) {
    controller->zoomOutAt(
        QPointF(view->width() * 0.5, view->height() * 0.5));
  }
}

void ArtifactCompositionEditor::zoomFit() {
  if (auto *controller = impl_ ? impl_->activeRenderController() : nullptr) {
    controller->zoomFit();
  }
}

void ArtifactCompositionEditor::zoomFill() {
  if (auto *controller = impl_ ? impl_->activeRenderController() : nullptr) {
    controller->zoomFill();
  }
}

void ArtifactCompositionEditor::zoom100() {
  if (auto *controller = impl_ ? impl_->activeRenderController() : nullptr) {
    controller->zoom100();
  }
}

bool ArtifactCompositionEditor::handleImportPlacementKeyPress(QKeyEvent *event) {
  return impl_ ? impl_->handleImportPlacementKeyPress(this, event) : false;
}

void ArtifactCompositionEditor::toggleViewportToolboxes() {
  if (impl_) {
    impl_->toggleViewportToolboxes(this);
  }
}

} // namespace Artifact
