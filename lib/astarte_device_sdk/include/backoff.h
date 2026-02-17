// (C) Copyright 2025, SECO Mind Srl
//
// SPDX-License-Identifier: Apache-2.0

#ifndef BACKOFF_H
#define BACKOFF_H

#include <stdint.h>

/** @brief Struct for generating exponential backoff delays with jitter. */
struct backoff_context
{
    /** @brief Multiplier coefficient as defined in the configuration. */
    uint32_t mul_coeff;
    /** @brief Cutoff coefficient as defined in the configuration. */
    uint32_t cutoff_coeff;
    /** @brief Previous delay used to calculate the next one. */
    uint32_t prev_delay;
};

/**
 * @brief Initializes an backoff_context struct.
 *
 * @details The exponential backoff will compute an exponential delay using
 * 2 as the base for the power operation and @p mul_coeff as the multiplier coefficient.
 * The values returned by calls to #backoff_get_next_delay will follow the formula:
 * min( @p mul_coeff * 2 ^ ( number of calls ) , @p cutoff_coeff ) + random jitter.
 * The random jitter will be in the range [ - @p mul_coeff , + @p mul_coeff ].
 *
 * @param backoff Pointer to the backoff structure to initialize.
 * @param mul_coeff Multiplier coefficient used in the exponential delay calculation (in ms).
 * @param cutoff_coeff The cut-off coefficient, an upper bound for the exponential curve (in ms).
 * @return 0 on success, or -EINVAL if the coefficients are invalid.
 */
int backoff_init(struct backoff_context *backoff, uint32_t mul_coeff, uint32_t cutoff_coeff);

/**
 * @brief Calculates and returns the next backoff delay.
 *
 * @param backoff Pointer to the initialized backoff structure.
 * @return The calculated delay duration in milliseconds, or 0 if the pointer is null.
 */
uint32_t backoff_get_next_delay(struct backoff_context *backoff);

/**
 * @brief Resets the backoff generator.
 *
 * @param backoff Pointer to the initialized backoff structure.
 */
void backoff_reset(struct backoff_context *backoff);

#endif /* BACKOFF_H */
