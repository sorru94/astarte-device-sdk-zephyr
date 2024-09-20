/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef WIFI_H
#define WIFI_H

/**
 * @brief Initialize the wifi driver
 */
void wifi_init(void);

/**
 * @brief This poll function makes sure the wifi connection is still up
 */
void wifi_poll(void);

#endif /* WIFI_H */
