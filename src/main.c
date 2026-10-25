#include "app_tasks.h"
#include "connectivity.h"

void app_main(void)
{
    app_tasks_start();
    connectivity_start();
}