# Phase 6UI-A2N — Native HUD Ownership Migration

Date: **2026-08-31**

Status: **IN PROGRESS — R0 COMPLETE / R1 NOT STARTED**

## 1. Purpose

UI-A2 and UI-A2E are already complete, validated and sealed. This initiative moves
the sealed HUD behavior from large Widget Blueprint graphs into reviewable and
testable native Widget classes without changing what the behavior means.

This is:

```text
UI implementation ownership migration
```

It is not:

```text
Gameplay redesign
Presentation redesign
Record / Envelope redesign
UI-A3 Preview development
pure-C++ WidgetTree reconstruction
```

The target is:

```text
C++ Widget = UI behavior and visual playback implementation
WBP        = Designer hierarchy, style and visual assets
ViewModel  = frozen display state and current input state
Controller = committed-history playback authority
Gameplay   = authoritative truth
```

Starting A2N does not reopen the sealed Legacy UI-A2E implementation. Until the
production cutover passes on its own final head, the Legacy stack remains the formal
production fallback and the Native stack remains an experimental parallel path.

UI-A3 remains outside this initiative. If A2N starts, do not add new HUD Preview
behavior to both stacks in parallel; complete the agreed A2N boundary or explicitly
pause it before resuming UI-A3 work.

---

## 2. Locked baseline

The current sealed baseline is:

```text
Behavior implementation commit:
81cbfb6af09a52f96ececff597491c5bfcc3665f

Seal documentation commit:
666025c4cc6af2dc1ecf22c51f23810fd8892bb3

WBP_BattleHUD SHA-256:
990125C951D52D5F23194D9EB7C079C2F3C514C78A285DF0DDE273B6B1C0F94A

WBP_BattleCard SHA-256:
1E7579EAFE8BF49AEB953B521604CDE4C442E6580BDEB3E071C210846BC6631F

WBP_BattleStatus SHA-256:
205180C8DF03DAE5D825AB4428ADD4B90EDFBBBB54F9BFEFE76AF07412DA52D2
```

The accepted baseline includes:

```text
Scenario A-E real PIE
active Skip / Cancel
stale-token rejection
Input Unlock after catch-up
Phase6UIA2D5 exactly 6/6
formal Phase6R 100/100
clean-worktree Shipping exclusion
```

R0 must create `docs/UIA2NNativeHUDBaseline.md` with the detailed per-Record visual
contract, control-binding inventory, Native test injection point and current evidence
paths. This plan defines the migration; that future baseline document records the
exact execution starting point.

---

## 3. Architecture invariants

The migration must not change:

```text
Gameplay authority
Presentation Record / Envelope schema
UBattlePresentationController authority
WorkingSnapshot / reducer semantics
ViewModel data semantics
Record ordering
Request APIs
terminal timing
Cancel semantics
input-unlock semantics
PresentationUnavailable separation
```

### 3.1 Controller ownership

`UBattlePresentationController` continues to own:

```text
PlaybackQueue
ActiveEnvelope
WorkingPresentationSnapshot
DisplayedPresentationSnapshot
ActiveRecordIndex
LocalPlaybackGeneration
Controller timeout
Reducer
FinalSnapshot reconciliation
input-binding catch-up
```

The HUD never stores a second WorkingSnapshot or advances the Controller directly.

The HUD may store only presentation-local state:

```text
current visual Token
current Record type
visual finish timer
presentation-only transient Widget references
temporary visual target references
```

### 3.2 Frozen-data rule

The Widget consumes only:

```text
ViewModel frozen display state
FPresentationRecord frozen payload
```

It must not:

```text
recalculate Damage, Block or Energy
derive Status merge outcomes
query UCardInstance or UStatusInstance for historical playback
rebuild historical display from live pile contents
mutate ViewModel or Gameplay truth
```

### 3.3 Complete Record migration

A Record type is migrated only when all of the following are complete:

