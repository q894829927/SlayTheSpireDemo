# Phase 6UI-A2E — Unified Blueprint Playback & PIE Acceptance

Date: **2026-08-22**

Status: **READY TO IMPLEMENT / A2D5 C++ GATE SEALED**.

UI-A2E closes the remaining player-visible gap in UI-A2. UI-A2A through UI-A2D establish the committed-presentation transport, reducers, status identity, terminal semantics and combined C++ acceptance. A2E is the unified Blueprint/UMG integration and PIE acceptance step that proves those committed historical facts are actually shown to the player in the intended order.

The mainline must not jump directly from A2D5 C++ acceptance to unfinished UI-A3 Preview work. A2 historical playback is the post-commit surface; A3 Preview is the pre-commit surface. They are intentionally validated in that order so A3 PIE does not simultaneously debug A2 playback and Preview behavior.

## 1. Locked development order

```text
A2D5 C++ / Automation closure     COMPLETE / SEALED
↓
synchronize AGENTS + A2/A2D validation evidence
↓
UI-A2E Unified Blueprint Playback NEXT
↓
UI-A2E PIE end-to-end acceptance
↓
UI-A2 COMPLETE / SEALED
↓
A3-1 Dynamic Text formally sealed
↓
A3-2 Target-Specific Current-State Preview
↓
A3-3 Energy + Target-Aware Legality
↓
A3-4 ViewModel Transient Preview Lifecycle
↓
A3-5 Minimal UMG + A2/A3 Combined PIE
```

Owner-confirmed A2D closure evidence:

```text
A2D5 focused                    PASS 6/6
A2D5-7 Terminal.ResolutionFault VALIDATED
Phase6R aggregate               PASS 100/100
Shipping exclusion              PASS
```

The C++ committed-presentation path is therefore closed. A2 itself remains open only because unified Blueprint/UMG historical playback and PIE acceptance have not yet been completed.

---

## 2. A2E responsibility

A2E adds no new Gameplay rule and no second presentation authority.

Required runtime/Blueprint flow:

```text
Gameplay Commit
↓
immutable FPresentationRecord
↓
immutable FPresentationResolutionEnvelope
↓
UBattlePresentationController
↓
WBP_BattleHUD.PlayPresentationRecord(Record, Token)
↓
Blueprint/UMG visual playback
↓
NotifyPresentationFinished(Token)
↓
Controller reducer advances WorkingPresentationSnapshot
↓
ViewModel exposes completed historical state
↓
next Record
↓
Envelope.FinalSnapshot reconciliation
↓
refresh newest live input bindings
↓
unlock only when authoritative Gameplay is request-eligible
```

Blueprint may read the frozen Record payload and animate it. Blueprint must not query mutable historical Gameplay state, recalculate authoritative values, mutate HP/Block/Energy/Status/Deck truth, decide Record order, or advance the Controller without the formal token callback.

### Historical display timing rule

The active animation uses the Record payload itself. The ViewModel represents the historical state that has already completed playback.

Example:

```text
Damage Record: HP 30 -> 21, IncomingDamage 9
↓
Blueprint displays hit/number/bar transition from the Record payload
↓
NotifyPresentationFinished(Token)
↓
Controller applies Damage reducer
↓
Working/ViewModel HP becomes 21
```

Do not first snap the ViewModel to 21 and then animate 30 -> 21 from live state.

---

## 3. A2E-1 — Unified Blueprint record routing

The concrete WBP playback router must handle every currently visible A2 Record type through the same `PlayPresentationRecord` contract:

```text
CardPlayed
EnergyChanged
Damage
BlockChanged
CardZoneChanged
DeckShuffled
StatusChanged
Victory
Defeat
ResolutionFault
```

First-pass visuals may be minimal and diagnostic. The purpose is deterministic visible playback and correct completion timing, not final VFX polish.

Recommended minimum visible treatment:

| Record | Minimum A2E treatment |
|---|---|
| CardPlayed | card leaves normal Hand pose / enters played state |
| EnergyChanged | Energy value transition |
| Damage | hit feedback + incoming damage value + HP/Block transition from frozen payload |
| BlockChanged | Block value transition |
| CardZoneChanged | visible card/zone-count transition |
| DeckShuffled | short shuffle cue and corresponding pile transition |
| StatusChanged | add/update/reduce/remove exact status row |
| Victory | Victory terminal overlay/treatment |
| Defeat | Defeat terminal overlay/treatment |
| ResolutionFault | explicit framework-fault terminal surface |

`PresentationUnavailable` is not a Record type and must use its existing fail-safe/UI-only surface rather than the `ResolutionFault` terminal route.

---

## 4. CardPlayed vs CardZoneChanged

Blueprint must preserve the committed semantic distinction:

```text
CardPlayed
= the card was formally played, paid its cost and left the current Hand display for resolution

CardZoneChanged(PlayArea -> Destination)
= effect resolution finished and the card reached Discard / Exhaust / Removed
```

For a normal damaging card the visible order may be:

