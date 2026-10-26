# Laboratory rubric and evidence checklist

[Documentation guide](README.md) · [Results index](results/README.md) · [Verification plan](test-plan.md) · [Project README](../README.md)

Source: BCA152 Project No. 1, *Smart Plant Monitoring System Using ESP32, FreeRTOS, and Internet Communication Services*, prepared by Asst. Prof. Paul Rodolf P. Castor, September 2026, pages 2–6. Documentation review: 26 October 2026.

## How to read the status

- **Evidence available:** the linked implementation or result can be reviewed; this is not an awarded score.
- **Partial:** some required behavior has evidence, but the listed gap remains.
- **Adapted:** the implementation differs from the original brief under instructor permission reported by the builder.
- **Pending:** no completed artifact or experiment is claimed.

The point values below are copied from the assignment rubric. No predicted grade or automatic full-credit claim is made. Hardware and service substitutions are retained as reported project decisions; they are not silently presented as the original hardware/services.

## Assessment areas: 100 points

| Original assessment area | Points | Available evidence | Status and remaining issue |
|---|---:|---|---|
| Hardware integration, acquisition, documented calibration | 15 | [GPIO map and actual photo](../README.md#wiring), [threshold explanation](../README.md#policy), [sensor regression](results/sensor-validation-test.md) | Adapted/partial: DHT11 and potentiometer work on the bench; dry/wet soil reference calibration and independent accuracy checks are absent |
| ESP-IDF architecture and FreeRTOS coordination | 20 | [Block/task/data-flow diagrams](../README.md#architecture), task priorities/stacks/ownership, two queues, event groups, task notifications; [continued sampling during dispatch](results/four-channel-delivery-test.md) | Evidence available; queue-pressure, stack-margin, and worst-case timing experiments remain open |
| Wi-Fi, transport security, fault handling | 10 | [BLE onboarding and reconnection](../README.md#wifi), [controlled disconnect](results/wifi-reconnection-test.md), [TLS date configuration](results/tls-date-validation-test.md) | Partial: shared-router outage, long unavailability, negative-certificate tests, and user reprovisioning remain unverified/incomplete |
| Gmail end-to-end notification | 8 | [Matching-event email](results/final-gmail.png), [serial and recipient comparison](results/four-channel-delivery-test.md), [SMTP integration](results/gmail-delivery-test.md) | Recipient delivery evidenced; rejection/retry/rate-limit coverage incomplete |
| WhatsApp end-to-end notification | 8 | The selected substitute is [Discord](results/discord-delivery-test.md), with [matching recipient evidence](results/final-discord.png) | Adapted: instructor permission was reported; this is Discord evidence, not a WhatsApp message |
| Telegram end-to-end notification | 8 | [Matching bot message](results/final-telegram.png), [API integration](results/telegram-delivery-test.md), [shared event](results/four-channel-delivery-test.md) | Recipient delivery evidenced; rate/error/response-edge coverage incomplete |
| X end-to-end posting | 8 | The selected substitute is [Slack](results/slack-delivery-test.md), with [matching recipient evidence](results/final-slack.png) | Adapted: instructor permission was reported; this is a Slack update, not an X post |
| Unit/integration tests and reliability evidence | 10 | [Six Unity cases](results/monitoring-unit-tests.md), [fault records](results/README.md), [compiler review](results/compiler-build-check.md), [Cppcheck 2.11 output](results/static-analysis-output.txt) | Partial: raw conversion, formatting, numeric rejection, sequence gap, overflow, endurance, and custom-component analysis remain open |
| GitHub quality, reproducibility, technical documentation | 8 | [Main README](../README.md), [meaningful history](progress.md), [PDF report](technical-report.pdf), [source](technical-report.md), [rebuild procedure](report-source/README.md), safe configuration template | Artifacts available; platform is unpinned and a separate clean-clone reproduction is not yet evidenced |
| Live demo, explanation, integrity of results | 5 | Actual assembly and four-recipient evidence; [short demonstration procedure](test-plan.md#demonstration) | Pending: video deferred; face-to-face demonstration has not been marked completed |
| **Total rubric weight** | **100** | | **Assessment remains with the instructor** |

## Required deliverables

| Deliverable from the brief | Location | State |
|---|---|---|
| Repository URL and meaningful commits | [GitHub](https://github.com/xavieryyyyyy/esp32_smart-plant-monitoring-system), [milestones](progress.md) | Available |
| Source, PlatformIO/CMake configuration, manifests, lock file | [Repository map](../README.md#repository-map) | Available |
| Sanitized configuration example | [config.example.h](../components/notifications/config.example.h) | Available; private header excluded |
| Board identity, GPIOs, environment, reproduction steps | [Main README](../README.md) | Available |
| Block, hardware wiring, task, and data-flow diagrams | [Architecture](../README.md#architecture), [electrical wiring](../README.md#wiring), report figures 2–5 | Available |
| Actual hardware photograph | [Assembly photo](images/hardware-setup.jpg) | Available |
| Technical report PDF | [Report](technical-report.pdf), [editable source](technical-report.md) | Prepared and reviewed with the builder; documented gaps retained |
| Test plan and API-access checklist | [Verification plan](test-plan.md) | Documented; pending experiments remain planned |
| Logs and actual recipient outputs | [Results index](results/README.md) | Saved excerpts/screenshots; not every historical raw capture is available |
| Static-analysis tool/version/findings/corrections | [Cppcheck record](results/static-analysis.md), [raw output](results/static-analysis-output.txt) | Fresh versioned check available; source scope remains limited |
| Video or instructor-directed live demo | [Planned demonstration](test-plan.md#demonstration) | Pending |

## Remaining work most relevant to grading

1. Add meaningful tests for raw ADC conversion and message formatting. Fix the missing measurement timestamp in the failed-DHT branch before claiming complete payload fields.
2. Demonstrate numeric-validation rejection and additional failure paths without substituting normal-operation logs for those tests.
3. Extend reliability evidence for queue pressure, stack margins, sequence gaps, and longer connectivity interruptions in a controlled network. Do not disrupt the shared boarding-house router.
4. Establish analysis coverage of custom components and perform a clean-clone reproduction using the recorded environment.
5. Complete the short video or instructor-directed face-to-face demonstration.

Real soil calibration remains outside the potentiometer evidence. The adaptation should be explained accurately rather than replaced by invented calibration numbers. Similarly, the four real selected recipients provide genuine delivery evidence, but the instructor determines how the approved substitutions map to the original service-specific marks.
