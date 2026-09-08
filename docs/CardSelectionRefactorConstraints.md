# Card Selection Refactor Constraints

Status: DESIGN CONSTRAINT / NOT IMPLEMENTED

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
- continuation execution through the existing `SelectionRequestAction → SelectionResolver → AuthoredContinuation` chain.

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
SelectionRequestAction
    begins and holds pending selection
    waits for validated result
    inserts continuation Actions
    finishes itself
        ↓
SelectionResolver
    owns / validates pending request and result
        ↓
AuthoredContinuation
    answers: "what happens after this result?"
```

No individual Effect may reimplement this full pipeline.

## 5. CandidateSource contract

### 5.1 Runtime lifetime

A CandidateSource MUST be a one-resolution runtime object.

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

### 11.2 Queue insertion failure

If the framework cannot enqueue the required `SelectionRequestAction` or validated continuation batch, this is a Gameplay framework failure.

The implementation MUST NOT silently `Finish()` and continue later Effects as though the mandatory selection succeeded.

It MUST request an appropriate resolution fault according to existing queue fault policy.

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
- exact-N count clamped when candidates are fewer than requested;
- zero candidates as legal no-op under the current policy;
- invalid runtime dependency distinguished from zero candidates;
- Random selection uses post-prefix current Hand and remains deterministic under DeckRuntime RNG;
- one card containing two player Selections establishes two valid interactive boundaries;
- Presentation writer is preserved/rebound correctly through final played-card cleanup;
- Presentation unavailable/degraded does not suppress authoritative Gameplay Selection;
- queue insertion failure requests a resolution fault rather than silently skipping mandatory interaction;
- pending candidate set remains frozen after `BeginSelection`;
- existing Burning Pact behavior when authored as Selection-before-Draw remains intact;
- existing Warcry Draw-before-Selection observable ordering remains intact.

Fast-forward click consumption should be covered by the later input/continuous-play work, but its known requirement must remain documented.

## 19. Validation budget and sealing

This refactor changes shared Selection execution and invalidates prior validation for touched paths.

Required gates after implementation:

```text
Development Editor Win64 Build
→ focused unified-selection Automation
→ existing affected C0/C1 selection regression filters
→ focused PIE for Player selection ordering / presentation catch-up
```

Do not claim PASS until evidence exists for the corresponding gate.

Passing unrelated historical gates remain sticky unless the refactor invalidates their code path.

## 20. Definition of done

The refactor is complete only when all of the following are true:

```text
[ ] Current-Hand candidate enumeration exists in one shared runtime source.
[ ] Player and Random current-Hand selections capture candidates at Execute time.
[ ] Player selections use one shared interactive Presentation boundary.
[ ] Effects no longer own Hand-enumeration / SelectionRequest construction logic.
[ ] Burning Pact-style and Warcry-style Player selections use the same Action path.
[ ] Random exhaust no longer captures candidates during BuildActions().
[ ] ExactNClampToAvailable is explicit and not treated as universal future semantics.
[ ] NoCandidates and InvalidRuntimeDependency are distinguishable.
[ ] Presentation degradation cannot remove authoritative Gameplay selection.
[ ] Queue insertion failure cannot silently skip mandatory selection.
[ ] Pending requests freeze their candidate sets.
[ ] Multi-selection-boundary writer handoff is verified.
[ ] Focused Automation passes.
[ ] Focused PIE ordering passes.
```

Only after these gates pass should continuous-card-play implementation proceed on top of the unified Selection boundary.
