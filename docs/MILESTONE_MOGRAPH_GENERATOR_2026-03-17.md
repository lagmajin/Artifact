# M12 MoGraph Generator Foundation (2026-03-17)

日付：2026-03-17  
目標：Cinema 4D MoGraph 風のジェネレーター/エフェクターシステムを実装する

---

## Goal

- オブジェクトを複製・変形する MoGraph 風ジェネレーターを実装
- 複数エフェクターをスタックして適用可能に
- リアルタイムプレビューで動作を確認可能

---

## Definition of Done

- [ ] **Cloner ジェネレーター** - グリッド/放射状/ランダム配置
- [ ] **Matrix ジェネレーター** - 点/グリッドマトリックス生成
- [ ] **Plain エフェクター** - 位置/回転/スケール変形
- [ ] **Random エフェクター** - ランダム変形
- [ ] エフェクターの順序制御（スタック）
- [ ] Inspector からパラメータ編集可能
- [ ] リアルタイムプレビューで視認可能

---

## Design Concept

### Cinema 4D MoGraph を参考にした理由

1. **直感的な操作** - ジェネレーターで生成、エフェクターで変形
2. **モジュラー構成** - 組み合わせで無限の表現
3. **リアルタイム** - 即時フィードバック
4. **モーショングラフィックスに最適** - 量産・変形が主目的

---

## Architecture

### パイプライン構成

```
┌─────────────────────────────────────────────────────────────┐
│                    MoGraph Pipeline                         │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐  │
│  │  Generator   │───▶│  Effector 1  │───▶│  Effector 2  │  │
│  │  (Cloner)    │    │  (Plain)     │    │  (Random)    │  │
│  └──────────────┘    └──────────────┘    └──────────────┘  │
│         │                   │                   │           │
│         ▼                   ▼                   ▼           │
│  ┌─────────────────────────────────────────────────────┐    │
│  │              Matrix Data (Transform Array)          │    │
│  │  [位置，回転，スケール，カラー，...] × N instances  │    │
│  └─────────────────────────────────────────────────────┘    │
│                             │                                │
│                             ▼                                │
│                    ┌──────────────┐                          │
│                    │   Renderer   │                          │
│                    │  (Instance)  │                          │
│                    └──────────────┘                          │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### データフロー

1. **Generator** が Matrix データ（変換行列の配列）を生成
2. **Effector** が Matrix データを順に変形
3. **Renderer** が変換された行列でインスタンス描画

---

## Effect Pipeline Structure

### 既存のエフェクトパイプライン構造

現在の Inspector は 6 段階のエフェクトステージを持つ：

```cpp
enum class EffectPipelineStage {
    PreProcess,       // 0: 前処理
    Generator,        // 1: ジェネレーター
    GeometryTransform,// 2: 形状変形
    MaterialRender,   // 3: マテリアル/レンダリング
    Rasterizer,       // 4: ラスタライズ
    LayerTransform    // 5: レイヤー変形
};
```

### Inspector ラック構成

| ラック | ステージ | カテゴリ | 実装エフェクト |
| --- | --- | --- | --- |
| **Rack 0** | PreProcess | 前処理 | （未実装） |
| **Rack 1** | Generator | ジェネレーター | Cloner, FractalNoise |
| **Rack 2** | GeometryTransform | 形状変形 | Twist, Bend |
| **Rack 3** | MaterialRender | マテリアル/レンダリング | PBRMaterial |
| **Rack 4** | Rasterizer | ラスタライズ | Blur, Glow, DropShadow, Wave, Spherize |
| **（予備）** | LayerTransform | レイヤー変形 | Transform2D |

### MoGraph エフェクターの統合方針

**Generator ステージ（Rack 1）に MoGraph ジェネレーターを配置**

```
MoGraph Generator Pipeline:
┌──────────────────────────────────────────────────────────┐
│ Rack 1: Generator                                        │
│ ┌────────────────────────────────────────────────────┐   │
│ │ MoGraph Generator (Cloner/Matrix/Fracture)         │   │
│ └────────────────────────────────────────────────────┘   │
│                                                          │
│ Rack 2: GeometryTransform                                │
│ ┌────────────────────────────────────────────────────┐   │
│ │ MoGraph Effector 1 (Plain/Random/Step/Delay)       │   │
│ └────────────────────────────────────────────────────┘   │
│                                                          │
│ Rack 3: MaterialRender                                   │
│ ┌────────────────────────────────────────────────────┐   │
│ │ MoGraph Effector 2 (Shader/Color/Field)            │   │
│ └────────────────────────────────────────────────────┘   │
│                                                          │
│ Rack 4: Rasterizer                                       │
│ ┌────────────────────────────────────────────────────┐   │
│ │ MoGraph Effector 3 (Blur/Glow/Distortion)          │   │
│ └────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────┘
```

### 統合メリット

1. **既存構造を再利用** - 新しい UI システム不要
2. **既存エフェクトと併用可能** - 従来エフェクトと MoGraph の混在
3. **段階的導入** - 1 つのステージから実装開始可能
4. **既存 Inspector UI 継続利用** - チェックボックス/順序制御/パラメータ表示

---

## MoGraph 拡張仕様

### 新規データ構造

```cpp
// Matrix データ（インスタンス変換情報）
struct MatrixInstance {
    QMatrix4x4 transform;      // 変換行列
    FloatColor color{1,1,1,1}; // カラー
    float scale{1.0f};         // 追加スケール
    int index{0};              // インデックス
};

