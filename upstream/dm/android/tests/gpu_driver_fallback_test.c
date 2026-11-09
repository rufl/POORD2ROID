#include "../native/gpu_driver_policy.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    uint32_t subgroup_size=16;
    int subgroup_policy_required=1;
    assert(dm_vk_driver_prepare_system_fallback(1, 0, &subgroup_size, &subgroup_policy_required));
    assert(subgroup_size==0 && subgroup_policy_required==0);

    subgroup_size=64;subgroup_policy_required=1;
    assert(!dm_vk_driver_prepare_system_fallback(1, 1, &subgroup_size, &subgroup_policy_required));
    assert(subgroup_size==64 && subgroup_policy_required==1);
    assert(!dm_vk_driver_prepare_system_fallback(0, 0, &subgroup_size, &subgroup_policy_required));
    assert(subgroup_size==64 && subgroup_policy_required==1);


    assert(dm_vk_malisx2_identity_matches(0x13b5, "v1.r44p1-malisx2.0.4.0.s"));
    assert(dm_vk_malisx2_identity_matches(0x13b5, "v1.r44p1-malisx2.0.4.1.s"));
    assert(!dm_vk_malisx2_identity_matches(0x13b5, "stock ARM Vulkan driver"));
    assert(!dm_vk_malisx2_identity_matches(0x5143, "v1.r44p1-malisx2.0.4.0.s"));
    puts("Vulkan fallback: optional custom driver clears its subgroup override; required custom driver remains strict");
    return 0;
}