```text
CardPlayed
→ Damage
→ StatusChanged...
→ CardZoneChanged
```

Do not move the card directly to Discard when `CardPlayed` arrives if the producer history says the final zone transition occurs later.

---

## 5. StatusChanged Blueprint identity

Status playback is driven entirely by frozen A2D payload data.

The visual identity is:

```text
TargetPresentationId
+ StatusId
+ RuntimeSequence
```

not `StatusId` alone.

Minimum behavior:

```text
Applied       0 -> N  : create/show exact runtime status row
Increased     A -> B  : update amount/description
Reduced       A -> B  : update amount/description
TurnEndDecay  A -> B  : update amount/description with decay feedback if desired
Removed       A -> 0  : remove exact runtime status row
```

Use the frozen `DisplayName`, `DescriptionBefore`, `DescriptionAfter` and icon/atlas metadata. Do not query `UStatusInstance` or `UStatusData` during historical playback.

---

## 6. Terminal timing

Terminal overlays must not appear merely because authoritative Gameplay already reached its terminal state.

Required visible timing:

```text
prior Records complete
↓
start Victory / Defeat / ResolutionFault playback
↓
WorkingSnapshot remains non-terminal
↓
ViewModel remains Resolving + input locked
↓
NotifyPresentationFinished(TerminalToken)
↓
terminal reducer applies
↓
ViewModel becomes Terminal
↓
Envelope FinalSnapshot reconciles exactly
```

Lethal facts must already be visible before the terminal record completes. Duplicate/stale terminal callbacks remain NoOp through the existing token/generation checks.

---

## 7. A2E-2 — PIE end-to-end acceptance

At minimum, run these real WBP/PIE scenarios.

### Scenario A — ordinary card damage

```text
select Strike
→ select Enemy
→ CardPlayed
→ Damage
→ CardZoneChanged
→ FinalSnapshot
→ input unlock
```

Verify there is no immediate jump to the final snapshot before the visible records complete.

### Scenario B — card + statuses

Use an Uppercut-like card or equivalent configured effects:

```text
CardPlayed
→ Damage
→ Weak Applied
→ Vulnerable Applied
→ CardZoneChanged
```

A repeated application must update the existing exact status rows rather than create duplicate rows for the same runtime identity.

### Scenario C — full EndTurn macro envelope

Exercise the actual configured facts, including as applicable:

```text
EnergyChanged end-turn clear
→ Hand discard Records
→ StatusChanged TurnEndDecay
→ Enemy Block clear
→ Enemy Damage
→ EnergyChanged player-turn restore
→ Player Block clear
→ DeckShuffled
→ DrawPile -> Hand Records
```

Verify one Envelope may contain many Record types while preserving producer order and token-by-token playback.

### Scenario D — Victory and Defeat

Verify lethal state becomes visible first, terminal treatment follows, and the terminal callback completes the Envelope exactly once.

### Scenario E — ResolutionFault and PresentationUnavailable separation

Verify a real framework ResolutionFault reaches the fault terminal surface. Separately verify Presentation-only failure reaches `PresentationUnavailable`, keeps Gameplay ownership unchanged and is not rendered as a Gameplay ResolutionFault.

---

## 8. Input unlock acceptance

The acceptance criterion is not merely that a button eventually becomes clickable.

Required order:

```text
Gameplay may already be stable
↓
Presentation backlog still active
↓
normal input remains locked
↓
last Record callback / safe fallback
↓
Apply Envelope.FinalSnapshot
↓
Controller catches up to newest display revision
↓
RefreshLiveInputBindingsIfCaughtUp
↓
formal Gameplay Query/Request state is eligible
↓
unlock normal input
```

While playback is active, repeated card/EndTurn input must not start another player request.

---

## 9. UI-A2E completion criteria

UI-A2 is not complete until all of the following are true:

```text
A2 committed C++/Automation contracts are validated
all visible A2 Record types have a coherent WBP playback path
Blueprint historical playback does not query mutable Gameplay
async playback owns exactly one completion token callback
producer Record order is preserved
WorkingSnapshot advances only after each completed Record
FinalSnapshot reconciles exactly at Envelope completion
input stays locked until Controller catch-up + live binding refresh
Victory / Defeat / ResolutionFault timing is correct
PresentationUnavailable remains distinct from ResolutionFault
PIE ordinary card/status/turn-cycle/terminal scenarios pass
```

Only then mark `UI-A2 COMPLETE / SEALED` and resume unfinished UI-A3 work.

---

# UI-A3 follow-up roadmap

## 10. Core A3 boundary

```text
A3
= before submission: display read-only current-state Operation resolution values

A2
= after submission: play facts that actually committed
```

A3 Preview must not become a second Gameplay simulation engine.

Do not describe the target-specific value as an "exact final result". The locked name is:

```text
Target-Specific Current-State Preview
```

It answers:

> For this BattleId/StateRevision, Card and current Target, what does this supported Operation resolve to if sent through the current read-only Gameplay pipeline now?

