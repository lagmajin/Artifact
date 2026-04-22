module;
#include <QAction>
#include <QActionGroup>
#include <QClipboard>
#include <QCloseEvent>
#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QCursor>
#include <QDebug>
#include <QLoggingCategory>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QEvent>
#include <QFileInfo>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHash>
#include <QHideEvent>
#include <QIcon>
#include <QImageReader>
#include <QKeySequence>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPointer>
#include <QResizeEvent>
#include <QSet>
#include <QShortcut>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVector>
#include <QWheelEvent>
#include <algorithm>
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

import Artifact.Widgets.CompositionRenderController;
import Artifact.Widgets.TransformGizmo;
import Artifact.Widgets.Gizmo3D;
import Artifact.Widgets.PieMenu;
import UI.View.Orientation.Navigator;
import Artifact.MainWindow;
import Color.Float;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Text;
import Artifact.Layer.Svg;
import Artifact.Application.Manager;
import Artifact.Layers.Selection.Manager;
import Artifact.Service.Project;
import Artifact.Service.Playback;
import Artifact.Layer.Video;
import Artifact.Tool.Manager;
import Utils.Path;
import Artifact.Layer.InitParams;
import File.TypeDetector;
import Application.AppSettings;
import Widgets.Utils.CSS;
import Event.Bus;
import Artifact.Event.Types;
import Artifact.Widgets.ProfilerOverlay;
import Artifact.Widgets.ProfilerPanel;
import Artifact.Widgets.EventBusDebugger;
import ArtifactCore.Utils.PerformanceProfiler;
import Codec.Thumbnail.FFmpeg;

