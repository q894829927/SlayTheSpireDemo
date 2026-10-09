# Architecture

G9-E keeps input retirement synchronous and separate from readiness consumption.
Discarding pending input immediately updates Native draft, pointer and gesture
surfaces. A valid Session invalidation rechecks exact intent credentials;
binding teardown drops pending requests. Ordinary Session-less DirectBaseline
Ready preserves valid same-turn FIFO and EndTurn authority. Neither notification
consumes input or pumps Gameplay.

HUD model/controller replacement detaches the old binding before tracked playback
and cosmetic cleanup, then installs the new owner. Input is rejected during that
transition. Binding generations let a reentrant newer model win; strong lifetime
pins cover old/new objects across callbacks and GC. Old timers and completions
cannot mutate the new surface. Hosted Blocking completion finds the unique job
by the current exact playback token and then validates its visual generation.
IncomingHandAttachment is the sole temporary draw owner; no duplicate active
draw Widget or active visual-token mirror is maintained. Native Blocking fallback
remains required. E integration gates are accepted; overall G9 sealing is F.

G9-D2 uses a validated, default-on Native detached arrival transaction. The Controller proves the
entire remaining current sealed Envelope on copied snapshot/history using the
shared card reducer, with exactly one supported destination for the same play
occurrence. It never commits a future record during preflight. New admission
requires both D2 arrival and D1 destination policies. Preparation freezes the
source pose and complete journey in a hidden Native job; formal CardPlayed
snapshot/history and its independent destination receipt install together before
publication. Only the exact committed receipt permits visual activation.
Preparation decline retains Blocking; post-commit visual failure cannot replay
history. The destination consumes its formal correlation at its own cursor even
after visual loss or policy disable. An entering job remembers that commitment
and starts its tail only after arrival completes, carrying elapsed overshoot.
Private clocks do not complete the Controller or submit queued input. Same-runtime
formal Hand ownership retires the old visual; old receipts cannot affect a new
generation. All D2 visual gates are accepted. Native startup enables D2 by
default; runtime disable retains Blocking admission and necessary formal
correlations. Generic Base/Controller defaults remain opt-in.

G9-D1 destination tails are an independent, validated default-on Native policy
with a runtime disable path. The Controller proves a copied sealed card record
with candidate snapshot/history,
prepares a bounded visual receipt, then installs both formal candidates before
publication. Only that current committed receipt authorizes activation; prepared
or stale receipts cannot start animation. Publication/activation reentry rechecks
the exact cursor, Widget and Session. Post-commit failures drop only cosmetics.
The frame pins Controller/Widget lifetimes across callbacks and never retains a
reference into an Envelope that a callback may free.

Native tails reuse C's GC-safe jobs with frozen normalized endpoints and their
own elapsed time; they have no Blocking timer, tracked completion, input debt or
Gameplay request. New formal Hand ownership wins over an old tail, including
normal redraw of the same RuntimeId. Selection hiding excludes detached tails.
Disable retires tails/preparations and pending input without replacing Session,
turn authority or unresolved formal correlations. Skip/recovery clears retained
visuals even with no active Blocking unit. D1 alone retains Blocking arrival;
explicit D2 admission uses the transaction above.

G9-C Native played cards use a HUD-owned, noninteractive DetachedCardVFXHost.
Each GC-safe job owns its frozen clone, exact play occurrence plus independent
visual generation, binding/surface identity, geometry, elapsed time and phase.
Blocking completion still requires the current record token and visual token;
animation Tick neither reduces history nor requests Gameplay. Visible formal
Hand ownership retires an older visual of the same RuntimeId even when input is
disabled. Session invalidation, Skip/recovery and destruction clean private jobs.
Preparation is bounded at 32 and has no formal side effects. Resource decline
retains the sealed Native Blocking fallback; a bound Controller with invalid
history/lifecycle never uses that fallback. Standalone renderer tests retain the
existing R8 contract. D1/D2 nonblocking timing is not enabled by this migration.

G9-C card history uses one atomic pure reducer for Blocking commits, record
preflight and Group candidates. Controller owns exact unresolved play occurrences;
normal envelope completion retains them across mandatory-choice continuation.
Destination consumes one correlation; cosmetic loss cannot consume it. Recovery
clears collapsed correlations before publication without reusing generations.

