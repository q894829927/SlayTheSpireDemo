# Codex Goal Checkpoint — Phase 6UI-A3

Last updated: **2026-09-02**

## Goal

Complete Phase 6UI-A3 under `docs/Phase6UIA3Implementation.md` without changing Gameplay authority, A2 committed-presentation semantics, Native/Legacy ownership, or phase ordering.

```text
A3 = pre-commit read-only current-state supported Operation values
A2 = post-commit playback of immutable facts that actually committed
```

## Current execution status

```text
UI-A3: IN PROGRESS / AUTHORIZED
A3-1 Dynamic Text: COMPLETE / VALIDATED / SEALED
A3-2 Target-Specific Current-State Preview: COMPLETE / VALIDATED / SEALED
A3-3 Energy + Target-Aware Legality: COMPLETE / VALIDATED / SEALED
A3-4 ViewModel Transient Preview Lifecycle: COMPLETE / VALIDATED / SEALED
A3-5 Minimal Native UMG + A2/A3 Combined PIE: PIE DEFECT FIXED / REVALIDATION REQUIRED
```

UI-A2 remains complete/sealed. Native HUD remains the sole active production implementation. Legacy HUD/Card/Status remain retained/deprecated with zero production runtime dependency.

## Active authority

```text
AGENTS.md
Source/SlayTheSpireDemo/UI/AGENTS.md
docs/Phase6UIA3Implementation.md
docs/Phase6UIA3DynamicTextImplementation.md
docs/ValidationExecutionPolicy.md
```

`docs/Phase6UIA3Implementation.md` is the active implementation/ordering/acceptance authority. `docs/Phase6UIA2EImplementation.md` remains sealed A2 history and design background.

## Sealed predecessor evidence

```text
A3-2A Editor Build: PASS
A3-2A Automation SlayTheSpireDemo.UIA3.ImmediatePreview: 3/3 Success, exit 0
A3-2B Editor Build: PASS
A3-2B Automation SlayTheSpireDemo.UIA3.ImmediatePreviewQuery: 2/2 Success, exit 0

A3-3 Editor Build: PASS
A3-3 Automation SlayTheSpireDemo.UIA3.ImmediatePreviewLegality: 2/2 Success, exit 0
A3-3 compatibility rerun SlayTheSpireDemo.UIA3.ImmediatePreviewQuery: 2/2 Success (user-reported)

A3-4 Editor Build: PASS (user-reported)
A3-4 Automation SlayTheSpireDemo.UIA3.ViewModelPreviewLifecycle: PASS (3 expected, user-reported)
Manual PIE: NOT REQUIRED for A3-2/A3-3/A3-4
```

A3-4 historical shared-contract note remains: `SlayTheSpireDemo.Phase6UIA1.ViewModel` discovered 11 tests; directly affected interaction/revision/terminal cases passed, while two stale historical assertions (`SubscribeThenPullBuildsHUD`, `CardPresentationFieldsComeFromDefinition`) failed because they mutate Status/CardData after the opening frozen Presentation baseline has already been created. Do not weaken frozen Presentation semantics to make those assertions pass.

## A3-5 implementation state

Implementation/fix commits after the A3-4 seal include:

```text
1dd72a378089d6e3e61a1de4996062e9e81c9f9f  feat(ui-a3): add dedicated combatant preview events
5947d70a13b3979da9ac5e82eb95000b5d167265  feat(ui-a3): publish independent preview lifecycle
b77b435d9aafd4cb1d8f86c1be4cb38a344572f6  feat(ui-a3): expose formatted immediate preview text
3124784b6856e2b71369f5218cf862ffeeece05d  feat(ui-a3): format immediate preview for native hud
b995e5b489347bda39c028d9b6c77feb59e4b316  feat(ui-a3): add native immediate preview text surface
2ab3fdc549368473e2ce0a04f92c2d67a11447a8  feat(ui-a3): drive native preview text from viewmodel
b108012a0c0f3c1a0fb37ce18481821806e3e906  feat(ui-a3): wire native preview surface contract
6f27914acaa9fddbe59282fda29685cd5b907d50  feat(ui-a3): integrate native preview target surface
bab2a1f31a10999de7b5b628722f46be15598809  feat(ui-a3): clear preview before target request
8eae36f1b5077f02e96e575583ace2d355bcfbc5  test(ui-a3): add native preview integration probes
03c52b76af82108a12eaf45d5d708dac0f971c90  test(ui-a3): implement native preview integration probes
0b4b0e6406d598a5acdd0e4b70bf94c26457bdd0  test(ui-a3): cover native preview and a3 a2 handoff
5b0522b3c96cef9e069d5cb85d7a1f71f4b8f071  fix(ui-a3): avoid preview overlay slot shadowing
e6e9d55bad20f790216cd5fbaf48b4169c7e12a9  fix(ui-a3): restore native energy presentation state name
0ea1f50aa4d459f862b7415821077aee330a22ae  test(ui-a3): give handoff preview visible energy state
a8f875a5a0b52017ef15b26abd9183e361729992  fix(ui-a3): release preview surface before target handoff
1a102dc321ea3a1fac6c93e321c519aea5eade15  test(ui-a3): expose production preview-clear handoff
63531a214cee455ad0609b2e13d59ea39a9f7923  test(ui-a3): route probe through production preview clear
36076047e25ea8e6d47831eac61ada23e66e11f0  test(ui-a3): cover production preview clear before target request
```

Changed production boundary:

