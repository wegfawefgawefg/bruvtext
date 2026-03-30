# Bug Report: Atlas Tolerance Causes Layout Drift Instead Of Visible Bitmap Degradation

## Summary

`bruvtext` currently behaves incorrectly when atlas size tolerance is increased.

The intended behavior is:

- requested text size should continue to control shaping, spacing, wrapping, and placement
- atlas tolerance should only affect glyph bitmap reuse
- higher tolerance should reduce atlas bucket growth and eventually make glyphs look softer or lower fidelity

The observed behavior is:

- as atlas tolerance increases, glyphs become increasingly mispositioned and mis-sized
- spacing drifts and sidebearings look wrong
- glyphs do **not** visibly become low resolution in the way you would expect from bitmap reuse

So the current bug is:

- tolerance is still leaking into layout or quad placement semantics
- but not producing the straightforward “same layout, fuzzier bitmap” tradeoff that the feature is supposed to demonstrate

## Repro

Use the current showcase demo.

Steps:

1. launch `bruvtext-demo`
2. pick a Latin language like French or Polish
3. press `C` to clear the active font cache
4. use `=` to raise feature scale
5. use `]` to raise atlas tolerance
6. continue adjusting scale and tolerance while watching:
   - feature title
   - wrapped paragraph lines
   - active font pages / size buckets / memory

## Expected Behavior

At tolerance `0`:

- every new requested size should generate a new exact-size atlas bucket
- active font size-bucket count should grow as new sizes are requested
- active font memory/page usage should grow accordingly
- layout should remain correct

At higher tolerance:

- nearby requested sizes should collapse into shared atlas buckets
- active font size-bucket growth should slow down
- active font memory growth should slow down
- layout should still remain correct
- glyphs should eventually look softer, blurrier, or more obviously resampled

## Actual Behavior

At higher tolerance:

- glyphs drift in spacing and perceived size
- letters look misaligned relative to one another
- paragraphs look optically wrong
- text does **not** clearly degrade as a reused low-resolution bitmap
- the resulting failure mode looks like broken placement rather than expected texture reuse

## Why This Is Wrong

The intended contract for atlas tolerance is:

- requested size determines layout
- atlas bucket size determines cached bitmap source

That means tolerance is allowed to affect:

- cache hit rate
- bucket count
- page count
- memory use
- bitmap sharpness

It is **not** allowed to affect:

- shaping output
- glyph advances
- line wrapping semantics
- placement logic

The current behavior strongly suggests the implementation still mixes:

- requested-size geometry metrics
- atlas-size bitmap metrics

in a way that leaks into final placement or visible glyph extents.

## Current Implementation Notes

The current pipeline has already been partially split:

- shaping uses requested size
- atlas lookup uses a resolved atlas pixel size
- draw quads were changed to use requested-size glyph metrics

Files most relevant:

- [shaping.cpp](/home/vega/Coding/Graphics/bruvtext/src/shaping.cpp)
- [draw_list.cpp](/home/vega/Coding/Graphics/bruvtext/src/draw_list.cpp)
- [atlas_cache.cpp](/home/vega/Coding/Graphics/bruvtext/src/atlas_cache.cpp)
- [frame_state.h](/home/vega/Coding/Graphics/bruvtext/src/frame_state.h)
- [demo/main.cpp](/home/vega/Coding/Graphics/bruvtext/demo/main.cpp)

Despite those changes, the bug remains.

## Strong Suspicion

The most likely problem area is still the mismatch between:

- requested-size glyph geometry
- atlas-size bitmap extents

Possible fault lines:

- quad placement still implicitly assumes atlas bitmap dimensions match requested-size raster dimensions
- bitmap sampling into the quad may effectively change perceived sidebearings even if pen advances are correct
- hinted raster differences between atlas size and requested size may require a stricter strategy than simple bitmap reuse
- the current tolerance bucketing policy may be too aggressive to preserve usable optical alignment with hinted glyphs

## Questions To Investigate

1. Should atlas tolerance ever reuse hinted bitmap glyphs across significantly different ppem sizes?
2. Is the correct behavior for this feature actually:
   - reuse only very near sizes
   - or rasterize unhinted glyphs for tolerance buckets?
3. Should glyph geometry use requested-size metrics from FreeType outline load, not requested-size rendered bitmap metrics?
4. Is the current demo proving a limitation of hinted bitmap reuse rather than just a straightforward bug?

## Practical Next Steps

Recommended investigation order:

1. instrument one glyph at two requested sizes and compare:
   - HarfBuzz advances
   - requested-size FreeType bearings/bitmap size
   - atlas-size cached bearings/bitmap size
   - final quad rects
2. test with hinting reduced or disabled on the cached bitmap path
3. test with tighter tolerance buckets only
4. verify whether the issue is:
   - true layout drift
   - or optical distortion from scaling hinted bitmaps into requested-size quads

## Current Conclusion

The feature is not behaving according to its intended contract.

The current visible result is:

- wrong-looking spacing
- wrong-looking glyph sizing
- little obvious low-resolution degradation

So atlas tolerance should be treated as **buggy / unresolved** right now.
