# Card Selection Refactor Constraints

Status: IMPLEMENTED / AUTOMATED GATES PASS / MANUAL PIE USER ACTION REQUIRED / NOT SEALED

Scope: unify existing card-selection execution timing and interactive Presentation boundaries before continuous-card-play work.

## 1. Purpose

This document defines the mandatory constraints for refactoring current card-selection flows into one shared execution pipeline.

The refactor exists to eliminate the current split behavior where some Effects capture selectable cards during `BuildActions()` while others defer candidate capture until Action execution. That inconsistency causes authored Effect order to diverge from runtime selection semantics and Presentation timing.

The target behavior is:

```text
Effect authored order
→ preceding Gameplay Actions execute
→ selection candidate set is captured from CURRENT Gameplay state
→ player-interactive Presentation boundary is established when required
→ pending Selection begins
→ validated SelectionResult drives the authored Continuation
→ later Effects / played-card cleanup continue normally
```

This is a framework refactor. It must not introduce card-id-specific handling for Burning Pact, Warcry, or any other production card.

## 2. First-phase scope

The first implementation phase is deliberately narrow.

It MUST unify:

- current-Hand candidate capture;
- Player selection;
- Random selection;
- exact-N selection using the current existing clamp-to-available behavior;
- interactive Presentation boundary behavior for Player selection;
- Player continuation execution through the existing `SelectionRequestAction → SelectionResolver → AuthoredContinuation` chain, and Random execution through its non-pending Action path into the same authored Continuation contract.

The first phase MUST NOT attempt to build a universal selection framework for every future zone or interaction surface.

Specifically out of scope for the first phase:

- DiscardPile / ExhaustPile / DrawPile selection UI;
- generic predicate graphs or filter registries;
- arbitrary min/max range semantics;
- multi-zone candidate composition;
- Grid Select presentation;
- continuous-card-play input implementation;
- card-specific production asset edits.

## 3. Existing contracts to preserve

The refactor MUST preserve these architectural contracts:

### 3.1 Gameplay remains authoritative

Selection state, legal candidates, exact runtime identity, result validation, zone mutation, and continuation execution belong to Gameplay.

Presentation may display a pending selection and submit runtime identities, but MUST NOT:

- decide candidate legality;
- mutate Gameplay state;
- rebuild or reinterpret the candidate set;
- execute a Continuation directly;
- own authoritative pending-selection state.

### 3.2 Existing resolver core remains shared

`USelectionResolver` remains the single owner of the active pending Selection.

Its responsibility remains limited to:

- holding one pending request;
- validating request/result structure;
- validating submitted objects against the frozen request;
- invoking the authored Continuation to build downstream Actions;
- resuming the awaiting `USelectionRequestAction` through the existing result path.

The refactor MUST NOT move card-specific semantics into `USelectionResolver`.

### 3.3 Continuations remain authored consumers

`UAuthoredContinuation` remains responsible only for:

```text
validated SelectionResult
→ build downstream Action batch
```

A Continuation MUST NOT:

- enumerate candidates;
- hold mutable resolution-local selection state;
- mutate Gameplay directly;
- drive/pump the ActionQueue;
- own Presentation timing.

## 4. Required responsibility split

The unified design MUST converge on the following responsibility model:

```text
CardEffect
    configures intent only
        ↓
Runtime CandidateSource
    answers: "who is selectable NOW?"
        ↓
Deferred Selection Action
    captures candidates at Execute time
    applies count/cancel policy
    establishes interactive boundary when needed
        ↓
    frozen candidate request
        ├─ Player: interactive boundary
        │    → SelectionRequestAction ↔ SelectionResolver
        │    → pending request / validated player result
        └─ Random: authoritative RNG selection Action
             → validated result, no pending request or interactive boundary
        ↓
AuthoredContinuation builds downstream Actions
        ↓
owning Action inserts the batch in deterministic order and finishes
```

No individual Effect may reimplement this full pipeline.

## 5. CandidateSource contract

### 5.1 Runtime lifetime

A CandidateSource MUST be a runtime object belonging to one selection execution. Its lifetime is tied to that execution, not to one Presentation segment: one Gameplay chain may span several sealed Presentation segments.

