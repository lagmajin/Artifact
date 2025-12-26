module;
#include <QtGlobal>
#include <QVector>
export module Audio.Segment;

import std;

export namespace ArtifactCore {
 enum class AudioChannelLayout {
  Mono,       // 1ch
  Stereo,     // 2ch
  Surround51, // 6ch (L, R, C, LFE, Ls, Rs)
  Surround71, // 8ch
  Custom10ch, // 10ch
  Ambisonics  // 球面調和関数（VRなどで利用）
 };

 enum class ChannelType {
  Left, Right, Center, LFE, LeftSurround, RightSurround,
  LeftBack, RightBack, TopFrontLeft, TopFrontRight, // 10ch用など
  Unknown
 };

 struct AudioSegment {
  std::vector<float> samples; // マルチチャネルならインターリーブ（LRLR...）
  int sampleRate = 44100;
  int channels = 2;
  qint64 startFrame = 0;      // タイムライン上の開始位置
 };



};