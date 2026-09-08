# Card Expansion — Wave 1C-C1 Configurable Hand→DrawPileTop Amendment

Date: **2026-09-08**

Status:

```text
USER-AUTHORIZED AMENDMENT / IMPLEMENTATION ACTIVE
```

This amendment supersedes only the fixed-exact-one count statements in:

```text
docs/CardExpansionWave1CC1WarcryHandToDrawPileTopPlan.md
```

The user explicitly requires the reusable Effect itself to support a Blueprint-authored number of selected Hand cards.

## Effect contract

The reusable Effect remains:

```text
USelectHandCardToDrawPileTopEffect
```

and exposes explicit Base / Upgraded values:

```text
BaseSelectionCount     >= 0
UpgradedSelectionCount >= 0
```

No sentinel/fallback semantics are used. If upgrade does not change the value, author the same value in both fields.

Runtime exact-N rule:

```text
RequiredCount = Min(EffectiveSelectionCount, CurrentValidHandCandidateCount)
```

with:

```text
EffectiveSelectionCount == 0
→ legal no-op
→ no pending Selection
→ no zone mutation
→ later authored Effects / normal card cleanup continue

no current Hand candidates
→ legal no-op

EffectiveSelectionCount > current Hand candidates
→ clamp to all current valid Hand candidates
```

Selection remains:

```text
Player
exact-N
CancelPolicy = Forbidden
candidates = valid CURRENT Hand CardInstances at DeferredHandSelectionAction Execute time
```

No Random mode, optional range, arbitrary insertion index or generic zone-selection API is authorized by this amendment.

## Multi-card draw-pile-top ordering

The selected set is canonicalized through the existing Gameplay selection facade to current Hand candidate order. The continuation preserves that canonical result order and builds one exact move Action per selected card.

For canonical selected cards:

```text
A, B, C
```

execution is:

```text
A -> DrawPile.Add(A)
B -> DrawPile.Add(B)
C -> DrawPile.Add(C)
```

therefore final top-to-bottom order begins:

```text
C
B
A
```

This is deterministic and does not make UI click order an implicit Gameplay ordering control.

## Description contract

The Effect has a semantic, non-None preview argument default:

```text
DescriptionArgumentName = DrawPileTopCount
```

Automatic card description is fixed Chinese text:

```text
将手牌中的 {Count} 张牌放到你的抽牌堆顶部。
```

`Count` resolves independently per Effect instance:

```text
Base card     -> BaseSelectionCount
Upgraded card -> UpgradedSelectionCount
```

Example:

```text
BaseSelectionCount = 1
UpgradedSelectionCount = 3

Base     -> 将手牌中的 1 张牌放到你的抽牌堆顶部。
Upgraded -> 将手牌中的 3 张牌放到你的抽牌堆顶部。
```

## Preserved C1 architecture

The original ordering requirement remains unchanged:

```text
prior Draw / Shuffle work completes
→ UDeferredHandSelectionAction executes
→ reads CURRENT Hand
→ ordinary USelectionRequestAction
→ authored continuation
→ UMoveHandCardToDrawPileTopAction x N
→ exact Hand -> DrawPile commits
→ later authored effects / played-card cleanup continue normally
```

`UDeckRuntime::DrawPile` remains bottom -> top with array end as top. UI/Presentation does not gain hidden DrawPile card identities.

## Production asset scope

This amendment does not authorize creation or modification of a production Warcry `.uasset` or any existing user-owned binary asset. C1 capability validation uses C++/Automation and a later explicit PIE/content-authoring step.
