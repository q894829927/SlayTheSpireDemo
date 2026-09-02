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
A3-4 ViewModel Transient Preview Lifecycle: COMPLETE / REVALIDATED / SEALED
A3-5 Native card-face Preview + A2/A3 PIE: VALIDATED / SEAL PENDING
```

A production PIE regression was traced back to A3-4 revision invalidation rather than the sealed A2 CardPlayed implementation. A3-5 also exposed two real ownership problems during diagnosis (standalone Preview sharing `OV_PlayArea`, and Preview hover using structural `OnChanged`), both of which remain fixed, but neither was the final cause of the missing CardPlayed animation.

The A3-4 regression fix has now been revalidated by both focused Automation suites and production PIE. CardPlayed animation is restored.

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
A3-4 regression-fix Automation SlayTheSpireDemo.UIA3.ViewModelPreviewLifecycle: 3/3 PASS (user-reported)
```

The original A3-4 focused gate did not cover the interaction between its `OnReadStateReady` revision invalidation broadcast and an A2 Record already started by deferred public Presentation delivery. That regression gap is now covered explicitly: Presentation-owned Ready invalidation may clear stale transient state but must not emit structural `OnChanged`.

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

Current regression-fix evidence:

```text
SlayTheSpireDemo.UIA3.ViewModelPreviewLifecycle: 3/3 PASS (user-reported, 2026-09-02)
SlayTheSpireDemo.UIA3.NativePreviewIntegration: 3/3 PASS (user-reported, 2026-09-02)
Production L_BattleTest PIE: PASS for restored CardPlayed animation (user-reported, 2026-09-02)
```

Result:

```text
A3 transient Preview invalidation no longer cancels committed A2 CardPlayed playback.
CardPlayed visible animation is restored in production PIE.
```

## Next exact action

Do not make further behavioral changes to A2 playback or A3 Preview ownership for this defect. The regression is fixed and revalidated.

The remaining A3 administrative action is to review the final A3-5 acceptance evidence and seal A3-5 / UI-A3 if no additional PIE acceptance defect is reported.

Do not run Phase6R, A2D5, Shipping, broad Scenario suites or Legacy parity unless a concrete failure invalidates a sealed shared contract. Do not start Phase 7 until A3-5 is sealed.
