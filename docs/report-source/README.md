# Rebuilding the technical report

The editable source is `docs/technical-report.md`; the output is `docs/technical-report.pdf`. The script reads the same Markdown, figure assets, and existing evidence images. It does not access the private notification header, flash the ESP32, or contact a notification provider.

## Environment used

- Windows with Segoe UI normal, bold, and italic fonts available in `C:/Windows/Fonts/`.
- Python with ReportLab 4.4.9 and Pillow 12.3.0 available.
- The generated diagrams in `docs/assets/report/` are already included; no Mermaid/browser dependency is needed just to rebuild the PDF.

If a separate Python environment lacks these packages, install them there only after deciding to use that environment. They are documentation tools, not firmware dependencies.

From the project root:

```powershell
python docs/report-source/build_report.py
```

The script reads the author names, course/section, instructor, and report date from the Markdown metadata. The current visual layout has been prepared for the project title and three supplied authors. It uses A4 pages, numbered sections, an automatic contents table, page headers/footers, captioned figures, and wrapping tables.

The Markdown's `<!-- pagebreak -->` comments and `<!-- toc -->` marker control PDF pagination and contents generation. ReportLab may need more than one pass to resolve the contents page; the script handles that with `multiBuild()`.

## Figure sources

- `system-block.png` and `task-architecture.png`: rendered from the README's source-derived Mermaid diagrams using a light print theme.
- `processing-flow.svg` and `.png`: a compact rendering of the implemented processing checks for print readability.
- `wiring.png`: rendered from `docs/assets/readme/wiring.svg`.
- Assembly photograph and recipient/test screenshots: existing project evidence, referenced directly from `docs/images/` and `docs/results/`.

Rebuilding the PDF does not rerun tests or establish any new hardware result. After content or layout changes, render the PDF and inspect page breaks, figure readability, and table alignment before submission. The finished report documents remaining gaps rather than claiming that every original laboratory requirement has been met.
