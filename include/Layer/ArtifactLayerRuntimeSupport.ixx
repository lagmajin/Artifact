module;

#include <QString>
#include <QVector3D>

#include <cstddef>
#include <optional>

export module Artifact.Layer.RuntimeSupport;

import Container.NamedVector;

export namespace Artifact {

struct ClonerTransformOperation {
  QString name = QStringLiteral("Transform");
  bool enabled = true;
  QVector3D position{0.0f, 0.0f, 0.0f};
  QVector3D rotation{0.0f, 0.0f, 0.0f};
  QVector3D scale{1.0f, 1.0f, 1.0f};
};

struct MaskPropertyAddress {
  int maskIndex = -1;
  int pathIndex = -1;
  QString field;
};

std::optional<MaskPropertyAddress>
parseMaskPropertyPath(const QString &propertyPath);

QString maskPropertyPrefix(int maskIndex);
QString maskPathPropertyPrefix(int maskIndex, int pathIndex);

struct ClonerTransformPropertyAddress {
  int index = -1;
  QString field;
};

std::optional<ClonerTransformPropertyAddress>
parseClonerTransformPropertyPath(const QString &propertyPath);

struct MotionTrailRingBuffer {
  ArtifactCore::NamedVector<QVector3D> samples{
      ArtifactCore::ContainerName{"Layer.MotionTrailSamples"}};
  std::size_t head = 0;
  std::size_t count = 0;

  void clear();
  void push(const QVector3D &sample, std::size_t capacity);
};

}