Validated Native startup enables amended G9-B buffered input by default.
Runtime disable retains the G8-B fallback; generic/test Base HUDs remain disabled
until explicitly enabled. See `G9BClosure.md` for activation evidence.

Amended G9-B input is one HUD-owned draft / confirmed FIFO / EndTurn arbiter.
Frozen display authorizes busy-time target affordances; only the caught-up
ViewModel boundary resolves current bindings and requests Gameplay. Revision
records intent provenance, while Battle/turn/Session/generation fence identity.
Mandatory choices and destructive display/lifetime boundaries retire pending
inputs. NativeTick cannot submit them.

EndTurn accepts exact authority, preserves earlier confirmed commands and their
busy retries, then executes after them; it rejects later card additions and
cancels only the unconfirmed draft/FastInput retry.
Its submitted-turn receipt survives pending-input cleanup and prevents accepting
future-turn commands while old history is still visible. A different player turn
must be visibly caught up and normally ready before fresh input is available.
At that proven boundary the prior-turn receipt retires once. A later card's
Resolving state must not reactivate a completed fence. The shared event-driven
input evaluation owns this lifecycle transition.
Any ordinary selection, including attack aiming, suppresses other cards' hover
lift/scale while retaining resting-strip hit routing and slot identity.
Native card clicks and holds share one press request: formal pointer cards
forward native preview press/reply to the HUD for immediate capture; unclaimed
attack/mandatory presses retain UButton OnPressed. Capture never waits for the
next Slate pass and moves never re-capture.
Button release has no second selection effect. The HUD owns an exact pointer-card
drag gesture and may confirm once on release outside current Hand geometry;
inside release keeps following, single-target Attacks only aim. No Tick request
or target guess. See `NativeCardDragRelease.md` for the current acceptance scope.
See `NativeInputHoverAndEndTurnRevision.md` for the 2026-10-06 user revision.

TurnEndDiscard uses explicit action-local metadata, with canonical IDs frozen
from the complete turn-end Hand. The generic Controller/Base/card-transition
engine accepts Hand sources without creating a Selection lifecycle. All clones
prepare before activation, move together from their own frozen fan geometry,
and retain input-disabled Hidden historical slots. Controller suppression is
scoped to exact committed Group records; future members reduce only at their
normal cursor. Invalid geometry/metadata/interference declines to serial
playback. Cancel/timeout/disable retires all visuals before final-snapshot
recovery and never faults Gameplay. Native activation shares the opt-in G9 flag.

Relic scheduling amendment: Status reactions enter the ActionQueue front; Relic
event reactions enter its back, atomically validated together. Card continuations
retain dependency order, so actual Relic gains follow the current card's effects
and destination. Future player card requests remain outside ActionQueue until
that command/relic work and Blocking playback finish. This replaces Relic-before-
RetryDraw scheduling; pre-commit Modifiers remain unchanged. See
`QueuedCardPlayAndRelicTimingAmendment.md` (2026-10-05).

Native HUD publications enter one final C++ dispatcher with restricted hooks.
The HUD privately prepares and commits frozen membership; its GC-rooted registry
is scoped by BattleId/RuntimeId. Incoming draw adoption requires matching frozen
identity/index, exact playback token and HUD surface lifetime. UBattleHandFanPanel
uses a dedicated Hand Slot and private Slate SPanel to arrange frozen ranks from
the current allotted size on the first layout pass. Paint order uses explicit
layer then frozen rank. A moving Hand card holds base geometry under its exact
playback token; other cards follow viewport changes. Hover writes only eligible
Hand-card render transforms/layers. No layout dirtiness or Tick repair remains.
Animation completion retains an exact cancellable receipt until tracked playback
forwards or cancels it; temporary draw adoption and retained PlayArea cleanup
therefore cover the deferred-completion window too. See
[Native Hand refactor](NativeHandStructureRefactor.md) for migration and evidence.

