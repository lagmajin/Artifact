module;
#ifdef _WIN32
#include <windows.h>
#include <wincodec.h>
#include <shobjidl.h>
#include <wrl/client.h>
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "gdi32.lib")
#endif
#include <utility>
#include <functional>
#include <QFileSystemModel>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QWidget>
#include <QListView>
#include <QListWidget>
#include <QLayoutItem>
#include <QToolButton>
#include <QPushButton>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QSortFilterProxyModel>
#include <QDrag>
#include <QButtonGroup>
#include <QPixmap>
#include <QIcon>
#include <QHash>
#include <QFileInfo>
#include <QStyle>
#include <QApplication>
#include <QEvent>
#include <QMenu>
#include <QDesktopServices>
#include <QUrl>
#include <QFileDialog>
#include <QFocusEvent>
#include <QFont>
#include <QColor>
#include <QClipboard>
#include <QApplication>
#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QPoint>
#include <QRect>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QThread>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imageio.h>
#include <opencv2/opencv.hpp>
#include <QSlider>
#include <QFrame>
#include <QGridLayout>
#include <QElapsedTimer>
#include <QSet>
#include <QHBoxLayout>
#include <QAbstractItemView>
#include <QComboBox>
#include <QMouseEvent>
#include <QCursor>
#include <cstdint>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <vector>
#include <wobjectimpl.h>

// Async waveform thumbnail
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QCache>

// Audio waveform includes
#include <QAudioFormat>
#include <QAudioDecoder>
#ifdef emit
#pragma push_macro("emit")
#undef emit
#define ARTIFACT_RESTORE_QT_EMIT_MACRO
#endif
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#ifdef ARTIFACT_RESTORE_QT_EMIT_MACRO
#pragma pop_macro("emit")
#undef ARTIFACT_RESTORE_QT_EMIT_MACRO
#endif

module Widgets.AssetBrowser;

import Widgets.Utils.CSS;
import Artifact.Service.Project;
import Artifact.Application.Manager;
import Artifact.Service.FootageInterpret;
import Widgets.Dialog.InterpretFootage;
import Artifact.Event.Types;
import Event.Bus;
import Artifact.Project.Manager;
import Artifact.Project.Cleanup;
import Artifact.Project.PresetManager;
import Artifact.Mask.LayerMask;
import Asset.Manager;
import AssetMenuModel;
import AssetDirectoryModel;
import Utils.String.UniString;
import Media.SourceInterpret;
import File.TypeDetector;
import Codec.Thumbnail.FFmpeg;
import Artifact.Audio.Waveform;
import Audio.Segment;
import Audio.SimpleWav;
import Input.Operator;

namespace Artifact {

using namespace ArtifactCore;

namespace {
constexpr int kAssetThumbnailMinPx = 25;
constexpr int kAssetThumbnailMaxPx = 256;
constexpr int kAssetThumbnailDefaultPx = 128;
constexpr auto kAssetBrowserContext = "Panel.AssetBrowser";

void applyAssetBrowserPanelPalette(QWidget* widget)
{
  if (!widget) {
    return;
  }

  QPalette pal = widget->palette();
  const auto& theme = ArtifactCore::currentDCCTheme();
  const QColor surface(18, 23, 29);
  const QColor base(12, 17, 22);
  pal.setColor(QPalette::Window, surface);
  pal.setColor(QPalette::Base, base);
  pal.setColor(QPalette::AlternateBase, QColor(24, 30, 37));
  pal.setColor(QPalette::WindowText, QColor(theme.textColor));
  pal.setColor(QPalette::Text, QColor(theme.textColor));
  pal.setColor(QPalette::Button, surface);
  pal.setColor(QPalette::ButtonText, QColor(theme.textColor));
  pal.setColor(QPalette::Mid, QColor(49, 58, 68));
  widget->setAutoFillBackground(true);
  widget->setPalette(pal);
}

void applyAssetBrowserFilterPalette(QAbstractButton* button)
{
  if (!button) {
    return;
  }
  const auto& theme = ArtifactCore::currentDCCTheme();
  QPalette pal = button->palette();
  const QColor text(theme.textColor);
  const QColor surface(theme.secondaryBackgroundColor);
  const QColor accent(theme.accentColor);
  pal.setColor(QPalette::Button, surface.darker(108));
  pal.setColor(QPalette::ButtonText, text);
  pal.setColor(QPalette::Highlight, accent);
  pal.setColor(QPalette::HighlightedText, QColor(Qt::white));
  button->setAutoFillBackground(true);
  button->setPalette(pal);
}

QFrame* makeAssetBrowserPanel(QWidget* parent = nullptr)
{
  auto* frame = new QFrame(parent);
  frame->setFrameShape(QFrame::StyledPanel);
  frame->setFrameShadow(QFrame::Plain);
  applyAssetBrowserPanelPalette(frame);
  return frame;
}

class RecentFolderButton final : public QToolButton {
 public:
  explicit RecentFolderButton(QWidget* parent = nullptr) : QToolButton(parent) {
    setAutoRaise(true);
    setCursor(Qt::PointingHandCursor);
   setIcon(QApplication::style()->standardIcon(QStyle::SP_DirIcon));
   setIconSize(QSize(16, 16));
   setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    setMinimumHeight(28);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QPalette pal = palette();
    const auto& theme = ArtifactCore::currentDCCTheme();
    pal.setColor(QPalette::Button, QColor(theme.secondaryBackgroundColor).darker(108));
    pal.setColor(QPalette::ButtonText, QColor(theme.textColor));
    setAutoFillBackground(true);
    setPalette(pal);
  }

  void setEntry(const QString& text, const QString& path, std::function<void(const QString&)> activate) {
    text_ = text;
    path_ = path;
    activate_ = std::move(activate);
    setText(text_.isEmpty() ? QStringLiteral("(unnamed)") : text_);
    setToolTip(path_.isEmpty() ? text_ : path_);
    setVisible(!path_.isEmpty());
  }

 protected:
  void mouseReleaseEvent(QMouseEvent* event) override {
    QToolButton::mouseReleaseEvent(event);
    if (event && event->button() == Qt::LeftButton && activate_ && !path_.isEmpty()) {
      activate_(path_);
    }
  }

 private:
  QString text_;
 QString path_;
  std::function<void(const QString&)> activate_;
};

class HoverPreviewPopup final : public QFrame {
 public:
  explicit HoverPreviewPopup(QWidget* parent = nullptr) : QFrame(parent) {
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
    setFrameShape(QFrame::StyledPanel);
    setFrameShadow(QFrame::Plain);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    title_ = new QLabel(this);
    title_->setWordWrap(true);
    preview_ = new QLabel(this);
    preview_->setAlignment(Qt::AlignCenter);
    preview_->setMinimumSize(320, 180);
    preview_->setScaledContents(false);
    details_ = new QLabel(this);
    details_->setWordWrap(true);
    layout->addWidget(title_);
    layout->addWidget(preview_);
    layout->addWidget(details_);
  }

  void showFile(const QString& filePath, const QPoint& globalPos, const QIcon& icon, const QFileInfo& info) {
    const QSize targetSize(480, 270);
    QPixmap pixmap = icon.isNull() ? QPixmap() : icon.pixmap(targetSize);
    if (pixmap.isNull()) {
      pixmap = QPixmap(targetSize);
      pixmap.fill(Qt::transparent);
    }
    preview_->setPixmap(pixmap.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    title_->setText(info.fileName().isEmpty() ? filePath : info.fileName());
    details_->setText(QStringLiteral("%1\n%2")
                          .arg(info.exists() ? info.absoluteFilePath() : QStringLiteral("Missing"))
                          .arg(info.isDir() ? QStringLiteral("Folder") : info.suffix().toUpper()));
    adjustSize();
    move(globalPos + QPoint(18, 18));
    show();
    raise();
  }

 private:
  QLabel* title_ = nullptr;
  QLabel* preview_ = nullptr;
  QLabel* details_ = nullptr;
};

#ifdef _WIN32
using Microsoft::WRL::ComPtr;

QImage loadImageThumbnailViaWIC(const QString& filePath,
                                const QSize& targetSize = QSize(),
                                QString* errorOut = nullptr)
{
  const std::wstring widePath = filePath.toStdWString();
  const HRESULT initHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  const bool comReady = SUCCEEDED(initHr) || initHr == RPC_E_CHANGED_MODE;
  const bool shouldUninitialize = SUCCEEDED(initHr);
  if (!comReady) {
    if (errorOut) {
      *errorOut = QStringLiteral("CoInitializeEx failed: 0x%1")
                      .arg(static_cast<qulonglong>(initHr), 0, 16);
    }
    return {};
  }

  struct CoUninitializeScope {
    bool enabled = false;
    ~CoUninitializeScope() {
      if (enabled) {
        CoUninitialize();
      }
    }
  } cleanup{shouldUninitialize};

  ComPtr<IWICImagingFactory> factory;
  HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory2, nullptr,
                                CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
  if (FAILED(hr)) {
    hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                          CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
  }
  if (FAILED(hr) || !factory) {
    if (errorOut) {
      *errorOut = QStringLiteral("WIC factory creation failed: 0x%1")
                      .arg(static_cast<qulonglong>(hr), 0, 16);
    }
    return {};
  }

  ComPtr<IWICBitmapDecoder> decoder;
  hr = factory->CreateDecoderFromFilename(
      widePath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand,
      &decoder);
  if (FAILED(hr) || !decoder) {
    if (errorOut) {
      *errorOut = QStringLiteral("WIC decoder open failed: 0x%1")
                      .arg(static_cast<qulonglong>(hr), 0, 16);
    }
    return {};
  }

  ComPtr<IWICBitmapFrameDecode> frame;
  hr = decoder->GetFrame(0, &frame);
  if (FAILED(hr) || !frame) {
    if (errorOut) {
      *errorOut = QStringLiteral("WIC GetFrame failed: 0x%1")
                      .arg(static_cast<qulonglong>(hr), 0, 16);
    }
    return {};
  }

  UINT width = 0;
  UINT height = 0;
  hr = frame->GetSize(&width, &height);
  if (FAILED(hr) || width == 0 || height == 0) {
    if (errorOut) {
      *errorOut = QStringLiteral("WIC GetSize failed: 0x%1")
                      .arg(static_cast<qulonglong>(hr), 0, 16);
    }
    return {};
  }

  UINT targetWidth = width;
  UINT targetHeight = height;
  if (targetSize.isValid() && !targetSize.isEmpty()) {
    const double sx = static_cast<double>(targetSize.width()) /
                      static_cast<double>(std::max<UINT>(1, width));
    const double sy = static_cast<double>(targetSize.height()) /
                      static_cast<double>(std::max<UINT>(1, height));
    const double scale = std::min(sx, sy);
    if (scale > 0.0) {
      targetWidth = std::max<UINT>(1, static_cast<UINT>(std::lround(width * scale)));
      targetHeight =
          std::max<UINT>(1, static_cast<UINT>(std::lround(height * scale)));
    }
  }

  ComPtr<IWICBitmapSource> bitmapSource = frame;
  ComPtr<IWICBitmapScaler> scaler;
  if (targetWidth != width || targetHeight != height) {
    hr = factory->CreateBitmapScaler(&scaler);
    if (FAILED(hr) || !scaler) {
      if (errorOut) {
        *errorOut = QStringLiteral("WIC scaler creation failed: 0x%1")
                        .arg(static_cast<qulonglong>(hr), 0, 16);
      }
      return {};
    }
    hr = scaler->Initialize(frame.Get(), targetWidth, targetHeight,
                            WICBitmapInterpolationModeFant);
    if (FAILED(hr)) {
      if (errorOut) {
        *errorOut = QStringLiteral("WIC scaler init failed: 0x%1")
                        .arg(static_cast<qulonglong>(hr), 0, 16);
      }
      return {};
    }
    bitmapSource = scaler;
  }

  ComPtr<IWICFormatConverter> converter;
  hr = factory->CreateFormatConverter(&converter);
  if (FAILED(hr) || !converter) {
    if (errorOut) {
      *errorOut = QStringLiteral("WIC converter creation failed: 0x%1")
                      .arg(static_cast<qulonglong>(hr), 0, 16);
    }
    return {};
  }

  hr = converter->Initialize(bitmapSource.Get(), GUID_WICPixelFormat32bppRGBA,
                             WICBitmapDitherTypeNone, nullptr, 0.0,
                             WICBitmapPaletteTypeCustom);
  if (FAILED(hr)) {
    if (errorOut) {
      *errorOut = QStringLiteral("WIC converter init failed: 0x%1")
                      .arg(static_cast<qulonglong>(hr), 0, 16);
    }
    return {};
  }

  QImage image(static_cast<int>(targetWidth), static_cast<int>(targetHeight),
               QImage::Format_RGBA8888);
  if (image.isNull()) {
    if (errorOut) {
      *errorOut = QStringLiteral("Failed to allocate WIC thumbnail image.");
    }
    return {};
  }

  const UINT stride = targetWidth * 4;
  const UINT bytes = stride * targetHeight;
  hr = converter->CopyPixels(nullptr, stride, bytes, image.bits());
  if (FAILED(hr)) {
    if (errorOut) {
      *errorOut = QStringLiteral("WIC CopyPixels failed: 0x%1")
                      .arg(static_cast<qulonglong>(hr), 0, 16);
    }
    return {};
  }

  return image;
}

QImage imageFromShellBitmap(HBITMAP bitmap, QString* errorOut = nullptr)
{
  if (!bitmap) {
    if (errorOut) {
      *errorOut = QStringLiteral("Shell thumbnail returned a null bitmap.");
    }
    return {};
  }

  BITMAP bitmapInfo{};
  if (::GetObject(bitmap, sizeof(bitmapInfo), &bitmapInfo) == 0 ||
      bitmapInfo.bmWidth <= 0 || bitmapInfo.bmHeight <= 0) {
    if (errorOut) {
      *errorOut = QStringLiteral("GetObject failed for Shell thumbnail bitmap.");
    }
    return {};
  }

  QImage image(bitmapInfo.bmWidth, bitmapInfo.bmHeight,
               QImage::Format_ARGB32);
  if (image.isNull()) {
    if (errorOut) {
      *errorOut = QStringLiteral("Failed to allocate Shell thumbnail image.");
    }
    return {};
  }

  BITMAPINFO dibInfo{};
  dibInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  dibInfo.bmiHeader.biWidth = bitmapInfo.bmWidth;
  dibInfo.bmiHeader.biHeight = -bitmapInfo.bmHeight;
  dibInfo.bmiHeader.biPlanes = 1;
  dibInfo.bmiHeader.biBitCount = 32;
  dibInfo.bmiHeader.biCompression = BI_RGB;

  HDC dc = ::GetDC(nullptr);
  const int rows = dc ? ::GetDIBits(dc, bitmap, 0,
                                    static_cast<UINT>(bitmapInfo.bmHeight),
                                    image.bits(), &dibInfo, DIB_RGB_COLORS)
                      : 0;
  if (dc) {
    ::ReleaseDC(nullptr, dc);
  }
  if (rows == 0) {
    if (errorOut) {
      *errorOut = QStringLiteral("GetDIBits failed for Shell thumbnail bitmap.");
    }
    return {};
  }

  bool hasAlpha = false;
  for (int y = 0; y < image.height() && !hasAlpha; ++y) {
    const auto *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
    for (int x = 0; x < image.width(); ++x) {
      if (qAlpha(line[x]) != 0) {
        hasAlpha = true;
        break;
      }
    }
  }
  if (!hasAlpha) {
    for (int y = 0; y < image.height(); ++y) {
      auto *line = reinterpret_cast<QRgb *>(image.scanLine(y));
      for (int x = 0; x < image.width(); ++x) {
        line[x] = qRgba(qRed(line[x]), qGreen(line[x]), qBlue(line[x]), 255);
      }
    }
  }

  return image;
}

QImage loadImageThumbnailViaWindowsShell(const QString& filePath,
                                         const QSize& targetSize = QSize(),
                                         QString* errorOut = nullptr)
{
  const std::wstring widePath = filePath.toStdWString();
  const HRESULT initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  const bool comReady = SUCCEEDED(initHr) || initHr == RPC_E_CHANGED_MODE;
  const bool shouldUninitialize = SUCCEEDED(initHr);
  if (!comReady) {
    if (errorOut) {
      *errorOut = QStringLiteral("CoInitializeEx failed for Shell thumbnail: 0x%1")
                      .arg(static_cast<qulonglong>(initHr), 0, 16);
    }
    return {};
  }

  struct CoUninitializeScope {
    bool enabled = false;
    ~CoUninitializeScope() {
      if (enabled) {
        CoUninitialize();
      }
    }
  } cleanup{shouldUninitialize};

  ComPtr<IShellItemImageFactory> imageFactory;
  HRESULT hr = SHCreateItemFromParsingName(
      widePath.c_str(), nullptr, IID_PPV_ARGS(&imageFactory));
  if (FAILED(hr) || !imageFactory) {
    if (errorOut) {
      *errorOut = QStringLiteral("SHCreateItemFromParsingName failed: 0x%1")
                      .arg(static_cast<qulonglong>(hr), 0, 16);
    }
    return {};
  }

  const int width = targetSize.width() > 0 ? targetSize.width() : 256;
  const int height = targetSize.height() > 0 ? targetSize.height() : width;
  const SIZE shellSize{width, height};
  HBITMAP bitmap = nullptr;
  hr = imageFactory->GetImage(
      shellSize,
      static_cast<SIIGBF>(SIIGBF_THUMBNAILONLY | SIIGBF_BIGGERSIZEOK),
      &bitmap);
  if (FAILED(hr) || !bitmap) {
    hr = imageFactory->GetImage(
        shellSize,
        static_cast<SIIGBF>(SIIGBF_THUMBNAILONLY | SIIGBF_RESIZETOFIT),
        &bitmap);
  }
  if (FAILED(hr) || !bitmap) {
    if (errorOut) {
      *errorOut = QStringLiteral("IShellItemImageFactory::GetImage failed: 0x%1")
                      .arg(static_cast<qulonglong>(hr), 0, 16);
    }
    return {};
  }

  QImage image = imageFromShellBitmap(bitmap, errorOut);
  ::DeleteObject(bitmap);
  if (!image.isNull() && targetSize.isValid() &&
      (image.width() > targetSize.width() ||
       image.height() > targetSize.height())) {
    image = image.scaled(targetSize, Qt::KeepAspectRatio,
                         Qt::SmoothTransformation);
  }
  return image;
}
#endif

QImage loadImageThumbnailViaOIIO(const QString& filePath, const QSize& targetSize = QSize(), QString* errorOut = nullptr)
{
  try {
    const QByteArray utf8 = filePath.toUtf8();
    OIIO::ImageBuf source(utf8.constData());
    if (!source.read(0, 0, true, OIIO::TypeDesc::UINT8)) {
      if (errorOut) {
        *errorOut = QStringLiteral("read failed: %1").arg(QString::fromStdString(source.geterror()));
      }
      return {};
    }

    OIIO::ImageBuf oriented = OIIO::ImageBufAlgo::reorient(source);
    const OIIO::ImageSpec& spec = oriented.spec();
    const int width = std::max(1, spec.width);
    const int height = std::max(1, spec.height);

    QImage image(width, height, QImage::Format_RGBA8888);
    if (image.isNull()) {
      if (errorOut) {
        *errorOut = QStringLiteral("Failed to allocate thumbnail image.");
      }
      return {};
    }

    OIIO::ImageBuf rgba;
    const int channelCount = spec.nchannels;
    std::vector<int> channelOrder = {0, 1, 2, 3};
    std::vector<float> channelValues = {0.0f, 0.0f, 0.0f, 1.0f};
    if (channelCount == 1) {
      // Grayscale: replicate R channel into G and B, set A=1
      channelOrder = {0, 0, 0, -1};
      channelValues = {0.0f, 0.0f, 0.0f, 1.0f};
      rgba = OIIO::ImageBufAlgo::channels(oriented, 4, channelOrder, channelValues);
    } else if (channelCount == 2) {
      // 2-channel inputs still need an explicit alpha default or OIIO fills it with 0.
      rgba = OIIO::ImageBufAlgo::channels(oriented, 4, channelOrder, channelValues);
    } else if (channelCount == 3) {
      // 3-channel inputs also need alpha=1 so thumbnails stay visible.
      rgba = OIIO::ImageBufAlgo::channels(oriented, 4, channelOrder, channelValues);
    } else if (channelCount >= 4) {
      rgba = OIIO::ImageBufAlgo::channels(oriented, 4, channelOrder);
    } else {
      rgba = OIIO::ImageBufAlgo::channels(oriented, 4, channelOrder, channelValues);
    }

    if (!rgba.get_pixels(OIIO::ROI::All(), OIIO::TypeDesc::UINT8, image.bits())) {
      if (errorOut) {
        *errorOut = QStringLiteral("get_pixels failed: %1").arg(QString::fromStdString(rgba.geterror()));
      }
      return {};
    }

    QImage thumb = image;
    if (targetSize.isValid() && !targetSize.isEmpty()) {
      thumb = image.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return thumb;
  } catch (const std::exception& e) {
    if (errorOut) {
      *errorOut = QString::fromUtf8(e.what());
    }
  } catch (...) {
    if (errorOut) {
      *errorOut = QStringLiteral("Unknown OIIO thumbnail error.");
    }
  }
  return {};
}

QString normalizeAssetPath(const QString& path)
{
  if (path.trimmed().isEmpty()) {
   return {};
  }
  const QFileInfo info(path);
  const QString canonical = info.canonicalFilePath();
  return QDir::cleanPath(canonical.isEmpty() ? info.absoluteFilePath() : canonical);
}

QSize assetGridSizeForThumbnail(const int thumbnailPx)
{
  const int clamped = std::clamp(thumbnailPx, kAssetThumbnailMinPx,
                                 kAssetThumbnailMaxPx);
  return QSize(clamped + 30, clamped + 58);
}

void applyAssetBrowserViewMode(QListView* view, const QListView::ViewMode mode,
                               const int thumbnailPx)
{
  if (!view) {
    return;
  }

  const int clamped = std::clamp(thumbnailPx, kAssetThumbnailMinPx,
                                 kAssetThumbnailMaxPx);
  view->setViewMode(mode);
  if (mode == QListView::ListMode) {
    view->setIconSize(QSize(32, 32));
    view->setFlow(QListView::TopToBottom);
    view->setWrapping(false);
    view->setGridSize(QSize());
    view->setWordWrap(true);
    view->setSpacing(0);
  } else {
    view->setIconSize(QSize(clamped, clamped));
    view->setFlow(QListView::LeftToRight);
    view->setWrapping(true);
    view->setGridSize(assetGridSizeForThumbnail(clamped));
    view->setWordWrap(true);
    view->setSpacing(6);
  }
}
}

class ArtifactBreadcrumbWidget::Impl
{
public:
 Impl() = default;
 QWidget* owner_ = nullptr;
 QHBoxLayout* layout_ = nullptr;
 QString rootPath_;
 QString currentPath_;

