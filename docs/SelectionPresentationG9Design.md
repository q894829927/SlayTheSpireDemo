# Selection Presentation G9 — Buffered Card Input + Detached Card Presentation

Date: **2026-09-12**

Status: **DESIGN PLANNING / NOT IMPLEMENTED**

Authority baseline: [`SelectionPresentationG8FSeal.md`](SelectionPresentationG8FSeal.md).

G8 is sealed. G9 is a new initiative and must not reopen sealed G8 behavior for speculative cleanup.
This document defines the proposed scope, sequencing, ownership rules, stale fencing and acceptance
criteria for buffered next-card input and detached played-card visual tails. It does **not** authorize
or imply production implementation by itself.

## 1. Goal

G9 targets the following player experience:

```text
play card A
→ A CardPlayed / destination visual continues

while A visual is still alive:
→ surviving Hand card B can hover / raise normally
→ clicking B does not Skip A

if Gameplay is not yet at the exact safe surface:
→ capture one buffered intent for B
→ do not execute B Gameplay early

when the exact next safe player-card surface is reached:
→ revalidate Battle / Session / revision / RuntimeId / legality
→ only if still exact and legal, submit B normally

A visual may continue while B Gameplay and B presentation begin
```

Core invariant:

> **G9 may overlap card visual lifetimes, but it must not overlap authoritative Gameplay resolutions.**

The authoritative flow remains:

```text
Gameplay request
→ BattleActionQueue / reducer / committed presentation facts
→ exact player-facing read boundary
→ next legal Gameplay request
```

Visual overlap is never permission to speculatively execute future Gameplay.

## 2. Current baseline and why G9 needs separate architecture

The current G8-sealed baseline has three important constraints.

### 2.1 FastInput currently means catch-up by Skip

`BattleHUDWidgetFastInput.cpp` currently captures an exact presentation credential and, when a real
Blocking presentation owns the delay, executes:

```text
click
→ capture SessionToken + BattleId + ExpectedCatchUpRevision
→ SkipPresentation()
→ next-tick exact retry
```

That contract remains correct for ordinary Blocking catch-up, but it is not a buffered-input scheduler.
G9 must not reinterpret every FastInput request as a buffered intent.

### 2.2 Hand hover currently pauses during tracked/native presentation

`BattleHUDWidgetHandInteraction.cpp` currently updates FanHand interaction only when there is no
tracked/native presentation and the current interaction surface is available.

G9 must separate:

```text
Hand structural/layout reconciliation
from
surviving-card hover affordance
```

so that hover can continue without rewriting the source/arrival geometry owned by an active card visual.

### 2.3 Card lifecycle currently uses one cross-record played-card reference

The sealed R8 card lifecycle intentionally retains one `NativePlayedCardWidget` from `CardPlayed`
Finish until the matching `PlayArea -> destination` record.

That model is suitable for one chronological Blocking card visual, but it is not sufficient for:

```text
A destination tail still alive
+
B CardPlayed already active
```

G9 therefore needs explicit multi-instance card visual ownership before true card-tail detach can be enabled.

## 3. Scope

### 3.1 First-version G9 target

G9 v1 may make these visual paths overlap with future input / future card presentation:

```text
CardPlayed
PlayArea -> DiscardPile
PlayArea -> ExhaustPile
PlayArea -> RemovedPile
```

These are the initial detached-card candidates because they form the normal played-card lifecycle and
already share the cross-record `NativePlayedCardWidget` dependency.

### 3.2 Remain Blocking in G9 v1

Unless separately amended, the following stay Blocking:

```text
DrawPile -> Hand
Hand -> Discard at turn cleanup
SelectionArea transitions
G6 multi-selection Group
Shuffle
PendingSelection presentation
TargetChoice presentation
formal Energy / HP / Block / Status state updates
Terminal / unavailable / recovery
```

G9 is not a universal presentation concurrency framework.

### 3.3 Buffered intent scope

First version buffers **only one card-selection intent**.

It does not buffer:

```text
EndTurn
Target click
Confirm
Cancel
PendingSelection submit/cancel
multiple queued future cards
```

For a target-requiring card:

