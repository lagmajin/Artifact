# Copilot Instructions

## Project Goals
- このアプリの目標・ビジョンは [PROJECT_GOALS.md](../PROJECT_GOALS.md) を参照してください。

## General Guidelines
- Use C++20 modules (e.g., `import std`) instead of traditional header includes (e.g., `#include`) whenever possible. Prioritize module imports to enhance code organization and efficiency.
- In Qt projects, prefer using Qt containers (e.g., `QVector`, `QList`) over standard containers like `std::vector` to maintain consistency and leverage Qt's features.
- When using Qt types in C++ module (.cpp, .cppm) files, always add the necessary Qt includes in the global module fragment before the `module;` declaration. For example, explicitly include `#include <QString>`, `#include <QVector>`, `#include <QColor>`, etc. This ensures that independent includes are present in the implementation file rather than relying on the module's external interface (.ixx).
- C++20 モジュールコードを書くときはちゃんとグローバルモジュールフラグメントも書いてほしい。

## File Extension Rules
**?? CRITICAL: This project uses C++20 modules. Always follow these file extension rules:**
- **Module interface files**: MUST use `.ixx` extension (NOT `.h` or `.hpp`)
- **Module implementation files**: MUST use `.cppm` extension (NOT `.cpp`)
- **? NEVER create `.cpp` files in this project - ALWAYS use `.cppm` for implementation files**
- When asked to create implementation files, source files, or .cpp files, you MUST create `.cppm` files instead
- This is a hard requirement - violations will cause build failures

Absolute rule about `.cpp` files (MANDATORY):
- As a general principle, do not create new `.cpp` files in this repository. New implementation source files MUST be `.cppm` placed under `src/`.
- Exception (allowed only when strictly necessary): if a corresponding `.cpp` implementation file already exists in the workspace for the same module/class, you may edit that existing `.cpp` file instead of creating a `.cppm`. Treat this as a last resort and prefer migrating to module-style `.cppm` when feasible.
- If there is no existing `.cpp` file for the target module/class, you MUST create a `.cppm` implementation file in `src/`. Do NOT create new `.cpp` files.
- Violating this rule will break the build and is not permitted.

**?? 重要：このプロジェクトはC++20モジュールを使用します。必ず以下のファイル拡張子ルールに従ってください：**
- **モジュールインターフェースファイル**：必ず `.ixx` 拡張子を使用（`.h` や `.hpp` は絶対に使用しない）
- **モジュール実装ファイル**：必ず `.cppm` 拡張子を使用（`.cpp` は絶対に使用しない）
- **? このプロジェクトでは絶対に `.cpp` ファイルを作成しないでください - 実装ファイルには必ず `.cppm` を使用**
- 実装ファイル、ソースファイル、または .cpp ファイルの作成を求められた場合、必ず `.cppm` ファイルを作成してください
- これは厳格な要件です - 違反するとビルドが失敗します

## File Location Rules
**?? MANDATORY: Always follow these folder structure rules when creating new files:**

### Module Interface Files (.ixx)
- ? **MUST be placed in `include/` folder or its subfolders**
- ? **Follow the same subfolder structure as the module namespace**
- ? **NEVER place `.ixx` files in `src/` folder**

### Module Implementation Files (.cppm)
- ? **MUST be placed in `src/` folder or its subfolders**
- ? **Follow the same subfolder structure as the corresponding `.ixx` file**
- ? **NEVER place `.cppm` files in `include/` folder**

### Examples (CORRECT):
```
// Module interface
include/Media/MediaAudioDecoder.ixx      ? Correct!

// Module implementation
src/Media/MediaAudioDecoder.cppm         ? Correct!

// Another example
include/Layer/ArtifactLayer.ixx          ? Correct!
src/Layer/ArtifactLayer.cppm             ? Correct!
```

### Anti-patterns (WRONG):
```
src/Media/MediaAudioDecoder.ixx          ? Wrong! .ixx must be in include/
include/Media/MediaAudioDecoder.cppm     ? Wrong! .cppm must be in src/
```

### Rules Summary:
- ? `.ixx` → `include/` folder
- ? `.cppm` → `src/` folder
- ? Maintain consistent subfolder structure between interface and implementation
- ? Never mix interface and implementation files in the same folder

