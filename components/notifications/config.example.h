/*
 * Copy this template to config.local.h and put real settings in that private copy.
 * The four sender files read the private copy. Keep this example full of placeholders.
 */
#pragma once

/* Slack incoming-webhook address; replace this only in the private config.local.h copy. */
#define SLACK_WEBHOOK_URL "https://hooks.slack.com/services/REPLACE_ME"
/* Discord incoming-webhook address; the sender adds wait=true itself. */
#define DISCORD_WEBHOOK_URL "https://discord.com/api/webhooks/REPLACE_ME"
/* Telegram bot credential issued by BotFather; keep the real value private. */
#define TELEGRAM_BOT_TOKEN "REPLACE_ME"
/* Destination Telegram conversation ID; this is different from the bot token. */
#define TELEGRAM_CHAT_ID "REPLACE_ME"
/* Account used to authenticate and send the email. */
#define GMAIL_ADDRESS "REPLACE_ME@gmail.com"
/*
 * Sixteen-character Gmail app password with display spaces removed; not the normal login
 * password.
 */
#define GMAIL_APP_PASSWORD "REPLACE_ME"
/* Inbox that should receive the update; it can match the sender account. */
#define GMAIL_RECIPIENT "REPLACE_ME@gmail.com"