# Checkpoint — amended G9-B

Branch `codex/g9-buffered-input-detached-cards`; implementation HEAD `72a564a`
plus the final Native PIE documentation receipt. Resolve the receipt commit hash
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

At `72a564a`, Native production PIE passed confirmed FIFO, EndTurn tail,
simultaneous five-card discard and draw/shuffle followed by relic reward.
Mandatory EndTurn isolation observed. Full queued-mandatory clearing remains
USER ACTION REQUIRED; minimal steps and actual configuration are in
`docs/G9BRevisionNativePIE.md`. Next: obtain that one complete natural time line,
then a separately built/tested default-enable commit and production readback.

Preserve/exclude the externally saved Native HUD Content asset modification.
Editor reopened, PIE stopped, observer removed. Native user asset/live G9=true
was tested without modifying or saving it. No asset/config/plugin/dependency/generated files
belong in these commits. Each complete batch has docs/tests and an independent
local commit; never push. G9 C++ default stays false until amended B gates pass.
Original G9-C–F remain unstarted; no G9 seal.