```text
Begin
payload and target validation
normal Finish
Cancel historical restore
invalid/unsupported false fallback
stale callback behavior
real PIE parity
```

Do not migrate normal playback first and defer Cancel to a later phase.

### 3.4 Legacy compatibility

Legacy asset files remain frozen during migration:

```text
WBP_BattleHUD
WBP_BattleCard
WBP_BattleStatus
```

They are not renamed, reparented, cleaned or edited. Their Designer, EventGraph,
resource references and production WidgetClass remain unchanged until separately
authorized cleanup.

`UBattleHUDWidgetBase` is a shared dependency and may receive only additive,
backward-compatible protocol extensions. Every shared-base change must run the
Legacy regression gate.

### 3.5 Production cutover

The production WidgetClass changes exactly once, in an isolated cutover commit.
Migration-only test configurations may independently select the Native stack; no
player-visible Legacy/Native runtime toggle is allowed.

---

## 4. Target asset and class structure

### 4.1 Legacy stack

```text
UBattleHUDWidgetBase
└── WBP_BattleHUD

WBP_BattleCard
WBP_BattleStatus
```

This remains the formal production path until R12.

### 4.2 Native stack

```text
UBattleHUDWidgetBase
└── UBattleHUDWidget
    └── WBP_BattleHUD_Native

UBattleCardWidget
└── WBP_BattleCard_Native

UBattleStatusWidget
└── WBP_BattleStatus_Native
```

Native WBP assets are created by:

```text
Duplicate Legacy WBP
→ Reparent duplicate to the corresponding native class
→ preserve Designer hierarchy, Slots, animations, resources and Widget names
→ remove business EventGraph and override implementations only from the duplicate
```

Do not reconstruct the complete Designer hierarchy manually unless duplication is
proven unable to preserve a required asset contract.

### 4.3 Recommended source layout

```text
Source/SlayTheSpireDemo/UI/

BattleHUDWidgetBase.h/.cpp
    Widget ↔ Controller playback protocol

BattleHUDWidget.h/.cpp
    Native HUD refresh, input binding and visual playback

BattleCardWidget.h/.cpp
    Card view and card-request event

BattleStatusWidget.h/.cpp
    Status view and frozen identity access

BattleHUDViewModel.h/.cpp
BattleHUDPresenter.h/.cpp
BattleHUDTypes.h
```

No native class hard-codes a WBP asset path. Dynamic classes are supplied with
`TSubclassOf` defaults on `WBP_BattleHUD_Native`.

---

## 5. R0 — Baseline and Native test injection

### 5.1 Baseline document

Create `docs/UIA2NNativeHUDBaseline.md` and record:

```text
current HEAD and baseline commits
Legacy HUD/Card/Status hashes
Scenario A-E evidence
active Skip/Cancel evidence
A2D5 / Phase6R / Shipping evidence
per-Record visible behavior
per-Record validation and Cancel behavior
```

### 5.2 Designer binding inventory

Classify every Designer Widget as:

```text
Required BindWidget
BindWidgetOptional
Designer-only
```

Record:

```text
Widget name
Widget type
Is Variable
native requirement
Legacy usage
Native usage
```

Only Widgets actually manipulated by C++ become bindings. In particular, confirm
`Txt_DamagePresentation` is bindable in the Native duplicate without modifying the
Legacy asset.

### 5.3 Native test injection

Lock one non-production way to start the Native HUD before implementation proceeds:

```text
dedicated migration test Map
or dedicated Presenter test asset
or Editor Automation injection before Presenter initialization
```

The production configuration remains:

```text
WidgetClass = WBP_BattleHUD
```

The migration test configuration uses:

```text
WidgetClass = WBP_BattleHUD_Native
```

Do not add a runtime toggle and do not create a second Controller assembly path.
The existing `ABattleHUDPresenter::WidgetClass` remains the unique injection point.

### R0 acceptance

```text
Legacy hashes unchanged
Legacy PIE smoke unchanged
binding inventory complete
Native test injection selected and documented
single production WidgetClass ownership confirmed
```