```text
early click B
→ may buffer/select B later
→ if B remains legal, normal replay enters ChoosingTarget
→ player must provide a new physical target click
```

One old physical click must never cross multiple interaction-state transitions.

## 4. Buffered intent identity

G9 introduces a dedicated buffered card intent. It is conceptually separate from the current FastInput
catch-up credential even if some identity fields are shared.

Proposed shape:

```cpp
struct FBufferedCardIntent
{
    FPresentationSessionToken SessionToken;
    int64 BattleId = 0;
    int64 ExpectedReadyRevision = 0;
    int32 RuntimeId = INDEX_NONE;
};
```

The exact implementation location is not frozen by this document, but the authority rules are.

### 4.1 Capture rule

Capturing an intent means only:

```text
"the player requested RuntimeId X during this exact authority window"
```

It does **not** mean:

```text
X is guaranteed to execute later
energy is reserved
future target is reserved
future legality is frozen
future Gameplay has begun
```

### 4.2 Exact replay rule

Replay is allowed only if all required identity and legality checks still pass:

```text
same exact PresentationSessionToken
same BattleId
current StateRevision == ExpectedReadyRevision
no authoritative PendingCardSelection
expected normal player-card interaction surface
RuntimeId still exists in the current displayed/live Hand
RuntimeId still identifies the same authoritative card instance
card remains Gameplay-playable
current energy / cost / target requirements remain legal
battle Outcome remains None
no newer decision surface superseded the intent
```

Revision comparison is exact:

```text
CurrentRevision == ExpectedReadyRevision
```

Never use:

```text
CurrentRevision >= ExpectedReadyRevision
```

because a newer revision may represent Draw, trigger resolution, pending selection, target state,
terminal state or another decision boundary. Old physical input must become stale rather than leak forward.

### 4.3 Replacement and stale fencing

Any of the following makes the buffered intent stale:

```text
PresentationSessionToken replacement
Controller replacement / ControllerEpoch change
HUD replacement
BattleId change
battle replacement
presentation authority transition
DirectBaseline / unavailable transition when the captured mode no longer applies
terminal transition
ExpectedReadyRevision mismatch
card leaving Hand
card becoming unplayable
new PendingSelection boundary
```

Stale intents are dropped silently. They do not trigger Skip and do not replay onto a newer surface.

### 4.4 Single buffered intent policy

G9 v1 permits at most one buffered card intent.

If another card is clicked in the same exact buffered window, the implementation may replace only the
requested `RuntimeId` while retaining the original immutable authority credential, mirroring the existing
one-request FastInput behavior.

A new credential requires a new physical input boundary.

G9 v1 does not create a command queue for B/C/D/E.

## 5. Buffered-input window

G9 must explicitly distinguish three cases.

### Case A — normal ready surface

```text
input already legal
→ submit normally
→ no buffering
```

### Case B — G9-recognized harmless card-visual window

```text
formal prior Gameplay is complete or progressing toward the exact known next player surface
and the remaining obstacle is an eligible card Presentation tail
→ capture buffered card intent
→ do not Skip that harmless card visual
→ do not submit Gameplay yet
```

### Case C — other Blocking / recovery / ambiguous authority

```text
PendingSelection
TargetChoice transition
terminal
unavailable
unknown/recovery presentation
non-G9 Blocking presentation
```

These do not become generic buffered-input windows. Preserve the existing sealed behavior or reject the
request according to the current authority contract.

G9 must not turn "presentation exists" into a blanket permission to buffer future Gameplay.

## 6. Hand hover and interaction split

The current FanHand interaction gate prevents hover while tracked/native presentation is active. G9-B
must split structural ownership from hover affordance.

Conceptually:

```text
A. structural Hand layout/reconciliation
   authoritative card set / slot / ordering / transition geometry
   remains gated by reducer/presentation ownership rules

B. hover affordance
   visual-only lift/scale for surviving formal Hand cards
   may update while an eligible prior card visual is still alive
```

Possible API direction:

```cpp
FanHand->UpdateLayout(...);
FanHand->UpdateHoverAffordance(...);
```

The exact names are not frozen, but the separation is.

Hover updates must not:

