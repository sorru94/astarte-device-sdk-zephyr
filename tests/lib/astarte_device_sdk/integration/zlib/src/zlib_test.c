/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

#include "astarte_device_sdk/result.h"

#include <astarte_zlib.h>

LOG_MODULE_REGISTER(zlib_test, CONFIG_LOG_DEFAULT_LEVEL); // NOLINT

ZTEST_SUITE(astarte_device_sdk_zlib, NULL, NULL, NULL, NULL, NULL); // NOLINT

#define EXP_EMPTY_COMPRESSED_BOUND 13
#define EXP_EMPTY_COMPRESSED_LEN 8

ZTEST(astarte_device_sdk_zlib, test_zlib_compress_empty) // NOLINT
{
    char input_text[] = "";
    size_t input_text_len = ARRAY_SIZE(input_text) - 1;

    uLongf compressed_len = compressBound(input_text_len);
    zassert_equal(compressed_len, EXP_EMPTY_COMPRESSED_BOUND,
        "compressBound unexpected result: %zu", compressed_len);

    uint8_t compressed[EXP_EMPTY_COMPRESSED_BOUND] = { 0 };
    int res = astarte_zlib_compress(compressed, &compressed_len, input_text, input_text_len);
    zassert_equal(res, Z_OK, "compress unexpected result: %d", res);
    zassert_equal(compressed_len, EXP_EMPTY_COMPRESSED_LEN,
        "astarte_zlib_compress unexpected result: %zu", compressed_len);
    uint8_t exp_compressed[EXP_EMPTY_COMPRESSED_LEN] = { 0x18, 0x95, 0x3, 0x0, 0x0, 0x0, 0x0, 0x1 };

    zassert_mem_equal(compressed, exp_compressed, EXP_EMPTY_COMPRESSED_LEN,
        "0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x", compressed[0], compressed[1],
        compressed[2], compressed[3], compressed[4], compressed[5], compressed[6], compressed[7]);

    char output_text[ARRAY_SIZE(input_text)] = { 0 };
    uLongf output_text_len = ARRAY_SIZE(output_text);
    res = astarte_zlib_uncompress(output_text, &output_text_len, exp_compressed, compressed_len);
    zassert_equal(res, Z_OK, "compress unexpected result: %d", res);
    zassert_mem_equal(output_text, input_text, ARRAY_SIZE(input_text),
        "Incorrectly decompressed data '%s'", output_text);
}

#define EXP_COMPRESSED_BOUND 23
#define EXP_COMPRESSED_LEN 18

ZTEST(astarte_device_sdk_zlib, test_zlib_compress) // NOLINT
{
    char input_text[] = "HelloWorld";
    size_t input_text_len = ARRAY_SIZE(input_text) - 1;

    uLongf compressed_len = compressBound(input_text_len);
    zassert_equal(compressed_len, EXP_COMPRESSED_BOUND, "compressBound unexpected result: %zu",
        compressed_len);

    uint8_t compressed[EXP_COMPRESSED_BOUND] = { 0 };
    int res = astarte_zlib_compress(compressed, &compressed_len, input_text, input_text_len);
    zassert_equal(res, Z_OK, "compress unexpected result: %d", res);
    zassert_equal(compressed_len, EXP_COMPRESSED_LEN,
        "astarte_zlib_compress unexpected result: %zu", compressed_len);
    uint8_t exp_compressed[EXP_COMPRESSED_LEN] = { 0x18, 0x95, 0xf3, 0x48, 0xcd, 0xc9, 0xc9, 0xf,
        0xcf, 0x2f, 0xca, 0x49, 0x1, 0x0, 0x15, 0x56, 0x3, 0xfd };

    zassert_mem_equal(compressed, exp_compressed, EXP_COMPRESSED_LEN,
        "0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, "
        "0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x",
        compressed[0], compressed[1], compressed[2], compressed[3], compressed[4], compressed[5],
        compressed[6], compressed[7], compressed[8], compressed[9], compressed[10], compressed[11],
        compressed[12], compressed[13], compressed[14], compressed[15], compressed[16],
        compressed[17]);

    char output_text[ARRAY_SIZE(input_text)] = { 0 };
    uLongf output_text_len = ARRAY_SIZE(output_text);
    res = astarte_zlib_uncompress(output_text, &output_text_len, exp_compressed, compressed_len);
    zassert_equal(res, Z_OK, "compress unexpected result: %d", res);
    zassert_mem_equal(output_text, input_text, ARRAY_SIZE(input_text),
        "Incorrectly decompressed data '%s'", output_text);
}
