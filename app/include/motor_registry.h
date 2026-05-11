#pragma once

#include "motor_driver.h"

#include <stddef.h>

/**
 * @brief Motor registry entry.
 *
 * Associates a human-readable name with a concrete motor driver instance.
 * The registry is populated at build time in motor_registry.c using the
 * names "left" and "right".  The board overlay's motor-left / motor-right
 * aliases determine which hardware backend (ESC, sim, …) is instantiated
 * for each entry; the entry names themselves are independent of the alias
 * names.
 */
struct motor_registry_entry {
    const char *name;
    const struct motor_driver *drv;
};

/** @brief Return the total number of registered motors. */
size_t motor_registry_count(void);

/**
 * @brief Return the motor entry at the given index.
 * @return Pointer to the entry, or NULL if idx is out of range.
 */
const struct motor_registry_entry *motor_registry_get(size_t idx);

/**
 * @brief Look up a motor by name.
 * @return Pointer to the entry, or NULL if not found.
 */
const struct motor_registry_entry *motor_registry_find(const char *name);
