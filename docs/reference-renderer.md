# Reference Renderer

`bruvtext` now ships with a real GPU reference renderer in the demo.

This matters because the library is not just:

- shaping text correctly
- loading fonts correctly

It is also supposed to show a practical Vulkan rendering path that other
projects can copy.

## Current Reference Files

The current reference implementation is:

- [demo/vulkan_demo.h](/home/vega/Coding/Graphics/bruvtext/demo/vulkan_demo.h)
- [demo/vulkan_demo.cpp](/home/vega/Coding/Graphics/bruvtext/demo/vulkan_demo.cpp)
- [demo/shaders/text.vert](/home/vega/Coding/Graphics/bruvtext/demo/shaders/text.vert)
- [demo/shaders/text.frag](/home/vega/Coding/Graphics/bruvtext/demo/shaders/text.frag)

The demo consumes the same public and renderer-facing outputs that external
projects are expected to consume:

- atlas pages
- dirty atlas flags
- draw glyphs
- draw batches

## What It Demonstrates

The current reference path demonstrates:

1. persistent GPU atlas page textures
2. dirty-page uploads
3. per-frame text quad generation
4. atlas-page batching
5. alpha-blended text rendering in Vulkan

That makes the demo more than a showcase. It is also the current integration
template.

## What It Is Not

The reference renderer is not meant to be:

- the only possible renderer shape
- a framework-owned Vulkan backend
- the final answer for every engine

Projects can still adapt the same core outputs into:

- screen-space UI
- world-space quads
- billboards
- offscreen document rendering

## Why We Keep It As A Reference

The demo renderer is useful because it gives vendoring users:

- working shaders
- working atlas uploads
- working batch submission
- a boring example to copy first

That is the right compromise between:

- “you must invent your own text renderer”
- and “the library owns your Vulkan architecture”
