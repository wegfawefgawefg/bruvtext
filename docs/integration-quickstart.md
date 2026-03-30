# Integration Quickstart

This is the intended boring integration flow for `bruvtext`.

The important split is:

- gameplay or UI code uses the simple text API
- the renderer consumes atlas pages and draw batches

## 1. Vendor And Build

Add `bruvtext` to your project and build it with CMake:

```cmake
add_subdirectory(external/bruvtext)
target_link_libraries(my_app PRIVATE bruvtext)
```

`bruvtext` itself depends on:

- `FreeType`
- `HarfBuzz`

The demo additionally depends on:

- `SDL3`
- `Vulkan`
- `glslangValidator`

## 2. Create A Context

Create one text context for your app:

```cpp
bruvtext::CreateInfo createInfo{};
createInfo.assetsRoot = "external/bruvtext/assets";

bruvtext::Context* text = bruvtext::CreateContext(createInfo);
```

## 3. Register Fonts

Register fonts explicitly.

For v1, callers are expected to choose fonts that actually cover the languages
they want to render.

```cpp
bruvtext::FontId uiSans = bruvtext::RegisterFont(
    *text,
    {
        .debugName = "DejaVu Sans",
        .filePath = "assets/fonts/DejaVuSans.ttf",
    });
```

## 4. Queue Text Each Frame

Normal app code should mostly touch:

- `BeginFrame(...)`
- `DrawText(...)`
- `DrawTextEx(...)`
- `MeasureText(...)`
- `MeasureLineAdvance(...)`
- `EndFrame(...)`

Example:

```cpp
bruvtext::BeginFrame(*text);

bruvtext::DrawText(
    *text,
    uiSans,
    "Hello world",
    32.0f,
    48.0f,
    22.0f,
    {1.0f, 1.0f, 1.0f, 1.0f});

bruvtext::DrawTextEx(
    *text,
    {
        .font = uiSans,
        .text = "Scaled text",
        .position = {32.0f, 84.0f},
        .pixelSize = 18.0f,
        .scale = 1.5f,
        .color = {0.9f, 0.8f, 0.7f, 1.0f},
    });

bruvtext::EndFrame(*text);
```

## 5. Consume The Renderer Data

After `EndFrame(...)`, the renderer should consume:

- atlas pages:
  - `GetAtlasPageCount(...)`
  - `GetAtlasPages(...)`
- draw glyphs:
  - `GetDrawGlyphCount(...)`
  - `GetDrawGlyphs(...)`
- draw batches:
  - `GetDrawBatchCount(...)`
  - `GetDrawBatches(...)`

The intended flow is:

1. upload dirty atlas pages to GPU textures
2. build or upload quad data for the current draw glyph list
3. draw batches grouped by atlas page
4. clear atlas dirty flags after successful upload

## 6. What The Host Owns

The host renderer owns:

- Vulkan device
- image allocation
- staging buffer allocation
- descriptor sets
- pipeline state
- command recording

`bruvtext` owns:

- font loading
- shaping
- glyph metrics
- atlas bookkeeping
- draw-glyph generation

## 7. Reference Renderer

The current repo already ships a reference Vulkan integration in the demo:

- [demo/vulkan_demo.h](/home/vega/Coding/Graphics/bruvtext/demo/vulkan_demo.h)
- [demo/vulkan_demo.cpp](/home/vega/Coding/Graphics/bruvtext/demo/vulkan_demo.cpp)
- [demo/shaders/text.vert](/home/vega/Coding/Graphics/bruvtext/demo/shaders/text.vert)
- [demo/shaders/text.frag](/home/vega/Coding/Graphics/bruvtext/demo/shaders/text.frag)

That is the current “copy this and adapt it” path.

The demo is not meant to be the only renderer shape forever, but it is the
current reference implementation for:

- atlas uploads
- text quad generation
- atlas-page batching
- alpha-blended text drawing
