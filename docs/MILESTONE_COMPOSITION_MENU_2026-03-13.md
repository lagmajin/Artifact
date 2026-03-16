# Composition Menu Milestone

Composition Menu / コンポジションメニューの機能強化と UX 改善のための実装メモ。

## M-CM-1 Basic Composition Operations

- 目標:
  コンポジションの作成・編集・削除を直感的に行えるようにする。
- 対象:
  新規作成、複製、名前変更、削除、設定変更。
- 完了条件:
  - `Ctrl+N` で新規コンポジション作成ダイアログが開く
  - コンポジションを複製できる
  - 名前変更が F2 または右クリックメニューから可能
  - 削除時に確認ダイアログが表示される
  - 設定（解像度、FPS、フレーム範囲）を変更できる

## M-CM-2 Composition Presets

- 目標:
  頻繁な解像度/FPS 設定をプリセットから選択できるようにする。
- 対象:
  解像度プリセット、FPS プリセット、カスタムプリセット保存。
- 完了条件:
  - HD 1920x1080 30fps などの標準プリセットを利用可能
  - 4K 3840x2160 60fps などの高解像度プリセットに対応
  - SNS 向け（Instagram, YouTube Shorts など）プリセットを提供
  - カスタムプリセットの保存・読み込みが可能

## M-CM-3 Composition Navigation

- 目標:
  複数のコンポジション間を素早く移動・切り替えできるようにする。
- 対象:
  タブ切り替え、最近使用したコンポジション、お気に入り登録。
- 完了条件:
  - 開いているコンポジションをタブで切り替え可能
  - 最近使用したコンポジションの一覧を表示
  - お気に入りコンポジションを登録・管理可能
  - コンポジション検索機能を提供

## M-CM-4 Composition Organization

- 目標:
  プロジェクト内のコンポジションを整理・階層化できるようにする。
- 対象:
  フォルダ分け、ネストされたコンポジション、ビン整理。
- 完了条件:
  - コンポジションをフォルダに整理可能
  - ネストされたコンポジション（プレコンポ）を作成可能
  - フォルダの展開・折りたたみが可能
  - 未使用コンポジションのフィルタ表示

## M-CM-5 Composition Export Integration

- 目標:
  コンポジションメニューから直接エクスポート・レンダリングを設定できるようにする。
- 対象:
  クイックエクスポート、レンダーキュー追加、エクスポート設定。
- 完了条件:
  - 現在のコンポジションをレンダーキューに追加
  - クイックエクスポート（現在のフレーム、ワークエリア）
  - エクスポート設定（フォーマット、コーデック、解像度）をメニューから設定
  - バッチエクスポート（複数コンポジション一括）に対応

## First Pass Notes

- `2026-03-13`:
  - コンポジションメニューの基本構造を整理
  - 新規作成 (`Ctrl+N`)、複製、削除のアクションを実装
  - コンポジション設定ダイアログを改善（プリセット追加）
  - 右クリックメニューに「レンダーキューに追加」を追加
  - エクスポート機能（現在のフレーム、ワークエリア）を実装

## Implementation Status

### ✅ Completed (2026-03-13)

- [x] 新規コンポジション作成 (`Ctrl+N`)
- [x] コンポジション複製
- [x] 名前変更 (F2 / 右クリック)
- [x] 削除（確認ダイアログ付き）
- [x] 設定ダイアログ（解像度、FPS、フレーム範囲）
- [x] レンダーキューに追加機能
- [x] エクスポート機能（現在のフレーム、ワークエリア）

### 🔄 In Progress

- [ ] コンポジションプリセットの充実
- [ ] タブ切り替え UI
- [ ] 最近使用したコンポジション一覧

### 📋 Backlog

- [ ] お気に入りコンポジション登録
- [ ] コンポジション検索
- [ ] フォルダ整理機能
- [ ] ネストされたコンポジション UI
- [ ] バッチエクスポート

## Next Slice for M-CM-2 (Presets)

- 標準プリセットの定義（HD, 4K, SNS 向け）
- プリセット保存・読み込み機能
- カスタムプリセットの永続化（設定ファイル）

## Next Slice for M-CM-3 (Navigation)

- タブベースのコンポジション切り替え UI
- 最近使用したコンポジションの記録（最大 10 件）
- キーボードショートカットでの切り替え (`Ctrl+Tab`)

## Technical Notes

### Composition Menu Structure

```cpp
// ArtifactCompositionMenu.cppm
class ArtifactCompositionMenu : public QMenu {
  // Actions
  - newCompositionAction      (Ctrl+N)
  - duplicateCompositionAction
  - renameCompositionAction   (F2)
  - deleteCompositionAction
  - settingsAction            (Ctrl+Shift+B)
  
  // Export submenu
  - addToQueueAction          (Ctrl+M)
  - exportCurrentFrameAction  (Ctrl+Alt+E)
  - exportWorkAreaAction      (Ctrl+E)
  
  // Presets submenu
  - hdPresetAction
  - fourKPresetAction
  - instagramPresetAction
  - youtubeShortsPresetAction
};
```

### Composition Settings Dialog

```cpp
// ArtifactCreateCompositionDialog.cppm
class CreateCompositionDialog : public QDialog {
  // Basic settings
  - nameEdit          : QLineEdit
  - presetCombo       : QComboBox (プリセット選択)
  - widthSpin         : QSpinBox (解像度 幅)
  - heightSpin        : QSpinBox (解像度 高さ)
  - fpsSpin           : QDoubleSpinBox (フレームレート)
  - durationSpin      : QSpinBox (再生時間)
  - bgColorButton     : QColorButton (背景色)
  
  // Advanced settings
  - startFrameSpin    : QSpinBox
  - endFrameSpin      : QSpinBox
  - pixelAspectCombo  : QComboBox
};
```

## Related Documents

- `MILESTONE_PROJECT_VIEW_2026-03-12.md` - Project View との連携
- `MILESTONE_ASSET_SYSTEM_2026-03-12.md` - アセットシステムとの統合
- `MILESTONE_FILE_MENU_2026-03-13.md` - ファイルメニューとの連携
- `MILESTONE_EDIT_MENU_2026-03-13.md` - 編集メニューとの連携

## Open Questions

1. **ネストされたコンポジションの扱い**
   - プレコンポとして扱うか、独立した機能とするか
   - タイムライン上の表現方法

2. **コンポジションテンプレートの保存形式**
   - 単独ファイル (.artifactcomp) とするか
   - プロジェクト内に埋め込むか

3. **タブ UI の実装場所**
   - Composition Viewer ドックに統合するか
   - 独立したタブバーを追加するか

## References

- After Effects: Composition Menu
- Premiere Pro: Sequence Menu
- DaVinci Resolve: Timeline Menu
- Final Cut Pro: Project Menu
