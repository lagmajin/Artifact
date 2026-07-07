module;
#include <QAction>
#include <QActionGroup>
#include <QClipboard>
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
#include <QJsonObject>
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
#include <QImageReader>
#include <QKeySequence>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
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
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStringList>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTransform>
#include <QVBoxLayout>
#include <QLabel>
#include <QVector>
#include <QVector3D>
#include <QWheelEvent>
#include <QFileDialog>
#include <QApplication>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QMainWindow>
#include <QWidgetAction>
#include <QtSVG/QSvgRenderer>
#include <algorithm>
#include <cmath>
#include <array>
#include <atomic>
#include <deque>
#include <functional>
#include <thread>
#include <utility>
#include <wobjectimpl.h>
#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#endif

module Artifact.Widgets.CompositionEditor;
import std;

import Artifact.Widgets.CompositionRenderController;
import Artifact.Contents.Viewer;
import Artifact.Widgets.TransformGizmo;
import Artifact.Widgets.Gizmo3D;
import Artifact.Widgets.PieMenu;
import UI.ShortcutBindings;
import UI.View.Orientation.Navigator;
import Math.Interpolate;
import Color.Float;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Shape;
import Artifact.Layer.Text;
import Artifact.Layer.Svg;
import Artifact.Layer.Image;
import Artifact.Layers.SolidImage;
import Artifact.Application.Manager;
import Artifact.Layers.Selection.Manager;
import Artifact.Service.ActiveContext;
import Artifact.Service.Project;
import Artifact.Service.Playback;

import Artifact.Audio.ScrubController;
import Artifact.Application.ProjectBundleIpc;
import Time.Rational;
import Artifact.Layer.Video;
import Artifact.Layer.Clone;
import Artifact.Layer.Camera;
import Artifact.Tool.Manager;
import FloatColorPickerDialog;
import Artifact.Widgets.CreateCameraLayerDialog;
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
import Codec.Thumbnail.FFmpeg;
import UI.ShortcutBindings;
import Undo.UndoManager;

namespace Artifact {

W_OBJECT_IMPL(ArtifactCompositionEditor)
Q_LOGGING_CATEGORY(compositionViewLog, "artifact.compositionview");

namespace {
QDockWidget* findDockByTitle(QMainWindow* window, const QString& title)
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

void activateDock(QMainWindow* window, const QString& title)
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
    if (auto *mainWindow = qobject_cast<QMainWindow *>(widget)) {
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

  if (const auto video = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
    return video->currentFrameToQImage();
  }
  if (const auto text = std::dynamic_pointer_cast<ArtifactTextLayer>(layer)) {
    return text->toQImage();
  }
  if (const auto image = std::dynamic_pointer_cast<ArtifactImageLayer>(layer)) {
    return image->toQImage();
  }
  if (const auto solidImage =
          std::dynamic_pointer_cast<ArtifactSolidImageLayer>(layer)) {
    return solidImage->toQImage();
  }
  if (const auto svg = std::dynamic_pointer_cast<ArtifactSvgLayer>(layer)) {
    return svg->toQImage();
  }
  if (const auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer)) {
    return shape->toQImage();
  }
  if (const auto clone = std::dynamic_pointer_cast<ArtifactCloneLayer>(layer)) {
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
  if (controller) {
    const QImage frame = controller->captureCurrentFrameImage();
    if (!frame.isNull()) {
      return frame;
    }
  }

  if (fallbackWidget) {
    return fallbackWidget->grab().toImage();
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

QString shapeSelectionDetail(const std::shared_ptr<ArtifactShapeLayer> &shape) {
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

  detail += QStringLiteral(" - vertex/segment edit ready");
  return detail;
}

ArtifactCompositionPtr resolvePreferredComposition() {
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

  if (auto *service = ArtifactProjectService::instance()) {
    return service->currentComposition().lock();
  }

  return {};
}

class TextOverlayFilter : public QObject {
public:
  TextOverlayFilter(QPlainTextEdit *editor,
                    std::shared_ptr<ArtifactTextLayer> layer,
                    CompositionRenderController *ctrl)
      : QObject(editor), editor_(editor), layer_(layer), ctrl_(ctrl) {}

  bool eventFilter(QObject *obj, QEvent *event) override {
    if (event->type() == QEvent::KeyPress) {
      auto *ke = static_cast<QKeyEvent *>(event);
      if (isCancelKey(ke)) {
        cancel();
        return true;
      }
      if (isCommitKey(ke)) {
        commit();
        return true;
      }
    } else if (event->type() == QEvent::FocusOut) {
      commit();
      return false;
    }
    return QObject::eventFilter(obj, event);
  }

private:
  void commit() {
    if (!editor_)
      return;
    if (layer_->text().toQString() != editor_->toPlainText()) {
      layer_->setText(
          ArtifactCore::UniString::fromQString(editor_->toPlainText()));
      if (auto *comp = static_cast<ArtifactAbstractComposition *>(
              layer_->composition())) {
        ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
            LayerChangedEvent{comp->id().toString(), layer_->id().toString(),
                              LayerChangedEvent::ChangeType::Modified});
      }
      if (ctrl_)
        ctrl_->markRenderDirty();
    }
    editor_->hide();
    editor_->deleteLater();
    editor_ = nullptr;
  }

  void cancel() {
    if (!editor_) {
      return;
    }
    editor_->hide();
    editor_->deleteLater();
    editor_ = nullptr;
  }

  bool isCommitKey(QKeyEvent *ke) const {
    return ke && ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) &&
                  (ke->modifiers() & Qt::ControlModifier));
  }

  bool isCancelKey(QKeyEvent *ke) const {
    return ke && ke->key() == Qt::Key_Escape;
  }

  void finishInlineEdit(bool commitChanges) {
    if (commitChanges) {
      commit();
    } else {
      cancel();
    }
  }

