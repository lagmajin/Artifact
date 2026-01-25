# Copilot Instructions

## General Guidelines
- Use C++20 modules (e.g., `import std`) instead of traditional header includes (e.g., `#include`) whenever possible. Prioritize module imports to enhance code organization and efficiency.
- In Qt projects, prefer using Qt containers (e.g., `QVector`, `QList`) over standard containers like `std::vector` to maintain consistency and leverage Qt's features.
- When using Qt types in C++ module (.cpp, .cppm) files, always add the necessary Qt includes in the global module fragment before the `module;` declaration. For example, explicitly include `#include <QString>`, `#include <QVector>`, `#include <QColor>`, etc. This ensures that independent includes are present in the implementation file rather than relying on the module's external interface (.ixx).
- C++20 モジュールコードを書くときはちゃんとグローバルモジュールフラグメントも書いてほしい。

## Module and Source File Generation
- When creating new module files (.ixx, .cppm), ensure they are properly added to the .vcxproj file with appropriate compile settings. 
- For module implementation files (.cppm), always specify `<CompileAs Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">CompileAsCpp</CompileAs>` (or equivalent for other configurations) in the project file. 
- Verify module dependencies are correctly set up in the build system.
- モジュール＆ソースを生成するときはコンパイルオプションに気をつけてほしい。プロジェクトファイルへの追加と適切な CompileAs 設定を忘れずに。

## Code Style
- Follow specific formatting rules.
- Adhere to naming conventions.