 void rebuild();
};

W_OBJECT_IMPL(ArtifactBreadcrumbWidget)

void ArtifactBreadcrumbWidget::Impl::rebuild()
{
 if (!owner_ || !layout_) {
  return;
 }
 while (QLayoutItem* item = layout_->takeAt(0)) {
  if (auto* widget = item->widget()) {
   widget->deleteLater();
  }
  delete item;
 }
 const QString root = QDir::cleanPath(rootPath_);
 const QString current = QDir::cleanPath(currentPath_.isEmpty() ? rootPath_ : currentPath_);
 QStringList parts;
 QString path;
 if (!root.isEmpty() && current.startsWith(root, Qt::CaseInsensitive)) {
  const QString relative = QDir(root).relativeFilePath(current);
  if (relative != QStringLiteral(".")) {
   parts = relative.split(QDir::separator(), Qt::SkipEmptyParts);
  }
 } else {
  parts = current.split(QDir::separator(), Qt::SkipEmptyParts);
 }
 auto addSeparator = [this]() {
  auto* separator = new QLabel(QStringLiteral("/"), owner_);
  separator->setObjectName(QStringLiteral("artifactBreadcrumbSeparator"));
  layout_->addWidget(separator);
 };
 auto configureButton = [](QToolButton* button) {
  button->setAutoRaise(true);
  button->setToolButtonStyle(Qt::ToolButtonTextOnly);
  button->setCursor(Qt::PointingHandCursor);
 };
 bool needsSeparator = false;
 if (!root.isEmpty()) {
  auto* rootButton = new QToolButton(owner_);
  rootButton->setText(QFileInfo(root).fileName().isEmpty() ? root : QFileInfo(root).fileName());
  configureButton(rootButton);
  QObject::connect(rootButton, &QToolButton::clicked, owner_, [this, root]() {
   Q_EMIT static_cast<ArtifactBreadcrumbWidget*>(owner_)->pathClicked(root);
  });
  layout_->addWidget(rootButton);
  path = root;
  needsSeparator = true;
 }
 for (const QString& part : parts) {
  if (needsSeparator) {
   addSeparator();
  }
  if (!path.isEmpty()) {
   path += QDir::separator();
  }
  path += part;
  auto* button = new QToolButton(owner_);
  button->setText(part);
  configureButton(button);
  QObject::connect(button, &QToolButton::clicked, owner_, [this, path]() {
   Q_EMIT static_cast<ArtifactBreadcrumbWidget*>(owner_)->pathClicked(path);
  });
  layout_->addWidget(button);
  needsSeparator = true;
 }
 layout_->addStretch(1);
}

ArtifactBreadcrumbWidget::ArtifactBreadcrumbWidget(QWidget* parent)
  : QFrame(parent), impl_(new Impl())
{
 impl_->owner_ = this;
 impl_->layout_ = new QHBoxLayout(this);
 impl_->layout_->setContentsMargins(0, 0, 0, 0);
 impl_->layout_->setSpacing(4);
 setFrameShape(QFrame::NoFrame);
}

ArtifactBreadcrumbWidget::~ArtifactBreadcrumbWidget()
{
 delete impl_;
}

void ArtifactBreadcrumbWidget::setRootPath(const QString& rootPath)
{
 if (!impl_) {
  return;
 }
 impl_->rootPath_ = normalizeAssetPath(rootPath);
 impl_->rebuild();
}

void ArtifactBreadcrumbWidget::setPath(const QString& path)
{
 if (!impl_) {
  return;
 }
 impl_->currentPath_ = normalizeAssetPath(path);
 impl_->rebuild();
}

 class AssetFileListView final : public QListView
 {
 public:
  explicit AssetFileListView(QWidget* parent = nullptr) : QListView(parent) {}

 protected:
  void startDrag(Qt::DropActions supportedActions) override
  {
   const QModelIndexList indexes = selectedIndexes();
   if (indexes.isEmpty() || !model()) {
    return;
   }

   QElapsedTimer dragTimer;
   dragTimer.start();
   auto* mimeData = new QMimeData();
   QList<QUrl> urls;
   QSet<QString> seenPaths;
   const int pathRole = static_cast<int>(AssetMenuRole::Path);
   for (const QModelIndex& index : indexes) {
    if (!index.isValid()) {
     continue;
    }
    const QString path = index.data(pathRole).toString();
    if (path.isEmpty() || seenPaths.contains(path)) {
     continue;
    }
    seenPaths.insert(path);
    urls.append(QUrl::fromLocalFile(path));
   }
   if (urls.isEmpty()) {
    delete mimeData;
    return;
   }
   mimeData->setUrls(urls);

   auto* drag = new QDrag(this);
   drag->setMimeData(mimeData);

   QPixmap dragPixmap(160, 28);
   dragPixmap.fill(QColor(32, 32, 32, 220));
   {
    QPainter painter(&dragPixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QColor(255, 255, 255, 235));
    painter.drawRoundedRect(dragPixmap.rect().adjusted(0, 0, -1, -1), 6, 6);
    const QString label = indexes.size() == 1
     ? model()->data(indexes.first(), Qt::DisplayRole).toString()
     : QStringLiteral("%1 items").arg(indexes.size());
    painter.drawText(dragPixmap.rect().adjusted(10, 0, -10, 0),
                     Qt::AlignVCenter | Qt::AlignLeft,
                     QFontMetrics(font()).elidedText(label, Qt::ElideRight, 140));
   }
   drag->setPixmap(dragPixmap);
   drag->setHotSpot(QPoint(12, dragPixmap.height() / 2));

   qDebug() << "[AssetBrowser][Drag]" << "mimeMs=" << dragTimer.elapsed()
            << "items=" << indexes.size();
   drag->exec(supportedActions, Qt::CopyAction);
  }
 };

class AssetCardDelegate final : public QStyledItemDelegate
{
public:
 explicit AssetCardDelegate(QListView* view)
     : QStyledItemDelegate(view), view_(view) {}

 QSize sizeHint(const QStyleOptionViewItem& option,
                const QModelIndex& index) const override
 {
  if (view_ && view_->viewMode() == QListView::ListMode) {
   return QStyledItemDelegate::sizeHint(option, index);
  }
  const int thumbnail = std::max(64, option.decorationSize.width());
  return QSize(thumbnail + 24, thumbnail + 54);
 }

 void paint(QPainter* painter, const QStyleOptionViewItem& option,
            const QModelIndex& index) const override
 {
  if (!painter || !index.isValid()) {
   return;
  }
  if (view_ && view_->viewMode() == QListView::ListMode) {
   QStyledItemDelegate::paint(painter, option, index);
   return;
  }

  QStyleOptionViewItem opt(option);
  initStyleOption(&opt, index);
  const bool selected = opt.state.testFlag(QStyle::State_Selected);
  const bool hovered = opt.state.testFlag(QStyle::State_MouseOver);
  const bool focused = opt.state.testFlag(QStyle::State_HasFocus);
  const QRect cardRect = opt.rect.adjusted(3, 3, -3, -3);
  const int informationHeight = 43;
  const QRect thumbnailRect = cardRect.adjusted(1, 1, -1,
                                                 -informationHeight);
  const QRect informationRect(cardRect.left() + 1,
                              thumbnailRect.bottom() + 1,
                              cardRect.width() - 2,
                              informationHeight - 1);

  const QColor base = opt.palette.color(QPalette::Base);
  const QColor text = opt.palette.color(QPalette::Text);
  const QColor accent = opt.palette.color(QPalette::Highlight);
  const QColor cardFill = hovered ? base.lighter(128) : base.lighter(112);
  QColor border = hovered ? text.darker(190) : base.lighter(150);
  if (selected) {
   border = accent.lighter(focused ? 135 : 118);
  }

  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setPen(QPen(border, selected ? 2.0 : 1.0));
  painter->setBrush(cardFill);
  painter->drawRoundedRect(cardRect, 6, 6);

  QPainterPath thumbnailClip;
  thumbnailClip.addRoundedRect(thumbnailRect, 5, 5);
  painter->save();
  painter->setClipPath(thumbnailClip);
  painter->fillRect(thumbnailRect, base.darker(118));
  const QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
  if (!icon.isNull()) {
   const QSize targetSize = thumbnailRect.size() - QSize(10, 10);
   const QPixmap pixmap = icon.pixmap(targetSize);
   if (!pixmap.isNull()) {
    const QSize fitted = pixmap.size().scaled(targetSize, Qt::KeepAspectRatio);
    const QRect pixmapRect(QPoint(0, 0), fitted);
    QRect centered = pixmapRect;
    centered.moveCenter(thumbnailRect.center());
    painter->drawPixmap(centered, pixmap);
   }
  }
  painter->restore();

  const QString title = index.data(static_cast<int>(AssetMenuRole::Name)).toString();
  const QString rawType = index.data(static_cast<int>(AssetMenuRole::Type)).toString();
  const bool isFolder = index.data(static_cast<int>(AssetMenuRole::IsFolder)).toBool();
  const bool favorite = rawType.contains(QStringLiteral("Favorite"), Qt::CaseInsensitive);
  const bool imported = rawType.contains(QStringLiteral("Imported"), Qt::CaseInsensitive);
  const bool missing = rawType.contains(QStringLiteral("Missing"), Qt::CaseInsensitive);
  const bool unused = rawType.contains(QStringLiteral("Unused"), Qt::CaseInsensitive);

  QStringList metadataParts;
  for (const QString& part : rawType.split(QStringLiteral(" • "), Qt::SkipEmptyParts)) {
   if (part.compare(QStringLiteral("Favorite"), Qt::CaseInsensitive) == 0 ||
       part.compare(QStringLiteral("Imported"), Qt::CaseInsensitive) == 0 ||
       part.compare(QStringLiteral("Missing"), Qt::CaseInsensitive) == 0 ||
       part.compare(QStringLiteral("Unused"), Qt::CaseInsensitive) == 0 ||
       part.startsWith(QStringLiteral("Source Uses:"), Qt::CaseInsensitive)) {
    continue;
   }
   metadataParts.push_back(part);
  }
  const QString metadata = isFolder
      ? QStringLiteral("Folder")
      : metadataParts.value(0, QStringLiteral("Asset"));

  const QRect titleRect = informationRect.adjusted(7, 3, -24, -19);
  const QRect metadataRect = informationRect.adjusted(7, 21, -7, -3);
  QFont titleFont = opt.font;
  titleFont.setWeight(QFont::DemiBold);
  painter->setFont(titleFont);
  painter->setPen(selected ? opt.palette.color(QPalette::HighlightedText) : text);
  painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
                    painter->fontMetrics().elidedText(
                        title, Qt::ElideRight, titleRect.width()));

  QFont metadataFont = opt.font;
  metadataFont.setPointSizeF(std::max(8.0, metadataFont.pointSizeF() - 1.0));
  metadataFont.setWeight(QFont::Normal);
  painter->setFont(metadataFont);
  painter->setPen(text.darker(145));
  painter->drawText(metadataRect, Qt::AlignLeft | Qt::AlignVCenter,
                    painter->fontMetrics().elidedText(
                        metadata, Qt::ElideRight, metadataRect.width()));

  if (favorite) {
   painter->setPen(QColor(245, 195, 66));
   painter->drawText(QRect(informationRect.right() - 23,
                           informationRect.top() + 2, 18, 18),
                     Qt::AlignCenter, QStringLiteral("★"));
  }
  if (imported || missing || unused) {
   const QColor statusColor = missing ? QColor(224, 82, 82)
       : unused ? QColor(224, 168, 68)
                : QColor(78, 190, 112);
   painter->setPen(Qt::NoPen);
   painter->setBrush(statusColor);
   painter->drawEllipse(QPoint(informationRect.right() - 10,
                               informationRect.bottom() - 9), 3, 3);
  }
  painter->restore();
 }

private:
 QListView* view_ = nullptr;
};



class ArtifactAssetBrowserToolBar::Impl
{
 private:
 public:
  Impl();
  ~Impl();
  QLineEdit* searchWidget = nullptr;
  QToolButton* gridViewButton = nullptr;
  QToolButton* listViewButton = nullptr;
};

ArtifactAssetBrowserToolBar::Impl::Impl()
{
  searchWidget = new QLineEdit();
  gridViewButton = new QToolButton();
  listViewButton = new QToolButton();
}

 ArtifactAssetBrowserToolBar::Impl::~Impl()
 {
 }

 W_OBJECT_IMPL(ArtifactAssetBrowserToolBar)

 ArtifactAssetBrowserToolBar::ArtifactAssetBrowserToolBar(QWidget* parent /*= nullptr*/) :QWidget(parent),impl_(new Impl())
 {
  auto layout = new QHBoxLayout();
  auto upButton = new QToolButton(this);
  upButton->setObjectName(QStringLiteral("assetBrowserUpButton"));
  upButton->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));
  upButton->setFixedSize(28, 28);
  upButton->setToolTip(QStringLiteral("Go to parent folder"));
  auto refreshButton = new QToolButton(this);
  refreshButton->setObjectName(QStringLiteral("assetBrowserRefreshButton"));
  refreshButton->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
  refreshButton->setFixedSize(28, 28);
  refreshButton->setToolTip(QStringLiteral("Refresh current folder"));
  impl_->gridViewButton->setObjectName(QStringLiteral("assetBrowserGridViewButton"));
  impl_->gridViewButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogContentsView));
  impl_->gridViewButton->setFixedSize(30, 28);
  impl_->gridViewButton->setToolTip(QStringLiteral("Show assets in grid view"));
  impl_->gridViewButton->setCheckable(true);
  impl_->gridViewButton->setChecked(true);
  impl_->listViewButton->setObjectName(QStringLiteral("assetBrowserListViewButton"));
  impl_->listViewButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
  impl_->listViewButton->setFixedSize(30, 28);
  impl_->listViewButton->setToolTip(QStringLiteral("Show assets in list view"));
  impl_->listViewButton->setCheckable(true);
  impl_->searchWidget->setPlaceholderText(QStringLiteral("Search assets"));
  impl_->searchWidget->setClearButtonEnabled(true);
  impl_->searchWidget->setMinimumWidth(220);
  impl_->searchWidget->setMinimumHeight(30);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);
  // Discovery comes first: search, then presentation, then navigation.
  layout->addWidget(impl_->searchWidget, 1);
  layout->addWidget(impl_->gridViewButton);
  layout->addWidget(impl_->listViewButton);
  layout->addSpacing(4);
  layout->addWidget(upButton);
  layout->addWidget(refreshButton);
  setLayout(layout);
 }

 ArtifactAssetBrowserToolBar::~ArtifactAssetBrowserToolBar()
 {
  delete impl_;
 }

 void ArtifactAssetBrowserToolBar::addSeparator()
 {
  auto* bar = new QFrame(this);
  bar->setFrameShape(QFrame::VLine);
  bar->setFrameShadow(QFrame::Plain);
  bar->setObjectName(QStringLiteral("assetBrowserToolBarSeparator"));
  if (auto* layout = this->layout()) {
   layout->addWidget(bar);
  } else {
   bar->deleteLater();
  }
 }

