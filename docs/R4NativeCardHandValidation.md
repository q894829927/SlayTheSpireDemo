# Phase 6UI-A2N — R4 Native Card Widget, Hand and Card Input

Status: **SOURCE IMPLEMENTED / UE VALIDATION PENDING**

Branch: `a2n/r4-native-card-hand`
Base: `main@9981dcebda27ae5be46be608177084412e78b1fb`

R4 migrates only formal Hand-card display and input ownership. It does not migrate committed Presentation playback; `UBattleHUDWidget::BeginPresentationRecordPlayback_Implementation` still returns `false` for every Record.

## Implemented boundary

### `UBattleCardWidget`

The Native Card class now owns:

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

### R5+ boundary retained

R4 does not implement:

```text
CardPlayed playback
CardZoneChanged playback
PlayArea transient cards
HiddenHandCardWidget ownership
Presentation Token/timer state
Damage / Block / Energy / Shuffle playback
Status lifecycle
Terminal Record sequencing
Controller / Reducer / Record / Envelope changes
production cutover
```

## Focused Editor-only Automation

One focused source-level contract test was added:

```text
SlayTheSpireDemo.Phase6UIA2N.R4.CardWidget.DTOAndRequest
```

It checks:

```text
SetCardView immediate field refresh
RuntimeId / CardId / full DTO round-trip
CardType and CardArt surface refresh
exact RuntimeId request payload
AddUniqueDynamic duplicate-listener protection
formal unplayable-card request remains available for ViewModel feedback
INDEX_NONE emits no card request
```

This Automation test does not replace real Designer-backed WBP or PIE acceptance.

## Required UE5.8 validation

Do not mark R4 COMPLETE / VALIDATED until the following pass on this branch.

### 1. Editor build

```powershell
& "E:\Unreal engine\UE_5.8\Engine\Build\BatchFiles\Build.bat" `
  SlayTheSpireDemoEditor Win64 Development `
  -Project="E:\UE_DEMO\SlayTheSpireDemo\SlayTheSpireDemo.uproject" `
  -WaitMutex -NoHotReload
```

Expected: `Result: Succeeded`.

### 2. Blueprint compile

Compile `WBP_BattleCard_Native` and `WBP_BattleHUD_Native`.

Expected:

```text
no missing BindWidget
no inherited member collision
no Blueprint compile error
```

Do not modify or save the three Legacy WBP assets.

### 3. Focused Automation

```powershell
& "E:\Unreal engine\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" `
  "E:\UE_DEMO\SlayTheSpireDemo\SlayTheSpireDemo.uproject" `
  -ExecCmds="Automation RunTests SlayTheSpireDemo.Phase6UIA2N.R4; Quit" `
  -unattended -nopause `
  -testexit="Automation Test Queue Empty" `
  -log
```

Expected: the R4 focused test passes with 0 failed / 0 notRun.

### 4. Native PIE — initial Hand parity

Open:

```text
/Game/SlayTheSpireDemo/Maps/L_BattleTest_Native
```

Confirm:

```text
initial Hand count equals ViewModel.HandCards
normally 5 cards in the existing battle fixture
Name / Cost / Type / Description / Art are populated
RuntimeId and CardId are stable when inspected
no duplicate Hand cards
```

### 5. Formal Card request / target flow

Use a normal enemy-target card such as Strike:

```text
click Strike once
-> ViewModel.SelectedCardRuntimeId becomes that exact card RuntimeId
-> InteractionState becomes ChoosingTarget
-> Enemy legal-target highlight appears
-> Confirm remains hidden
-> Cancel appears
```

Click the same selected card again or use Cancel:

```text
selection clears
InteractionState returns Idle
legal-target highlight clears
Cancel hides
Hand remains exactly one Widget per ViewModel.HandCards entry
```

Then select Strike and click the legal Enemy target:

```text
formal request is accepted once
no duplicate card callback / duplicate resolution
```

Committed presentation visuals are still immediate-fallback in R4; absence of Native Damage/CardPlayed animation is not an R4 failure.

### 6. ReadyToConfirm / Confirm flow

If the current fixture contains a `TargetType=None` card, use it and confirm:

```text
click card -> ReadyToConfirm
Confirm visible/enabled
Cancel visible/enabled
Confirm submits exactly once
```

If the standard battle fixture contains no `TargetType=None` card, record this item as not exercisable in that fixture rather than inventing a production asset change; the existing ViewModel contract remains covered by prior tests.

### 7. Unplayable formal card

Use a card whose current cost exceeds available Energy, or reduce Energy through an existing legitimate test path. Confirm:

```text
card remains present in Hand
click still enters the formal SelectCard request path
ViewModel rejects it authoritatively
Txt_Feedback shows the existing rejection reason
no Gameplay resolution starts
```

### 8. Regression boundary

Confirm:

```text
production L_BattleTest still uses WBP_BattleHUD_C
Legacy WBP_BattleHUD / WBP_BattleCard / WBP_BattleStatus unchanged
R3 static HUD remains correct
zero-Block badge remains collapsed
R5 is NOT STARTED
BeginPresentationRecordPlayback still returns false
```

After these gates pass, update `docs/CODEX_GOAL_CHECKPOINT.md`, `docs/Validation.md`, and the A2N plan status to `R4 COMPLETE / VALIDATED`; then merge this branch. Until then this branch remains **VALIDATION PENDING**.
