# ArtifactCore Library Reference

このドキュメントはArtifactCoreライブラリの主要クラス・ユーティリティの使い方と設計方針をまとめたものです。

## 優先度の高いクラス・ユーティリティ

### Image Processing
- **ImageF32x4_RGBA**
  - float RGBA画像データ。OpenCVのcv::Matと相互変換可能。
  - GPUキャッシュはImageF32x4RGBAWithCacheを使用。
  - API例: `setFromCVMat(const cv::Mat&)`, `toCVMat()`

### String Handling
- **UniString**
  - すべてのパブリックAPIで使用。QtのQStringとの変換は`toQString()`/`setQString()`。

### Identifiers
- **CompositionID / LayerID**
  - 型安全なID。`isNil()`で有効性判定、`toString()`で文字列化。

### Containers
- **MultiIndexContainer**
  - ID・型・線形アクセス可能な高速コンテナ。`add`/`findById`/`removeById`/`all()`。

### Media & Playback
- **MediaPlaybackController**
  - メディア再生制御。MediaSource/MediaMetaDataと連携。

### Mesh/Geometry
- **Mesh**
  - 頂点・インデックス・サブメッシュ・ボーン・マテリアル情報を保持。
  - MeshImporter/MeshEncoderで入出力。

### Material
- **Material**
  - PBR/Standard/Unlit/MaterialX対応。色・テクスチャ・MaterialX XMLを保持。

### Utilities
- **ExplorerUtils**
  - `openInExplorer(path, select)`でOSのエクススプローラー/ファインダー/ファイルマネージャを開く。
  - `select`でファイル選択状態も可能（Win/Macのみ）。

---

## Mask & Rotoscoping (2026/02/23 追加)

### RotoMask
- アニメーション対応ベジェパスマスク。
- 各頂点の位置・タンジェントが独立してアニメーション可能。
- キーフレーム管理、時間サンプリング、ラスタライズ機能。

```cpp
import Core.Mask.RotoMask;

RotoMask mask;
auto v1 = mask.addVertex(QPointF(100, 100));
mask.setVertexPosition(v1, QPointF(150, 100), 1.0); // 1秒時点で位置変更
mask.setFeather(5.0, 0.0);

// ラスタライズ
std::vector<float> data(width * height);
mask.rasterize(time, width, height, data.data());
```

### RotoMaskEditor
- ベジェパス編集UIウィジェット。
- 4つの編集モード（Select, Draw, Edit, Delete）。
- ズーム・パン、グリッド表示、アンドゥ/リドゥ対応。

```cpp
import Core.UI.RotoMaskEditor;

RotoMaskEditor* editor = new RotoMaskEditor(parent);
editor->setMask(&mask);
editor->setEditMode(RotoEditMode::Draw);
```

---

## Color & LUT (2026/02/23 追加)

### ColorLUT
- 3Dカラールックアップテーブル管理。
- CUBE, CSP, 3dl, HaldCLUT形式対応。
- 三線形補間による高品質な色変換。

```cpp
import Color.LUT;

ColorLUT lut("my_lut.cube");
QColor result = lut.apply(color);
QImage processed = lut.applyToImage(sourceImage);

// 強度指定
QColor blended = lut.applyWithIntensity(color, 0.5f);
```

### LUTManager
- 複数LUTの一括管理。
- ビルトインLUT（cinematic, vintage, warm, cold等）。

```cpp
auto& manager = LUTManager::instance();
manager.loadFromDirectory("/path/to/luts");
ColorLUT lut = manager.getLUT("cinematic");

// ビルトインLUT使用
ColorLUT vintage = BuiltinLUTs::vintage();
```

---

## Shape & Vector Graphics (2026/02/23 追加)

### ShapePath
- ベジェ曲線パス。MoveTo, LineTo, CubicTo, QuadTo等のコマンド対応。
- プリミティブ図形（矩形、楕円、多角形、星形）生成。

