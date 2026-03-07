module;
#include <QtGlobal>
#include <QList>
#include <QVector>
export module Audio.Segment;

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>




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
  // 各チャンネルのサンプルデータを格納する二次元構造
  // channels[0] = Left, channels[1] = Right ...
  // QVectorの暗黙的共有により、この構造体自体のコピーコストは非常に低い
  QVector<QVector<float>> channelData;

  int sampleRate = 44100;
  AudioChannelLayout layout = AudioChannelLayout::Stereo;
  qint64 startFrame = 0;

  // 便利関数：全チャンネルのサンプル数を取得
  int frameCount() const {
   return channelData.isEmpty() ? 0 : channelData[0].size();
  }

  // チャンネル数を取得
  int channelCount() const {
   return channelData.size();
  }

  // 特定のサンプルのポインタを安全に取得
  const float* constData(int channelIdx) const {
   if (channelIdx >= 0 && channelIdx < channelData.size()) {
	return channelData[channelIdx].constData();
   }
   return nullptr;
  }
 };



};