module;
#include <wobjectdefs.h>
#include <QObject>
#include <QWidget>
export module Artifact.Controller.TimelineViewProvider;

import Utils.Id;
import Artifact.Composition.Abstract;
import Artifact.Widgets.Timeline;

export namespace Artifact
{
 using namespace ArtifactCore;

 using Artifact::ArtifactTimelineWidget;
 /**
 * @class TimelineViewProvider
 * @brief コンポ実体とタイムラインUIの「1対1関係」を保証するコントローラー
 * * 役割:
 * 1. 1つのコンポ(QUuid)に対して、一意のTimelineWidgetをインスタンス化・管理する
 * 2. ウィジェットが破棄された際の参照クリーニング
 * 3. 外部(Project等)からのUI取得リクエストに対する窓口
 */

 class TimelineViewProvider : public QObject
 {
  W_OBJECT(TimelineViewProvider)
 private:
  class Impl;
  Impl* m_impl;
 public:
  explicit TimelineViewProvider(QObject* parent = nullptr);
  ~TimelineViewProvider();

  // Return existing widget for composition or create one if missing.
  ArtifactTimelineWidget *timelineWidgetForComposition(const CompositionID &id, QWidget *parent = nullptr);

 };



};