**?? 必須：新規ファイル作成時は必ず以下のフォルダ構造ルールに従ってください：**

### モジュールインターフェースファイル (.ixx)
- ? **必ず `include/` フォルダまたはそのサブフォルダに配置**
- ? **モジュールの名前空間と同じサブフォルダ構造に従う**
- ? **絶対に `.ixx` ファイルを `src/` フォルダに配置しない**

### モジュール実装ファイル (.cppm)
- ? **必ず `src/` フォルダまたはそのサブフォルダに配置**
- ? **対応する `.ixx` ファイルと同じサブフォルダ構造に従う**
- ? **絶対に `.cppm` ファイルを `include/` フォルダに配置しない**

### 例：
```
// 正しい配置
include/Media/MediaAudioDecoder.ixx      ?
src/Media/MediaAudioDecoder.cppm         ?

// 間違った配置
src/Media/MediaAudioDecoder.ixx          ? .ixx は include/ に！
include/Media/MediaAudioDecoder.cppm     ? .cppm は src/ に！
```

## Pimpl Idiom (Implementation Pattern)
**?? MANDATORY: All classes MUST use the raw pointer Pimpl idiom:**

### Required Pattern:
```cpp
// In .ixx (interface file)
class MyClass {
private:
  class Impl;
  Impl* impl_;  // MUST be raw pointer, NOT std::unique_ptr, NOT std::shared_ptr
public:
  MyClass();
  ~MyClass();
  // ... other members
};

// In .cppm (implementation file)
class MyClass::Impl {
  // implementation details
};

MyClass::MyClass() : impl_(new Impl()) {}
MyClass::~MyClass() { delete impl_; }
```

### Rules:
- ? **ALWAYS use `Impl* impl_;` (raw pointer)**
- ? **NEVER use `std::unique_ptr<Impl> impl_;`**
- ? **NEVER use `std::shared_ptr<Impl> impl_;`**
- ? **ALWAYS manually manage memory with `new` and `delete`**
- ? **Forward declare Impl as `class Impl;`**
- ? **Define Impl class in the .cppm file**

### Example (CORRECT):
```cpp
// MyClass.ixx
export class MyClass {
private:
  class Impl;
  Impl* impl_;  // ? Correct!
public:
  MyClass();
  ~MyClass();
};

// MyClass.cppm
class MyClass::Impl {
  int data_;
};

MyClass::MyClass() : impl_(new Impl()) {}
MyClass::~MyClass() { delete impl_; }
```

### Anti-patterns (WRONG):
```cpp
// ? WRONG - Do NOT use smart pointers
std::unique_ptr<Impl> impl_;
std::shared_ptr<Impl> impl_;

// ? WRONG - Do NOT expose implementation in header
class Impl {
  int data_;
};
Impl* impl_;
```

**?? 必須：すべてのクラスは生ポインタのPimplイディオムを使用する必要があります：**
- ? **常に `Impl* impl_;` （生ポインタ）を使用**
- ? **絶対に `std::unique_ptr<Impl>` や `std::shared_ptr<Impl>` を使用しない**
- ? **`new` と `delete` で手動メモリ管理**
- ? **Implクラスは `.cppm` ファイルで定義**

## String Type Rules
**?? MANDATORY: Always use `UniString` instead of `QString` in public APIs:**

### Required Pattern:
```cpp
// ? CORRECT - Use UniString in public API
export class MyClass {
public:
  void setName(const UniString& name);
  UniString getName() const;
};

// In .cppm - Convert to QString only when necessary for Qt APIs
void MyClass::setName(const UniString& name) {
  QString qstr = name.toQString();  // Convert only when needed
  // Use Qt API with QString
}
```

### Rules:
- ? **ALWAYS use `UniString` for all public API parameters and return values**
- ? **ALWAYS use `UniString` for class member variables (in Impl class)**
- ? **NEVER use `QString` directly in public API (exported functions/methods)**
- ?? **Use `QString` ONLY when directly interacting with Qt APIs internally**
- ? **Convert `UniString` to `QString` only inside implementation (.cppm files)**
- ? **Always `import Utils.String.UniString;` when using UniString**

