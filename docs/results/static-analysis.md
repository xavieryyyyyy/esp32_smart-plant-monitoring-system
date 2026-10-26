# Static analysis

## Command
    pio check --skip-packages

## Results
Cppcheck passed:
- High: 0
- Medium: 0
- Low: 2

Removed a duplicate now_us declaration that shadowed an outer variable.

## Remaining findings
- app_main: unusedFunction; invoked by ESP-IDF.
- app_tasks_start: unusedFunction; called from src/main.c.

These are treated as analysis limitations, not reasons to remove
the functions.

## Scope
The reported analysis covered src.
This does not establish coverage of custom components.