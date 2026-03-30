# Performance Notes

This document captures the current performance position of `bruvtext`, what the
benchmark is actually measuring, and what conclusions are reasonable today.

## Current Takeaway

`bruvtext` is in a usable place for normal game and app text workloads, but it
is not "free" yet for extreme high-FPS, high-volume dynamic text.

The important current result is:

- GPU submission is no longer the obvious bottleneck
- CPU-side text preparation is still where most of the cost lives

That is a good sign. It means the basic Vulkan renderer path is not obviously
broken or absurdly inefficient anymore.

## Benchmark Reality

The benchmark mode in the demo intentionally stresses the system.

It is not a perfect representation of a normal game scene because it can:

- queue hundreds of text runs every frame
- use many languages in one frame
- push several font atlases at once
- cause a large amount of alpha overdraw

That matters when interpreting the numbers.

### What the benchmark is good for

- catching obviously bad renderer behavior
- checking batch counts
- checking atlas growth and churn
- checking whether CPU preparation is exploding
- checking whether the Vulkan path is doing something stupid

### What the benchmark is not

- a perfect simulation of a normal shipped game HUD
- a perfect simulation of a code editor
- a perfect simulation of a one-language menu screen

## Current Practical Position

The demo measurements so far suggest:

- under roughly `3k` glyphs in a normal single-language scenario, performance is
  in a workable range
- most real games will have one active language at a time, not all demo
  languages mixed together
- most real games will use a small number of fonts, usually around `2-3`
- the current demo benchmark is often harsher than normal game usage

So the reasonable current claim is:

- `bruvtext` looks viable for ordinary game UI, HUD, dialogue, labels, and
  debug text
- `bruvtext` is not yet optimized for "thousands of dynamically changing glyphs
  at very high framerates"

## Important Recent Result

After the current renderer and shaped-run cache fixes, benchmark logging shows a
useful split:

- submitted batch count can stay low
- GPU/frame time can stay low
- CPU prepare time can still dominate

That means:

- batching and Vulkan submission are no longer the first thing to blame
- HarfBuzz shaping, draw-data rebuild, and general CPU-side frame prep are the
  main remaining cost centers

## Why Real Games May Look Better Than The Demo

The demo benchmark is intentionally pessimistic in several ways:

- it can mix many scripts in one frame
- it can push more atlas pages than a normal game scene
- it can keep a large amount of text active at once
- it can requeue and redraw a very large amount of text every frame

A more ordinary game or tool often has:

- one language active at a time
- mostly static UI text
- a small number of changing labels or counters
- a very small number of fonts

That is a friendlier workload than the current stress mode.

## Remaining Cheap Optimization Headroom

Without changing the simple public API, the main remaining performance options
are:

1. Cache shaped runs for unchanged text
2. Cache measurement results for unchanged text
3. Reduce per-frame CPU rebuild work for static text blocks
4. Improve vertex upload behavior further
5. Keep atlas churn and eviction boring and predictable

The first item is already in progress and is the right kind of optimization for
this library:

- it does not make the API more opinionated
- it does not require retained UI objects
- it keeps the raylib-style call surface intact

## Current Recommendation

Treat `bruvtext` as:

- good enough to keep pushing toward v1
- good enough for realistic single-language game UI usage
- not yet "done" for extreme dynamic-text performance claims

That is a healthy place for the project to be.