```text
move the active transient card source
reorder formal Hand
claim a card owned by PlayArea/detached job
change Gameplay selection state
mutate reducer state
invalidate exact transition geometry
```

## 7. Detached card visual ownership

True G9 card overlap requires replacing the single cross-record played-card reference with explicit
multi-instance visual jobs.

Conceptual model:

```cpp
struct FDetachedCardVisualJob
{
    FPresentationSessionToken SessionToken;
    int64 BattleId = 0;
    uint64 VisualJobId = 0;
    int32 RuntimeId = INDEX_NONE;

    // frozen visual identity / source / destination state
    // Presentation-only widget reference
    // phase and finite lifetime / completion state
};
```

Possible lifecycle:

```text
HandSource
→ PlayArea
→ DestinationTail
→ Done
```

The key requirement is not the struct shape; it is that each old visual has its own exact identity and
cannot act on a newer formal owner merely because RuntimeId matches.

### 7.1 Formal ownership always wins

A detached visual is never the authoritative card-zone owner.

If Gameplay later brings the same runtime card back into a formal zone while an old tail still exists:

```text
old visual job must be retired or detached from formal ownership
new formal Hand/zone owner wins immediately
old callback cannot hide/remove/move the new formal Widget
```

Example high-risk case:

```text
RuntimeId 17 played
→ old discard tail still alive
→ Gameplay redraws RuntimeId 17 into Hand
→ formal Hand Widget for 17 becomes authoritative
→ old visual tail must not later delete or hide it
```

This case requires automated coverage before G9-D activation.

### 7.2 One-way visual completion

Once a card visual is detached from authoritative chronology:

```text
visual completion
→ cleanup its own visual instance only
```

It must never:

```text
complete a Controller Record
advance reducer chronology
write deck zones
write selected card state
unlock/lock input
submit Gameplay
restore stale Hand visibility
```

### 7.3 Cleanup boundaries

Detached card visuals must be cleaned exactly on:

```text
HUD destruction/deactivation
Controller replacement
Battle replacement
session invalidation
terminal/unavailable transition where old presentation ownership is invalid
runtime feature disable
same-card formal ownership collision requiring old-tail retirement
explicit global cleanup / recovery boundary
```

Cleanup must be idempotent and stale-safe.

## 8. Relationship with FastInput

G9 does not delete G8 FastInput.

The two mechanisms serve different purposes:

```text
FastInput
→ real Blocking chronology should be collapsed
→ SkipPresentation
→ exact catch-up retry

G9 buffered intent
→ eligible harmless card visual should continue
→ do NOT Skip it
→ wait for exact safe player surface
→ exact replay
```

Decision ordering should therefore conceptually be:

```text
SelectCard(B)
→ already legal now? submit normally
→ eligible G9 buffered-card window? capture intent, no Skip
→ otherwise existing FastInput catch-up eligible? preserve sealed FastInput behavior
→ otherwise normal reject/base request behavior
```

G9 must not make a pure detached card tail report as `HasSkippablePresentationDelay()` merely so input can
be accepted. A detached tail is not a catch-up reason.

## 9. Card-record migration strategy

G9 must separate ownership migration from NonBlocking activation.

### 9.1 First migrate the visual owner while preserving Blocking parity

Before allowing input to overlap card visuals:

```text
single `NativePlayedCardWidget`
→ explicit card visual job ownership
```

but Controller behavior should remain equivalent to R8 Blocking semantics.

This proves identity, cleanup and destination handoff before timing/readiness changes are introduced.

### 9.2 Then enable NonBlocking card tail

After multi-instance ownership is validated:

```text
CardPlayed / eligible PlayArea destination formal record
→ validate frozen historical identity
→ prepare visual transaction
→ commit reducer/formal snapshot exactly once
→ publish/advance authoritative chronology
→ activate/update detached card visual job
→ visual continues privately
```

The visual transaction must follow the G8 lesson:

```text
pre-commit visual prepare failure
→ zero external visual side effects
→ fall back to sealed Blocking R8 path

post-formal-commit visual activation failure
→ drop visual only
→ never replay reducer
→ never roll back formal zone state
```

## 10. Proposed stages

### G9-A — Buffered Intent Foundation

