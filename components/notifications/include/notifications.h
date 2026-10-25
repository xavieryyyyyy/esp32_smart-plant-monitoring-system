#pragma once

#include "esp_err.h"

esp_err_t notifications_send_slack(const char *message);
esp_err_t notifications_send_discord(const char *message);