class MatrixData {
public:
    std::vector<MatrixInstance> instances;
    
    void reserve(int count);
    void addInstance(const MatrixInstance& inst);
    int count() const;
    
    MatrixInstance& operator[](int index);
    const MatrixInstance& operator[](int index) const;
};

// ジェネレーターコンテキスト
struct GeneratorContext {
    int count = 10;            // 生成数
    double time = 0.0;         // 現在時間
    int frameIndex = 0;        // フレーム番号
    float frameRate = 30.0f;   // フレームレート
};

// エフェクターコンテキスト
struct MoGraphEffectorContext {
    double time = 0.0;         // 現在時間
    int frameIndex = 0;        // フレーム番号
    float frameRate = 30.0f;   // フレームレート
    float strength = 1.0f;     // エフェクター強度
    int seed = 0;              // ランダムシード
};
```

### 基底クラス

```cpp
// MoGraph ジェネレーター基底
class ArtifactMoGraphGenerator : public ArtifactAbstractEffect {
public:
    virtual MatrixData generate(const GeneratorContext& ctx) = 0;
    
    // EffectPipelineStage は Generator を返す
    EffectPipelineStage pipelineStage() const override {
        return EffectPipelineStage::Generator;
    }
};

// MoGraph エフェクター基底
class ArtifactMoGraphEffector : public ArtifactAbstractEffect {
public:
    virtual void process(MatrixData& data, const MoGraphEffectorContext& ctx) = 0;
    
    float strength() const { return strength_; }
    void setStrength(float s) { strength_ = s; }
    
protected:
    float strength_ = 1.0f;
};
```

---

## Milestones

### M-MOGRAPH-1: Generator Foundation

**目標**: ジェネレーター基底クラスと Cloner 実装

**完了条件**:
- [ ] `ArtifactMoGraphGenerator` 基底クラス
- [ ] `ArtifactClonerGenerator` 実装（グリッド/放射状/ランダム）
- [ ] `ArtifactMatrixData` データ構造
- [ ] Inspector からパラメータ編集

**API 案**:
```cpp
// ジェネレーター基底
class ArtifactMoGraphGenerator {
public:
    virtual MatrixData generate(const GeneratorContext& ctx) = 0;
    virtual std::vector<PropertyGroup> getProperties() const;
};

// Cloner ジェネレーター
class ArtifactClonerGenerator : public ArtifactMoGraphGenerator {
public:
    enum class Mode { Grid, Radial, Random, Object };
    
    void setMode(Mode mode);
    void setCount(int count);
    void setGridSize(const Vector3& size);
    void setRadialRadius(float radius);
    // ...
    
