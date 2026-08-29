# Unreal Content Rules

Applies to `Content/**`.

## Assets and Authority

Project-owned assets live under `Content/SlayTheSpireDemo/`.

Prefer C++ for authoritative battle/deck/status/modifier/event logic. Prefer Blueprint/UMG/DataAssets for presentation, assembly and content configuration. Blueprint/UMG never owns authoritative Gameplay state.

Do not claim a Blueprint, Widget, DataAsset, `.uasset` or `.umap` change was made unless it was actually edited in Unreal Editor or through a verified UE-supported tool. Do not infer Blueprint graph contents from filenames or asset metadata alone.

## Manual Unreal Work

When required UMG/Blueprint work cannot be performed automatically, label it `USER ACTION REQUIRED` and specify:

- exact asset path and class;
- graph/function;
- nodes and pins;
- property values;
- compile/save order;
- expected visible/runtime result;
- PIE steps and evidence to return on failure.

Do not substitute speculative C++ changes for required Blueprint work.

## Current UI-A2E Work

Current manual Blueprint/UMG work is concentrated under `Content/SlayTheSpireDemo/`.

Follow:

- `docs/UIA2ERemainingSteps.zh-CN.md`
- `docs/Phase6UIA2EImplementation.md`

Status update/removal playback must use exact historical identity: `TargetPresentationId + StatusId + RuntimeSequence`.

Keep Keyword presentation separate from Gameplay Status/Modifier/Action/DeckRule semantics. Do not model `Keyword = StatusData` or add KeywordLibrary/rich-text tooling without a concrete requested need.