void ArtifactAssetBrowserToolBar::addWidget(QWidget* widget, int stretch)
{
  if (!widget) {
    return;
  }
  if (auto* boxLayout = qobject_cast<QBoxLayout*>(this->layout())) {
   boxLayout->addWidget(widget, stretch);
  } else if (auto* layout = this->layout()) {
   layout->addWidget(widget);
  }
}

 class ArtifactAssetBrowser::Impl
 {
 private:
 QHash<QString, QIcon> thumbnailCache_;  // Cache thumbnails by file path
  mutable std::mutex thumbnailMutex_;  // Protects thumbnail cache access
  QSize thumbnailSize_{kAssetThumbnailDefaultPx, kAssetThumbnailDefaultPx};
  QIcon defaultFileIcon_;
  QIcon defaultImageIcon_;
  QIcon defaultVideoIcon_;
  QIcon defaultAudioIcon_;
  QIcon defaultFontIcon_;
  QSet<QString> unusedAssetPaths_;
  std::atomic<std::uint64_t> thumbnailGeneration_{0};

  // Async preview thumbnail generation for image / video files
  struct PendingPreviewJob {
    QString filePath;
    QFutureWatcher<QImage>* watcher = nullptr;
    std::uint64_t generation = 0;
  };
  QHash<QString, PendingPreviewJob> pendingPreviewJobs_;
  QSet<QString> failedPreviewPaths_;
  QHash<QString, QString> previewFailureReasons_;

  // Async waveform thumbnail generation
  struct PendingWaveJob {
    QString filePath;
    QFutureWatcher<QImage>* watcher = nullptr;
    std::uint64_t generation = 0;
  };
  QHash<QString, PendingWaveJob> pendingWaveJobs_;
  QSet<QString> failedWavePaths_;  // Don't retry failed loads
  HoverPreviewPopup* hoverPreviewPopup_ = nullptr;
  QTimer* hoverPreviewTimer_ = nullptr;
  QString hoverPreviewPath_;
  QPoint hoverPreviewGlobalPos_;

  // Optimized: partial audio loading for thumbnails
  // Only load first N seconds for waveform thumbnail
  static constexpr int kMaxThumbnailAudioSeconds = 30;
 public:
  Impl();
  ~Impl();
 QToolButton* upButton_ = nullptr;
 QToolButton* refreshButton_ = nullptr;
  ArtifactAssetBrowser* owner_ = nullptr;
 QTreeView* directoryView_ = nullptr;
  AssetDirectoryModel* directoryModel_ = nullptr;
  QListView* fileView_ = nullptr;
  AssetMenuModel* assetModel_ = nullptr;
  QLabel* syncStateLabel_ = nullptr;
  QLineEdit* searchEdit_ = nullptr;
  QFileSystemModel* fileModel_ = nullptr;
  QButtonGroup* filterButtonGroup_ = nullptr;
   ArtifactBreadcrumbWidget* breadcrumb_ = nullptr;
   QLabel* currentPathLabel_ = nullptr;
   QLabel* leftHubSummaryLabel_ = nullptr;
   QLabel* leftHubRecentLabel_ = nullptr;
   QLabel* leftHubSelectionLabel_ = nullptr;
   QVector<RecentFolderButton*> recentFolderButtons_;
   QLabel* filePreviewLabel_ = nullptr;
   QLabel* fileInfoLabel_ = nullptr;  // File details display
   QSlider* thumbnailSizeSlider_ = nullptr;  // Thumbnail size adjustment
    QString currentDirectoryPath_;
    QString currentFileTypeFilter_ = "all";
    QString currentStatusFilter_ = "all";
    QString currentSearchFilter_;
    QString currentSortBy_ = "date";  // name, date, size, type
    bool sortAscending_ = false;
    ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
    std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;

  void handleDirectryChanged();
  void handleDoubleClicked();
  void defaultHandleMousePressEvent(QMouseEvent* event);
  void applyFilters();
  bool matchesFileTypeFilter(const QString& fileName) const;
  bool matchesSearchFilter(const QString& fileName) const;
  QIcon generateThumbnail(const QString& filePath);
  QIcon fileTypeIconFor(const QString& fileName) const;
  QIcon getFileIcon(const QString& fileName, const QString& filePath);
  void clearThumbnailCache();
  void startAsyncPreviewThumbnailGeneration(const QString& filePath);
  QString thumbnailDebugStatus(const QString& filePath) const;
  void showHoverPreview(const QString& filePath, const QPoint& globalPos);
  void hideHoverPreview();
  void scheduleHoverPreview(const QString& filePath, const QPoint& globalPos);
  bool isHoverPreviewPath(const QString& filePath) const { return filePath == hoverPreviewPath_; }
   bool isImageFile(const QString& fileName) const;
   bool isVideoFile(const QString& fileName) const;
   bool isAudioFile(const QString& fileName) const;
   bool isFontFile(const QString& fileName) const;
   QIcon generateAudioWaveformThumbnail(const QString& audioFilePath);
   ArtifactCore::AudioSegment loadAudioFile(const QString& audioFilePath);
   void startAsyncWaveformGeneration(const QString& audioFilePath);
   ArtifactCore::FileType fileType(const QString& fileName) const;
  bool isImportedAssetPath(const QString& filePath) const;
  bool isFavoriteAssetPath(const QString& filePath) const;
  bool isUnusedAssetPath(const QString& filePath) const;
  int sourceUseCountForPath(const QString& filePath,
                            const QStringList& sequencePaths = {}) const;
  bool isMissingAssetPath(const QString& filePath) const;
  void toggleFavoritePath(const QString& filePath);
  QStringList selectedAssetPaths() const;
  void syncProjectAssetRoot();
  void syncDirectorySelection();
  void refreshUnusedAssetCache();
  void refreshLeftHubSummary();
  QString syncStateText() const;
   int thumbnailSizePx() const;
   void setThumbnailSizePx(int value);
   QFileSystemWatcher* fsWatcher_ = nullptr;
   bool watchScheduled_ = false;
   void setupFileSystemWatcher();
   void watchCurrentDirectory();
   void handleFileRenamed(const QString& oldPath, const QString& newPath);
   void handleFileDeleted(const QString& path);
   void createNewFolder();
   void renameSelected();
   void deleteSelected();
   QString promptNewName(const QString& currentName) const;
   QString promptNewFolderName() const;
   bool confirmDelete(const QStringList& paths) const;
   };

ArtifactAssetBrowser::Impl::Impl()
{
  // Initialize default icons using Qt standard icons
  QStyle* style = QApplication::style();
  if (style) {
   defaultFileIcon_ = style->standardIcon(QStyle::SP_FileIcon);
   defaultImageIcon_ = QIcon(QStringLiteral(":/icons/Studio/asset_file_image.svg"));
   if (defaultImageIcon_.isNull()) {
    defaultImageIcon_ = style->standardIcon(QStyle::SP_FileIcon);
   }
   defaultVideoIcon_ = QIcon(QStringLiteral(":/icons/Studio/asset_file_video.svg"));
   if (defaultVideoIcon_.isNull()) {
    defaultVideoIcon_ = style->standardIcon(QStyle::SP_MediaPlay);
   }
   defaultAudioIcon_ = QIcon(QStringLiteral(":/icons/Studio/asset_file_audio.svg"));
   if (defaultAudioIcon_.isNull()) {
    defaultAudioIcon_ = style->standardIcon(QStyle::SP_MediaVolume);
   }
   defaultFontIcon_ = style->standardIcon(QStyle::SP_FileDialogDetailedView);
  }
  hoverPreviewTimer_ = new QTimer();
  hoverPreviewTimer_->setSingleShot(true);
  hoverPreviewPopup_ = new HoverPreviewPopup();
}

ArtifactAssetBrowser::Impl::~Impl()
{
  thumbnailGeneration_.fetch_add(1, std::memory_order_relaxed);
  delete hoverPreviewTimer_;
  delete hoverPreviewPopup_;

  for (auto it = pendingPreviewJobs_.begin(); it != pendingPreviewJobs_.end(); ++it) {
    if (auto* watcher = it.value().watcher) {
      QObject::disconnect(watcher, nullptr, nullptr, nullptr);
      watcher->cancel();
      watcher->waitForFinished();
      delete watcher;
    }
  }
  pendingPreviewJobs_.clear();

  for (auto it = pendingWaveJobs_.begin(); it != pendingWaveJobs_.end(); ++it) {
    if (auto* watcher = it.value().watcher) {
      QObject::disconnect(watcher, nullptr, nullptr, nullptr);
      watcher->cancel();
      watcher->waitForFinished();
      delete watcher;
    }
  }
  pendingWaveJobs_.clear();
}

 int ArtifactAssetBrowser::Impl::thumbnailSizePx() const
 {
  return thumbnailSize_.width();
 }

 void ArtifactAssetBrowser::Impl::setThumbnailSizePx(int value)
 {
  const int clamped = std::clamp(value, kAssetThumbnailMinPx, kAssetThumbnailMaxPx);
  thumbnailSize_ = QSize(clamped, clamped);
 }

 void ArtifactAssetBrowser::Impl::handleDoubleClicked()
 {
 }

 void ArtifactAssetBrowser::Impl::defaultHandleMousePressEvent(QMouseEvent* event)
 {
 }

 bool ArtifactAssetBrowser::Impl::matchesFileTypeFilter(const QString& fileName) const
 {
  if (currentFileTypeFilter_ == "all") return true;

  QString lower = fileName.toLower();

  if (currentFileTypeFilter_ == "images") {
   return lower.endsWith(".png") || lower.endsWith(".jpg") ||
          lower.endsWith(".jpeg") || lower.endsWith(".jpe") ||
          lower.endsWith(".jfif") || lower.endsWith(".bmp") ||
          lower.endsWith(".gif") || lower.endsWith(".tga") ||
          lower.endsWith(".tif") || lower.endsWith(".tiff") ||
          lower.endsWith(".hdr") || lower.endsWith(".exr") ||
          lower.endsWith(".webp");
  }
  else if (currentFileTypeFilter_ == "videos") {
   return lower.endsWith(".mp4") || lower.endsWith(".mov") ||
          lower.endsWith(".avi") || lower.endsWith(".mkv") ||
          lower.endsWith(".webm") || lower.endsWith(".flv");
  }
  else if (currentFileTypeFilter_ == "audio") {
   return lower.endsWith(".mp3") || lower.endsWith(".wav") ||
          lower.endsWith(".ogg") || lower.endsWith(".flac") ||
          lower.endsWith(".aac") || lower.endsWith(".m4a");
  }
  else if (currentFileTypeFilter_ == "3d") {
   return lower.endsWith(".fbx") || lower.endsWith(".obj") ||
          lower.endsWith(".gltf") || lower.endsWith(".glb") ||
          lower.endsWith(".usd") || lower.endsWith(".usda") ||
          lower.endsWith(".usdc") || lower.endsWith(".abc") ||
          lower.endsWith(".stl");
  }

  return true;
 }

 bool ArtifactAssetBrowser::Impl::matchesSearchFilter(const QString& fileName) const
 {
  if (currentSearchFilter_.isEmpty()) return true;
  return fileName.contains(currentSearchFilter_, Qt::CaseInsensitive);
 }

 FileType ArtifactAssetBrowser::Impl::fileType(const QString& fileName) const
 {
  static FileTypeDetector detector;
  return detector.detectByExtension(fileName);
 }

 bool ArtifactAssetBrowser::Impl::isImageFile(const QString& fileName) const
 {
  if (fileType(fileName) == ArtifactCore::FileType::Image) {
    return true;
  }
  const QString suffix = QFileInfo(fileName).suffix().toLower();
  return suffix == QStringLiteral("jpe") ||
         suffix == QStringLiteral("jfif");
 }

 bool ArtifactAssetBrowser::Impl::isVideoFile(const QString& fileName) const
 {
  return fileType(fileName) == ArtifactCore::FileType::Video;
 }

 bool ArtifactAssetBrowser::Impl::isAudioFile(const QString& fileName) const
 {
  return fileType(fileName) == ArtifactCore::FileType::Audio;
 }

 bool ArtifactAssetBrowser::Impl::isFontFile(const QString& fileName) const
 {
  QString lower = fileName.toLower();
  return lower.endsWith(".ttf") || lower.endsWith(".otf") ||
         lower.endsWith(".ttc") || lower.endsWith(".woff") ||
         lower.endsWith(".woff2");
 }

 bool ArtifactAssetBrowser::Impl::isImportedAssetPath(const QString& filePath) const
{
  if (filePath.isEmpty()) {
   return false;
  }

  auto* svc = ArtifactProjectService::instance();
  if (!svc) {
   return false;
  }

  const QString canonicalTarget = QFileInfo(filePath).canonicalFilePath().isEmpty()
    ? QFileInfo(filePath).absoluteFilePath()
    : QFileInfo(filePath).canonicalFilePath();

  std::function<bool(ProjectItem*)> containsPath = [&](ProjectItem* item) -> bool {
   if (!item) {
    return false;
   }
   if (item->type() == eProjectItemType::Footage) {
    const QString candidatePath = static_cast<FootageItem*>(item)->filePath;
    const QString canonicalCandidate = QFileInfo(candidatePath).canonicalFilePath().isEmpty()
      ? QFileInfo(candidatePath).absoluteFilePath()
      : QFileInfo(candidatePath).canonicalFilePath();
    if (QDir::cleanPath(canonicalCandidate) == QDir::cleanPath(canonicalTarget)) {
     return true;
    }
   }
   for (auto* child : item->children) {
    if (containsPath(child)) {
     return true;
    }
   }
   return false;
  };

 const auto roots = svc->projectItems();
  for (auto* root : roots) {
   if (containsPath(root)) {
    return true;
   }
  }
  return false;
}

bool ArtifactAssetBrowser::Impl::isFavoriteAssetPath(const QString& filePath) const
{
  return directoryModel_ && directoryModel_->isFavoritePath(filePath);
}

QStringList ArtifactAssetBrowser::Impl::selectedAssetPaths() const
{
  QStringList paths;
  if (!fileView_ || !assetModel_ || !fileView_->selectionModel()) {
   return paths;
  }

  const QModelIndexList selectedIndexes = fileView_->selectionModel()->selectedIndexes();
  paths.reserve(selectedIndexes.size());
  for (const QModelIndex& index : selectedIndexes) {
   const AssetMenuItem item = assetModel_->itemAt(index.row());
   if (!item.isFolder) {
    const QString path = item.path.toQString();
    if (!path.isEmpty()) {
     paths.append(path);
    }
   }
  }
  paths.removeDuplicates();
  return paths;
}

bool ArtifactAssetBrowser::Impl::isUnusedAssetPath(const QString& filePath) const
{
  const QString canonicalPath = QFileInfo(filePath).canonicalFilePath().isEmpty()
    ? QFileInfo(filePath).absoluteFilePath()
    : QFileInfo(filePath).canonicalFilePath();
  return unusedAssetPaths_.contains(QDir::cleanPath(canonicalPath))
    || unusedAssetPaths_.contains(QDir::cleanPath(filePath));
}

int ArtifactAssetBrowser::Impl::sourceUseCountForPath(
    const QString& filePath, const QStringList& sequencePaths) const
{
  if (filePath.trimmed().isEmpty() || !isImportedAssetPath(filePath)) {
    return 0;
  }

  auto& assetManager = ArtifactCore::AssetManager::instance();
  const auto countForPath = [&assetManager](const QString& candidate) {
    const QString trimmed = candidate.trimmed();
    if (trimmed.isEmpty()) {
      return 0;
    }
    QUuid sourceId = assetManager.sourceId(trimmed);
    if (sourceId.isNull()) {
      const QString absolutePath = QFileInfo(trimmed).absoluteFilePath();
      if (absolutePath != trimmed) {
        sourceId = assetManager.sourceId(absolutePath);
      }
    }
    return sourceId.isNull() ? 0 : assetManager.useCount(sourceId);
  };

  int useCount = countForPath(filePath);
  for (const QString& sequencePath : sequencePaths) {
    useCount = std::max(useCount, countForPath(sequencePath));
  }
  return useCount;
}

bool ArtifactAssetBrowser::Impl::isMissingAssetPath(const QString& filePath) const
{
  if (filePath.isEmpty()) {
   return false;
  }
 return !QFileInfo::exists(filePath);
}

void ArtifactAssetBrowser::Impl::toggleFavoritePath(const QString& filePath)
{
  if (!directoryModel_ || filePath.trimmed().isEmpty()) {
   return;
  }

  if (directoryModel_->isFavoritePath(filePath)) {
   const QString guid = directoryModel_->favoriteGuidForPath(filePath);
   if (!guid.isEmpty()) {
    directoryModel_->removeFavorite(guid);
   }
   return;
  }

  const QFileInfo info(filePath);
  const QString displayName = info.fileName().isEmpty() ? filePath : info.fileName();
  directoryModel_->addFavorite(filePath, displayName);
}

QString ArtifactAssetBrowser::Impl::syncStateText() const
{
 auto* svc = ArtifactProjectService::instance();
  return svc ? QStringLiteral("Status: Project linked") : QStringLiteral("Status: Open a folder to browse assets");
}

