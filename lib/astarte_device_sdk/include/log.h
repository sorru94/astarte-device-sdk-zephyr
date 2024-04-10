/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LOG_H
#define LOG_H

/**
 * @file log.h
 * @brief Wrapper for the log module. Helps in keeping the code cleaner with less NOLINT comments.
 */

#include <zephyr/logging/log.h>

/** @brief Wrapper for the LOG_MODULE_REGISTER macro. */
#define ASTARTE_LOG_MODULE_REGISTER(...) LOG_MODULE_REGISTER(__VA_ARGS__) // NOLINT

/** @brief Wrapper for the LOG_DBG macro. */
#define ASTARTE_LOG_DBG(...) LOG_DBG(__VA_ARGS__) // NOLINT

/** @brief Wrapper for the LOG_INF macro. */
#define ASTARTE_LOG_INF(...) LOG_INF(__VA_ARGS__) // NOLINT

/** @brief Wrapper for the LOG_WRN macro. */
#define ASTARTE_LOG_WRN(...) LOG_WRN(__VA_ARGS__) // NOLINT

/** @brief Wrapper for the LOG_ERR macro. */
#define ASTARTE_LOG_ERR(...) LOG_ERR(__VA_ARGS__) // NOLINT

/** @brief Wrapper for the LOG_HEXDUMP_DBG macro. */
#define ASTARTE_LOG_HEXDUMP_DBG(...) LOG_HEXDUMP_DBG(__VA_ARGS__) // NOLINT

#endif // LOG_H
