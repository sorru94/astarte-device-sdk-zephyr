/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nvs.h"

#include <zephyr/drivers/flash.h>
#include <zephyr/fs/nvs.h>
#include <zephyr/kernel.h>
#include <zephyr/storage/flash_map.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(nvs, CONFIG_APP_LOG_LEVEL); // NOLINT

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

static struct nvs_fs file_system;

#define NVS_PARTITION storage_partition
#define NVS_PARTITION_DEVICE FIXED_PARTITION_DEVICE(NVS_PARTITION)
#define NVS_PARTITION_OFFSET FIXED_PARTITION_OFFSET(NVS_PARTITION)

#define NVS_SECTOR_COUNT 3U

#define NVS_FIRS_ENTRY_ID 1

/************************************************
 *         Global functions definitions         *
 ***********************************************/

int nvs_init(void)
{
    int nvs_rc = 0;
    struct flash_pages_info info;

    /* define the nvs file system by settings with:
     *	sector_size equal to the pagesize,
     *	3 sectors
     *	starting at NVS_PARTITION_OFFSET
     */
    file_system.flash_device = NVS_PARTITION_DEVICE;
    if (!device_is_ready(file_system.flash_device)) {
        LOG_ERR("Flash device %s is not ready.", file_system.flash_device->name); // NOLINT
        return -1;
    }
    file_system.offset = NVS_PARTITION_OFFSET;
    nvs_rc = flash_get_page_info_by_offs(file_system.flash_device, file_system.offset, &info);
    if (nvs_rc) {
        LOG_ERR("Unable to get page info."); // NOLINT
        return -1;
    }
    file_system.sector_size = info.size;
    file_system.sector_count = NVS_SECTOR_COUNT;

    nvs_rc = nvs_mount(&file_system);
    if (nvs_rc) {
        LOG_ERR("Flash Init failed."); // NOLINT
        return -1;
    }
    return 0;
}

int nvs_has_cred_secr(bool *res)
{
    int nvs_rc = nvs_read(&file_system, NVS_FIRS_ENTRY_ID, NULL, 0);
    if (nvs_rc == -ENOENT) {
        *res = false;
        return 0;
    }
    if (nvs_rc < 0) {
        LOG_ERR("nvs_read error %d.", nvs_rc); // NOLINT
        return -1;
    }
    *res = true;
    return 0;
}

int nvs_get_cred_secr(char *cred_secr, size_t cred_secr_size)
{
    int nvs_rc = nvs_read(&file_system, NVS_FIRS_ENTRY_ID, NULL, 0);
    if (nvs_rc < 0) {
        LOG_ERR("nvs_read error %d.", nvs_rc); // NOLINT
        return -1;
    }
    if (nvs_rc > cred_secr_size) {
        LOG_ERR("Unsufficient buffer space, %d bytes required.", nvs_rc); // NOLINT
        return -1;
    }

    nvs_rc = nvs_read(&file_system, NVS_FIRS_ENTRY_ID, cred_secr, cred_secr_size);
    if (nvs_rc <= 0) {
        LOG_ERR("nvs_read error %d.", nvs_rc); // NOLINT
        return -1;
    }
    return 0;
}

int nvs_store_cred_secr(char *cred_secr)
{
    ssize_t nvs_rc = nvs_write(&file_system, NVS_FIRS_ENTRY_ID, cred_secr, strlen(cred_secr) + 1);
    if (nvs_rc < 0) {
        LOG_ERR("nvs_write error %d.", nvs_rc); // NOLINT
        return -1;
    }
    return 0;
}
