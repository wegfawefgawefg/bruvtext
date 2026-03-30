# Library Spec

## Purpose

`bruvtext` is a small vendorable Vulkan text library.

It should be usable by projects that want to:

- clone a repo into their source tree
- build it directly
- update it with `git pull`
- avoid inventing their own text stack from scratch

The library is for:

- HUD text
- labels
- debug text
- menus
- dialogue
- multilingual game UI

It is not meant to be a full retained UI framework.

## Non-Goals

- full HTML/CSS-like layout
- rich text editing on day one
- every Unicode feature in the first milestone
- immediate support for every script on Earth

## Core Technical Direction

The library should use:

- `FreeType` for rasterization
- `HarfBuzz` for shaping and glyph positioning
- Vulkan quads and texture atlases for drawing

This is the important split:

- `FreeType` tells us how to rasterize glyphs
- `HarfBuzz` tells us which glyphs to draw and where to place them
- the renderer caches those glyphs and draws them efficiently

## Why This Direction

We do not want:

- whole-string CPU rasterization every frame
- per-language handwritten glyph placement rules
- ASCII-only toy text

We do want:

- real ligatures
- kerning
- shaping
- multilingual support
- reusable GPU atlas caches

## Intended Public API Shape

The public API should stay small and explicit.

Example direction:

```cpp
BruvTextContext* bruvtext_create(const BruvTextCreateInfo* info);
void bruvtext_destroy(BruvTextContext* ctx);

BruvTextFontId bruvtext_register_font(BruvTextContext* ctx, const BruvTextFontDesc* desc);
BruvTextTextStyleId bruvtext_register_style(BruvTextContext* ctx, const BruvTextStyleDesc* desc);

void bruvtext_begin_frame(BruvTextContext* ctx, const BruvTextFrameInfo* info);
void bruvtext_draw_text(BruvTextContext* ctx, const BruvTextDrawCmd* cmd);
void bruvtext_end_frame(BruvTextContext* ctx);
```

Possible lower-level API:

```cpp
bool bruvtext_shape_run(BruvTextContext* ctx, const BruvTextShapeRequest* req, BruvTextShapedRun* out);
bool bruvtext_build_draw_data(BruvTextContext* ctx, const BruvTextShapedRun* run, BruvTextDrawData* out);
```

The high-level API should be enough for normal engine use.
The lower-level API is useful for debugging and custom engine integration.

The important usability goal is:

- a normal user of the library should mostly interact with `draw_text(...)`
- they should not need to manually shape runs or manually manage atlases just to render HUD text
- the lower-level APIs exist so engine authors can integrate the library cleanly

## Internal Modules

Keep the structure flat and boring.

Suggested modules:

- `context`
- `font_registry`
- `font_fallback`
- `shape`
- `glyph_cache`
- `atlas_cache`
- `layout`
- `draw_data`
- `demo`

## Font Strategy

The library should support:

- one primary UI font
- a fallback chain
- per-size atlas pages

Likely font setup for the demo:

- Latin-capable font
- CJK-capable font
- Arabic-capable font
- Devanagari-capable font

In practice this will probably mean Noto-family fonts or a similar broad-coverage set.

## Atlas Strategy

Atlas generation should be lazy.

That means:

- atlases are created on demand per font and size
- glyphs are inserted on cache miss
- old atlas pages stay resident until explicitly evicted or the cache is rebuilt

This should not rerender whole strings.

It should only rasterize:

- new glyphs
- new font sizes
- new fallback fonts

### Atlas Size Tolerance

The library should support atlas size tolerance as a cache policy.

The intended model is:

- every draw command asks for a requested text size
- shaping and layout use that requested size directly
- atlas selection may snap that requested size to a nearby shared atlas bucket
- draw quads then sample from the chosen atlas bucket

This means tolerance is allowed to influence:

- how many atlas sizes are created
- how much atlas memory is used
- whether a cached glyph bitmap is an exact-size match or a reused nearby match

This means tolerance is not allowed to influence:

- shaping decisions
- glyph advances
- line wrapping logic
- paragraph layout semantics
- which size the caller thinks they requested

In short:

- requested size controls layout
- atlas bucket size controls glyph bitmap reuse

If changing tolerance causes obvious layout drift, that is a bug.

The expected user-visible tradeoff is:

- low tolerance:
  - more atlas buckets
  - more memory
  - more exact raster sizes
- high tolerance:
  - fewer atlas buckets
  - less memory
  - more bitmap reuse and therefore potentially softer or less perfectly aligned glyph imagery

The important subtlety is:

- higher tolerance may slightly affect bitmap appearance
- but it should not make text reflow into a different layout just because the cache policy changed

## Text Layout Scope

The first serious milestone should support:

- single-line shaped text
- multi-line wrapped text
- alignment
- scale and size selection
- kerning
- ligatures
- fallback fonts

Later milestones can add:

- bidi-aware layout
- richer paragraph shaping
- selection/cursor support
- text editing widgets

## Language Coverage

The first milestone target set should be:

- English
- French
- Spanish

That is enough to validate:

- proportional Latin layout
- accents
- punctuation
- atlas generation
- renderer integration

The planned expansion set after that is:

- Vietnamese
- Japanese
- Simplified Chinese
- Traditional Chinese
- Korean
- Arabic
- Hindi

That later set is intentionally broad enough to force:

- heavier Latin accents
- large CJK glyph sets
- RTL text
- shaped Indic text

## Demo Spec

The demo should prove the following:

1. text shapes correctly
2. glyph positions are correct
3. fallback fonts engage correctly
4. lazy atlas growth works
5. multiple sizes work
6. wrapped paragraphs look sane
7. the Vulkan output matches expected text

The demo should have:

- language selector
- font selector or font-chain selector
- size selector
- atlas page viewer
- glyph cache stats
- paragraph wrapping samples
- stress strings for punctuation and numbers

## Per-Language Demo Content

Each first-milestone language should have:

- title text
- short UI sample
- paragraph sample
- punctuation sample
- numeral sample
- stress sample

Examples of first-milestone stress cases:

- French apostrophes and accents
- Spanish punctuation and accents

Examples of later expansion stress cases:

- Vietnamese tone marks
- Japanese kana + kanji + punctuation
- Simplified and Traditional Chinese paragraphs
- Korean Hangul blocks
- Arabic connected script
- Hindi conjuncts and vowel marks

## Validation Requirements

The library should make it easy to inspect:

- atlas contents
- glyph positions
- chosen fallback font per glyph
- cache misses
- shape output

Useful debug views:

- atlas page panel
- glyph bounding boxes
- baseline display
- cluster display
- fallback font display

## Performance Direction

The intended cost model is:

- CPU:
  - shaping runs
  - cache miss rasterization
  - draw-data generation
- GPU:
  - atlas sampling
  - text quads

Avoid:

- rerendering full strings every frame
- rebuilding whole atlases constantly
- giant hidden text subsystems

## Expected Repository Layout

Keep it simple:

- `README.md`
- `docs/`
- `include/`
- `src/`
- `demo/`
- `third_party/` only if vendoring is actually needed

The repo should stay easy to vendor and easy to inspect.