Goal: build exact intent identity and stale fencing without changing production UX.

Implement:

```text
FBufferedCardIntent or equivalent exact credential
capture/revalidate helpers
single-intent lifecycle
replacement/terminal/pending-selection invalidation
Automation-only shadow evaluation
```

Do not yet:

```text
change hover gating
change Skip behavior
submit buffered Gameplay in production
change card visual ownership
```

Acceptance emphasis:

```text
same session + exact expected revision can validate
stale session drops
stale/newer revision drops
battle replacement drops
RuntimeId absent/unplayable drops
PendingSelection/terminal drops
no Gameplay request is emitted by shadow path
```

### G9-B — Buffered Hand Input

Goal: first player-visible improvement while card presentation is still Blocking.

Implement:

```text
separate FanHand hover affordance from structural reconciliation
allow surviving formal Hand cards to hover during eligible card-animation window
click B captures buffered intent instead of Skip
when the exact safe surface is reached, replay B once if still legal
```

At this stage A visual may still be Blocking. Therefore B Gameplay starts only after the existing
chronological card presentation reaches the safe boundary.

Acceptance:

```text
A card animation active
→ B hover raises normally
→ click B does not Skip A
→ B does not execute early
→ at exact safe boundary B executes once if legal
```

This is the first meaningful UX milestone.

### G9-C — Multi-instance Card Visual Job Ownership

Goal: replace the single `NativePlayedCardWidget` lifecycle with exact job ownership while preserving
Blocking timing parity.

Implement:

```text
card visual job identity
multi-instance container / owner
CardPlayed -> PlayArea -> destination handoff by exact job identity
cleanup / cancel / destruct / replacement behavior
same-RuntimeId formal-owner protection
```

Production timing remains Blocking until C passes.

Acceptance:

```text
normal R8 visual behavior unchanged
no duplicate / flashback
all exact cancel paths clean correct job only
multiple synthetic jobs do not cross-clean each other
same RuntimeId redraw cannot be damaged by old job
```

### G9-D — NonBlocking Card Tail Activation

Goal: allow eligible played-card visual tails to outlive formal chronology/readiness.

Initial detached candidates:

```text
CardPlayed
PlayArea -> DiscardPile
PlayArea -> ExhaustPile
PlayArea -> RemovedPile
```

Keep other card/selection transitions Blocking.

Acceptance:

```text
A card visual alive
+ exact next player surface ready
→ input available
→ B may execute
→ A is not Skipped merely to accept B
→ A and B visual lifetimes may overlap
→ authoritative Gameplay remains serial
```

### G9-E — Integration / Cleanup

Goal: remove migration scaffolding and validate system interactions.

Cover:

```text
FastInput coexistence
PendingSelection
TargetChoice
ReadyToConfirm
runtime feature disable
HUD replacement
Controller replacement
battle replacement
terminal/unavailable
DirectBaseline boundaries
same-RuntimeId redraw
GC / destruction
viewport/layout changes
multi-hit G8 DamageNumber coexistence
G6 Selection Group regression
```

No new feature should be added in E.

### G9-F — Evidence / Seal

Goal: final build, affected Automation, manual PIE and seal document.

Only after all required evidence passes may status become:

```text
G9 — COMPLETE / VALIDATED / SEALED
```

## 11. Automation requirements

At minimum, dedicated G9 tests must prove the following contracts.

### Buffered intent

```text
A visual active -> B intent captured -> no Skip
exact ready -> B replay exactly once
stale SessionToken -> drop
stale BattleId -> drop
older revision -> drop
newer revision -> drop
B left Hand -> drop
B becomes unplayable -> drop
energy/legality changes -> normal revalidation/drop
PendingSelection appears -> drop
target card replay selects card only; target is not auto-selected
second click in same credential window replaces RuntimeId only
replacement requires new physical credential
```

### Card visual ownership

```text
A and B visual jobs coexist
A callback cannot mutate B
completion order inversion is harmless
wrong/stale job completion is no-op
exact cleanup removes only exact job
same RuntimeId redraw retires old visual ownership safely
formal Hand/zone owner always wins
old tail cannot restore stale opacity/visibility/transform
```