This document is the durable architectural overview. The Chinese synchronized version is [`Architecture.zh-CN.md`](Architecture.zh-CN.md). Directory-level `AGENTS.md` files define implementation rules. The sealed UI-A2 contract is recorded in the Phase 6UI-A2 documents; current phase status is authoritative in `docs/DevelopmentPhases.md`, current Native UI details are in `docs/Phase6UIA2NNativeHUDRefactor.md` and `docs/WBPSavedBlueprintSnapshot.md`, and current selection-presentation details are in the `SelectionPresentationG*` and card-selection constraint documents. Older A2 implementation/validation files preserve phase evidence and must not be read as current pending work.

## Current-Hand Selection execution

The unified selection contract is defined in `docs/CardSelectionRefactorConstraints.md`. Effects compose a runtime CandidateSource, count/mode/cancel intent and an authored Continuation. One shared deferred Action captures authoritative Hand order at Execute time. Player mode receives an explicitly injected interactive-boundary capability, advances a decision revision, seals the committed prefix and rebinds the remaining tail before entering pending Selection. Random mode uses the same capture but no pending request or interactive segment, preserving canonical result order and authoritative RNG consumption.

Each real Player decision is a distinct `(BattleId, StateRevision)` display boundary, including consecutive identical choices. Recorded UI exposes candidates only at exact frozen catch-up; no-history mode publishes the matching frozen baseline without history. PresentationUnavailable retains the mandatory Gameplay choice while preserving the existing disabled-input/error surface. Invalid submissions keep a valid request pending; internal dependency, BeginSelection, Continuation or insertion failure follows the Gameplay framework fault policy. Actions insert dependent batches and Finish; neither candidates nor Continuations pump the queue.

### Native selected-card visual ownership (G5)

Production Selection uses a persistent Canvas-root SelectionArea Overlay. Each
selected RuntimeId has one visible frozen-data counterpart there; its historical
Hand widget remains an input-disabled Hidden structural slot. Confirm preserves
the SelectionArea object and its position, then G4 SingleRecord reparents that
same object to the transition surface. Hand reconciliation does not position or
recreate confirmed visuals. Safe simultaneous Group playback was completed in G6.

The ViewModel commits Confirm after accepted submission, holds re-entrant
snapshots/outcomes during that transaction, and arms exact G1 recorded/direct
receipts. One Native HUD synchronizes affected surfaces before public ownership
or snapshot notifications; external multicast registration order is irrelevant.
Missing correlation at a newer Ready edge is explicit UI-only unavailable
recovery. It never manufactures a completion watermark or Gameplay fault.
Scope and acceptance: `docs/SelectionPresentationG5Execution.md` and `docs/SelectionPresentationG6Execution.md`.

## 1. Battle Execution

```text
CardData / CardInstance
→ CardEffect
→ BattleAction
→ BattleActionQueue
→ typed Operation Spec
→ Modifier Pipeline
→ Commit
→ BattleEvent
→ Trigger collection
→ Reaction BattleActions
→ BattleActionQueue
```

`BattleStateMachine` controls macro turn flow. `BattleActionQueue` controls deterministic execution order. Modifiers change an operation before commit. Events describe facts after commit. Triggers build queued reactions.

Complex behavior must emerge from generic composition. Pommel Strike knows configured Damage/Draw effects; Defend knows Block; DeckRuntime knows zones/draw/shuffle; Sundial knows shuffle events. None should know concrete combinations.

## 2. State Ownership

### BattleManager

Owns battle orchestration, turn transitions, battle-scoped identity/RNG allocation, public Query/Request boundaries and stable read publication.

G9-A adds a read-only Gameplay player-turn identity, `BattleId + PlayerTurnSerial`.
The serial resets at battle setup and increments once per successful formal PlayerTurn
entry; same-turn StateRevision changes do not change it. It is independent of
Presentation sessions. G9-A introduced the shadow evaluator. G9-B adds opt-in
production consumption; default activation still depends on its visual gate.

