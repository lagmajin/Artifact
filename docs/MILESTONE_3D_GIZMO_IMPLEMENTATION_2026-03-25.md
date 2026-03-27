# 3D Transform Gizmo Implementation Milestone (2026-03-25)

`ArtifactCompositionEditor` で、3Dレイヤーや3D空間内のオブジェクトを直感的に操作するための **3D Manipulator (Gizmo)** を導入するマイルストーン。
Blender, After Effects, Unity 等の標準的な操作体系に準拠し、X/Y/Z軸の移動・回転・拡大縮小をサポートする。

## Goal

- ビューポート内に 3D 空間の X(赤), Y(緑), Z(青) 軸の操作ハンドルを表示する
- Perspective/Orthographic カメラ越しに、正確なマウス当たり判定（Hit Testing）を行う
- 特定の軸や平面に拘束された（Constrained）トランスフォーム操作を実現する
- 既存の 2D Transform Gizmo と競合せず、レイヤーの性質（2D/3D）に応じて適切に切り替わる

## Non-Goals

- 物理シミュレーションベースの操作（衝突判定など）
- スナップ機能（グリッドスナップ等）の完璧な実装（次フェーズ）
- カスタムシェイプのハンドル（独自の3Dモデルをハンドルにする等）
- 複数オブジェクトの同時複雑操作（重心計算等）

## User Value

- 数値入力パネルへの往復を減らし、直感的にレイヤーを配置できる
- Z軸（奥行き）の移動が可視化され、3D空間の把握が容易になる
- 自由な視点移動（Orbit）を行いながら、特定の軸に沿った正確な調整ができる

## Interaction Rules

- **軸選択**: 軸の矢印部分をホバーするとハイライトされ、クリックでその軸を選択
- **平面選択**: 軸の間の四角いエリアをクリックで、その平面（XY, YZ, XZ）に拘束して移動
- **スクリーン平面**: 中央付近の白円ドラッグで、カメラの視線に対して平行な平面で移動
- **キー操作**: `Shift` 押下で増分固定、`Alt` 押下で操作モード（移動・回転・拡縮）の切り替え（パイメニューとの連携も考慮）

## Responsibilities

- `Artifact3DGizmo` (New)
  3Dトランスフォーム計算、描画データ生成、当たり判定
- `GizmoGeometry`
  軸、円、矩形などの 3D プリミティブ定義
- `CoordinateSpaceConverter`
  Viewport(Pixel) <-> Canvas(3D) <-> Layer(Local) の相互変換
- `CompositionRenderController`
  マウスイベントのディスパッチ、現在の Gizmo 状態の保持

---

## Milestones

### GIZ-1 3D Visual Foundation

目的:
3D 空間に X, Y, Z 軸を正しく描画し、カメラの動きに追従させる。

内容:
- `ArtifactIRenderer` への 3D プリミティブ描画命令（Line, Arrow, Circle）の追加
- 深度テストを無視（常に手前に表示）するか、あるいは半透明で隠れた部分を表現するシェーダー設定
- カメラの Projection 行列を用いた、Gizmo の画面上での一定サイズ維持（Screen Space Scaling）

### GIZ-2 Ray-Object Intersection

目的:
マウスクリックした場所がどの軸・ハンドルに当たっているかを 3D 空間で判定する。

内容:
- マウス座標からの Ray Casting（光線飛ばし）の実装
- 各ハンドル（矢印、円環、四角）に対する Ray-Capsule / Ray-Torus / Ray-Box 交差判定
- 視点からの距離に応じた優先順位付け

### GIZ-3 Constrained Transformation Logic

目的:
ドラッグ操作による「変化量」をトランスフォーム（Position, Rotation, Scale）に変換する。

内容:
- **移動**: マウス移動を軸方向のベクトルへ射影、または平面との交差座標を計算
- **回転**: スクリーン空間での円運動を 3D 軸周りの回転角へ変換
- **拡大**: 中心からの距離変化をスケール値へ変換
- `Undo/Redo` システムとの連携（操作開始時にスナップショット作成）

### GIZ-4 Active Context Integration

目的:
レイヤーの種類（2Dか3Dか）やエディタの状態に応じて Gizmo を自動で切り替える。

内容:
- `ArtifactAbstractLayer::is3D()` プロパティに基づく 2D Gizmo / 3D Gizmo の動的切り替え
- ツールバーおよびパイメニューからの「移動/回転/拡大」モード同期
- カメラ操作（Orbit）中の Gizmo 表示非表示制御

### GIZ-5 Advanced Visual Feedback

目的:
操作中の軸のハイライトや、数値のプレビュー表示など、操作性を高めるフィードバック。

内容:
- ホバー/ドラッグ中の色の変化
- 操作中の座標値/回転角のフローティング表示（オーバーレイ）
- ジンバルロックを避けるための回転表現

---

## Recommended Order

1. `GIZ-1 3D Visual Foundation`
2. `GIZ-2 Ray-Object Intersection`
3. `GIZ-3 Constrained Transformation Logic`
4. `GIZ-4 Active Context Integration`

## Exit Criteria

- 3Dレイヤーを選択したとき、画面上に 3D Gizmo が現れる
- 各軸をドラッグして、位置・回転・拡大縮小が意図通りに変化する
- カメラを回しても、Gizmo が常に正しい方向を向き、サイズが適切に保たれる
- 操作が Undo 可能である
