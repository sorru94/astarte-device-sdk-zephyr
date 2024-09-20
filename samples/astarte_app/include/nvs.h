/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NVS_H
#define NVS_H

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief NVS driver.
 *
 * @return 0 upon success, -1 otherwise.
 */
int nvs_init(void);

/**
 * @brief Check if credential secret is already stored in NVS.
 *
 * @param[out] res Set to true if found, false if not, unchanged upon error.
 * @return 0 upon success, -1 following an internal error.
 */
int nvs_has_cred_secr(bool *res);

/**
 * @brief Get the stored credential secret.
 *
 * @param[out] cred_secr Buffer where the credential secret will be stored.
 * @param[in] cred_secr_size Size of the credential secret buffer.
 * @return 0 upon success, -1 otherwise.
 */
int nvs_get_cred_secr(char *cred_secr, size_t cred_secr_size);

/**
 * @brief Get the stored credential secret.
 *
 * @param[out] cred_secr Credential scret to store.
 * @return 0 upon success, -1 otherwise.
 */
int nvs_store_cred_secr(char *cred_secr);

#endif /* NVS_H */
