# Selection Presentation G6 Execution

Date: **2026-09-10**

Status:

```text
IMPLEMENTED /
DEVELOPMENT EDITOR BUILD PASS /
FOCUSED G6 AUTOMATION 4/4 PASS /
NATIVE L_BATTLETEST PIE PENDING /
NOT SEALED
```

G6 is the production activation stage for safe N-child Selection Presentation Group playback. It changes visible concurrency only. Gameplay mutation order, BattleEvent/trigger order, `PresentationSequence` order and chronological reducer order remain authoritative and serial.

The sealed G5 sequential path remains the mandatory correctness fallback.

## Implementation lineage

G6 production work landed on `main` through the following focused commits:

```text
cedd3581779f154f3fc228290e9f3c45a7452250  feat(g6): add atomic card ownership transfer surface
55ba87200a2c03c5bf98b55df4143391c1d77e6e  feat(g6): implement atomic ownership batch transfer
faaa6eff428b49debdb1873fd2b1c5a3d1d56001  feat(g6): expose native group transition hooks
76b07d6b0d6cf859a63ee363385da6b78f5ca7c4  feat(g6): implement transactional native group transitions
0a35751a1ee749b5169b9c779d1b341c6272df85  feat(g6): add tracked record-to-group upgrade boundary
decdc4f712ebe83740e0e7972c28ac9bf9860377  feat(g6): implement tracked group upgrade and completion
cd13db609546bf4061c8ab197cd9df78190cdf75   feat(g6): add production group activation state
5c2920700046d2b19eaee7dbc612c02908017773   feat(g6): activate semantic groups without reducer reordering
08628ca87459f2dbb363d3dbf221fa8ffd533f3d   feat(g6): route selection leaders through group activation
85f62cb1e0580f0b04a145a0997c1d92275b15ec   feat(g6): activate groups at selection playback boundary
f5f1a3c863a3f6054d7680cf58209b9e93092c11   fix(g6): complete groups through chronological reducer path
e9b566a7ca0db80e11e0cba70542af25480ba416   fix(g6): match offered records by immutable identity
```

A pre-existing include dependency was exposed by the changed Unity compilation layout and fixed separately:

```text
e7f722ca666a96b08258b9a59ef516cffc1b903f
fix(presentation): include record writer definition
```

Focused G6 test support then landed through:

```text
7efaee4052a96cd04b3fdfd8cecb0bfd1467bf6a  test(g6): add controller playback probe
a77470bb01924f5e983736f27b511b7a04e45b20  test(g6): implement controller playback probe
c12455742ba5d41eda0a2bca09d7f045265ea0ab  test(g6): cover parallel playback contracts
```

The later `531d90b858ea74823af2e2a58d08dec2d71cd6df` documentation commit is unrelated to G6 runtime behavior and remains preserved on top of the implementation/test commits.

## Implemented behavior

### Transactional visual preflight

For an explicit G2-approved `SelectionDestination` Group, the Native card-transition surface prepares every member before durable ownership mutation.

```text
prepare A
prepare B
prepare C

any failure
→ restore all prepared SelectionArea visuals
→ transfer zero ownership entries
→ decline Group
→ continue through sealed G5 SingleRecord playback
```

No partial visual cohort is accepted.

### Atomic ownership transfer

The ViewModel exposes a G6 batch ownership transaction. Every member is validated before mutation, then the whole cohort advances together and emits one ownership publication.

Accepted Group start:

```text
A/B/C SelectionArea(Confirmed)
→ atomic batch
→ A/B/C Transition
```

Visual completion:

```text
A/B/C Transition
→ atomic batch
→ A/B/C ConsumedPendingReducer
```

This transient ownership state is Presentation-only and does not mutate Gameplay zones.

### N-child same-tick playback

G6 reuses the G4 generic card-transition child engine and its `TArray<FNativeCardTransitionInstance>`. It does not introduce Effect/CardId-specific animation branches.

Once all children and destinations are accepted, the cohort is committed together and the N transition children begin from their exact current SelectionArea visuals in the same Native playback start.