namespace Artifact {

W_OBJECT_IMPL(ArtifactCompositionEditor)
Q_LOGGING_CATEGORY(compositionViewLog, "artifact.compositionview");

namespace {
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

ArtifactCompositionPtr resolvePreferredComposition() {
  if (auto *app = ArtifactApplicationManager::instance()) {
    if (auto *active = app->activeContextService()) {
      if (auto comp = active->activeComposition()) {
        return comp;
      }
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
        ctrl_->renderOneFrame();
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

bool editTextLayerInline(QWidget *parent, const ArtifactAbstractLayerPtr &layer,
                         CompositionRenderController *controller) {
  const auto textLayer = std::dynamic_pointer_cast<ArtifactTextLayer>(layer);
  if (!textLayer || !parent) {
    return false;
  }

  // Get renderer from controller (assuming we add a getter)
  auto *renderer = controller ? controller->renderer() : nullptr;
  if (!renderer) {
    return false;
  }

  // Get text layer bounding box in canvas coordinates
  QRectF bbox = layer->transformedBoundingBox();
  if (bbox.isEmpty()) {
    bbox = QRectF(0, 0, 400, 100); // Fallback
  }

  // Convert to viewport coordinates
  auto topLeft = renderer->canvasToViewport(
      {static_cast<float>(bbox.left()), static_cast<float>(bbox.top())});
  auto bottomRight = renderer->canvasToViewport(
      {static_cast<float>(bbox.right()), static_cast<float>(bbox.bottom())});

  int x = static_cast<int>(topLeft.x);
  int y = static_cast<int>(topLeft.y);
  int w = std::max(100, static_cast<int>(bottomRight.x - topLeft.x));
  int h = std::max(30, static_cast<int>(bottomRight.y - topLeft.y));

  QWidget *host = parent->window() ? parent->window() : parent;
  auto *editor = new QPlainTextEdit(host);
  editor->setPlainText(textLayer->text().toQString());
  editor->setPlaceholderText(QStringLiteral("Enter text..."));
  editor->selectAll();

  const float size = std::max(10.0f, textLayer->fontSize());
  const float zoom = renderer ? renderer->getZoom() : 1.0f;
  const int pointSize = static_cast<int>(size * 0.75f * zoom);
  const auto theme = ArtifactCore::currentDCCTheme();
  QFont editorFont = editor->font();
  editorFont.setFamily(textLayer->fontFamily().toQString());
  editorFont.setPointSize(pointSize);
  editor->setFont(editorFont);
  QPalette editorPalette = editor->palette();
  editorPalette.setColor(QPalette::Base,
                         QColor(theme.secondaryBackgroundColor));
  editorPalette.setColor(QPalette::Text, QColor(theme.textColor));
  editorPalette.setColor(QPalette::Window,
                         QColor(theme.secondaryBackgroundColor));
  editor->setPalette(editorPalette);

  const QPoint hostPos = parent->mapTo(host, QPoint(x, y));
  editor->setGeometry(hostPos.x(), hostPos.y(), w, h);

  editor->installEventFilter(
      new TextOverlayFilter(editor, textLayer, controller));
  editor->show();
  editor->setFocus();
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

class CompositionViewport final : public QWidget {
  friend class CompositionOverlayWidget;

public:
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
      controller_->renderOneFrame();
      resizePending_ = false;
      if (pendingInitialFit_) {
        QTimer::singleShot(50, this, [this]() { scheduleInitialFit(); });
      }
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

  void setPieMenu(ArtifactPieMenuWidget *pieMenu) { pieMenu_ = pieMenu; }
  void setOverlayWidget(QWidget *overlayWidget) {
    overlayWidget_ = overlayWidget;
    if (overlayWidget_) {
      overlayWidget_->setAttribute(Qt::WA_TransparentForMouseEvents);
    }
  }
  void setOverlayVisible(bool visible) {
    if (overlayWidget_) {
      overlayWidget_->setVisible(visible);
      if (visible) {
        overlayWidget_->raise();
      }
    }
  }

  void updateViewportCursor(const QPointF &pos) {
    if (!controller_ || spacePressed_) {
      return;
    }
    setCursor(controller_->cursorShapeForViewportPos(pos));
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
                                          : detector.detect(path);
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
        ArtifactLayerInitParams params(asset.layerName, LayerType::Video);
        svc->addLayerToCurrentComposition(params, shouldSelectThisLayer);
        if (auto comp = controller_ ? controller_->composition() : nullptr) {
          const auto layers = comp->allLayer();
          if (!layers.isEmpty()) {
            auto maybeLast = layers.last();
            if (auto videoLayer =
                    std::dynamic_pointer_cast<ArtifactVideoLayer>(maybeLast)) {
              videoLayer->setSourceFile(asset.importedPath);
            }
          }
        }
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
    if (controller_) {
      const bool needsInitialize = !controller_->isInitialized();
      if (needsInitialize) {
        QTimer::singleShot(16, this, [this]() {
          if (!controller_ || !isVisible() || controller_->isInitialized()) {
            return;
          }
          QElapsedTimer timer;
          timer.start();
          controller_->initialize(this);
          qInfo() << "[CompositionEditor][Startup] controller initialize ms="
                  << timer.elapsed();
          timer.restart();
          controller_->recreateSwapChain(this);
          qInfo() << "[CompositionEditor][Startup] recreateSwapChain ms="
                  << timer.elapsed();
          controller_->setViewportSize((float)width(), (float)height());
          // フェーズ2：Composition
          // 接続とレンダー開始はイベントループを1サイクル挟んで実行
          // (重いシェーダーコンパイル直後の UI ブロックを軽減)
          // [PERF] 初回コンポジションロードをUI安定後に遅延
          // 250ms
          // 待つことでウィンドウ表示アニメーションやドライバ初期化と競合しない
          QTimer::singleShot(250, this, [this]() {
            if (!controller_ || !isVisible() || window()->isMinimized()) {
              return;
            }
            if (syncPreferredComposition()) {
              return;
            }
            schedulePreferredCompositionRetry();
          });
        });
      } else {
        syncPreferredComposition();
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
    if (controller_) {
      controller_->stop();
    }
    QWidget::hideEvent(event);
  }

  void focusOutEvent(QFocusEvent *event) override {
    restoreTemporarySolo();
    restoreTemporaryPlayback();
    QWidget::focusOutEvent(event);
  }

  void resizeEvent(QResizeEvent *event) override {
    QWidget::resizeEvent(event);
    if (controller_ && controller_->isInitialized()) {
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

  void wheelEvent(QWheelEvent *event) override {
    if (pieMenu_ && pieMenu_->isVisible())
      return; // Block while menu open

    if (!controller_) {
      return;
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
    const auto layerId = controller_
                             ? controller_->layerAtViewportPos(event->pos())
                             : LayerID::Nil();
    if (layerId.isNil()) {
      QWidget::contextMenuEvent(event);
      return;
    }

    const auto comp = currentComposition();
    const auto layer =
        comp ? comp->layerById(layerId) : ArtifactAbstractLayerPtr{};
    if (!layer || !std::dynamic_pointer_cast<ArtifactTextLayer>(layer)) {
      QWidget::contextMenuEvent(event);
      return;
    }

    QMenu menu(this);
    auto *editAction = menu.addAction(QStringLiteral("Edit Text"));
    editAction->setEnabled(true);
    connect(editAction, &QAction::triggered, this,
            [this, layer]() { editTextLayerInline(this, layer, controller_); });

    menu.addSeparator();
    menu.addAction(QStringLiteral("Reset View"), this, [this]() {
      if (controller_) {
        controller_->resetView();
      }
    });
    menu.exec(event->globalPos());
    event->accept();
  }

  void mousePressEvent(QMouseEvent *event) override {
    if (pieMenu_ && pieMenu_->isVisible())
      return;

    qDebug() << "[VP] mousePressEvent button=" << event->button()
             << "middle=" << (event->button() == Qt::MiddleButton)
             << "space=" << spacePressed_
             << "pos=" << event->position();

    if (event->button() == Qt::MiddleButton ||
        (event->button() == Qt::LeftButton && spacePressed_)) {
      isPanning_ = true;
      isPanningWithMiddle_ = (event->button() == Qt::MiddleButton);
      lastMousePos_ = event->position();
      if (controller_) {
        controller_->notifyViewportInteractionActivity();
      }
      setCursor(Qt::ClosedHandCursor);
      qDebug() << "[VP] panning started, isPanning_=" << isPanning_;
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
    if (pieMenu_ && pieMenu_->isVisible()) {
      pieMenu_->updateMousePos(event->globalPosition().toPoint());
      event->accept();
      return;
    }

    // Recover isPanning_ state if grabMouse() didn't work on WA_NativeWindow
    if (!isPanning_ && (event->buttons() & Qt::MiddleButton) && controller_) {
      qDebug() << "[VP] mouseMoveEvent recovering pan, buttons=" << event->buttons();
      isPanning_ = true;
      isPanningWithMiddle_ = true;
      lastMousePos_ = event->position();
    }
    if (isPanning_ && controller_) {
      const QPointF delta = event->position() - lastMousePos_;
      lastMousePos_ = event->position();
      controller_->notifyViewportInteractionActivity();
      controller_->panBy(delta);
      qDebug() << "[VP] panning, delta=" << delta;
      event->accept();
      return;
    }

    if (controller_) {
      controller_->handleMouseMove(event->position());
      if (controller_->gizmo() && controller_->gizmo()->isDragging()) {
        // Throttle: collapse multiple mouse-move events into one render per
        // event-loop tick
        if (!pendingGizmoDragRender_) {
          pendingGizmoDragRender_ = true;
          QTimer::singleShot(16, this, [this]() {
            pendingGizmoDragRender_ = false;
            if (controller_)
              controller_->renderOneFrame();
          });
        }
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
    if (pieMenu_ && pieMenu_->isVisible()) {
      // Confirmation happens on KeyRelease (Tab), but we can also allow click
      // to confirm
      if (event->button() == Qt::LeftButton) {
        pieMenu_->confirmSelection();
      }
      event->accept();
      return;
    }

    if (isPanning_ &&
        ((isPanningWithMiddle_ && event->button() == Qt::MiddleButton) ||
         (!isPanningWithMiddle_ && event->button() == Qt::LeftButton))) {
      isPanning_ = false;
      isPanningWithMiddle_ = false;
      if (controller_) {
        controller_->finishViewportInteraction();
      }
      if (spacePressed_) {
        setCursor(Qt::OpenHandCursor);
      } else {
        unsetCursor();
      }
      event->accept();
      return;
    }

    if (controller_) {
      const bool wasScaleDrag = isScaleDragActive();
      controller_->handleMouseRelease();
      if (wasScaleDrag) {
        controller_->renderOneFrame();
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
    if (!isPanning_ && !(controller_ && controller_->gizmo() && controller_->gizmo()->isDragging())) {
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

    switch (msg->message) {
    case WM_LBUTTONDOWN:
      if (spacePressed_) {
        isPanning_ = true;
        isPanningWithMiddle_ = false;
        lastMousePos_ = logPos;
        SetCapture(msg->hwnd);
        setCursor(Qt::ClosedHandCursor);
        if (controller_)
          controller_->notifyViewportInteractionActivity();
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
      if (isPanning_ && !isPanningWithMiddle_) {
        isPanning_ = false;
        if (controller_)
          controller_->finishViewportInteraction();
        if (!spacePressed_)
          unsetCursor();
      } else if (controller_) {
        controller_->handleMouseRelease();
        if (overlayWidget_)
          overlayWidget_->update();
        controller_->renderOneFrame();
      }
      return true;

    case WM_MBUTTONDOWN:
      isPanning_ = true;
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
        ReleaseCapture();
        if (controller_)
          controller_->finishViewportInteraction();
        if (!spacePressed_)
          unsetCursor();
      }
      return true;

    case WM_MOUSEMOVE:
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
          if (!pendingGizmoDragRender_) {
            pendingGizmoDragRender_ = true;
            QTimer::singleShot(16, this, [this]() {
              pendingGizmoDragRender_ = false;
              if (controller_)
                controller_->renderOneFrame();
            });
          }
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
    if (event->key() == Qt::Key_Tab && !event->isAutoRepeat()) {
      showPieMenu();
      event->accept();
      return;
    }

    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
      spacePressed_ = true;
      setCursor(Qt::OpenHandCursor);
      event->accept();
      return;
    }
    if (event->key() == Qt::Key_F && !event->isAutoRepeat()) {
      if (controller_) {
        controller_->focusSelectedLayer();
      }
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
    if (!event->isAutoRepeat() && event->key() == Qt::Key_P) {
      beginTemporaryPlayback();
      event->accept();
      return;
    }
    if (!event->isAutoRepeat() && event->key() == Qt::Key_M) {
      if (auto *toolManager =
              ArtifactApplicationManager::instance()
                  ? ArtifactApplicationManager::instance()->toolManager()
                  : nullptr) {
        toolManager->setActiveTool(ToolType::Pen);
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
    if ((event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete) &&
        !event->isAutoRepeat()) {
      auto *app = ArtifactApplicationManager::instance();
      auto *svc = ArtifactProjectService::instance();
      auto *active = app ? app->activeContextService() : nullptr;
      auto *selection = app ? app->layerSelectionManager() : nullptr;
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
     if (event->key() == Qt::Key_Tab && !event->isAutoRepeat()) {
       if (pieMenu_ && pieMenu_->isVisible()) {
         pieMenu_->confirmSelection();
       }
       event->accept();
       return;
     }

     if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
       spacePressed_ = false;
       if (!isPanningWithMiddle_) {
         isPanning_ = false;
       }
       if (controller_) {
         controller_->finishViewportInteraction();
       }
       unsetCursor();
       if (controller_) {
         setCursor(controller_->cursorShapeForViewportPos(
             mapFromGlobal(QCursor::pos())));
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
    auto *app = ArtifactApplicationManager::instance();
    auto *selection = app ? app->layerSelectionManager() : nullptr;
    return selection ? selection->currentLayer() : ArtifactAbstractLayerPtr{};
  }

  ArtifactCompositionPtr currentComposition() const {
    auto *active =
        ArtifactApplicationManager::instance()
            ? ArtifactApplicationManager::instance()->activeContextService()
            : nullptr;
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
    const auto layers = comp->allLayer();
    temporarySoloStates_.reserve(layers.size());
    for (const auto &candidate : layers) {
      if (!candidate) {
        continue;
      }
      temporarySoloStates_.push_back({candidate->id(), candidate->isSolo()});
    }

    temporarySoloActive_ = true;
    svc->soloOnlyLayerInCurrentComposition(layer->id());
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
    auto *svc = ArtifactProjectService::instance();
    const auto layer = currentLayer();
    if (!svc || !layer) {
      return;
    }
    const bool current = svc->isLayerVisibleInCurrentComposition(layer->id());
    svc->setLayerVisibleInCurrentComposition(layer->id(), !current);
  }

  void soloCurrentLayer() {
    auto *svc = ArtifactProjectService::instance();
    const auto layer = currentLayer();
    if (!svc || !layer) {
      return;
    }
    svc->soloOnlyLayerInCurrentComposition(layer->id());
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
      controller_->renderOneFrame();
      if (autoStartPending_) {
        autoStartPending_ = false;
        controller_->start();
      }
    });
  }

  bool autoStartPending_ = false;

  ArtifactPieMenuWidget *pieMenu_ = nullptr;
  void showPieMenu() {
    if (!pieMenu_ || !controller_)
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
                               controller_->renderOneFrame();
                             }});
      model.items.push_back({"3D Rotate", QIcon(), "gizmo3d.rotate", true,
                             gizmo3D->mode() == GizmoMode::Rotate,
                             [this, gizmo3D]() {
                               gizmo3D->setMode(GizmoMode::Rotate);
                               controller_->renderOneFrame();
                             }});
      model.items.push_back({"3D Scale", QIcon(), "gizmo3d.scale", true,
                             gizmo3D->mode() == GizmoMode::Scale,
                             [this, gizmo3D]() {
                               gizmo3D->setMode(GizmoMode::Scale);
                               controller_->renderOneFrame();
                             }});
    }

    // Grid Toggle
    model.items.push_back(
        {"Grid", loadIconWithFallback("MaterialVS/neutral/grid.svg"),
         "display.grid", true, controller_->isShowGrid(),
         [this]() { controller_->setShowGrid(!controller_->isShowGrid()); }});

    // Safe Area Toggle
    model.items.push_back(
        {"Safe Area", loadIconWithFallback("MaterialVS/neutral/safe_area.svg"),
         "display.safeArea", true, controller_->isShowSafeMargins(), [this]() {
           controller_->setShowSafeMargins(!controller_->isShowSafeMargins());
         }});

    pieMenu_->setModel(model);
    pieMenu_->showAt(QCursor::pos());
  }

  void saveCurrentFrame(CompositionRenderController *controller) {
    auto comp = controller->composition();
    if (!comp)
      return;

    auto *svc = ArtifactProjectService::instance();
    if (!svc)
      return;

    // 選択されているレイヤーを取得（簡易的にコントローラーから）
    // 実際には SelectionManager 経由が良いが、controller にもキャッシュがある
    // ここでは CompositionRenderController の内部実装にアクセスできないため、
    // 外部から見える情報を元にするか、controller
    // にメソッドを追加する必要がある。
    // 一旦、コンポジション全体のレンダリング結果（もし取れれば）か、
    // ログ出力で生存確認する。

    qDebug() << "Debug: F12 pressed. Attempting to save current frame...";

    // フォルダ作成
    QDir dir(".");
    if (!dir.exists("test")) {
      dir.mkdir("test");
    }

    // TODO: 本来は controller->captureSelectedLayer() のようなものが必要
    // ここではコンポジション内の「最初の動画レイヤー」を探して保存するデバッグコードを試みる
    for (auto &layer : comp->allLayer()) {
      if (auto video = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
        QImage img = video->currentFrameToQImage();
        if (!img.isNull()) {
          QString path = QString("test/frame_%1_%2.png")
                             .arg(layer->id().toString())
                             .arg(comp->framePosition().framePosition());
          if (img.save(path)) {
            qDebug() << "Successfully saved debug frame to:" << path;
          } else {
            qWarning() << "Failed to save image to:" << path;
          }
        } else {
          qWarning() << "Layer" << layer->id().toString()
                     << "returned null image.";
        }
      }
    }
  }

  CompositionRenderController *controller_ = nullptr;
  bool isPanning_ = false;
  bool isPanningWithMiddle_ = false;
  bool spacePressed_ = false;
  bool pendingInitialFit_ = true;
  bool pendingGizmoDragRender_ = false;
  QTimer *resizeDebounceTimer_ = nullptr;
  QSize pendingResizeSize_;
  bool resizePending_ = false;
  QPointF lastMousePos_;
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
  // 動画ファイルのキャンバスサイズキャッシュ（非同期取得）
  QHash<QString, QSize> videoDimensionCache_;
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
      return QStringLiteral("3D layer");
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
      return QSizeF(cw * 0.5, ch * 0.5);
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
    ArtifactCore::FileTypeDetector detector;
    const bool svgShapeFile = isSvgShapeFile(path);
    const auto fileType =
        svgShapeFile ? ArtifactCore::FileType::Image : detector.detect(path);

    if (fileType == ArtifactCore::FileType::Video) {
      startVideoDimensionLoad(path);
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

protected:
  void paintEvent(QPaintEvent *) override { Q_UNUSED(viewport_); }

private:
  CompositionViewport *viewport_ = nullptr;
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
    update();
  }

  ArtifactCore::ViewOrientationHotspot orientation() const { return hotspot_; }

  void setEnabledState(bool enabled) {
    setEnabled(enabled);
    update();
  }

  void setActivatedCallback(
      std::function<void(ArtifactCore::ViewOrientationHotspot)> callback) {
    activatedCallback_ = std::move(callback);
  }

  QSize sizeHint() const override { return {132, 168}; }

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

    const float margin = 12.0f;
    const float gap = 8.0f;
    const float faceW = (width() - margin * 2.0f - gap) * 0.5f;
    const float faceH = (height() - 44.0f - margin * 2.0f - gap * 2.0f) / 3.0f;
    const float topY = 28.0f;
    const float row2Y = topY + faceH + gap;
    const float row3Y = row2Y + faceH + gap;
    const float leftX = margin;
    const float rightX = margin + faceW + gap;

    const std::array<std::pair<ArtifactCore::ViewOrientationHotspot, QRectF>, 6>
        faces = {{{ArtifactCore::ViewOrientationHotspot::Top,
                   QRectF(leftX, topY, faceW, faceH)},
                  {ArtifactCore::ViewOrientationHotspot::Back,
                   QRectF(rightX, topY, faceW, faceH)},
                  {ArtifactCore::ViewOrientationHotspot::Left,
                   QRectF(leftX, row2Y, faceW, faceH)},
                  {ArtifactCore::ViewOrientationHotspot::Front,
                   QRectF(rightX, row2Y, faceW, faceH)},
                  {ArtifactCore::ViewOrientationHotspot::Right,
                   QRectF(leftX, row3Y, faceW, faceH)},
                  {ArtifactCore::ViewOrientationHotspot::Bottom,
                   QRectF(rightX, row3Y, faceW, faceH)}}};

    for (const auto &[hotspot, faceRect] : faces) {
      const bool selected = hotspot == hotspot_;
      const bool hovered = hotspot == hoverHotspot_;
      QColor fill(51, 72, 96, 190);
      QColor border(125, 180, 230, 180);
      QColor text(233, 242, 248, 220);
      if (!isEnabled()) {
        fill.setAlpha(70);
        border.setAlpha(70);
        text.setAlpha(90);
      } else if (selected) {
        fill = QColor(78, 126, 170, 220);
        border = QColor(200, 232, 255, 230);
      } else if (hovered) {
        fill = QColor(68, 94, 125, 220);
        border = QColor(222, 240, 255, 230);
      }

      p.setPen(QPen(border, selected ? 2.0 : 1.2));
      p.setBrush(fill);
      p.drawRoundedRect(faceRect, 8.0, 8.0);

      p.setPen(text);
      p.drawText(faceRect, Qt::AlignCenter, hotspotLabel(hotspot));
    }

    p.setPen(QColor(255, 255, 255, 35));
    p.drawRoundedRect(panelRect.adjusted(4.0, 22.0, -4.0, -4.0), 10.0, 10.0);
  }

  void mouseMoveEvent(QMouseEvent *event) override {
    if (!isEnabled()) {
      QWidget::mouseMoveEvent(event);
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
    const auto hotspot = hotspotAt(event->position());
    if (hotspot == ArtifactCore::ViewOrientationHotspot::None) {
      QWidget::mousePressEvent(event);
      return;
    }
    hotspot_ = hotspot;
    navigator_.snapTo(hotspot_, true);
    update();
    if (activatedCallback_) {
      activatedCallback_(hotspot_);
    }
    event->accept();
  }

private:
  static QString hotspotLabel(ArtifactCore::ViewOrientationHotspot hotspot) {
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
    default:
      return QStringLiteral("-");
    }
  }

  ArtifactCore::ViewOrientationHotspot hotspotAt(const QPointF &pos) const {
    const QRectF panel = rect().adjusted(1, 1, -1, -1);
    const float margin = 12.0f;
    const float gap = 8.0f;
    const float faceW = (panel.width() - margin * 2.0f - gap) * 0.5f;
    const float faceH =
        (panel.height() - 44.0f - margin * 2.0f - gap * 2.0f) / 3.0f;
    const float topY = 28.0f;
    const float row2Y = topY + faceH + gap;
    const float row3Y = row2Y + faceH + gap;
    const float leftX = margin;
    const float rightX = margin + faceW + gap;

    const std::array<std::pair<ArtifactCore::ViewOrientationHotspot, QRectF>, 6>
        faces = {{{ArtifactCore::ViewOrientationHotspot::Top,
                   QRectF(leftX, topY, faceW, faceH)},
                  {ArtifactCore::ViewOrientationHotspot::Back,
                   QRectF(rightX, topY, faceW, faceH)},
                  {ArtifactCore::ViewOrientationHotspot::Left,
                   QRectF(leftX, row2Y, faceW, faceH)},
                  {ArtifactCore::ViewOrientationHotspot::Front,
                   QRectF(rightX, row2Y, faceW, faceH)},
                  {ArtifactCore::ViewOrientationHotspot::Right,
                   QRectF(leftX, row3Y, faceW, faceH)},
                  {ArtifactCore::ViewOrientationHotspot::Bottom,
                   QRectF(rightX, row3Y, faceW, faceH)}}};

    for (const auto &[hotspot, faceRect] : faces) {
      if (faceRect.contains(pos)) {
        return hotspot;
      }
    }
    return ArtifactCore::ViewOrientationHotspot::None;
  }

  ArtifactCore::ViewOrientationNavigator navigator_;
  ArtifactCore::ViewOrientationHotspot hotspot_ =
      ArtifactCore::ViewOrientationHotspot::Front;
  ArtifactCore::ViewOrientationHotspot hoverHotspot_ =
      ArtifactCore::ViewOrientationHotspot::None;
  std::function<void(ArtifactCore::ViewOrientationHotspot)> activatedCallback_;
};
} // namespace

class ArtifactCompositionEditor::Impl {
public:
  CompositionViewport *compositionView_ = nullptr;
  CompositionOverlayWidget *overlayView_ = nullptr;
  ViewOrientationWidget *viewOrientationWidget_ = nullptr;
  CompositionRenderController *renderController_ = nullptr;
  ArtifactPieMenuWidget *pieMenu_ = nullptr;

  // Top Toolbar (Zoom/View controls)
  QToolBar *topToolbar_ = nullptr;
  QAction *resetAction_ = nullptr;
  QAction *zoomInAction_ = nullptr;
  QAction *zoomOutAction_ = nullptr;
  QAction *zoomFitAction_ = nullptr;
  QAction *zoom100Action_ = nullptr;
  QAction *editTextAction_ = nullptr;
  QAction *motionPathAction_ = nullptr;
  QToolButton *toolModeButton_ = nullptr;
  QToolButton *gizmoModeButton_ = nullptr;
  QToolButton *pivotModeButton_ = nullptr;
  QAction *immersiveAction_ = nullptr;
  bool immersiveMode_ = false;

  // Bottom Viewer Controls
  QWidget *bottomBar_ = nullptr;
  QComboBox *resolutionCombo_ = nullptr;
  QToolButton *fastPreviewBtn_ = nullptr;
  QToolButton *displayOptionsBtn_ = nullptr;

  bool selectionSyncQueued_ = false;
  bool toolLabelSyncQueued_ = false;
  ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;
  ProfilerOverlayWidget *profilerOverlay_ = nullptr;
  ProfilerPanelWidget *profilerPanel_ = nullptr;
  EventBusDebuggerWidget *eventBusDebugger_ = nullptr;
  int startupCompositionRetryCount_ = 0;

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

  void syncSelectionState(ArtifactCompositionEditor *owner) {
    if (!owner || !renderController_) {
      return;
    }
    auto *app = ArtifactApplicationManager::instance();
    auto *selection = app ? app->layerSelectionManager() : nullptr;
    const auto current =
        selection ? selection->currentLayer() : ArtifactAbstractLayerPtr{};
    const int selectedCount =
        selection ? selection->selectedLayers().size() : 0;

    // Guard: if the selection manager reports no current layer, but the
    // previously-selected layer still exists in the composition, preserve the
    // gizmo. This prevents spurious selection clears triggered by deferred
    // syncSelectionState() calls that race with property-edit notifications.
    if (!current) {
      const auto prevId = renderController_->selectedLayerId();
      if (!prevId.isNil()) {
        const auto comp = renderController_->composition();
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
      const QString detail =
          selectedCount <= 1
              ? QStringLiteral("Layer selected")
              : QStringLiteral("%1 layers selected").arg(selectedCount);
      renderController_->setInfoOverlayText(title, detail);
    } else {
      renderController_->setInfoOverlayText(
          QStringLiteral("Composition Editor"),
          selectedCount <= 0
              ? QStringLiteral("No layer selected")
              : QStringLiteral("%1 layers selected").arg(selectedCount));
    }
    if (editTextAction_) {
      editTextAction_->setEnabled(
          selectedCount == 1 && current &&
          std::dynamic_pointer_cast<ArtifactTextLayer>(current));
    }
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
    renderController_->renderOneFrame();
    startupCompositionRetryCount_ = 0;
    return true;
  }

  void syncOverlayGeometry(ArtifactCompositionEditor *owner) {
    if (!owner || !overlayView_ || !compositionView_) {
      return;
    }
    overlayView_->setGeometry(compositionView_->geometry());
    if (overlayView_->isVisible()) {
      overlayView_->raise();
    }
    if (viewOrientationWidget_) {
      const QSize sz = viewOrientationWidget_->sizeHint();
      const int x = std::max(12, overlayView_->width() - sz.width() - 12);
      const int y = 12;
      viewOrientationWidget_->setGeometry(x, y, sz.width(), sz.height());
      const bool showNavigator = renderController_ != nullptr;
      viewOrientationWidget_->setVisible(showNavigator);
      viewOrientationWidget_->setEnabledState(showNavigator);
      if (viewOrientationWidget_->isVisible()) {
        viewOrientationWidget_->raise();
      }
    }
    if (profilerOverlay_) {
      const QRect vg = compositionView_->geometry();
      const QSize ps = profilerOverlay_->size();
      profilerOverlay_->move(vg.left() + 8, vg.top() + 8);
      if (profilerOverlay_->isVisible()) {
        profilerOverlay_->raise();
      }
      Q_UNUSED(ps);
    }
  }

  void toggleImmersiveMode(ArtifactCompositionEditor *owner, bool immersive) {
    if (!owner) {
      return;
    }
    immersiveMode_ = immersive;
    if (auto *mw = qobject_cast<ArtifactMainWindow *>(owner->window())) {
      mw->setDockImmersive(owner, immersive);
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
};

ArtifactCompositionEditor::ArtifactCompositionEditor(QWidget *parent)
    : QWidget(parent), impl_(new Impl()) {
  setMinimumSize(0, 0);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setAutoFillBackground(true);

  const auto theme = ArtifactCore::currentDCCTheme();
  QPalette editorPalette = palette();
  editorPalette.setColor(QPalette::Window, QColor(theme.backgroundColor));
  editorPalette.setColor(QPalette::WindowText, QColor(theme.textColor));
  setPalette(editorPalette);

  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  impl_->renderController_ = new CompositionRenderController(this);
  {
    const QColor clear(28, 40, 56);
    impl_->renderController_->setClearColor(
        {clear.redF(), clear.greenF(), clear.blueF(), 1.0f});
  }

  QObject::connect(impl_->renderController_,
                   &CompositionRenderController::videoDebugMessage, this,
                   &ArtifactCompositionEditor::videoDebugMessage);

  impl_->compositionView_ =
      new CompositionViewport(impl_->renderController_, this);
  impl_->overlayView_ =
      new CompositionOverlayWidget(impl_->compositionView_, this);
  impl_->compositionView_->setOverlayWidget(impl_->overlayView_);
  impl_->overlayView_->hide();
  impl_->viewOrientationWidget_ =
      new ViewOrientationWidget(impl_->overlayView_);
  impl_->viewOrientationWidget_->setActivatedCallback(
      [this](ArtifactCore::ViewOrientationHotspot hotspot) {
        if (impl_->renderController_) {
          impl_->renderController_->setViewportOrientation(hotspot);
        }
      });
  impl_->viewOrientationWidget_->show();

  impl_->pieMenu_ = new ArtifactPieMenuWidget(this);
  impl_->compositionView_->setPieMenu(impl_->pieMenu_);

  // Top Toolbar
  impl_->topToolbar_ = new QToolBar(this);
  impl_->topToolbar_->setMovable(false);
  impl_->topToolbar_->setIconSize(QSize(18, 18));
  {
    QPalette pal = impl_->topToolbar_->palette();
    pal.setColor(QPalette::Window, QColor(theme.secondaryBackgroundColor));
    pal.setColor(QPalette::Button, QColor(theme.secondaryBackgroundColor));
    pal.setColor(QPalette::WindowText, QColor(theme.textColor));
    impl_->topToolbar_->setPalette(pal);
  }

  impl_->resetAction_ = impl_->topToolbar_->addAction("Reset");
  impl_->topToolbar_->addSeparator();
  impl_->zoomInAction_ = impl_->topToolbar_->addAction("Zoom+");
  impl_->zoomOutAction_ = impl_->topToolbar_->addAction("Zoom-");
  impl_->zoomFitAction_ = impl_->topToolbar_->addAction("Fill");
  impl_->zoom100Action_ = impl_->topToolbar_->addAction("100%");
  impl_->editTextAction_ = impl_->topToolbar_->addAction("Edit Text");
  impl_->editTextAction_->setToolTip(QStringLiteral("Edit current text layer"));
  impl_->editTextAction_->setShortcut(QKeySequence(Qt::Key_F2));
  impl_->motionPathAction_ = impl_->topToolbar_->addAction("Motion Path");
  impl_->motionPathAction_->setCheckable(true);
  impl_->motionPathAction_->setChecked(false);
  impl_->motionPathAction_->setToolTip(
      QStringLiteral("Show motion path overlay for the selected layer"));

  auto *toolMenu = new QMenu(this);
  auto *toolGroup = new QActionGroup(this);
  toolGroup->setExclusive(true);
  const auto addToolAction = [&](const QString &text, const QString &iconName,
                                 ToolType toolType, bool checked) {
    QAction *action = toolMenu->addAction(loadIconWithFallback(iconName), text);
    action->setCheckable(true);
    action->setChecked(checked);
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
  impl_->toolModeButton_ = new QToolButton(this);
  impl_->toolModeButton_->setText(QStringLiteral("Select"));
  impl_->toolModeButton_->setMenu(toolMenu);
  impl_->toolModeButton_->setPopupMode(QToolButton::InstantPopup);
  impl_->topToolbar_->addWidget(impl_->toolModeButton_);

  auto *gizmoMenu = new QMenu(this);
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
  impl_->gizmoModeButton_ = new QToolButton(this);
  impl_->gizmoModeButton_->setText(QStringLiteral("Gizmo W/R/S"));
  impl_->gizmoModeButton_->setMenu(gizmoMenu);
  impl_->gizmoModeButton_->setIcon(
      loadEditorMenuIcon(QStringLiteral("MaterialVS/neutral/transform.svg")));
  impl_->gizmoModeButton_->setToolTip(
      QStringLiteral("W = Move, R = Rotate, S = Scale"));
  impl_->gizmoModeButton_->setPopupMode(QToolButton::InstantPopup);
  impl_->topToolbar_->addWidget(impl_->gizmoModeButton_);

  auto *pivotMenu = new QMenu(this);
  auto *pivotGroup = new QActionGroup(this);
  pivotGroup->setExclusive(true);
  const auto applyPivotPreset = [this](const bool useCenter) {
    auto *app = ArtifactApplicationManager::instance();
    auto *selection = app ? app->layerSelectionManager() : nullptr;
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
    const ArtifactCore::RationalTime time(layer->currentFrame(), 30);
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
    impl_->renderController_->renderOneFrame();
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

  // Bottom Bar (Viewer Controls)
  impl_->bottomBar_ = new QWidget(this);
  impl_->bottomBar_->setFixedHeight(28);
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
  impl_->resolutionCombo_->setFixedWidth(70);
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
  QAction *solidBgAct = displayMenu->addAction("Solid");
  QAction *solidColorAct = displayMenu->addAction("Solid Color...");
  QAction *checkerboardAct = displayMenu->addAction("Checkerboard");
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
  displayMenu->addSeparator();
  QAction *gpuBlendAct = displayMenu->addAction("GPU Blend Path");
  checkerboardAct->setCheckable(true);
  gridAct->setCheckable(true);
  guidesAct->setCheckable(true);
  safeMarginsAct->setCheckable(true);
  anchorCenterAct->setCheckable(true);
  gpuBlendAct->setCheckable(true);
  anchorCenterAct->setToolTip(
      QStringLiteral("Show the selected layer anchor point and center point"));
  gpuBlendAct->setToolTip(
      QStringLiteral("Enable the compute-shader blend path when the composition needs masks, non-normal blending, or rasterizer effects"));
  gpuBlendAct->setStatusTip(
      QStringLiteral("Toggle the experimental GPU blend path"));
  impl_->displayOptionsBtn_->setMenu(displayMenu);

  // Connect actions
  QObject::connect(solidBgAct, &QAction::triggered, this, [this]() {
    if (impl_->renderController_) {
      impl_->renderController_->setCompositionBackgroundMode(
          static_cast<int>(CompositionBackgroundMode::Solid));
    }
  });
  QObject::connect(solidColorAct, &QAction::triggered, this, [this]() {
    if (!impl_->renderController_) {
      return;
    }
    const QColor initial = QColor::fromRgbF(
        impl_->renderController_->clearColor().r(),
        impl_->renderController_->clearColor().g(),
        impl_->renderController_->clearColor().b(),
        impl_->renderController_->clearColor().a());
    const QColor chosen = QColorDialog::getColor(
        initial, this, QStringLiteral("Choose Solid Background Color"),
        QColorDialog::ShowAlphaChannel);
    if (!chosen.isValid()) {
      return;
    }
    impl_->renderController_->setClearColor(
        FloatColor(chosen.redF(), chosen.greenF(), chosen.blueF(),
                   chosen.alphaF()));
    impl_->renderController_->setCompositionBackgroundMode(
        static_cast<int>(CompositionBackgroundMode::Solid));
  });
  QObject::connect(checkerboardAct, &QAction::triggered, this, [this]() {
    if (impl_->renderController_) {
      impl_->renderController_->setCompositionBackgroundMode(
          static_cast<int>(CompositionBackgroundMode::Checkerboard));
    }
  });
  QObject::connect(mayaBgAct, &QAction::triggered, this, [this]() {
    if (impl_->renderController_) {
      impl_->renderController_->setCompositionBackgroundMode(
          static_cast<int>(CompositionBackgroundMode::MayaGradient));
    }
  });
  QObject::connect(gridAct, &QAction::toggled, this, [this](bool checked) {
    if (impl_->renderController_)
      impl_->renderController_->setShowGrid(checked);
  });
  QObject::connect(guidesAct, &QAction::toggled, this, [this](bool checked) {
    if (impl_->renderController_)
      impl_->renderController_->setShowGuides(checked);
  });
  QObject::connect(safeMarginsAct, &QAction::toggled, this,
                   [this](bool checked) {
                     if (impl_->renderController_)
                       impl_->renderController_->setShowSafeMargins(checked);
                   });
  QObject::connect(anchorCenterAct, &QAction::toggled, this,
                   [this](bool checked) {
                     if (impl_->renderController_)
                       impl_->renderController_->setShowAnchorCenterOverlay(checked);
                   });
  QObject::connect(gpuBlendAct, &QAction::toggled, this, [this](bool checked) {
    if (impl_->renderController_) {
      impl_->renderController_->setGpuBlendEnabled(checked);
    }
  });

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
    gpuBlendAct->setChecked(impl_->renderController_->isGpuBlendEnabled());
  }

  bottomLayout->addWidget(impl_->resolutionCombo_);
  bottomLayout->addWidget(impl_->fastPreviewBtn_);
  bottomLayout->addWidget(impl_->displayOptionsBtn_);
  bottomLayout->addStretch();

  // Assembly
  mainLayout->addWidget(impl_->topToolbar_);
  mainLayout->addWidget(impl_->compositionView_, 1);
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
    auto *app = ArtifactApplicationManager::instance();
    auto *selection = app ? app->layerSelectionManager() : nullptr;
    const auto layer =
        selection ? selection->currentLayer() : ArtifactAbstractLayerPtr{};
    if (!layer || !std::dynamic_pointer_cast<ArtifactTextLayer>(layer)) {
      return;
    }
    if (editTextLayerInline(impl_->compositionView_, layer,
                            impl_->renderController_) &&
        impl_->renderController_) {
      impl_->renderController_->renderOneFrame();
    }
  });
  QObject::connect(
      impl_->motionPathAction_, &QAction::toggled, this, [this](bool checked) {
        if (impl_->renderController_) {
          impl_->renderController_->setShowMotionPathOverlay(checked);
        }
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
    syncResolutionCombo(service->previewQualityPreset());
    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<PreviewQualityPresetChangedEvent>(
            [this, syncResolutionCombo](
                const PreviewQualityPresetChangedEvent &event) {
              syncResolutionCombo(
                  static_cast<PreviewQualityPreset>(event.preset));
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
  QObject::connect(profilerCopyShortcut, &QShortcut::activated, this, [this]() {
    if (!ArtifactCore::Profiler::instance().isEnabled())
      return;
    const std::string report =
        ArtifactCore::Profiler::instance().generateDiagnosticReport(60);
    QGuiApplication::clipboard()->setText(QString::fromStdString(report));
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
      return true;
    case CompositionEditorDeferredEvent::Kind::ToolLabelSync:
      impl_->toolLabelSyncQueued_ = false;
      impl_->syncToolLabel(this);
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
  }
  if (auto *playback = ArtifactPlaybackService::instance()) {
    playback->setCurrentComposition(composition);
  }
  if (impl_->compositionView_ && !sameCompositionPointer && !sameCompositionId) {
    impl_->compositionView_->requestInitialFit();
  }
  if (impl_) {
    impl_->queueSelectionSync(this);
  }
}

void ArtifactCompositionEditor::setClearColor(const FloatColor &color) {
  if (impl_->renderController_) {
    impl_->renderController_->setClearColor(color);
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
}

void ArtifactCompositionEditor::stop() {
  if (auto *playback = ArtifactPlaybackService::instance()) {
    playback->stop();
  }
  if (impl_->renderController_) {
    impl_->renderController_->stop();
  }
}

void ArtifactCompositionEditor::resetView() {
  if (impl_->renderController_) {
    impl_->renderController_->resetView();
  }
}

void ArtifactCompositionEditor::zoomIn() {
  if (impl_->renderController_ && impl_->compositionView_) {
    impl_->renderController_->zoomInAt(
        QPointF(impl_->compositionView_->width() * 0.5,
                impl_->compositionView_->height() * 0.5));
  }
}

void ArtifactCompositionEditor::zoomOut() {
  if (impl_->renderController_ && impl_->compositionView_) {
    impl_->renderController_->zoomOutAt(
        QPointF(impl_->compositionView_->width() * 0.5,
                impl_->compositionView_->height() * 0.5));
  }
}

void ArtifactCompositionEditor::zoomFit() {
  if (impl_->renderController_) {
    impl_->renderController_->zoomFit();
  }
}

void ArtifactCompositionEditor::zoomFill() {
  if (impl_->renderController_) {
    impl_->renderController_->zoomFill();
  }
}

void ArtifactCompositionEditor::zoom100() {
  if (impl_->renderController_) {
    impl_->renderController_->zoom100();
  }
}

} // namespace Artifact
