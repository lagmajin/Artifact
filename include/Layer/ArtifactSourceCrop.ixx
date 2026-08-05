module;
#include <algorithm>
#include <cmath>

#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QTransform>
#include <QString>

export module Artifact.Layer.SourceCrop;

import Serialization.ISerializable;

export namespace Artifact {

class SourceCrop : public ArtifactCore::Serialization::ISerializable {
public:
  SourceCrop() = default;

  bool enabled() const;
  void setEnabled(bool enabled);

  QRectF cropRect() const;
  void setCropRect(const QRectF &rect);

  QPointF pan() const;
  void setPan(const QPointF &pan);

  double zoom() const;
  void setZoom(double zoom);

  double rotation() const;
  void setRotation(double rotation);

  QPointF anchor() const;
  void setAnchor(const QPointF &anchor);

  bool preserveAspect() const;
  void setPreserveAspect(bool preserveAspect);

  void reset();
  void clampToSource(const QSizeF &sourceSize);

  QRectF effectiveCropRect(const QSizeF &sourceSize) const;
  QTransform sourceToOutputTransform(const QSizeF &sourceSize,
                                     const QSizeF &outputSize) const;

  QJsonObject toJson() const;
  void fromJson(const QJsonObject &obj);
  QJsonObject serialize() const override { return toJson(); }
  bool deserialize(const QJsonObject &obj) override
  {
    fromJson(obj);
    return true;
  }
  QString typeName() const override { return QStringLiteral("ArtifactSourceCrop"); }
  int schemaVersion() const override { return 1; }

private:
  bool enabled_ = false;
  QRectF cropRect_;
  QPointF pan_{0.0, 0.0};
  double zoom_ = 1.0;
  double rotation_ = 0.0;
  QPointF anchor_{0.5, 0.5};
  bool preserveAspect_ = true;
};

} // namespace Artifact
