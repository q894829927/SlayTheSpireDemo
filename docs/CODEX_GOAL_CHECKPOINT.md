# Checkpoint — amended G9-B

Branch `codex/g9-buffered-input-detached-cards`; current HEAD `b105fb1` plus
validated TurnEndDiscard batch awaiting its local commit. Resolve the next commit hash
from Git; implementation base `fe80565`, contract commit `fc0de47`.
Authority: `docs/QueuedCardPlayAndRelicTimingAmendment.md`.

Completed: Native Hand refactor; amended contract; relic scheduling commit
`6071c56` (54-case focused PASS). FIFO implemented and validated: frozen draft,
confirmed plays, exact targets, automatic one-at-a-time requests, invalid-item
feedback/skip, EndTurn marker, mandatory/lifecycle cleanup, disabled fallback.
Final affected FIFO build PASS; G9 + G8B + FastInput 30/30 PASS. Combined valid
FIFO coverage is 61 distinct cases across runs, not one run. Exact logs and
failed/repaired gates: `docs/G9BRevisionExecution.md`.

TurnEndDiscard implemented: explicit producer metadata, shared Hand-source Group
engine, exact suppression, transactional prepare, viewport/GC/lifecycle cleanup.
Final generation/build PASS; all 94 distinct affected cases have valid passing
evidence across repaired runs. See the execution document for actual counts.

Next: commit this batch. Final production MCP PIE must cover automatic B/C,
EndTurn tail, mandatory clear, simultaneous discard and deferred relic timing.
Any unperformed visual gate remains USER ACTION REQUIRED.

Preserve/exclude the externally saved Native HUD Content asset modification.
Editor was closed externally. No asset/config/plugin/dependency/generated files
belong in these commits. Each complete batch has docs/tests and an independent
local commit; never push. G9 C++ default stays false until amended B gates pass.
Original G9-C–F remain unstarted; no G9 seal.
