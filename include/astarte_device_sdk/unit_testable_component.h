/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ASTARTE_DEVICE_SDK_UNIT_TESTABLE_COMPONENT_H
#define ASTARTE_DEVICE_SDK_UNIT_TESTABLE_COMPONENT_H

/**
 * @brief Basic unit testable component
 *
 * @defgroup unit_testable_component Basic unit testable component
 * @ingroup astarte_device_sdk
 * @{
 */

#include <stdint.h>

/**
 * @brief Return cycle time
 *
 * @returns the current cycle time
 */
uint32_t unit_testable_component_get_clock_cycle(void);

/**
 * @brief Return parameter if non-zero, or Kconfig-controlled default
 *
 * Function returns the provided value if non-zero, or a Kconfig-controlled
 * default value if the parameter is zero.  This trivial function is
 * provided in order to have a library interface example that is trivial
 * to test.
 *
 * @param return_value_if_nonzero Value to return if non-zero
 * @returns return_value_if_nonzero when the parameter is non-zero
 * @returns CONFIG_CUSTOM_LIB_GET_VALUE_DEFAULT if parameter is zero
 */
int unit_testable_component_get_value(int return_value_if_nonzero);

/**
 * @}
 */

#endif /* ASTARTE_DEVICE_SDK_UNIT_TESTABLE_COMPONENT_H */
