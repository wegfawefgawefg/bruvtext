# Vulkan Integration

## Purpose

This document describes the intended Vulkan ownership split for `bruvtext`.

The goal is:

- low opinion about Vulkan resource ownership
- simple text API for normal app code
- a reference renderer path that people can copy

## Current Contract

`bruvtext` currently provides:

- font registration
- shaping
- measurement
- atlas page data
- dirty atlas flags
- draw glyphs
- draw batches

This is already enough to drive a Vulkan renderer.

## High-Level Text API

Normal app code should feel simple.

Current public shape:

```cpp
bruvtext::BeginFrame(*text);

bruvtext::DrawText(
    *text,
    font,
    "Hello",
    32.0f,
    48.0f,
    22.0f,
    {1.0f, 1.0f, 1.0f, 1.0f});

bruvtext::DrawTextEx(
    *text,
    {
        .font = font,
        .text = "Scaled",
        .position = {32.0f, 84.0f},
        .pixelSize = 18.0f,
        .scale = 1.5f,
        .color = {1.0f, 0.9f, 0.8f, 1.0f},
    });

bruvtext::EndFrame(*text);
```

This layer intentionally hides:

- shaping
- glyph atlas lookups
- cache misses
- atlas page churn

## Renderer-Facing API

After `EndFrame(...)`, the host renderer consumes:

- `GetAtlasPageCount(...)`
- `GetAtlasPages(...)`
- `GetDrawGlyphCount(...)`
- `GetDrawGlyphs(...)`
- `GetDrawBatchCount(...)`
- `GetDrawBatches(...)`

The current draw data shape is:

- atlas pixel rect per glyph
- screen-space destination rect per glyph
- atlas page index
- batch ranges grouped by atlas page

That is enough for a straightforward Vulkan text pass.

## Ownership Split

`bruvtext` owns:

- font loading
- HarfBuzz shaping
- FreeType rasterization
- glyph cache bookkeeping
- atlas packing
- text draw-list generation

The host renderer owns:

- Vulkan instance, device, queues
- images and image views
- staging buffers
- descriptors
- pipeline state
- command recording and submission

This keeps `bruvtext` vendor-friendly across different engine layouts.

## Reference Vulkan Renderer

The current demo is also the current reference Vulkan integration.

Reference files:

- [demo/vulkan_demo.h](/home/vega/Coding/Graphics/bruvtext/demo/vulkan_demo.h)
- [demo/vulkan_demo.cpp](/home/vega/Coding/Graphics/bruvtext/demo/vulkan_demo.cpp)
- [demo/shaders/text.vert](/home/vega/Coding/Graphics/bruvtext/demo/shaders/text.vert)
- [demo/shaders/text.frag](/home/vega/Coding/Graphics/bruvtext/demo/shaders/text.frag)

This renderer currently shows the intended minimal GPU path:

1. create one GPU texture per atlas page
2. upload only dirty atlas pages
3. build quad vertices from draw glyphs each frame
4. draw batches grouped by atlas page

That is the implementation other Vulkan projects should copy first.

## 3D And World-Space Use

The current draw glyph output is screen-space.

That does not prevent other render styles.

Host projects can reinterpret the draw data into:

- billboards in 3D
- text on world quads
- document/page render passes
- text rendered into offscreen textures

The important point is that `bruvtext` should not hardcode one camera model or
one UI framework assumption into the library core.

## Current Cache Policy

The current cache policy is intentionally simple:

- exact-size buckets per `font + raster size`
- lazy glyph insertion on cache miss
- bounded number of size buckets per font
- oldest-bucket eviction when the cap is exceeded

This is simpler than the earlier atlas-tolerance experiment and matches the
current public API better.

## Future Optional Helper Layer

If we want a more packaged integration later, the likely shape is:

- keep the current core API renderer-facing
- keep the demo renderer as the reference path
- optionally split out a small helper layer later, something like:
  - `bruvtext_vulkan_reference`

But that helper should stay optional.

The core library should not take ownership of host Vulkan policy.
