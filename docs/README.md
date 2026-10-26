# Documentation guide

**Smart Plant Monitoring System · BCA152 Microcontrollers — B186**<br>
Yestin John Paraguya · Kenneth Restauro · Regine Bermudo

[Project README](../README.md) · [Technical report PDF](technical-report.pdf) · [Rubric checklist](rubric-checklist.md) · [Test results](results/README.md)

## Review path

| Start here | What it contains |
|---|---|
| [Main README](../README.md) | Project scope, exact GPIO map, environment, private configuration, build and demonstration steps |
| [Code guide](code-guide.md) | File responsibilities, task-to-component connections, build declarations, clocks, errors, and serial-log meanings |
| [Rubric checklist](rubric-checklist.md) | The PDF's ten assessment areas and 100-point weighting, with evidence and remaining gaps |
| [Verification plan](test-plan.md) | T1–T8 procedures, acceptance checks, API-access checklist, and planned tests |
| [Results index](results/README.md) | Direct links to actual test records, screenshots, analysis output, and evidence limitations |
| [Technical report PDF](technical-report.pdf) | Formal methods, results, discussion, numbered figures/tables, references, and appendices |
| [Editable report](technical-report.md) | Source text for the PDF |
| [Report rebuild instructions](report-source/README.md) | Documentation-tool prerequisites and the reproducible PDF command |
| [Development record](progress.md) | Historical investigations and meaningful implementation milestones |

## Current evidence

- The real assembly photograph and electrical-net diagram are in the [wiring section](../README.md#wiring).
- [One matching event](results/four-channel-delivery-test.md) is visible in Gmail, Telegram, Discord, and Slack at 2026-10-26 05:23:26 UTC.
- [Six monitoring unit tests](results/monitoring-unit-tests.md) passed in the recorded run.
- [Native layouts, PhST, and email MIME](results/notification-formatting-test.md) have offline coverage; the new rendering has not yet been received on hardware.
- [Cppcheck 2.11](results/static-analysis.md) was checked again during the 26 October 2026 documentation review; the raw output is preserved.
- [Cooldown](results/notification-cooldown-test.md), [stale input](results/stale-sample-test.md), [DHT failure](results/dht11-disconnection-test.md), and [device-triggered reconnection](results/wifi-reconnection-test.md) have bounded experimental records.

These facts do not establish every acceptance condition. The potentiometer/DHT11 and Discord/Slack adaptations remain explicit. Raw conversion tests, new-layout hardware receipt, wider failure/reliability coverage, and the final demonstration remain incomplete. The [rubric checklist](rubric-checklist.md) separates these gaps from available evidence.

## Evidence conventions

A service-accepted request is distinguished from a message visible to the recipient. Injected statuses are distinguished from real provider responses. A saved source/configuration setting is distinguished from an exercised rejection path. Historical records retain their measurement times and original observations; later records may supersede implementation status without changing what happened in the earlier run.

The named authors are the report authors supplied for this project. These records do not infer individual contribution or test-execution roles from account names in screenshots.

## Demonstration status

The builder plans a short demonstration and face-to-face explanation. Video recording was explicitly deferred to a later session. No video or completed live demonstration is claimed. The brief allows a video or live demonstration as directed by the instructor; the actual acceptance of that deliverable is still pending.

No credentials belong in this directory. Public configurations use placeholders, and the local notification header remains ignored.
