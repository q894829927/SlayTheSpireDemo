# Card Expansion — Wave 1C-C1 True Grit Consumer Plan

Date: **2026-09-08**

Status:

```text
PLAN PROPOSED / DEVELOPMENT BRANCH ACTIVE
C0 VALIDATION DEFERRED BY USER
C1 MERGE / SEAL BLOCKED UNTIL C0 SEAL
```

Branch:

```text
Wave-1C-C1
```

Branch base:

```text
main @ c700db13a50d4c3c6302e836424de3e4693fafa4
```

This slice is the first real consumer of the generalized C0 Select-Exhaust capability.

C0 remains `NOT SEALED`. The user explicitly authorized beginning C1 work before running the deferred C0 Build / Automation / PIE validation. C1 may therefore implement and test its own consumer surface on this branch, but it must not be merged/sealed as dependency-complete until C0 is validated and any C0 validation fixes are integrated into this branch.

---

## 1. Consumer target

True Grit is authored as one normal Ironclad Skill using existing orthogonal Effects:

```text
GainBlockCardEffect
+
USelectExhaustHandCardEffect
```

No True-Grit-specific Gameplay Action, Selection Action, Exhaust Action, RNG implementation, HUD path, or CardId branch is authorized.

Expected card facts for this project slice:

```text
CardId             = TrueGrit
DisplayName        = True Grit
CardType           = Skill
Rarity             = Common
CardColor          = Red
BaseCost           = 1
UpgradedCost       = 1
TargetType         = Self
DefaultDestination = Discard

GainBlockCardEffect
BaseAmount          = 7
UpgradedAmount      = 9

USelectExhaustHandCardEffect
BaseSelectionMode      = Random
BaseSelectionCount     = 1
UpgradedSelectionMode  = Player
UpgradedSelectionCount = 1
```

The played True Grit card is excluded from Select-Exhaust candidates by the reusable C0 effect, so only other valid cards currently in Hand are eligible.

---

## 2. Locked execution shape

Base:

```text
play True Grit
→ GainBlockAction(7)
→ RandomSelectionAction(exactly 1 other Hand candidate)
→ FSelectionResult
→ ExhaustSelectedContinuation
→ ExhaustCardAction(selected card)
→ FinishCardPlay
→ True Grit PlayArea -> Discard
```

Upgraded:

```text
play True Grit+
→ GainBlockAction(9)
→ SelectionRequestAction(exactly 1 other Hand candidate)
→ Native HUD Player selection
→ FSelectionResult
→ ExhaustSelectedContinuation
→ ExhaustCardAction(chosen card)
→ FinishCardPlay
→ True Grit PlayArea -> Discard
```

Block must resolve before the exhaust step because authored Effects order is authoritative:

```text
Effects[0] = GainBlockCardEffect
Effects[1] = USelectExhaustHandCardEffect
```

The upgraded pending selection does not undo already committed Block.

---

## 3. No-candidate rule

C1 inherits the C0 no-candidate semantics rather than adding a True Grit exception.

If True Grit is the only valid Hand card at play time:

```text
Gain Block
→ SelectExhaust sees zero candidates
→ selection/exhaust step is a legal no-op
→ no pending UI
→ no RNG consumption
→ True Grit still finishes to Discard
```

This must work for both Base and Upgraded True Grit.

---

## 4. C1-0 — transient consumer proof first

Before production asset work, author a transient C++ True Grit fixture using exactly the intended Effects composition.

Required proof:

```text
Base
→ Gain 7 Block
→ Random / 1
→ no pending selection
→ exactly one other Hand card Exhausts
→ no resolution fault

Upgraded
→ Gain 9 Block
→ Player / 1
→ mandatory pending selection
→ selected exact card Exhausts
→ no resolution fault
```

If this requires a True-Grit-specific core Gameplay Action, stop and review whether C0's generic contract is incomplete.

---

## 5. C1-1 — mode-dependent description text

Gameplay composition should require no new mechanic code, but True Grit exposes one player-facing issue that C0 deliberately deferred:

```text
Base     = random exhaust
Upgraded = player-chosen exhaust
```

One immutable `UCardData::Description` must communicate that mode change accurately.

Preferred narrow extension:

```text
USelectExhaustHandCardEffect
+ optional SelectionModeDescriptionArgumentName
```

Semantics:

```text
NAME_None
→ declare no mode text argument
→ preserves existing Burning Pact compatibility

non-None
→ BuildPreviewArguments emits an FText derived from effective SelectionMode

Random → localized text token equivalent to "random"
Player → localized text token equivalent to "chosen"
```

C1 may then author one immutable description equivalent to:

```text
"Gain {Block} Block. Exhaust {Exhaust} {ExhaustMode} card from your hand."
```

with:

```text
GainBlockCardEffect.DescriptionArgumentName = Block
SelectExhaust.DescriptionArgumentName = Exhaust
SelectExhaust.SelectionModeDescriptionArgumentName = ExhaustMode
```

Expected resolved card face:

```text
Base     → Gain 7 Block. Exhaust 1 random card from your hand.
Upgraded → Gain 9 Block. Exhaust 1 chosen card from your hand.
```

Rules:

```text
- no TrueGrit CardId branch
- no duplicate Base/Upgraded Description field on UCardData
- mode text derives from the same authored SelectionMode used by Gameplay
- default mode-text argument remains opt-out for serialized Burning Pact
- do not redesign the global card-description system
```

