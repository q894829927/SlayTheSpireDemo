# Selection Presentation G8-F — Evidence / Seal

Date: **2026-09-12**

Status: **G8 COMPLETE / VALIDATED / SEALED**

This document is the final status and acceptance authority for Selection Presentation G8.
It records evidence already obtained for the G8 implementation and freezes the delivered
G8 contract. G8-F introduces no new runtime behavior.

## 1. Status authority and supersession

Final stage status:

```text
G8-A — COMPLETE / VALIDATED as sealed G8 infrastructure
G8-B — COMPLETE / VALIDATED
G8-C — HISTORICAL STAGING / VALIDATED AT ITS CHECKPOINT / SUPERSEDED IN PRODUCTION
G8-D — COMPLETE / VALIDATED
G8-E — COMPLETE / VALIDATED
G8-F — COMPLETE / SEALED
G9   — NOT STARTED
```

This seal supersedes stale **status/resume wording only** in older documents that still say
G8 is `DESIGN PROPOSAL`, `NOT IMPLEMENTED`, `DEFERRED`, `NOT AUTHORIZED`, or that G6 is the
next active Selection Presentation stage. In particular, stale banners in the following files
are historical after this seal:

```text
docs/SelectionPresentationG8Design.md
docs/DevelopmentPhases.md
docs/CODEX_GOAL_CHECKPOINT.md
docs/SelectionPresentationG7SealAmendment.md
```

Those documents remain valid for their durable architecture/history where they do not conflict
with this final G8 status. This file does not rewrite historical G8-C staging evidence.

## 2. Frozen production contract

The sealed G8 production behavior is:

```text
Gameplay / reducer chronology remains authoritative and serial.

Damage formal state
→ Controller/reducer owns HP/Block commit

DamageNumber
→ detached private cosmetic
→ finite visual lifetime
→ may outlive the formal Damage record
→ never writes HP/Block back
→ never completes a Controller record
→ never grants or revokes authoritative readiness

combatant Hit / committed enemy Attack cues
→ best-effort Presentation-only cues
→ do not own readiness

pure DamageNumber cosmetic tail
→ HasSkippablePresentationDelay() == false
→ no FastInput catch-up credential
→ ordinary next input does not Skip merely because the number is alive

real Blocking chronology
→ remains skippable/catch-up eligible under the existing exact FastInput contract

prepare-decline / detached runtime disable
→ later Damage uses the existing Blocking fallback

runtime detached-feature disable without authority replacement
→ keeps current PresentationSessionToken
→ cancels current-session detached cosmetics
→ re-evaluates input through exact authority/read guards

HUD / Controller / battle / authority replacement
→ invalidates old PresentationSessionToken
→ old callbacks / deferred input / cosmetics become stale
```

G8-E physically removed the former compatibility-debt runtime state and debt-named production
helper surface. Historical G8-C documentation may still mention that staging mechanism; current
production does not use it.

## 3. Final automated evidence

### 3.1 Build

User-reported UE 5.8 Development Editor build after the final G8-E E2 production cleanup:

```text
PASS
```

The final G8 production cleanup removed compatibility-debt state/helpers and compiled before the
G8-E integration Automation and PIE acceptance were completed.

### 3.2 Required integration Automation

The five required post-E2 G8 integration suites were run locally. The exported `index.json`
summary for each suite reported `failed: 0`:

```text
SlayTheSpireDemo.SelectionPresentation.G8D       PASS / 0 failed
SlayTheSpireDemo.SelectionPresentation.G8B       PASS / 0 failed
SlayTheSpireDemo.Phase6UIA2N.FastInput           PASS / 0 failed
SlayTheSpireDemo.SelectionPresentation.G8A       PASS / 0 failed
SlayTheSpireDemo.CardSelection.Unified           PASS / 0 failed
```

Previously recorded exact suite counts at the validated G8-D checkpoint were:

```text
G8D                    4/4 PASS
G8B                    9/9 PASS
Phase6UIA2N.FastInput  2/2 PASS
G8A                    4/4 PASS
CardSelection.Unified 14/14 PASS
```

Local exported G8-E evidence paths:

```text
Saved/AutomationReports/G8E/SlayTheSpireDemo_SelectionPresentation_G8D/index.json
Saved/AutomationReports/G8E/SlayTheSpireDemo_SelectionPresentation_G8B/index.json
Saved/AutomationReports/G8E/SlayTheSpireDemo_Phase6UIA2N_FastInput/index.json
Saved/AutomationReports/G8E/SlayTheSpireDemo_SelectionPresentation_G8A/index.json
Saved/AutomationReports/G8E/SlayTheSpireDemo_CardSelection_Unified/index.json
```

