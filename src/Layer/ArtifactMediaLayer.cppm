module;

module Artifact.Layer.Media;

import Artifact.Layer.Media;

namespace Artifact {

ArtifactMediaLayer::ArtifactMediaLayer() = default;

ArtifactMediaLayer::~ArtifactMediaLayer() = default;

QJsonObject ArtifactMediaLayer::toJson() const
{
 auto obj = ArtifactVideoLayer::toJson();
 obj["type"] = "MediaLayer";
 return obj;
}

std::shared_ptr<ArtifactMediaLayer> ArtifactMediaLayer::fromJson(const QJsonObject& obj)
{
 auto layer = std::make_shared<ArtifactMediaLayer>();

 if (obj.contains("sourcePath")) {
  layer->loadFromPath(obj["sourcePath"].toString());
 }
 if (obj.contains("inPoint")) {
  layer->setInPoint(obj["inPoint"].toInteger());
 }
 if (obj.contains("outPoint")) {
  layer->setOutPoint(obj["outPoint"].toInteger());
 }
 if (obj.contains("playbackSpeed")) {
  layer->setPlaybackSpeed(obj["playbackSpeed"].toDouble());
 }
 if (obj.contains("loopEnabled")) {
  layer->setLoopEnabled(obj["loopEnabled"].toBool());
 }
 if (obj.contains("audioVolume")) {
  layer->setAudioVolume(obj["audioVolume"].toDouble());
 }
 if (obj.contains("audioMuted")) {
  layer->setAudioMuted(obj["audioMuted"].toBool());
 }
 if (obj.contains("proxyQuality")) {
  layer->setProxyQuality(static_cast<ProxyQuality>(obj["proxyQuality"].toInt()));
 }
 if (obj.contains("audioEnabled")) {
  layer->setHasAudio(obj["audioEnabled"].toBool());
 }
 if (obj.contains("videoEnabled")) {
  layer->setHasVideo(obj["videoEnabled"].toBool());
 }
 if (obj.contains("layerName")) {
  layer->setLayerName(obj["layerName"].toString());
 }
 return layer;
}

}
