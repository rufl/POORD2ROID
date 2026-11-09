#pragma once

#include <stdint.h>

#include <string.h>

static inline int dm_vk_malisx2_identity_matches(uint32_t vendor_id, const char *driver_info) {
    return vendor_id == 0x13b5 && driver_info && strstr(driver_info, "malisx2") != NULL;
}

// A requested custom driver is optional unless require_custom is set. Its subgroup policy is not portable to
// the system driver: the shader cache is checked against the system GPU's physical subgroup before shader load.
static inline int dm_vk_driver_prepare_system_fallback(int custom_requested, int require_custom,
        uint32_t *requested_subgroup_size, int *subgroup_policy_required) {
    if(!custom_requested||require_custom)return 0;
    *requested_subgroup_size=0;
    *subgroup_policy_required=0;
    return 1;
}