    MatrixData generate(const GeneratorContext& ctx) override;
    
private:
    Mode mode_;
    int count_ = 10;
    Vector3 gridSize_{100, 100, 100};
    float radialRadius_ = 100;
    // ...
};
```

**見積**: 6-10h

### Progress Note (2026-03-27)

- `Cloner (Generator)` をエフェクトタブから追加できるようにした。
- `ClonerGenerator` に最小のプロパティと `generateCloneData()` を入れた。
- image / text / svg / video / solid 系の描画へ clone 展開を接続した。
- 個別 clone 編集はまだ行わない。source layer と generator パラメータだけを編集対象にする。

---

### M-MOGRAPH-2: Effector Foundation

**目標**: エフェクター基底と Plain/Random 実装

**完了条件**:
- [ ] `ArtifactMoGraphEffector` 基底クラス
- [ ] `ArtifactPlainEffector` 実装（位置/回転/スケール）
- [ ] `ArtifactRandomEffector` 実装（ランダム変形）
- [ ] エフェクターのスタック管理
- [ ] Inspector から順序変更

**API 案**:
```cpp
// エフェクター基底
class ArtifactMoGraphEffector {
public:
    virtual void process(MatrixData& data, const EffectorContext& ctx) = 0;
    virtual std::vector<PropertyGroup> getProperties() const;
    
    // パラメータ
    float strength_ = 1.0f;  // 影響度
    bool enabled_ = true;
};

// Plain エフェクター（基本変形）
class ArtifactPlainEffector : public ArtifactMoGraphEffector {
public:
    void setPositionOffset(const Vector3& offset);
    void setRotationOffset(const Vector3& eulerDeg);
    void setScaleFactor(const Vector3& factor);
    
    void process(MatrixData& data, const EffectorContext& ctx) override;
    
private:
    Vector3 positionOffset_{0, 0, 0};
    Vector3 rotationOffset_{0, 0, 0};
    Vector3 scaleFactor_{1, 1, 1};
};

// Random エフェクター
class ArtifactRandomEffector : public ArtifactMoGraphEffector {
public:
    void setRandomSeed(int seed);
    void setRandomPosition(const Vector3& range);
    void setRandomRotation(const Vector3& rangeDeg);
    void setRandomScale(const Vector3& range);
    
    void process(MatrixData& data, const EffectorContext& ctx) override;
    
private:
    int seed_ = 0;
    Vector3 randomPositionRange_{50, 50, 50};
    Vector3 randomRotationRange_{45, 45, 45};
    Vector3 randomScaleRange_{0.5, 0.5, 0.5};
};
```

**見積**: 6-10h

---

### M-MOGRAPH-3: UI Integration

**目標**: Inspector 統合とプレビュー

**完了条件**:
- [ ] MoGraph Generator/Effectors カテゴリ
- [ ] エフェクター追加ダイアログ
- [ ] エフェクタースタック UI（順序変更）
- [ ] パラメータプロパティ表示
- [ ] リアルタイムプレビュー連携

**UI 構成案**:
```
┌─────────────────────────────────────┐
│ MoGraph                             │
├─────────────────────────────────────┤
│ Generator: [Cloner        ▼]        │
│                                     │
│ ── Cloner Parameters ───            │
│ Mode:      [Grid         ▼]         │
│ Count:     [30          ]           │
│ Grid Size: [100,100,100]            │
│                                     │
│ ── Effectors (2) ───────            │
│ ☑ Plain Effector                    │
│   └─ Position: (10, 0, 0)           │
│ ☑ Random Effector                   │
│   └─ Random Pos: (50, 50, 50)       │
│ [+ Add Effector]                    │
└─────────────────────────────────────┘
```

**見積**: 4-6h

---

### M-MOGRAPH-4: Additional Effectors

**目標**: 追加エフェクターで表現力向上

**完了条件**:
- [ ] `Step Effector` - 段階的変化
- [ ] `Delay Effector` - 遅延効果
- [ ] `Formula Effector` - 数式ベース
- [ ] `Spline Effector` - スプライン沿い配置

**実装優先度**:
1. Step - 実装簡単、使用頻度高
2. Delay - アニメーションに有用
3. Formula - 上級者向け
4. Spline - 要パスシステム

**見積**: 8-12h（合計）

---

### M-MOGRAPH-5: Animation & Time

**目標**: タイムライン連携とアニメーション

**完了条件**:
- [ ] エフェクターパラメータのキーフレーム
- [ ] Time Offset（インデックス別時間オフセット）
- [ ] Sound Effector（音声反応）
- [ ] プレビュー再生連携

**API 案**:
```cpp
// タイムコンテキスト
struct EffectorContext {
    double time = 0.0;           // 現在時間
    int frameIndex = 0;          // フレーム番号
    float frameRate = 30.0f;     // フレームレート
    