QIcon ArtifactAssetBrowser::Impl::fileTypeIconFor(const QString& fileName) const
{
 const QString suffix = QFileInfo(fileName).suffix().toLower();
 static const QHash<QString, QString> iconBySuffix = {
  {QStringLiteral("jpg"), QStringLiteral("asset_file_jpeg.svg")},
  {QStringLiteral("jpeg"), QStringLiteral("asset_file_jpeg.svg")},
  {QStringLiteral("jpe"), QStringLiteral("asset_file_jpeg.svg")},
  {QStringLiteral("jfif"), QStringLiteral("asset_file_jpeg.svg")},
  {QStringLiteral("png"), QStringLiteral("asset_file_png.svg")},
  {QStringLiteral("exr"), QStringLiteral("asset_file_exr.svg")},
  {QStringLiteral("webp"), QStringLiteral("asset_file_webp.svg")},
  {QStringLiteral("gif"), QStringLiteral("asset_file_gif.svg")},
  {QStringLiteral("tif"), QStringLiteral("asset_file_tiff.svg")},
  {QStringLiteral("tiff"), QStringLiteral("asset_file_tiff.svg")},
  {QStringLiteral("psd"), QStringLiteral("asset_file_psd.svg")},
  {QStringLiteral("psb"), QStringLiteral("asset_file_psd.svg")},
  {QStringLiteral("ai"), QStringLiteral("asset_file_ai.svg")},
  {QStringLiteral("eps"), QStringLiteral("asset_file_eps.svg")},
  {QStringLiteral("svg"), QStringLiteral("asset_file_svg.svg")},
  {QStringLiteral("pdf"), QStringLiteral("asset_file_pdf.svg")},
  {QStringLiteral("aep"), QStringLiteral("asset_file_aep.svg")},
  {QStringLiteral("aepx"), QStringLiteral("asset_file_aep.svg")},
  {QStringLiteral("mp4"), QStringLiteral("asset_file_mp4.svg")},
  {QStringLiteral("mov"), QStringLiteral("asset_file_mov.svg")},
  {QStringLiteral("avi"), QStringLiteral("asset_file_avi.svg")},
  {QStringLiteral("mkv"), QStringLiteral("asset_file_mkv.svg")},
  {QStringLiteral("wav"), QStringLiteral("asset_file_wav.svg")},
  {QStringLiteral("mp3"), QStringLiteral("asset_file_mp3.svg")},
  {QStringLiteral("aac"), QStringLiteral("asset_file_aac.svg")},
  {QStringLiteral("m4a"), QStringLiteral("asset_file_aac.svg")},
  {QStringLiteral("flac"), QStringLiteral("asset_file_flac.svg")},
  {QStringLiteral("obj"), QStringLiteral("asset_file_obj.svg")},
  {QStringLiteral("fbx"), QStringLiteral("asset_file_fbx.svg")},
  {QStringLiteral("glb"), QStringLiteral("asset_file_gltf.svg")},
  {QStringLiteral("gltf"), QStringLiteral("asset_file_gltf.svg")},
  {QStringLiteral("abc"), QStringLiteral("asset_file_abc.svg")},
  {QStringLiteral("stl"), QStringLiteral("asset_file_stl.svg")},
  {QStringLiteral("usd"), QStringLiteral("asset_file_usd.svg")},
  {QStringLiteral("usda"), QStringLiteral("asset_file_usd.svg")},
  {QStringLiteral("usdc"), QStringLiteral("asset_file_usd.svg")},
  {QStringLiteral("usdz"), QStringLiteral("asset_file_usd.svg")},
  {QStringLiteral("blend"), QStringLiteral("asset_file_blend.svg")},
  {QStringLiteral("c4d"), QStringLiteral("asset_file_c4d.svg")},
  {QStringLiteral("ma"), QStringLiteral("asset_file_maya.svg")},
  {QStringLiteral("mb"), QStringLiteral("asset_file_maya.svg")},
  {QStringLiteral("hip"), QStringLiteral("asset_file_hip.svg")},
  {QStringLiteral("hiplc"), QStringLiteral("asset_file_hip.svg")},
  {QStringLiteral("hipnc"), QStringLiteral("asset_file_hip.svg")},
  {QStringLiteral("ztl"), QStringLiteral("asset_file_zbrush.svg")},
  {QStringLiteral("zpr"), QStringLiteral("asset_file_zbrush.svg")},
  {QStringLiteral("kra"), QStringLiteral("asset_file_kra.svg")},
  {QStringLiteral("krz"), QStringLiteral("asset_file_kra.svg")},
  {QStringLiteral("clip"), QStringLiteral("asset_file_clip.svg")},
  {QStringLiteral("ase"), QStringLiteral("asset_file_aseprite.svg")},
  {QStringLiteral("aseprite"), QStringLiteral("asset_file_aseprite.svg")}
 };

 QString iconName = iconBySuffix.value(suffix);
 if (!iconName.isEmpty()) {
  iconName.replace(QStringLiteral(".svg"), QStringLiteral("_ext.svg"));
 }
 if (!iconName.isEmpty()) {
  const QIcon icon(QStringLiteral(":/icons/Studio/%1").arg(iconName));
  if (!icon.isNull()) {
   return icon;
  }
 }
 if (isImageFile(fileName)) {
  return defaultImageIcon_;
 }
 if (isVideoFile(fileName)) {
  return defaultVideoIcon_;
 }
 if (isAudioFile(fileName)) {
  return defaultAudioIcon_;
 }
 static const QSet<QString> modelSuffixes = {
  QStringLiteral("obj"),
  QStringLiteral("fbx"),
  QStringLiteral("glb"),
  QStringLiteral("gltf"),
  QStringLiteral("abc"),
  QStringLiteral("stl"),
  QStringLiteral("usd"),
  QStringLiteral("usda"),
  QStringLiteral("usdc"),
  QStringLiteral("usdz")
 };
 if (modelSuffixes.contains(suffix)) {
  const QIcon icon(QStringLiteral(":/icons/Studio/asset_file_3d.svg"));
  if (!icon.isNull()) {
   return icon;
  }
 }
 return defaultFileIcon_;
}

 QIcon ArtifactAssetBrowser::Impl::generateThumbnail(const QString& filePath)
 {
  std::unique_lock<std::mutex> lock(thumbnailMutex_);
  // Check cache first
  if (thumbnailCache_.contains(filePath)) {
   return thumbnailCache_[filePath];
  }

  const std::uint64_t currentGeneration = thumbnailGeneration_.load(std::memory_order_relaxed);

  QFileInfo fileInfo(filePath);

  // For folders, use folder icon
  if (fileInfo.isDir()) {
   QStyle* style = QApplication::style();
   if (style) {
    QIcon folderIcon = style->standardIcon(QStyle::SP_DirIcon);
    thumbnailCache_[filePath] = folderIcon;
    return folderIcon;
   }
   return defaultFileIcon_;
  }

  // Generate thumbnail for image files
  if (isImageFile(fileInfo.fileName())) {
   const QIcon placeholder = fileTypeIconFor(fileInfo.fileName());
   if (auto it = pendingPreviewJobs_.find(filePath); it != pendingPreviewJobs_.end()) {
    if (it.value().generation == currentGeneration) {
     return placeholder;
    }
    pendingPreviewJobs_.erase(it);
   }
   if (failedPreviewPaths_.contains(filePath)) {
    return placeholder;
   }
   lock.unlock();
   startAsyncPreviewThumbnailGeneration(filePath);
   return placeholder;
  }

  // Extract first frame as thumbnail for video files
  if (isVideoFile(fileInfo.fileName())) {
      const QIcon placeholder = fileTypeIconFor(fileInfo.fileName());
      if (auto it = pendingPreviewJobs_.find(filePath); it != pendingPreviewJobs_.end()) {
       if (it.value().generation == currentGeneration) {
        return placeholder;
       }
       pendingPreviewJobs_.erase(it);
      }
      if (failedPreviewPaths_.contains(filePath)) {
       return placeholder;
      }
      lock.unlock();
      startAsyncPreviewThumbnailGeneration(filePath);
      return placeholder;
  }

  // For audio files, generate waveform thumbnail
  if (isAudioFile(fileInfo.fileName())) {
   QIcon waveIcon = generateAudioWaveformThumbnail(filePath);
   if (!waveIcon.isNull()) {
    thumbnailCache_[filePath] = waveIcon;
    return waveIcon;
   }
   // Fallback to default audio icon
   const QIcon placeholder = fileTypeIconFor(fileInfo.fileName());
   thumbnailCache_[filePath] = placeholder;
   return placeholder;
  }

  if (isFontFile(fileInfo.fileName())) {
   thumbnailCache_[filePath] = defaultFontIcon_;
   return defaultFontIcon_;
  }

  // Ask the platform for a real thumbnail before falling back to the
  // application-owned file type icon.
  const QIcon placeholder = fileTypeIconFor(fileInfo.fileName());
  if (auto it = pendingPreviewJobs_.find(filePath); it != pendingPreviewJobs_.end()) {
   if (it.value().generation == currentGeneration) {
    return placeholder;
   }
   pendingPreviewJobs_.erase(it);
  }
  if (!failedPreviewPaths_.contains(filePath)) {
   thumbnailCache_[filePath] = placeholder;
   lock.unlock();
   startAsyncPreviewThumbnailGeneration(filePath);
   return placeholder;
  }
  thumbnailCache_[filePath] = placeholder;
  return placeholder;
 }

 QIcon ArtifactAssetBrowser::Impl::getFileIcon(const QString& fileName, const QString& filePath)
 {
  return generateThumbnail(filePath);
 }

void ArtifactAssetBrowser::Impl::clearThumbnailCache()
{
  thumbnailGeneration_.fetch_add(1, std::memory_order_relaxed);
  {
    std::lock_guard<std::mutex> lock(thumbnailMutex_);
    thumbnailCache_.clear();
    failedPreviewPaths_.clear();
    failedWavePaths_.clear();
    previewFailureReasons_.clear();
  }
  if (fileView_) {
    fileView_->update();
  }
 }

