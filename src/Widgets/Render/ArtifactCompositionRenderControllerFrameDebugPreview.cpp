module;

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

#include <QImage>
#include <QRgb>
#include <QString>
#include <QtGlobal>

module Artifact.Widgets.CompositionRenderController;

import Frame.Debug;

namespace Artifact::CompositionFrameDebugPreview {

static QImage ensurePreviewImage(const QImage& image) {

  if (image.isNull()) {

    return {};

  }

  return image.format() == QImage::Format_RGBA8888

             ? image

             : image.convertToFormat(QImage::Format_RGBA8888);

}



QString previewPixelStats(const QImage& source) {

  const QImage image = ensurePreviewImage(source);

  if (image.isNull() || image.width() <= 0 || image.height() <= 0) {

    return QStringLiteral("state=unavailable");

  }



  constexpr qint64 kMaxSamples = 65536;

  const qint64 pixelCount =

      static_cast<qint64>(image.width()) * static_cast<qint64>(image.height());

  const int step =

      std::max(1, static_cast<int>(std::ceil(

                      std::sqrt(static_cast<double>(pixelCount) /

                                static_cast<double>(kMaxSamples)))));

  std::array<int, 4> minimum = {255, 255, 255, 255};

  std::array<int, 4> maximum = {0, 0, 0, 0};

  qint64 samples = 0;

  qint64 rgbZero = 0;

  qint64 rgbFull = 0;

  qint64 blueDominant = 0;



  for (int y = 0; y < image.height(); y += step) {

    const auto* row = image.constScanLine(y);

    for (int x = 0; x < image.width(); x += step) {

      const auto* pixel = row + x * 4;

      for (int channel = 0; channel < 4; ++channel) {

        const int value = pixel[channel];

        minimum[channel] = std::min(minimum[channel], value);

        maximum[channel] = std::max(maximum[channel], value);

      }

      rgbZero += (pixel[0] == 0) + (pixel[1] == 0) + (pixel[2] == 0);

      rgbFull += (pixel[0] == 255) + (pixel[1] == 255) + (pixel[2] == 255);

      if (pixel[2] >= 240 && pixel[2] >= pixel[0] + 32 &&

          pixel[2] >= pixel[1] + 32) {

        ++blueDominant;

      }

      ++samples;

    }

  }



  const double rgbDenominator =

      std::max(1.0, static_cast<double>(samples) * 3.0);

  const double sampleDenominator =

      std::max(1.0, static_cast<double>(samples));

  return QStringLiteral(

             "state=sampled domain=post-readback-sdr samples=%1 step=%2 "

             "min=%3,%4,%5,%6 max=%7,%8,%9,%10 "

             "rgbZeroPct=%11 rgbFullPct=%12 blueDominantPct=%13")

      .arg(samples)

      .arg(step)

      .arg(minimum[0])

      .arg(minimum[1])

      .arg(minimum[2])

      .arg(minimum[3])

      .arg(maximum[0])

      .arg(maximum[1])

      .arg(maximum[2])

      .arg(maximum[3])

      .arg(QString::number(100.0 * static_cast<double>(rgbZero) /

                               rgbDenominator,

                           'f', 2))

      .arg(QString::number(100.0 * static_cast<double>(rgbFull) /

                               rgbDenominator,

                           'f', 2))

      .arg(QString::number(100.0 * static_cast<double>(blueDominant) /

                               sampleDenominator,

                           'f', 2));

}



static QImage makePreviewDiffImage(const QImage& before, const QImage& after) {

  if (before.isNull() || after.isNull() || before.size() != after.size()) {

    return {};

  }



  const QImage lhs = ensurePreviewImage(before);

  const QImage rhs = ensurePreviewImage(after);

  if (lhs.isNull() || rhs.isNull() || lhs.size() != rhs.size()) {

    return {};

  }



  QImage diff(lhs.size(), QImage::Format_RGBA8888);

  for (int y = 0; y < lhs.height(); ++y) {

    const QRgb* lhsRow = reinterpret_cast<const QRgb*>(lhs.constScanLine(y));

    const QRgb* rhsRow = reinterpret_cast<const QRgb*>(rhs.constScanLine(y));

    QRgb* outRow = reinterpret_cast<QRgb*>(diff.scanLine(y));

    for (int x = 0; x < lhs.width(); ++x) {

      outRow[x] = qRgba(std::abs(qRed(lhsRow[x]) - qRed(rhsRow[x])),

                        std::abs(qGreen(lhsRow[x]) - qGreen(rhsRow[x])),

                        std::abs(qBlue(lhsRow[x]) - qBlue(rhsRow[x])),

                        255);

    }

  }

  return diff;

}



void addSnapshotPreview(ArtifactCore::FrameDebugSnapshot& snapshot,

                        const QString& key, const QString& label,

                        const QString& note, const QImage& beforeImage,

                        const QImage& afterImage, const QImage& alphaImage) {

  ArtifactCore::FrameDebugImagePreviewRecord preview;

  preview.key = key;

  preview.label = label;

  preview.note = note;

  preview.beforeImage = ensurePreviewImage(beforeImage);

  preview.afterImage = ensurePreviewImage(afterImage);

  preview.alphaImage = alphaImage;

  preview.diffImage =

      makePreviewDiffImage(preview.beforeImage, preview.afterImage);

  snapshot.previews.push_back(std::move(preview));

}

} // namespace Artifact::CompositionFrameDebugPreview
