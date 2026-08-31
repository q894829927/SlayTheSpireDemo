# Phase 6UI-A2N — R4 Native Card Widget, Hand and Card Input

Status: **COMPLETE / VALIDATED**

Branch: `a2n/r4-native-card-hand`
Base: `main@9981dcebda27ae5be46be608177084412e78b1fb`
Validation date: **2026-08-31**

R4 migrates only formal Hand-card display and input ownership. It does not migrate committed Presentation playback; `UBattleHUDWidget::BeginPresentationRecordPlayback_Implementation` still returns `false` for every Record.

## Implemented boundary

### `UBattleCardWidget`

The Native Card class owns:

```text
SetCardView(FBattleHUDCardView)
GetRuntimeId()
GetCardId()
GetCardView() by value
OnBattleCardRequested(RuntimeId)
```

Designer-backed fields are refreshed directly from the supplied DTO:

```text
DisplayName  -> Txt_CardName
Cost         -> Txt_Cost
Description  -> Txt_CardDescription
CardType     -> Txt_CardType
CardArt      -> Img_CardArt
```

Required Designer bindings are:

```text
Btn_Card
Txt_CardName
Txt_Cost
Txt_CardDescription
Txt_CardType
Img_CardArt
```

`CurrentCardView` is deliberately named differently from the duplicated Legacy Blueprint's inert `CardView` member residue, so R4 does not require a binary asset edit merely to remove that old Blueprint variable.

The Card Widget owns no HUD, ViewModel, Gameplay card instance, BattleManager or PresentationController reference.

### Formal Hand ownership

`UBattleHUDWidget::RefreshHUDFromViewModel` now includes `RefreshHand`.

`RefreshHand` performs:

```text
remove old formal-card request bindings
-> HB_Hand.ClearChildren
-> iterate ViewModel.HandCards
-> CreateWidget(CardWidgetClass)
-> SetCardView(frozen/current DTO)
-> AddUniqueDynamic(OnBattleCardRequested)
-> AddChildToHorizontalBox
```

A formal card request forwards only its `RuntimeId` to the existing `UBattleHUDWidgetBase::SelectCard` API. ViewModel / Gameplay retain authoritative playability and target validation.

A formal Hand card with `bGameplayPlayable=false` is still allowed to make the formal request so the existing ViewModel path can show authoritative rejection feedback such as insufficient Energy. Later presentation-only cards remain a separate path: they must not bind this HUD request delegate and must be `HitTestInvisible`; R4 does not create presentation-only cards.

## Focused Editor-only Automation

Permanent R4 source-level contract coverage:

```text
SlayTheSpireDemo.Phase6UIA2N.R4.CardWidget.DTOAndRequest
```

It covers:

```text
SetCardView immediate field refresh
RuntimeId / CardId / full DTO round-trip
CardType and CardArt surface refresh
exact RuntimeId request payload
AddUniqueDynamic duplicate-listener protection
formal unplayable-card request remains available for ViewModel feedback
INDEX_NONE emits no card request
```

## UE5.8 validation evidence — PASS

The saved R4 branch was validated locally in UE5.8. The user reports the complete requested R4 acceptance gate passed.

```text
1. SlayTheSpireDemoEditor Win64 Development build: PASS
2. WBP_BattleCard_Native compile: PASS
3. WBP_BattleHUD_Native compile: PASS
4. SlayTheSpireDemo.Phase6UIA2N.R4 focused Automation: PASS
5. L_BattleTest_Native initial Hand parity: PASS
6. Card Name / Cost / Type / Description / Art rendering: PASS
7. Formal card SelectCard(RuntimeId) request path: PASS
8. ChoosingTarget / legal-target highlight / Cancel behavior: PASS
9. Accepted legal-target submission occurs once; no duplicate card callback: PASS
10. Hand rebuild remains one Widget per ViewModel.HandCards entry: PASS
11. Existing R3 zero-Block badge behavior remains correct: PASS
12. production L_BattleTest remains on WBP_BattleHUD_C: PASS
13. Legacy WBP_BattleHUD / WBP_BattleCard / WBP_BattleStatus remain unchanged: PASS
14. BeginPresentationRecordPlayback remains unmigrated / false fallback: PASS
15. R5 and later remain NOT STARTED: PASS
```

The focused Automation supplements but does not replace the real Designer-backed Native WBP and PIE acceptance above.

## Accepted R4 behavior

R4 now closes the formal static/input Hand boundary:

```text
ViewModel.HandCards
-> Native formal UBattleCardWidget instances
-> frozen/current Card DTO rendering
-> exact RuntimeId request
-> UBattleHUDWidgetBase::SelectCard
-> existing ViewModel / Gameplay authority
```

The accepted implementation does not restore the Legacy `OwnerHUD : WBP_BattleHUD` dependency and does not query mutable Gameplay from the Card Widget.

## R5+ boundary retained

The following are explicitly not R4 acceptance requirements and remain unmigrated:

```text
CardPlayed committed visual playback
CardZoneChanged committed visual playback
PlayArea presentation-only transient cards
HiddenHandCardWidget presentation ownership
Presentation Token / visual timer state
Damage number / animation
BlockChanged / EnergyChanged / DeckShuffled playback
formal Status-row lifecycle
Terminal Record sequencing
Controller / Reducer / Record / Envelope changes
production cutover
```

Therefore absence of Native CardPlayed or Damage animation at this stage is expected and is not an R4 failure.

## Acceptance

**R4 is COMPLETE / VALIDATED.**

Next phase:

```text
R5 — Native playback kernel
```

R5 is **NOT STARTED** and must not be entered implicitly by this acceptance record.
