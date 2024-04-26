/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <astarte_device_sdk/uuid.h>

#include <zephyr/ztest.h>

#include <string.h>

ZTEST_SUITE(astarte_device_sdk_uuid, NULL, NULL, NULL, NULL, NULL);

ZTEST(astarte_device_sdk_uuid, test_uuid_v5)
{
    astarte_uuid_t namespace;
    int result;
    result = astarte_uuid_from_string("c21fb11c-b6c9-452a-9e86-6075e313d7e2", namespace);
    zassert_true(result == 0, "astarte_uuid_from_string returned an error");
    astarte_uuid_t out;
    result = astarte_uuid_generate_v5(namespace, "00225588", 8, out);
    zassert_true(result == 0, "astarte_uuid_generate_v5 returned an error");
    char uuid_str[37];
    result = astarte_uuid_to_string(out, uuid_str, 37);
    zassert_true(result == 0, "astarte_uuid_from_string returned an error");

    zassert_true(strcmp("63c8fb48-02ab-53f4-a254-52956dcbbce4", uuid_str) == 0,
        "uuid_str != 63c8fb48-02ab-53f4-a254-52956dcbbce4");
}

ZTEST(astarte_device_sdk_uuid, test_uuid_from_string)
{
    const char *first_uuid_v4_string = "44b35f73-cfbd-43b4-8fef-ca7baea1375f";
    astarte_uuid_t first_uuid_v4;
    int res = astarte_uuid_from_string(first_uuid_v4_string, first_uuid_v4);
    zassert_equal(0, res, "astarte_uuid_from_string returned an error");
    uint8_t expected_first_uuid_v4_byte_array[16u] = { 0x44, 0xb3, 0x5f, 0x73, 0xcf, 0xbd, 0x43,
        0xb4, 0x8f, 0xef, 0xca, 0x7b, 0xae, 0xa1, 0x37, 0x5f };
    for (size_t i = 0; i < 16; i++)
        zassert_equal(expected_first_uuid_v4_byte_array[i], first_uuid_v4[i],
            "first_uuid != from expected value");

    const char *second_uuid_v4_string = "6f2fd4cb-94a0-41c7-8d27-864c6b13b8c0";
    astarte_uuid_t second_uuid_v4;
    res = astarte_uuid_from_string(second_uuid_v4_string, second_uuid_v4);
    zassert_equal(0, res, "astarte_uuid_from_string returned an error");
    uint8_t expected_second_uuid_v4_byte_array[16u] = { 0x6f, 0x2f, 0xd4, 0xcb, 0x94, 0xa0, 0x41,
        0xc7, 0x8d, 0x27, 0x86, 0x4c, 0x6b, 0x13, 0xb8, 0xc0 };
    for (size_t i = 0; i < 16; i++)
        zassert_equal(expected_second_uuid_v4_byte_array[i], second_uuid_v4[i],
            "second_uuid != from expected value");

    const char *third_uuid_v4_string = "8f65dbbc-5868-4015-8523-891cc0bffa58";
    astarte_uuid_t third_uuid_v4;
    res = astarte_uuid_from_string(third_uuid_v4_string, third_uuid_v4);
    zassert_equal(0, res, "astarte_uuid_from_string returned an error");
    uint8_t expected_third_uuid_v4_byte_array[16u] = { 0x8f, 0x65, 0xdb, 0xbc, 0x58, 0x68, 0x40,
        0x15, 0x85, 0x23, 0x89, 0x1c, 0xc0, 0xbf, 0xfa, 0x58 };
    for (size_t i = 0; i < 16; i++)
        zassert_equal(expected_third_uuid_v4_byte_array[i], third_uuid_v4[i],
            "third_uuid != from expected value");

    const char *first_uuid_v5_string = "0575a569-51eb-575c-afe4-ce7fc03bcdc5";
    astarte_uuid_t first_uuid_v5;
    res = astarte_uuid_from_string(first_uuid_v5_string, first_uuid_v5);
    zassert_equal(0, res, "astarte_uuid_from_string returned an error");
    uint8_t expected_first_uuid_v5_byte_array[16u] = { 0x05, 0x75, 0xa5, 0x69, 0x51, 0xeb, 0x57,
        0x5c, 0xaf, 0xe4, 0xce, 0x7f, 0xc0, 0x3b, 0xcd, 0xc5 };
    for (size_t i = 0; i < 16; i++)
        zassert_equal(expected_first_uuid_v5_byte_array[i], first_uuid_v5[i],
            "uuid_v5!= from expected value");
}