---

## 6. R1 — Backward-compatible base native hook

Current path:

```text
ViewModel.OnChanged
→ UBattleHUDWidgetBase::HandleViewModelChanged
→ BP_OnViewModelChanged
```

Add one protected virtual hook:

```cpp
virtual void NativeOnBattleHUDViewModelChanged();
```

Preserve the exact current cancellation suppression logic:

```cpp
void UBattleHUDWidgetBase::HandleViewModelChanged()
{
    if (!bSuppressPresentationCancellation)
    {
        CancelTrackedPresentationPlayback();
    }

    NativeOnBattleHUDViewModelChanged();
}

void UBattleHUDWidgetBase::NativeOnBattleHUDViewModelChanged()
{
    BP_OnViewModelChanged();
}
```

Legacy behavior remains:

```text
Base native hook
→ BP_OnViewModelChanged
→ existing Legacy EventGraph
```

`UBattleHUDWidget` overrides the native hook and does not call `Super`, so the Native
stack does not execute the Legacy Blueprint refresh.

Do not change ViewModel delegate ownership or allow ViewModel to manipulate Widgets.

### R1 acceptance

```text
project files regenerated with the bundled UE 5.8 .NET runtime
Editor build PASS
Legacy WBP hash unchanged
Legacy initial HUD and ViewModel refresh PIE PASS
normal Finish/Skip does not become visual Cancel
fail-safe ViewModel change still cancels abandoned tracked visual
```

---

## 7. R2 — Native HUD shell

Create:

```text
UBattleHUDWidget
WBP_BattleHUD_Native
```

The new WBP is the duplicated and reparented Native asset. Remove Legacy business
graphs only from this duplicate. Retain its complete Designer contract.

### 7.1 Bindings

Required controls use:

```cpp
UPROPERTY(meta = (BindWidget))
```

Truly optional controls use:

```cpp
UPROPERTY(meta = (BindWidgetOptional))
```

Decorative controls are not bound.

`BindWidget` should make missing required controls a Widget Blueprint compile error.
`NativeOnInitialized` or `NativeConstruct` additionally uses `ensureMsgf` and an
explicit `UE_LOG(Error)` for invalid required runtime bindings. A broken Native HUD
must disable its input/playback path rather than silently operate with partial state.
Do not expand the Presenter initialization API merely to simulate a return value from
`NativeConstruct` unless a concrete need is later proven.

### 7.2 Dynamic classes

The HUD declares:

```cpp
UPROPERTY(EditDefaultsOnly, Category = "Battle HUD|Widgets")
TSubclassOf<UBattleCardWidget> CardWidgetClass;

UPROPERTY(EditDefaultsOnly, Category = "Battle HUD|Widgets")
TSubclassOf<UBattleStatusWidget> StatusWidgetClass;
```

The Native WBP supplies `WBP_BattleCard_Native` and
`WBP_BattleStatus_Native`; C++ contains no WBP object path.

### R2 acceptance

```text
Editor build PASS
Native WBP compile/save PASS
Native test configuration creates the real Designer-backed HUD
all required bindings valid
Legacy WBP assets unchanged
```

---

## 8. R3-A — Static HUD and long-lived input bindings

This phase does not yet migrate Hand/Card input or Presentation Records.

Implement small refresh functions rather than one monolithic method:

```text
RefreshHUDFromViewModel
RefreshCombatants
RefreshEnergy
RefreshPileCounts
RefreshInputState
RefreshFeedback
RefreshTerminalFromViewModel
```

`RefreshTerminalFromViewModel` renders only the historical ViewModel state. It must
not reveal a future Gameplay Outcome before the corresponding Terminal Record has
been played and reduced.

### 8.1 Long-lived delegates

Bind once in `NativeConstruct` and unbind in `NativeDestruct`:

```text
EndTurn
Confirm
Cancel
Combatant OnTargetRequested
Combatant OnInspectRequested
Combatant OnInspectCleared
```