```cpp
import Shape.Path;

ShapePath path;
path.moveTo(0, 0);
path.cubicTo(QPointF(50, 0), QPointF(50, 100), QPointF(100, 100));

// プリミティブ
path.setRectangle(0, 0, 100, 100);
path.setEllipse(QPointF(50, 50), 50, 50);
path.setStar(QPointF(100, 100), 5, 100, 50); // 5点星
```

### ShapeGroup / ShapeElement
- シェイプ要素の階層構造。
- PathShape, RectanglePathShape, EllipsePathShape, StarPathShape等。

### ShapeLayer
- After Effects風シェイプレイヤー。
- ストローク・フィル設定、ブレンドモード対応。

```cpp
import Shape.Layer;

// プリミティブ作成
auto rectLayer = ShapeLayer::createRectangle(
    QRectF(0, 0, 100, 100),
    FillSettings(Qt::blue),
    StrokeSettings(Qt::white, 2.0)
);

// SVG入出力
QString svg = layer.toSvg();
ShapeLayer imported = ShapeLayer::fromSvg(svgContent);
```

---

## Motion Tracking (2026/02/23 追加)

### MotionTracker
- オブジェクト追跡機能。オプティカルフロー、特徴点追跡対応。
- 順方向/逆方向トラッキング、範囲トラッキング。
- 結果の補間、スムージング、外れ値除去。

```cpp
import Tracking.MotionTracker;

MotionTracker tracker;
tracker.setName("Main Tracker");
tracker.setSettings(TrackerSettings::highQuality());

// トラッキングポイント追加
tracker.addTrackPoint(QPointF(100, 100));
tracker.addTrackPoints({QPointF(50, 50), QPointF(150, 50)});

// フレーム設定とトラッキング実行
tracker.setFrame(0.0, frame1);
tracker.setFrame(0.04, frame2);
tracker.trackForward(0.0, 0.04);

// 範囲トラッキング
tracker.trackRange(0.0, 10.0, [](double progress) {
    qDebug() << "Progress:" << progress * 100 << "%";
    return true; // 継続
});

// 結果取得
QPointF pos = tracker.pointPositionAt(pointId, 5.0);
auto keyframes = tracker.exportKeyframes(pointId);

// 補正
tracker.smoothTrack(5);
tracker.removeOutliers(3.0);
```

### TrackerManager
- 複数トラッカーの一括管理。

```cpp
auto& manager = TrackerManager::instance();
MotionTracker* t1 = manager.createTracker("Tracker 1");
MotionTracker* t2 = manager.createTracker("Tracker 2");
```

---

## Property Types (2026/02/23 追加)

### Point2DValue / Point3DValue
- 2D/3D座標プロパティ型。アニメーション対応。

```cpp
import Property.Types;

Point2DValue pos(100.0, 200.0);
QPointF qtPos = pos.toQPointF();

// 演算
Point2DValue offset(10, 0);
auto newPos = pos + offset;
double dist = Point2DValue::distance(pos, newPos);
```

---

## 使い方例・APIサンプル

```cpp
import Image.ImageF32x4_RGBA;
ImageF32x4_RGBA img;
img.setFromCVMat(mat);

import Utils.ExplorerUtils;
ArtifactCore::openInExplorer(filePath, true); // ファイルを選択状態で開く

import Core.Mask.RotoMask;
RotoMask mask;
mask.addVertex(QPointF(0, 0));

import Tracking.MotionTracker;
MotionTracker tracker;
tracker.trackRange(0.0, 5.0);
```

---

## その他の推奨クラス・ユーティリティ
- MediaAudioDecoder, MediaSource, FFMpegEncoder, FramePosition, FrameRate, FloatRGBA, ExpressionParser, ScriptContext, AnimatableTransform2D/3D, getIconPath, ScopedTimer など

---

## 設計・利用方針
- まずArtifactCoreに既存機能があるか確認し、独自実装は避ける
- Pimplパターン・UniString型・ID型を必ず使用
- UI層からはArtifactCoreのAPIを直接呼び出す