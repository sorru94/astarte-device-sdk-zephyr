/*
 * (C) Copyright 2025, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>

#include "backoff.h"

#include <errno.h>

#define MS_IN_MINUTE (60U * 1000U)
#define MS_IN_HOUR (60U * MS_IN_MINUTE)
#define MS_IN_DAY (24U * MS_IN_HOUR)

ZTEST_SUITE(astarte_device_sdk_backoff, NULL, NULL, NULL, NULL, NULL);

ZTEST(astarte_device_sdk_backoff, test_incorrect_inputs)
{
    struct backoff_context backoff;

    // cutoff_coeff < mul_coeff
    zassert_equal(-EINVAL, backoff_init(&backoff, 2 * MS_IN_MINUTE, 1 * MS_IN_MINUTE),
        "backoff_init should fail when cutoff_coeff < mul_coeff");

    // mul_coeff == 0
    zassert_equal(-EINVAL, backoff_init(&backoff, 0, 1 * MS_IN_MINUTE),
        "backoff_init should fail when mul_coeff is 0");

    // cutoff_coeff == 0
    zassert_equal(-EINVAL, backoff_init(&backoff, 1 * MS_IN_MINUTE, 0),
        "backoff_init should fail when cutoff_coeff is 0");

    // NULL context
    zassert_equal(-EINVAL, backoff_init(NULL, 1 * MS_IN_MINUTE, 2 * MS_IN_MINUTE),
        "backoff_init should fail when context is NULL");
}

ZTEST(astarte_device_sdk_backoff, test_ordinary_backoff)
{
    struct backoff_context backoff;
    int err = backoff_init(&backoff, 1 * MS_IN_MINUTE, 18 * MS_IN_MINUTE);
    zassert_equal(0, err, "backoff_init returned an error");

    uint32_t delay;

    delay = backoff_get_next_delay(&backoff);
    zassert_true(delay >= 0 && delay <= 2 * MS_IN_MINUTE, "Delay out of bounds");

    delay = backoff_get_next_delay(&backoff);
    zassert_true(delay >= 1 * MS_IN_MINUTE && delay <= 3 * MS_IN_MINUTE, "Delay out of bounds");

    delay = backoff_get_next_delay(&backoff);
    zassert_true(delay >= 3 * MS_IN_MINUTE && delay <= 5 * MS_IN_MINUTE, "Delay out of bounds");

    delay = backoff_get_next_delay(&backoff);
    zassert_true(delay >= 7 * MS_IN_MINUTE && delay <= 9 * MS_IN_MINUTE, "Delay out of bounds");

    delay = backoff_get_next_delay(&backoff);
    zassert_true(delay >= 15 * MS_IN_MINUTE && delay <= 17 * MS_IN_MINUTE, "Delay out of bounds");

    for (size_t i = 0; i < 1048576u; i++) {
        delay = backoff_get_next_delay(&backoff);
        zassert_true(delay >= 17 * MS_IN_MINUTE && delay <= 19 * MS_IN_MINUTE,
            "Delay out of bounds in loop");
    }
}

ZTEST(astarte_device_sdk_backoff, test_very_large_backoff)
{
    struct backoff_context backoff;
    int err = backoff_init(&backoff, 1 * MS_IN_HOUR, 40 * MS_IN_DAY);
    zassert_equal(0, err, "backoff_init returned an error");

    uint32_t delay;

    delay = backoff_get_next_delay(&backoff);
    zassert_true(delay >= 0 && delay <= 2 * MS_IN_HOUR, "Delay out of bounds");

    delay = backoff_get_next_delay(&backoff);
    zassert_true(delay >= 1 * MS_IN_HOUR && delay <= 3 * MS_IN_HOUR, "Delay out of bounds");

    delay = backoff_get_next_delay(&backoff);
    zassert_true(delay >= 3 * MS_IN_HOUR && delay <= 5 * MS_IN_HOUR, "Delay out of bounds");

    delay = backoff_get_next_delay(&backoff);
    zassert_true(delay >= 7 * MS_IN_HOUR && delay <= 9 * MS_IN_HOUR, "Delay out of bounds");

    // A lot of calls in between
    for (size_t i = 0; i < 1000000u; i++) {
        backoff_get_next_delay(&backoff);
    }

    // Check it settled around the proper value (40 days +/- 1 hour of jitter)
    uint32_t lower_bound = 40 * MS_IN_DAY - 1 * MS_IN_HOUR;
    uint32_t upper_bound = 40 * MS_IN_DAY + 1 * MS_IN_HOUR;
    for (size_t i = 0; i < 100u; i++) {
        delay = backoff_get_next_delay(&backoff);
        zassert_true(
            delay >= lower_bound && delay <= upper_bound, "Delay out of bounds after settling");
    }
}
