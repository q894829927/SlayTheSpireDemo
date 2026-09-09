# Selection Presentation G2 Execution Record

Date: **2026-09-09**

Status:

```text
COMPLETE / VALIDATED / SEALED
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

## Validation evidence — 2026-09-09

User-confirmed local validation on the G2 implementation head:

```text
[x] SlayTheSpireDemoEditor Win64 Development Build PASS
[x] SlayTheSpireDemo.SelectionPresentation.G2 focused Automation PASS (6/6)
[x] Automation process exit code 0
```

The supplied Automation log reports exactly six tests discovered for the G2 prefix
and records `Result={Success}` for all six:

```text
ContiguousGroup
FutureMemberInterference
ManifestAndIncomplete
NonContiguousUnrelated
SequenceAndUnknown
SingletonAndShape
```

No manual PIE is required for G2 because this stage does not activate visible Group
playback or change production Selection visual ownership.

G1 passing evidence remains sticky; G2 did not invalidate the sealed G1 contract.

## Seal

G2 is **COMPLETE / VALIDATED / SEALED**. Do not rerun its passing gates unless a
later edit invalidates this evidence or a concrete regression directly implicates
the sealed G2 semantic-preflight contract.

Next active slice:

```text
G3 — Base Widget Record-or-Group playback hardening
     + unified playback-unit token ownership
     + exact completion
     + scoped recovery
```

G3 is the next architectural-risk boundary. It must preserve chronological reducer
ownership and later queued Envelopes during active-envelope recovery. G3 still must
not switch production SelectionArea ownership or enable G6 multi-member parallel
playback.