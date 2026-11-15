#pragma once

#include <stdint.h>

#include <stddef.h>
#include <string.h>

static inline int dm_vk_mali_model_matches(const char *device_name, const char *model) {
    if(!device_name || !model)return 0;
    size_t length=strlen(model);
    for(const char *match=device_name;(match=strstr(match,model))!=NULL;match++){
        unsigned char next=(unsigned char)match[length];
        if(next<'0'||next>'9')return 1;
    }
    return 0;
}

// Keep the runtime policy architecture-based rather than device-model-based. These are the Mali families
// covered by the bundled MaliSX2/PanVK line; unknown Mali names remain eligible for capability probing.
static inline const char *dm_vk_mali_profile(const char *device_name) {
    static const char *const bifrost[]={"Mali-G31","Mali-G51","Mali-G52","Mali-G71","Mali-G72","Mali-G76"};
    static const char *const valhall[]={"Mali-G57","Mali-G68","Mali-G77","Mali-G78","Mali-G78AE",
        "Mali-G310","Mali-G510","Mali-G610","Mali-G710","Mali-G615","Mali-G715"};
    if(!device_name || !strstr(device_name,"Mali"))return "generic-vulkan";
    for(size_t i=0;i<sizeof bifrost/sizeof bifrost[0];i++)
        if(dm_vk_mali_model_matches(device_name,bifrost[i]))return "mali-bifrost";
    for(size_t i=0;i<sizeof valhall/sizeof valhall[0];i++)
        if(dm_vk_mali_model_matches(device_name,valhall[i]))return "mali-valhall";
    return "mali-unknown";
}

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
