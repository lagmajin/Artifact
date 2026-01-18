module;


export module Artifact.Layer.Settings;

import Utils.String.UniString;
import Utils.Id;

namespace Artifact {

 using namespace ArtifactCore;

 class ArtifactLayerSetting {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactLayerSetting();
  ArtifactLayerSetting(const ArtifactLayerSetting& other);
  ArtifactLayerSetting(ArtifactLayerSetting&& other) noexcept;
  ~ArtifactLayerSetting();
  ArtifactLayerSetting& operator=(const ArtifactLayerSetting& other);
  ArtifactLayerSetting& operator=(ArtifactLayerSetting&& other) noexcept;

  // ID
  LayerID id() const;
  void setId(const LayerID &id);

  // Basic flags
  bool visible() const;
  void setVisible(bool v);

  bool locked() const;
  void setLocked(bool l);

  bool solo() const;
  void setSolo(bool s);
 };

};