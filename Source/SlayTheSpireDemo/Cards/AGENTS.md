# Card Definition and Effect Rules

Applies to `Source/SlayTheSpireDemo/Cards/**`.

Before adding or changing a CardEffect, read `docs/AutomaticCardDescriptions.md`. Its fixed Chinese wording and automatic-description contract are part of Effect implementation acceptance.

## Effects and descriptions must change together

- Every new concrete CardEffect must supply a non-empty `GetAutoDescriptionFormat` for its supported configurations. Implementing `BuildActions` alone is incomplete; do not rely on the base class's empty description or disable automatic descriptions to bypass this requirement.
- When changing an existing Effect's player-facing behavior or authored properties, update its description and preview arguments in the same change. This includes amounts, counts, repeated hits, target wording, selection modes and Base/Upgraded differences.
- Keep fixed wording in the Effect as localizable Chinese `FText` using stable `NSLOCTEXT` / `LOCTEXT` namespace and keys. Use named format arguments for variable values. Do not hard-code gameplay numbers into sentences or flatten localized sentences into concatenated FString text.
- `BuildAutoDescriptionArguments` may reuse `BuildPreviewArguments`; override it only when a distinct local argument contract is needed. Numeric values must follow effective authored upgrade values and the existing read-only Gameplay preview pipeline. Do not execute Actions, mutate shared definitions or reproduce modifier calculations in UI.
- Automatic arguments are local to each Effect instance. Repeated Effect types must work with their default argument names; do not require card authors to rename arguments globally. Target-specific preview overrides remain scoped by `EffectIndex`.
- The card resolver owns Effects-array ordering, separators and the final self-Exhaust keyword. An Effect describes only its own behavior. In particular, consuming other hand cards does not imply that the played card itself exhausts.
- Use the referenced definition's localized display name for statuses or other named content. Do not introduce CardId/StatusId checks or a centralized concrete-Effect switch to generate descriptions.
- Preserve the existing explicit custom-template compatibility path. Normal new content uses automatic descriptions; custom mode is not a substitute for implementing a new Effect's text.

## Acceptance and documentation

For a new or changed description contract, update the fixed-text catalog in `docs/AutomaticCardDescriptions.md` and add or update the smallest focused Automation coverage. Cover relevant Base/Upgraded values and mode/target variants; when adding an argument path, verify repeated Effect instances and preview isolation where applicable.

Follow `docs/ValidationExecutionPolicy.md`: build for C++ changes and run the affected focused tests. Request manual PIE only for an actual visual gate such as text clipping or wrapping. Do not rerun sealed suites for documentation-only changes.