  QPlainTextEdit *editor_;
  std::shared_ptr<ArtifactTextLayer> layer_;
  CompositionRenderController *ctrl_;
};

class ArtifactTextEditorDialog final : public QDialog {
public:
  ArtifactTextEditorDialog(const ArtifactAbstractLayerPtr &layer,
                           CompositionRenderController *controller,
                           QWidget *parent = nullptr)
      : QDialog(parent), layer_(layer), controller_(controller) {
    setWindowTitle(QStringLiteral("Text Editor"));
    setAttribute(Qt::WA_DeleteOnClose);
    setModal(false);

    const auto textLayer = std::dynamic_pointer_cast<ArtifactTextLayer>(layer_);
    const auto theme = ArtifactCore::currentDCCTheme();

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    auto *header = new QLabel(this);
    header->setText(textLayer ? QStringLiteral("Text Layer Editor") : QStringLiteral("Text Editor"));
    header->setFont(font());
    root->addWidget(header);

    auto *summary = new QLabel(this);
    summary->setWordWrap(true);
    summary->setText(editorSummaryText(textLayer));
    root->addWidget(summary);

    auto *preview = new QFrame(this);
    preview->setFrameShape(QFrame::StyledPanel);
    preview->setFrameShadow(QFrame::Plain);
    preview->setMinimumHeight(180);
    preview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    preview->setAutoFillBackground(true);
    QPalette previewPalette = preview->palette();
    previewPalette.setColor(QPalette::Window,
                            QColor(theme.secondaryBackgroundColor));
    previewPalette.setColor(QPalette::Base, QColor(theme.secondaryBackgroundColor));
    previewPalette.setColor(QPalette::Text, QColor(theme.textColor));
    preview->setPalette(previewPalette);
    preview->installEventFilter(this);
    preview_ = preview;
    root->addWidget(preview, 1);

    editor_ = new QPlainTextEdit(this);
    editor_->setPlainText(textLayer ? textLayer->text().toQString() : QString());
    editor_->setPlaceholderText(QStringLiteral("Enter text..."));
    editor_->selectAll();
    editor_->setMinimumHeight(160);
    editor_->setTabChangesFocus(false);

    QFont editorFont = editor_->font();
    if (textLayer) {
      editorFont.setFamily(textLayer->fontFamily().toQString());
      editorFont.setPointSizeF(std::max(11.0f, textLayer->fontSize()));
    } else {
      editorFont.setPointSizeF(std::max(11.0, editorFont.pointSizeF()));
    }
    editor_->setFont(editorFont);

    QPalette editorPalette = editor_->palette();
    editorPalette.setColor(QPalette::Base, QColor(theme.backgroundColor));
    editorPalette.setColor(QPalette::Text, QColor(theme.textColor));
    editorPalette.setColor(QPalette::Window, QColor(theme.secondaryBackgroundColor));
    editor_->setPalette(editorPalette);

    editor_->installEventFilter(this);
    root->addWidget(editor_);

    setMinimumSize(680, 520);
    resize(900, 680);
  }

protected:
  bool eventFilter(QObject *obj, QEvent *event) override {
    if (obj == editor_) {
      if (event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Escape) {
          reject();
          return true;
        }
        if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) &&
            (ke->modifiers() & Qt::ControlModifier)) {
          accept();
          return true;
        }
      } else if (event->type() == QEvent::FocusOut) {
        accept();
        return false;
      }
    } else if (obj == preview_ && event->type() == QEvent::Paint) {
      paintPreview(static_cast<QWidget *>(obj));
      return true;
    }
    return QDialog::eventFilter(obj, event);
  }

  void accept() override {
    commit();
    QDialog::accept();
  }

  void reject() override {
    QDialog::reject();
  }

private:
  static QString editorSummaryText(const std::shared_ptr<ArtifactTextLayer> &textLayer) {
    if (!textLayer) {
      return QStringLiteral("No text layer selected.");
    }
    const QRectF bbox = textLayer->transformedBoundingBox();
    return QStringLiteral("%1 | layout=%2 | box=%3x%4 | mode=%5")
        .arg(textLayer->fontFamily().toQString())
        .arg(textLayer->layoutMode() == TextLayoutMode::Point
                 ? QStringLiteral("Point")
                 : textLayer->layoutMode() == TextLayoutMode::Box
                       ? QStringLiteral("Box")
                       : QStringLiteral("Path"))
        .arg(bbox.width(), 0, 'f', 1)
        .arg(bbox.height(), 0, 'f', 1)
        .arg(textLayer->writingMode() == TextWritingMode::Vertical
                 ? QStringLiteral("Vertical")
                 : QStringLiteral("Horizontal"));
  }

  void paintPreview(QWidget *widget) {
    if (!widget) {
      return;
    }
    QPainter painter(widget);
    painter.fillRect(widget->rect(), widget->palette().window());
    painter.setRenderHint(QPainter::Antialiasing, true);

    const auto textLayer = std::dynamic_pointer_cast<ArtifactTextLayer>(layer_);
    const QRectF inner = widget->rect().adjusted(20, 20, -20, -20);
    painter.setPen(QPen(QColor(120, 160, 220, 220), 1.5));
    painter.drawRoundedRect(inner, 8, 8);

    painter.setPen(QPen(QColor(255, 255, 255, 180), 1.0, Qt::DashLine));
    painter.drawLine(inner.left(), inner.center().y(), inner.right(), inner.center().y());

    painter.setPen(QColor(240, 240, 245));
    painter.setFont(font());
    const QString title = textLayer ? QStringLiteral("Diligent text surface shell")
                                    : QStringLiteral("Text editor shell");
    painter.drawText(inner.adjusted(14, 10, -14, -10),
                     Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, title);

    if (textLayer) {
      const QRectF bbox = textLayer->transformedBoundingBox();
      const QString detail = QStringLiteral("bbox %1 x %2  |  text length %3")
                                 .arg(bbox.width(), 0, 'f', 1)
                                 .arg(bbox.height(), 0, 'f', 1)
                                 .arg(textLayer->text().toQString().size());
      painter.setPen(QColor(180, 200, 220));
      painter.drawText(inner.adjusted(14, 44, -14, -10),
                       Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, detail);
    }
  }

  void commit() {
    if (!editor_) {
      return;
    }

    const auto textLayer = std::dynamic_pointer_cast<ArtifactTextLayer>(layer_);
    if (!textLayer) {
      return;
    }

    const QString nextText = editor_->toPlainText();
    if (textLayer->text().toQString() != nextText) {
      textLayer->setText(ArtifactCore::UniString::fromQString(nextText));
      if (auto *comp = static_cast<ArtifactAbstractComposition *>(textLayer->composition())) {
        ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
            LayerChangedEvent{comp->id().toString(), textLayer->id().toString(),
                              LayerChangedEvent::ChangeType::Modified});
      }
      if (controller_) {
        controller_->markRenderDirty();
      }
    }
  }

  ArtifactAbstractLayerPtr layer_;
  CompositionRenderController *controller_ = nullptr;
  QPlainTextEdit *editor_ = nullptr;
  QWidget *preview_ = nullptr;
};

