module;
export module Artifact.Layer.UIInfo;

import std;
import Frame.Range;
import Frame.Position;

export namespace Artifact
{
	
 struct ArtifactLayerUiInfo
 {
  bool visible = true;
  bool locked = false;
  bool solo = false;
  bool shy = false;
  

  bool hasAudio = false;
  bool hasEffects = false;
 };
	

};