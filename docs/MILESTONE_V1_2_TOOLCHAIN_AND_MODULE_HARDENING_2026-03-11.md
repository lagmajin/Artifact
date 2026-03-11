# Artifact v1.2 Toolchain and Module Hardening (2026-03-11)

v1.2 は、C++23 / `import std` / C++ Modules の実運用安定化を目的としたフェーズ。

## Goal

次の 6 項目を満たしたら v1.2 を達成とする。

1. Windows 向け前提マクロ（`_WIN32_WINNT` など）を全ターゲットで統一
2. `import std` を小規模パイロットから段階展開できる
3. モジュール境界でマクロ依存コードを排除し、型ベース登録へ移行
4. CMake の `CXX_MODULES` / FILE_SET エラー再発を防止
5. `std` モジュール利用時の環境不一致警告（C5050）を縮小
6. ビルド障害時の診断手順をドキュメント化

## Definition Of Done

- `Artifact / ArtifactCore / ArtifactWidgets` で Windows ターゲット定義が統一される
- `import std` パイロットで2〜5ファイル規模の成功実績がある
- モジュール内でマクロ起因の構文崩れ（C4430など）が残っていない
- Toolchain 診断チェックリストが docs に存在する

## Work Packages

### 1. Windows Target Macro Normalization
- `_WIN32_WINNT` / `WINVER` をターゲット定義へ明示
- CI/ローカル差分が出ないよう CMake 集約

### 2. import std Pilot and Rollout
- 依存の薄い `.cppm` から順次移行
- 失敗パターン（定義不一致、header混在）をテンプレ化
- 進捗 (2026-03-11):
  - `ArtifactSeekBar.cppm` で `import std;` パイロット適用
  - `ArtifactEffectMenu.cppm` で `import std;` パイロット適用

### 3. Macro-Free Module Registration
- `REGISTER_*` 系を型ベース静的登録へ置換
- マクロ非透過問題を根絶

### 4. CMake Module File-Set Robustness
- モジュール収集スクリプトの重複/誤検出対策を強化
- dyndep エラーの再現条件と回避策を固定

### 5. Build Diagnostics Playbook
- 典型エラー（C4430, C5050, FILE_SET, LNK2019）の対処手順化

## Suggested Order

1. `Windows Target Macro Normalization`
2. `Macro-Free Module Registration`
3. `import std Pilot and Rollout`
4. `CMake Module File-Set Robustness`
5. `Build Diagnostics Playbook`