bool editTextLayerInline(QWidget *parent, const ArtifactAbstractLayerPtr &layer,
                         CompositionRenderController *controller) {
  const auto textLayer = std::dynamic_pointer_cast<ArtifactTextLayer>(layer);
  if (!textLayer || !parent) {
    return false;
  }

  QWidget *host = parent->window() ? parent->window() : parent;
  static QPointer<ArtifactTextEditorDialog> activeDialog;
  if (activeDialog) {
    activeDialog->raise();
    activeDialog->activateWindow();
    return true;
  }
  auto *dialog = new ArtifactTextEditorDialog(layer, controller, host);
  activeDialog = dialog;
  const QPoint center = host->rect().center();
  const QPoint globalCenter = host->mapToGlobal(center);
  dialog->move(globalCenter.x() - dialog->width() / 2,
               globalCenter.y() - dialog->height() / 2);
  dialog->show();
  dialog->raise();
  dialog->activateWindow();
  return true;
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

class EmptyCompositionOverlayWidget final : public QWidget {
public:
  explicit EmptyCompositionOverlayWidget(
      QWidget *parent, std::function<void()> createRequested)
      : QWidget(parent), createRequested_(std::move(createRequested)) {
    setAutoFillBackground(false);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::NoFocus);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(24, 24, 24, 24);
    rootLayout->setSpacing(0);
    rootLayout->addStretch(1);

    auto *card = new QFrame(this);
    card->setFrameShape(QFrame::StyledPanel);
    card->setFrameShadow(QFrame::Plain);
    card->setAutoFillBackground(true);
    card->setMinimumWidth(0);
    card->setMaximumWidth(640);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    QPalette cardPalette = card->palette();
    cardPalette.setColor(QPalette::Window, QColor(18, 20, 25, 230));
    cardPalette.setColor(QPalette::WindowText, QColor(248, 248, 248));
    cardPalette.setColor(QPalette::Base, QColor(18, 20, 25, 230));
    cardPalette.setColor(QPalette::Text, QColor(248, 248, 248));
    card->setPalette(cardPalette);

    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(32, 28, 32, 28);
    cardLayout->setSpacing(14);

    titleLabel_ = new QLabel(QStringLiteral("まだコンポジションがありません"), card);
    QFont titleFont = titleLabel_->font();
    titleFont.setPointSizeF(std::max(14.0, titleFont.pointSizeF() + 2.0));
    titleFont.setBold(true);
    titleLabel_->setFont(titleFont);
    titleLabel_->setAlignment(Qt::AlignCenter);
    titleLabel_->setMinimumWidth(0);
    titleLabel_->setWordWrap(true);

    bodyLabel_ = new QLabel(
        QStringLiteral("新規コンポジションを作成して、編集を始めましょう。"),
        card);
    bodyLabel_->setAlignment(Qt::AlignCenter);
    bodyLabel_->setMinimumWidth(0);
    bodyLabel_->setWordWrap(true);

    helperLabel_ = new QLabel(
        QStringLiteral("ボタンを押すと、コンポジション設定ダイアログを開きます。"),
        card);
    helperLabel_->setAlignment(Qt::AlignCenter);
    helperLabel_->setMinimumWidth(0);
    helperLabel_->setWordWrap(true);

    createButton_ = new QPushButton(QStringLiteral("新規コンポジション"), card);
    createButton_->setMinimumHeight(46);
    createButton_->setMinimumWidth(0);
    createButton_->setMaximumWidth(240);
    QFont buttonFont = createButton_->font();
    buttonFont.setPointSizeF(std::max(12.0, buttonFont.pointSizeF() + 1.0));
    buttonFont.setBold(true);
    createButton_->setFont(buttonFont);
    createButton_->setCursor(Qt::PointingHandCursor);
    createButton_->setDefault(true);

    cardLayout->addWidget(titleLabel_);
    cardLayout->addWidget(bodyLabel_);
    cardLayout->addWidget(helperLabel_);
    cardLayout->addSpacing(8);
    cardLayout->addWidget(createButton_, 0, Qt::AlignHCenter);

    rootLayout->addWidget(card, 0, Qt::AlignHCenter);
    rootLayout->addStretch(1);

    QObject::connect(createButton_, &QPushButton::clicked, this,
                     [this]() {
                       if (createRequested_) {
                         createRequested_();
                       }
                     });
  }

  void setCompositionAvailable(bool hasComposition) {
    if (!titleLabel_ || !bodyLabel_ || !helperLabel_ || !createButton_) {
      return;
    }
    if (hasComposition) {
      titleLabel_->setText(QStringLiteral("レイヤーがありません"));
      bodyLabel_->setText(QStringLiteral("平面やテキストなどのレイヤーを追加すると、ここに表示されます。"));
      helperLabel_->setText(QStringLiteral("Layer メニューからレイヤーを追加してください。"));
      createButton_->hide();
      return;
    }
    titleLabel_->setText(QStringLiteral("まだコンポジションがありません"));
    bodyLabel_->setText(QStringLiteral("新規コンポジションを作成して、編集を始めましょう。"));
    helperLabel_->setText(QStringLiteral("ボタンを押すと、コンポジション設定ダイアログを開きます。"));
    createButton_->show();
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(10, 12, 16, 148));

    QPen borderPen(QColor(255, 255, 255, 26));
    borderPen.setWidthF(1.0);
    painter.setPen(borderPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5),
                            18.0, 18.0);
  }