    // タイムオフセット（インデックス別）
    float getTimeOffset(int index) const;
    void setTimeOffsetMode(OffsetMode mode);  // Linear, Step, Sine...
};
```

**見積**: 6-10h

---

## Recommended Order

1. **M-MOGRAPH-1** - Generator Foundation（Cloner 実装）
2. **M-MOGRAPH-2** - Effector Foundation（Plain/Random）
3. **M-MOGRAPH-3** - UI Integration（Inspector 統合）
4. **M-MOGRAPH-4** - Additional Effectors（Step/Delay 等）
5. **M-MOGRAPH-5** - Animation & Time（キーフレーム連携）

---

## Total Estimate

| フェーズ | 見積時間 |
| --- | --- |
| M-MOGRAPH-1 | 6-10h |
| M-MOGRAPH-2 | 6-10h |
| M-MOGRAPH-3 | 4-6h |
| M-MOGRAPH-4 | 8-12h |
| M-MOGRAPH-5 | 6-10h |
| **合計** | **30-48h** |

---

## Dependencies

### 既存システム
- `ArtifactAbstractLayer` - レイヤー基底クラス
- `PropertyGroup` - プロパティシステム
- `ArtifactIRenderer` - インスタンス描画
- `AnimatableTransform` - アニメーション

### 新規実装
- `MatrixData` - 変換行列配列
- `MoGraphGenerator` - ジェネレーター基底
- `MoGraphEffector` - エフェクター基底
- `MoGraphLayer` - MoGraph 専用レイヤー

---

## Implementation Notes

### MatrixData 構造

```cpp
struct MatrixInstance {
    QMatrix4x4 transform;      // 変換行列
    FloatColor color{1,1,1,1}; // カラー
    float scale{1.0f};         // 追加スケール
    int index{0};              // インデックス
};

class MatrixData {
public:
    std::vector<MatrixInstance> instances;
    
    void reserve(int count);
    void addInstance(const MatrixInstance& inst);
    int count() const;
    
    // データアクセス
    MatrixInstance& operator[](int index);
    const MatrixInstance& operator[](int index) const;
};
```

### インスタンス描画

DiligentEngine / Software Renderer 両対応：

```cpp
// Renderer 側でインスタンス描画
void renderMoGraphLayer(ArtifactMoGraphLayer* layer, ArtifactIRenderer* renderer) {
    const MatrixData& data = layer->getMatrixData();
    
    for (const auto& inst : data.instances) {
        renderer->drawInstanced(layer->getSourceMesh(), inst.transform);
    }
}
```

---

## Future Extensions

### 未実装（将来）

- **Fracture Generator** - オブジェクト断片化
- **Object Generator** - オブジェクト表面に配置
- **Target Effector** - ターゲット指向
- **Inheritance Effector** - 親オブジェクトから継承
- **Volume Effector** - ボリュームベース
- **Fields System** - 影響範囲フィールド（C4D 風）
- **MoGraph Selection** - インデックス選択
- **Render Instance** - 軽量インスタンス描画

---

## First Step

**M-MOGRAPH-1** から着手。

最初にやること：
1. `MatrixData` データ構造実装
2. `ArtifactMoGraphGenerator` 基底クラス
3. `ArtifactClonerGenerator`（Grid/Radial/Random モード）
4. テスト用プレビューウィジェット
