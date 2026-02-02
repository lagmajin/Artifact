# Copilot Instructions

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

## Code Style
- Follow specific formatting rules.
- Adhere to naming conventions.