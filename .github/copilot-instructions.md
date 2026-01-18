# Copilot Instructions

## General Guidelines
- Use C++20 modules (e.g., `import std`) instead of traditional header includes (e.g., `#include`) whenever possible. Prioritize module imports to enhance code organization and efficiency.
- In Qt projects, prefer using Qt containers (e.g., `QVector`, `QList`) over standard containers like `std::vector` to maintain consistency and leverage Qt's features.
- When using Qt types in C++ module (.cpp, .cppm) files, always add the necessary Qt includes immediately after the `module;` declaration. For example, explicitly include `#include <QString>`, `#include <QVector>`, `#include <QColor>`, etc. This ensures that independent includes are present in the implementation file rather than relying on the module's external interface (.ixx).
- C++20 モジュールコードを書くときはちゃんとグローバルモジュールフラグメントも書いてほしい。

## Code Style
- Follow specific formatting rules.
- Adhere to naming conventions.