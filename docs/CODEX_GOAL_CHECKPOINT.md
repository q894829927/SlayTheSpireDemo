# Codex checkpoint — G9-B confirmed FIFO / relic tails / simultaneous discard

Branch `codex/g9-buffered-input-detached-cards`, implementation base/current code
HEAD `fe80565`. Contract commit's parent is `fe80565`; obtain its own hash from Git.
Authority: `docs/QueuedCardPlayAndRelicTimingAmendment.md`, approved by the user.
Every coherent batch requires tests/docs and a local commit; no push.

Completed: previous Native Hand refactor (see dedicated evidence); revised
contract documented. No new C++/assets modified or build/test/PIE claimed.
User EndTurn/mandatory observations lack supplied HEAD/configuration and are
recorded as feedback rather than controlled verification.

Next: atomic front/back insertion and Status front/Relic back, focused order/
failure tests; regenerate projects, build Editor, run affected Automation, record
and commit. Then confirmed FIFO, then TurnEndDiscard. Capacity 32, no queue UI;
invalid entries skip; EndTurn follows confirmed cards; mandatory clears all input.
Future requests stay outside Gameplay queue until prior card/relic/playback ends.

Startup remains false; default activation awaits amended B acceptance. No original
C–F or G9 seal. Content/Config, Legacy, plugins/dependencies/generated files excluded.
