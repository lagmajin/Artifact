module;
#include <utility>


module Artifact.Layer.Null;
import std;
import Artifact.Layers.Abstract._2D;


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
   //throw std::logic_error("The method or operation is not implemented.");
  }

  std::vector<ArtifactCore::PropertyGroup> ArtifactNullLayer::getLayerPropertyGroups() const
  {
    return ArtifactAbstract2DLayer::getLayerPropertyGroups();
  }

  bool ArtifactNullLayer::setLayerPropertyValue(const QString &propertyPath, const QVariant &value)
  {
    return ArtifactAbstract2DLayer::setLayerPropertyValue(propertyPath, value);
  }

  QImage ArtifactNullLayer::toQImage() const
  {
    return QImage();
  }

  QJsonObject ArtifactNullLayer::toJson() const
  {
    return ArtifactAbstract2DLayer::toJson();
  }

  std::shared_ptr<ArtifactNullLayer> ArtifactNullLayer::fromJson(const QJsonObject& obj)
  {
    auto layer = std::make_shared<ArtifactNullLayer>();
    layer->fromJsonProperties(obj);
    return layer;
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
