# bruvtext

Boring Rudimentary Vulkan Text.

`bruvtext` is a vendorable text rendering library for Vulkan projects. It gives
you:

- `FreeType + HarfBuzz` shaping and rasterization
- GPU-rendered text from cached glyph atlases
- a simple public API for draw and measure
- a reference Vulkan renderer you can copy or adapt

It does **not** try to be a UI framework.

This `docs/` folder is for repo-local technical notes:

- integration details
- renderer notes
- performance notes
- remaining scope and future work

The public-facing docs site lives under `pages/`.

## What You Get

- a normal `DrawText(...)` / `DrawTextEx(...)` / `MeasureText(...)` style API
- multilingual demo coverage
- exact-size glyph atlases with lazy cache growth
- reference shaders and a working Vulkan demo renderer

## Fast Start

1. Vendor `bruvtext` into your repo.
2. Add it to your build.
3. Create one `bruvtext::Context`.
4. Register your own fonts.
5. Each frame:
   - `BeginFrame(...)`
   - `DrawText(...)` / `DrawTextEx(...)`
   - `EndFrame(...)`
6. In your renderer:
   - upload dirty atlas pages
   - draw glyph batches

## Important Packaging Note

This repo includes demo fonts and demo text assets so the bundled showcase and
benchmark work out of the box.

Those assets are:

- for the demo
- not required by the core library
- not required by projects that vendor `bruvtext`

Real projects are expected to register and ship their own fonts.

## Main Docs

- [Integration Quickstart](integration-quickstart.md)
- [Vendoring Guide](vendoring.md)
- [Vulkan Integration](vulkan-integration.md)
- [Reference Renderer](reference-renderer.md)
- [Library Spec](library-spec.md)
- [Performance Notes](performance-notes.md)
- [Benchmark Plan](benchmark-plan.md)
- [V1 Remaining Scope](v1-remaining-scope.md)