It MUST NOT be cached on a shared `UCardEffect` / DataAsset definition.

A Selection Action MUST hold its CandidateSource through a reflected owning reference (`UPROPERTY` or equivalent UObject lifetime-safe ownership).

### 5.2 Execute-time capture

Candidate enumeration MUST occur when the deferred Selection Action executes, not when `UPlayCardAction` calls `Effect->BuildActions()`.

This is mandatory so that authored order has real semantic meaning.

Example:

```text
Draw
→ Select from Hand
```

MUST mean:

```text
Draw Gameplay commit
→ candidate source reads the post-Draw Hand
→ newly drawn cards are eligible when otherwise legal
```

Likewise:

```text
Select from Hand
→ Draw
```

MUST capture the pre-Draw Hand because Selection executes first.

### 5.3 Deterministic ordering

CandidateSource output MUST have deterministic canonical ordering.

For `CurrentHandCandidateSource`, ordering MUST follow the authoritative current Hand order and use exact runtime identity.

Each candidate MUST preserve:

- exact runtime object;
- stable runtime sequence / runtime id;
- selection key required by current Selection contracts.

### 5.4 Freeze after capture

Once `FSelectionRequest` is created and accepted as pending, its candidate set is frozen for that request.

Presentation/UI reads MUST NOT re-enumerate the Hand to reconstruct legal candidates.

## 6. Candidate build result semantics

Candidate capture MUST distinguish a legal empty result from a broken runtime dependency.

A single `bool` return is insufficient.

The implementation SHOULD expose an explicit result state equivalent to:

```cpp
enum class ESelectionCandidateBuildStatus
{
    Success,
    NoCandidates,
    InvalidRuntimeDependency
};
```

Required semantics:

- `Success`: candidate capture succeeded and returned one or more candidates;
- `NoCandidates`: capture succeeded but no legal candidates currently exist;
- `InvalidRuntimeDependency`: required runtime state such as Deck/owner/source is invalid or unavailable.

These cases MUST NOT be silently treated as equivalent.

The resulting execution behavior is mandatory and is specified in section 11.3; reporting a distinct status alone is not sufficient.

## 7. Count policy contract

The first phase supports the existing exact-N clamp behavior only.

This behavior MUST be named/documented as a policy rather than treated as universal Selection semantics.

Equivalent semantic name:

```text
ExactNClampToAvailable
```

Rules:

```text
Requested = N
N <= 0 → legal no-op; no capture, pending request, interactive boundary or RNG consumption
Available >= N  → require exactly N
0 < Available < N → require exactly Available
Available = 0 → legal no-op under this policy
```

The framework MUST NOT imply that every future Selection uses this rule.

Future behaviors such as optional 0..N, at-most-N, or hard-fail-if-fewer-than-N require separate explicit policies.

## 8. Player vs Random selection

Player and Random selection MUST share execute-time candidate capture.

They MUST diverge only after the candidate set is frozen.

### 8.1 Player selection

Player selection MUST:

- use the shared deferred candidate capture;
- establish the interactive Presentation boundary;
- create/enter normal pending Selection;
- use the existing `SelectionRequestAction → Resolver → Continuation` flow.

### 8.2 Random selection

Random selection MUST:

- use the same execute-time CandidateSource;
- freeze the same deterministic candidate ordering;
- use authoritative Gameplay RNG;
- NOT create a pending player Selection;
- NOT establish a player-interactive Presentation boundary;
- build the same authored Continuation result path where practical.

Random selection MUST NOT retain eager candidate capture in `Effect::BuildActions()`.

Preserve the existing random-selection algorithm for the same ordered candidates, requested count and RNG state: sample without replacement, emit selected objects in canonical candidate order rather than draw order, and consume no RNG when every candidate is selected. Zero-count and empty-candidate no-ops also consume no RNG. A test that checks only same-seed repeatability is insufficient; focused coverage must verify these consumption and ordering contracts. Different candidates caused by the intended Execute-time capture change may legitimately produce a different result.

## 9. Interactive Presentation boundary contract

