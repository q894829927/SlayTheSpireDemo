# Card Expansion — Wave 1C-C1 Configurable Hand→DrawPileTop Amendment

Date: **2026-09-08**

Status:

```text
USER-AUTHORIZED AMENDMENT / IMPLEMENTED / MERGED TO main VIA PR #18
PRESENTATION CONTRACT SUPERSEDED INTO SHARED G5 OWNERSHIP PATH
FINAL STANDALONE C1 SEAL NOT RECORDED
```

Merge evidence:

```text
PR #18
ffbc164905a875bea5c9ab3dfe0a07df5068b8cc
```

This amendment supersedes the fixed-exact-one count statements and any earlier C1 Presentation statement that requires an in-place Hand→DrawPileTop fade in:

```text
docs/CardExpansionWave1CC1WarcryHandToDrawPileTopPlan.md
```

The shared player-selection Presentation contract is now defined by:

```text
docs/CardSelectionPresentationConstraints.md
```

That document is authoritative for Selection UI confirmation, selected-card visual handoff, destination animation ownership, input-transition consumption, and reuse rules. The G4+G5 production SelectionArea path was subsequently user-validated and sealed; this amendment therefore remains the C1 configuration/Gameplay contract while the shared G5/G6 documents own current visual-lifecycle status.

The user explicitly requires the reusable Effect itself to support a Blueprint-authored number of selected Hand cards.

## Effect contract

The reusable Effect remains:

```text
USelectHandCardToDrawPileTopEffect
```

and exposes explicit Base / Upgraded values:

```text
BaseSelectionCount     >= 0
UpgradedSelectionCount >= 0
```

No sentinel/fallback semantics are used. If upgrade does not change the value, author the same value in both fields.

Runtime exact-N rule:

```text
RequiredCount = Min(EffectiveSelectionCount, CurrentValidHandCandidateCount)
```

with:

```text
EffectiveSelectionCount == 0
→ legal no-op
→ no pending Selection
→ no zone mutation
→ later authored Effects / normal card cleanup continue

no current Hand candidates
→ legal no-op

EffectiveSelectionCount > current Hand candidates
→ clamp to all current valid Hand candidates
```

Selection remains:

```text
Player
exact-N
CancelPolicy = Forbidden
candidates = valid CURRENT Hand CardInstances captured by the shared DeferredSelectionAction at Execute time
```

No Random mode, optional range, arbitrary insertion index or generic zone-selection API is authorized by this amendment.

## Shared Selection execution contract

C1 uses the unified Selection pipeline defined by `docs/CardSelectionRefactorConstraints.md`:

```text
prior Draw / Shuffle work completes
→ UCurrentHandSelectionSource captures CURRENT Hand at Execute time
→ UDeferredSelectionAction establishes the shared Player interactive boundary
→ ordinary USelectionRequestAction / USelectionResolver pending flow
→ explicit UI confirmation submits the validated SelectionResult
→ authored continuation
→ UMoveHandCardToDrawPileTopAction x N
→ exact Hand -> DrawPile commits
→ later authored Effects / played-card cleanup continue normally
```

C1 MUST NOT reintroduce a separate Hand-selection Action, eager candidate capture in `BuildActions()`, or card-specific Selection path.

## Multi-card draw-pile-top ordering

The selected set is canonicalized through the existing Gameplay selection facade to current Hand candidate order. The continuation preserves that canonical result order and builds one exact move Action per selected card.

For canonical selected cards:

```text
A, B, C
```

execution is:

```text
A -> DrawPile.Add(A)
B -> DrawPile.Add(B)
C -> DrawPile.Add(C)
```

therefore final top-to-bottom order begins:

```text
C
B
A
```

This is deterministic and does not make UI click order an implicit Gameplay ordering control.

Presentation consumes the resulting committed zone records in canonical committed order. UI click order may affect transient highlight history only; it MUST NOT redefine Gameplay or DrawPile ordering.

## Description contract

The Effect has a semantic, non-None preview argument default:

```text
DescriptionArgumentName = DrawPileTopCount
```

Automatic card description is fixed Chinese text:

```text
将手牌中的 {Count} 张牌放到你的抽牌堆顶部。
```

`Count` resolves independently per Effect instance:

```text
Base card     -> BaseSelectionCount
Upgraded card -> UpgradedSelectionCount
```

Example:

```text
BaseSelectionCount = 1
UpgradedSelectionCount = 3

Base     -> 将手牌中的 1 张牌放到你的抽牌堆顶部。
Upgraded -> 将手牌中的 3 张牌放到你的抽牌堆顶部。
```

## C1 Selection UI confirmation contract

