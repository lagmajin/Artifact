module;


export module Artifact.Layer.Audio;

import std;

export namespace Artifact
{
 class ArtifactAudioLayer
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactAudioLayer();
  ~ArtifactAudioLayer();
  void setVolume();
  void mute();
 };


};

