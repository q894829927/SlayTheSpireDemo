# Codex Goal Checkpoint — Native Hand architecture correction

Branch: `codex/g9-buffered-input-detached-cards`. Completed commits: baseline
`5f7f4fca3fe46208d18042461cf9420ae18a49c5`, structure `4237526`. This layout
commit's tested parent is `4237526`; retrieve its final hash from Git history.

Authority: `docs/NativeHandStructureRefactor.md` and the user-approved correction.
Root AGENTS requires a tested/documented local commit per coherent batch. No push,
assets, Gameplay semantics, Legacy, plugins or dependencies changed.

Implemented: final shared notification dispatcher/restricted hooks, private
GC-safe exact-Battle registry, serialized prepare/commit, exact-token incoming
draw attachment/adoption/cancel; dedicated Hand Slot/private Slate SPanel,
first-pass allotted-size arrangement, explicit layers/frozen ranks and exact-token
moving-card geometry protection. Layout no longer depends on interaction Tick.
Base tracked playback also owns exact cancellation retirement after animation
completion, cleaning temporary draw and retained cross-record visuals before
derived cancellation dispatch, without another pending token owner.

Validation: prescribed project generation and UE 5.8 Development Editor build
PASS. Structure batch: 50 distinct successful cases across initial/recovery runs.
Layout batch initially covered 56 cases with a repaired production latent fixture;
the later Skip finding required affected revalidation. Final 55-case receipt
scope: 53 successful cases (one expected warning) plus two old R5 sequence-mismatch
failures; corrected fixture Records passed a targeted 2/2 recovery. Unaffected
panel/frozen-face evidence is reused. Exact scopes/logs are in the dedicated
document; overlapping totals are not added together.

Native D3D12 MCP PIE with G9 false observed normal play/two draws, Selection
select/deselect/confirm, viewport resize during playback, later hover and
Resolving stop/restart/fresh hover. The additional same-HUD Skip check found a
retained PlayArea ghost in the completion-receipt window. After tracked retirement
cleanup, that exact reproduction and earlier moving-draw Skip passed, with fresh
Defend/Twin Strike hover and exact ordinary selection. This refactor's focused
visual gate is PASS. Editor sessions ended normally; no Content/Config changes.

Next: collect the original full G9-B enabled-input/EndTurn visual gates (USER
ACTION REQUIRED), then update evidence/status in a separate documented
commit. G9-B remains OPT-IN / PARTIAL PIE / NOT SEALED, default false. Do not
begin C–F or declare acceptance before all B gates are satisfied.