Use `AddUniqueDynamic` where appropriate. Do not bind long-lived delegates in
ViewModel Changed, refresh functions or per-Record playback.

All input handlers continue through the formal `UBattleHUDWidgetBase` APIs:

```text
SelectCard
SelectTarget
ConfirmSelectedCard
CancelSelection
EndTurn
```

### R3-A acceptance

```text
initial HP / Block / Energy / pile counts parity
feedback and button-state parity
combatant inspect/target delegates bind once
terminal historical surface parity
PresentationUnavailable ViewModel rendering parity
Legacy configuration unchanged
```

---

## 9. R4 — Native Card Widget, Hand and card input

Create:

```text
UBattleCardWidget
WBP_BattleCard_Native
```

The Card Widget owns only its frozen/current display DTO and a UI request event. It
does not know the HUD, ViewModel, Gameplay or Controller.

```cpp
void SetCardView(const FBattleHUDCardView& View);
int32 GetRuntimeId() const;
FName GetCardId() const;
```

If Blueprint needs the full view, return it by value:

```cpp
UFUNCTION(BlueprintPure)
FBattleHUDCardView GetCardView() const;
```

Card request delegate:

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnBattleCardRequested,
    int32,
    RuntimeId
);
```

### 9.1 Delegate ownership

Long-lived HUD delegates follow R3-A. Dynamic Hand Card delegates follow Widget
lifetime:

```text
Create formal Hand Card
→ bind OnBattleCardRequested exactly once
→ removal/rebuild releases that Widget and its binding
```

Presentation-only cards:

```text
do not bind input delegate
bGameplayPlayable = false
HitTestInvisible
```

### 9.2 Hand refresh

Migrate `RefreshHand` only after the Native Card Widget exists. Do not use string
reflection to read Legacy Blueprint variables and do not preserve the Legacy
`OwnerHUD : WBP_BattleHUD` dependency.

### R4 acceptance

```text
Hand rebuild parity
RuntimeId and CardId parity
playable/unplayable card parity
card selection and cancellation
ChoosingTarget / legal-target highlight
Confirm and EndTurn input behavior
presentation-only Card cannot receive input
no duplicate dynamic delegate callbacks
```

This acceptance completes the full static/input parity gate that R3-A intentionally
left open.

---

## 10. R5 — Native playback kernel

Override the real `BlueprintNativeEvent` implementation points:

```cpp
virtual bool BeginPresentationRecordPlayback_Implementation(
    const FPresentationRecord& Record,
    const FPresentationPlaybackToken& Token
) override;

