# Native card source and pointer selection amendment

Date: 2026-10-06. Implementation base: `de75414`, current branch unchanged.
User-authorized extension of amended G9-B. G9-C–F remain unstarted; default
activation and remaining mandatory-choice acceptance gates remain pending.

## Behavior and ownership

CardPlayed starts at the visible source pose, established during preparation,
before the first Slate paint. Use current arranged Hand geometry (including
hover/pointer render transforms), never the fixed Hand fallback as a production
origin. Copy the center, rendered size and angle into a cosmetic receipt in HUD
coordinates. PlayArea is the committed visual owner after accepted preparation;
the exact historical Hand slot becomes Hidden. Existing Blocking timing,
reducer validation and frozen card faces remain unchanged.

Selecting a Skill or Power follows the mouse; an Attack with TargetType=None
uses the same presentation policy for future untargeted AOE content. This adds
no AOE Gameplay effect or card. Enemy-target Attacks retain their target arrow.
Right click cancels only the ordinary draft and immediately restores the fan
pose. Left click confirms Self through the existing player-target Request,
or None through the existing confirm Request. Enemy-target Skills still require
their legal enemy click. EndTurn and explicit controls retain their own routes.
Mandatory selection never enters this pointer flow.

The formal Hand Widget/slot/Slate tree stays attached. The fan panel reserves
only render transforms and paint layers for pointer-controlled cards; its
ordinary allotted-size arrangement and frozen order remain authoritative.
Hover cannot overwrite those transforms. A transparent HUD backdrop below all
authored controls gives blank-viewport clicks the same preview route without
adding a competing PlayerController left-click listener.

Confirmed FIFO entries optionally carry a UI-only visual origin. Gameplay and
ViewModel Request validation ignore that cosmetic data. A pointer-confirmed
card waits at its frozen center without energy/zone mutation, then transfers
its receipt to the single submitting Request. Battle, ViewModel, Controller,
session, card identity and the ordered input credential scope that receipt;
accepted CardPlayed consumes it into its exact existing playback token. Queue
clear drops all pending receipts; already submitted Gameplay keeps its normal
history. Busy retry preserves the same receipt. Rejected requests discard it.
Viewport changes rebase normalized centers; panel membership/base geometry is
never rewritten by input Tick. Missing production source geometry declines
through the existing presentation fallback rather than inventing a position.

## Execution evidence

The implementation is a local batch on base `de75414`, not a sealed G9 stage.
Prescribed bundled project generation PASS; final UE 5.8 Development Editor
Win64 build PASS (6.93 s). Logs: `Saved/Logs/G9PointerProjectFiles.log` and
`G9PointerFinalBuild.log`. Initial builds exposed local Slot-name shadowing and
a headless FGeometry constructor import; both were repaired without dependencies.
A later build was blocked by active editor Live Coding; closing this run's
editor permitted the prescribed build. Failed builds are not passing evidence.

Focused Automation initially selected 68 cases: HandInteraction, G9-B, G8-B,
FastInput, R8, frozen CardPlayed handoff, CardSelection.Presentation and affected
G0/G4/G5/G6/G7. It completed 57 (56 passed, one failed) before a turn-end GC
fixture crash; no complete report exported (`G9PointerAutomation.log`). The new
pointer fixture needed an explicitly unlocked input state. The turn-end fixture
now removes its raw capture delegate and shuts down its Controller/HUD before
storage/World teardown; its GC/cleanup case passes after this lifecycle repair.

Affected/unfinished 12-case rerun: 11 success, one fail (`G9PointerRepair`).
The real Native no-Tick fixture lacked initial allotted geometry; it now supplies
Slate allocation without calling NativeTick. After removing the remaining
non-fan fixed-origin fallback, the affected 16-case run had 12 success, one
expected warning and three failures (`G9PointerFinal`). All three were old R8
fixtures without retained source Slate/allocation; those fixtures now retain
their Hand Slate tree and provide source geometry. Final R8 is **5 success,
one expected warning, zero failures/notRun** (`G9PointerR8`). The Native no-Tick,
pointer source and FIFO cases passed in the 16-case run.

An additional request test covers Self/None with G9 both enabled and disabled,
ordinary Gameplay cost/effect/consumption and repeated confirmation. Its initial
None fixture incorrectly authored a Self-only Block effect; corrected authored
data passes **1/1** (`G9PointerConfirmFinal`). The source test also checks missing
geometry declines with no PlayArea child, hidden source or energy mutation,
followed by first-paint pose and exact-token completion. Final receipt is below.
Reports live in `Saved/AutomationReports/<name>/index.json`; logs use matching
`Saved/Logs/<name>Automation.log`. Passing scopes are reused only when unaffected.
There is no claim of one uninterrupted aggregate run.

## Native PIE and remaining visual gate

Actual production map: `/Game/SlayTheSpireDemo/Maps/L_Battle_RuinedCitadel`, Native
asset parent Selection HUD, D3D12, ordinary 0.5 s Blocking card timing. C++ G9
default remains false; this preserved user asset/live run uses its existing true
option. User asset SHA256 remained
`F896F7B59A91D4EC86AD73973451E19012B7523EE7D1D1ECEE2B570894E20F81`.
No asset/config/plugin/Legacy changes are part of this batch.

MCP started actual PIE; Windows control selected Warcry. VM readback proves its
draft (`ChoosingTarget`, RuntimeId 11, Energy 5, unlocked). Native fan/draft and
later PlayArea cards were visible. Other continuous inputs changed the battle
and PIE subsequently stopped, so those observations do not prove a controlled
pointer/queue motion timeline. Zero CardPlayedReject entries occurred in that
run. Evidence: `Saved/Logs/G9PointerPIE.log`, `G9PointerStartPIE.json`,
`G9PointerBindings.json`, `G9PointerSkillDraft.json` and tool screenshots in this
conversation. The final non-fan contract tightening does not replace missing
visual proof. Editor is closed after the required final build.

**USER ACTION REQUIRED:** on that map, retain one continuous recording showing:

1. Strike aims at the enemy and travels from its visible Hand location into
   PlayArea, with no initial flash from a fixed position.
2. Warcry and Inflame follow the cursor to two different locations. Right click
   restores the same card to its fan slot; selecting again and left clicking a
   blank battle location plays from that cursor position. Only one Request/cost
   occurs. Enemy-target Skills still require their exact enemy click.
3. During A's playback, confirm a pointer card B, optionally C, then move the
   cursor away. B waits at its confirmed center and starts from that center when
   its turn arrives. Resize during the wait/playback and verify continuous
   placement, later hover and unchanged Hand order.
4. Enter a mandatory choice and verify ordinary mouse following/left confirmation
   cannot bypass it. The previously pending full queued-mandatory clearing gate
   remains in `G9BRevisionNativePIE.md`.

Record commit HEAD, actual G9 option, viewport and result. Future AOE Gameplay
content is absent; its None-target pointer policy is automated, not a visual
content acceptance claim. No G9 default activation, C–F work or seal.

Final source/decline receipt: **1/1 PASS**, zero failures/notRun
(`G9PointerSourceFinal`). Across the initial scope, repaired affected runs and
the added request test, **69 distinct tests have valid passing evidence**.
This is not a single 69/69 run. `Saved/G9PointerPassingCases.txt` lists the paths.
Status: **IMPLEMENTED / BUILD PASS / AFFECTED AUTOMATION PASS / PARTIAL PIE /
USER ACTION REQUIRED / NOT SEALED**.
