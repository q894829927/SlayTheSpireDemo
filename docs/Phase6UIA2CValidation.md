# Phase 6UI-A2C Validation Record

Date: **2026-08-22**

Validated main revision before this record commit:

```text
b5b52d14237a92c8ed88f7a5e79fbe720ec78a0a
test
```

This revision includes the A2C runtime/test implementation, the reducer-valid A2B playback fixture correction, and the follow-up null-source / rollback expectation hardening that was present when the successful rerun was reported.

## Result

The repository owner confirmed that the UE5.8 self-hosted Phase6R aggregate automation rerun completed successfully on the revision above.

The aggregate gate is configured to require exactly:

```text
SlayTheSpireDemo.Phase5        13
SlayTheSpireDemo.Phase6A       23
SlayTheSpireDemo.Phase6B       12
SlayTheSpireDemo.Phase6C        5
SlayTheSpireDemo.Phase6UIA2A    8
SlayTheSpireDemo.Phase6UIA2B    8
SlayTheSpireDemo.Phase6UIA2C    8
---------------------------------
Total                           77
```

The workflow gate rejects discovery-count mismatch, non-success tests, failed/not-run results, or non-zero Unreal Editor exit status. The successful rerun is therefore recorded as the current C++/Automation validation evidence for UI-A2C.

## UI-A2C validation status

```text
Design contract                    LOCKED
A2C C++ implementation             PASSED
Static source review               PASSED
UE5.8 Editor build                 PASSED
Focused Phase6UIA2C Automation     PASSED 8/8
Affected Phase5-Phase6UIA2C gate   PASSED 77/77
WorkingSnapshot reducer regression PASSED
A2A/A2B regression in same gate    PASSED
Blueprint Damage/Block playback    PENDING
Blueprint Card/Energy/Zone visual  PENDING
PIE smoke                          PENDING
```

UI-A2C C++ validation is closed for this source revision. This does not claim visible Blueprint animation or PIE integration completion.

## Preserved boundaries

- Gameplay remains authoritative and does not wait for presentation playback.
- `CardPlayed` remains the composite Hand->PlayArea + card-cost fact; no duplicate card-cost `EnergyChanged` or Hand->PlayArea `CardZoneChanged` is emitted.
- Other card movement remains represented by `CardZoneChanged` with frozen card values.
- `DeckShuffled` remains a batch semantic fact recorded before `FDeckShuffledEvent` dispatch.
- Controller owns `DisplayedPresentationSnapshot` / `WorkingPresentationSnapshot`; Blueprint does not mutate historical Hand, Energy, HP, Block or pile counts.
- Invalid current-writer presentation history remains fail-soft and cannot roll back committed Gameplay.
- The earlier A2B visual Blueprint/PIE work is still intentionally deferred and is not counted as complete by this validation record.

## Next slice

With A2C C++ validation closed, the next source slice should be design-locked before implementation. The existing UI-A2 contract leaves Status/Relic presentation and terminal/fault presentation polish outside A2C; those boundaries should be reviewed against current source before UI-A2D code begins.
