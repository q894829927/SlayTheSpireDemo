# Selection Presentation G2 Execution Record

Date: **2026-09-09**

Status:

```text
IMPLEMENTED IN SOURCE /
BUILD NOT RUN / AUTOMATION NOT RUN / NO PASS CLAIM
```

## Scope

G2 only: Controller semantic PresentationGroup discovery, chronological reducer
dry-run and future-member interference validation over sealed immutable Envelope
facts.

Explicitly not included: Group-visible playback, Widget Group APIs, playback-unit
token changes, SelectionArea production ownership, generic transition migration or
G8 early input.

## Implemented contract

`UBattlePresentationController` owns the semantic preflight. The candidate contains
only:

```text
Group tag
canonical selected RuntimeIds
member Record indices
member RuntimeIds in chronological Record order
```

The preflight reads only the supplied frozen baseline and sealed Envelope. It does
not inspect concrete Widgets, SelectionArea ownership, geometry, destination anchors
or mutable Gameplay state.

A multi-member candidate is accepted only when:

```text
one exact declaration exists
ExpectedMemberCount > 1
canonical manifest is unique and complete
tagged members use exact matching metadata
all members are eligible Hand -> Discard/Draw/Exhaust CardZoneChanged records
member RuntimeIds exactly equal the canonical manifest
leader is the first chronological member
BattleId / ResolutionId / PresentationSequence are valid in the lookahead range
chronological reducer dry-run succeeds through every interleaved record
no outside record modifies a still-future exact member RuntimeId
```

Current exact-card interference classification includes `CardZoneChanged` and
`CardPlayed`. Damage, Block, Energy, DeckShuffled and StatusChanged do not reject a
group merely by interleaving. Unknown/terminal shapes inside the lookahead range are
conservatively rejected. A record tagged for another group is not exempt from exact
future-member interference.

The dry-run reuses the production Controller reducer by temporarily swapping only
its internal working snapshot / active Envelope and restoring them before return.
No ViewModel publication, Widget call, visual ownership mutation, reducer advance or
Gameplay mutation occurs.

Malformed, incomplete or interfered optional grouping returns `false` and therefore
remains ordinary serial history. G2 does not collapse an Envelope or request a
Gameplay ResolutionFault because optional Group semantics are unavailable.

## Focused Automation source

```text
SlayTheSpireDemo.SelectionPresentation.G2.ContiguousGroup
SlayTheSpireDemo.SelectionPresentation.G2.NonContiguousUnrelated
SlayTheSpireDemo.SelectionPresentation.G2.FutureMemberInterference
SlayTheSpireDemo.SelectionPresentation.G2.ManifestAndIncomplete
SlayTheSpireDemo.SelectionPresentation.G2.SingletonAndShape
SlayTheSpireDemo.SelectionPresentation.G2.SequenceAndUnknown
```

The interference test intentionally proves:

```text
B leaves Hand
-> B returns to Hand
-> chronological reducer dry-run succeeds
-> B is still a future Group member
-> semantic Group eligibility rejects
```

so reducer success alone is not treated as proof that early visible consumption is
safe.

## Validation state

No Build or Automation result is claimed yet. G2 has no visible production behavior
change, so no new manual PIE gate is required for this stage unless validation finds
a concrete visual regression that implicates the changed source.

Before sealing G2, run:

```text
SlayTheSpireDemoEditor Win64 Development Build
SlayTheSpireDemo.SelectionPresentation.G2 focused Automation
```

G1 passing evidence remains sticky unless a G2 edit directly invalidates it.
