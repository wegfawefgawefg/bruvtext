# Vendoring Guide

This is the practical guide for bringing `bruvtext` into another project.

## Recommended Options

Use one of these:

1. `git submodule`
2. `git subtree`
3. plain copied folder committed into your repo

Do **not** gitignore the vendored source tree itself. Track it normally.

## CMake Example

If the repo is vendored at `third_party/bruvtext`:

```cmake
add_subdirectory(third_party/bruvtext)

target_link_libraries(my_app
    PRIVATE
        bruvtext
)
```

The bundled demo target does not need to be part of your application build. It
is there as a reference implementation and validation tool.

## What To Ignore

Ignore only local/generated outputs.

Good host-project ignores include:

```gitignore
third_party/bruvtext/build/
third_party/bruvtext/artifacts/
third_party/bruvtext/.codex/
```

Do **not** ignore:

- `third_party/bruvtext/src/`
- `third_party/bruvtext/include/`
- `third_party/bruvtext/demo/shaders/`
- `third_party/bruvtext/docs/`

## Fonts And Demo Assets

The repo includes demo-only fonts and text assets.

That means:

- they will be present in the checkout
- they are useful for the bundled showcase and benchmark
- they are not required by the core library
- your project should still register its own fonts

If you do not build or ship the demo, those assets do not become a runtime
dependency of your application automatically.

## Expected Integration Flow

Application code should stay simple:

```cpp
bruvtext::BeginFrame(text);
bruvtext::DrawText(text, font, "hello", 32.0f, 64.0f, 20.0f);
bruvtext::EndFrame(text);
```

Renderer code owns the Vulkan details:

- atlas image creation
- atlas uploads for dirty pages
- vertex buffer uploads
- draw submission

See:

- [Integration Quickstart](integration-quickstart.md)
- [Vulkan Integration](vulkan-integration.md)
- [Reference Renderer](reference-renderer.md)
