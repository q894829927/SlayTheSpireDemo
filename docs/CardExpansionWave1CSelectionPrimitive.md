# Card Expansion — Wave 1C Selection Primitive

Date: **2026-09-07**

Status: **WAVE 1C-A / 1C-B COMPLETE / VALIDATED / SEALED**

## Purpose

Wave 1C introduces the generic Gameplay-owned selection primitive required by cards whose resolution depends on player choice, and closes its first playable consumer path with Burning Pact-shaped composition.

Wave 1B remains the sealed targeted-exhaust primitive. Wave 1C composes selection with that existing capability rather than merging selection semantics into the exhaust action itself.

---

## Ownership Contract

Selection state belongs to Gameplay authority.

```text
Gameplay Action / Resolver
        |
        | Create SelectionRequest
        v
SelectionResolver
        |
        | UI-safe read/request facade
        v
Native HUD / ViewModel
        |
        | RuntimeId-only player input
        v
Gameplay selection request facade
        |
        | Build validated SelectionResult
        v
SelectionResolver
        |
        | Resume awaiting Action
        v
BattleActionQueue continuation
```

Presentation must not:

- store authoritative pending-selection objects;
- decide valid candidates;
- mutate cards/zones;
- directly execute a Gameplay continuation;
- enqueue authoritative BattleActions.

The Native HUD/ViewModel is only an input submission surface. It exposes stable card RuntimeIds and cancelability, never authoritative candidate `UObject*` pointers.

---

## Selection Request Contract

A `FSelectionRequest` contains:

- selection source;
- candidate runtime objects;
- minimum / maximum selection count;
- request-level cancellation policy.

The typed stateless `UAuthoredContinuation` is passed alongside the request into `USelectionResolver::BeginSelection`.

Cancellation policy:

```text
ESelectionCancelPolicy::Allowed
  -> generic legal cancel path

ESelectionCancelPolicy::Forbidden
  -> mandatory choice; cancel is rejected and the awaiting Action remains pending
```

---

## Selection Result Contract

A `FSelectionResult` is accepted only through Gameplay validation.

Validation includes:

- selected object exists;
- selected object belongs to the authoritative candidate set;
- selection count satisfies the request bounds;
- runtime state is still valid.

Invalid selection must not mutate Gameplay state.

For the Native single-card Hand bridge, Presentation submits only a RuntimeId. `BattleSelectionRequest` resolves that RuntimeId back to the exact pending Gameplay candidate and constructs the result inside Gameplay.

---

## Continuation Resume Contract

Continuation execution belongs to the Gameplay queue flow.

```text
SelectionResult
      |
      v
Gameplay Resolver
      |
      v
Resume awaiting SelectionRequestAction
      |
      v
Authored Continuation builds dependent Actions
      |
      v
BattleActionQueue
```

`USelectionRequestAction` propagates the active committed-Presentation writer to Actions created by the continuation. This is a common boundary rule, not a Burning Pact-specific special case.

Therefore post-selection mutations stay inside the same sealed Presentation resolution:

```text
selection resolves
→ continuation creates dependent Action(s)
→ dependent Action(s) inherit current Presentation writer
→ committed records remain ordered with later authored Effects
```

---

## Cancel / Invalid Semantics

### Allowed cancel

When `CancelPolicy = Allowed`:

- cancellation is a legal resolution path;
- pending selection clears;
- no Gameplay mutation occurs;
- no ResolutionFault is emitted;
- waiting `SelectionRequestAction` finishes;
- queue resumes.

### Forbidden cancel

When `CancelPolicy = Forbidden`:

- `SubmitCancel()` returns false;
- a submitted `Cancelled` result is rejected;
- pending request remains active;
- awaiting `SelectionRequestAction` does not finish;
- later queued Effects cannot continue;
- no Gameplay mutation and no ResolutionFault occur.

This prevents mandatory costs such as Burning Pact's exhaust step from being skipped while Draw still resolves.

### Invalid result

Examples include:

- missing RuntimeId;
- object not in the candidate list;
- invalid selection count.

Rules:

- no Gameplay mutation;
- deterministic rejection/failure behavior;
- production Native bridge rejects invalid/non-candidate RuntimeIds before constructing a result.

---

## Native HUD input bridge

Wave 1C closes the playable input gap without reopening the sealed A2/A3 ownership model.

```text
UBattleCardWidget click
→ UBattleHUDWidget::SelectCard(RuntimeId)
→ if no pending Gameplay selection:
     unchanged normal card-play / fast-presentation path
→ if a supported pending single-card Gameplay selection exists:
     UBattleHUDViewModel::SubmitPendingCardSelectionByRuntimeId
     → BattleSelectionRequest::SubmitPendingCardSelection
     → USelectionResolver::SubmitResult
```

Boundaries:

```text
- ordinary Resolving input remains locked
- only an actual pending single-card request activates the alternate click route
- UI never receives candidate UObject pointers
- non-candidate RuntimeIds are rejected
- committed Presentation display remains Controller-owned
```

---

## Wave 1C Scope Split

### Wave 1C-A — Selection Primitive

Sealed capabilities:

- `FSelectionRequest` / `FSelectionResult`;
- `USelectionResolver`;
- request-level cancel policy;
- queue hold/resume continuation contract;
- common continuation Presentation-writer propagation;
- generic allowed-cancel behavior;
- focused Automation coverage.

