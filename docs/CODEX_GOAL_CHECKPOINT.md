# Codex checkpoint — G9-B confirmed FIFO / relic tails / simultaneous discard

Branch `codex/g9-buffered-input-detached-cards`, contract HEAD `fc0de47`,
implementation base `fe80565`; batch 2 commit has parent `fc0de47` (resolve its
own hash from Git).
Authority: `docs/QueuedCardPlayAndRelicTimingAmendment.md`, approved by the user.
Every coherent batch requires tests/docs and a local commit; no push.

Completed: previous Native Hand refactor (see dedicated evidence); revised
contract committed. Batch 2 C++ and architecture/tests are validated:
atomic front/back insertion, Status front / Relic back, tests for a real draw-two
card crossing the shuffle threshold (with/without recording), mixed sources,
multiple events, stale membership and invalid atomic insertion. Runtime relic
authoring rules and architecture documents are updated in the working tree.
User EndTurn/mandatory observations lack supplied HEAD/configuration and are
recorded as feedback rather than controlled verification.

Project generation and prescribed UE 5.8 Development Editor build PASS.
Focused Phase6A + Phase6C + Phase7: 54 cases, 28 Success / 26 with warnings,
zero Fail/NotRun. Exact paths and scope: `docs/G9BRevisionExecution.md`.
Existing PIE was stopped through MCP; the Native HUD asset was subsequently
saved and editor closed externally. Preserve/exclude that user Content change.
Manual relic timing is pending, not an acceptance PASS.

Next: commit validated batch 2, then implement confirmed FIFO and its tests,
then TurnEndDiscard. Required final focused production MCP PIE remains pending.
Capacity 32, no queue UI;
invalid entries skip; EndTurn follows confirmed cards; mandatory clears all input.
Future requests stay outside Gameplay queue until prior card/relic/playback ends.

Startup remains false; default activation awaits amended B acceptance. No original
C–F or G9 seal. Content/Config, Legacy, plugins/dependencies/generated files excluded.