### Examples (CORRECT):
```cpp
// MyClass.ixx
export module MyModule;
import Utils.String.UniString;

export class MyClass {
public:
  void setTitle(const UniString& title);      // ? Correct!
  UniString getTitle() const;                 // ? Correct!
  void setItems(const std::vector<UniString>& items);  // ? Correct!
};

// MyClass.cppm
module;
#include <QString>

module MyModule;

class MyClass::Impl {
  UniString title_;  // ? Store as UniString
};

void MyClass::setTitle(const UniString& title) {
  impl_->title_ = title;
  // Convert only when calling Qt API
  someQtWidget->setText(title.toQString());
}
```

### Anti-patterns (WRONG):
```cpp
// ? WRONG - Do NOT use QString in public API
export class MyClass {
public:
  void setName(const QString& name);   // ? Wrong!
  QString getName() const;             // ? Wrong!
};

// ? WRONG - Do NOT store QString in public-facing structures
struct MyData {
  QString text;  // ? Wrong! Use UniString instead
};
```

### When to use QString:
- ? Inside `.cppm` implementation files when calling Qt APIs
- ? In private helper functions that don't cross module boundaries
- ? As temporary variables for Qt API calls
- ? NEVER in exported functions, classes, or structures

**?? 必須：パブリックAPIでは常に`UniString`を使用し、`QString`は使用しない：**
- ? **すべてのパブリックAPI（エクスポートされた関数・メソッド）で`UniString`を使用**
- ? **パブリックAPIで`QString`を直接使用しない**
- ?? **`QString`は実装内部でQt APIと直接やり取りする場合のみ使用**
- ? **`UniString`から`QString`への変換は実装ファイル（.cppm）内でのみ行う**

## Module and Source File Generation
- When creating new module files (.ixx, .cppm), ensure they are properly added to the .vcxproj file with appropriate compile settings. 
- For module implementation files (.cppm), always specify `<CompileAs Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">CompileAsCpp</CompileAs>` (or equivalent for other configurations) in the project file. 
- Verify module dependencies are correctly set up in the build system.
- モジュール＆ソースを生成するときはコンパイルオプションに気をつけてほしい。プロジェクトファイルへの追加と適切な CompileAs 設定を忘れずに。