void ArtifactAssetBrowser::Impl::startAsyncPreviewThumbnailGeneration(const QString& filePath)
{
  if (!owner_) {
    return;
  }

  if (QThread::currentThread() != owner_->thread()) {
    QTimer::singleShot(0, owner_, [this, filePath]() {
      startAsyncPreviewThumbnailGeneration(filePath);
    });
    return;
  }

  {
    std::lock_guard<std::mutex> lock(thumbnailMutex_);
    if (pendingPreviewJobs_.contains(filePath)) {
      const auto existing = pendingPreviewJobs_.value(filePath);
      if (existing.generation == thumbnailGeneration_.load(std::memory_order_relaxed)) {
        return;
      }
      pendingPreviewJobs_.remove(filePath);
    }
    previewFailureReasons_.remove(filePath);
  }

  const quint64 jobGeneration = thumbnailGeneration_.load(std::memory_order_relaxed);
  auto* watcher = new QFutureWatcher<QImage>();
  QObject::connect(watcher, &QFutureWatcher<QImage>::finished, [this, watcher, filePath, jobGeneration]() {
    const QImage image = watcher->result();
    {
      std::lock_guard<std::mutex> lock(thumbnailMutex_);
      pendingPreviewJobs_.remove(filePath);
    }
    if (jobGeneration != thumbnailGeneration_.load(std::memory_order_relaxed)) {
      watcher->deleteLater();
      return;
    }
    if (!image.isNull()) {
      const QIcon icon(QPixmap::fromImage(image));
      {
        std::lock_guard<std::mutex> lock(thumbnailMutex_);
        thumbnailCache_[filePath] = icon;
        failedPreviewPaths_.remove(filePath);
        previewFailureReasons_.remove(filePath);
      }
      if (assetModel_ && assetModel_->updateItemIconByPath(filePath, icon)) {
        // model updated via dataChanged
      } else if (fileView_) {
        fileView_->update();
      }
      if (owner_ && fileView_ && fileView_->selectionModel()) {
        const QModelIndexList selectedIndexes = fileView_->selectionModel()->selectedIndexes();
        for (const QModelIndex& index : selectedIndexes) {
          const AssetMenuItem item = assetModel_->itemAt(index.row());
          if (item.path.toQString() == filePath) {
            owner_->updateFileInfo(filePath);
            break;
          }
        }
      }
    } else {
      QFileInfo fileInfo(filePath);
      const QString failureReason =
          QStringLiteral("Async thumbnail decode returned empty image for %1.")
              .arg(fileInfo.suffix().toUpper());
      {
        std::lock_guard<std::mutex> lock(thumbnailMutex_);
        failedPreviewPaths_.insert(filePath);
        previewFailureReasons_[filePath] = failureReason;
      }
      qWarning().noquote() << "[AssetBrowser][Thumbnail]" << filePath << failureReason;
      if (owner_ && fileView_ && fileView_->selectionModel()) {
        const QModelIndexList selectedIndexes = fileView_->selectionModel()->selectedIndexes();
        for (const QModelIndex& index : selectedIndexes) {
          const AssetMenuItem item = assetModel_->itemAt(index.row());
          if (item.path.toQString() == filePath) {
            owner_->updateFileInfo(filePath);
            break;
          }
        }
      }
    }
    watcher->deleteLater();
  });

  const QSize thumbSize = thumbnailSize_;
  QFuture<QImage> future = QtConcurrent::run([filePath, thumbSize]() -> QImage {
    const QFileInfo fileInfo(filePath);
    const QString suffix = fileInfo.suffix().toLower();
    const auto isJpegExt = [&]() {
      return suffix == QStringLiteral("jpg")
          || suffix == QStringLiteral("jpeg")
          || suffix == QStringLiteral("jpe")
          || suffix == QStringLiteral("jfif");
    };
    const auto isImageExt = [&]() {
      return suffix == QStringLiteral("png")
          || isJpegExt()
          || suffix == QStringLiteral("bmp")
          || suffix == QStringLiteral("gif")
          || suffix == QStringLiteral("tga")
          || suffix == QStringLiteral("tif")
          || suffix == QStringLiteral("tiff")
          || suffix == QStringLiteral("hdr")
          || suffix == QStringLiteral("exr")
          || suffix == QStringLiteral("webp");
    };
    const auto isVideoExt = [&]() {
      return suffix == QStringLiteral("mp4")
          || suffix == QStringLiteral("mov")
          || suffix == QStringLiteral("avi")
          || suffix == QStringLiteral("mkv")
          || suffix == QStringLiteral("webm")
          || suffix == QStringLiteral("flv");
    };

    if (isImageExt()) {
      QString oiioError;
      const bool preferQtReaderFirst = suffix == QStringLiteral("webp");
      QImage image;
      if (!preferQtReaderFirst) {
        image = loadImageThumbnailViaOIIO(filePath, thumbSize, &oiioError);
      }
      if (!image.isNull()) {
        return image;
      }

#ifdef _WIN32
      if (isJpegExt()) {
        QString wicError;
        image = loadImageThumbnailViaWIC(filePath, thumbSize, &wicError);
        if (!image.isNull()) {
          return image;
        }
      }
#endif

      QImageReader reader(filePath);
      reader.setAutoTransform(true);
      const QImage fallbackImage = reader.read();
      if (!fallbackImage.isNull()) {
        return fallbackImage.scaled(thumbSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
      }
#ifdef _WIN32
      if (!isJpegExt()) {
        QString wicError;
        image = loadImageThumbnailViaWIC(filePath, thumbSize, &wicError);
        if (!image.isNull()) {
          return image;
        }
      }
      QString shellError;
      image = loadImageThumbnailViaWindowsShell(filePath, thumbSize, &shellError);
      if (!image.isNull()) {
        return image;
      }
#endif
      return {};
    }

    if (isVideoExt()) {
      FFmpegThumbnailExtractor extractor;
      const auto result = extractor.extractThumbnail(UniString::fromQString(filePath));
      if (result.success && !result.image.isNull()) {
        return result.image.scaled(thumbSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
      }
      return {};
    }

#ifdef _WIN32
    QString shellError;
    return loadImageThumbnailViaWindowsShell(filePath, thumbSize, &shellError);
#else
    return {};
#endif
  });

  watcher->setFuture(future);
  {
    std::lock_guard<std::mutex> lock(thumbnailMutex_);
    pendingPreviewJobs_[filePath] = {filePath, watcher, jobGeneration};
  }
 }

QString ArtifactAssetBrowser::Impl::thumbnailDebugStatus(const QString& filePath) const
{
  std::lock_guard<std::mutex> lock(thumbnailMutex_);
  if (thumbnailCache_.contains(filePath)) {
    return QStringLiteral("Ready");
  }
  if (pendingPreviewJobs_.contains(filePath)) {
    return QStringLiteral("Pending");
  }
  if (failedPreviewPaths_.contains(filePath)) {
    const QString failureReason = previewFailureReasons_.value(filePath);
    if (!failureReason.isEmpty()) {
      return QStringLiteral("Failed (%1)").arg(failureReason);
    }
    return QStringLiteral("Failed");
  }
  return QStringLiteral("Placeholder");
}

void ArtifactAssetBrowser::Impl::syncProjectAssetRoot()
{
  if (!directoryModel_) return;

  QString assetsPath = ArtifactProjectManager::getInstance().currentProjectAssetsPath();
  if (assetsPath.isEmpty()) {
   assetsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Assets";
  }

  QDir assetsDir(assetsPath);
  if (!assetsDir.exists()) {
   assetsDir.mkpath(".");
  }

  QString previousRoot = currentDirectoryPath_;
  directoryModel_->setAssetRootPath(assetsPath);

  if (previousRoot.isEmpty() || !QDir(previousRoot).exists() || !previousRoot.startsWith(assetsPath, Qt::CaseInsensitive)) {
   currentDirectoryPath_ = assetsPath;
  } else {
   currentDirectoryPath_ = previousRoot;
  }

 if (breadcrumb_) {
   breadcrumb_->setRootPath(assetsPath);
   breadcrumb_->setPath(currentDirectoryPath_);
  }

  refreshUnusedAssetCache();
  refreshLeftHubSummary();
  clearThumbnailCache();
  applyFilters();
  syncDirectorySelection();
}

void ArtifactAssetBrowser::Impl::refreshUnusedAssetCache()
{
  unusedAssetPaths_.clear();
  auto* svc = ArtifactProjectService::instance();
  if (!svc) {
   return;
  }
  auto project = svc->getCurrentProjectSharedPtr();
  if (!project) {
   return;
  }
  const QStringList unused = ArtifactProjectCleanupTool::findUnusedAssetPaths(project.get());
  for (const QString& path : unused) {
   const QString canonicalPath = QFileInfo(path).canonicalFilePath().isEmpty()
    ? QFileInfo(path).absoluteFilePath()
    : QFileInfo(path).canonicalFilePath();
   unusedAssetPaths_.insert(QDir::cleanPath(path));
   unusedAssetPaths_.insert(QDir::cleanPath(canonicalPath));
  }
}

void ArtifactAssetBrowser::Impl::hideHoverPreview()
{
  if (hoverPreviewTimer_) {
    hoverPreviewTimer_->stop();
  }
  if (hoverPreviewPopup_) {
    hoverPreviewPopup_->hide();
  }
  hoverPreviewPath_.clear();
}

void ArtifactAssetBrowser::Impl::showHoverPreview(const QString& filePath, const QPoint& globalPos)
{
  if (!hoverPreviewPopup_ || filePath.isEmpty()) {
    return;
  }
  const QFileInfo info(filePath);
  const QIcon icon = generateThumbnail(filePath);
  hoverPreviewPopup_->showFile(filePath, globalPos, icon, info);
}

void ArtifactAssetBrowser::Impl::scheduleHoverPreview(const QString& filePath, const QPoint& globalPos)
{
  if (!hoverPreviewTimer_) {
    return;
  }
  hoverPreviewPath_ = filePath;
  hoverPreviewGlobalPos_ = globalPos;
  hoverPreviewTimer_->stop();
  QObject::disconnect(hoverPreviewTimer_, nullptr, nullptr, nullptr);
  QObject::connect(hoverPreviewTimer_, &QTimer::timeout, [this]() {
    showHoverPreview(hoverPreviewPath_, hoverPreviewGlobalPos_);
  });
  hoverPreviewTimer_->start(300);
}

  void ArtifactAssetBrowser::Impl::syncDirectorySelection()
 {
  if (!directoryView_ || !directoryModel_ || currentDirectoryPath_.isEmpty()) {
   return;
  }

  std::function<QModelIndex(const QModelIndex&)> findByPath = [&](const QModelIndex& parent) -> QModelIndex {
   const int rowCount = directoryModel_->rowCount(parent);
   for (int row = 0; row < rowCount; ++row) {
    const QModelIndex index = directoryModel_->index(row, 0, parent);
    if (!index.isValid()) {
     continue;
    }
    const QString path = directoryModel_->pathFromIndex(index);
    if (QDir::cleanPath(path) == QDir::cleanPath(currentDirectoryPath_)) {
     return index;
    }
    if (directoryModel_->canFetchMore(index)) {
     directoryModel_->fetchMore(index);
    }
    if (const QModelIndex child = findByPath(index); child.isValid()) {
     return child;
    }
   }
   return {};
  };

  const QModelIndex matchedIndex = findByPath({});
  if (!matchedIndex.isValid()) {
   return;
  }

  directoryView_->expand(matchedIndex.parent());
  directoryView_->setCurrentIndex(matchedIndex);
  directoryView_->scrollTo(matchedIndex, QAbstractItemView::PositionAtCenter);
 }

 void ArtifactAssetBrowser::Impl::refreshLeftHubSummary()
 {
  if (currentPathLabel_) {
   QString currentName;
   if (currentDirectoryPath_.isEmpty()) {
    currentName = QStringLiteral("(none)");
   } else {
    currentName = QFileInfo(currentDirectoryPath_).fileName();
    if (currentName.isEmpty()) {
     currentName = currentDirectoryPath_;
    }
   }
   currentPathLabel_->setText(QStringLiteral("Current: Library Hub / %1").arg(currentName));
   currentPathLabel_->setToolTip(currentDirectoryPath_.isEmpty()
                                     ? QStringLiteral("Select a folder to browse assets")
                                     : currentDirectoryPath_);
  }
  if (leftHubSummaryLabel_) {
   const int recentCount = directoryModel_ ? directoryModel_->recentEntries().size() : 0;
   const int favoriteCount = directoryModel_ ? directoryModel_->favoriteEntries().size() : 0;
   const int sourceCount =
       (directoryModel_ && directoryModel_->indexFromGuid(QStringLiteral("assets")).isValid() ? 1 : 0) +
       (directoryModel_ && directoryModel_->indexFromGuid(QStringLiteral("packages")).isValid() ? 1 : 0);
   QString statusText = QStringLiteral("All");
   if (currentStatusFilter_ == QStringLiteral("imported")) {
    statusText = QStringLiteral("Imported");
   } else if (currentStatusFilter_ == QStringLiteral("missing")) {
    statusText = QStringLiteral("Missing");
   } else if (currentStatusFilter_ == QStringLiteral("unused")) {
    statusText = QStringLiteral("Unused");
   }
   leftHubSummaryLabel_->setText(
       QStringLiteral("Favorites: %1  •  Sources: %2  •  Status: %3")
           .arg(favoriteCount)
           .arg(sourceCount)
           .arg(statusText));
   leftHubSummaryLabel_->setToolTip(QStringLiteral("Status follows the current asset filter."));
  }
  if (leftHubRecentLabel_) {
   const QVector<RecentEntry> entries = directoryModel_ ? directoryModel_->recentEntries() : QVector<RecentEntry>{};
   if (entries.isEmpty()) {
    leftHubRecentLabel_->setText(QStringLiteral("Open a folder to continue | Recent folders appear here"));
    leftHubRecentLabel_->setToolTip(QStringLiteral("Select a folder to browse assets."));
   } else {
    QStringList names;
    const int limit = static_cast<int>(std::min<qsizetype>(entries.size(), 3));
    names.reserve(limit);
    for (int i = 0; i < limit; ++i) {
     QString label = entries[i].name.trimmed();
     if (label.isEmpty()) {
      label = QFileInfo(entries[i].path).fileName();
     }
     if (label.isEmpty()) {
      label = entries[i].path;
     }
     names.append(label);
    }
    leftHubRecentLabel_->setText(QStringLiteral("Recent folders: %1").arg(names.join(QStringLiteral("  •  "))));
    leftHubRecentLabel_->setToolTip(entries.first().path);
   }
  }
  if (leftHubSelectionLabel_) {
   const QStringList paths = selectedAssetPaths();
   if (paths.isEmpty()) {
    leftHubSelectionLabel_->setText(QStringLiteral("No asset selected"));
    leftHubSelectionLabel_->setToolTip(QStringLiteral("Select an asset to inspect details."));
   } else {
    QString name = QFileInfo(paths.first()).fileName();
    if (name.isEmpty()) {
     name = paths.first();
    }
    leftHubSelectionLabel_->setText(
        QStringLiteral("Selection: %1 selected | %2").arg(static_cast<int>(paths.size())).arg(name));
    leftHubSelectionLabel_->setToolTip(paths.join(QStringLiteral("\n")));
   }
  }
  if (!recentFolderButtons_.isEmpty()) {
   const QVector<RecentEntry> entries = directoryModel_ ? directoryModel_->recentEntries() : QVector<RecentEntry>{};
   for (int i = 0; i < recentFolderButtons_.size(); ++i) {
    RecentFolderButton* button = recentFolderButtons_[i];
    if (!button) {
     continue;
    }
    if (i < entries.size()) {
     QString label = entries[i].name.trimmed();
     if (label.isEmpty()) {
      label = QFileInfo(entries[i].path).fileName();
     }
     if (label.isEmpty()) {
      label = entries[i].path;
     }
     button->setEntry(label, entries[i].path, [this](const QString& path) {
      if (owner_) {
       owner_->navigateToFolder(path);
      }
     });
    } else {
     button->setEntry(QString(), QString(), {});
    }
   }
  }
 }

 void ArtifactAssetBrowser::Impl::applyFilters()
 {
  if (!fileView_ || !assetModel_ || currentDirectoryPath_.isEmpty()) return;

  QDir dir(currentDirectoryPath_);
  if (!dir.exists()) return;

  if (breadcrumb_) {
   breadcrumb_->setPath(currentDirectoryPath_);
  }

   // Get both files and directories, excluding . and ..
   QStringList entries = dir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
   QList<AssetMenuItem> items;

   // --- Phase 1: pre-filter directories ---
   QStringList dirNames;
   for (const QString& entry : entries) {
    QString fullPath = dir.absoluteFilePath(entry);
    QFileInfo fileInfo(fullPath);
    if (!fileInfo.isDir()) continue;
    if (currentFileTypeFilter_ != "all") continue;
    if (!matchesSearchFilter(entry)) continue;
    dirNames.append(entry);
   }

   // --- Phase 2: pre-filter files and detect sequences ---
   struct SeqFile { QString name; int frame; int pad; QString fullPath; };
   static const QRegularExpression kSeqRx(QStringLiteral(R"(^(.*?)([._-]?)(\d{3,})(\.[a-zA-Z0-9]+)$)"));
   QMap<QString, QList<SeqFile>> seqMap;
   QSet<QString> seqFiles;

   for (const QString& entry : entries) {
    QString fullPath = dir.absoluteFilePath(entry);
    QFileInfo fileInfo(fullPath);
    if (fileInfo.isDir()) continue;
    if (!matchesSearchFilter(entry) || !matchesFileTypeFilter(entry)) continue;

    QRegularExpressionMatch m = kSeqRx.match(entry);
    if (!m.hasMatch()) continue;
    QString frameStr = m.captured(3);
    bool ok = false;
    int frameNum = frameStr.toInt(&ok);
    if (!ok) continue;
    QString key = m.captured(1) + m.captured(2) + m.captured(4);
    seqMap[key].append({entry, frameNum, static_cast<int>(frameStr.length()), fullPath});
   }

   for (auto it = seqMap.begin(); it != seqMap.end(); ++it) {
    if (it.value().size() < 2) continue;
    for (const auto& sf : it.value()) seqFiles.insert(sf.name);
   }

   // --- Phase 3: build items (dirs → sequences → standalone files) ---

   // Directories
   for (const QString& dirName : dirNames) {
    AssetMenuItem item;
    QString fullPath = dir.absoluteFilePath(dirName);
    item.name = UniString::fromQString(dirName);
    item.path = UniString::fromQString(fullPath);
    QStringList markers;
    if (isFavoriteAssetPath(fullPath)) markers.append(QStringLiteral("Favorite"));
    QString itemType = QStringLiteral("Folder");
    if (!markers.isEmpty()) itemType = QStringLiteral("%1 • Folder").arg(markers.join(QStringLiteral(" • ")));
    item.type = UniString::fromQString(itemType);
    item.isFolder = true;
    item.icon = generateThumbnail(fullPath);
    items.append(item);
   }

   // Sequences
   for (auto it = seqMap.begin(); it != seqMap.end(); ++it) {
    if (it.value().size() < 2) continue;
    auto& seq = it.value();
    std::sort(seq.begin(), seq.end(), [](const SeqFile& a, const SeqFile& b) { return a.frame < b.frame; });

    AssetMenuItem item;
    int firstFrame = seq.first().frame;
    int lastFrame = seq.last().frame;
    int count = seq.size();
    int pad = seq.first().pad;
    QFileInfo fi(seq.first().name);
    QString ext = fi.suffix().toUpper();

    // Name: "prefix[####].ext (N frames)"
    QString prefix;
    {
     QRegularExpressionMatch m2 = kSeqRx.match(seq.first().name);
     prefix = m2.hasMatch() ? m2.captured(1) : fi.completeBaseName();
    }
    item.name = UniString::fromQString(QStringLiteral("%1[%2-%3].%4 (%5 frames)")
      .arg(prefix)
      .arg(firstFrame, pad, 10, QLatin1Char('0'))
      .arg(lastFrame, pad, 10, QLatin1Char('0'))
      .arg(ext.toLower())
      .arg(count));
    item.path = UniString::fromQString(seq.first().fullPath);
    item.isSequence = true;
    item.sequenceFrameCount = count;
    item.sequenceStartFrame = firstFrame;
    item.sequencePadding = pad;
    for (const auto& sf : seq) item.sequencePaths.append(sf.fullPath);

    // Status markers (aggregated across all frames)
    bool allImported = true, allUnused = true, anyMissing = false, allFav = true;
    for (const auto& sf : seq) {
     if (!isImportedAssetPath(sf.fullPath)) allImported = false;
     if (!isUnusedAssetPath(sf.fullPath)) allUnused = false;
     if (isMissingAssetPath(sf.fullPath)) anyMissing = true;
     if (!isFavoriteAssetPath(sf.fullPath)) allFav = false;
    }
    // Apply status filter
    if (currentStatusFilter_ != "all") {
     bool matchFilter = false;
     if (currentStatusFilter_ == "imported") matchFilter = allImported;
     else if (currentStatusFilter_ == "favorite") matchFilter = allFav;
     else if (currentStatusFilter_ == "missing") matchFilter = anyMissing;
     else if (currentStatusFilter_ == "unused") matchFilter = allUnused;
     if (!matchFilter) continue;
    }

    QStringList markers;
    if (allFav) markers.append(QStringLiteral("Favorite"));
    if (anyMissing) markers.append(QStringLiteral("Missing"));
    if (allImported) markers.append(QStringLiteral("Imported"));
    if (allUnused) markers.append(QStringLiteral("Unused"));
    QString seqType = QStringLiteral("Sequence • %1").arg(ext);
    if (!markers.isEmpty()) seqType = QStringLiteral("%1 • %2").arg(markers.join(QStringLiteral(" • ")), seqType);
    item.type = UniString::fromQString(seqType);

    item.icon = generateThumbnail(seq.first().fullPath);
    items.append(item);
   }

   // Standalone files — parallelized with TBB
   std::vector<AssetMenuItem> standaloneItems(entries.size());
   tbb::parallel_for(tbb::blocked_range<int>(0, static_cast<int>(entries.size())),
    [&](const tbb::blocked_range<int>& range) {
     for (int i = range.begin(); i < range.end(); ++i) {
      const QString& entry = entries.at(i);
      const QString fullPath = dir.absoluteFilePath(entry);
      const QFileInfo fileInfo(fullPath);
      if (fileInfo.isDir() || !matchesSearchFilter(entry) || !matchesFileTypeFilter(entry) || seqFiles.contains(entry)) {
       continue;
      }

      // Check status filter
      if (currentStatusFilter_ != "all") {
       const bool imported = isImportedAssetPath(fullPath);
       const bool unused = isUnusedAssetPath(fullPath);
       const bool missing = isMissingAssetPath(fullPath);
       const bool favorite = isFavoriteAssetPath(fullPath);
       if (currentStatusFilter_ == "imported" && !imported) continue;
       if (currentStatusFilter_ == "favorite" && !favorite) continue;
       if (currentStatusFilter_ == "missing" && !missing) continue;
       if (currentStatusFilter_ == "unused" && !unused) continue;
      }

      AssetMenuItem item;
      item.name = UniString::fromQString(entry);
      item.path = UniString::fromQString(fullPath);
      QStringList markers;
      if (isFavoriteAssetPath(fullPath)) markers.append(QStringLiteral("Favorite"));
      const bool imported = isImportedAssetPath(fullPath);
      const bool unused = isUnusedAssetPath(fullPath);
      const bool missing = isMissingAssetPath(fullPath);
      if (missing) markers.append(QStringLiteral("Missing"));
      if (imported) markers.append(QStringLiteral("Imported"));
      if (unused) markers.append(QStringLiteral("Unused"));
      QString itemType = fileInfo.suffix().toUpper();
      if (!markers.isEmpty()) itemType = QStringLiteral("%1 • %2").arg(markers.join(QStringLiteral(" • ")), itemType);
      item.type = UniString::fromQString(itemType);
      item.isFolder = false;
      item.icon = generateThumbnail(fullPath);
      standaloneItems[static_cast<size_t>(i)] = std::move(item);
     }
    });

   for (auto& item : standaloneItems) {
    if (!item.name.toQString().isEmpty()) {
     items.append(std::move(item));
    }
   }

   // Source use count is project state, so decorate the browser metadata only
   // after the filesystem scan has completed. This keeps the parallel scan
   // focused on filesystem/project filtering and avoids adding row widgets.
   for (auto& item : items) {
    if (item.isFolder) {
     continue;
    }
    const QString path = item.path.toQString();
    if (path.isEmpty()) {
     continue;
    }
    const int sourceUseCount = sourceUseCountForPath(path, item.sequencePaths);
    if (sourceUseCount <= 0 && !isImportedAssetPath(path)) {
     continue;
    }
    QString typeText = item.type.toQString();
    if (!typeText.contains(QStringLiteral("Source Uses:"))) {
     typeText += QStringLiteral(" • Source Uses: %1").arg(sourceUseCount);
     item.type = UniString::fromQString(typeText);
    }
   }

   // Sort items
   std::sort(items.begin(), items.end(), [this](const AssetMenuItem& a, const AssetMenuItem& b) {
    // Folders always first
    if (a.isFolder && !b.isFolder) return true;
    if (!a.isFolder && b.isFolder) return false;

    if (currentSortBy_ == "name") {
     int result = a.name.toQString().compare(b.name.toQString(), Qt::CaseInsensitive);
     return sortAscending_ ? result < 0 : result > 0;
    } else if (currentSortBy_ == "date") {
     QFileInfo infoA(a.path.toQString());
     QFileInfo infoB(b.path.toQString());
     QDateTime dateA = infoA.lastModified();
     QDateTime dateB = infoB.lastModified();
     int result = dateA < dateB ? -1 : (dateA > dateB ? 1 : 0);
     return sortAscending_ ? result < 0 : result > 0;
    } else if (currentSortBy_ == "size") {
     qint64 sizeA = QFileInfo(a.path.toQString()).size();
     qint64 sizeB = QFileInfo(b.path.toQString()).size();
     int result = sizeA < sizeB ? -1 : (sizeA > sizeB ? 1 : 0);
     return sortAscending_ ? result < 0 : result > 0;
    } else if (currentSortBy_ == "type") {
     int result = a.type.toQString().compare(b.type.toQString(), Qt::CaseInsensitive);
     return sortAscending_ ? result < 0 : result > 0;
    }
    return a.name.toQString().compare(b.name.toQString(), Qt::CaseInsensitive) < 0;
   });

   assetModel_->setItems(items);
 }

 ArtifactAssetBrowser::ArtifactAssetBrowser(QWidget* parent /*= nullptr*/) :QWidget(parent), impl_(new Impl())
 {
  impl_->owner_ = this;
  setWindowTitle("AssetBrowser");

  // Enable drag and drop
  setAcceptDrops(true);

  auto assetToolBar = new ArtifactAssetBrowserToolBar();
  impl_->searchEdit_ = assetToolBar->findChild<QLineEdit*>();
  if (impl_->searchEdit_) {
   impl_->searchEdit_->installEventFilter(this);
  }
  impl_->upButton_ = assetToolBar->findChild<QToolButton*>(QStringLiteral("assetBrowserUpButton"));
  impl_->refreshButton_ = assetToolBar->findChild<QToolButton*>(QStringLiteral("assetBrowserRefreshButton"));
  auto* gridViewButton = assetToolBar->findChild<QToolButton*>(QStringLiteral("assetBrowserGridViewButton"));
  auto* listViewButton = assetToolBar->findChild<QToolButton*>(QStringLiteral("assetBrowserListViewButton"));

  // File type filter buttons
  auto typeFiltersLayout = new QHBoxLayout();
  typeFiltersLayout->setContentsMargins(0, 0, 0, 0);
  typeFiltersLayout->setSpacing(4);
  impl_->filterButtonGroup_ = new QButtonGroup(this);

  auto allButton = new QToolButton();
  allButton->setText("All");
  allButton->setCheckable(true);
  allButton->setChecked(true);

  auto imagesButton = new QToolButton();
  imagesButton->setText("Images");
  imagesButton->setCheckable(true);

  auto videosButton = new QToolButton();
  videosButton->setText("Videos");
  videosButton->setCheckable(true);

  auto audioButton = new QToolButton();
  audioButton->setText("Audio");
  audioButton->setCheckable(true);

  auto fontsButton = new QToolButton();
  fontsButton->setText("3D");
  fontsButton->setCheckable(true);

  impl_->filterButtonGroup_->addButton(allButton, 0);
  impl_->filterButtonGroup_->addButton(imagesButton, 1);
  impl_->filterButtonGroup_->addButton(videosButton, 2);
  impl_->filterButtonGroup_->addButton(audioButton, 3);
  impl_->filterButtonGroup_->addButton(fontsButton, 4);

   typeFiltersLayout->addWidget(allButton);
   typeFiltersLayout->addWidget(imagesButton);
   typeFiltersLayout->addWidget(videosButton);
   typeFiltersLayout->addWidget(audioButton);
   typeFiltersLayout->addWidget(fontsButton);
   typeFiltersLayout->addStretch();

   auto statusAllBtn = new QToolButton(this);
   statusAllBtn->setText("Status: All");
   statusAllBtn->setCheckable(true);
   statusAllBtn->setChecked(true);

   auto importedBtn = new QToolButton(this);
   importedBtn->setText("Imported");
   importedBtn->setCheckable(true);

   auto favoriteBtn = new QToolButton(this);
   favoriteBtn->setText("Favorite");
   favoriteBtn->setCheckable(true);

   auto missingBtn = new QToolButton(this);
   missingBtn->setText("Missing");
   missingBtn->setCheckable(true);

   auto unusedBtn = new QToolButton(this);
   unusedBtn->setText("Unused");
   unusedBtn->setCheckable(true);

   auto* statusGroup = new QButtonGroup(this);
   statusGroup->setExclusive(true);
   statusGroup->addButton(statusAllBtn, 0);
   statusGroup->addButton(importedBtn, 1);
   statusGroup->addButton(favoriteBtn, 2);
   statusGroup->addButton(missingBtn, 3);
   statusGroup->addButton(unusedBtn, 4);

   // Status filters remain available to the model/context menu, but the main
   // browser header follows the mockup's compact type-first presentation.
   for (auto* button : {statusAllBtn, importedBtn, favoriteBtn, missingBtn,
                        unusedBtn}) {
    button->hide();
   }

   for (auto *filterButton : {allButton, imagesButton, videosButton,
                              audioButton, fontsButton, statusAllBtn,
                              importedBtn, favoriteBtn, missingBtn, unusedBtn}) {
    filterButton->setAutoRaise(true);
    filterButton->setMinimumHeight(26);
    filterButton->setCursor(Qt::PointingHandCursor);
    applyAssetBrowserFilterPalette(filterButton);
   }

   assetToolBar->addSeparator();

   // Sort by combo box
   auto* sortByCombo = new QComboBox();
   sortByCombo->addItem("Sort: Name", "name");
   sortByCombo->addItem("Sort: Date", "date");
   sortByCombo->addItem("Sort: Size", "size");
   sortByCombo->addItem("Sort: Type", "type");
   sortByCombo->setCurrentIndex(1);
   sortByCombo->setMinimumWidth(112);
   sortByCombo->setMinimumHeight(28);
   sortByCombo->setToolTip(QStringLiteral("Sort visible assets"));
   assetToolBar->addWidget(sortByCombo);

   // Sort order toggle button
   auto* sortOrderBtn = new QToolButton();
   sortOrderBtn->setText("\u2191"); // Up arrow
   sortOrderBtn->setCheckable(true);
   sortOrderBtn->setChecked(false);
   sortOrderBtn->setText("↓");
   sortOrderBtn->setFixedWidth(30);
   sortOrderBtn->setToolTip("Sort Order: Ascending/Descending");
   assetToolBar->addWidget(sortOrderBtn);

   connect(statusGroup, &QButtonGroup::idClicked, this, [this](int id) {
    switch (id) {
     case 0: impl_->currentStatusFilter_ = "all"; break;
     case 1: impl_->currentStatusFilter_ = "imported"; break;
     case 2: impl_->currentStatusFilter_ = "favorite"; break;
     case 3: impl_->currentStatusFilter_ = "missing"; break;
     case 4: impl_->currentStatusFilter_ = "unused"; break;
    }
    impl_->applyFilters();
    impl_->refreshLeftHubSummary();
   });

   connect(sortByCombo, &QComboBox::currentIndexChanged, this, [this, sortByCombo](int) {
    impl_->currentSortBy_ = sortByCombo->currentData().toString();
    impl_->applyFilters();
   });

   connect(sortOrderBtn, &QToolButton::toggled, this, [this, sortOrderBtn](bool checked) {
    impl_->sortAscending_ = checked;
    sortOrderBtn->setText(checked ? "\u2191" : "\u2193");
    impl_->applyFilters();
   });

  auto vLayout = new QVBoxLayout();
  vLayout->setContentsMargins(0, 0, 0, 0);
  vLayout->setSpacing(0);

  auto layout = new QHBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  auto directoryView = impl_->directoryView_ = new QTreeView();
  auto directoryModel = impl_->directoryModel_ = new AssetDirectoryModel(this);

  QString assetsPath = ArtifactProjectManager::getInstance().currentProjectAssetsPath();
  if (assetsPath.isEmpty()) {
   assetsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Assets";
  }
  QString packagesPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Packages";

  directoryModel->setAssetRootPath(assetsPath);
  directoryModel->setPackageRootPath(packagesPath);

  directoryView->setModel(directoryModel);
  directoryView->setHeaderHidden(true);
  directoryView->setIndentation(16);
  directoryView->setMinimumWidth(156);
  directoryView->setMaximumWidth(224);
  directoryView->setUniformRowHeights(true);
  directoryView->setAlternatingRowColors(false);
  directoryView->setExpandsOnDoubleClick(true);
   directoryView->setAnimated(true);
   directoryView->setAcceptDrops(true);
   directoryView->setDropIndicatorShown(true);
   directoryView->setDragDropMode(QAbstractItemView::DropOnly);

  QString desktopPath = assetsPath;

  auto* breadcrumbBar = impl_->breadcrumb_ = new ArtifactBreadcrumbWidget(this);
  breadcrumbBar->setRootPath(assetsPath);
  breadcrumbBar->setPath(desktopPath);
  breadcrumbBar->setToolTip(QStringLiteral("Current asset folder"));
  connect(breadcrumbBar, &ArtifactBreadcrumbWidget::pathClicked, this,
          [this](const QString& path) {
            navigateToFolder(path);
          });

  impl_->syncStateLabel_ = new QLabel(impl_->syncStateText(), this);
  impl_->syncStateLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  impl_->syncStateLabel_->setMinimumHeight(24);
  applyAssetBrowserPanelPalette(impl_->syncStateLabel_);

  auto* leftHubCard = makeAssetBrowserPanel(this);
  auto* leftHubLayout = new QVBoxLayout();
  leftHubLayout->setContentsMargins(10, 10, 10, 8);
  leftHubLayout->setSpacing(5);
  auto* leftHubTitle = new QLabel(QStringLiteral("Sources"), leftHubCard);
  {
   QFont font = leftHubTitle->font();
   font.setBold(true);
   leftHubTitle->setFont(font);
  }
  impl_->currentPathLabel_ = new QLabel(QStringLiteral("Current: %1").arg(desktopPath), leftHubCard);
  impl_->leftHubSummaryLabel_ = new QLabel(leftHubCard);
  impl_->leftHubRecentLabel_ = new QLabel(leftHubCard);
  impl_->leftHubSelectionLabel_ = new QLabel(leftHubCard);
  impl_->recentFolderButtons_.clear();
  impl_->currentPathLabel_->setWordWrap(false);
  impl_->leftHubSummaryLabel_->setWordWrap(true);
  impl_->leftHubRecentLabel_->setWordWrap(true);
  impl_->leftHubSelectionLabel_->setWordWrap(true);
  impl_->currentPathLabel_->setMaximumHeight(24);
  impl_->leftHubSummaryLabel_->setMaximumHeight(40);
  impl_->leftHubRecentLabel_->setMaximumHeight(40);
  impl_->leftHubSelectionLabel_->setMaximumHeight(40);
  impl_->currentPathLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  impl_->leftHubSummaryLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  impl_->leftHubRecentLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  impl_->leftHubSelectionLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  applyAssetBrowserPanelPalette(leftHubTitle);
  applyAssetBrowserPanelPalette(impl_->currentPathLabel_);
  applyAssetBrowserPanelPalette(impl_->leftHubSummaryLabel_);
  applyAssetBrowserPanelPalette(impl_->leftHubRecentLabel_);
  applyAssetBrowserPanelPalette(impl_->leftHubSelectionLabel_);
  leftHubLayout->addWidget(leftHubTitle);
  auto* leftHubSection = new QLabel(QStringLiteral("FAVORITES"), leftHubCard);
  {
   QFont font = leftHubSection->font();
   font.setPointSizeF(std::max<qreal>(8.0, font.pointSizeF() - 1.0));
   font.setWeight(QFont::DemiBold);
   leftHubSection->setFont(font);
   QPalette pal = leftHubSection->palette();
   pal.setColor(QPalette::WindowText,
                QColor(ArtifactCore::currentDCCTheme().textColor).darker(135));
   leftHubSection->setPalette(pal);
  }
  leftHubLayout->addWidget(leftHubSection);
  auto* allFavoritesButton = new QToolButton(leftHubCard);
  allFavoritesButton->setText(QStringLiteral("All Favorites"));
  allFavoritesButton->setIcon(QIcon(QStringLiteral(":/icons/Studio/shape_star.svg")));
  allFavoritesButton->setIconSize(QSize(16, 16));
  allFavoritesButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  allFavoritesButton->setAutoRaise(true);
  allFavoritesButton->setCursor(Qt::PointingHandCursor);
  leftHubLayout->addWidget(allFavoritesButton);
  connect(allFavoritesButton, &QToolButton::clicked, favoriteBtn,
          &QToolButton::click);
  impl_->currentPathLabel_->hide();
  impl_->leftHubSummaryLabel_->hide();
  impl_->leftHubRecentLabel_->hide();
  impl_->leftHubSelectionLabel_->hide();
  auto* leftHubRecentSection = new QLabel(QStringLiteral("RECENT"), leftHubCard);
  {
   QFont font = leftHubRecentSection->font();
   font.setPointSizeF(std::max<qreal>(8.0, font.pointSizeF() - 1.0));
   font.setWeight(QFont::DemiBold);
   leftHubRecentSection->setFont(font);
   QPalette pal = leftHubRecentSection->palette();
   pal.setColor(QPalette::WindowText,
                QColor(ArtifactCore::currentDCCTheme().textColor).darker(135));
   leftHubRecentSection->setPalette(pal);
  }
  leftHubLayout->addWidget(leftHubRecentSection);
  for (int i = 0; i < 3; ++i) {
   auto* recentButton = new RecentFolderButton(leftHubCard);
   recentButton->setVisible(false);
   leftHubLayout->addWidget(recentButton);
   impl_->recentFolderButtons_.append(recentButton);
  }
  leftHubCard->setLayout(leftHubLayout);

  auto assetModel = impl_->assetModel_ = new AssetMenuModel(this);
  auto fileView = impl_->fileView_ = new AssetFileListView();
  fileView->setModel(assetModel);
  fileView->setItemDelegate(new AssetCardDelegate(fileView));
  impl_->currentDirectoryPath_ = desktopPath;  // Set initial directory
  fileView->setResizeMode(QListView::Adjust);
  fileView->setTextElideMode(Qt::ElideRight);
  fileView->setUniformItemSizes(true);  // Optimize rendering with uniform sizes
  fileView->setDragEnabled(true);
  fileView->setAcceptDrops(true);
  fileView->setDropIndicatorShown(true);
  fileView->setDragDropMode(QAbstractItemView::DragDrop);
  fileView->setDefaultDropAction(Qt::CopyAction);
  fileView->setSelectionMode(QAbstractItemView::ExtendedSelection);
  fileView->setContextMenuPolicy(Qt::CustomContextMenu);  // Enable custom context menu
  fileView->setMouseTracking(true);
  fileView->viewport()->setMouseTracking(true);
  fileView->viewport()->installEventFilter(this);
  fileView->setContentsMargins(8, 8, 8, 8);
  applyAssetBrowserViewMode(fileView, QListView::IconMode,
                            impl_->thumbnailSizePx());
  {
   QPalette palette = fileView->palette();
   const auto& theme = ArtifactCore::currentDCCTheme();
   palette.setColor(QPalette::Base, QColor(12, 17, 22));
   palette.setColor(QPalette::AlternateBase, QColor(24, 30, 37));
   palette.setColor(QPalette::Text, QColor(theme.textColor));
   palette.setColor(QPalette::Highlight, QColor(theme.accentColor).darker(145));
   palette.setColor(QPalette::HighlightedText, QColor(0xF5, 0xF7, 0xFA));
   fileView->setPalette(palette);
   fileView->viewport()->setAutoFillBackground(true);
  }

  auto* viewModeGroup = new QButtonGroup(this);
  viewModeGroup->setExclusive(true);
  if (gridViewButton) {
    viewModeGroup->addButton(gridViewButton, static_cast<int>(QListView::IconMode));
  }
  if (listViewButton) {
    viewModeGroup->addButton(listViewButton, static_cast<int>(QListView::ListMode));
  }
  connect(viewModeGroup, &QButtonGroup::idClicked, this, [this, fileView](int id) {
    applyAssetBrowserViewMode(fileView, static_cast<QListView::ViewMode>(id),
                              impl_->thumbnailSizePx());
  });

  // Connect search filter
  if (impl_->searchEdit_) {
   connect(impl_->searchEdit_, &QLineEdit::textChanged, this, [this](const QString& text) {
    impl_->currentSearchFilter_ = text;
    impl_->applyFilters();
   });
  }

  if (impl_->upButton_) {
   connect(impl_->upButton_, &QToolButton::clicked, this, [this]() {
    if (impl_->currentDirectoryPath_.isEmpty()) return;
    const QString assetsRoot = ArtifactProjectManager::getInstance().currentProjectAssetsPath();
    const QDir currentDir(impl_->currentDirectoryPath_);
    QString nextPath = QFileInfo(currentDir.absolutePath()).dir().absolutePath();
    if (nextPath.isEmpty()) {
     nextPath = assetsRoot;
    }
    if (!assetsRoot.isEmpty() && !nextPath.startsWith(assetsRoot, Qt::CaseInsensitive)) {
     nextPath = assetsRoot;
    }
    if (nextPath.isEmpty() || nextPath == impl_->currentDirectoryPath_) return;
    navigateToFolder(nextPath);
   });
  }

  if (impl_->refreshButton_) {
   connect(impl_->refreshButton_, &QToolButton::clicked, this, [this]() {
    impl_->clearThumbnailCache();
    impl_->applyFilters();
   });
  }

  // Connect file type filter buttons
  connect(impl_->filterButtonGroup_, &QButtonGroup::idClicked, this, [this](int id) {
   switch(id) {
    case 0: impl_->currentFileTypeFilter_ = "all"; break;
   case 1: impl_->currentFileTypeFilter_ = "images"; break;
   case 2: impl_->currentFileTypeFilter_ = "videos"; break;
   case 3: impl_->currentFileTypeFilter_ = "audio"; break;
   case 4: impl_->currentFileTypeFilter_ = "3d"; break;
   }
   impl_->applyFilters();
  });

  // Connect directory change to update file list (LEFT -> RIGHT widget coordination)
  connect(directoryView, &QTreeView::clicked, this, [this, directoryModel](const QModelIndex& index) {
   QString path = directoryModel->pathFromIndex(index);

   if (!path.isEmpty()) {
     navigateToFolder(path);
    }
   });

  // Connect file double-click to add to project or navigate into folder
  connect(fileView, &QListView::doubleClicked, this, [this](const QModelIndex& index) {
   if (!index.isValid()) return;
   AssetMenuItem item = impl_->assetModel_->itemAt(index.row());
   QString filePath = item.path.toQString();
   if (filePath.isEmpty()) return;
   itemDoubleClicked(filePath);

   // If it's a folder, navigate into it
   if (item.isFolder) {
    navigateToFolder(filePath);
    return;
   }

   // Otherwise, add file to project
   auto* svc = ArtifactProjectService::instance();
   if (!svc) return;
   svc->importAssetsFromPaths(QStringList() << filePath);
  });

  // Connect right-click context menu
  connect(fileView, &QListView::customContextMenuRequested, this, &ArtifactAssetBrowser::showContextMenu);

  // Connect file item selection to update details
  connect(fileView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
   QModelIndexList selectedIndexes = impl_->fileView_->selectionModel()->selectedIndexes();
   QStringList selectedFiles;
   selectedFiles.reserve(selectedIndexes.size());
   if (!selectedIndexes.isEmpty()) {
    for (const QModelIndex& index : selectedIndexes) {
     const AssetMenuItem item = impl_->assetModel_->itemAt(index.row());
     const QString filePath = item.path.toQString();
     if (!filePath.isEmpty()) {
      selectedFiles.append(filePath);
     }
    }
    if (!selectedFiles.isEmpty()) {
     updateFileInfo(selectedFiles.first());
    }
   } else if (impl_->fileInfoLabel_) {
    impl_->fileInfoLabel_->setText(QStringLiteral("Open a file to inspect details"));
    if (impl_->filePreviewLabel_) {
     impl_->filePreviewLabel_->setPixmap(QPixmap());
     impl_->filePreviewLabel_->setText(QStringLiteral("Select an asset to preview"));
    }
   }
   selectionChanged(selectedFiles);
   impl_->refreshLeftHubSummary();
  });

  // Create thumbnail size adjustment
  auto thumbnailControlGroup = makeAssetBrowserPanel(this);
  auto thumbnailLayout = new QHBoxLayout();
  thumbnailLayout->setContentsMargins(8, 5, 8, 5);
  thumbnailLayout->setSpacing(6);

  auto sizeLabel = new QLabel(QStringLiteral("%1px").arg(kAssetThumbnailDefaultPx));
  auto sizeSlider = impl_->thumbnailSizeSlider_ = new QSlider(Qt::Horizontal);
  sizeSlider->setMinimum(kAssetThumbnailMinPx);
  sizeSlider->setMaximum(kAssetThumbnailMaxPx);
  sizeSlider->setValue(kAssetThumbnailDefaultPx);
  sizeSlider->setTickPosition(QSlider::TicksBelow);
  sizeSlider->setTickInterval(25);

  connect(sizeSlider, &QSlider::valueChanged, this, [this, sizeLabel, fileView](int value) {
    sizeLabel->setText(QString("%1px").arg(value));
    impl_->setThumbnailSizePx(value);
    applyAssetBrowserViewMode(fileView, fileView->viewMode(), value);
  });
  connect(sizeSlider, &QSlider::sliderReleased, this, [this]() {
    impl_->clearThumbnailCache();
    impl_->applyFilters();
  });

  thumbnailLayout->addWidget(new QLabel("Thumbnail:"));
  thumbnailLayout->addWidget(sizeSlider);
  thumbnailLayout->addWidget(sizeLabel);
  thumbnailControlGroup->setLayout(thumbnailLayout);

  // Create file info panel
  auto fileInfoGroup = makeAssetBrowserPanel(this);
  auto fileInfoLayout = new QHBoxLayout();
  fileInfoLayout->setContentsMargins(10, 10, 10, 10);
  fileInfoLayout->setSpacing(14);

  auto* filePreviewLabel = impl_->filePreviewLabel_ = new QLabel(fileInfoGroup);
  filePreviewLabel->setAlignment(Qt::AlignCenter);
  filePreviewLabel->setFixedSize(280, 164);
  filePreviewLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  filePreviewLabel->setText(QStringLiteral("Select an asset to preview"));
  {
   QPalette pal = filePreviewLabel->palette();
   const auto& theme = ArtifactCore::currentDCCTheme();
   pal.setColor(QPalette::Window, QColor(theme.secondaryBackgroundColor).darker(112));
   pal.setColor(QPalette::WindowText, QColor(theme.textColor).darker(135));
   filePreviewLabel->setAutoFillBackground(true);
   filePreviewLabel->setPalette(pal);
  }

  auto fileInfoLabel = impl_->fileInfoLabel_ = new QLabel(QStringLiteral("Open a file to inspect details"));
  fileInfoLabel->setWordWrap(true);
  fileInfoLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  {
    QPalette pal = fileInfoLabel->palette();
    pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(130));
    pal.setColor(QPalette::Text, QColor(ArtifactCore::currentDCCTheme().textColor).darker(130));
    fileInfoLabel->setPalette(pal);
  }

  applyAssetBrowserPanelPalette(fileInfoLabel);

  fileInfoLayout->addWidget(filePreviewLabel);
  auto* detailsColumn = new QWidget(fileInfoGroup);
  auto* detailsLayout = new QVBoxLayout(detailsColumn);
  detailsLayout->setContentsMargins(0, 0, 0, 0);
  detailsLayout->setSpacing(6);
  auto* fileInfoTitle = new QLabel(QStringLiteral("Asset Details"), detailsColumn);
  {
   QFont font = fileInfoTitle->font();
   font.setBold(true);
   font.setPointSizeF(std::max<qreal>(11.0, font.pointSizeF()));
   fileInfoTitle->setFont(font);
  }
  applyAssetBrowserPanelPalette(fileInfoTitle);
  detailsLayout->addWidget(fileInfoTitle);
  detailsLayout->addWidget(fileInfoLabel, 1);
  auto* importButton = new QPushButton(QStringLiteral("Import"), detailsColumn);
  importButton->setMinimumWidth(92);
  importButton->setMinimumHeight(30);
  importButton->setIcon(style()->standardIcon(QStyle::SP_ArrowDown));
  {
   QPalette pal = importButton->palette();
   const auto& theme = ArtifactCore::currentDCCTheme();
   pal.setColor(QPalette::Button, QColor(theme.accentColor));
   pal.setColor(QPalette::ButtonText, QColor(Qt::white));
   pal.setColor(QPalette::Highlight, QColor(theme.accentColor).lighter(112));
   pal.setColor(QPalette::HighlightedText, QColor(Qt::white));
   importButton->setAutoFillBackground(true);
   importButton->setPalette(pal);
  }
  importButton->setEnabled(false);
  detailsLayout->addWidget(importButton, 0, Qt::AlignRight);
  fileInfoLayout->addWidget(detailsColumn, 1);
  fileInfoGroup->setLayout(fileInfoLayout);
  fileInfoGroup->setFixedHeight(188);

  connect(fileView->selectionModel(), &QItemSelectionModel::selectionChanged,
          this, [fileView, importButton]() {
            importButton->setEnabled(
                fileView->selectionModel() &&
                !fileView->selectionModel()->selectedIndexes().isEmpty());
          });
  connect(importButton, &QPushButton::clicked, this, [this]() {
    if (!impl_ || !impl_->fileView_ || !impl_->fileView_->selectionModel()) {
      return;
    }
    QStringList paths;
    for (const QModelIndex& index :
         impl_->fileView_->selectionModel()->selectedIndexes()) {
      const AssetMenuItem item = impl_->assetModel_->itemAt(index.row());
      if (!item.isFolder && !item.path.toQString().isEmpty()) {
        paths.append(item.path.toQString());
      }
    }
    if (auto* service = ArtifactProjectService::instance();
        service && !paths.isEmpty()) {
      service->importAssetsFromPaths(paths);
    }
  });

  // Initial load
  impl_->applyFilters();

  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<ProjectCreatedEvent>([this](const ProjectCreatedEvent&) {
        impl_->syncProjectAssetRoot();
        if (impl_->syncStateLabel_) {
          impl_->syncStateLabel_->setText(impl_->syncStateText());
        }
      }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<ProjectChangedEvent>([this](const ProjectChangedEvent&) {
        impl_->syncProjectAssetRoot();
        if (impl_->syncStateLabel_) {
          impl_->syncStateLabel_->setText(impl_->syncStateText());
        }
      }));

  auto* navigationHeader = makeAssetBrowserPanel(this);
  auto* navigationHeaderLayout = new QVBoxLayout(navigationHeader);
  navigationHeaderLayout->setContentsMargins(12, 10, 12, 9);
  navigationHeaderLayout->setSpacing(8);
  auto* browserTitle = new QLabel(QStringLiteral("Asset Browser"), navigationHeader);
  {
   QFont font = browserTitle->font();
   font.setPointSizeF(std::max<qreal>(12.0, font.pointSizeF()));
   font.setBold(true);
   browserTitle->setFont(font);
   applyAssetBrowserPanelPalette(browserTitle);
  }
  auto* titleRow = new QHBoxLayout();
  titleRow->setContentsMargins(0, 0, 0, 0);
  titleRow->setSpacing(16);
  titleRow->addWidget(browserTitle);
  titleRow->addWidget(breadcrumbBar, 1);
  navigationHeaderLayout->addLayout(titleRow);
  navigationHeaderLayout->addWidget(assetToolBar);
  navigationHeaderLayout->addLayout(typeFiltersLayout);

  auto* browserSurface = makeAssetBrowserPanel(this);
  auto* VBoxLayout = new QVBoxLayout(browserSurface);
  VBoxLayout->setContentsMargins(0, 0, 0, 0);
  VBoxLayout->setSpacing(0);
  impl_->syncStateLabel_->hide();
  VBoxLayout->addWidget(fileView);
  VBoxLayout->addWidget(fileInfoGroup);
  thumbnailControlGroup->hide();

  auto leftColumnLayout = new QVBoxLayout();
  leftColumnLayout->setContentsMargins(0, 0, 0, 0);
  leftColumnLayout->setSpacing(0);
  leftColumnLayout->addWidget(leftHubCard);
  auto* localSection = new QLabel(QStringLiteral("LOCAL"), this);
  {
   QFont font = localSection->font();
   font.setPointSizeF(std::max<qreal>(8.0, font.pointSizeF() - 1.0));
   font.setWeight(QFont::DemiBold);
   localSection->setFont(font);
   QPalette pal = localSection->palette();
   pal.setColor(QPalette::Window, QColor(18, 23, 29));
   pal.setColor(QPalette::WindowText,
                QColor(ArtifactCore::currentDCCTheme().textColor).darker(135));
   localSection->setAutoFillBackground(true);
   localSection->setPalette(pal);
   localSection->setContentsMargins(10, 9, 10, 5);
  }
  leftColumnLayout->addWidget(localSection);
  leftColumnLayout->addWidget(directoryView, 1);

  vLayout->addWidget(navigationHeader);
  layout->addLayout(leftColumnLayout, 2);
  layout->addWidget(browserSurface, 7);
  vLayout->addLayout(layout);
  setLayout(vLayout);

  impl_->refreshLeftHubSummary();

  impl_->setupFileSystemWatcher();
 }

 ArtifactAssetBrowser::~ArtifactAssetBrowser()
 {
  delete impl_;
 }

 QSize ArtifactAssetBrowser::sizeHint() const
 {
  return QSize(250, 600);
 }

 W_OBJECT_IMPL(ArtifactAssetBrowser)

 void ArtifactAssetBrowser::mousePressEvent(QMouseEvent* event)
 {
  impl_->defaultHandleMousePressEvent(event);
 }

 void ArtifactAssetBrowser::focusInEvent(QFocusEvent* event)
 {
  if (auto* input = InputOperator::instance()) {
   input->setActiveContext(QString::fromLatin1(kAssetBrowserContext));
  }
  QWidget::focusInEvent(event);
 }

 void ArtifactAssetBrowser::focusOutEvent(QFocusEvent* event)
 {
  if (auto* input = InputOperator::instance()) {
   if (input->activeContext() == QString::fromLatin1(kAssetBrowserContext)) {
    input->setActiveContext(QStringLiteral("Global"));
   }
  }
  QWidget::focusOutEvent(event);
 }

 void ArtifactAssetBrowser::keyPressEvent(QKeyEvent* event)
 {
  if (!impl_->fileView_ || !impl_->assetModel_) {
   QWidget::keyPressEvent(event);
   return;
  }

  if (auto* input = InputOperator::instance()) {
   input->setActiveContext(QString::fromLatin1(kAssetBrowserContext));
   if (event && input->processKeyPress(this, event->key(), event->modifiers())) {
    event->accept();
    return;
   }
  }

  const auto* sel = impl_->fileView_->selectionModel();

  // Ctrl+A — 全選択
  if (event->key() == Qt::Key_A && (event->modifiers() & Qt::ControlModifier)) {
   impl_->fileView_->selectAll();
   event->accept();
   return;
  }

  // Ctrl+C — パスをコピー
  if (event->key() == Qt::Key_C && (event->modifiers() & Qt::ControlModifier)) {
   QStringList paths = impl_->selectedAssetPaths();
   if (!paths.isEmpty()) {
    QApplication::clipboard()->setText(paths.join("\n"));
   }
   event->accept();
   return;
  }

  // Ctrl+V — クリップボードのファイルをインポート
  if (event->key() == Qt::Key_V && (event->modifiers() & Qt::ControlModifier)) {
   const QMimeData* mime = QApplication::clipboard()->mimeData();
   if (mime && mime->hasUrls()) {
    QStringList paths;
    for (const QUrl& url : mime->urls()) {
     if (url.isLocalFile()) paths.append(url.toLocalFile());
    }
    if (!paths.isEmpty() && ArtifactProjectService::instance()) {
     ArtifactProjectService::instance()->importAssetsFromPaths(paths);
    }
   }
   event->accept();
   return;
  }

  // Delete — 選択ファイルをプロジェクトから削除 + 物理ファイル削除
  if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
   if (!(event->modifiers() & Qt::ControlModifier) && !(event->modifiers() & Qt::ShiftModifier)) {
    impl_->deleteSelected();
    event->accept();
    return;
   }
  }

  // F2 — リネーム
  if (event->key() == Qt::Key_F2) {
   impl_->renameSelected();
   event->accept();
   return;
  }

  // Ctrl+Shift+N — 新規フォルダ作成
  if (event->key() == Qt::Key_N && (event->modifiers() & Qt::ControlModifier) && (event->modifiers() & Qt::ShiftModifier)) {
   impl_->createNewFolder();
   event->accept();
   return;
  }

  // Enter — ダブルクリック相当
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
   if (sel && sel->hasSelection()) {
    QModelIndex idx = sel->currentIndex();
    if (idx.isValid()) {
     impl_->handleDoubleClicked();
    }
   }
   event->accept();
   return;
  }

  QWidget::keyPressEvent(event);
 }

