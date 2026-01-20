module;
#include <wobjectimpl.h>
#include <QObject>
#include <QString>

export module Artifact.Preview.Clock;

import std;
import Frame.Rate;
import Frame.Range;
import Frame.Position;

export namespace Artifact
{
 using namespace ArtifactCore;
 
 // UI層のタイムラインクロック通知クラス
 // 
 // 設計方針:
 // - 高精度タイミングは ArtifactCore::TimelineClock で処理（マイクロ秒精度）
 // - UIへの通知のみ Signal/Slot を使用（60fps = 16ms間隔）
 // - Composition/Layerは clock() で直接TimelineClockを参照して高精度タイミングを取得
 // 
 // Signal/Slotを使わない理由:
 // - Qt のイベントキューイング遅延（数ミリ秒）により高精度が失われる
 // - スレッド間通信ではさらに遅延が発生
 // - UI更新は60fps（16ms）で十分
 class ArtifactTimelineClock : public QObject
 {
  W_OBJECT(ArtifactTimelineClock)
 private:
  class Impl;
  Impl* impl_;
  
 public:
  ArtifactTimelineClock(QObject* parent = nullptr);
  ~ArtifactTimelineClock();
  
  // 高精度クロックへの直接アクセス（Composition/Layerで使用）
  // 注意: このメソッドはスレッドセーフです
  class TimelineClock* clock();
  const class TimelineClock* clock() const;
  
 public /*signals*/:
  // UI更新用のシグナル（60fps程度で発行される）
  // 注意: これらは高精度タイミングには使用しないこと
  void tick() W_SIGNAL(tick);
  void tickFrame(const FramePosition& position) W_SIGNAL(tickFrame, position);
  void playbackStateChanged(bool isPlaying) W_SIGNAL(playbackStateChanged, isPlaying);
  void timecodeChanged(const QString& timecode) W_SIGNAL(timecodeChanged, timecode);
  
 public /*slots*/:
  void start(); W_SLOT(start);
  void stop(); W_SLOT(stop);
  void pause(); W_SLOT(pause);
  void resume(); W_SLOT(resume);
  void startClockRange(int startFrame, int endFrame); W_SLOT(startClockRange);
  void setPlaybackSpeed(double speed); W_SLOT(setPlaybackSpeed);
 };

};

W_REGISTER_ARGTYPE(ArtifactCore::FramePosition)