The interactive boundary is a property of entering a player decision, not a property of Warcry or any specific Effect.

All Player Selection flows in scope MUST use one shared boundary mechanism.

Required semantics:

```text
preceding Gameplay commits
→ capture current candidates
→ seal/freeze the committed Presentation prefix
→ switch the remaining Action tail to the continuation writer/segment
→ Gameplay may enter pending Selection immediately
→ Presentation catches up asynchronously
→ UI exposes the pending Selection only when displayed state reaches the frozen boundary
```

The boundary MUST NOT turn Presentation into a Gameplay driver.

The implementation MUST NOT use:

- arbitrary `Delay()` values;
- animation-duration guesses;
- animation callbacks to perform authoritative Gameplay mutation;
- card-specific boundary rules.

### 9.1 Boundary identity and UI exposure

Every actual non-empty Player decision MUST advance the battle's monotonic `StateRevision`, including consecutive decisions with no intervening Gameplay mutation. Zero-count/empty-candidate no-ops and Random selection do not create this decision boundary. This revision advancement is a Gameplay/read-state responsibility and must remain independent of Presentation availability.

When recording is available, freeze the committed prefix at that revision and seal its Records together with its matching FinalSnapshot. The next segment has a new ResolutionId; it must receive both the already-queued tail and later Continuation Actions before they execute. Public delivery remains deferred and must not occur re-entrantly inside an accepted public Request.

While Presentation owns display, UI may expose candidates only when its displayed `(BattleId, StateRevision)` exactly matches the newest frozen decision boundary and the corresponding authoritative request is still pending. An older boundary, a different battle or completion of only an intermediate Record is insufficient. Apply the boundary snapshot before exposing its candidates. Clear transient selected RuntimeIds on a boundary revision/battle change even when source name, count and candidate IDs are identical between requests.

Recording-disabled and unavailable behavior is specified separately in section 11.1; failure to produce visual history must not roll back the decision revision or bypass mandatory Gameplay selection.

## 10. Boundary access injection

The unified Selection Action MUST NOT discover `ABattleManager` by relying on `Queue->GetOuter()` casting as its architectural contract.

Interactive boundary capability MUST be explicitly injected through a narrow runtime access object/interface/delegate owned by the Selection Action or its execution context.

The exact type name is implementation-defined, but its responsibility must remain narrow, equivalent to:

```text
advance committed Presentation to an interactive-selection boundary
and return the writer used by the continuation segment
```

Tests must be able to provide or omit this capability without constructing hidden Outer hierarchies solely to satisfy Selection behavior.

## 11. Gameplay / Presentation failure isolation

Presentation failure and Gameplay queue failure are different fault classes and MUST remain separate.

### 11.1 Presentation degradation

If the interactive Presentation boundary cannot produce an available writer, Gameplay Selection MUST still proceed when Gameplay dependencies are otherwise valid.

Presentation degradation MUST NOT cause a mandatory Gameplay choice to disappear.

Equivalent principle:

```text
Presentation failure → degraded/missing visual history
Gameplay Selection → still authoritative and required
```

Distinguish the following modes explicitly:

| Mode | Gameplay contract | UI contract |
|---|---|---|
| Recording disabled / valid no-history mode | Enter the same mandatory pending request without waiting for history. | Use the supported direct-state selection path; do not require a nonexistent recorded boundary. |
| Normal Skip / timeout catch-up while Presentation remains available | Do not cancel or resolve the pending request. | Reconcile to the exact frozen boundary, then expose selection under section 9.1. Input-event reuse remains deferred under section 14. |
| PresentationUnavailable due to recording/boundary failure | Keep the mandatory request authoritative; do not fabricate a choice, continue past it, or request a Gameplay fault solely for this failure. | Preserve the existing visible error surface and disabled-input policy. This phase does not promise that the player can complete the choice in this mode or introduce a live-state recovery fallback. |

“Selection is preserved” means authoritative pending state survives; it does not mean every degradation mode remains playable. These modes require distinct automated assertions.

### 11.2 Queue insertion failure

If the framework cannot enqueue the required `SelectionRequestAction` or validated continuation batch, this is a Gameplay framework failure.