void ArtifactAssetBrowser::keyReleaseEvent(QKeyEvent* event)
{
}

bool ArtifactAssetBrowser::eventFilter(QObject* watched, QEvent* event)
{
  if (watched == impl_->searchEdit_ && event && event->type() == QEvent::KeyPress) {
   auto* keyEvent = static_cast<QKeyEvent*>(event);
   if (keyEvent->key() == Qt::Key_Escape && impl_->searchEdit_ && !impl_->searchEdit_->text().isEmpty()) {
    impl_->searchEdit_->clear();
    return true;
    }
  }

  if (impl_ && impl_->fileView_ && watched == impl_->fileView_->viewport() && event) {
    switch (event->type()) {
      case QEvent::Leave:
        impl_->hideHoverPreview();
        break;
      case QEvent::MouseMove: {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (!mouseEvent) {
          break;
        }
        const QModelIndex index = impl_->fileView_->indexAt(mouseEvent->pos());
        if (!index.isValid()) {
          impl_->hideHoverPreview();
          break;
        }
        const AssetMenuItem item = impl_->assetModel_->itemAt(index.row());
        const QString filePath = item.path.toQString();
        if (filePath.isEmpty() || impl_->isHoverPreviewPath(filePath)) {
          break;
        }
        impl_->scheduleHoverPreview(filePath, mouseEvent->globalPosition().toPoint());
        break;
      }
      default:
        break;
    }
  }

  return QWidget::eventFilter(watched, event);
}

