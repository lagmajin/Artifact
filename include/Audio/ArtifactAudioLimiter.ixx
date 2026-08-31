module;

#include <vector>

export module Artifact.Audio.Limiter;

import Audio.Segment;

export namespace Artifact {

class AudioLimiter {
public:
  AudioLimiter();

  void process(ArtifactCore::AudioSegment& segment, int sampleRate);
  void reset();

private:
  int sampleRate_ = 0;
  float threshold_ = 0.9f;
  float lookAheadMs_ = 3.0f;
  float attackMs_ = 1.0f;
  float releaseMs_ = 100.0f;
  float envelope_ = 1.0f;
  float attackCoeff_ = 1.0f;
  float releaseCoeff_ = 0.0f;
  int delaySize_ = 0;
  int delayPos_ = 0;
  std::vector<std::vector<float>> delayBuf_;
};

void softClipAudioSegment(ArtifactCore::AudioSegment& segment);

}