The implementation MUST NOT silently `Finish()` and continue later Effects as though the mandatory selection succeeded.

It MUST request an appropriate resolution fault according to existing queue fault policy.

### 11.3 Selection failure disposition

The shared pipeline must implement the following outcomes. These are explicit intended changes where existing fail-soft paths currently finish and continue; preserving the resolver's responsibility split does not require preserving those failure outcomes.

| Condition | Required outcome |
|---|---|
| Requested count <= 0 or legal NoCandidates | Finish as the documented no-op; later authored Effects continue. No pending request or interactive boundary. |
| InvalidRuntimeDependency for a non-zero selection | Log the broken dependency and request a Gameplay framework resolution fault at the queue's safe point; do not disguise it as an empty Hand. |
| BeginSelection rejects an internally authored request, or an unexpected request is already pending | Request a framework resolution fault; do not continue later Effects as if selection succeeded. Fault cleanup must release pending ownership coherently. |
| Invalid player submission (wrong count, duplicate/non-candidate identity) | Reject the submission without running the Continuation or releasing the current valid pending request. Cover the public facade and the resolver's direct submission path. |
| Cancel submitted for a mandatory request | Reject and retain pending state; no downstream execution. |
| Legal cancellation of a cancellable request | Clear pending state and Finish through the existing cancellation contract; no selection Continuation is built. |
| A captured runtime dependency becomes invalid while pending, or Continuation construction fails for an otherwise valid result | Request a Gameplay framework resolution fault and clean up pending ownership; do not treat the failure as a legal cancellation. |
| Selection/continuation batch insertion fails | Apply section 11.2; no silent success or partial batch insertion. |
| Presentation writer/boundary unavailable with otherwise valid Gameplay dependencies | Apply section 11.1 only; do not convert visual failure into Gameplay fault. |

The implementation must distinguish legal cancellation, invalid input and internal continuation failure rather than treating a single `false` result as permission to Finish. Do not add queue pumping or Gameplay mutation to the CandidateSource/Continuation to implement these outcomes.

## 12. SelectionRequestAction responsibility

Documentation and implementation MUST define `USelectionRequestAction` as:

- beginning a validated pending Selection;
- remaining the current awaiting Action while a result is pending;
- receiving the submitted result/cancel path through the Resolver;
- validating/building the continuation batch through existing contracts;
- inserting that batch in deterministic order;
- finishing itself when the pending interaction is resolved or legally cancelled.

It MUST NOT be described or implemented as actively pumping/resuming the ActionQueue itself.

## 13. Effect contract after refactor

Effects in scope MUST stop enumerating Hand candidates directly.

An Effect may configure only the selection intent required to compose the shared runtime pipeline, including:

- CandidateSource type/configuration;
- selection mode (Player / Random);
- requested count;
- count policy;
- cancellation policy;
- selection source/debug semantic name;
- authored Continuation.

For current examples, the meaningful Effect-specific difference should reduce approximately to:

```text
Burning Pact-style selection
CurrentHandCandidateSource
+ ExhaustSelectedContinuation

Warcry-style selection
CurrentHandCandidateSource
+ MoveSelectedHandCardsToDrawPileTopContinuation
```

No `CardId` checks are permitted.

## 14. Input-transition constraint

This refactor does NOT, by itself, solve input-event reuse across Presentation skip/catch-up and newly activated Selection.

That issue remains a separate input-transition concern.

Required future interaction rule:

> When one input event causes Presentation to cross into a new player-decision state, that event is consumed and MUST NOT also be interpreted as a selection click in the newly activated state.

Example:

```text
click
→ fast-forward / complete current Presentation
→ Selection becomes visible
→ same click ends here
→ next click expresses selection intent
```

This rule should be implemented with the continuous-card-play/input work, not hidden inside CandidateSource or Resolver logic.

This phase may seal selection execution and ordinary display ordering while this known fast-click issue remains open. Its completion report MUST explicitly state that fast-forward click consumption is not implemented or accepted here; it must not claim complete fast-input interaction correctness.

## 15. Non-Hand candidate sources