### Recovery / lifecycle

```text
HUD replacement
Controller replacement
battle replacement
terminal
runtime detached-card disable
Widget destruction
Skip/recovery
GC
```

must leave:

```text
no ghost card
no duplicate card
no stale callback mutation
no stuck input
no lost formal Hand card
```

## 12. Required regressions

G9 validation should include the directly affected sealed suites/contracts, not arbitrary whole-project reruns.
Expected regression set includes at least:

```text
G8-D detached Damage behavior
G8-B exact FastInput/session fencing
Phase6UIA2N.FastInput
R8 Native Card Lifecycle
CardSelection.Unified
G5/G6 Selection Presentation lifecycle/group behavior
```

Exact suite names/counts should be recorded in the eventual G9 execution document from the current tree at
implementation time rather than frozen prematurely here.

## 13. Manual PIE gates

### G9-B PIE

```text
play A
→ while A card animation is active, hover B
→ B raises/scales normally
→ click B
→ A does not Skip
→ B does not execute before safe boundary
→ at safe boundary B executes once if still legal
```

### G9-D PIE

Core scenario:

```text
play A
→ A card visual continues
→ hover B works immediately on eligible window
→ click B does not Skip A
→ B request is accepted/replayed at exact safe boundary
→ A visual tail remains alive
→ B Gameplay begins
→ A/B card visuals may overlap
```

Observe:

```text
no duplicate
no flashback
no ghost card
no stale opacity/visibility restore
no formal zone rollback
no stuck input
no accidental FastInput Skip caused only by detached card tail
DamageNumber and card-tail cosmetics may coexist without owning Gameplay readiness
```

Target-card scenario:

```text
early click target card B
→ B replay later may enter ChoosingTarget
→ old click does not auto-select any target
→ player performs a fresh target click
```

Same-RuntimeId redraw scenario when achievable:

```text
old destination tail alive
→ same runtime card formally returns to Hand
→ formal Hand card is correct and interactive
→ old tail cannot later remove/hide it
```

## 14. Feature-disable / fallback contract

G9 detached-card presentation must have a narrow runtime fallback policy.

If detached-card activation is disabled without changing presentation authority:

```text
keep current PresentationSessionToken
clear current detached-card visual jobs safely
clear buffered intent if its replay assumptions no longer hold
future eligible card records use sealed Blocking R8 path
re-evaluate input through existing exact guards
```

Do not mint a new SessionToken merely because the feature flag changed.

If disable occurs together with a true HUD/Controller/battle/authority replacement, normal replacement rules
invalidate the old session.

## 15. Non-goals

G9 v1 does not authorize:

```text
parallel Gameplay resolutions
multiple-command buffered queue
speculative energy reservation
speculative target reservation
auto-target replay from an old click
generalized detached framework for every Presentation record
detached DrawPile -> Hand
detached Selection Group
rewriting G8 DamageNumber ownership
changing Gameplay card-zone authority
```

Future broader Presentation overlap requires separate design evidence after G9 is sealed.

## 16. Delivery order and stop gates

Recommended execution order:

```text
G9-A buffered intent identity / shadow fencing
→ build + focused Automation

G9-B hover + buffered input production activation
→ build + focused Automation + manual PIE

G9-C card visual job ownership migration, still Blocking
→ build + R8-focused Automation + manual parity PIE

G9-D NonBlocking card-tail activation
→ build + G9-D/G8/FastInput/card regressions + manual overlap PIE

G9-E integration cleanup
→ affected integration matrix

G9-F evidence / seal
```

Do not proceed from C to D until card visual ownership is proven independently of timing changes.
Do not mark G9 complete based only on buffered input; true G9 completion requires detached card-tail overlap and
final integration evidence.

## 17. Current status

At creation of this plan:

```text
G8   — COMPLETE / VALIDATED / SEALED
G9   — DESIGN PLANNING / NOT IMPLEMENTED
G9-A — NOT STARTED
G9-B — NOT STARTED
G9-C — NOT STARTED
G9-D — NOT STARTED
G9-E — NOT STARTED
G9-F — NOT STARTED
```

No production code change is authorized by this document alone.
