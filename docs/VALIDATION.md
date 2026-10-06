# Validation scope — 6 October 2026

Development results and maintainer reports are separate from CI. Private lesson
fixtures, screenshots, logs and machine identifiers are not published.

| Environment | Recorded result |
| --- | --- |
| Native build | Passed on Windows x64 with VS 2022 Build Tools |
| DLL standalone host | Six-slide Plumbing lesson; four audio controls; two videos; seek/resume; captions; six transcript popups; zero page errors |
| Native lifecycle host | Switching, changed-file reload, Unicode/UNC, resize, missing-file recovery and independent instances checked |
| Private catalog | 15/16 playable; one malformed Accordion package displayed a library exception; source hashes unchanged |
| Actual Opus | Plugin loaded; lesson rendering and selection changes observed |
| Second clean computer | Maintainer reports successful installation/use; detailed independent acceptance report not collected |
| Transfer installer | Nine local checks passed in an isolated fake Opus folder; elevation and live-process guard mocked |

The detailed media suite was **not** repeated inside Opus: its diagnostic endpoint
was unavailable. Host/browser tests do not replace actual panel verification.
The second-machine report does not imply every checklist item passed.

External document/worker fetch probes were blocked and assets loaded locally. The
internet adapter was not disabled for an offline test.

## Public tests

The helper tests generate a metadata-only ZIP and need no private lesson or share.
The shell test checks intended player options. These are not playback tests.
Windows CI also downloads pinned dependencies and builds native code; consult the
actual GitHub run for its current result.

Use `transfer/VERIFY.txt` for real Opus media, questions, captions, keyboard focus,
switching, resizing, multiple viewers, Unicode/UNC and an appropriate offline test.
Record PASS, FAIL or NOT TESTED. Wider compatibility/accessibility remains open.
