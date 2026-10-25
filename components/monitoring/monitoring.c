#include "monitoring.h"

int monitoring_percent(int raw)
{
    return (raw * 100) / 4095;
}

const char *monitoring_classify(int percent)
{
    if (percent < 30) {
        return "DRY";
    }

    if (percent > 70) {
        return "WET";
    }

    return "NORMAL";
}