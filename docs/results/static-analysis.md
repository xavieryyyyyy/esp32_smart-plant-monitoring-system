# Static analysis

[Results index](README.md) · [Verification plan](../test-plan.md) · [Rubric checklist](../rubric-checklist.md)

## Tool and command

- Analyzer: Cppcheck 2.11, verified with the installed executable's `--version` option.
- Runner: PlatformIO Core 6.2.0, `esp32-s3-devkitc-1` environment.
- Command: `pio check --skip-packages`.
- Review date: 26 October 2026. This is separate from the earlier compiler warning check.

## Acceptance check and result

The check should finish successfully, with each remaining finding explained rather than silently suppressed.

The check was repeated after the source comments and formatting changes. It passed in 0.261 seconds with 0 high, 0 medium, and 2 low findings. The [captured output](static-analysis-output.txt) retains the tool version and runner summary. These counts describe the analyzed scope; they are not a claim of a defect-free firmware.

## Findings and disposition

| Finding | Disposition | Reason |
|---|---|---|
| Local `now_us` shadows an outer variable | Fixed in an earlier commit | The unnecessary duplicate declaration was removed; the existing monotonic timestamp is used |
| `app_main`: `unusedFunction` | Retained and documented | ESP-IDF invokes the application entry point |
| `app_tasks_start`: `unusedFunction` | Retained and documented | `src/main.c` calls it; the source-level analysis did not resolve that use |

Line numbers can change when explanatory comments are added. The captured output identifies the lines for that particular run.

## Scope and limitations

PlatformIO reports component `src`. This does not establish coverage of the custom components under `components/`. No claim is made that the reported two findings are the only findings a broader analysis could produce. Static analysis does not replace hardware fault tests, stack measurements, or the six monitoring tests.
