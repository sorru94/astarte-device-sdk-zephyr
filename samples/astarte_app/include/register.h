/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef REGISTER_H
#define REGISTER_H

#include <astarte_device_sdk/device.h>

int register_device(char device_id[static ASTARTE_DEVICE_ID_LEN + 1],
    char cred_secr[static ASTARTE_PAIRING_CRED_SECR_LEN + 1]);

#endif // REGISTER_H
