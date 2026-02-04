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
  - `openInExplorer(path, select)`でOSのエクスプローラー/ファインダー/ファイルマネージャを開く。
  - `select`でファイル選択状態も可能（Win/Macのみ）。

---

## 使い方例・APIサンプル

```cpp
import Image.ImageF32x4_RGBA;
ImageF32x4_RGBA img;
img.setFromCVMat(mat);

import Utils.ExplorerUtils;
ArtifactCore::openInExplorer(filePath, true); // ファイルを選択状態で開く
```

---

## その他の推奨クラス・ユーティリティ
- MediaAudioDecoder, MediaSource, FFMpegEncoder, FramePosition, FrameRate, FloatRGBA, LUT, ExpressionParser, ScriptContext, AnimatableTransform2D/3D, getIconPath, ScopedTimer など

---

## 設計・利用方針
- まずArtifactCoreに既存機能があるか確認し、独自実装は避ける
- Pimplパターン・UniString型・ID型を必ず使用
- UI層からはArtifactCoreのAPIを直接呼び出す
