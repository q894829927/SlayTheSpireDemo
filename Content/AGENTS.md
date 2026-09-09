# Unreal Content Rules

Applies to `Content/**`.

## Assets and Authority

Project-owned assets live under `Content/SlayTheSpireDemo/`.

Prefer C++ for authoritative battle/deck/status/modifier/event logic. Prefer Blueprint/UMG/DataAssets for presentation, assembly and content configuration. Blueprint/UMG never owns authoritative Gameplay state.

Do not claim a Blueprint, Widget, DataAsset, `.uasset` or `.umap` change was made unless it was actually edited in Unreal Editor or through a verified UE-supported tool. Do not infer Blueprint graph contents from filenames or asset metadata alone.

## Manual Unreal Work

When required UE work cannot be performed with available tools, label it `USER ACTION REQUIRED` and provide the shortest executable instructions for the affected asset/map and expected result. Include graph nodes/pins, property values and compile/save order when that edit needs them; do not impose a full Blueprint checklist on every asset task.

Do not substitute speculative C++ changes for required Blueprint work.

## Battle UI Content

Native is the active battle UI stack. Follow root Legacy protection rules and `docs/LegacyUIPreservationPolicy.md` when touching battle UI assets or dependencies. UI-A2E is sealed history; its old remaining-steps document is not a current work queue.

Status update/removal playback must use exact historical identity: `TargetPresentationId + StatusId + RuntimeSequence`.

Keep Keyword presentation separate from Gameplay Status/Modifier/Action/DeckRule semantics. Do not model `Keyword = StatusData` or add KeywordLibrary/rich-text tooling without a concrete requested need.