virtual void CancelPresentationRecordPlayback_Implementation(
    const FPresentationPlaybackToken& Token
) override;
```

Never bypass:

```text
UBattleHUDWidgetBase::PlayPresentationRecord
UBattleHUDWidgetBase::NotifyPresentationFinished
```

Never call `UBattlePresentationController::NotifyPresentationFinished` directly.

### 10.1 Active visual context

Store only local visual ownership:

```text
Token
Record type
VisualFinishTimer
presentation-only transient references
temporary visual target references
```

Prefer typed UObject ownership:

```cpp
TObjectPtr<UBattleCardWidget>
TObjectPtr<UBattleStatusWidget>
TWeakObjectPtr<UBattleHUDCombatantPresentationWidgetBase>
```

Do not copy queue, reducer, WorkingSnapshot, Controller generation or Controller
timeout authority.

### 10.2 Begin invariant

Every false return must leave zero local visual side effects:

```text
no active presentation
no visual timer
no leaked transient Widget
no formal Widget visibility mutation
no frozen After value left on the formal HUD
```

Begin order:

```text
validate payload and target
→ validate current local visual state
→ prepare required resources
→ commit active ownership
→ apply visible mutation
→ start token-captured timer
→ return true
```

If preparation or timer start can fail, roll back before returning false.

Unmigrated or unsupported Record types return false and use the existing Controller
immediate fallback. They must not start a placeholder async timer.

### 10.3 Finish

The visual timer captures the Token by value:

```text
FinishNativePresentation(ExpectedToken)
```

It first requires:

```text
ActivePresentation.IsActive
AND ExpectedToken == ActivePresentation.Token
```

Otherwise it is a no-op.

Normal completion:

```text
type-specific Finish
→ copy exact Token
→ clear visual Timer
→ clear local active ownership
→ NotifyPresentationFinished(exact Token)
```

`NotifyPresentationFinished` remains the base-class deferred bridge; it is not
overridden. An old callback must never clear a new active Token.

### 10.4 Cancel

Only the exact local active Token is handled:

```text
clear VisualFinishTimer
→ type-specific historical restore
→ remove presentation-only transient state
→ clear local ownership
→ never Notify
```

### 10.5 Widget destruction

The base class intentionally does not dispatch visual Cancel from `NativeDestruct`;
it uses `NotifyWidgetLost` for authoritative catch-up. The Native HUD therefore
clears only its own timer and local transient ownership before calling `Super`:

```cpp
void UBattleHUDWidget::NativeDestruct()
{
    ClearVisualFinishTimer();
    CleanupPresentationOnlyTransientState();
    Super::NativeDestruct();
}
```

Destruction does not historical-restore, does not call the Cancel override and does
not Notify completion.

### 10.6 Synthetic fixtures

Contract-valid synthetic Records may test:

```text
router and payload validation
true / false
timer
duplicate Finish
Cancel
stale Token
old/new Token isolation
Widget destruction cleanup
```

Ownership tests should call public `PlayPresentationRecord(...)`; direct
`_Implementation()` calls may test isolated handlers but cannot prove the base
tracked-token contract. Synthetic fixtures are not final PIE evidence.

### R5 acceptance

```text
valid async Begin returns true
invalid/unmigrated Begin returns false with zero side effects
normal Finish produces one exact Notify
duplicate Finish no-op
Cancel produces zero Notify
timer after Cancel no-op
old callback cannot affect new Token
NativeDestruct clears local timer/transients without Notify
Legacy playback regression unchanged
```

---

## 11. R6 — Simple frozen Records

Migrate in order:

```text
EnergyChanged
BlockChanged
DeckShuffled
```

For each Record, complete together:

```text
payload-specific validation
required historical Before-state validation
frozen After rendering
normal Finish
Cancel historical restore
invalid fallback
stale callback
real PIE parity
```

Do not invent a universal Before-state rule; reproduce the sealed validation contract
for each payload. Energy has no `Reason`. DeckShuffled does not fabricate per-card
shuffle Records.

### R6 acceptance matrix

| Case | Required result |
|---|---|
| valid playback | async true |
| invalid target/value | false, zero side effects |
| normal Finish | one exact Notify |
| duplicate Finish | no-op |
| Cancel | historical restore |
| Cancel Notify count | zero |
| stale Token | no-op |
| next Record | unaffected |
| real PIE | Legacy semantic parity |

---

## 12. R7 — Damage

Damage is isolated because it combines target identity, multiple frozen values,
transient feedback, timer and historical Cancel restore.

Consume directly:

```text
TargetPresentationId
IncomingDamage
HPBefore / HPAfter
BlockBefore / BlockAfter
```

Never derive `HPAfter` from `IncomingDamage`.

Prefer a typed weak target reference or PresentationId rather than a Player/Enemy
boolean:

```cpp
TWeakObjectPtr<UBattleHUDCombatantPresentationWidgetBase>
    ActiveDamageTargetWidget;
