# Card Expansion — Wave 1C-C1 True Grit Consumer Plan

Date: **2026-09-08**

Status:

```text
SUPERSEDED / NOT ACTIVE
```

This branch originally planned True Grit as the next consumer after C0. That plan is no longer active because the generalized C0 capability already supports True Grit Gameplay through authored composition:

```text
GainBlockCardEffect
+
USelectExhaustHandCardEffect
  Base     = Random / 1
  Upgraded = Player / 1
```

No additional True-Grit-specific Gameplay primitive is required.

The active Wave 1C-C1 plan on this branch is now:

```text
docs/CardExpansionWave1CC1WarcryHandToDrawPileTopPlan.md
```

That slice develops the reusable exact `Hand -> DrawPileTop` capability and execution-time current-Hand selection needed by a Warcry-style consumer.

True Grit remains deferred as a thin future content-consumer slice.
