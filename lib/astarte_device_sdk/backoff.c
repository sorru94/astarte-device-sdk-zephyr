// (C) Copyright 2026, SECO Mind Srl
//
// SPDX-License-Identifier: Apache-2.0

#include "backoff.h"

#include <zephyr/random/random.h>
#include <zephyr/sys/util.h>

#include <errno.h>

int backoff_init(struct backoff_context *backoff, uint32_t mul_coeff, uint32_t cutoff_coeff)
{
    if (!backoff) {
        return -EINVAL;
    }
    if (mul_coeff == 0 || cutoff_coeff == 0) {
        return -EINVAL;
    }
    if (cutoff_coeff < mul_coeff) {
        return -EINVAL;
    }

    backoff->mul_coeff = mul_coeff;
    backoff->cutoff_coeff = cutoff_coeff;
    backoff->prev_delay = 0;

    return 0;
}

uint32_t backoff_get_next_delay(struct backoff_context *backoff)
{
    if (!backoff) {
        return 0;
    }

    const uint32_t mul_coeff = backoff->mul_coeff;
    const uint32_t max_milliseconds = UINT32_MAX;
    const uint32_t max_allowed_final_delay = max_milliseconds - mul_coeff;

    // Update last delay value with the new value
    uint32_t delay = 0;
    if (backoff->prev_delay == 0) {
        delay = mul_coeff;
    } else if (backoff->prev_delay <= (max_allowed_final_delay / 2)) {
        delay = 2 * backoff->prev_delay;
    } else {
        delay = max_allowed_final_delay;
    }

    // Bound the delay to the maximum
    const uint32_t bounded_delay = MIN(delay, backoff->cutoff_coeff);

    // Store the new delay before jitter application
    backoff->prev_delay = bounded_delay;

    // Insert some jitter
    uint32_t lower_bound = 0;
    if (bounded_delay > mul_coeff) {
        lower_bound = bounded_delay - mul_coeff;
    }
    uint32_t upper_bound = UINT32_MAX;
    if (bounded_delay < max_allowed_final_delay) {
        upper_bound = bounded_delay + mul_coeff;
    }

    // Calculate the range for the jitter
    uint32_t jitter_range = upper_bound - lower_bound;
    uint32_t final_delay = bounded_delay;
    if (jitter_range == UINT32_MAX) {
        // Prevent overflow: jitter_range + 1 would be 0.
        final_delay = sys_rand32_get();
    } else if (jitter_range > 0) {
        // sys_rand32_get() returns a 32-bit unsigned integer.
        // The modulo operation confines it to the calculated range.
        uint32_t random_val = sys_rand32_get();
        final_delay = lower_bound + (random_val % (jitter_range + 1));
    }

    return final_delay;
}

void backoff_reset(struct backoff_context *backoff)
{
    if (backoff) {
        backoff->prev_delay = 0;
    }
}
