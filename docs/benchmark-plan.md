# Benchmark Plan

`bruvtext` should ship a benchmark or stress mode in addition to the visual
showcase.

The showcase answers:

- does the text look correct

The benchmark should answer:

- how does the renderer behave under load

## Purpose

The benchmark should help detect regressions in:

- glyph shaping cost
- atlas cache churn
- atlas upload cost
- draw batch growth
- GPU text rendering cost

## Benchmark Modes

Useful first benchmark cases:

1. repeated Latin text
2. mixed multilingual text
3. many short labels
4. fewer very long paragraphs
5. raster-size churn

## Useful Controls

The benchmark mode should let us vary:

- number of text runs
- total glyph count
- font choice
- raster size
- draw scale
- whether text content is stable or changing

## Useful On-Screen Stats

The benchmark should display:

- total text runs
- total draw glyphs
- total draw batches
- atlas page count
- atlas memory
- active font pages
- frame time
- FPS

## Stress Cases We Care About

Important cases:

- many repeated HUD labels
- many different strings
- multilingual CJK-heavy content
- repeated size changes
- first-use glyph churn versus steady-state rendering

## Goal

The benchmark does not need to become a full profiling suite.

It just needs to give us a boring, reproducible way to answer:

- did the cache behave sanely
- did batching regress
- did atlas uploads explode
- did text rendering get slower after a change