```

### R7 acceptance

```text
Enemy target
Player target
Block absorption
IncomingDamage > 0 while HP is unchanged
lethal Damage
Cancel during active window
stale timer
invalid PresentationId false fallback
next Record unaffected
```

---

## 13. R8 — Card lifecycle

Migrate together:

```text
CardPlayed
CardZoneChanged
```

They form one visible lifecycle but remain distinct committed facts.

### 13.1 CardPlayed

Validate the sealed payload contract:

```text
RuntimeId
CardId
HandIndexBefore
EnergyBefore
EnergyAfter
```

Visual behavior:

```text
hide exact formal Hand Widget
→ create frozen presentation-only Card
→ place it in PlayArea
```

CardPlayed already includes the paid card cost. It must not generate or simulate an
additional EnergyChanged visual.

### 13.2 Hand to DiscardPile

```text
Begin  → hide exact Hand Widget
Finish → do not restore and do not proactively rebuild Hand
Cancel → restore the historical exact Hand Widget to Visible
```

The reducer/ViewModel refresh owns the formal committed removal after Finish.

### 13.3 DrawPile to Hand

```text
Begin
→ validate RuntimeId, CardId, count and ToIndex
→ create frozen noninteractive presentation Card

Finish
→ do not proactively remove it
→ do not convert it into Gameplay-playable state
→ reducer/ViewModel catch-up and formal Hand refresh take ownership

Cancel
→ remove the presentation-only Card
→ restore historical display
```

### 13.4 PlayArea destination

Support only the current producer set:

```text
PlayArea → DiscardPile
PlayArea → ExhaustPile
PlayArea → RemovedPile
```

Unknown zone pairs return false. Do not mutate unrelated pile display early.

### R8 acceptance

```text
Scenario A
Scenario C
duplicate RuntimeId rejection
wrong CardId / FromIndex / ToIndex
unknown zone pair false fallback
no transient leak
Draw presentation Card cannot receive input
Finish does not roll back committed visual
Cancel restores historical Hand
CardPlayed does not duplicate EnergyChanged
```

---

## 14. R9 — Native Status Widget and StatusChanged

Create:

```text
UBattleStatusWidget
WBP_BattleStatus_Native
```

The native Widget stores the current frozen `FBattleHUDStatusView` and exposes native
identity getters. It owns no Status lifecycle rule and queries no Gameplay object.

Exact identity remains:

```text
TargetPresentationId
+ StatusId
+ RuntimeSequence
```

Resolve it by:

```text
TargetPresentationId
→ choose Player or Enemy formal Status container
→ find StatusId + RuntimeSequence inside that container
```

Do not add `TargetPresentationId` to `FBattleHUDStatusView` merely for Widget lookup.
Do not identify by StatusId alone, array index or DisplayName.

Support:

```text
0 → N create
A → B increase
A → B where B > 0 reduction
A → 0 removal
```

Update/reduction reuses the exact Widget. Removal hides the exact Widget; it does not
remove an arbitrary same-StatusId row.

Cancel does not calculate `B → A`. It rebuilds both formal Status rows from the
historical ViewModel.

### R9 acceptance

```text
creation
increase
reduction
2 → 1 → 0
same StatusId with a new RuntimeSequence
no duplicate
no A → B → A flashback
wrong target false fallback
Cancel historical rebuild
stale callback no-op
```

---

## 15. R10 — Terminal and PresentationUnavailable rendering

Migrate:

```text
Victory
Defeat
ResolutionFault
```

Preserve ordering:

```text
preceding visible Records complete
→ Terminal Record begins
→ formal terminal visual
→ exact Notify
→ Controller reducer
→ FinalSnapshot reconciliation
```

A Gameplay Outcome already existing does not authorize the HUD to reveal a future
terminal surface before the Terminal Record reaches the visual head.

### 15.1 PresentationUnavailable

PresentationUnavailable is not a Record and is not HUD authority. Its formal path
remains:

```text
Controller / Presenter
→ ViewModel.EnterPresentationUnavailable
→ ViewModel Changed
→ Native HUD rendering
```

The Native HUD may own only a pure rendering helper such as:

```text
RefreshPresentationAvailabilityFromViewModel
```

It must not:

```text
call EnterPresentationUnavailable
derive the failure reason
generate ResolutionFault
treat unavailable as a Terminal Record
```

### R10 acceptance

```text
lethal Enemy Damage visible before Victory
lethal Player Damage visible before Defeat
ResolutionFault isolated
PresentationUnavailable does not show ResolutionFault
active Terminal Cancel restores historical terminal surface
stale Terminal callback no-op
input remains locked until catch-up
```

---

## 16. R11 — Dual-stack candidate parity

The production configuration is still Legacy. Run both explicit configurations with
the same real Gameplay/UI Request producers:

```text
Legacy configuration
Native candidate configuration
```

Both run:

```text
Scenario A
Scenario B
Scenario C
Scenario D
Scenario E
active Skip
active Cancel
stale callback
Input Unlock
```

Compare observable contracts:

```text
Record acceptance/rejection
frozen display facts
visible Record order
Cancel historical result
Hand / Energy / HP / Block / Status / pile display
FinalSnapshot surface
terminal timing
input-unlock timing
final Idle/Terminal state
```

Do not require identical private helpers, timer implementation, transient construction
or internal call order. Final PIE evidence must use real producers; synthetic Records
cannot close parity.

### R11 candidate gate

```text
Legacy regression PASS
Native Scenario A-E PASS
active Skip/Cancel PASS
stale callback PASS
input-unlock PASS
focused native handler tests PASS
Editor build PASS
Blueprint compile/save PASS for all Native WBP assets
```

---

## 17. R12-A — One-time production cutover

Only after R11 passes, change the unique production configuration:

```text
WidgetClass

