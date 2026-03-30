# Vulkan Integration

## Purpose

This document describes how `bruvtext` should integrate into Vulkan projects
without forcing one allocator, one descriptor model, or one renderer architecture.

The main design goal is:

- low opinion about Vulkan ownership
- high ease of use at the text API layer

That means users should be able to write:

```cpp
bruvtext_draw_text(ctx, &cmd);
```

without manually dealing with shaping or glyph caches every frame.

At the same time, host engines should stay in control of:

- image allocation
- buffer allocation
- descriptor strategy
- command buffer submission

## Ownership Split

`bruvtext` should own:

- font loading
- shaping
- glyph metrics
- glyph cache bookkeeping
- atlas packing decisions
- CPU-side text draw list generation

The host renderer should own:

- Vulkan device and queues
- image allocation policy
- buffer allocation policy
- descriptor set policy
- pipeline policy
- command recording and submission

This keeps the library vendor-friendly across many engine layouts.

## Two-Layer API

The clean approach is to expose two layers.

### 1. High-Level API

This is what most users should touch:

```cpp
BruvTextFontId font = bruvtext_register_font(ctx, &fontDesc);

BruvTextDrawCmd cmd = {};
cmd.font = font;
cmd.x = 16.0f;
cmd.y = 16.0f;
cmd.size = 24.0f;
cmd.color = {1, 1, 1, 1};
cmd.text = "bonjour";

bruvtext_draw_text(ctx, &cmd);
```

This API should feel boring and easy.

It should hide:

- shaping
- kerning
- atlas lookups
- glyph cache misses

### 2. Renderer Integration API

This is what the engine author uses to feed Vulkan.

The library should expose:

- dirty atlas page uploads
- atlas metadata
- text draw batches
- vertex/index or instance data

Example direction:

```cpp
const BruvTextAtlasUpdate* updates = nullptr;
uint32_t updateCount = 0;
bruvtext_get_atlas_updates(ctx, &updates, &updateCount);

const BruvTextDrawBatch* batches = nullptr;
uint32_t batchCount = 0;
bruvtext_get_draw_batches(ctx, &batches, &batchCount);
```

The host then:

- uploads atlas page data into its own Vulkan images
- uploads draw data into its own buffers
- binds its own descriptors
- issues its own draw calls

## Why This Split Is Reasonable

If the library owns all Vulkan objects, it becomes too opinionated.

Different projects will want:

- different allocators
- different descriptor models
- different frame graph designs
- different transient buffer strategies

If the library owns nothing but shaping, it becomes annoying to use.

So the right middle ground is:

- simple `draw_text(...)` for normal use
- explicit atlas and draw-data outputs for renderer integration

## Reasonable Draw Data Shape

The simplest portable output is:

- textured quads grouped by atlas page

That can be represented as:

- vertices + indices
- or instances plus a shared quad mesh

A plausible first version:

```cpp
struct BruvTextVertex
{
    float x;
    float y;
    float u;
    float v;
    float r;
    float g;
    float b;
    float a;
};

struct BruvTextDrawBatch
{
    uint32_t atlasPage;
    uint32_t firstVertex;
    uint32_t vertexCount;
};
```

That is simple enough for almost any Vulkan project to consume.

## Atlas Upload Interface

The library should not force a specific image creation path.

Instead, atlas changes should be surfaced as:

- page id
- width/height
- pixel format
- raw pixel pointer
- dirty region if partial updates are supported

Example:

```cpp
struct BruvTextAtlasUpdate
{
    uint32_t pageId;
    uint32_t width;
    uint32_t height;
    const void* pixels;
};
```

The host project can then decide whether it wants:

- one image per atlas page
- array textures
- staging buffer uploads
- direct mapped upload paths

## Ease Of Use Goal

Even with the low-opinion Vulkan layer, the common path should still be easy.

The library should feel like:

1. create context
2. register font
3. begin frame
4. call `draw_text(...)`
5. ask for draw data and atlas updates
6. render them in Vulkan

That is a reasonable compromise between:

- “we own your whole renderer”
- and “you build a text system yourself from raw shaping results”

## Future Optional Helper Layer

It may still be useful to provide an optional helper module:

- `bruvtext_vulkan_helper`

This helper could show one reference integration for:

- atlas image creation
- staging uploads
- descriptor setup
- simple text pipeline setup

But it should stay optional.

The core library should not require host projects to adopt that exact Vulkan helper.

