# V1 Remaining Scope

This is the narrowed remaining scope for `bruvtext` v1.

The goal is not to turn the library into a UI framework. The goal is to ship a
small vendorable Vulkan text library with a normal public API, solid demo
coverage, and boring renderer integration.

## Current State

Already in place:

- `FreeType + HarfBuzz` shaping and rasterization
- multilingual demo coverage
- real Vulkan text rendering in the demo
- vendorable repo structure
- cleaned git history and ignore rules

Not yet considered done:

- public API still needs a final close-out pass
- cache behavior needs a simple final policy
- packaging and integration docs need a practical pass
- tests are light

## Remaining V1 Work

1. Public API close-out
2. Simple cache policy
3. Packaging and integration polish
4. Minimal smoke tests and regression checks
5. Docs pass for actual usage

## Explicitly Out Of Scope For V1

These are not blockers for the first usable release:

- full paragraph layout engine
- CSS-like layout or styling
- rich text editing
- automatic font fallback chains
- advanced alignment helpers beyond measurement
- public atlas tuning knobs like the old tolerance experiment

## Notes On Scope Decisions

### 1. Public API close-out

This is real v1 work.

The public surface should feel normal to users of libraries like raylib and
SDL_ttf:

- register a font
- draw text at a position
- measure text
- optionally use a more explicit `DrawTextEx` form

The public API should hide internal cache policy details.

### 2. Line layout API

This is not required for v1.

The library should expose measurement so callers can do their own spacing,
alignment, and wrapping. The demo can keep lightweight sample wrapping code.

### 3. Cache policy

This should stay simple.

We do not need a heroic cache system for v1. We do need something predictable
and safe, such as:

- bounded atlas pages per font
- clear-on-demand
- or simple oldest-bucket eviction

The important part is that the library should not crash or leak when the caller
uses many raster sizes.

### 4. Font fallback

This is out of scope for v1 as a library feature.

If a caller chooses a font that does not contain the glyphs they need, that is
their responsibility for now. The demo can keep using explicit per-language
font choices.

### 5. Tests

Useful but not launch-blocking.

At minimum we want:

- buildable demo
- shaping smoke checks
- measurement sanity checks
- multilingual showcase regression coverage

### 6. Packaging and integration polish

This is important.

The whole point of `bruvtext` is that another Vulkan project can vendor it and
use it without inventing a text renderer from scratch.

### 7. Alignment and layout helpers

Optional.

`MeasureText(...)` and line advance measurement are enough for v1. Callers can
do their own centering, anchoring, and line placement in their own code.