### Controller activation and reducer chronology

The Selection playback boundary offers the ordinary leader SingleRecord first. The Controller then uses the sealed G2 semantic candidate to attempt an exact tracked-unit upgrade:

```text
tracked SingleRecord leader
→ Group candidate + visual preflight accepted
→ exact tracked Group token
```

If the upgrade is declined, the original SingleRecord tracked token is restored without a cancellation side effect and G5 proceeds normally.

On successful Group visual completion, only the leader record is reduced immediately. Future Group member indices are remembered as already visually presented. When chronological playback later reaches those records, their visual replay is suppressed but their reducer step still executes in normal record order.

Therefore an interleaved record remains ordered, for example:

```text
A(group) visual cohort begins with B/C
→ reduce A
→ reduce unrelated Damage
→ reach B(group), skip duplicate visual, reduce B
→ reach C(group), skip duplicate visual, reduce C
```

G6 never skips or reorders authoritative records.

### Exact completion path

G6 uses a dedicated exact Group completion callback rather than the historical G3 dormant Group-completion recovery path. Group timeout, cancellation, widget replacement and collapse remain protected by existing exact-token/scoped recovery behavior.

## Automated validation

### Development Editor build

**PASS / user confirmed 2026-09-09 local execution.**

The initial G6 implementation exposed an unrelated incomplete-type compile dependency in `EnergyPresentationRecord.cpp`; adding the complete `FPresentationRecordWriter` definition include resolved it. The subsequent UE 5.8 `SlayTheSpireDemoEditor Win64 Development` build passed.

### Focused G6 Automation

**PASS / user confirmed 2026-09-10.**

Command scope:

```text
SlayTheSpireDemo.SelectionPresentation.G6
```

Current inventory is **4/4 PASS**:

```text
SlayTheSpireDemo.SelectionPresentation.G6.BatchOwnershipAtomicity
SlayTheSpireDemo.SelectionPresentation.G6.TrackedUpgradeRollback
SlayTheSpireDemo.SelectionPresentation.G6.ParallelVisualSerialReducer
SlayTheSpireDemo.SelectionPresentation.G6.GroupDeclineSequentialFallback
```

Coverage proves:

- batch ownership validation is all-or-nothing;
- the formal lifecycle advances through `ConsumedPendingReducer`;
- failed Group upgrade restores the exact leader SingleRecord owner;
- accepted Group visual completion does not reorder the chronological reducer;
- already co-presented future members do not replay visually when their reducer cursor arrives;
- Group decline retains the sealed G5 sequential fallback.

This is protocol/control-flow evidence. Headless Automation does not prove real Slate-frame simultaneity, production geometry, absence of clipping or human-visible flashback.

## Remaining manual Native PIE gate

G6 is **not sealed** until production Native `L_BattleTest` confirms the actual visual cohort.

Required acceptance:

```text
[ ] use a Player multi-select flow with N > 1 selected cards
[ ] after Confirm, all selected destination animations visibly begin together
[ ] no selected card briefly returns to the formal Hand row
[ ] each animation starts from that exact selected card's displayed SelectionArea position
[ ] no duplicate visible card
[ ] no ghost/stale selected card after completion
[ ] no clipping introduced by the simultaneous cohort
[ ] destination/pile result is correct after the animations finish
[ ] input returns normally; no stuck input
[ ] repeat/select again successfully after the first Group completes
```

Also run one ordinary single-selection path (Warcry is suitable) to confirm the G5/G4 SingleRecord behavior remains intact and did not accidentally require a multi-member Group.

## Seal rule

Do not mark G6 `COMPLETE / VALIDATED / SEALED` from build + Automation alone.

If the Native PIE gate passes, update the current status authorities and `docs/Validation.md`, then make G7 cleanup the next active Selection Presentation stage. If PIE fails, preserve the failure as evidence and repair G6 without weakening the G5 sequential fallback.

G8 early input / Presentation pipelining remains out of scope regardless of G6 PIE outcome.