void ArtifactAssetBrowser::dragEnterEvent(QDragEnterEvent* event)
{
  // Accept file drops from external sources
  if (event->mimeData()->hasUrls()) {
   event->acceptProposedAction();
  }
 }

void ArtifactAssetBrowser::selectAssetPaths(const QStringList& filePaths)
{
  if (!impl_ || !impl_->fileView_ || !impl_->assetModel_) {
   return;
  }

  QStringList normalizedPaths;
  normalizedPaths.reserve(filePaths.size());
  for (const QString& filePath : filePaths) {
   const QString normalized = normalizeAssetPath(filePath);
   if (!normalized.isEmpty()) {
    normalizedPaths.append(normalized);
   }
  }
  normalizedPaths.removeDuplicates();
  if (normalizedPaths.isEmpty()) {
   return;
  }

  const QString targetFolder = QFileInfo(normalizedPaths.first()).absolutePath();
  if (!targetFolder.isEmpty() && targetFolder != impl_->currentDirectoryPath_) {
   navigateToFolder(targetFolder);
  } else {
   impl_->applyFilters();
  }

  auto* selection = impl_->fileView_->selectionModel();
  if (!selection) {
   return;
  }

  QSignalBlocker blocker(selection);
  selection->clearSelection();
  QModelIndex firstSelected;
  for (int row = 0; row < impl_->assetModel_->rowCount(); ++row) {
   const AssetMenuItem item = impl_->assetModel_->itemAt(row);
   if (item.isFolder) {
    continue;
   }
   const QString itemPath = normalizeAssetPath(item.path.toQString());
   if (itemPath.isEmpty() || !normalizedPaths.contains(itemPath)) {
    continue;
   }
   const QModelIndex index = impl_->assetModel_->index(row, 0);
   if (!index.isValid()) {
    continue;
   }
   selection->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
   if (!firstSelected.isValid()) {
    firstSelected = index;
   }
  }

  if (firstSelected.isValid()) {
   impl_->fileView_->setCurrentIndex(firstSelected);
   updateFileInfo(normalizedPaths.first());
   emit selectionChanged(normalizedPaths);
  }
  impl_->refreshLeftHubSummary();
}

 void ArtifactAssetBrowser::dropEvent(QDropEvent* event)
 {
  const QMimeData* mimeData = event->mimeData();

  if (mimeData->hasUrls()) {
   QStringList filePaths;
   QList<QUrl> urls = mimeData->urls();

   for (const QUrl& url : urls) {
    if (url.isLocalFile()) {
     QString filePath = url.toLocalFile();
     filePaths.append(filePath);
    }
   }

   if (!filePaths.isEmpty()) {
    auto* svc = ArtifactProjectService::instance();
    if (svc) {
     QStringList imported = svc->importAssetsFromPaths(filePaths);
     if (!imported.isEmpty()) {
      filesDropped(imported);
     }
    }
    // Refresh file view
    impl_->applyFilters();
   }

   event->acceptProposedAction();
  }
 }

 void ArtifactAssetBrowser::setSearchFilter(const QString& filter)
 {
  impl_->currentSearchFilter_ = filter;
  if (impl_->searchEdit_) {
   impl_->searchEdit_->setText(filter);
  }
  impl_->applyFilters();
 }

  void ArtifactAssetBrowser::setFileTypeFilter(const QString& type)
  {
   impl_->currentFileTypeFilter_ = type;

   // Update button state
   if (impl_->filterButtonGroup_) {
    if (type == "all") impl_->filterButtonGroup_->button(0)->setChecked(true);
    else if (type == "images") impl_->filterButtonGroup_->button(1)->setChecked(true);
    else if (type == "videos") impl_->filterButtonGroup_->button(2)->setChecked(true);
    else if (type == "audio") impl_->filterButtonGroup_->button(3)->setChecked(true);
    else if (type == "3d") impl_->filterButtonGroup_->button(4)->setChecked(true);
   }

   impl_->applyFilters();
  }

  void ArtifactAssetBrowser::setStatusFilter(const QString& status)
  {
   impl_->currentStatusFilter_ = status;
   impl_->applyFilters();
   impl_->refreshLeftHubSummary();
  }

 void ArtifactAssetBrowser::navigateToFolder(const QString& folderPath)
  {
  if (folderPath.isEmpty() || !QDir(folderPath).exists()) return;
  impl_->currentDirectoryPath_ = folderPath;
  if (impl_->directoryModel_) {
   impl_->directoryModel_->addRecentPath(folderPath, QFileInfo(folderPath).fileName());
  }
  impl_->watchCurrentDirectory();
  impl_->clearThumbnailCache();
  if (impl_->breadcrumb_) {
   impl_->breadcrumb_->setPath(folderPath);
  }
  impl_->applyFilters();
  impl_->syncDirectorySelection();
  impl_->refreshLeftHubSummary();
  folderChanged(folderPath);
 }

 void ArtifactAssetBrowser::updateFileInfo(const QString& filePath)
 {
  if (filePath.isEmpty() || !impl_->fileInfoLabel_) return;

  QFileInfo fileInfo(filePath);

 if (!fileInfo.exists()) {
   impl_->fileInfoLabel_->setText("File not found");
   if (impl_->filePreviewLabel_) {
    impl_->filePreviewLabel_->setPixmap(QPixmap());
    impl_->filePreviewLabel_->setText(QStringLiteral("Preview unavailable"));
   }
   return;
  }

  if (impl_->filePreviewLabel_) {
   const QSize previewSize(std::max(160, impl_->filePreviewLabel_->width() - 12), 144);
   const QIcon previewIcon = impl_->getFileIcon(fileInfo.fileName(), filePath);
   const QPixmap previewPixmap = previewIcon.pixmap(previewSize);
   if (!previewPixmap.isNull()) {
    impl_->filePreviewLabel_->setText(QString());
    impl_->filePreviewLabel_->setPixmap(previewPixmap.scaled(
        previewSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
   } else {
    impl_->filePreviewLabel_->setPixmap(QPixmap());
    impl_->filePreviewLabel_->setText(fileInfo.isDir()
        ? QStringLiteral("Folder")
        : QStringLiteral("Preview unavailable"));
   }
  }

// Build information string
   QString info;
   info += QString("<b>%1</b><br>").arg(fileInfo.fileName());

   // Check if it's a folder
  if (fileInfo.isDir()) {
    info += "Type: Folder<br>";
    info += QString("Entries: %1<br>").arg(QDir(filePath).entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot).size());
    info += QString("Favorite: %1<br>").arg(impl_->isFavoriteAssetPath(filePath) ? QStringLiteral("Yes") : QStringLiteral("No"));
    info += QString("Project: %1<br>").arg(impl_->isImportedAssetPath(filePath) ? QStringLiteral("Imported") : QStringLiteral("Not Imported"));
    info += QString("Usage: %1<br>").arg(impl_->isUnusedAssetPath(filePath) ? QStringLiteral("Unused") : QStringLiteral("In Use / N.A."));
    info += QString("Status: %1<br>").arg(impl_->isMissingAssetPath(filePath) ? QStringLiteral("Missing") : QStringLiteral("OK"));
    impl_->fileInfoLabel_->setText(info);
    return;
   }

   info += QString("Size: %1 KB<br>").arg(fileInfo.size() / 1024);
   const QString lowerName = fileInfo.fileName().toLower();
   if (lowerName.endsWith(QStringLiteral(".mask.json"))) {
    info += QStringLiteral("Type: Mask Preset<br>");
   } else {
    info += QString("Type: %1<br>").arg(fileInfo.suffix().toUpper());
   }
   info += QString("Modified: %1<br>").arg(fileInfo.lastModified().toString("yyyy-MM-dd hh:mm"));
   info += QString("Favorite: %1<br>").arg(impl_->isFavoriteAssetPath(filePath) ? QStringLiteral("Yes") : QStringLiteral("No"));
   info += QString("Project: %1<br>").arg(impl_->isImportedAssetPath(filePath) ? QStringLiteral("Imported") : QStringLiteral("Not Imported"));
   info += QString("Source Uses: %1<br>").arg(impl_->sourceUseCountForPath(filePath));
   info += QString("Usage: %1<br>").arg(impl_->isUnusedAssetPath(filePath) ? QStringLiteral("Unused") : QStringLiteral("In Use"));
   info += QString("Status: %1<br>").arg(impl_->isMissingAssetPath(filePath) ? QStringLiteral("Missing") : QStringLiteral("OK"));
   info += QString("Thumbnail: %1<br>").arg(impl_->thumbnailDebugStatus(filePath).toHtmlEscaped());

   // Get detailed information based on file type
   const QString fileName = fileInfo.fileName();

   // Image files
   if (lowerName.endsWith(".png") || lowerName.endsWith(".jpg") ||
       lowerName.endsWith(".jpeg") || lowerName.endsWith(".bmp") ||
       lowerName.endsWith(".gif") || lowerName.endsWith(".tga") ||
       lowerName.endsWith(".tiff") || lowerName.endsWith(".exr")) {
    QSize imageSize;
    QString imageError;
    const QImage imagePreview = loadImageThumbnailViaOIIO(filePath, QSize(), &imageError);
    if (!imagePreview.isNull()) {
     imageSize = imagePreview.size();
    }
    if (imageSize.isValid()) {
     info += QString("Resolution: %1 x %2 px<br>").arg(imageSize.width()).arg(imageSize.height());
     const QString suffix = fileInfo.suffix().toUpper();
     if (!suffix.isEmpty()) {
      info += QString("Format: %1<br>").arg(suffix);
     }
    } else if (!imageError.isEmpty()) {
     info += QString("Image decode: %1<br>").arg(imageError);
    }
   }
   // Video files
   else if (impl_->isVideoFile(fileName)) {
    cv::VideoCapture cap(filePath.toLocal8Bit().constData());
    if (cap.isOpened()) {
     const double width = cap.get(cv::CAP_PROP_FRAME_WIDTH);
     const double height = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
     const double fps = cap.get(cv::CAP_PROP_FPS);
     const double frameCount = cap.get(cv::CAP_PROP_FRAME_COUNT);
     if (width > 0.0 && height > 0.0) {
      info += QString("Resolution: %1 x %2 px<br>").arg(static_cast<int>(width)).arg(static_cast<int>(height));
     }
     if (fps > 0.0) {
      info += QString("FPS: %1<br>").arg(QString::number(fps, 'f', fps == std::floor(fps) ? 0 : 2));
     }
     if (fps > 0.0 && frameCount > 0.0) {
      const double duration = frameCount / fps;
      info += QString("Duration: %1 s<br>").arg(QString::number(duration, 'f', 2));
     }
    }
   }
   // Audio files
   else if (impl_->isAudioFile(fileName)) {
    // Basic audio information
    info += QString("Kind: Audio<br>");
    // Additional audio metadata could be added here if needed
   }
   // Font files
   else if (impl_->isFontFile(fileName)) {
    info += QString("Kind: Font<br>");
    // Font-specific information could be added here if needed
   }
   else if (fileName.toLower().endsWith(QStringLiteral(".mask.json"))) {
    info += QString("Kind: Mask Preset<br>");
   }

   impl_->fileInfoLabel_->setText(info);
}

 void ArtifactAssetBrowser::showContextMenu(const QPoint& pos)
 {
  QModelIndex index = impl_->fileView_->indexAt(pos);
  if (!index.isValid()) return;  // No item under cursor

  AssetMenuItem item = impl_->assetModel_->itemAt(index.row());
  QString filePath = item.path.toQString();
  if (filePath.isEmpty()) return;

  // Create context menu
  QMenu contextMenu;
  QMenu* frequentMenu = contextMenu.addMenu(QStringLiteral("Frequent"));
  QMenu* allMenu = contextMenu.addMenu(QStringLiteral("All"));

  auto addAction = [this](QMenu* menu, const QString& text, auto&& callback) -> QAction* {
    if (!menu) {
      return nullptr;
    }
    QAction* action = menu->addAction(text);
    connect(action, &QAction::triggered, this, std::forward<decltype(callback)>(callback));
    return action;
  };

  // New Folder action (always available)
  addAction(frequentMenu, QStringLiteral("New Folder (Ctrl+Shift+N)"), [this]() {
   if (impl_) {
    impl_->createNewFolder();
   }
  });

  const QStringList selectedAssetPaths = impl_->selectedAssetPaths();
  const QStringList importTargets = selectedAssetPaths.isEmpty() ? QStringList{filePath} : selectedAssetPaths;

  // Add to Project action
  const QString addActionLabel = importTargets.size() > 1
   ? QStringLiteral("Add %1 Items to Project").arg(importTargets.size())
   : QStringLiteral("Add to Project");
  addAction(frequentMenu, addActionLabel, [this, importTargets, filePath]() {
   if (importTargets.isEmpty() && filePath.isEmpty()) return;
   auto* svc = ArtifactProjectService::instance();
   if (!svc) return;
   const QStringList imported = svc->importAssetsFromPaths(importTargets.isEmpty() ? QStringList{filePath} : importTargets);
   if (!imported.isEmpty()) {
    filesDropped(imported);
    impl_->applyFilters();
   }
  });

  if (!item.isFolder) {
   addAction(frequentMenu, QStringLiteral("Preview in Contents Viewer"), [this, filePath]() {
    if (filePath.isEmpty()) return;
    itemDoubleClicked(filePath);
   });
  }

if (item.isFolder) {
    addAction(frequentMenu, QStringLiteral("Open Folder"), [this, filePath]() {
     if (filePath.isEmpty()) return;
     impl_->currentDirectoryPath_ = filePath;
     impl_->clearThumbnailCache();
     impl_->applyFilters();
     impl_->syncDirectorySelection();
     folderChanged(filePath);
    });
   }

// Relink action for footage items
if (!item.isFolder) {
  addAction(allMenu, QStringLiteral("Relink Selected Footage..."), [this, filePath]() {
    if (filePath.isEmpty()) return;
    // Show file dialog to select new file path
    QString newPath = QFileDialog::getOpenFileName(nullptr, "Relink Footage", QDir::homePath(), "All Files (*.*)");
    if (newPath.isEmpty()) return;
    // Relink the footage item by path
    auto* svc = ArtifactProjectService::instance();
    if (!svc) return;
    bool success = svc->relinkFootageByPath(filePath, newPath);
    if (success) {
      impl_->applyFilters();
    }
  });
}

  // Interpret Footage action for media files
  if (!item.isFolder && (impl_->isVideoFile(filePath) || impl_->isImageFile(filePath))) {
    QAction* interpretAction = contextMenu.addAction("Interpret Footage...");
    connect(interpretAction, &QAction::triggered, this, [this, filePath]() {
      if (filePath.isEmpty()) return;
      auto* svc = ArtifactProjectService::instance();
      if (!svc) return;
      auto project = svc->getCurrentProjectSharedPtr();
      if (!project) return;
      Artifact::FootageItem* footage = nullptr;
      for (auto* root : project->projectItems()) {
        std::function<void(ProjectItem*)> walk = [&](ProjectItem* item) {
          if (!item) return;
          if (item->type() == Artifact::eProjectItemType::Footage) {
            auto* fi = static_cast<Artifact::FootageItem*>(item);
            if (fi->filePath == filePath || fi->sequencePaths.contains(filePath)) {
              footage = fi;
              return;
            }
          }
          for (auto* child : item->children) walk(child);
        };
        walk(root);
        if (footage) break;
      }
      if (!footage) return;
      auto& interpretSvc = FootageInterpretService::instance();
      auto report = interpretSvc.preflightChange(footage, footage->frameRate);
      ArtifactWidgets::InterpretFootageDialog dialog(
          QFileInfo(filePath).fileName(),
          footage->frameRate,
          footage->frameRate,
          report.affectedLayerCount,
          report.affectedKeyframeCount,
          report.hasTimeRemap);
      if (dialog.exec() == QDialog::Accepted) {
        double newFps = dialog.selectedFrameRate();
        int mode = dialog.selectedPreserveMode();
        FrameRatePreserveMode preserveMode;
        switch (mode) {
          case 0: preserveMode = FrameRatePreserveMode::KeepKeyframes; break;
          case 1: preserveMode = FrameRatePreserveMode::KeepTime; break;
          default: preserveMode = FrameRatePreserveMode::ReSample; break;
        }
        QString error;
        interpretSvc.applyFrameRateChange(footage, newFps, preserveMode, &error);
        if (error.isEmpty()) {
          interpretSvc.applyColorInterpretation(
              footage, dialog.selectedInputColorSpace(),
              dialog.selectedInputTransferFunction(), &error);
        }
        if (!error.isEmpty()) {
          QMessageBox::warning(nullptr, "Interpret Footage", error);
        }
        impl_->applyFilters();
      }
    });
  }

  // Open in File Explorer action
  // Open in File Explorer action
  const bool favorite = impl_->isFavoriteAssetPath(filePath);
  addAction(frequentMenu, favorite ? QStringLiteral("Remove from Favorites") : QStringLiteral("Add to Favorites"), [this, filePath]() {
   if (filePath.isEmpty()) return;
   if (!impl_->directoryModel_) return;
   impl_->toggleFavoritePath(filePath);
   impl_->applyFilters();
   impl_->syncDirectorySelection();
  });

  if (filePath.toLower().endsWith(QStringLiteral(".mask.json"))) {
    addAction(frequentMenu, QStringLiteral("Apply Mask Preset to Selected Layer"), [this, filePath]() {
      auto* app = ArtifactApplicationManager::instance();
      auto* selectionManager = app ? app->layerSelectionManager() : nullptr;
      if (!selectionManager) {
        QMessageBox::information(this, QStringLiteral("Mask Preset"),
                                 QStringLiteral("適用先レイヤーが見つかりません。"));
        return;
      }
      auto layer = selectionManager->currentLayer();
      if (!layer) {
        QMessageBox::information(this, QStringLiteral("Mask Preset"),
                                 QStringLiteral("先に適用先レイヤーを選択してください。"));
        return;
      }
      LayerMask mask;
      if (!ArtifactPresetManager::loadMaskPreset(mask, filePath)) {
        QMessageBox::warning(this, QStringLiteral("Mask Preset"),
                             QStringLiteral("マスクプリセットを読み込めませんでした。"));
        return;
      }
      const auto choice = QMessageBox::question(
          this, QStringLiteral("Mask Preset"),
          QStringLiteral("マスクを置換しますか？\n\n"
                         "Yes: 置換\nNo: 追加"),
          QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
      if (choice == QMessageBox::Yes) {
        layer->clearMasks();
      }
      layer->addMask(mask);
      layer->changed();
    });
  }

  // Open in File Explorer action
   addAction(allMenu, QStringLiteral("Open in File Explorer"), [filePath]() {
    QFileInfo fileInfo(filePath);
    QString folderPath = fileInfo.dir().absolutePath();
    QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath));
   });

  // Copy file path action
  addAction(frequentMenu, QStringLiteral("Copy File Path"), [filePath]() {
   QApplication::clipboard()->setText(filePath);
  });

  // Rename action (F2)
  addAction(allMenu, QStringLiteral("Rename (F2)"), [this]() {
   if (impl_) {
    impl_->renameSelected();
   }
  });

  // Delete action (Del)
  const QString deleteLabel = selectedAssetPaths.size() > 1
   ? QStringLiteral("Delete %1 Items (Del)").arg(selectedAssetPaths.size())
   : QStringLiteral("Delete (Del)");
  addAction(allMenu, deleteLabel, [this]() {
   if (impl_) {
    impl_->deleteSelected();
   }
  });

  // Show file properties action
  addAction(frequentMenu, QStringLiteral("Properties"), [filePath]() {
   QFileInfo fileInfo(filePath);
   QString info = QString("Name: %1\nSize: %2 bytes\nType: %3\nPath: %4")
     .arg(fileInfo.fileName())
     .arg(fileInfo.size())
     .arg(fileInfo.suffix())
     .arg(filePath);
   QMessageBox::information(nullptr,
                            QStringLiteral("Asset Properties"),
                            info);
  });

  // Show menu at cursor position
  contextMenu.exec(impl_->fileView_->mapToGlobal(pos));
 }

