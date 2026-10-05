# G9-B amendment — confirmed card FIFO, turn-end discard group, relic tails

Date: **2026-10-05**. Branch: `codex/g9-buffered-input-detached-cards`.
Implementation base: `fe80565`. Status: **IMPLEMENTED / AUTOMATED GATES PASS /
VISUAL GATES PENDING / OPT-IN / NOT SEALED**.
This user-approved amendment supersedes G9-B's selection-only buffer, single
pending-intent limit, fresh confirmation after catch-up, and Relic reactions
preceding RetryDraw. G9-C–F are not part of this amendment; G9 remains NOT SEALED.

## Input contract

The sole HUD input arbiter owns one unconfirmed card/target draft, a confirmed
FIFO (capacity 32), and one accepted EndTurn marker. No queue region, numbering
or queue-management buttons are added. A confirmed exact card cannot be queued
twice. Cancel only clears the unconfirmed draft.

During Blocking playback, frozen visible Hand owners permit hover, selection,
Enemy/Self target clicks and None confirmation. Confirmation stores Battle,
PlayerTurnSerial, session/binding generation, RuntimeId/CardId, exact target
PresentationId, capture revision and ordered input identity. Capture changes no
Gameplay zone, energy or live binding. Revision records origin; ordinary history
progression does not invalidate later FIFO entries.

At a matching legal read/display boundary the HUD removes the head and forwards
its complete intent through ViewModel to existing RequestPlayCard. One accepted
card completes Gameplay, tail reactions and Blocking history before the next
Request. Ordinary busy waits; insufficient energy, dead/wrong target or card no
longer in Hand gives feedback and skips without retargeting/spending. Battle,
turn or authority mismatch clears all input. Ready/Presentation/ViewModel changes
schedule merged non-reentrant evaluation; NativeTick never consumes the FIFO.
Enabled card input never invokes FastInput Skip; disabled preserves G8-B.

EndTurn permission is accepted before draft/FastInput cancellation. Confirmed
cards remain; no more cards may be added after acceptance. EndTurn executes once
after them, in the same turn. Mandatory choice immediately clears draft/FIFO/old
EndTurn, including before choice display; old input never returns. Disable,
explicit Skip/recovery, replacement/destruction clear pending input without
undoing accepted Gameplay. Stale callbacks cannot repopulate it.

## Relic contract

Dispatch synchronously freezes eligibility and builds reactions. Status enters
the front; all Relic reactions enter the back of existing ActionQueue, atomically
validated together. Per-event order remains Priority -> RuntimeSequence ->
LocalTriggerIndex; deferred events retain occurrence order. Card continuations
preserve dependency order. Prepared counter/reward batches execute continuously
once their deferred reaction begins.

Future player PlayCardActions remain outside ActionQueue: **A effects/destination
-> A relics -> A Blocking history complete -> B**. Actual energy/block rewards
are deferred, not merely UI. No Sundial/Abacus/CardId special cases. Future Relic
event reactions use this rule; pre-commit Modifiers remain pre-commit. Gameplay
never waits for Presentation.

## Turn-end discard group

Add TurnEndDiscard preserving existing enum values/Selection protocols. Turn-end
declares canonical Hand RuntimeIds and tags only its discard Actions. Shared
Controller/Base/N-child transition supports Hand sources without fake Selection
lifecycles. All children prepare before transfer, then start together from their
own fan positions with existing motion/duration/fade parameters. Gameplay and
reducer remain serial; exact group/token suppression prevents future redisplay.
Incomplete/interfered/unavailable groups fall back to existing single-record
playback. Cancel/timeout/recovery clean every child, never Gameplay ResolutionFault.

## Delivery and validation

Separate local commits: contract; relic scheduling; confirmed FIFO; turn-end group.
Each C++ batch regenerates projects with bundled .NET 10, builds UE 5.8 Development
Editor, runs affected Automation and necessary focused Native PIE. Fix failures
and rerun only affected gates. No push/generated files/Content/Config/Legacy/
plugins/dependency changes.

Automate exact ordering/identity/counts, two draws crossing shuffle threshold,
multiple shuffles/relics, Status interaction, no-history, atomic failure, FIFO
duplicate/invalid/busy/reentrant/mandatory/ABA/disable/replacement, group preflight/
chronology/GC/cancel/timeout/stale callbacks and affected Selection/R8 contracts.
PIE: confirmed B/C during A, queued EndTurn, mandatory-clear, simultaneous discard,
draw/shuffle threshold followed by reward. Use Native production map
/Game/SlayTheSpireDemo/Maps/L_Battle_RuinedCitadel. Unperformed visual gates are USER
ACTION REQUIRED; numeric guarantees belong to Automation. Startup remains false
until amended B gates pass; default activation is a separate tested local commit
with actual production HUD readback.

## User observations at authorization

The user observed hover but could not complete card/target input (expected under
the superseded selection-only contract). EndTurn during playback and replacement
of transient selection reportedly meet their standard. Mandatory-choice EndTurn
must remain unavailable. No HEAD/configuration/RHI supplied: these are user
observations, not a new controlled final-head validation receipt.

## Execution evidence

- Contract batch: documentation only; no new build/Automation/PIE claimed.
- Relic scheduling: `6071c56`, focused build/54-case Automation PASS.
- Confirmed FIFO: `b105fb1`, build and affected Automation PASS.
- Turn-end discard group: implemented; build and 94 distinct affected cases have
  valid passing evidence across initial/repaired runs, not one aggregate run.
- Actual scopes, failures, repairs and pending visual gates:
  [revision execution](G9BRevisionExecution.md).
- Default enablement: gated.
