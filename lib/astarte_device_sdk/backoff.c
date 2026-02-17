// (C) Copyright 2025, SECO Mind Srl
//
// SPDX-License-Identifier: Apache-2.0

#include "backoff.h"

#include <zephyr/random/random.h>

#include <errno.h>

int backoff_init(struct backoff_context *backoff, int64_t mul_coeff, int64_t cutoff_coeff)
{
    if (!backoff) {
        return -EINVAL;
    }
    if (mul_coeff <= 0 || cutoff_coeff <= 0) {
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

int64_t backoff_get_next_delay(struct backoff_context *backoff)
{
    if (!backoff) {
        return -EINVAL;
    }

    const int64_t mul_coeff = backoff->mul_coeff;
    const int64_t max_milliseconds = INT64_MAX;
    const int64_t max_allowed_final_delay = max_milliseconds - mul_coeff;

    // Update last delay value with the new value
    int64_t delay = 0;
    if (backoff->prev_delay == 0) {
        delay = mul_coeff;
    } else if (backoff->prev_delay <= (max_allowed_final_delay / 2)) {
        delay = 2 * backoff->prev_delay;
    } else {
        delay = max_allowed_final_delay;
    }

    // Bound the delay to the maximum
    const int64_t bounded_delay = MIN(delay, backoff->cutoff_coeff);

    // Store the new delay before jitter application
    backoff->prev_delay = bounded_delay;

    // Insert some jitter
    int64_t jitter_minimum = -mul_coeff;
    if (bounded_delay - mul_coeff < 0) {
        jitter_minimum = 0;
    }
    int64_t jitter_maximum = mul_coeff;
    if (bounded_delay > max_milliseconds - mul_coeff) {
        jitter_maximum = max_milliseconds - bounded_delay;
    }

    // Calculate the range for the jitter
    int64_t jitter_range = jitter_maximum - jitter_minimum;
    int64_t jitter = 0;

    if (jitter_range > 0) {
        // sys_rand32_get() returns a 32-bit unsigned integer.
        // The modulo operation confines it to the calculated range.
        uint32_t random_val = sys_rand32_get();
        jitter = (int64_t) (random_val % (jitter_range + 1)) + jitter_minimum;
    }

    return bounded_delay + jitter;
}

void backoff_reset(struct backoff_context *backoff)
{
    if (backoff) {
        backoff->prev_delay = 0;
    }
}