```text
Source/SlayTheSpireDemo/UI/BattleHUDCombatantPresentationWidgetBase.h/.cpp
Source/SlayTheSpireDemo/UI/BattleHUDViewModel.h
Source/SlayTheSpireDemo/UI/BattleHUDViewModelUIA3Preview.cpp
Source/SlayTheSpireDemo/UI/BattleHUDWidget.h
Source/SlayTheSpireDemo/UI/BattleHUDWidgetBase.cpp
Source/SlayTheSpireDemo/UI/BattleHUDWidgetUIA3Preview.cpp
Source/SlayTheSpireDemo/UI/BattleImmediatePreviewTextBlock.h/.cpp
```

Implemented A3-5 semantics:

```text
Combatant presentation exposes dedicated OnPreviewRequested(TargetId) / OnPreviewCleared delegates
pointer/focus may drive both Inspection and Preview, but the two event/state lifecycles are independent
stationary hover/focus republishes Preview when legal target state changes
committed target interaction clears transient Preview before OnTargetRequested
UBattleHUDWidget binds only the dedicated Preview delegates for PreviewTarget nomination
ViewModel formats only already-resolved ImmediatePreview DTO fields; no Widget Gameplay calculations
Native Preview surface is a dedicated UBattleImmediatePreviewTextBlock, not Enemy Intent, feedback, inspection tooltip or A2 damage-number text
Preview surface is dynamically parented into OV_PlayArea only while valid Preview text exists
production OnPreviewCleared now explicitly releases the A3 Preview Widget before clearing the ViewModel DTO, so physical OV_PlayArea ownership is gone before synchronous OnTargetRequested / authoritative request / A2 playback
UBattleHUDWidgetBase::SelectTarget also clears the ViewModel Preview before SelectTargetById as a second semantic guard
accepted authoritative Request then follows the unchanged Gameplay/A2 resolution path
no Legacy behavior/fallback/parity code was added
no Blueprint asset dependency was added; production Legacy dependency closure is therefore not intentionally changed
```

First compact display formatting is presentation-only:

```text
Damage 9
Damage 9 x 2
Block 8
Energy 3 -> 2
```

Rejected validation remains unavailable on this minimal numeric surface; existing HUD feedback continues to own player-facing request-failure text. This avoids creating a duplicate legality/failure-reason table in A3 UI formatting.

## Focused A3-5 Automation

Prefix:

```text
SlayTheSpireDemo.UIA3.NativePreviewIntegration
```

Tests:

```text
DedicatedPreviewEventsStayIndependentFromInspection
NativeSurfaceRendersAndClearsFromViewModel
TargetSubmissionClearsPreviewBeforeAuthoritativeRequest
```

The handoff regression now exercises the production ordering explicitly: Preview is visible, the HUD production Preview-clear handler physically removes the transient OV_PlayArea child and clears the DTO while selection is still `ChoosingTarget`, and only then is the target submitted. This closes the earlier test gap where the test called `HUD->SelectTarget()` directly and therefore did not exercise the real Combatant `OnPreviewCleared -> OnTargetRequested` sequence.

## Validation history and current state for A3-5

Before the PIE defect fix:

```text
Editor Build: PASS (user-reported)
SlayTheSpireDemo.UIA3.NativePreviewIntegration: 3/3 Success, exit 0 (user-reported)
```

The first Automation run had exposed a test-fixture defect: the handoff card had `Cost=0` and no supported Damage/Block effect, so the DTO could be valid while the minimal numeric Preview text was correctly empty. The fixture was changed to `Cost=1`, producing deterministic `Energy 3 -> 2`; the rerun passed 3/3.

Manual production PIE then exposed a real handoff defect:

```text
L_BattleTest: FAIL
Symptom: after target submission, the played card no longer appeared in the Native play area.
```

The relevant A2 `CardPlayed` path requires `OV_PlayArea` to be physically empty before it takes ownership. A3 had been relying on the Preview TextBlock's ViewModel multicast callback to remove itself. The real Combatant path already emitted `OnPreviewCleared` before `OnTargetRequested`, but the HUD handler only cleared the ViewModel DTO. The fix makes the HUD handler explicitly call `ReleaseImmediatePreviewSurface()` before `ClearPreviewTarget()`, removing dependence on multicast callback ordering for physical PlayArea ownership.

Because production code and the focused regression changed after the previous green run, that earlier A3-5 automated result is historical evidence only and does not seal the current head.

## Next exact action — revalidate the handoff fix

Run only:

```text
1. Editor Build once at current head.
2. SlayTheSpireDemo.UIA3.NativePreviewIntegration once — expect 3/3 Success, exit 0.
3. Return to production L_BattleTest PIE and recheck Strike/Enemy and Defend/Player handoff.
```

PIE acceptance:

```text
Strike / Enemy:
select Strike -> hover/focus Enemy -> Damage Preview appears
submit Enemy -> Preview disappears -> played card visibly enters A2 play area -> committed Damage playback remains coherent

Defend / Player:
select Defend -> hover/focus Player -> Block Preview appears
submit Player -> Preview disappears -> played card / committed Block presentation proceeds normally

revision invalidation:
relevant state/modifier change -> old selection/Preview does not survive new StateRevision
new selection -> Preview reflects current state

No duplicate target highlight, stale Preview, Preview-over-A2 overlap or visible A3/A2 flashback.
```

Do not run Phase6R, A2D5, Shipping, broad Scenario A-E or Legacy parity unless a concrete failure invalidates their sealed contracts. Do not start Phase 7 until A3-5 is validated and sealed.