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
    assert(strcmp(dm_vk_mali_profile("Mali-G57 MC2"), "mali-valhall")==0);
    assert(strcmp(dm_vk_mali_profile("Mali-G77 MC9"), "mali-valhall")==0);
    assert(strcmp(dm_vk_mali_profile("Mali-G310"), "mali-valhall")==0);
    assert(strcmp(dm_vk_mali_profile("Mali-G76 MC4"), "mali-bifrost")==0);
    assert(strcmp(dm_vk_mali_profile("Mali-G31"), "mali-bifrost")==0);
    assert(strcmp(dm_vk_mali_profile("Mali-G999"), "mali-unknown")==0);
    assert(strcmp(dm_vk_mali_profile("Adreno (TM) 740"), "generic-vulkan")==0);
    puts("Vulkan fallback and Mali family classification: optional custom driver fallback remains strict; G31-G715 families classify correctly");
    return 0;
}
