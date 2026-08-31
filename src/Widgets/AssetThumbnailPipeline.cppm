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

#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QIcon>
#include <QPixmap>
#include <QStandardPaths>
#include <QStringList>
#include <QSize>
#include <QString>

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imageio.h>

#include <algorithm>
#include <cmath>
#include <exception>
#include <string>
#include <vector>

module Artifact.Widgets.AssetThumbnailPipeline;

namespace Artifact::AssetThumbnail {
namespace {
QString assetThumbnailDiskPath(const QFileInfo& fileInfo) {
  if (!fileInfo.exists() || !fileInfo.isFile()) return {};
  const QString cacheRoot = QStandardPaths::writableLocation(
      QStandardPaths::CacheLocation) +
      QStringLiteral("/ArtifactStudio/AssetThumbnails");
  QDir().mkpath(cacheRoot);
  const QByteArray identity =
      (fileInfo.absoluteFilePath() + QStringLiteral("|") +
       QString::number(fileInfo.size()) + QStringLiteral("|") +
       QString::number(fileInfo.lastModified().toMSecsSinceEpoch()))
          .toUtf8();
  const QByteArray digest = QCryptographicHash::hash(
      identity, QCryptographicHash::Sha1).toHex();
  return cacheRoot + QLatin1Char('/') + QString::fromLatin1(digest) +
         QStringLiteral(".png");
}

} // namespace

QIcon loadFromDisk(const QFileInfo& fileInfo) {
  const QString path = assetThumbnailDiskPath(fileInfo);
  if (path.isEmpty()) return {};
  constexpr qint64 kThumbnailCacheMaxAgeDays = 30;
  const QFileInfo cachedFile(path);
  if (cachedFile.exists() &&
      cachedFile.lastModified().daysTo(QDateTime::currentDateTime()) >
          kThumbnailCacheMaxAgeDays) {
    QFile::remove(path);
    return {};
  }
  QPixmap pixmap;
  if (!pixmap.load(path, "PNG")) return {};
  return QIcon(pixmap);
}

void saveToDisk(const QFileInfo& fileInfo, const QImage& image) {
  const QString path = assetThumbnailDiskPath(fileInfo);
  if (path.isEmpty() || image.isNull() || !image.save(path, "PNG")) return;

  constexpr qint64 kMaxThumbnailCacheBytes = 256LL * 1024LL * 1024LL;
  const QFileInfo cacheFile(path);
  const QDir cacheDir(cacheFile.absolutePath());
  QFileInfoList entries = cacheDir.entryInfoList(
      QStringList() << QStringLiteral("*.png"),
      QDir::Files | QDir::Readable,
      QDir::Time | QDir::Reversed);
  qint64 totalBytes = 0;
  for (const QFileInfo& entry : entries) totalBytes += entry.size();
  for (const QFileInfo& entry : entries) {
    if (totalBytes <= kMaxThumbnailCacheBytes) break;
    if (entry.absoluteFilePath() == cacheFile.absoluteFilePath()) continue;
    if (QFile::remove(entry.absoluteFilePath())) totalBytes -= entry.size();
  }
}

#ifdef _WIN32
using Microsoft::WRL::ComPtr;

QImage loadImageViaWIC(const QString& filePath,
                       const QSize& targetSize,
                       QString* errorOut)
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

QImage loadImageViaWindowsShell(const QString& filePath,
                                const QSize& targetSize,
                                QString* errorOut)
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

#ifndef _WIN32
QImage loadImageViaWIC(const QString&, const QSize&, QString* errorOut) {
  if (errorOut) *errorOut = QStringLiteral("WIC thumbnails are unavailable on this platform.");
  return {};
}

QImage loadImageViaWindowsShell(const QString&, const QSize&, QString* errorOut) {
  if (errorOut) *errorOut = QStringLiteral("Windows Shell thumbnails are unavailable on this platform.");
  return {};
}
#endif

QImage loadImageViaOIIO(const QString& filePath, const QSize& targetSize,
                        QString* errorOut)
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

} // namespace Artifact::AssetThumbnail
