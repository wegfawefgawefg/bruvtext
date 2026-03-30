# Docs Polish Plan

The repository now has the core technical docs, but it still needs a final
polish pass aimed at outside users.

## Goal

Make the repo easy to understand for someone who lands on it from GitHub and
wants to:

- vendor it into a Vulkan project
- understand what `bruvtext` owns
- understand what their renderer must own
- copy the reference integration quickly

## Current Status

These are now mostly in place:

1. the README is the canonical landing page
2. there is a minimal copy-paste integration example
3. there is a minimal CMake vendoring example
4. the ownership split is stated near the top-level docs
5. there is a dedicated vendoring guide
6. a separate GitHub Pages site now exists under `pages/`

## GitHub Pages

GitHub Pages is now wired through GitHub Actions and publishes the `pages/`
folder.

Remaining presentation polish is optional:

1. keep the `pages/` site and README aligned
2. tighten wording when technical behavior changes
3. improve visual presentation only if it materially helps adoption
