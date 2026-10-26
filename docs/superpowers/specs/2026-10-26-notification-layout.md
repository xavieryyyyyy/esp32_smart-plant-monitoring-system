# Approved notification layout and Philippine time

The builder approved the final four-service mockup on 26 October 2026. This is a presentation change to the existing ESP-IDF application, not a new notification policy.

- All four services receive the same captured Plant-01 event. Show state, simulated moisture, temperature/humidity or explicit unavailability, measured time, reason, and the potentiometer qualification.
- Show captured measurement time in English, twelve-hour Philippine Standard Time, with seconds and `PhST` / `UTC+08:00`. Keep stored Unix time and monotonic cooldown/freshness timers unchanged. Do not replace measurement time with send time or change the process-wide timezone.
- Put condition/readings before measured time. Use bold labels and regular values. Telegram uses formatted text; Slack uses native sections/fields plus accessible fallback text; Discord uses a native colored embed with mentions disabled.
- Gmail uses a forest header, DRY/NORMAL/WET status treatment, prominent simulated percentage, supporting readings, measured time, reason, and readable qualification. Use HTML email with a complete plain-text alternative. Font preference is Inter with system fallbacks; actual clients control rendering and remote fonts are not required.
- Keep existing request timeouts, TLS checks, sender order, retry policy, queue behavior, task priorities/stacks, and credentials unchanged.
- No real service request, ESP32 upload, commit, or push is part of automated verification. Hardware receipt of the new layouts remains pending until the builder tests the uploaded firmware.

Reference: `docs/assets/notifications/design-preview.png` is an illustrative approved mockup, not delivery evidence.