## Project File Modification Rules
**?? CRITICAL/FORBIDDEN: DO NOT modify visual studio project files directly (.vcxproj, .vcxproj.filters)**
- ? **NEVER modify `.vcxproj` or `.vcxproj.filters` files directly, especially the filter definitions.**
- ? **Modifying these files manually breaks the Visual Studio project structure and filters.**
- ? **If a new file is created, instruct the user to add it to the project using Visual Studio, or use a safe method if one exists (which usually doesn't for filters).**
- ? **Under NO circumstances should you attempt to parse and rewrite the `.vcxproj` or `.filters` XML to "fix" include paths or filters.**

**?? 禁止事項：Visual Studioのプロジェクトファイルを直接変更しないでください**
- ? **`.vcxproj` や `.vcxproj.filters` ファイルを直接編集することは絶対に禁止です。特にフィルタ定義は触らないでください。**
- ? **これらのファイルを手動で変更すると、Visual Studioのプロジェクト構造やフィルタが壊れます。**
- ? **新しいファイルを作成した場合は、ユーザーにVisual Studioを使って追加するように指示してください。**

## ArtifactCore Library Usage (Preferred Classes)
**?? MANDATORY: Always prefer using ArtifactCore library classes instead of creating new implementations:**

### Image Processing
- **Use `ImageF32x4_RGBA`** (from `import Image.ImageF32x4_RGBA;`) for all float RGBA image data
  - Provides efficient cv::Mat-backed storage with conversion methods
  - Use `setFromCVMat(const cv::Mat&)` to convert various cv::Mat types
  - Use `toCVMat()` to get underlying cv::Mat for OpenCV operations
  - Supports pixel access, blending, cropping, flipping
- **Use `ImageF32x4RGBAWithCache`** (from `import Image.ImageF32x4RGBAWithCache;`) for GPU-accelerated images
  - Provides CPU-GPU synchronization for rendering pipelines
  - Use `UpdateGpuTextureFromCpuData()` and `UpdateCpuDataFromGpuTexture()` for sync
  - Wraps `ImageF32x4_RGBA` with GPU texture caching

### String Handling
- **Use `UniString`** (from `import Utils.String.UniString;`) for ALL public APIs and internal storage
  - Never use `QString` in public APIs - only convert to `QString` internally when calling Qt APIs
  - Use `toQString()` to convert to `QString` when needed
  - Use `setQString(const QString&)` to set from `QString`

### Identifiers
- **Use `CompositionID`** and `LayerID`** (from `import Utils.Id;`) for all ID types
  - Both inherit from `Id` class
  - Use `isNil()` to check validity
  - Use `toString()` for debugging/logging
  - Supports comparison operators and hashing

### Media & Video
- **Use `MediaAudioDecoder`** (from `import Media.MediaAudioDecoder;`) for audio decoding
- **Use `MediaPlaybackController`** (from `import Media.MediaPlaybackController;`) for playback control
- **Use `MediaSource`** (from `import Media.MediaSource;`) for media file handling
- **Use `FFMpegEncoder`** (from `import Video.FFMpegEncoder;`) for video encoding
- **Use `MediaMetaData`** (from `import Media.MediaMetaData;`) for media file metadata

### Frame & Time
- **Use `FramePosition`** (from `import Frame.Position;`) for frame positions
- **Use `FrameRate`** (from `import Frame.Rate;`) for frame rate handling
- **Use `FrameRange`** (from `import Frame.Range;`) for frame ranges
- **Use `RationalTime`** (from `import Time.RationalTime;`) for precise time calculations

### Color
- **Use `FloatRGBA`** (from `import FloatRGBA;`) for RGBA color in float [0-1] range
  - Constructor: `FloatRGBA(float r, float g, float b, float a = 1.0f)`
  - Accessors: `r()`, `g()`, `b()`, `a()`
- **Use `FloatColor`** (from `import Color.Float;`) for RGB color operations
- **Use `LUT`** (from `import Color.LUT;`) for color lookup tables

### Containers
- **Use `MultiIndexContainer<Ptr, Id, TypeKey>`** (from `import Container.MultiIndex;`) for indexed collections
  - Provides fast access by ID, type, and linear iteration
  - Use `add(ptr, id, typeKey)` to insert
  - Use `findById(id)` to retrieve by ID
  - Use `removeById(id)` to remove
  - Use `all()` to get all items as QVector

### Script & Expression Engine
- **Use `ExpressionParser`** and `ExpressionEvaluator`** (from `import Script.Expression.Parser;` and `import Script.Expression.Evaluator;`) for expression evaluation
- **Use `ScriptContext`** (from `import Script.Engine.Context;`) for script execution context

### Transforms & Animation
- **Use `AnimatableTransform2D`** and `AnimatableTransform3D`** (from `import Animation.Transform2D;` and `import Animation.Transform3D;`) for animated transformations
- **Use `StaticTransform2D/3D`** for non-animated transforms

### Utilities
- **Use `getIconPath()`** (from `import Utils.Path;`) to get the application icon directory path
- **Use `ScopedTimer`** (from `import Utils.ScopedTimer;`) for performance measurement

### Rules for ArtifactCore Usage
- ? **ALWAYS check if ArtifactCore provides the functionality before implementing from scratch**
- ? **Import the appropriate ArtifactCore module at the top of your implementation file**
- ? **Use ArtifactCore types in Pimpl Impl classes to keep implementation details hidden**
- ? **When converting between types (e.g., cv::Mat ? ImageF32x4_RGBA), use provided conversion methods**

## Expanded ArtifactCore Reference (quick lookup)
Use these preferred types and helpers when implementing features. Import modules shown in parentheses.

- Image & GPU
  - `ImageF32x4_RGBA` (`import Image.ImageF32x4_RGBA;`) ? float RGBA backed by `cv::Mat`.
  - `ImageF32x4RGBAWithCache` (`import Image.ImageF32x4RGBAWithCache;`) ? CPU/GPU cache, call `UpdateGpuTextureFromCpuData()`.
  - `GPUTexture`, `GPUTextureCacheManager` (`import Graphics.GPUTexture;`) ? low-level texture helpers used by renderer.

- Rendering pipeline
  - `RenderSettings`, `RendererQueueManager`, `RenderWorker` (`import Render.Renderer;`) ? use for scheduled render jobs.
  - `RenderJobModel` (`import Render.RenderJobModel;`) ? job description object for queueing.

- Video / Media
  - `FFMpegEncoder` (`import Video.FFMpegEncoder;`) ? exposures for encoding settings.
  - `MediaPlaybackController`, `MediaSource`, `MediaMetaData` (`import Media.MediaPlaybackController;`) ? playback and metadata.

- Composition / Project
  - `ArtifactProject`, `ArtifactProjectManager` (`import Artifact.Project.Manager;`) ? central project APIs.
  - `ArtifactAbstractComposition`, `CompositionID` (`import Artifact.Composition.Abstract;`, `import Utils.Id;`) ? composition containers and ids.
  - `MultiIndexLayerContainer` (`import Container.MultiIndex;`) ? store layers; use `add`, `findById`, `removeById`, `all()`.

- Layers
  - `ArtifactAbstractLayer` and concrete types (`import Layer.*;`) ? use factory `ArtifactLayerFactory` to create layers.
  - Layer metadata: `LayerID`, `LayerState`, `LayerBlendType` (`import Layer.LayerState;`).

- Time & Frames
  - `FramePosition`, `FrameRate`, `FrameRange`, `RationalTime` (`import Frame.Position;`, `import Time.RationalTime;`) ? canonical time types.

- Scripting
  - `ScriptContext`, `ExpressionParser`, `ExpressionEvaluator` (`import Script.Engine.Context;`) ? prefer these for expression evaluation and script bindings.

## Recommended Patterns and Examples

- Module import example (in `.cppm`):
  - Use global includes then module declaration, e.g.:
    ```cpp
    module;
    #include <QString>
    #include <QWidget>
    module Artifact.Widgets.MyWidget;

    import std;
    import Utils.String.UniString;
    import Image.ImageF32x4_RGBA;
    ```

- Pimpl usage (reminder): store ArtifactCore types inside `Impl` and convert in public methods.

- Signal & threading rules:
  - Emit UI signals on the main thread. Use `QMetaObject::invokeMethod` or `QTimer::singleShot(0, ...)` for cross-thread delivery.
  - Heavy CPU tasks (encoding, image processing) should run on worker threads; marshal results back to the UI via signals.

## Error handling and diagnostics

- Use `Q_ASSERT` for invariants during development, and return sensible `Result` objects for API errors (`CreateCompositionResult`, etc.).
- Log important events through ArtifactCore logging helpers (`import Log.Log;`) rather than printf.

## Tests and CI

- Add unit tests for conversions (QImage ? cv::Mat ? ImageF32x4_RGBA) and for project/composition add/remove flows.
- Ensure new modules compile under CI for all supported configurations (Debug/Release x64).

## Adding new UI dialogs or widgets

- Follow the existing pattern: create `.ixx` interface in `include/...` and `.cppm` implementation in `src/...`.
- Add resources (icons) under the project's icon path (`getIconPath()` helper) and load in UI code.

## Final checklist for contributors

1. Did you search ArtifactCore for existing functionality before implementing? (mandatory)
2. Did you add only `.ixx`/`.cppm` files per rules? (mandatory)
3. Are public APIs using `UniString` and IDs using `CompositionID`/`LayerID`? (mandatory)
4. Are UI signals marshalled to main thread? (recommended)
5. Are conversions and heavy processing delegated to ArtifactCore utilities? (recommended)

-- End of additional guidance

**?? 必須：ArtifactCoreライブラリの優先使用**
- ? **新しい実装を作る前に、必ずArtifactCoreに該当機能があるか確認してください**
- ? **画像処理には `ImageF32x4_RGBA` と `ImageF32x4RGBAWithCache` を使用**
- ? **文字列には `UniString`、IDには `CompositionID`/`LayerID` を使用**
- ? **メディア処理には `MediaAudioDecoder`, `MediaPlaybackController` などを使用**

## Code Style
- Follow specific formatting rules.
- Adhere to naming conventions.