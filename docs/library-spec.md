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

Current intended direction:

```cpp
bruvtext::Context* CreateContext(const bruvtext::CreateInfo& info);
void DestroyContext(bruvtext::Context* context);

bruvtext::FontId RegisterFont(bruvtext::Context& context, const bruvtext::FontDesc& desc);

void BeginFrame(bruvtext::Context& context);
bool DrawText(
    bruvtext::Context& context,
    bruvtext::FontId font,
    std::string_view text,
    float x,
    float y,
    float pixelSize,
    bruvtext::Color color = {});
bool DrawTextEx(bruvtext::Context& context, const bruvtext::DrawTextCmd& cmd);
bruvtext::TextSize MeasureText(
    bruvtext::Context& context,
    bruvtext::FontId font,
    std::string_view text,
    float pixelSize);
float MeasureLineAdvance(
    bruvtext::Context& context,
    bruvtext::FontId font,
    float pixelSize,
    float scale = 1.0f);
bool EndFrame(bruvtext::Context& context);
```

The important usability goal is:

- a normal user of the library should mostly interact with `DrawText(...)`
- they should not need to manually shape runs or manually manage atlases just to render HUD text
- lower-level inspection APIs can still exist for debugging and engine integration

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

For v1, the library should support:

- explicit font registration
- per-font, per-size atlas pages
- caller-chosen fonts for the languages they care about

The demo should continue to ship a practical set of fonts:

- Latin-capable font
- CJK-capable font
- Arabic-capable font
- Devanagari-capable font

Automatic fallback chains are a later feature, not a v1 requirement.

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

### Cache Policy

The cache policy should stay simple for v1.

We do not need a heroic cache system. We do need a predictable one.

Current intended v1 behavior:

- atlas pages are created lazily per font and exact raster size
- glyphs are inserted on cache miss
- there is no cross-size glyph reuse
- page growth is bounded
- each font gets a bounded number of raster-size buckets
- over-cap behavior evicts the oldest bucket for that font

What matters is:

- no leaks
- no crashes under size churn
- easy-to-understand behavior while debugging

### Optional Preload Path

The library should eventually support an explicit preload path, but this is not
the default caching model.

The intended split is:

- default: lazy glyph insertion on cache miss
- optional: caller-requested preload for a known glyph set

This is useful when the caller knows the text ahead of time and wants to avoid
first-use rasterization spikes.

Likely directions:

- preload from UTF-8 text
- preload from explicit codepoint arrays

This should be done for a specific:

- font
- raster size

The preload path must respect the same page and glyph limits as the lazy path.
That means very large preload sets, especially for CJK, may need to:

- stop early and report failure
- or evict older buckets according to the normal cache policy

The library should not silently switch to “bake all of Chinese up front” as a
default behavior.

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
- text measurement
- caller-controlled line placement
- scale and raster-size selection
- kerning
- ligatures

Later milestones can add:

- library-provided wrapping helpers
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
