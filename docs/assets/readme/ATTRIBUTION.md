# README artwork and attribution

The README uses local assets so its logos, icons, and header do not depend on a live image-badge service.

| Asset | Source | Notes |
|---|---|---|
| `header.png` | Rendered from `header-source.html` with Inter and local brand artwork | Approved glass/gradient design; static image, not a fake status dashboard |
| Inter | [Inter project](https://rsms.me/inter/) / [source and OFL license](https://github.com/rsms/inter) | SIL Open Font License 1.1; font rendered into the header; font files are not bundled |
| `icons/*.svg` | [Lucide](https://lucide.dev/) | Actual Lucide paths on a custom dark badge; license copied to `LUCIDE-LICENSE.txt` |
| `gmail.svg` | [Wikimedia: Gmail icon (2020)](https://commons.wikimedia.org/wiki/File:Gmail_icon_(2020).svg) | Google/Gmail service mark, original colors |
| `telegram.svg` | [Wikimedia: Telegram logo](https://commons.wikimedia.org/wiki/File:Telegram_logo.svg) | Telegram service mark, original colors |
| `slack.svg` | [Wikimedia: Slack icon 2019](https://commons.wikimedia.org/wiki/File:Slack_icon_2019.svg) | Slack service mark, original colors |
| `discord.svg`, `espressif.svg`, `c.svg` | [Simple Icons](https://simpleicons.org/) | Brand artwork retrieved through its SVG endpoint; [project license](https://github.com/simple-icons/simple-icons/blob/develop/LICENSE.md) |
| `wiring.svg` | Original electrical-net diagram for this project | Labels represent electrical nets, not physical board/header orientation |
| Recipient screenshots | Existing `docs/results/` files | Captured during this project's development; not generated design evidence |

Service marks identify integrations and remain the property of their respective owners. Their use does not imply endorsement. This notice does not grant rights to trademarks or apply a new license to the firmware.

## Header source

`header-source.html` contains the header styling, locally referenced logos, and inline Lucide artwork. It uses a Google Fonts import for editable Inter text. The shipped PNG embeds the rendered lettering and needs no font download when viewed on GitHub. GitHub Markdown cannot apply this CSS to body text; normal documentation stays selectable Markdown.

## Design scope

The implementation preserves the approved dark glass header, restrained gradient, real logos, compact technology badges, and consistent section icons. The body uses native headings, tables, disclosures, and Mermaid diagrams. Badges name configuration values; they do not imply hosted CI, full test coverage, or complete laboratory acceptance.

## Aligned evidence previews

`evidence-gmail.png`, `evidence-telegram.png`, `evidence-discord.png`, and `evidence-slack.png` place the existing recipient screenshots inside equal-size frames. The full screenshot is fitted without cropping or stretching. Original files in `docs/results/` remain unchanged and are linked from each preview. Logos use the local source assets credited above.
