/*
 * main.c includes this header so it can call app_tasks_start().
 * The actual startup code is in app_tasks.c.
 */
#pragma once

/* Create both data queues before starting the three tasks that will use them. */
void app_tasks_start(void);