// ─────────────────────────────────────────────
// Audio waveform thumbnail generation
// ─────────────────────────────────────────────

ArtifactCore::AudioSegment ArtifactAssetBrowser::Impl::loadAudioFile(const QString& audioFilePath)
{
    ArtifactCore::AudioSegment segment;

    // Try to load via SimpleWav
    ArtifactCore::SimpleWav wav;
    if (!wav.loadFromFile(audioFilePath)) {
        qWarning() << "[AssetBrowser] Failed to load audio file:" << audioFilePath;
        return segment;
    }

    // Convert to AudioSegment - OPTIMIZED: only load first N seconds
    const int maxFrames = kMaxThumbnailAudioSeconds * wav.sampleRate();
    const auto fullData = wav.getAudioData();
    const int framesToLoad = static_cast<int>(std::min<qsizetype>(fullData.size(), static_cast<qsizetype>(maxFrames)));

    segment.sampleRate = wav.sampleRate();
    segment.layout = ArtifactCore::AudioChannelLayout::Mono;
    segment.channelData.resize(1);
    segment.channelData[0] = fullData.mid(0, framesToLoad);
    segment.startFrame = 0;

    return segment;
}

QIcon ArtifactAssetBrowser::Impl::generateAudioWaveformThumbnail(const QString& audioFilePath)
{
    // Check if we already have a pending job for this file
    const std::uint64_t currentGeneration = thumbnailGeneration_.load(std::memory_order_relaxed);
    if (auto it = pendingWaveJobs_.find(audioFilePath); it != pendingWaveJobs_.end()) {
        if (it.value().generation == currentGeneration) {
            return defaultAudioIcon_;  // Return placeholder while generating
        }
        pendingWaveJobs_.erase(it);
    }

    // Check if this file previously failed
    if (failedWavePaths_.contains(audioFilePath)) {
        return defaultAudioIcon_;
    }

    // Check cache first
    if (thumbnailCache_.contains(audioFilePath)) {
        return thumbnailCache_[audioFilePath];
    }

    // Start async generation
    startAsyncWaveformGeneration(audioFilePath);

    // Return placeholder while generating
    return defaultAudioIcon_;
}

void ArtifactAssetBrowser::Impl::startAsyncWaveformGeneration(const QString& audioFilePath)
{
    if (pendingWaveJobs_.contains(audioFilePath)) {
        const auto existing = pendingWaveJobs_.value(audioFilePath);
        if (existing.generation == thumbnailGeneration_.load(std::memory_order_relaxed)) {
            return;  // Already pending
        }
        pendingWaveJobs_.remove(audioFilePath);
    }

    const quint64 jobGeneration = thumbnailGeneration_.load(std::memory_order_relaxed);
    auto* watcher = new QFutureWatcher<QImage>();

    // Connect finished signal
    QObject::connect(watcher, &QFutureWatcher<QImage>::finished, [this, watcher, audioFilePath, jobGeneration]() {
        const QImage image = watcher->result();
        pendingWaveJobs_.remove(audioFilePath);
        if (jobGeneration != thumbnailGeneration_.load(std::memory_order_relaxed)) {
            watcher->deleteLater();
            return;
        }
        if (!image.isNull()) {
            // QPixmap/QIcon are GUI resources.  Keep their construction on the
            // watcher's thread (the browser/UI thread), never in QtConcurrent.
            const QIcon icon(QPixmap::fromImage(image));
            thumbnailCache_[audioFilePath] = icon;
            if (assetModel_ && assetModel_->updateItemIconByPath(audioFilePath, icon)) {
                // model updated via dataChanged
            } else if (fileView_) {
                fileView_->update();
            }

        } else {
            // Mark as failed to avoid retry
            failedWavePaths_.insert(audioFilePath);
        }
        watcher->deleteLater();
    });

    // Capture thumbnail size for the worker
    const QSize thumbSize = thumbnailSize_;

    // Run waveform generation in background thread
    QFuture<QImage> future = QtConcurrent::run([audioFilePath, thumbSize]() -> QImage {
        try {
            // Load audio (in background thread)
            ArtifactCore::SimpleWav wav;
            if (!wav.loadFromFile(audioFilePath)) {
                return QImage();
            }

            // Downsample for thumbnail (max 30 seconds)
            const int maxFrames = 30 * wav.sampleRate();
            const auto fullData = wav.getAudioData();
    const qsizetype framesToLoad = qMin(fullData.size(), static_cast<qsizetype>(maxFrames));

            ArtifactCore::AudioSegment segment;
            segment.sampleRate = wav.sampleRate();
            segment.layout = ArtifactCore::AudioChannelLayout::Mono;
            segment.channelData.resize(1);
            segment.channelData[0] = fullData.mid(0, framesToLoad);
            segment.startFrame = 0;

            if (segment.channelData[0].isEmpty()) {
                return QImage();
            }

            // Generate waveform data
            AudioWaveformGenerator generator;
            const int thumbWidth = thumbSize.width() * 2;
            WaveformData waveData = generator.generate(segment, thumbWidth);

            if (waveData.peaks.isEmpty()) {
                return QImage();
            }

            // QImage is reentrant and can be painted in a worker.  Conversion
            // to GUI-backed QPixmap/QIcon happens in the finished callback.
            QImage image(thumbSize, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);

            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing);

            const int w = image.width();
            const int h = image.height();
            const int centerY = h / 2;
            const float padding = 2.0f;

            painter.fillRect(0, 0, w, h, QColor(30, 30, 40, 200));
            painter.setPen(QColor(80, 80, 100));
            painter.drawLine(QLineF(padding, centerY, w - padding, centerY));

            const float maxAmplitude = std::max(std::abs(waveData.minSample), std::abs(waveData.maxSample));
            const float scale = (centerY - padding) / (maxAmplitude > 0.001f ? maxAmplitude : 1.0f);

            if (!waveData.rms.isEmpty()) {
                painter.setPen(QPen(QColor(60, 100, 160, 120), 1.0f));
                for (int i = 0; i < waveData.rms.size() && i < w; ++i) {
                    const float rmsVal = waveData.rms[i] * scale;
                    painter.drawLine(QLineF(i, centerY - rmsVal, i, centerY + rmsVal));
                }
            }

            painter.setPen(QPen(QColor(100, 160, 230, 200), 1.2f));
            for (int i = 0; i < waveData.peaks.size() && i < w; ++i) {
                const float peakVal = waveData.peaks[i] * scale;
                painter.drawLine(QLineF(i, centerY - peakVal, i, centerY + peakVal));
            }

            painter.end();
            return image;
        } catch (...) {
            return QImage();
        }
    });

    watcher->setFuture(future);
    pendingWaveJobs_[audioFilePath] = {audioFilePath, watcher, jobGeneration};
}

// ─────────────────────────────────────────────
// File Operations: Delete / Rename / New Folder / FileSystemWatcher
// ─────────────────────────────────────────────

void ArtifactAssetBrowser::Impl::setupFileSystemWatcher()
{
  if (fsWatcher_) return;
  fsWatcher_ = new QFileSystemWatcher();
  QObject::connect(fsWatcher_, &QFileSystemWatcher::directoryChanged,
                   QCoreApplication::instance(), [this](const QString& path) {
    Q_UNUSED(path);
    if (!watchScheduled_) {
      watchScheduled_ = true;
      QTimer::singleShot(500, [this]() {
        watchScheduled_ = false;
        clearThumbnailCache();
        applyFilters();
      });
    }
  });
  QObject::connect(fsWatcher_, &QFileSystemWatcher::fileChanged,
                   QCoreApplication::instance(), [this](const QString& path) {
    Q_UNUSED(path);
    if (!watchScheduled_) {
      watchScheduled_ = true;
      QTimer::singleShot(500, [this]() {
        watchScheduled_ = false;
        clearThumbnailCache();
        applyFilters();
      });
    }
  });
}

void ArtifactAssetBrowser::Impl::watchCurrentDirectory()
{
  if (!fsWatcher_) return;
  if (!currentDirectoryPath_.isEmpty() && QDir(currentDirectoryPath_).exists()) {
    if (!fsWatcher_->directories().contains(currentDirectoryPath_)) {
      fsWatcher_->addPath(currentDirectoryPath_);
    }
  }
}

void ArtifactAssetBrowser::Impl::handleFileRenamed(const QString& oldPath, const QString& newPath)
{
  Q_UNUSED(oldPath);
  Q_UNUSED(newPath);
  clearThumbnailCache();
  applyFilters();
}

void ArtifactAssetBrowser::Impl::handleFileDeleted(const QString& path)
{
  Q_UNUSED(path);
  clearThumbnailCache();
  applyFilters();
}

void ArtifactAssetBrowser::Impl::createNewFolder()
{
  QString folderName = promptNewFolderName();
  if (folderName.isEmpty()) return;

  QString parentDir = currentDirectoryPath_;
  if (parentDir.isEmpty()) {
    parentDir = ArtifactProjectManager::getInstance().currentProjectAssetsPath();
  }
  if (parentDir.isEmpty()) {
    parentDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Assets";
  }
  if (parentDir.isEmpty()) return;

  QString newPath = QDir(parentDir).filePath(folderName);
  if (QDir().mkpath(newPath)) {
    clearThumbnailCache();
    applyFilters();
  }
}

void ArtifactAssetBrowser::Impl::renameSelected()
{
  QStringList paths = selectedAssetPaths();
  if (paths.isEmpty()) return;

  QString oldPath = paths.first();
  QFileInfo fi(oldPath);
  QString newName = promptNewName(fi.fileName());
  if (newName.isEmpty() || newName == fi.fileName()) return;

  QString newPath = fi.absolutePath() + "/" + newName;
  if (QFile::rename(oldPath, newPath)) {
    clearThumbnailCache();
    applyFilters();
  }
}

void ArtifactAssetBrowser::Impl::deleteSelected()
{
  QStringList paths = selectedAssetPaths();
  if (paths.isEmpty()) return;

  if (!confirmDelete(paths)) return;

  int deletedCount = 0;
  for (const QString& path : paths) {
    QFileInfo fi(path);
    if (fi.isDir()) {
      if (QDir(path).removeRecursively()) {
        ++deletedCount;
      }
    } else {
      if (QFile::remove(path)) {
        ++deletedCount;
      }
    }
  }

  if (deletedCount > 0) {
    auto* svc = ArtifactProjectService::instance();
    if (svc) {
      svc->removeAllAssets();
      for (const QString& path : paths) {
        if (QFileInfo::exists(path)) {
          svc->importAssetsFromPaths(QStringList() << path);
        }
      }
    }
    clearThumbnailCache();
    applyFilters();
  }
}

QString ArtifactAssetBrowser::Impl::promptNewName(const QString& currentName) const
{
  bool ok = false;
  QString newName = QInputDialog::getText(nullptr, "Rename", "New name:",
                                           QLineEdit::Normal, currentName, &ok);
  if (!ok || newName.isEmpty()) return "";
  return newName;
}

QString ArtifactAssetBrowser::Impl::promptNewFolderName() const
{
  bool ok = false;
  QString name = QInputDialog::getText(nullptr, "New Folder", "Folder name:",
                                        QLineEdit::Normal, "New Folder", &ok);
  if (!ok || name.isEmpty()) return "";
  return name;
}

bool ArtifactAssetBrowser::Impl::confirmDelete(const QStringList& paths) const
{
  QString message;
  if (paths.size() == 1) {
    QFileInfo fi(paths.first());
    message = QStringLiteral("Are you sure you want to delete '%1'?<br>This will permanently remove the file from disk.").arg(fi.fileName());
  } else {
    message = QStringLiteral("Are you sure you want to delete %1 items?<br>This will permanently remove the files from disk.").arg(paths.size());
  }

  QMessageBox msgBox;
  msgBox.setWindowTitle("Confirm Delete");
  msgBox.setText(message);
  msgBox.setIcon(QMessageBox::Warning);
  msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
  msgBox.setDefaultButton(QMessageBox::No);
  return msgBox.exec() == QMessageBox::Yes;
}

} // namespace Artifact