The first phase implements only `CurrentHandCandidateSource`.

Future sources such as:

- DiscardPile;
- ExhaustPile;
- DrawPile;
- Combatant targets;

may reuse the same Action/Resolver contract, but adding a Gameplay CandidateSource does NOT imply that Native HUD already has a valid presentation surface for those candidates.

Each new visual selection domain must provide its own RuntimeId-to-widget / grid / target presentation path.

## 16. Required migration

The first phase is incomplete until all existing current-Hand selection consumers in scope stop using eager candidate capture.

At minimum:

1. Warcry-style `SelectHandCardToDrawPileTop` must use the shared pipeline.
2. Player `SelectExhaustHandCard` must use the same shared pipeline.
3. Random `SelectExhaustHandCard` must use execute-time current-Hand capture rather than `BuildActions()`-time capture.
4. The old dedicated `DeferredHandSelectionAction` must either be removed or reduced to a thin compatibility wrapper around the new shared primitive; no parallel behavior may remain.

The migration must leave one authoritative current-Hand candidate capture implementation.

## 17. Authored-order invariants

After refactor, Effect array order MUST be sufficient to determine these semantics without card-specific code:

```text
Draw → Player Exhaust Selection
```

- Draw commits first.
- newly drawn legal cards are included in candidates.
- Draw Presentation reaches the interactive boundary before Selection becomes visible.

```text
Draw → Put Hand Card On DrawPile Top Selection
```

- same ordering guarantees as above.

```text
Player Exhaust Selection → Draw
```

- Selection occurs first.
- only after the selected cards are exhausted does Draw execute.

```text
Selection A → Effect(s) → Selection B
```

- each player Selection gets its own interactive boundary;
- each frozen request preserves its own candidate set;
- writer/Presentation segmentation remains valid across both boundaries;
- final played-card cleanup remains in the correct continuation segment.

## 18. Required validation

Implementation is not complete until focused validation covers at least:

- `Draw → Player Exhaust Selection`: newly drawn card is selectable;
- `Draw → DrawPileTop Selection`: newly drawn card is selectable;
- `Player Exhaust Selection → Draw`: original order remains unchanged;
- no other Hand card before Draw, then Draw creates the only legal candidate: Selection must not be skipped;
- exact-N count with enough candidates;
- `RequestedCount <= 0` is a no-op without candidate capture, interactive boundary or RNG consumption;
- exact-N count clamped when candidates are fewer than requested;
- zero candidates as legal no-op under the current policy;
- invalid runtime dependency distinguished from zero candidates;
- Random selection uses post-prefix current Hand and remains deterministic under DeckRuntime RNG;
- Random selection preserves sampling without replacement, canonical result ordering, and no RNG consumption for all-selected/empty/zero-count cases;
- one card containing two player Selections establishes two valid interactive boundaries;
- consecutive decisions with identical source name, count and candidate IDs have distinct revisions and clear the previous UI selection state;
- Presentation writer is preserved/rebound correctly through final played-card cleanup;
- Presentation unavailable/degraded does not suppress authoritative Gameplay Selection;
- recording-disabled, normal Skip/timeout catch-up and PresentationUnavailable each satisfy their distinct section 11.1 contracts;
- queue insertion failure requests a resolution fault rather than silently skipping mandatory interaction;
- BeginSelection/internal dependency/Continuation failures follow section 11.3 rather than silently continuing;
- invalid submissions and forbidden cancellation preserve the valid pending request; legal cancellation follows its separate finish contract;
- pending candidate set remains frozen after `BeginSelection`;
- existing Burning Pact behavior when authored as Selection-before-Draw remains intact;
- existing Warcry Draw-before-Selection observable ordering remains intact.

Fast-forward click consumption should be covered by the later input/continuous-play work, but its known requirement must remain documented.

## 19. Validation budget and sealing

This refactor changes shared Selection execution and invalidates prior validation for touched paths.

### AUTOMATED GATES

Run standard bundled UE 5.8 project generation, then Development Editor Win64 Build once. Run the new focused prefix `SlayTheSpireDemo.CardSelection.Unified` (planned tests, not currently claimed to exist) covering the deterministic contracts in section 18.

