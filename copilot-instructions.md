
# Copilot Instructions (local)

## vcxproj.filters Editing Guidelines

- IMPORTANT: When editing Visual Studio filter files (.vcxproj.filters), you MUST add filter definitions BEFORE using them in file entries.
- Filter definitions must be added in the `<ItemGroup>` section with `<Filter Include="path">` elements and a `<UniqueIdentifier>{guid}</UniqueIdentifier>` for each filter.
- Example:
  ```xml
  <ItemGroup>
    <Filter Include="src\Widgets\NewSubFolder">
      <UniqueIdentifier>{12345678-1234-1234-1234-123456789012}</UniqueIdentifier>
    </Filter>
  </ItemGroup>
  ```
- After adding filter definitions, you can then use them in `<ClCompile>` entries:
  ```xml
  <ClCompile Include="src\Widgets\NewSubFolder\MyFile.cppm">
    <Filter>src\Widgets\NewSubFolder</Filter>
  </ClCompile>
  ```
- NEVER use a filter in a file entry without first adding its definition - this will corrupt the file and make it unreadable in Visual Studio.
- When moving files to subdirectory filters, always ensure the corresponding filter definition exists.

## Qt Signals & Slots (Verdigris Requirement)

- IMPORTANT: This codebase requires using Verdigris macros for Qt signals and slots. Do NOT use Qt's `signals:` section or `Q_SIGNAL` for declarations in exported classes.
- Always declare `W_OBJECT` in class declarations and use `W_OBJECT_IMPL` (or `W_OBJECT_IMPL(ClassName)`) in implementation files.
- Use Verdigris `W_SIGNAL(...)` to declare signals and `W_SLOT(...)` to declare slots. Examples:
  - `void frameChanged(const FramePosition& frame) W_SIGNAL(frameChanged, frame);`
  - `void onActionTriggered() W_SLOT(onActionTriggered);`
- Emitting signals may use `Q_EMIT`, but declarations must use Verdigris macros so the Verdigris meta-object generator works correctly.

This requirement is mandatory for correct meta-object generation and must be followed by all contributors and automated tools.

## PIMPL Idiom and Memory Management Guidelines

- IMPORTANT: When implementing the PIMPL (Pointer to Implementation) idiom, **DO NOT** use modern smart pointers (`std::shared_ptr` or `std::unique_ptr`) to manage the `impl_` pointer if the class is part of a public API or could potentially cross DLL boundaries.
- Usage of standard library templates in DLL interfaces can cause ABI incompatibility issues, memory corruption across different heaps, and crashes.
- Always use **raw pointers** (`Impl* impl_;`) for the PIMPL idiom in these contexts.
- To prevent memory leaks, double-free crashes, and shallow-copy bugs, you **MUST** explicitly implement the "Rule of 5" (or Rule of 3) for any class using a raw `impl_` pointer:
  1. Destructor (to `delete impl_`)
  2. Copy Constructor (to perform a deep copy: `impl_(new Impl(*other.impl_))`)
  3. Copy Assignment Operator (deep copy)
  4. Move Constructor (take ownership, nullify the other)
  5. Move Assignment Operator
- Be extremely careful when passing PIMPL objects by value through Qt Signals/Slots, as a missing deep copy will result in a double-free crash when the event loop cleans up the parameter copies.