Selecting the required number of cards is not the same action as confirming the choice.

For C1 Player Selection:

```text
selected count < RequiredCount
→ Confirm disabled

selected count == RequiredCount
→ Confirm enabled
→ pending Selection remains unresolved

player explicitly presses Confirm
→ submit the exact selected RuntimeIds once
```

The final candidate click MUST NOT auto-submit the Selection or double as Confirm.

This behavior belongs to the shared Selection Presentation and MUST be reusable by later Player card-selection Effects.

## C1 Hand→DrawPileTop Presentation contract

The previously discussed in-place fade requirement is withdrawn.

For each committed selected-card fact:

```text
CardZoneChanged
FromZone = Hand
ToZone   = DrawPile
RuntimeId = X
```

Native Presentation MUST use the shared Selection/zone transition to:

```text
exact selected Hand visual for RuntimeId X
→ detach from normal Hand layout for the transition
→ visibly move from its current Hand position to the DrawPile visual anchor
→ complete the transfer at the DrawPile area
→ reconcile Hand layout and DrawCount to committed facts
```

It MUST NOT:

- perform an in-place opacity-only fade as the primary transition;
- replay a DrawPile→Hand draw animation;
- pretend the selected card was discarded or exhausted;
- put this animation inside `USelectHandCardToDrawPileTopEffect`;
- add Warcry/CardId-specific movement code.

This is a reusable selected-card Hand→DrawPile Presentation capability. Future Effects that produce the same committed zone transition MUST reuse it.

## Warcry played-card cleanup contract

The selected Hand card transition and Warcry's own cleanup are separate Presentation responsibilities.

Required observable sequence:

```text
Warcry played
→ Draw Gameplay commit
→ Draw Presentation shown
→ post-Draw Hand displayed
→ shared Selection UI
→ select required card(s)
→ explicit Confirm
→ selected exact RuntimeId card(s) visibly fly Hand → DrawPile
→ Selection Presentation ends
→ Warcry PlayArea visual is visible/available again
→ FinishCardPlay performs Warcry's authored destination
→ existing generic PlayArea → Exhaust Presentation runs
→ resolution completes
```

Warcry MUST NOT receive a dedicated fade/disappear/Exhaust animation for this Effect.

If Warcry exhausts, its final disappearance comes only from the existing generic played-card Exhaust Presentation driven by normal cleanup facts.

Explicitly forbidden examples:

```text
PlayWarcryFadeOut()
Warcry-specific Exhaust animation
SelectHandCardToDrawPileTopEffect-owned Exhaust animation
one hard-coded sequence that combines selected-card transfer with Warcry cleanup
```

## Preserved C1 architecture

`UDeckRuntime::DrawPile` remains bottom -> top with array end as top. UI/Presentation does not gain hidden DrawPile card identities.

Gameplay remains authoritative. Presentation consumes committed facts and MUST NOT mutate Gameplay to make the movement animation work.

The interactive boundary still exists only to ensure the player sees the committed pre-selection state before Selection becomes interactive. It does not make animation completion an authoritative Gameplay trigger.

## Production asset scope

This amendment does not itself authorize automatic modification of user-owned production `.uasset` or `.umap` files by tooling. Production asset changes remain explicit user-authoring / user-authorization work.

## Updated C1 acceptance points

The durable C1 acceptance contract remains:

- Draw is visibly presented before Selection becomes interactive;
- newly drawn legal Hand cards are selectable;
- selecting RequiredCount cards does not auto-submit;
- explicit Confirm submits the exact selected RuntimeIds once;
- selected cards visibly move from Hand to the DrawPile anchor;
- Hand→DrawPile does not use the old in-place fade requirement;
- multi-select transition order follows committed canonical record order;
- the Selection animation path is generic/reusable and contains no Warcry/Effect-specific branch;
- Warcry cleanup reuses the existing generic Exhaust animation;
- no dedicated Warcry Exhaust/fade animation is added;
- normal later Effects / FinishCardPlay continue after the Selection continuation;
- Gameplay remains authoritative under Presentation skip/degradation according to the shared constraints.

Recorded repository evidence includes C1 implementation merged by PR #18, Development Editor build PASS and the focused `SlayTheSpireDemo.CardExpansion.Wave1CC1.DrawPileTop` 7/7 Automation boundary review. The later G4+G5 user PIE acceptance also confirms the shared exact visible SelectionArea→DrawPile transition has no flashback/duplicate/ghost/clipping/stuck-input regression. These facts must not be conflated with a standalone final C1 seal: no dedicated final C1 user-seal record is currently present.

No Build, Automation, or PIE gate may be marked PASS without actual execution evidence.