ZTEST(astarte_device_sdk_uuid, test_uuid_to_string)
{
    char first_uuid_v4_byte_array[37];
    const astarte_uuid_t first_uuid_v4 = { 0x44, 0xb3, 0x5f, 0x73, 0xcf, 0xbd, 0x43, 0xb4, 0x8f,
        0xef, 0xca, 0x7b, 0xae, 0xa1, 0x37, 0x5f };
    int err = astarte_uuid_to_string(first_uuid_v4, first_uuid_v4_byte_array, 37);
    zassert_equal(0, err, "astarte_uuid_to_string returned an error");
    const char *expected_first_uuid_v4_string = "44b35f73-cfbd-43b4-8fef-ca7baea1375f";
    zassert_true(strcmp(expected_first_uuid_v4_string, first_uuid_v4_byte_array) == 0,
        "expected 44b35f73-cfbd-43b4-8fef-ca7baea1375f");

    char second_uuid_v4_byte_array[37];
    const astarte_uuid_t second_uuid_v4 = { 0x6f, 0x2f, 0xd4, 0xcb, 0x94, 0xa0, 0x41, 0xc7, 0x8d,
        0x27, 0x86, 0x4c, 0x6b, 0x13, 0xb8, 0xc0 };
    err = astarte_uuid_to_string(second_uuid_v4, second_uuid_v4_byte_array, 37);
    zassert_equal(0, err, "astarte_uuid_to_string returned an error");
    const char *expected_second_uuid_v4_string = "6f2fd4cb-94a0-41c7-8d27-864c6b13b8c0";
    zassert_true(strcmp(expected_second_uuid_v4_string, second_uuid_v4_byte_array) == 0,
        "expected 6f2fd4cb-94a0-41c7-8d27-864c6b13b8c0");

    char first_uuid_v5_byte_array[37];
    const astarte_uuid_t first_uuid_v5 = { 0x05, 0x75, 0xa5, 0x69, 0x51, 0xeb, 0x57, 0x5c, 0xaf,
        0xe4, 0xce, 0x7f, 0xc0, 0x3b, 0xcd, 0xc5 };
    err = astarte_uuid_to_string(first_uuid_v5, first_uuid_v5_byte_array, 37);
    zassert_equal(0, err, "astarte_uuid_to_string returned an error");
    const char *expected_first_uuid_v5_string = "0575a569-51eb-575c-afe4-ce7fc03bcdc5";
    zassert_true(strcmp(expected_first_uuid_v5_string, first_uuid_v5_byte_array) == 0,
        "expected 0575a569-51eb-575c-afe4-ce7fc03bcdc5");
}

ZTEST(astarte_device_sdk_uuid, test_uuid_to_base64)
{
    char first_uuid_v4_base64[ASTARTE_UUID_BASE64_LEN + 1] = { 0 };
    const astarte_uuid_t first_uuid_v4 = { 0x44, 0xb3, 0x5f, 0x73, 0xcf, 0xbd, 0x43, 0xb4, 0x8f,
        0xef, 0xca, 0x7b, 0xae, 0xa1, 0x37, 0x5f };
    int err
        = astarte_uuid_to_base64(first_uuid_v4, first_uuid_v4_base64, ASTARTE_UUID_BASE64_LEN + 1);
    zassert_equal(0, err, "%s", astarte_result_to_name(err));
    const char expected_first_uuid_v4_base64[] = "RLNfc8+9Q7SP78p7rqE3Xw==";
    zassert_true(strcmp(expected_first_uuid_v4_base64, first_uuid_v4_base64) == 0,
        "expected: '%s', gotten: '%s'", expected_first_uuid_v4_base64, first_uuid_v4_base64);

    char second_uuid_v4_base64[ASTARTE_UUID_BASE64_LEN + 1];
    const astarte_uuid_t second_uuid_v4 = { 0x6f, 0x2f, 0xd4, 0xcb, 0x94, 0xa0, 0x41, 0xc7, 0x8d,
        0x27, 0x86, 0x4c, 0x6b, 0x13, 0xb8, 0xc0 };
    err = astarte_uuid_to_base64(
        second_uuid_v4, second_uuid_v4_base64, ASTARTE_UUID_BASE64_LEN + 1);
    zassert_equal(0, err, "astarte_uuid_to_string returned an error");
    const char expected_second_uuid_v4_base64[] = "by/Uy5SgQceNJ4ZMaxO4wA==";
    zassert_true(strcmp(expected_second_uuid_v4_base64, second_uuid_v4_base64) == 0,
        "expected: '%s', gotten: '%s'", expected_second_uuid_v4_base64, second_uuid_v4_base64);

    char first_uuid_v5_base64[ASTARTE_UUID_BASE64_LEN + 1];
    const astarte_uuid_t first_uuid_v5 = { 0x05, 0x75, 0xa5, 0x69, 0x51, 0xeb, 0x57, 0x5c, 0xaf,
        0xe4, 0xce, 0x7f, 0xc0, 0x3b, 0xcd, 0xc5 };
    err = astarte_uuid_to_base64(first_uuid_v5, first_uuid_v5_base64, ASTARTE_UUID_BASE64_LEN + 1);
    zassert_equal(0, err, "astarte_uuid_to_string returned an error");
    const char expected_first_uuid_v5_base64[] = "BXWlaVHrV1yv5M5/wDvNxQ==";
    zassert_true(strcmp(expected_first_uuid_v5_base64, first_uuid_v5_base64) == 0,
        "expected: '%s', gotten: '%s'", expected_first_uuid_v5_base64, first_uuid_v5_base64);
}

