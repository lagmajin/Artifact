module;
#include <utility>
#include <QImage>
#include <QSize>


module Artifact.Layer.Null;
import std;
import Artifact.Layer.Abstract;
import Memory.SharedPtr;


namespace Artifact {

 class ArtifactNullLayer::Impl {
 private:

 public:
  Impl();
  ~Impl();
 };

 ArtifactNullLayer::Impl::Impl()
 {

 }

 ArtifactNullLayer::Impl::~Impl()
 {

 }

  ArtifactNullLayer::ArtifactNullLayer():impl_(new Impl())
  {
   setSourceSize(Size_2D(100, 100));
  }

 ArtifactNullLayer::~ArtifactNullLayer()
 {
  delete impl_;
 }

  void ArtifactNullLayer::draw(ArtifactIRenderer* renderer)
  {
   // Null layers intentionally contribute no pixels; the transparent image
   // returned by toQImage() preserves that contract for preview/export paths.
  }

  std::vector<ArtifactCore::PropertyGroup> ArtifactNullLayer::getLayerPropertyGroups() const
  {
    return ArtifactAbstractLayer::getLayerPropertyGroups();
  }

  bool ArtifactNullLayer::setLayerPropertyValue(const QString &propertyPath, const QVariant &value)
  {
    return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
  }

  QImage ArtifactNullLayer::toQImage() const
  {
    const auto size = sourceSize();
    const QSize imageSize(std::max(1, size.width), std::max(1, size.height));
    QImage image(imageSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    return image;
  }

  QJsonObject ArtifactNullLayer::toJson() const
  {
    QJsonObject obj = ArtifactAbstractLayer::toJson();
    obj[QStringLiteral("type")] = static_cast<int>(LayerType::Null);
    return obj;
  }

  SharedPtr<ArtifactNullLayer> ArtifactNullLayer::fromJson(const QJsonObject& obj)
  {
    auto layer = ArtifactCore::makeShared<ArtifactNullLayer>();
    layer->fromJsonProperties(obj);
    return layer;
  }

  QImage ArtifactNullLayer::getThumbnail(int width, int height) const
  {
    const QSize targetSize(std::max(1, width), std::max(1, height));
    const QImage image = toQImage();
    return image.isNull()
        ? ArtifactAbstractLayer::getThumbnail(targetSize.width(), targetSize.height())
        : image.scaled(targetSize, Qt::KeepAspectRatio,
                       Qt::SmoothTransformation);
  }

  bool ArtifactNullLayer::isAdjustmentLayer() const
  {
   return false;
  }

  bool ArtifactNullLayer::isNullLayer() const
  {

   return true;
  }

};