WBP_BattleHUD
→ WBP_BattleHUD_Native
```

The cutover is an isolated commit. It contains no Gameplay, Presentation, cleanup,
asset deletion or unrelated refactor.

If the cutover fails validation, restore only the production WidgetClass in a new
recovery commit. Do not destructively reset the already-reviewed Native implementation.

---

## 18. R12-B — Cutover-head acceptance

Pre-cutover candidate evidence is not enough. The production cutover commit itself
must pass from the formal production configuration:

```text
Editor build
Native WBP compile/save/reopen
Scenario A-E real PIE
active Skip/Cancel real PIE
stale callback and Input Unlock
Phase6UIA2D5 exact expected count
formal Phase6R aggregate
clean-worktree Shipping exclusion
```

Only this cutover-head evidence may mark the Native HUD as the formal default path.
Documentation closure follows validation; it does not alter the tested implementation
asset or C++ behavior.

---

## 19. R13 — Objective stabilization

Legacy assets remain present after cutover. Cleanup is allowed only after all of the
following are true:

```text
Native HUD remains the production default through an explicitly named milestone
no Legacy runtime fallback occurred during that milestone
at least one later UI change was implemented only in the Native stack
Scenario A-E passes again
active Skip/Cancel passes again
Phase6R passes again
Shipping exclusion passes again
asset-reference audit finds no formal runtime Legacy HUD/Card/Status dependency
```

The milestone and its evidence must be named before R13 begins; “one stable cycle” is
not a sufficient acceptance statement.

---

## 20. R14-A — Safe cleanup

Without deleting Legacy assets, a separately reviewed cleanup may:

```text
remove migration-only compatibility code
remove confirmed-unreferenced helpers
remove abandoned code from the Native stack
update AGENTS and durable documentation
```

Every cleanup slice keeps Editor build, focused Automation and PIE smoke evidence.

Legacy WBP assets remain intact.

---

## 21. R14-B — Separately authorized Legacy removal

Legacy asset removal is destructive and requires a separate explicit user request.

Before removal:

```text
Reference Audit
Redirector Audit
runtime asset dependency audit
recovery point
```

Only then may the approved exact assets be deleted, followed by:

```text
Fix Redirectors
Editor build
Automation
Scenario smoke PIE
package / Shipping validation
```

If removal has no concrete maintenance or packaging benefit, Legacy assets may remain
indefinitely in a clearly documented deprecated area.

---

## 22. Validation cadence

Ordinary coherent C++/WBP slice:

```text
implement smallest complete behavior
→ regenerate project files when required
→ Editor build
→ compile/save affected Native WBP assets
→ focused Automation for the changed contract
→ smallest real PIE parity scenario
```

Meaningful batch boundary:

```text
one independent architecture review
→ one focused regression scope
→ update migration baseline/checkpoint once
```

Final candidate and cutover heads:

```text
Scenario A-E
→ active Skip/Cancel/Input Unlock
→ expected A2D5 discovery count
→ formal Phase6R aggregate
→ clean-worktree Shipping exclusion
```

Automation does not replace Blueprint compile/save or real PIE. Synthetic fixtures do
not replace real producer ordering evidence.

During implementation, `docs/CODEX_GOAL_CHECKPOINT.md` records only current execution
state: actual HEAD, completed R phase, exact next action, blockers and tests already
run. This dedicated document remains the migration contract and phase order.

---

## 23. Rollback boundaries

| Boundary | Production path | Recovery |
|---|---|---|
| R0-R11 | Legacy | stop Native work; Legacy remains untouched |
| R12-A | Native after isolated config commit | restore WidgetClass only |
| R12-B failure | restore Legacy production config | keep Native code for diagnosis |
| R13 | Native, Legacy assets retained | explicit Legacy fallback if required |
| R14-A | Native, Legacy assets retained | revert cleanup slice |
| R14-B | Native, Legacy removal authorized | use approved recovery point |

Never use deletion as the migration mechanism and never remove the last known-good
Legacy path before cutover-head validation.

---

## 24. Prohibited changes

Throughout A2N, do not:

```text
modify Presentation Record or Envelope schema
change Controller reducer or timeout authority
move Gameplay authority into UI
query mutable Gameplay from the HUD
make Card Widget depend on a concrete HUD WBP
identify Status by StatusId alone
roll historical display backward during normal Finish
Notify completion from Cancel
return true without starting valid async playback
create a second Controller/Widget assembly path
add a player-visible Legacy/Native toggle
delete Legacy before cutover-head validation and stabilization
start UI-A3 Preview in parallel
mix unrelated Gameplay or Presentation redesign into migration commits
```

---

## 25. Execution order

```text
R0    Baseline + Native test injection
↓
R1    Backward-compatible base native hook
↓
R2    Native HUD shell
↓
R3-A  Static HUD + long-lived delegates
↓
R4    Native Card Widget + Hand + card input
↓
R5    Playback kernel + destruction cleanup
↓
R6    Energy / Block / Shuffle
↓
R7    Damage
↓
R8    Card lifecycle
↓
R9    Status
↓
R10   Terminal + PresentationUnavailable rendering
↓
R11   Dual-stack candidate parity
↓
R12-A One-time production cutover commit
↓
R12-B Cutover-head full acceptance
↓
R13   Objective stabilization
↓
R14-A Safe cleanup
↓
R14-B Separately authorized Legacy removal
```

Risk increases in this order:

```text
static display
< simple frozen values
< targeted transient feedback
< Card ownership and zone lifecycle
< exact Status identity
< Terminal and global sequencing
```

---

## 26. Completion definition

The Native ownership migration is complete only when:

```text
WBP_BattleHUD_Native owns no business EventGraph behavior
WBP_BattleCard_Native does not know a concrete HUD
WBP_BattleStatus_Native owns no Status lifecycle rule
HUD never queries mutable Gameplay
Controller authority is unchanged
all Records preserve exact-token behavior
all Cancel paths restore historical display without Notify
all stale callbacks are harmless
Native Scenario A-E matches Legacy presentation semantics
active Skip/Cancel and Input Unlock pass
focused and aggregate Automation pass
clean-worktree Shipping exclusion passes
production WidgetClass points to the Native WBP
cutover-head validation passes
Legacy remains recoverable through the agreed stabilization boundary
```

The migration changes only who implements the sealed UI behavior. It must not change
what that behavior means.