private:
  std::function<void()> createRequested_;
  QLabel *titleLabel_ = nullptr;
  QLabel *bodyLabel_ = nullptr;
  QLabel *helperLabel_ = nullptr;
  QPushButton *createButton_ = nullptr;
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
    setFocusPolicy(Qt::StrongFocus);
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
    QStringList items;
    QVector<std::function<void()>> actions;
    const auto add = [&](const QString &label, std::function<void()> action) {
      items.push_back(label);
      actions.push_back(std::move(action));
    };
    add(QStringLiteral("Reset View"), [this]() {
      if (controller_) controller_->resetView();
    });
    add(QStringLiteral("Zoom Fit"), [this]() {
      if (controller_) controller_->zoomFit();
    });
    add(QStringLiteral("Zoom 100%"), [this]() {
      if (controller_) controller_->zoom100();
    });
    add(QStringLiteral("Focus Selected Layer"), [this]() {
      if (controller_) controller_->focusSelectedLayer();
    });
    add(QStringLiteral("Move Tool"), [this]() {
      if (controller_) controller_->setGizmoMode(TransformGizmo::Mode::Move);
    });
    add(QStringLiteral("Rotate Tool"), [this]() {
      if (controller_) controller_->setGizmoMode(TransformGizmo::Mode::Rotate);
    });
    add(QStringLiteral("Scale Tool"), [this]() {
      if (controller_) controller_->setGizmoMode(TransformGizmo::Mode::Scale);
    });
    viewportOverlayActions_ = actions;
    controller_->showCommandPaletteOverlay(QString(), items);
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
    add(QStringLiteral("Place Work Cursor Here"),
        [this, viewportPos]() {
          if (controller_) {
            controller_->placeWorkCursorAtViewportPos(viewportPos);
            controller_->setWorkCursorLabel(
                QStringLiteral("Placed in %1").arg(impl_ ? impl_->activePaneViewLabel()
                                                        : QStringLiteral("Active View")));
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
              QStringLiteral("Centered in %1").arg(impl_->activePaneViewLabel()));
          controller_->setInfoOverlayText(
              QStringLiteral("Work Cursor"),
              QStringLiteral("Centered in the composition"));
        },
        comp != nullptr);
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
        typeText = QStringLiteral("Group Layer");
      } else if (targetLayer->isCloneLayer() ||
                 className.contains(QStringLiteral("Clone"), Qt::CaseInsensitive)) {
        typeText = QStringLiteral("Clone Layer");
      } else if (className.contains(QStringLiteral("Text"), Qt::CaseInsensitive)) {
        typeText = QStringLiteral("Text Layer");
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
      for (const auto &val : layersArray) {
        if (!val.isObject()) {
          continue;
        }
        auto pastedLayer = ArtifactAbstractLayer::fromJson(val.toObject());
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
        ++pasted;
      }
      if (pasted == 0) {
        QMessageBox::warning(this, QStringLiteral("Paste Layers"),
                             QStringLiteral("No layers could be pasted."));
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
      if (std::dynamic_pointer_cast<ArtifactTextLayer>(layer)) {
        add(QStringLiteral("Edit Text"), [this, layer]() {
          editTextLayerInline(this, layer, controller_);
        });
      }
      add(QStringLiteral("Copy Layer"), [layer]() {
        ArtifactCore::ClipboardManager::instance().copyLayer(layer->toJson(),
                                                             layer->layerName());
      });
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
            UndoManager::instance()->push(
                std::make_unique<SetLayerVisibilityCommand>(l, !visible));
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
            UndoManager::instance()->push(
                std::make_unique<SetLayerLockCommand>(l, !locked));
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
            UndoManager::instance()->push(
                std::make_unique<SetLayerSoloCommand>(l, !solo));
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
            UndoManager::instance()->push(
                std::make_unique<SetLayerShyCommand>(l, !shy));
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
        clickedLayer->setPosition3D(
            QVector3D(compCenterX, compCenterY, current.z()));
        clickedLayer->changed();
        ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
            LayerChangedEvent{compNow->id().toString(),
                              clickedLayer->id().toString(),
                              LayerChangedEvent::ChangeType::Modified});
      });
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
                ? std::dynamic_pointer_cast<ArtifactCameraLayer>(
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
        addSeparator();
        add(QStringLiteral("Apply to Layer Position"),
            [this, ctrl = controller_]() {
              if (ctrl) ctrl->trackerApplyToPosition();
            });
        add(QStringLiteral("Apply to Layer Anchor"),
            [this, ctrl = controller_]() {
              if (ctrl) ctrl->trackerApplyToAnchor();
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
    for (const QString &path : paths) {
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

    if (!event->mimeData()->hasUrls()) {
      event->ignore();
      return;
    }

    auto *svc = ArtifactProjectService::instance();
    if (!svc) {
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

    // アセットインポートを非同期で行い、完了後にレイヤーを追加する
    // ウィジェット破棄後に戻ってこないよう、QPointer で寿命を監視する。
    QPointer<CompositionViewport> self(this);
    svc->importAssetsFromPathsAsync(
        paths, [self, paths](const QStringList &importedPaths) mutable {
          if (!self) {
            return;
          }
          self->enqueueDroppedAssets(paths, importedPaths);
        });
    event->acceptProposedAction();
  }

  void hideEvent(QHideEvent *event) override {
    restoreTemporarySolo();
    restoreTemporaryPlayback();
    clearNavigationFeedback();
    if (controller_) {
      controller_->stop();
    }
    QWidget::hideEvent(event);
  }

  void focusOutEvent(QFocusEvent *event) override {
    restoreTemporarySolo();
    restoreTemporaryPlayback();
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
        resizeDebounceTimer_->start(160);
      }
      // Render is deferred to the debounce timer — no immediate
      // renderOneFrame() to avoid redundant work during continuous resize.
    }
  }

  bool event(QEvent *event) override {
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

    if (activatedCallback_) {
      activatedCallback_();
    }

    controller_->notifyViewportInteractionActivity();

    const auto modifiers = event->modifiers();
    const QPointF angleDelta = event->angleDelta();

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
      float deltaX = angleDelta.y(); // Vertical wheel converted to horizontal
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
      const auto layerId = controller_->layerAtViewportPos(event->position());
      if (!layerId.isNil()) {
        if (const auto comp = currentComposition()) {
          if (auto layer = comp->layerById(layerId)) {
            if (std::dynamic_pointer_cast<ArtifactTextLayer>(layer)) {
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
    showViewportContextMenu(event->pos());
    event->accept();
  }

  void mousePressEvent(QMouseEvent *event) override {
    if (activatedCallback_) {
      activatedCallback_();
    }

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
        !event->modifiers().testFlag(Qt::ControlModifier) && controller_) {
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
      if (controller_->gizmo() && controller_->gizmo()->isDragging()) {
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
      if (controller_->gizmo() && controller_->gizmo()->isDragging()) {
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
      clearNavigationFeedback();
      if (controller_) {
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
      controller_->handleMouseRelease();
      if (wasScaleDrag) {
        controller_->markRenderDirty();
      }
      if (wasScaleDrag) {
        update();
      }
      if (!spacePressed_) {
        updateViewportCursor(event->position());
      }
    }

    QWidget::mouseReleaseEvent(event);
  }

  void leaveEvent(QEvent *event) override {
    if (!isPanning_ && !isAltOrbiting_ &&
        !(controller_ && controller_->gizmo() && controller_->gizmo()->isDragging())) {
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

    switch (msg->message) {
    case WM_RBUTTONDOWN:
      if (controller_ && controller_->isPieMenuOverlayVisible()) {
        controller_->cancelPieMenuOverlay();
        return true;
      }
      if (altDown) {
        isAltZooming_ = true;
        setNavigationFeedback(NavigationFeedbackMode::Zoom);
        lastMousePos_ = logPos;
        SetCapture(msg->hwnd);
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
        setCursor(Qt::ClosedHandCursor);
        if (controller_)
          controller_->notifyViewportInteractionActivity();
      } else if (altDown && (GetKeyState(VK_CONTROL) & 0x8000) == 0 &&
                 controller_) {
        isAltOrbiting_ = true;
        setNavigationFeedback(NavigationFeedbackMode::Orbit);
        orbitDragStartPos_ = logPos;
        orbitDragStartOrientation_ =
            controller_->viewportOrientationQuaternion();
        SetCapture(msg->hwnd);
        setCursor(Qt::SizeAllCursor);
        controller_->notifyViewportInteractionActivity();
      } else if (altDown && (GetKeyState(VK_CONTROL) & 0x8000) != 0 &&
                 controller_) {
        controller_->placeWorkCursorAtViewportPos(logPos);
      } else if (controller_) {
        SetCapture(msg->hwnd);
        QMouseEvent synth(QEvent::MouseButtonPress, logPos,
                          mapToGlobal(logPos), Qt::LeftButton,
                          Qt::LeftButton, Qt::NoModifier);
        controller_->handleMousePress(&synth);
        if (controller_->gizmo() && controller_->gizmo()->isDragging()) {
          updateViewportCursor(logPos);
          if (overlayWidget_)
            overlayWidget_->update();
        }
      }
      return true;

    case WM_LBUTTONUP:
      ReleaseCapture();
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
      }
      return true;

    case WM_MBUTTONDOWN:
      isPanning_ = true;
      setNavigationFeedback(NavigationFeedbackMode::Pan);
      isPanningWithMiddle_ = true;
      lastMousePos_ = logPos;
      SetCapture(msg->hwnd);
      setCursor(Qt::ClosedHandCursor);
      if (controller_)
        controller_->notifyViewportInteractionActivity();
      return true;

    case WM_MBUTTONUP:
      if (isPanning_ && isPanningWithMiddle_) {
        isPanning_ = false;
        isPanningWithMiddle_ = false;
        clearNavigationFeedback();
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
      if ((msg->wParam & MK_LBUTTON) && controller_) {
        controller_->handleMouseMove(logPos);
        if (controller_->gizmo() && controller_->gizmo()->isDragging()) {
          // Phase 3: Use fixed-rate render tick instead of singleShot(16) + renderOneFrame().
          controller_->markRenderDirty();
          updateViewportCursor(logPos);
          return true;
        }
        updateViewportCursor(logPos);
      }
      break;

    default:
      break;
    }
    return QWidget::nativeEvent(eventType, message, result);
  }
#endif

  void keyPressEvent(QKeyEvent *event) override {
    if (auto *owner = qobject_cast<ArtifactCompositionEditor *>(parentWidget())) {
      if (owner->handleImportPlacementKeyPress(event)) {
        return;
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
      auto* app = ArtifactApplicationManager::instance();
      if (app && app->motionSketchTool() && app->motionSketchTool()->isSketching()) {
        app->motionSketchTool()->cancelSketch();
        if (controller_) {
          controller_->markRenderDirty();
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
    if (!event->isAutoRepeat() &&
        (event->key() == Qt::Key_W || event->key() == Qt::Key_E ||
         event->key() == Qt::Key_R)) {
      if (controller_) {
        if (event->key() == Qt::Key_W) {
          controller_->setGizmoMode(TransformGizmo::Mode::Move);
        } else if (event->key() == Qt::Key_E) {
          controller_->setGizmoMode(TransformGizmo::Mode::Rotate);
        } else {
          controller_->setGizmoMode(TransformGizmo::Mode::Scale);
        }
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
        spacePressed_ = false;
        const bool shouldTogglePlayback = !didSpacePan_;
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
         togglePlaybackPreview();
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
      return;
    }
    if (playback->isPlaying()) {
      playback->pause();
      return;
    }
    playback->play();
    if (controller_) {
      controller_->start();
    }
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
    UndoManager::instance()->push(
        std::make_unique<SetLayerVisibilityCommand>(layer, !layer->isVisible()));
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

    if (layer->is3D()) {
      const QVector3D current = layer->position3D();
      const QVector3D centeredPos(compCenterX, compCenterY, current.z());
      layer->setPosition3D(centeredPos);
    } else {
      const QVector3D current = layer->position3D();
      const QVector3D centeredPos(compCenterX, compCenterY, current.z());
      layer->setPosition3D(centeredPos);
    }
    layer->changed();
    ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
        LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                          LayerChangedEvent::ChangeType::Modified});
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
                           "tool.mask", true, false, [toolManager]() {
                             if (toolManager)
                               toolManager->setActiveTool(ToolType::Pen);
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
                             [this, gizmo3D]() {
                               gizmo3D->setMode(GizmoMode::Move);
                               controller_->markRenderDirty();
                             }});
      model.items.push_back({"3D Rotate", QIcon(), "gizmo3d.rotate", true,
                             gizmo3D->mode() == GizmoMode::Rotate,
                             [this, gizmo3D]() {
                               gizmo3D->setMode(GizmoMode::Rotate);
                               controller_->markRenderDirty();
                             }});
      model.items.push_back({"3D Scale", QIcon(), "gizmo3d.scale", true,
                             gizmo3D->mode() == GizmoMode::Scale,
                             [this, gizmo3D]() {
                               gizmo3D->setMode(GizmoMode::Scale);
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

  CompositionRenderController *controller_ = nullptr;
  std::function<void()> resizeCallback_;
  bool isPanning_ = false;
  bool isPanningWithMiddle_ = false;
  bool isAltOrbiting_ = false;
  bool isAltZooming_ = false;
  bool spacePressed_ = false;
  bool didSpacePan_ = false;
  NavigationFeedbackMode navigationFeedbackMode_ =
      NavigationFeedbackMode::None;
  quint64 navigationFeedbackGeneration_ = 0;
  std::chrono::steady_clock::time_point lastMaskShortcutPressTime_{};
  bool lastMaskShortcutPressValid_ = false;
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

class ViewportLayoutButton final : public QToolButton {
public:
  explicit ViewportLayoutButton(QWidget *parent = nullptr)
      : QToolButton(parent) {
    setCursor(Qt::PointingHandCursor);
  }

  void setActivatedCallback(std::function<void()> callback) {
    activatedCallback_ = std::move(callback);
  }

protected:
  void mousePressEvent(QMouseEvent *event) override {
    pressedInside_ =
        event && event->button() == Qt::LeftButton && rect().contains(event->position().toPoint());
    QToolButton::mousePressEvent(event);
  }

  void mouseReleaseEvent(QMouseEvent *event) override {
    const bool activate =
        pressedInside_ && event && event->button() == Qt::LeftButton &&
        rect().contains(event->position().toPoint());
    pressedInside_ = false;
    QToolButton::mouseReleaseEvent(event);
    if (activate && activatedCallback_) {
      activatedCallback_();
    }
  }

private:
  std::function<void()> activatedCallback_;
  bool pressedInside_ = false;
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
    QPen framePen(QColor(110, 190, 255, 214), 2.0);
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

  QSize sizeHint() const override { return {138, 164}; }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF panelRect = rect().adjusted(1, 1, -1, -1);
    p.setPen(QPen(QColor(255, 255, 255, isEnabled() ? 42 : 24), 1.0));
    p.setBrush(QColor(14, 18, 26, 156));
    p.drawRoundedRect(panelRect, 12.0, 12.0);

    p.setPen(QColor(210, 225, 240, isEnabled() ? 168 : 80));
    p.drawText(QRectF(0.0, 8.0, width(), 18.0), Qt::AlignCenter,
               QStringLiteral("View"));
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
      faceFont.setPointSizeF(20.0);
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

    p.setPen(QColor(255, 255, 255, 35));
    p.drawRoundedRect(panelRect.adjusted(4.0, 22.0, -4.0, -4.0), 10.0, 10.0);
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
      const float pitchDelta = static_cast<float>(-delta.y()) * 0.55f;
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
    const QRectF bounds = rect().adjusted(16.0, 28.0, -16.0, -16.0);
    const QPointF center(bounds.center().x(), bounds.center().y() + 4.0);
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
    const QRectF bounds = rect().adjusted(16.0, 28.0, -16.0, -16.0);
    const QPointF center(bounds.center().x(), bounds.center().y() + 4.0);
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
    const QRectF bounds = rect().adjusted(16.0, 28.0, -16.0, -16.0);
    const QPointF center(bounds.center().x(), bounds.center().y() + 4.0);
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
  ViewportLayoutMode viewportLayoutMode_ = ViewportLayoutMode::Single;
  int activePaneId_ = 0;
  // Top Toolbar (Zoom/View controls)
  QToolBar *topToolbar_ = nullptr;
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
  QToolButton *displayOptionsBtn_ = nullptr;
  bool layerChromeVisible_ = true;
  bool lockViewToSelection_ = false;
  bool autoAssignFourUpViews_ = true;

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
    const QString viewName = activePaneViewLabel();
    const QString channelName = viewportChannelDisplayLabel();
    auto *playback = ArtifactPlaybackService::instance();
    const QString playState =
        playback && playback->isPlaying() ? QStringLiteral("Playing")
                                          : QStringLiteral("Idle");
    const QString controllerState =
        renderController_ && renderController_->isRunning()
            ? QStringLiteral("Render hot")
            : QStringLiteral("Render paused");
    const QString gizmoState =
        viewportToggleLabel(QStringLiteral("Gizmo"),
                            !gizmoVisibleAction_ ||
                                gizmoVisibleAction_->isChecked());
    const QString xRayState =
        viewportToggleLabel(QStringLiteral("X-Ray"),
                            xRayAction_ && xRayAction_->isChecked());
    const QString isolationState =
        viewportToggleLabel(QStringLiteral("Isolate"),
                            isolationAction_ && isolationAction_->isChecked());
    const QString previewState =
        viewportToggleLabel(QStringLiteral("Preview"), previewOrbitMode_);
    const QString renderSuspendState =
        viewportToggleLabel(QStringLiteral("Hold"),
                            renderSuspendAction_ &&
                                renderSuspendAction_->isChecked());
    const QString lockViewState =
        viewportToggleLabel(QStringLiteral("Lock"), lockViewToSelection_);

    chromeTitleLabel_->setText(QStringLiteral("Composition: %1").arg(compName));
    chromeDetailLabel_->setText(
        QStringLiteral("Layer: %1  |  Selection: %2  |  View: %3  |  Channel: %4")
            .arg(layerName)
            .arg(selectedCount)
            .arg(viewName)
            .arg(channelName));
    chromeMetaLabel_->setText(QStringLiteral("%1  |  %2  |  %3  |  %4  |  %5  |  %6  |  %7  |  %8")
                                  .arg(controllerState)
                                  .arg(playState)
                                  .arg(gizmoState)
                                  .arg(xRayState)
                                  .arg(isolationState)
                                  .arg(previewState)
                                  .arg(renderSuspendState)
                                  .arg(lockViewState));
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
                std::dynamic_pointer_cast<ArtifactShapeLayer>(current)) {
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
          std::dynamic_pointer_cast<ArtifactTextLayer>(current));
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
      const bool showInPane =
          showEmptyState && i < activeViewportPaneCount() && paneState &&
          paneState->view && paneState->view->isVisible();
      if (!emptyStateOverlay) {
        continue;
      }
      if (showInPane) {
        const QPoint paneTopLeft =
            paneState->view->mapTo(owner, QPoint(0, 0));
        emptyStateOverlay->setGeometry(
            QRect(paneTopLeft, paneState->view->size()));
        emptyStateOverlay->setCompositionAvailable(hasComposition);
        emptyStateOverlay->show();
        emptyStateOverlay->raise();
      } else {
        emptyStateOverlay->hide();
      }
    }
    if (viewOrientationWidget_) {
      const QSize sz = viewOrientationWidget_->sizeHint();
      const int x =
          viewportGeometry.left() +
          std::max(12, overlayViewport->width() - sz.width() - 12);
      const int y = viewportGeometry.top() + 12;
      viewOrientationWidget_->setGeometry(x, y, sz.width(), sz.height());
      if (auto *controller = activeRenderController()) {
        viewOrientationWidget_->setOrientationQuaternion(
            controller->viewportOrientationQuaternion());
      }
      const bool showNavigator = hasComposition;
      viewOrientationWidget_->setVisible(showNavigator);
      viewOrientationWidget_->setEnabledState(showNavigator);
      if (viewOrientationWidget_->isVisible()) {
        viewOrientationWidget_->raise();
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
    return visible ? QStringLiteral("Gizmo: ON") : QStringLiteral("Gizmo: OFF");
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
    lines << QStringLiteral("Transform gizmo visibility");
    lines << QStringLiteral("Current: %1").arg(gizmoButtonLabel());
    lines << QStringLiteral("W = Move, R = Rotate, S = Scale");
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
      shadingButton_->setText(viewportChannelDisplayLabel());
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

    const QImage frame = renderer->readbackToImage();
    if (frame.isNull()) {
      QMessageBox::warning(owner, QStringLiteral("Viewport Render Output"),
                           QStringLiteral("現在のビューポートを取得できませんでした。"));
      return;
    }

    if (!saveScreenshotImage(frame, options.filePath, options.format, options.jpegQuality)) {
      QMessageBox::warning(owner, QStringLiteral("Viewport Render Output"),
                           QStringLiteral("保存に失敗しました:\n%1").arg(options.filePath));
      return;
    }

    QMessageBox::information(owner, QStringLiteral("Viewport Render Output"),
                             QStringLiteral("保存しました:\n%1").arg(options.filePath));
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
    if (auto *mw = qobject_cast<QMainWindow *>(owner->window())) {
      mw->setDockNestingEnabled(immersive);
    } else if (auto *topLevel = owner->window()) {
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
    const QImage screenshot = captureScreenshotForOptions(
        renderController_, owner, ScreenshotCaptureSource::Renderer);
    if (screenshot.isNull()) {
      QMessageBox::warning(owner, QStringLiteral("スクリーンショット"),
                           QStringLiteral("現在のフレームを取得できませんでした。"));
      return false;
    }

    if (!saveScreenshotImage(screenshot, filePath, suffix, 95)) {
      QMessageBox::warning(owner, QStringLiteral("スクリーンショット"),
                           QStringLiteral("保存に失敗しました:\n%1").arg(filePath));
      return false;
    }

    QMessageBox::information(owner, QStringLiteral("スクリーンショット"),
                             QStringLiteral("保存しました:\n%1").arg(filePath));
    return true;
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
  }
};

ArtifactCompositionEditor::ArtifactCompositionEditor(QWidget *parent)
    : QWidget(parent), impl_(new Impl()) {
  QElapsedTimer ctorTimer;
  ctorTimer.start();
  setMinimumSize(0, 0);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setAutoFillBackground(true);

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
  if (auto *settings = ArtifactCore::ArtifactAppSettings::instance()) {
    QObject::connect(settings, &ArtifactCore::ArtifactAppSettings::settingsChanged,
                     this, [this]() {
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
                     });
  }

  QObject::connect(impl_->renderController_,
                   &CompositionRenderController::videoDebugMessage, this,
                   &ArtifactCompositionEditor::videoDebugMessage);

  impl_->compositionView_ =
      new CompositionViewport(impl_->renderController_, this);
  impl_->panes_[0].view = impl_->compositionView_;
  for (int i = 1; i < ArtifactCompositionEditor::Impl::kViewportPaneCount; ++i) {
    impl_->panes_[i].view =
        new CompositionViewport(impl_->panes_[i].controller, this);
    impl_->panes_[i].view->hide();
  }
  impl_->viewportHost_ = new QWidget(this);
  auto *viewportHostLayout = new QVBoxLayout(impl_->viewportHost_);
  viewportHostLayout->setContentsMargins(0, 0, 0, 0);
  viewportHostLayout->setSpacing(0);
  impl_->viewportRowsSplitter_ = new QSplitter(Qt::Vertical, impl_->viewportHost_);
  impl_->viewportRowsSplitter_->setChildrenCollapsible(false);
  impl_->viewportTopSplitter_ = new QSplitter(Qt::Horizontal, impl_->viewportRowsSplitter_);
  impl_->viewportTopSplitter_->setChildrenCollapsible(false);
  impl_->viewportBottomSplitter_ =
      new QSplitter(Qt::Horizontal, impl_->viewportRowsSplitter_);
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
        if (impl_) {
          if (auto *controller = impl_->activeRenderController()) {
            controller->setViewportOrientationQuaternion(orientation);
            if (impl_->overlayView_) {
              impl_->overlayView_->update();
            }
          }
        }
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
  impl_->topToolbar_->setMovable(false);
  impl_->topToolbar_->setToolButtonStyle(Qt::ToolButtonTextOnly);
  impl_->topToolbar_->setIconSize(QSize(18, 18));
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
    const auto composition = impl_->renderController_
                                 ? impl_->renderController_->composition()
                                 : ArtifactCompositionPtr{};
    impl_->forEachSecondaryController(
        [&composition](CompositionRenderController *controller) {
          controller->stop();
          controller->setComposition(composition);
        });
    impl_->applyViewportLayout();
    if (impl_->autoAssignFourUpViews_) {
      impl_->applyFourUpDefaultOrientations();
    }
    if (composition) {
      impl_->forEachActiveSecondaryController(
          [](CompositionRenderController *controller) { controller->start(); });
      impl_->forEachActiveViewport([](CompositionViewport *view, int index) {
        if (index > 0) {
          view->requestInitialFit();
        }
      });
    }
    impl_->syncOverlayGeometry(this);
  };
  impl_->viewportLayoutButton_->setActivatedCallback([this, setViewportLayout]() {
    if (!impl_) {
      return;
    }
    setViewportLayout(impl_->nextViewportLayoutMode());
  });

  auto *viewPresetMenu = new QMenu(this);
  polishEditorMenu(viewPresetMenu, this);
  auto *viewValuesHost = new QWidget(viewPresetMenu);
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
  QObject::connect(viewPresetMenu, &QMenu::aboutToShow, this,
                   [syncViewValueEditors]() { syncViewValueEditors(); });
  impl_->topToolbar_->addWidget(impl_->viewPresetButton_);
  impl_->viewportBookmarkButton_ = new QToolButton(impl_->topToolbar_);
  impl_->viewportBookmarkButton_->setText(QStringLiteral("Bookmark"));
  impl_->viewportBookmarkButton_->setIcon(QIcon(resolveIconPath("Studio/viewmenu_bookmarks.svg")));
  impl_->viewportBookmarkButton_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  impl_->viewportBookmarkButton_->setToolTip(
      QStringLiteral("Open the Camera ブックマーク menu from the main View menu"));
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
  impl_->screenshotButton_ = new QToolButton(this);
  impl_->screenshotButton_->setText(QStringLiteral("Screenshot"));
  impl_->screenshotButton_->setMenu(screenshotMenu);
  impl_->screenshotButton_->setPopupMode(QToolButton::InstantPopup);
  impl_->topToolbar_->addWidget(impl_->screenshotButton_);
  impl_->viewportRenderOutputButton_ = new QToolButton(this);
  impl_->viewportRenderOutputButton_->setText(QStringLiteral("Render Output"));
  impl_->viewportRenderOutputButton_->setToolTip(
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
    connect(action, &QAction::triggered, this, [this, toolType, text]() {
      if (auto *toolManager =
              ArtifactApplicationManager::instance()
                  ? ArtifactApplicationManager::instance()->toolManager()
                  : nullptr) {
        toolManager->setActiveTool(toolType);
      }
      if (impl_->toolModeButton_) {
        impl_->toolModeButton_->setText(text);
      }
    });
  };
  addToolAction(QStringLiteral("Select"),
                QStringLiteral("MaterialVS/neutral/select.svg"),
                ToolType::Selection, true);
  addToolAction(QStringLiteral("Hand"),
                QStringLiteral("MaterialVS/neutral/hand.svg"), ToolType::Hand,
                false);
  addToolAction(QStringLiteral("Mask"),
                QStringLiteral("MaterialVS/neutral/draw.svg"), ToolType::Pen,
                false);
  addToolAction(QStringLiteral("Shape"),
                QStringLiteral("MaterialVS/neutral/select.svg"), ToolType::Shape,
                false);
  addToolAction(QStringLiteral("Anchor"),
                QStringLiteral("MaterialVS/neutral/transform.svg"),
                ToolType::AnchorPoint, false);
  impl_->toolModeButton_ = new QToolButton(this);
  impl_->toolModeButton_->setText(QStringLiteral("Select"));
  impl_->toolModeButton_->setMenu(toolMenu);
  impl_->toolModeButton_->setPopupMode(QToolButton::InstantPopup);
  impl_->toolModeButton_->setToolTip(QStringLiteral("Select current editing tool. Mask opens mask editing. Anchor opens pivot editing."));
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
  impl_->gizmoModeButton_ = new QToolButton(this);
  impl_->gizmoModeButton_->setText(impl_->gizmoButtonLabel());
  impl_->gizmoModeButton_->setMenu(gizmoMenu);
  impl_->gizmoModeButton_->setIcon(
      loadEditorMenuIcon(QStringLiteral("MaterialVS/neutral/transform.svg")));
  impl_->gizmoModeButton_->setToolTip(impl_->gizmoButtonTooltip());
  impl_->gizmoModeButton_->setPopupMode(QToolButton::InstantPopup);
  impl_->topToolbar_->addWidget(impl_->gizmoModeButton_);

  impl_->chromeStrip_ = new QFrame(this);
  impl_->chromeStrip_->setFrameShape(QFrame::StyledPanel);
  impl_->chromeStrip_->setFrameShadow(QFrame::Plain);
  impl_->chromeStrip_->setAutoFillBackground(true);
  impl_->chromeStrip_->setMinimumHeight(44);
  impl_->chromeStrip_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
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
  impl_->pivotModeButton_ = new QToolButton(this);
  impl_->pivotModeButton_->setText(QStringLiteral("Pivot"));
  impl_->pivotModeButton_->setMenu(pivotMenu);
  impl_->pivotModeButton_->setPopupMode(QToolButton::InstantPopup);
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

  // Bottom Bar (Viewer Controls)
  impl_->bottomBar_ = new QWidget(this);
  impl_->bottomBar_->setMinimumHeight(28);
  impl_->bottomBar_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
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
  {
    QPalette pal = impl_->resolutionCombo_->palette();
    pal.setColor(QPalette::Base, QColor(theme.backgroundColor));
    pal.setColor(QPalette::Button, QColor(theme.secondaryBackgroundColor));
    pal.setColor(QPalette::Text, QColor(theme.textColor));
    impl_->resolutionCombo_->setPalette(pal);
  }

  // Fast Preview Button (Lightning)
  impl_->fastPreviewBtn_ = new QToolButton(impl_->bottomBar_);
  impl_->fastPreviewBtn_->setText("⚡"); // Lightning icon
  impl_->fastPreviewBtn_->setToolTip("Fast Preview (Lightning)");
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

  // Display Options Button (Background / Grid / Guides)
  impl_->displayOptionsBtn_ = new QToolButton(impl_->bottomBar_);
  impl_->displayOptionsBtn_->setText("👁"); // View options icon
  impl_->displayOptionsBtn_->setToolTip(
      "Choose background, grid, and guide options");
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
  anchorCenterAct->setCheckable(true);
  cameraOverlayAct->setCheckable(true);
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
  bottomLayout->addWidget(impl_->shadingButton_);
  bottomLayout->addWidget(impl_->displayOptionsBtn_);
  bottomLayout->addStretch();

  // Assembly
  mainLayout->addWidget(impl_->topToolbar_);
  mainLayout->addWidget(impl_->chromeStrip_);
  mainLayout->addWidget(impl_->viewportHost_, 1);
  mainLayout->addWidget(impl_->bottomBar_);
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
    if (!layer || !std::dynamic_pointer_cast<ArtifactTextLayer>(layer)) {
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
            [this](const ToolChangedEvent &) {
              if (impl_) {
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
                       const std::string report =
                           ArtifactCore::Profiler::instance()
                               .generateDiagnosticReport(60);
                       QGuiApplication::clipboard()->setText(
                           QString::fromStdString(report));
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

ArtifactCompositionEditor::~ArtifactCompositionEditor() { delete impl_; }

bool ArtifactCompositionEditor::event(QEvent *event) {
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

} // namespace Artifact