### Wave 1C-B — First Consumer / playable bridge

Sealed capabilities:

- reusable `USelectExhaustHandCardEffect`;
- mandatory select-one-Hand-card semantics;
- `UExhaustSelectedContinuation`;
- Native HUD RuntimeId-only pending-card route;
- full Burning Pact-shaped composition (`SelectExhaust -> Draw`);
- committed `Hand -> ExhaustPile` Presentation reduction;
- Native Hand-exhaust in-place fade presentation.

### Future expansion

Future consumers may reuse the primitive, but remain separately authorized:

- True Grit upgraded behavior;
- Exhume-style selection;
- future Random selection;
- future generic multi-select UI where a real consumer requires it.

---

## Burning Pact Design Details (sealed)

### Delivery shape

Wave 1C-B delivers a reusable orthogonal CardEffect, not a hard-coded card.

```text
Burning Pact = [
  USelectExhaustHandCardEffect,
  UDrawCardEffect(DrawCount=2, UpgradedDrawCount=3)
]
```

`PlayCardAction` authors Effects in array order. `USelectionRequestAction` holds the queue. Resolving the selection inserts the exhaust continuation before the already-authored Draw Action, so Draw cannot run before the mandatory selection and exhaust complete.

### Select-and-exhaust effect

```text
USelectExhaustHandCardEffect
  candidates = current Hand cards minus the played card

has candidates
  -> mandatory select exactly one
  -> CancelPolicy = Forbidden
  -> exhaust exact selected card

no candidates
  -> skip selection/exhaust without fault
```

Candidate timing is deterministic and computed in `BuildActions` from the current Hand while excluding `Context.Card`.

### Orthogonality

```text
USelectionRequestAction       -> does not know exhaust details
UExhaustCardAction            -> does not know who selected it or that Draw follows
UDrawCardsAction              -> does not know what preceded it
USelectExhaustHandCardEffect  -> composition point connecting selection -> exhaust
```

### Numerical transient Automation shape

```text
Burning Pact:  Cost 1; exhaust 1 Hand card; draw 2; destination Discard
Burning Pact+: Cost 1; exhaust 1 Hand card; draw 3; destination Discard
```

### Committed Presentation order

The sealed record order for the base test shape is:

```text
CardPlayed
→ Hand → ExhaustPile
→ DrawPile → Hand
→ DrawPile → Hand
→ PlayArea → DiscardPile
```

`UBattlePresentationController` reduces the exact Hand-exhaust transition before later Draw records. This preserves the historical Hand count/index contract used by Native Draw playback.

### Native Hand-exhaust visual

The selected Hand card does not fly to the Exhaust counter.

```text
formal historical Hand card
→ stays at its current Hand position
→ scale remains 1
→ opacity fades 1 → 0 over the Native presentation duration
→ on record completion Controller removes the card formally
→ subsequent Draw animation begins
```

Cancel/destruct cleanup restores the historical card transform and opacity when an active exhaust presentation is aborted.

---

## Final Validation / Seal

Final focused Automation inventory:

```text
5 Selection primitive cases
6 SelectExhaust / Burning Pact Gameplay/input cases
2 committed-Presentation regression cases
= 13 total Wave 1C cases
```

The two Presentation regression cases are:

```text
SlayTheSpireDemo.CardExpansion.Wave1C.BurningPact.PresentationRecordOrder
SlayTheSpireDemo.CardExpansion.Wave1C.Presentation.HandExhaustReducer
```

User-confirmed validation on **2026-09-07**:

```text
[x] SlayTheSpireDemoEditor Win64 Development Build PASS
[x] SlayTheSpireDemo.CardExpansion.Wave1C focused Automation PASS (13/13)
[x] Native HUD PIE base Burning Pact PASS
[x] Native HUD PIE Burning Pact+ PASS
[x] mandatory selection cannot be cancelled/skipped into Draw PASS
[x] selected exhausted card fades in place PASS
[x] Draw 2 / Draw 3 Native presentation PASS
[x] exhausted card does not reappear in Hand PASS
[x] HUD returns to normal interaction after resolution PASS
[x] final user validation confirmation received
```

The earlier 5/5 and 7/7 results are historical pre-closure evidence only. The current seal is based on the validation above.

Wave 1C-A and Wave 1C-B are **COMPLETE / VALIDATED / SEALED**.

---

## Binary test content boundary

The branch contains owner-authored:

```text
Content/SlayTheSpireDemo/Data/Cards/Ironclad/Skills/DA_Card_BurningPact.uasset
Content/SlayTheSpireDemo/Maps/L_BattleTest.umap
```

Those files were used as owner-controlled/ad-hoc content and were not modified by the C++ closure work. The reusable behavior seal is established by the C++/Automation/PIE contract above, not by treating those binary changes as a generic architecture authority.

---

## Not implemented / not authorized by Wave 1C

```text
- Random selection mode
- selection-into-other-moves (Exhume / True Grit expansion)
- bulk exhaust (Fiend Fire / Second Wind)
- reactive exhaust powers (Feel No Pain / Dark Embrace)
- generic multi-select Native UI before a real consumer requires it
- generic AnyZone universal movement API
```