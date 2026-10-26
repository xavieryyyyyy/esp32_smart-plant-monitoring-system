# Clean compiler build check

[Results index](README.md) · [Verification plan](../test-plan.md) · [Rubric checklist](../rubric-checklist.md)

## Test summary

| Field | Record |
|---|---|
| Coverage | QA / compiler diagnostics |
| Method | Historical clean build and warning search |
| Acceptance check | The production build succeeds and configured compiler warnings are reviewed. |
| Outcome and scope | Observed pass for the recorded build; separate from static analysis. |

## Commands
    pio run -t clean
    pio run 2>&1 | Tee-Object -FilePath build-check.log
    Select-String -Path build-check.log -Pattern 'warning:|warning!'

## Results
- Clean build succeeded.
- Warning search returned no matches.

## Limits
This checks the compiler's configured diagnostics.
It is not a separate static-analysis run or proof of no defects.

## Comment and documentation regression check — 26 October 2026

After adding explanatory comments and formatting the source:

- `pio run` completed successfully in 128.70 seconds, without uploading firmware.
- The image fit the 4 MB application partition: 1,341,233 bytes; static RAM reported 42,080 bytes.
- A comparison of executable C tokens before/after annotation found no changes. The Python build-hook AST and non-comment CMake/manifest declarations were also unchanged across the 25 annotated files.
- Cppcheck 2.11 was rerun after annotation: 0 high, 0 medium, 2 low; see the [analysis record](static-analysis.md) and [captured output](static-analysis-output.txt).

This build validates compilation/linking after the comment pass. Unity tests and physical notification delivery were not rerun in this pass; their earlier records remain the evidence for those checks. The historical warning-search result above describes its original clean-build run.
