# Clean compiler build check

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