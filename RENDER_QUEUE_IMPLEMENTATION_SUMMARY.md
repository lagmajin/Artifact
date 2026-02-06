# レンダリングキュー管理強化の実装概要

## 実装した機能

### 1. レンダリングキューサービスの強化 (`src/Render/ArtifactRenderQueueService.cppm`)

#### データモデル
- `ArtifactRenderJob` クラス：レンダリングジョブの状態管理
  - ステータス (待機中/レンダリング中/完了/失敗/キャンセル)
  - コンポジション名、出力パス、フォーマット、コーデック
  - 解像度、フレームレート、ビットレート、フレーム範囲
  - 進捗率、エラーメッセージ

- `ArtifactRenderQueueManager` クラス：キュー管理
  - ジョブの追加、削除、移動
  - ステータス管理 (開始/一時停止/キャンセル)
  - 進捗率の計算
  - コールバック機能

#### サービスメソッド
- `addRenderQueue()`：現在のコンポジションをキューに追加
- `removeRenderQueue()`：選択されたジョブを削除
- `removeAllRenderQueues()`：すべてのジョブを削除
- `startAllJobs()`：すべての待機中のジョブを開始
- `pauseAllJobs()`：すべてのレンダリング中のジョブを一時停止
- `cancelAllJobs()`：すべてのレンダリング中のジョブをキャンセル
- `jobCount()`：ジョブ数の取得
- `getJob(int index)`：指定インデックスのジョブを取得
- `getTotalProgress()`：全体の進捗率の取得

#### コールバック機能
- `setJobAddedCallback()`：ジョブ追加時のコールバック
- `setJobRemovedCallback()`：ジョブ削除時のコールバック
- `setJobUpdatedCallback()`：ジョブ更新時のコールバック
- `setJobStatusChangedCallback()`：ジョブステータス変更時のコールバック
- `setJobProgressChangedCallback()`：ジョブ進捗変更時のコールバック
- `setAllJobsCompletedCallback()`：すべてのジョブ完了時のコールバック
- `setAllJobsRemovedCallback()`：すべてのジョブ削除時のコールバック

### 2. レンダリングキューマネージャーウィジェット (`src/Widgets/Render/ArtifactRenderQueueManagerWidget.cpp`)

#### UIコンポーネント
- ジョブリスト：QListWidgetでジョブの一覧表示
- コントロールボタン：
  - Add：キューにジョブを追加
  - Remove：選択されたジョブを削除
  - Clear All：すべてのジョブを削除
  - Start：すべてのジョブを開始
  - Pause：すべてのジョブを一時停止
  - Cancel：すべてのジョブをキャンセル

- 進捗表示：QProgressBarで全体の進捗率を表示
- ステータスラベル：現在のステータスを表示

#### 機能
- ジョブリストの更新
- ジョブの選択管理
- レンダリングキューサービスとの連携
- イベントハンドリング

### 3. レンダリングキュージョブウィジェット (`src/Widgets/Render/ArtifactRenderQueueJobPanel.cpp`)

#### UIコンポーネント
- トグルボタン：ジョブの詳細を表示/非表示
- コンポジション名ラベル：EditableLabelでコンポジション名を表示
- 開始ボタン：ジョブのレンダリングを開始
- ステータスラベル：ジョブのステータスを表示
- 進捗バー：QProgressBarでジョブの進捗率を表示

#### 機能
- ジョブステータスの更新
- ジョブ進捗の更新

### 4. コンポジションメニューの強化 (`src/Widgets/Menu/ArtifactCompositionMenu.cppm`)

#### メニュー機能
- "Add to Render Queue" アクション
  - ショートカット：Ctrl + M
  - 現在のコンポジションをレンダリングキューに追加
  - コンポジションがない場合は無効化

#### コネクション
- メニューアクションとレンダリングキューサービスの連携
- メニューのrebuildメソッドの更新

### 5. テストモジュール (`src/Test/ArtifactTestRenderQueue.cppm`)

#### テスト機能
- `testAddRenderQueue()`：ジョブ追加のテスト
- `testJobCount()`：ジョブ数のテスト
- `testGetJob()`：ジョブ取得のテスト
- `testGetTotalProgress()`：全体進捗率のテスト
- `testStartAllJobs()`：ジョブ開始のテスト
- `testJobStatusChanged()`：ステータス変更のテスト
- `testPauseAllJobs()`：ジョブ一時停止のテスト
- `testCancelAllJobs()`：ジョブキャンセルのテスト
- `testRemoveAllRenderQueues()`：すべてのジョブ削除のテスト

#### 実行メソッド
- `runAllTests()`：すべてのテストを実行

## 使用方法

### 1. レンダリングキューの開き方

1. アプリケーションのメニューバーから "Window" メニューを選択
2. "Render Queue" をクリック
3. レンダリングキューパネルが開きます

### 2. ジョブの追加方法

1. コンポジションを選択している状態で
2. メニューバーから "Composition" -> "Add to Render Queue" を選択
3. またはショートカット Ctrl + M を押す
4. レンダリングキューパネルにジョブが追加されます

### 3. ジョブの管理

- **開始**："Start" ボタンをクリックしてすべての待機中のジョブを開始
- **一時停止**："Pause" ボタンをクリックしてすべてのレンダリング中のジョブを一時停止
- **キャンセル**："Cancel" ボタンをクリックしてすべてのレンダリング中のジョブをキャンセル
- **削除**：ジョブを選択して "Remove" ボタンをクリック
- **クリア**："Clear All" ボタンをクリックしてすべてのジョブを削除

### 4. テストの実行

```cpp
#include "ArtifactTestRenderQueue.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    ArtifactTestRenderQueue test;
    test.runAllTests();

    return 0;
}
```

## 今後の改良点

### 1. レンダリングエンジンの連携

- 実際のレンダリング処理との連携
- バックグラウンドレンダリングの実装
- レンダリング進捗の更新

### 2. UIの改善

- ジョブの詳細情報の表示
- 出力パスの編集機能
- レンダリング設定の編集
- ジョブの並べ替え機能

### 3. エラー処理の強化

- エラーメッセージの表示
- レンダリング失敗時の処理
- ログの保存機能

### 4. パフォーマンスの最適化

- ジョブの並列実行
- メモリ管理の最適化
- レンダリングキャッシュの実装

### 5. 機能の拡張

- プリセットの保存と読み込み
- バッチレンダリング
- ネットワークレンダリング

## 技術的な特徴

### 1. モジュール化

- C++20のモジュールシステムを使用
- レンダリングキュー機能は単一のモジュールにまとまっている

### 2. シングルトンパターン

- ArtifactRenderQueueServiceはシングルトンパターンで実装
- アプリケーション全体で共有可能な状態管理

### 3. シグナルとスロット

- Qtの信号とスロット機構を使用
- UIとサービスの通信を簡潔に実現

### 4. コールバック機構

- 各種イベントに対するコールバック機能
- 柔軟な拡張が可能

### 5. メモリ管理

- インプリメンテーションポインタパターンを使用
- ヘッダーファイルの依存関係を最小化

## まとめ

レンダリングキュー管理の強化により、複数のコンポジションを効率的にレンダリングすることが可能になりました。ユーザーはレンダリングキューにジョブを追加し、一括でレンダリングを管理することで、作業効率を大幅に向上させることができます。

今後の改良では、レンダリングエンジンとの連携、UIの改善、エラー処理の強化、パフォーマンスの最適化を行い、より完成度の高いレンダリング管理システムを構築していきます。