Run these existing affected regression prefixes once:

```text
SlayTheSpireDemo.CardExpansion.Wave1C.Selection
SlayTheSpireDemo.CardExpansion.Wave1CC0.SelectExhaust
SlayTheSpireDemo.CardExpansion.Wave1CC0.Selection
SlayTheSpireDemo.CardExpansion.Wave1CC0.Random
SlayTheSpireDemo.CardExpansion.Wave1CC0.Presentation
SlayTheSpireDemo.CardExpansion.Wave1CC1.DrawPileTop
```

Update affected assertions when they encode a failure disposition explicitly replaced by section 11.3; record that semantic change rather than weakening tests to make them pass. If another concrete touched path requires an additional filter, name the path and filter before running it. Do not add unrelated phase aggregates. Record the exact filters, counts and evidence paths actually executed; overlapping assertions do not require a second run of the same Gate.

### MANUAL PIE GATES

Use Native `/Game/SlayTheSpireDemo/Maps/L_BattleTest` with transient/session-only test card definitions for the following authored orders. Do not save changes to production card assets or the map as part of this refactor. Before handoff, provide the exact fixture setup; if available tooling cannot stage it, label the setup and observations `USER ACTION REQUIRED`.

1. `Draw → Player Exhaust Selection`: play the test card, let the prefix finish naturally, verify newly drawn cards visibly enter Hand before selection appears, select a newly drawn card, and observe its exhaust and normal played-card cleanup.
2. `Draw → DrawPileTop Selection`: verify the same draw-before-selection visibility, select a newly drawn card, and observe its move to the draw pile and normal cleanup.
3. `Player Exhaust Selection → Draw`: select a card, observe exhaust before the later Draw, and verify the new Hand is visually coherent.
4. Two Player decisions in one card: complete each in turn and verify distinct selection transitions, no retained highlight from the first decision, and normal final cleanup.

One focused pass covering these visual paths is sufficient. Evidence is the user's explicit observations or one short recording of the relevant paths; repeated screenshots are not a replacement. Exact pile-top identity, revisions, tokens, candidate membership and failure state belong to Automation. Fast-forward input consumption is excluded under section 14.

Do not claim PASS until evidence exists for the corresponding gate.

Passing unrelated historical gates remain sticky unless the refactor invalidates their code path.

## 20. Definition of done

The refactor is complete only when all of the following are true:

```text
[x] Current-Hand candidate enumeration exists in one shared runtime source.
[x] Player and Random current-Hand selections capture candidates at Execute time.
[x] Player selections use one shared interactive Presentation boundary.
[x] Effects no longer own Hand-enumeration / SelectionRequest construction logic.
[x] Burning Pact-style and Warcry-style Player selections use the same Action path.
[x] Random exhaust no longer captures candidates during BuildActions().
[x] ExactNClampToAvailable is explicit and not treated as universal future semantics.
[x] NoCandidates and InvalidRuntimeDependency are distinguishable.
[x] All failure dispositions in section 11.3 are implemented and tested.
[x] Presentation degradation cannot remove authoritative Gameplay selection.
[x] Queue insertion failure cannot silently skip mandatory selection.
[x] Pending requests freeze their candidate sets.
[x] Multi-selection-boundary writer handoff is verified.
[x] Each real Player decision has a distinct revision and exact display exposure gate.
[x] Random ordering and RNG-consumption compatibility are verified.
[x] No-history, catch-up and PresentationUnavailable modes are separately verified.
[x] Focused Automation passes.
[ ] Focused PIE ordering passes.
```

Only after these gates pass should continuous-card-play implementation proceed on top of the unified Selection boundary.

Implementation and automated validation are complete under the user's authorization; manual PIE remains pending. Approval or editing of this document alone does not satisfy any build, Automation, PIE or fast-input acceptance gate. Evidence and remaining manual gates are recorded below when available.

## 21. Implementation handoff