When enabled, one HUD-owned arbiter stores at most one physical player intent.
An accepted exact-turn EndTurn takes priority over buffered card selection and
Native FastInput retry. Its ViewModel forwarding boundary calls the existing
Gameplay Request without requiring displayed history to be current and without
rebuilding that history from live state. Card selection buffers only an exact
already-sealed Presentation target and still requires fresh Confirm/Target input.
Ready, Controller and ViewModel notifications drive coalesced non-reentrant
consumption; cosmetic NativeTick does not poll Gameplay readiness. Formal Hand
Widgets retain `(BattleId, RuntimeId)` identity, frozen order and hidden structural
slots; hover changes transforms/layers separately from structural layout.
Card playback remains Blocking. Ordinary Skill/Power/untargeted-Attack drafts
can follow the mouse through Hand render transforms, without reparenting formal
Widgets or changing panel layout. An optional queued cosmetic source receipt
feeds the exact accepted CardPlayed visual; Gameplay never reads it. Source
geometry is established before first Slate paint, not recovered by Tick. This
is independent of mandatory SelectionArea ownership. See
`docs/NativePointerCardPresentation.md` and `docs/SelectionPresentationG9BExecution.md`.

### Combatants

Own authoritative HP and Block. Typed CommitResults carry before/after facts to Actions without making Combatants depend on Presentation.

### DeckRuntime

Owns DrawPile, Hand, DiscardPile, ExhaustPile and PlayArea truth. DrawPile end is top. Runtime card identity is stable `UCardInstance` identity; RuntimeId is presentation/debug identity.

### StatusContainer

Owns authoritative Status membership and merge/create decisions. `UStatusInstance` owns mutable Amount, RuntimeSequence and Owner; `UStatusData` is immutable definition data.

### RelicContainer

Owns battle-scoped Relic membership. `URelicData` is immutable definition data; `URelicInstance` owns mutable runtime state such as Sundial Counter and shares the battle-wide RuntimeSequence domain with Status runtime sources.

## 3. BattleActionQueue

Only one authoritative Action executes at a time. Ordering and completion are explicit. Actions may enqueue dependencies but never drive queue advancement.

Dependent batches for one logical chain are inserted before the current action finishes. Nested reactions use queued depth-first semantics. Queue faults enter at safe points, broadcast once, suppress normal QueueEmpty and reject further mutation.

QueueEmpty is an observable non-reentrant boundary. BattleManager defers macro turn continuation until all observers return.

## 4. Modifier Pipelines

```text
ActionQueue       → execution timing/order
Modifier Pipeline → pre-commit modification/interception/override/clamp
BattleEvent       → post-commit fact
Trigger           → post-commit reaction that builds Actions
```

Use typed specs such as `FDamageSpec` and `FBlockSpec`. Avoid a universal modifier context.

Deterministic ordering within a domain is:

```text
Phase → Priority → RuntimeSequence → LocalModifierIndex
```

Ratio arithmetic uses explicit integer numerator/denominator, safe intermediates and floors after every modifier.

## 5. Events and Triggers

A BattleEvent is a short-lived immutable-by-contract value fact. Dispatch/Trigger code must not cache its references.

Trigger sources are collected on demand. Eligibility uses snapshot semantics; built Actions validate live state at Execute-time. Status and Relic Trigger ordering is `Priority → RuntimeSequence → LocalTriggerIndex`.

Triggers are read-only builders. They never mutate Gameplay or drive the queue. Event emission follows a successful commit, and reaction batches are atomically inserted before the source action finishes.

`FDeckShuffledEvent` occurs only after a committed gameplay `UShuffleDeckAction` and before the remaining bulk-draw continuation. A committed shuffle normally moves DiscardPile cards to an empty DrawPile, but `MovedCardCount` may be `0` when that ShuffleAction was already planned by an earlier bulk-draw step before the available DrawPile cards were consumed. A fresh draw request against `DrawPile=0 / DiscardPile=0` does not schedule a ShuffleAction. Initial battle setup randomization is normalization, not a Gameplay event.

## 6. Card and Deck Resolution

```text
UCardData
→ UCardInstance
→ UPlayCardAction
→ UCardEffect::BuildActions() const
→ effect Actions
→ UFinishCardPlayAction
→ Execute-time destination resolution
```

Effects are immutable shared definitions that capture base intent. Mutable-state-dependent outcomes resolve at Action Execute-time. FinishCardPlay delegates authoritative movement to DeckRuntime.

