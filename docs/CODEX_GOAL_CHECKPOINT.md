# Codex Goal Checkpoint — Phase 6UI-A3

Last updated: **2026-09-02**

## Goal

Complete Phase 6UI-A3 without changing Gameplay authority, sealed A2 committed-presentation semantics, Native/Legacy ownership or phase ordering.

```text
A3 = pre-commit read-only current-state supported Operation values
A2 = post-commit playback of immutable facts that actually committed
```

## Current status

```text
UI-A3: IN PROGRESS / AUTHORIZED
A3-1 Dynamic Text: COMPLETE / VALIDATED / SEALED
A3-2 Target-Specific Current-State Preview: COMPLETE / VALIDATED / SEALED
A3-3 Energy + Target-Aware Legality: COMPLETE / VALIDATED / SEALED
A3-4 ViewModel Transient Preview Lifecycle: REGRESSION FIX IMPLEMENTED / REVALIDATION PENDING
A3-5 Native card-face Preview + A2/A3 PIE: IMPLEMENTED / REVALIDATION PENDING
```

A production PIE regression was traced back to A3-4 revision invalidation rather than the sealed A2 CardPlayed implementation. A3-5 also exposed two real ownership problems during diagnosis (standalone Preview sharing `OV_PlayArea`, and Preview hover using structural `OnChanged`), both of which remain fixed, but neither was the final cause of the missing CardPlayed animation.

## Active authority

```text
AGENTS.md
Source/SlayTheSpireDemo/UI/AGENTS.md
docs/Phase6UIA3Implementation.md
docs/Phase6UIA3CardFacePreviewAmendment.md
docs/Phase6UIA3DynamicTextImplementation.md
docs/ValidationExecutionPolicy.md
```

`docs/Phase6UIA3CardFacePreviewAmendment.md` is the later explicit A3-5 UX amendment and controls visible Preview presentation where it differs from the original A3 implementation document.

UI-A2 remains complete/sealed. Native HUD remains the sole active production implementation. Legacy HUD/Card/Status remain retained/deprecated with zero intended production runtime dependency.

## Sealed predecessor evidence

```text
A3-2A Editor Build: PASS
A3-2A Automation SlayTheSpireDemo.UIA3.ImmediatePreview: 3/3 Success, exit 0
A3-2B Editor Build: PASS
A3-2B Automation SlayTheSpireDemo.UIA3.ImmediatePreviewQuery: 2/2 Success, exit 0

A3-3 Editor Build: PASS
A3-3 Automation SlayTheSpireDemo.UIA3.ImmediatePreviewLegality: 2/2 Success, exit 0
A3-3 compatibility rerun ImmediatePreviewQuery: 2/2 Success (user-reported)

A3-4 original Editor Build: PASS (user-reported)
A3-4 original Automation SlayTheSpireDemo.UIA3.ViewModelPreviewLifecycle: 3/3 PASS (user-reported)
```

The original A3-4 focused gate did not cover the interaction between its `OnReadStateReady` revision invalidation broadcast and an A2 Record already started by deferred public Presentation delivery. That omission is now treated as a regression gap; the production behavior must be revalidated before A3-4 is considered sealed again.

Historical shared-contract note: the old `SlayTheSpireDemo.Phase6UIA1.ViewModel` suite contains two stale assertions that mutate CardData/Status after the opening frozen Presentation baseline and expect a later subscriber to see those mutable changes. Do not weaken sealed frozen Presentation semantics to satisfy them.

## A3-5 visible design

The first A3-5 visible Preview used a dynamically created `UBattleImmediatePreviewTextBlock` attached to `OV_PlayArea`. That was a real ownership error because sealed A2 `CardPlayed` owns that container exclusively.

The current design is:

```text
select card
→ hover/focus a legal PreviewTarget
→ BattleManager builds FImmediateCardPreview through current read-only Gameplay pipelines
→ ViewModel stores exact BattleId/StateRevision/Card/Target stamped Preview
→ selected UBattleCardWidget temporarily displays target-specific card-face values
→ leave/unfocus/submit/revision change
→ card face restores its frozen historical description
```

Visible rules:

```text
No standalone Damage preview label
No standalone Block preview label
No standalone Energy-loss preview label
A3 never adds a child to OV_PlayArea
```

Energy/cost remain in the DTO for Gameplay-owned legality only.

### Card-face semantic formatting

A3-1 remains the normal card-face baseline. `FBattleTextResolver` first builds validated current source-side/self semantic arguments, then the target-specific A3 path overrides only semantic names supplied by already-resolved `ImmediatePreview.Operations`.

```text
Strike:   Deal {Damage} damage. -> target-specific Damage replaces {Damage}
Defend:   Gain {Block} Block.   -> current resolved Block replaces {Block}
Uppercut: supported Damage may change while unsupported Weak/Vulnerable values keep normal A3-1 text
```

UI does not parse formatted text and does not rerun Damage/Block formulas.

Each supported operation carries:

```text
BaseAmount     = authored immutable effect amount
ResolvedAmount = current Gameplay-pipeline result
```

Native comparison styling:

```text
ResolvedAmount > BaseAmount  -> red emphasis
ResolvedAmount < BaseAmount  -> blue emphasis
ResolvedAmount == BaseAmount -> original description style
```