If review finds an existing generic text surface that already expresses this correctly, reuse it and omit this extension.

---

## 6. C1-2 — focused Gameplay Automation

Add a dedicated filter:

```text
SlayTheSpireDemo.CardExpansion.Wave1CC1.TrueGrit
```

Minimum tests:

```text
TrueGrit.BaseComposition
- Skill/Common/Red transient definition facts
- cost 1
- Block 7
- Random / 1

TrueGrit.UpgradedComposition
- cost remains 1
- Block 9
- Player / 1

TrueGrit.BaseRandomExecution
- Block commits first
- no pending player selection
- exactly one other Hand card exhausts
- exhausted card is not True Grit
- deterministic seed produces reproducible membership
- True Grit finishes to Discard

TrueGrit.UpgradedPlayerExecution
- Block commits before wait
- mandatory Player / 1 request
- chosen exact card exhausts
- True Grit finishes to Discard

TrueGrit.BaseNoCandidate
- Gain 7 Block
- no RNG-dependent failure
- no Exhaust
- no pending UI
- finishes to Discard

TrueGrit.UpgradedNoCandidate
- Gain 9 Block
- no pending UI
- no Exhaust
- finishes to Discard
```

---

## 7. C1-3 — player-facing text Automation

If C1-1 adds the mode text argument, test:

```text
Base description
→ Block = 7
→ Exhaust = 1
→ mode = random

Upgraded description
→ Block = 9
→ Exhaust = 1
→ mode = chosen

Burning Pact compatibility
→ existing mode-text opt-out remains valid
```

The description extension is Presentation/read-only. It must never select cards or consume RNG.

---

## 8. C1-4 — committed Presentation regression

True Grit should reuse existing committed presentation machinery.

Base expected record ordering:

```text
CardPlayed(True Grit)
→ BlockChanged(7)
→ CardZoneChanged(selected Hand -> ExhaustPile)
→ CardZoneChanged(True Grit PlayArea -> DiscardPile)
```

Upgraded expected record ordering after the player resolves selection:

```text
CardPlayed(True Grit+)
→ BlockChanged(9)
→ CardZoneChanged(chosen Hand -> ExhaustPile)
→ CardZoneChanged(True Grit+ PlayArea -> DiscardPile)
```

Required regressions:

```text
- exhausted card uses existing in-place Hand fade
- no ghost/tail reappearance
- Block remains committed while upgraded selection is pending
- normal input returns after resolution
```

No new True-Grit-specific Presentation animation is authorized.

---

## 9. C1-5 — production DataAsset handoff

Target production asset path:

```text
Content/SlayTheSpireDemo/Data/Cards/Ironclad/Skills/DA_Card_TrueGrit.uasset
```

This repository's production `.uasset` content is Git-LFS-backed and must be authored/saved through Unreal Editor.

This planning step does **not** authorize the assistant to modify/create binary `.uasset` or `.umap` files. Production asset creation waits for explicit user authorization / local Unreal authoring.

Asset checklist when authorized:

```text
CardId=TrueGrit
DisplayName=True Grit
Skill / Common / Red
Cost 1 / 1
Target Self
Destination Discard
Effects order:
  1. GainBlock 7 / 9
  2. SelectExhaust Random/1 -> Player/1
Description arguments wired to Block + Exhaust + mode text as applicable
```

No card-specific code registration should be added merely to recognize `TrueGrit`.

---

## 10. C1-6 — validation and dependency reconciliation

C1 can be developed now, but final validation must occur on a branch state that includes the final sealed C0 implementation.

If deferred C0 validation later produces fixes on `main`:

```text
C0 validation fix on main
→ integrate updated main into Wave-1C-C1
→ rerun C1 validation
```

C1 merge/seal gates:

```text
[ ] C0 COMPLETE / VALIDATED / SEALED
[ ] latest sealed C0 integrated into Wave-1C-C1
[ ] SlayTheSpireDemoEditor Win64 Development Build PASS
[ ] C1 focused Automation PASS
[ ] C0 focused 20/20 regression PASS
[ ] Wave 1C sealed 13/13 regression PASS
[ ] True Grit Base PIE PASS
[ ] True Grit+ PIE PASS
[ ] no-candidate behavior PASS
[ ] production DataAsset validation PASS when asset is authorized/authored
[ ] user seal confirmation
```

Until those gates pass:

```text
C1 = DEVELOPMENT ACTIVE / NOT SEALED
```

---

## 11. Explicit non-goals

C1 does not authorize:

```text
- new random-selection architecture beyond C0
- bulk exhaust
- Exhume / ExhaustPile selection
- Fiend Fire / Second Wind bulk mechanics
- Feel No Pain / Dark Embrace
- Sentinel / Card Trigger Source Expansion
- multi-enemy
- generic AnyZone movement
- optional selection ranges / Confirm button
- True-Grit-specific Gameplay Action
- binary map changes
```

---

## 12. Recommended implementation order

```text
C1-0  transient True Grit consumer proof
C1-1  generic mode-dependent description token only if required
C1-2  Base/Upgraded Gameplay Automation
C1-3  description Automation
C1-4  Presentation ordering regression
C1-5  production DataAsset handoff after explicit binary authorization
C1-6  integrate final sealed C0 + validation + seal
```

The first code change should therefore be the transient consumer Automation, not a production asset and not a new True-Grit-specific Action.