ZTEST(astarte_device_sdk_uuid, test_uuid_to_base64url)
{
    char first_uuid_v4_base64url[ASTARTE_UUID_BASE64URL_LEN + 1] = { 0 };
    const astarte_uuid_t first_uuid_v4 = { 0x44, 0xb3, 0x5f, 0x73, 0xcf, 0xbd, 0x43, 0xb4, 0x8f,
        0xef, 0xca, 0x7b, 0xae, 0xa1, 0x37, 0x5f };
    int err = astarte_uuid_to_base64url(
        first_uuid_v4, first_uuid_v4_base64url, ASTARTE_UUID_BASE64URL_LEN + 1);
    zassert_equal(0, err, "%s", astarte_result_to_name(err));
    const char expected_first_uuid_v4_base64url[] = "RLNfc8-9Q7SP78p7rqE3Xw";
    zassert_true(strcmp(expected_first_uuid_v4_base64url, first_uuid_v4_base64url) == 0,
        "expected: '%s', gotten: '%s'", expected_first_uuid_v4_base64url, first_uuid_v4_base64url);

    char second_uuid_v4_base64url[ASTARTE_UUID_BASE64URL_LEN + 1];
    const astarte_uuid_t second_uuid_v4 = { 0x6f, 0x2f, 0xd4, 0xcb, 0x94, 0xa0, 0x41, 0xc7, 0x8d,
        0x27, 0x86, 0x4c, 0x6b, 0x13, 0xb8, 0xc0 };
    err = astarte_uuid_to_base64url(
        second_uuid_v4, second_uuid_v4_base64url, ASTARTE_UUID_BASE64URL_LEN + 1);
    zassert_equal(0, err, "astarte_uuid_to_string returned an error");
    const char expected_second_uuid_v4_base64url[] = "by_Uy5SgQceNJ4ZMaxO4wA";
    zassert_true(strcmp(expected_second_uuid_v4_base64url, second_uuid_v4_base64url) == 0,
        "expected: '%s', gotten: '%s'", expected_second_uuid_v4_base64url,
        second_uuid_v4_base64url);

    char first_uuid_v5_base64url[ASTARTE_UUID_BASE64URL_LEN + 1];
    const astarte_uuid_t first_uuid_v5 = { 0x05, 0x75, 0xa5, 0x69, 0x51, 0xeb, 0x57, 0x5c, 0xaf,
        0xe4, 0xce, 0x7f, 0xc0, 0x3b, 0xcd, 0xc5 };
    err = astarte_uuid_to_base64url(
        first_uuid_v5, first_uuid_v5_base64url, ASTARTE_UUID_BASE64URL_LEN + 1);
    zassert_equal(0, err, "astarte_uuid_to_string returned an error");
    const char expected_first_uuid_v5_base64url[] = "BXWlaVHrV1yv5M5_wDvNxQ";
    zassert_true(strcmp(expected_first_uuid_v5_base64url, first_uuid_v5_base64url) == 0,
        "expected: '%s', gotten: '%s'", expected_first_uuid_v5_base64url, first_uuid_v5_base64url);
}
