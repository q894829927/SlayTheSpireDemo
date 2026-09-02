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
A3-2A Immediate Preview DTO + Effect contribution: COMPLETE / VALIDATED / SEALED
A3-2B BattleManager Query + identity stamping: COMPLETE / VALIDATED / SEALED
A3-3 Energy + Target-Aware Legality: IMPLEMENTED / VALIDATION PENDING
A3-4 ViewModel Transient Preview Lifecycle: NOT STARTED
A3-5 Minimal Native UMG + A2/A3 Combined PIE: NOT STARTED
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

## A3-2 sealed evidence

A3-2A implementation:

```text
08f878e9f4f74b438985d187884c877d613617af  feat(ui-a3): add immediate preview effect contributions
```

A3-2B implementation:

```text
a796709034427d470881b6a7d01f2701305406b7  feat(ui-a3): expose immediate preview query
57d0cc0db45089cf025310002806f944bf59769f  feat(ui-a3): build immediate preview query
ebf3838b188b9ccf66e40df549d1fa3665fddd6a  test(ui-a3): cover immediate preview query assembly
```

Validation:

```text
A3-2A Editor Build: PASS
A3-2A Automation SlayTheSpireDemo.UIA3.ImmediatePreview: 3/3 Success, exit 0
A3-2B Editor Build: PASS
A3-2B Automation SlayTheSpireDemo.UIA3.ImmediatePreviewQuery: 2/2 Success, exit 0
Manual PIE: NOT REQUIRED
```

A3-2 is therefore COMPLETE / VALIDATED / SEALED.

## A3-3 implementation state

Implementation commits after the A3-2 seal:

```text
fa7832e07e78a2b2e0b23ce9835b95b95ea25ed2  feat(ui-a3): add preview legality and energy
fca059cd150a8f1d004068e95cbc4573e3438749  test(ui-a3): keep A3-2B gate compatible with pre-target preview
874013d7f62744a514459398e406f650d1aa2720  test(ui-a3): cover preview legality and energy
```

Implemented behavior:

```text
Target == nullptr
-> coherent pre-target Preview
-> Validation = QueryCardPlayability(Card)
-> TargetPresentationId = None
-> target-specific Damage contribution remains absent until a concrete target exists

Target != nullptr
-> target identity must resolve to the current battle
-> Validation = QueryPlayCard(Card, Target)

EnergyBefore = current authoritative Energy
EffectiveCost = Card->GetCurrentCost()
EnergyAfter is valid only when Validation.bAllowed and current Energy covers EffectiveCost
rejected Preview leaves bHasEnergyAfter=false and does not fabricate negative EnergyAfter
normal InvalidTarget / NotEnoughEnergy are DTO validation outcomes, not build failures
RequestPlayCard remains authoritative and is not called by Preview construction
```

New focused A3-3 Automation prefix:

```text
SlayTheSpireDemo.UIA3.ImmediatePreviewLegality
```

Expected tests:

```text
PreTargetUsesPlayabilityAndBoundTargetUsesPlayCard
InsufficientEnergyHasNoEnergyAfterAndRequestStillRejects
```

The A3-3 implementation intentionally extends the same public Preview query so null Target is now a coherent pre-target state. This invalidates the old A3-2B test assertion that null Target was a transport failure. That assertion was removed while the A3-2B target-bound identity/order/read-only coverage was retained.

## Validation actually performed for A3-3

No new UE validation is claimed yet for the A3-3 code.

```text
Editor Build: NOT RUN for A3-3
SlayTheSpireDemo.UIA3.ImmediatePreviewQuery compatibility rerun: NOT RUN
SlayTheSpireDemo.UIA3.ImmediatePreviewLegality: NOT RUN
Manual PIE: NOT REQUIRED for A3-3
Phase6R / A2D5 / Shipping / broad Scenario / Legacy parity: intentionally NOT RUN
```

## Next exact action

Run only the closed-scope A3-3 Gate:

```text
1. Editor Build once.
2. Rerun the invalidated A3-2B compatibility prefix exactly once:
   SlayTheSpireDemo.UIA3.ImmediatePreviewQuery
   expected: 2 tests
3. Run the new A3-3 prefix exactly once:
   SlayTheSpireDemo.UIA3.ImmediatePreviewLegality
   expected: 2 tests
4. Both prefixes must be all Success and Automation must exit 0.
5. Record exact evidence and seal A3-3.
```

Do not rerun A3-2A, Phase6R, A2D5, Shipping, broad Scenario suites or Legacy parity unless a concrete failure invalidates them.

Do not start A3-4 until this Gate passes.
