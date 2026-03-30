# GPU Renderer Gap

## Summary

`bruvtext` already has the right text-processing foundation:

- `HarfBuzz` shaping
- `FreeType` glyph rasterization
- atlas caching
- draw-glyph generation

But the current demo renderer is still not a real GPU text renderer.

Right now the showcase path does this:

1. shape text and build draw glyphs
2. CPU-rasterize the final text into a full-screen framebuffer
3. upload that full framebuffer to Vulkan every frame
4. copy it into the swapchain image

That means the current demo is still effectively:

- a software text compositor
- wrapped in Vulkan presentation

This is the biggest remaining gap between:

- the intended architecture
- and what the demo actually proves

## Why This Matters

The whole point of `bruvtext` is not just:

- shaping text correctly
- loading multilingual fonts correctly

It is also to provide a real GPU text rendering path for Vulkan projects.

Until we do that, the demo has major limitations:

- very high CPU usage
- full-screen CPU framebuffer clears every frame
- per-pixel alpha blending on the CPU
- full framebuffer upload every frame
- poor representation of the real runtime cost of the library

## Current CPU-Heavy Path

The hot path is in:

- [demo/vulkan_demo.cpp](/home/vega/Coding/Graphics/bruvtext/demo/vulkan_demo.cpp)

Specifically:

- `RasterizeTextToCpuFramebuffer(...)`
- `UploadCpuFramebuffer(...)`

What it does:

- clears the full framebuffer on CPU
- walks every draw glyph
- samples the atlas page pixels on CPU
- alpha-blends into the CPU framebuffer
- copies the full framebuffer into a Vulkan upload buffer with `memcpy`
- copies that buffer into the swapchain image

So the current demo is a correctness scaffold, not a performance proof.

## Intended GPU Path

The intended renderer path is much simpler:

1. shape text into positioned glyphs
2. look up glyphs in atlas pages
3. rasterize only cache misses into atlas pages
4. upload dirty atlas pages only when they change
5. render text as textured quads on the GPU

That means:

- no full-screen CPU framebuffer
- no CPU blending of every glyph pixel
- no full-frame upload every frame
- only atlas uploads on cache misses or atlas updates
- per-frame work becomes mostly:
  - queue text
  - shape text
  - upload small vertex/instance data
  - draw quads

## What The Real Vulkan Renderer Should Do

The first real GPU milestone should:

- create persistent Vulkan images for atlas pages
- upload only dirty atlas pages
- build a text quad vertex or instance buffer each frame
- render batches grouped by atlas page
- alpha blend in a normal text pipeline
- keep swapchain rendering GPU-native end to end

The library already provides the important inputs for this:

- atlas pages
- dirty flags
- draw glyphs
- draw batches

So the renderer milestone is mostly about consuming those outputs correctly.

## Minimum Viable GPU Renderer Milestone

The minimum serious version should:

1. keep one Vulkan image per atlas page
2. create a small text pipeline
3. upload only dirty atlas pages
4. build a dynamic vertex buffer of quads each frame
5. draw one batch per atlas page

That is enough to make the demo a real GPU text renderer.

It does **not** need on day one:

- fancy SDF/MSDF text
- subpixel AA
- instancing tricks
- advanced material systems
- a frame graph

It just needs:

- atlas textures
- quad vertices
- alpha-blended text draw calls

## Why We Did Not Start There

The CPU path was useful as a bootstrap because it let us validate:

- multilingual shaping
- atlas population
- draw-glyph generation
- demo layout

without having to build the Vulkan text pipeline first.

That was acceptable as a scaffold.

But it should now be treated as temporary.

## Practical Next Step

The next real renderer task should be:

- replace CPU framebuffer compositing in `demo/vulkan_demo.cpp`
- with a real Vulkan atlas + quad draw path

Once that is done, the library will finally demonstrate:

- correct shaping
- real atlas caching
- real GPU rendering

instead of just the first two.

## Bottom Line

`bruvtext` is not finished until the demo stops being a CPU text compositor.

The shaping/cache side is already useful.
The renderer side is still the missing piece that makes the library actually live up to its purpose.
