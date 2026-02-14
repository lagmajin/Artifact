
# Copilot Instructions (local)

## Qt Signals & Slots (Verdigris Requirement)

- IMPORTANT: This codebase requires using Verdigris macros for Qt signals and slots. Do NOT use Qt's `signals:` section or `Q_SIGNAL` for declarations in exported classes.
- Always declare `W_OBJECT` in class declarations and use `W_OBJECT_IMPL` (or `W_OBJECT_IMPL(ClassName)`) in implementation files.
- Use Verdigris `W_SIGNAL(...)` to declare signals and `W_SLOT(...)` to declare slots. Examples:
  - `void frameChanged(const FramePosition& frame) W_SIGNAL(frameChanged, frame);`
  - `void onActionTriggered() W_SLOT(onActionTriggered);`
- Emitting signals may use `Q_EMIT`, but declarations must use Verdigris macros so the Verdigris meta-object generator works correctly.

This requirement is mandatory for correct meta-object generation and must be followed by all contributors and automated tools.