It does not promise the final outcome of the complete card Resolution.

For a fixed multi-hit Damage effect, `7 x 2` means two independent attack intents each currently resolving incoming damage 7. It does not guarantee 14 final HP loss because later hits may observe state changed by earlier commits/reactions.

---

## 11. A3-1 — Dynamic Text

The existing Dynamic Text slice is sealed at 8/8 with its already-recorded DataAsset/PIE/package evidence.

Card-face enemy-target Damage continues to omit one concrete Enemy target. It may include source-side modifiers while target-specific modifiers are reserved for A3-2.

---

## 12. A3-2 — Gameplay-owned Target-Specific Current-State Preview

Preview construction must be owned by a Gameplay/read Query boundary, conceptually:

```cpp
bool ABattleManager::TryBuildImmediateCardPreview(
    const UCardInstance* Card,
    const ACombatant* Target,
    FImmediateCardPreview& OutPreview
) const;
```

The exact return type may be refined, but ownership is fixed: ViewModel/UMG do not iterate `UCardEffect` objects and do not calculate Damage/Block formulas.

Gameplay-side preview construction owns:

```text
QueryCardPlayability when no concrete target is bound
QueryPlayCard when a target is bound
read-only Effect traversal
Damage/Block typed Modifier Pipeline evaluation
current effective cost / Energy result
stable BattleId + StateRevision identity
```

The first preview surface covers only:

```text
target-specific Damage
Self Block
Energy result
legality / failure reason
```

Status merge outcomes, concrete draw results, Shuffle, Trigger/Relic chains and terminal prediction stay out of this first version.

---

## 13. Preview data model — operation array

Do not assume one Damage and one Block effect per card.

Use a minimal flat Blueprint-friendly operation array, conceptually:

```text
FImmediateCardPreview
├── BattleId / StateRevision
├── CardRuntimeId
├── SourcePresentationId
├── TargetPresentationId
├── Validation / FailureReason
├── EnergyBefore / EffectiveCost / optional EnergyAfter
└── Operations[]

FImmediatePreviewOperation
├── EffectIndex
├── SemanticArgumentName
├── Type = Damage / Block
├── ResolvedAmount
└── HitCount
```

No `FInstancedStruct` is required.

Damage `ResolvedAmount` means **resolved incoming damage per hit before Block absorption**, not guaranteed HP loss. UI may display "Damage 9" but must not render `-9 HP` or an HP ghost bar from this first preview contract.

---

## 14. A3-3 — Energy + target-aware legality

Before target selection:

```text
QueryCardPlayability(Card)
```

may expose current failures such as WrongTurn, NotEnoughEnergy or CardNoLongerInHand without treating the absence of a target as `InvalidTarget`.

After a concrete target is bound:

```text
QueryPlayCard(Card, Target)
```

owns target legality.

Example:

```text
Energy 3, effective Cost 2
→ EnergyBefore 3
→ EnergyAfter 1

Energy 1, effective Cost 2
→ NotEnoughEnergy
→ no fabricated EnergyAfter = -1
```

---

## 15. A3-4 — ViewModel transient lifecycle

Preview-target interaction must have its own semantics:

```text
SetPreviewTargetById(TargetId)
ClearPreviewTarget()
```

Do not reuse combatant/status inspection events such as `OnInspectRequested`.

Mouse hover and keyboard/controller focus may both nominate the same PreviewTarget candidate, but inspection and preview lifecycles remain separate.

First-version revision policy is deliberately conservative:

```text
BattleId changes OR StateRevision changes
→ clear selection
→ clear legal targets
→ clear preview target
→ clear ImmediatePreview
→ wait for Controller catch-up / live binding refresh
```

Do not retain or auto-rebuild a selection across revisions in the first implementation.

Also clear preview on cancel, accepted request, terminal/fault transition, target unhover/unfocus and any loss of the required live binding.

---

## 16. A3-5 — Minimal UMG + combined A2/A3 PIE

After A2 is already independently PIE-validated:

```text
select Strike
→ target candidate highlighted
→ hover/focus Enemy
→ current-state target Damage preview appears
→ click/submit target
→ Preview clears
→ A2 committed CardPlayed/Damage/... playback takes over
```

This combined PIE verifies the ownership handoff rather than debugging both systems at once.

---

## 17. Explicitly deferred from first A3 completion

```text
multi-enemy Gameplay/read-model expansion
HP ghost bars
predicted final HP loss
full-card final-result simulation
Trigger/reaction simulation
Relic reaction simulation
Status merge-result preview
specific cards drawn
Shuffle result prediction
Victory/Defeat prediction
cross-revision retained selection
advanced target arrows / polished VFX
```

Those remain future multi-enemy work, UI-B Advanced UX/Preview, or Presentation Polish according to their actual requirement.

The current project has one Player and one current Enemy in its formal read/target model; A3 must not introduce multi-enemy architecture merely to test target preview variation. Use a single Enemy whose modifier state changes across a new StateRevision to test stale-preview invalidation and recalculation.