The runtime implementation uses `UCurrentHandSelectionSource` and `UDeferredSelectionAction`; `UDeferredHandSelectionAction` is a compatibility adapter only. Both production Effects explicitly bind the battle's interactive boundary capability, with no Queue Outer discovery. The resolver exposes a typed disposition while retaining its bool compatibility API. Direct/no-history UI also waits for its matching frozen baseline before exposing candidates.

### Manual fixture setup — USER ACTION REQUIRED

Use Native `L_BattleTest`. Preserve the user's saved card/map edits. For each case below, create an unsaved temporary CardData test copy in the editor session (do not overwrite BurningPact/Warcry), set cost to 0, target to None, type to Skill, destination to Exhaust, and apply the listed Effects in order. Keep Base/Upgraded counts identical. On the map's BattleManager, temporarily set `DebugStartingDeck` to four copies of that test definition, `OpeningHandDrawCount=2`, `PlayerTurnDrawCount=0`, and recording enabled. Run PIE and play one card. Do not save these temporary map/asset changes; restore the previous BattleManager values after the pass.

| Case | Effects in order | Observe |
|---|---|---|
| A | Draw(1), SelectExhaust(Player, 1) | Draw visibly enters Hand, then selection appears; choose the drawn card and observe its exhaust, followed by played-card cleanup. |
| B | Draw(1), SelectHandCardToDrawPileTop(1) | Draw visibly enters Hand before selection; selected card moves to draw pile, then played card cleans up. |
| C | SelectExhaust(Player, 1), Draw(1) | First select/exhaust the other Hand card; Draw follows, then cleanup. |
| D | Draw(2), SelectExhaust(Player, 1), SelectHandCardToDrawPileTop(1) | Two separately displayed choices, no stale selected highlight across the boundary, correct final cleanup. |

Let playback finish naturally for this ordering Gate. Fast-forward click reuse remains a known deferred input issue. Report A/B/C/D observations explicitly; Automation proves exact pile-top identity and revision/writer contracts, while this pass proves appearance and mouse interaction. This setup is a user-operated session fixture, not a claim that PIE has been run by the agent.

### Validation evidence — 2026-09-08

Current HEAD: `6fce24e39c53178b59561be31932659a0a542087`; implementation is uncommitted on top of this design-document commit. Existing user-edited BurningPact/map and untracked Warcry are preserved; no production assets were changed by this refactor.

AUTOMATED GATES:

- Standard bundled UE 5.8 project generation PASS. Initial build was blocked by Live Coding; after the user saved/closed UE Editor, Development Editor Win64 Build PASS. Runtime build log: `Saved/Logs/UnifiedSelectionBuild.log`.
- The seven prefixes in section 19 ran together: **43 tests, 42 passed (9 with expected rejection/degradation warnings), 1 failed**. All **12 unified-selection tests passed**. Evidence: `Saved/AutomationReports/UnifiedSelection/index.json`, `Saved/Logs/UnifiedSelection.log`.
- The single failure was the historical C0 `MultiExhaustRecordOrder` assumption of one envelope. Updated it to assert prefix/continuation identities, pre-choice frozen Hand, canonical multi-exhaust ordering and final cleanup. No runtime code changed after the first run.
- Rebuilt the affected test module successfully (`Saved/Logs/UnifiedSelectionFinalBuild.log`) and reran only `SlayTheSpireDemo.CardExpansion.Wave1CC0.Presentation.MultiExhaustRecordOrder`: **1 passed, 0 failed, 0 warnings**, exit code 0. Evidence: `Saved/AutomationReports/UnifiedSelectionRecordOrder/index.json`, `Saved/Logs/UnifiedSelectionRecordOrder.log`. The earlier passing Gates remain sticky; this is not a claim of a second full-suite run.
- Read-only review identified inconsistent pending cleanup and direct-mode early exposure; both were fixed before the passing unified test run, with explicit regression coverage. `git diff --check` PASS.

MANUAL PIE GATES:

- **USER ACTION REQUIRED**: section 21 cases A–D. No PIE/Blueprint visual or packaged-game acceptance is claimed. Do not seal this refactor or begin continuous-card-play work until the user provides the focused visual evidence.
- Fast-forward click consumption remains explicitly deferred to the later input work; automated selection acceptance does not close that issue.
