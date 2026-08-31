module;

#include <QStringList>

module Artifact.Layer.RuntimeSupport;

namespace Artifact {

std::optional<MaskPropertyAddress>
parseMaskPropertyPath(const QString &propertyPath) {
  const QStringList parts = propertyPath.split(QLatin1Char('.'), Qt::SkipEmptyParts);
  if (parts.size() < 3 || parts[0] != QStringLiteral("mask")) {
    return std::nullopt;
  }

  bool ok = false;
  const int maskIndex = parts[1].toInt(&ok);
  if (!ok || maskIndex < 0) {
    return std::nullopt;
  }

  if (parts.size() == 3 && parts[2] == QStringLiteral("enabled")) {
    return MaskPropertyAddress{maskIndex, -1, parts[2]};
  }

  if (parts.size() != 5 || parts[2] != QStringLiteral("path")) {
    return std::nullopt;
  }

  const int pathIndex = parts[3].toInt(&ok);
  if (!ok || pathIndex < 0) {
    return std::nullopt;
  }

  const QString field = parts[4];
  if (field == QStringLiteral("closed") ||
      field == QStringLiteral("opacity") ||
      field == QStringLiteral("feather") ||
      field == QStringLiteral("featherHorizontal") ||
      field == QStringLiteral("featherVertical") ||
      field == QStringLiteral("featherInner") ||
      field == QStringLiteral("featherOuter") ||
      field == QStringLiteral("expansion") ||
      field == QStringLiteral("inverted") ||
      field == QStringLiteral("mode") ||
      field == QStringLiteral("name")) {
    return MaskPropertyAddress{maskIndex, pathIndex, field};
  }

  return std::nullopt;
}

QString maskPropertyPrefix(const int maskIndex) {
  return QStringLiteral("mask.%1").arg(maskIndex);
}

QString maskPathPropertyPrefix(const int maskIndex, const int pathIndex) {
  return QStringLiteral("mask.%1.path.%2").arg(maskIndex).arg(pathIndex);
}

std::optional<ClonerTransformPropertyAddress>
parseClonerTransformPropertyPath(const QString &propertyPath) {
  const QString prefix = QStringLiteral("component.cloner.transforms.");
  if (!propertyPath.startsWith(prefix, Qt::CaseInsensitive)) {
    return std::nullopt;
  }
  const QString tail = propertyPath.mid(prefix.size());
  const QStringList parts = tail.split(QLatin1Char('.'), Qt::SkipEmptyParts);
  if (parts.size() != 2) {
    return std::nullopt;
  }
  bool ok = false;
  const int index = parts[0].toInt(&ok);
  if (!ok || index < 0) {
    return std::nullopt;
  }
  return ClonerTransformPropertyAddress{index, parts[1]};
}

void MotionTrailRingBuffer::clear() {
  head = 0;
  count = 0;
}

void MotionTrailRingBuffer::push(const QVector3D &sample,
                                 const std::size_t capacity) {
  if (capacity == 0) {
    clear();
    return;
  }
  if (samples.size() != capacity) {
    samples.assign(capacity, sample);
    head = 0;
    count = 1;
    return;
  }
  const std::size_t writeIndex = (head + count) % capacity;
  samples[writeIndex] = sample;
  if (count < capacity) {
    ++count;
  } else {
    head = (head + 1) % capacity;
  }
}

}