The current Native Designer uses a plain `UTextBlock` for the description, so this C++ slice colors that description surface as a whole. Exact per-number run coloring requires a later RichText Designer migration; it must not reintroduce a separate Preview overlay or Gameplay calculations in UMG.

## Final CardPlayed regression root cause

The missing animation was introduced by A3-4 commit `5552e382e279090a2afb7794cd85763d4434dc4a` (`feat(ui-a3): clear stale preview with viewmodel lifecycle`). It added structural `BroadcastChanged()` when `HandleReadStateReady()` observed a new Gameplay revision, even when the ViewModel was Presentation-owned.

The public boundary order is:

```text
stable Gameplay boundary
→ seal immutable Presentation Envelope
→ deferred TryPublishReadStateReady
→ DrainPendingPublicPresentationDeliveries FIRST
→ PresentationController starts CardPlayed
→ Native HUD creates/owns the played-card visual and timer
→ OnReadStateReady.Broadcast(new revision) SECOND
```

Before the regression fix, the second step then did:

```text
HandleReadStateReady
→ ClearSelectionInternal
→ ClearLiveInputBindings
→ SetResolving
→ structural BroadcastChanged
→ UBattleHUDWidgetBase::HandleViewModelChanged
→ CancelTrackedPresentationPlayback
→ CancelPresentationRecordPlayback
→ remove the already-started CardPlayed visual
```

This exactly explains the PIE symptom of no visible card animation while Presentation timing/delay still existed. The A2 `BattleHUDWidget.cpp` CardPlayed implementation itself was unchanged from the pre-A3-5 stable implementation.

### Implemented fix

Production commit:

```text
583660b41725486cde3d363e749e2a2bbec96d51
fix(ui-a3): keep revision invalidation off presentation refresh
```

When `bPresentationDisplayOwned == true`, new-revision invalidation still immediately clears stale selection, legal targets, live bindings and ImmediatePreview and sets Resolving, but it publishes only `OnPreviewChanged`. It must not publish structural `OnChanged` before the PresentationController advances historical display.

Non-Presentation-owned ViewModels retain the existing structural `OnChanged` behavior.

Regression-test commit:

```text
2cf435bff03a4736d6751316393eaf1ee326093e
test(ui-a3): keep ready invalidation off structural channel
```

The existing `RevisionChangeClearsBeforePresentationCatchUp` test now proves both sides of the contract:

```text
new revision still clears stale selection/legal targets/Preview immediately
Presentation-owned display BattleId/StateRevision do not jump ahead
Interaction becomes Resolving and input locks
structural OnChanged count does NOT increase at the Ready invalidation edge
PreviewChanged count increases exactly once
```

This locks the condition required for an already-started A2 `CardPlayed` visual to survive the later ReadStateReady edge.

## CardPlayed rejection diagnostics

A precise read-only diagnostic remains available only when a Native `CardPlayed` Record is rejected before Controller immediate fallback.

Search prefix:

```text
[BattleHUD][CardPlayedReject]
```

It reports Record/token identity, card snapshot, participant ids, Energy/cost consistency, historical Hand matching and actual `OV_PlayArea` children. Diagnostics do not mutate Gameplay, ViewModel, Widget ownership or Presentation state and must not be used to weaken sealed A2 predicates.

## Validation history

Historical pre-fix evidence:

```text
A3-5 card-face Editor Build: PASS (user-reported)
SlayTheSpireDemo.UIA3.NativePreviewIntegration: 3/3 PASS (user-reported)
production L_BattleTest PIE: FAIL — CardPlayed animation absent, only Presentation delay visible
```

Current A3-4 playback-regression fix:

```text
Editor Build: NOT YET RUN on current head
SlayTheSpireDemo.UIA3.ViewModelPreviewLifecycle: NOT YET RUN on current head
SlayTheSpireDemo.UIA3.NativePreviewIntegration: NOT YET RUN on current head
Production L_BattleTest PIE: NOT YET RUN on current head
```

## Next exact validation

Run only:

```text
1. Development Editor Build once.
2. Run SlayTheSpireDemo.UIA3.ViewModelPreviewLifecycle once; expect 3/3 PASS.
3. Run SlayTheSpireDemo.UIA3.NativePreviewIntegration once; expect 3/3 PASS.
4. Run one production /Game/SlayTheSpireDemo/Maps/L_BattleTest PIE session.
```

PIE acceptance:

```text
Strike / Enemy:
select Strike -> hover/focus Enemy -> selected Strike card-face Damage changes
no separate Damage/Energy preview appears
submit Enemy -> Preview clears
A2 played card visibly moves from Hand toward OV_PlayArea
committed Damage playback follows normally

Defend / Player:
selected Defend card-face Block changes
no separate Block/Energy preview appears
submit -> Preview clears -> committed A2 playback remains coherent

revision invalidation:
old selection/Preview does not survive a new StateRevision
but the Ready edge does not cancel an already-started committed A2 visual
```

If this exact fix still produces a missing visual, do not revisit Preview ownership first. Check whether `[BattleHUD][CardPlayedReject]` exists. If there is no rejection, inspect Native CardPlayed start/cancel/finish logging and Controller timeout state to establish whether any other caller cancels the accepted token.

Do not run Phase6R, A2D5, Shipping, broad Scenario suites or Legacy parity unless a concrete failure invalidates a sealed shared contract. Do not start Phase 7 until A3-5 is validated and sealed.
