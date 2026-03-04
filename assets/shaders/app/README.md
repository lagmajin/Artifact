# App shaders

Place application HLSL shader source files here.

Conventions:
- Vertex input semantics must match the `HLSLSemantic` values you set on `Diligent::LayoutElement` (e.g. "POSITION", "ATTRIB", "TEXCOORD").
- Constant buffer names used in code (e.g. `TransformCB`) must match the HLSL `cbuffer` names.
- Keep shaders simple and separate by stage (e.g. `xxx_vs.hlsl`, `xxx_ps.hlsl`).

Example files are provided in this directory: `line_vs.hlsl`, `solidcolor_ps.hlsl`.
