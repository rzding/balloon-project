/**
 * @file app.h
 * @brief Application entry point for the flight computer superloop.
 */

#pragma once

#include <stdbool.h>

/**
 * @brief One-time application initialization after CubeMX peripheral init.
 * @return false on fatal init failure (reserved for later phases).
 */
bool app_init(void);

/**
 * @brief One superloop iteration; must return promptly.
 */
void app_run(void);