## 4. Final manual PIE evidence

G8-E manual PIE was **USER ACCEPTED** on 2026-09-12.

The acceptance includes the intended G8 distinction between detached DamageNumber lifetime and
still-Blocking card Presentation. During the final review the user observed that Hand hover does
not raise cards while the played card animation is still active. Code inspection confirmed that
Native Hand fan interaction intentionally pauses while tracked/native Blocking Presentation is
active. This is consistent with the frozen G8 non-goal and is not a G8 failure.

Accepted boundary:

```text
DamageNumber-only cosmetic tail
→ does not own readiness / FastInput Skip

CardPlayed / CardZoneChanged / card-tail Presentation still active
→ remains Blocking in G8
→ Hand hover/input overlap with that card tail is not required
```

The user explicitly accepted G8-E after this boundary was verified and chose to proceed to G8-F
seal.

## 5. Final static evidence

Current-main source search at seal time found `CompatibilityDebt` only in historical G8
documentation/evidence; no production-source match remained in the G8 Controller implementation.
The G8-E checkpoint had already confirmed zero production symbols in:

```text
Source/SlayTheSpireDemo/Presentation/BattlePresentationController.h
Source/SlayTheSpireDemo/Presentation/BattlePresentationController.cpp
Source/SlayTheSpireDemo/Presentation/BattlePresentationControllerG8C.cpp
```

Current `BattlePresentationControllerG8B.cpp` keeps the sealed FastInput gate:

```cpp
return bWaitingForCompletion
    || bHasActiveEnvelope
    || PlaybackQueue.Num() > 0;
```

Detached DamageNumber lifetime is not included in this predicate.

## 6. Evidence-head / later unrelated repository work

The last G8 production-code cleanup commit is:

```text
499b2996fb1ded6c14e4a97c76a205abb71c5188
refactor(g8-e): remove compatibility debt call sites
```

After that commit, the repository received documentation updates and unrelated Interior Portal
work. A seal-time compare from `499b2996...` to current `main` showed no later modification to
G8 Presentation/UI/Test production source; the later C++ changes are under the separate
`Interior` feature area.

Under `docs/ValidationExecutionPolicy.md`, previously passing G8 evidence remains valid because the
G8 proving source/contracts were not changed by that unrelated work. Therefore G8-F does not rerun
passing G8 gates solely for more confidence.

Important limitation:

```text
G8 COMPLETE / VALIDATED / SEALED
!=
current whole-repository build validated
!=
Interior Portal work validated
```

This seal makes no claim that unrelated post-G8 work compiles, passes Automation, or passes PIE.

## 7. G8 non-goal frozen for G9

G8 does **not** detach card-tail Presentation and does not implement buffered next-card intent.
The following experience remains outside the G8 seal:

```text
A CardPlayed/CardZoneChanged animation continues
+ B Hand card already hovers/raises normally
+ B click is buffered without skipping A
+ A/B card visual lifetimes overlap across the next request
```

That capability belongs to a separate future initiative:

```text
G9 — Buffered Card Input + Detached Card Presentation
```

G9 must define exact buffered intent identity, stale fencing, card visual ownership, Hand/PlayArea
reconciliation, safe Gameplay boundary, replacement cleanup and GC behavior before activation.
G8-F does not authorize or start G9.

## 8. Seal decision

Final acceptance matrix:

```text
G8-E structural cleanup build                              PASS
G8-D/G8-B/FastInput/G8-A/CardSelection integration        PASS / 0 failed
G8-E manual PIE                                           USER ACCEPTED
CompatibilityDebt production runtime/helper removal       PASS
Detached DamageNumber one-way cosmetic ownership          PASS
FastInput pure-cosmetic-tail exclusion                     PASS
Blocking Damage fallback / runtime-disable fallback        PASS
Session replacement / exact authority rules retained       PASS
G9 card-tail overlap non-goal retained                     PASS
```

Therefore:

```text
SELECTION PRESENTATION G8 — COMPLETE / VALIDATED / SEALED
```

Normal forward work must not reopen G8 for speculative cleanup. Any broader card Presentation
overlap or buffered input work requires an explicitly authorized G9 design/implementation scope.
