module;


export module Artifact.Layer.Audio;


import std;
import Audio.Volume;
import Artifact.Layer.Abstract;

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
  //bool isMuted() const;
  void mute();
 };


};