Card descriptions default to localized sentences contributed by each Effect in array order, followed by the card's Exhaust keyword when applicable. Each contribution resolves read-only preview arguments independently; target-specific overrides are scoped by EffectIndex. Shared definitions remain immutable and the resulting FText follows the existing frozen snapshot pipeline. Explicit custom-template mode retains the old authored Description contract. See `docs/AutomaticCardDescriptions.md`.

Draw uses a two-level Action model:

```text
UDrawCardEffect(DrawCount = N)
→ UDrawCardsAction(N)                 // bulk intent / orchestration
   ├─ UDrawCardAction                 // atomic one-card DrawPile -> Hand commit
   ├─ UShuffleDeckAction              // committed shuffle + DeckShuffled event
   └─ UDrawCardsAction(Remaining)     // continue the same bulk request
```

`UDrawCardsAction` owns `RemainingDraws`, evaluates live Hand capacity and pile counts, and plans deterministic continuation batches. It never mutates DeckRuntime directly. `UDrawCardAction` performs exactly one card movement and never decides to shuffle or retry.

A fresh bulk request with both DrawPile and DiscardPile empty ends immediately. If a bulk request still owes cards after consuming the currently available DrawPile, it pre-plans `ShuffleDeckAction → DrawCardsAction(Remaining)`. Therefore a previously planned ShuffleAction may later execute with both piles empty and commit `MovedCardCount=0`; this is a real gameplay shuffle fact, not a general “empty draw means shuffle” rule.

Draw/shuffle never execute synchronously outside the queue. Battle RNG is initialized once and consumed deterministically by committed non-empty shuffles.

## 7. Presentation Architecture

Gameplay and Presentation are independent timelines:

```text
Gameplay validation / Request
→ Begin Presentation Resolution
→ BattleActionQueue
→ Gameplay Commit
→ immutable typed Presentation Records
→ Gameplay/macro stability
→ freeze exact FPresentationStateSnapshot
→ seal immutable Resolution Envelope
→ deferred public delivery
→ BattlePresentationController
→ ViewModel working state
→ UMG playback
→ apply matching Envelope.FinalSnapshot
```

A sealed Envelope owns one `BattleId`, `ResolutionId`, Origin, ordered Records, FinalStateRevision and matching FinalSnapshot. Historical playback never reconstructs the past from mutable Gameplay.

Historical committed-card projection has one presentation-only boundary:

```text
FPresentationCardSnapshot
→ PresentationCardView::MakePresentationOnlyCardView
→ FBattleHUDCardView
```

That mapper is only for frozen Record/presentation cards and therefore always produces a presentation-only, non-gameplay-playable view. `FCardReadView → FBattleHUDCardView` remains a separate stable current-state freeze path owned by `TryFreezePresentationStateSnapshot`; it carries current Gameplay legality and must not be routed through the presentation-only mapper. Projection completeness and identity matching are separate concerns: display fields such as `RichDescription` must survive the projection, while historical identity predicates may intentionally compare only stable identity fields.

Internal seal and public notification are distinct. Seal releases the builder before another Resolution begins; `OnPresentationResolutionReady` and `OnReadStateReady` remain deferred until after an accepted Request returns.

See `docs/Phase6UIA2Implementation.md` for the complete contract and `docs/Phase6UIA2EImplementation.md` for current Blueprint/PIE closure.

## 8. MVVM Boundaries

```text
MODEL
BattleManager / Combatants / DeckRuntime / Status runtime / Relic runtime / Enemy Intent / BattleActionQueue

VIEWMODEL
frozen player-facing display state
formal Request forwarding
presentation-only selection/focus
latest-only live input bindings

VIEW
UMG Widgets
```

`FBattleReadSnapshot` is a coherent current Gameplay/read structure and may hold weak runtime references. `FPresentationStateSnapshot` is the frozen display model for one exact revision and has no mutable Gameplay dependency.

Latest-only runtime bindings map current RuntimeId/TargetId to weak objects solely for formal Request submission after Presentation catches up. They are not historical state.

## 9. Public Read and Request Boundary

Normal UI uses formal Query/Request APIs. Query is advisory; Request revalidates current authoritative state. `AcceptedForResolution` does not mean playback has completed.

Widgets do not use QueueEmpty/idle as their public completion protocol. BattleManager publishes coherent battle-level Ready edges. Public notifications never fire re-entrantly before the originating accepted Request returns.
