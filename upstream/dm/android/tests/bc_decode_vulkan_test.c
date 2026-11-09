#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef HMODULE bc_vulkan_library;
static bc_vulkan_library bc_library_open(void) { return LoadLibraryA("vulkan-1.dll"); }
static void *bc_library_symbol(bc_vulkan_library library,const char *name) { return (void*)GetProcAddress(library,name); }
static void bc_library_close(bc_vulkan_library library) { FreeLibrary(library); }
#else
#include <dlfcn.h>
typedef void *bc_vulkan_library;
static bc_vulkan_library bc_library_open(void) { return dlopen("libvulkan.so.1",RTLD_NOW|RTLD_LOCAL); }
static void *bc_library_symbol(bc_vulkan_library library,const char *name) { return dlsym(library,name); }
static void bc_library_close(bc_vulkan_library library) { dlclose(library); }
#endif
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../../gfx/tex/nvn_bcdec.h"

#define LOAD_GLOBAL(name) PFN_vk##name p##name=(PFN_vk##name)bc_library_symbol(loader,"vk" #name); if(!p##name)fail("load " #name)
#define LOAD_INSTANCE(name) PFN_vk##name p##name=(PFN_vk##name)gipa(instance,"vk" #name); if(!p##name)fail("load " #name)
#define LOAD_DEVICE(name) PFN_vk##name p##name=(PFN_vk##name)gdpa(device,"vk" #name); if(!p##name)fail("load " #name)
static void fail(const char *s){fprintf(stderr,"FAIL: %s\n",s);exit(2);}
static void vkcheck(VkResult r,const char *s){if(r!=VK_SUCCESS){fprintf(stderr,"FAIL: %s VkResult=%d\n",s,(int)r);exit(2);}}
static uint32_t rng=0x5eed1234u;
static uint8_t random_byte(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return (uint8_t)rng;}
int main(int argc,char **argv){
    if(argc!=2)fail("usage: bc-test shader.spv");
    bc_vulkan_library loader=bc_library_open();if(!loader)fail("open Vulkan loader");
    PFN_vkGetInstanceProcAddr gipa=(PFN_vkGetInstanceProcAddr)bc_library_symbol(loader,"vkGetInstanceProcAddr");if(!gipa)fail("load vkGetInstanceProcAddr");
    LOAD_GLOBAL(CreateInstance);
    VkApplicationInfo app={.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO,.pApplicationName="D2R BC parity",.apiVersion=VK_API_VERSION_1_1};
    VkInstanceCreateInfo ici={.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,.pApplicationInfo=&app};VkInstance instance=VK_NULL_HANDLE;
    vkcheck(pCreateInstance(&ici,NULL,&instance),"vkCreateInstance");
    LOAD_INSTANCE(EnumeratePhysicalDevices);LOAD_INSTANCE(GetPhysicalDeviceProperties);LOAD_INSTANCE(GetPhysicalDeviceMemoryProperties);
    LOAD_INSTANCE(GetPhysicalDeviceQueueFamilyProperties);LOAD_INSTANCE(CreateDevice);
    uint32_t count=0;vkcheck(pEnumeratePhysicalDevices(instance,&count,NULL),"enumerate physical device count");if(!count)fail("no Vulkan physical device");
    VkPhysicalDevice *devices=calloc(count,sizeof *devices);if(!devices)fail("device array allocation");vkcheck(pEnumeratePhysicalDevices(instance,&count,devices),"enumerate physical devices");
    VkPhysicalDevice physical=VK_NULL_HANDLE;uint32_t family=UINT32_MAX;VkPhysicalDeviceProperties properties;
    for(uint32_t i=0;i<count&&!physical;i++){uint32_t n=0;pGetPhysicalDeviceQueueFamilyProperties(devices[i],&n,NULL);VkQueueFamilyProperties *q=calloc(n,sizeof *q);if(!q)fail("queue array allocation");pGetPhysicalDeviceQueueFamilyProperties(devices[i],&n,q);for(uint32_t j=0;j<n;j++)if(q[j].queueCount&&(q[j].queueFlags&VK_QUEUE_COMPUTE_BIT)){physical=devices[i];family=j;break;}free(q);}
    if(!physical)fail("no compute queue");
    pGetPhysicalDeviceProperties(physical,&properties);free(devices);
    float priority=1;VkDeviceQueueCreateInfo qci={.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,.queueFamilyIndex=family,.queueCount=1,.pQueuePriorities=&priority};
    VkDeviceCreateInfo dci={.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,.queueCreateInfoCount=1,.pQueueCreateInfos=&qci};VkDevice device=VK_NULL_HANDLE;
    vkcheck(pCreateDevice(physical,&dci,NULL,&device),"vkCreateDevice");
    PFN_vkGetDeviceProcAddr gdpa=(PFN_vkGetDeviceProcAddr)gipa(instance,"vkGetDeviceProcAddr");if(!gdpa)fail("load vkGetDeviceProcAddr");
    LOAD_DEVICE(DestroyDevice);LOAD_DEVICE(GetDeviceQueue);LOAD_DEVICE(CreateBuffer);LOAD_DEVICE(GetBufferMemoryRequirements);
    LOAD_DEVICE(AllocateMemory);LOAD_DEVICE(BindBufferMemory);LOAD_DEVICE(MapMemory);LOAD_DEVICE(UnmapMemory);LOAD_DEVICE(DestroyBuffer);LOAD_DEVICE(FreeMemory);
    LOAD_DEVICE(CreateDescriptorSetLayout);LOAD_DEVICE(CreatePipelineLayout);LOAD_DEVICE(CreateShaderModule);LOAD_DEVICE(CreateComputePipelines);
    LOAD_DEVICE(DestroyShaderModule);LOAD_DEVICE(DestroyPipeline);LOAD_DEVICE(DestroyPipelineLayout);LOAD_DEVICE(DestroyDescriptorSetLayout);
    LOAD_DEVICE(CreateDescriptorPool);LOAD_DEVICE(AllocateDescriptorSets);LOAD_DEVICE(UpdateDescriptorSets);
    LOAD_DEVICE(CreateCommandPool);LOAD_DEVICE(AllocateCommandBuffers);LOAD_DEVICE(ResetCommandBuffer);LOAD_DEVICE(BeginCommandBuffer);
    LOAD_DEVICE(CmdPipelineBarrier);LOAD_DEVICE(CmdBindPipeline);LOAD_DEVICE(CmdBindDescriptorSets);LOAD_DEVICE(CmdPushConstants);LOAD_DEVICE(CmdDispatch);
    LOAD_DEVICE(EndCommandBuffer);LOAD_DEVICE(QueueSubmit);LOAD_DEVICE(QueueWaitIdle);LOAD_DEVICE(DestroyCommandPool);LOAD_DEVICE(DestroyDescriptorPool);
    VkQueue queue; pGetDeviceQueue(device,family,0,&queue);
    VkPhysicalDeviceMemoryProperties memprops;pGetPhysicalDeviceMemoryProperties(physical,&memprops);uint32_t memory_type=UINT32_MAX;
    for(uint32_t i=0;i<memprops.memoryTypeCount;i++)if((memprops.memoryTypes[i].propertyFlags&(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))==(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)){memory_type=i;break;}
    if(memory_type==UINT32_MAX)fail("no host-visible coherent memory");
    VkDeviceSize storage_alignment=properties.limits.minStorageBufferOffsetAlignment;if(storage_alignment<4)storage_alignment=4;
    VkDeviceSize max_input=144,max_output=324,output_offset=(max_input+storage_alignment-1)&~(storage_alignment-1),buffer_size=output_offset+max_output;
    VkBufferCreateInfo bci={.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,.size=buffer_size,.usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,.sharingMode=VK_SHARING_MODE_EXCLUSIVE};VkBuffer buffer;
    vkcheck(pCreateBuffer(device,&bci,NULL,&buffer),"vkCreateBuffer");VkMemoryRequirements requirements;pGetBufferMemoryRequirements(device,buffer,&requirements);
    VkMemoryAllocateInfo mai={.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,.allocationSize=requirements.size,.memoryTypeIndex=memory_type};VkDeviceMemory memory;
    vkcheck(pAllocateMemory(device,&mai,NULL,&memory),"vkAllocateMemory");vkcheck(pBindBufferMemory(device,buffer,memory,0),"vkBindBufferMemory");void *mapped;
    vkcheck(pMapMemory(device,memory,0,VK_WHOLE_SIZE,0,&mapped),"vkMapMemory");
    VkDescriptorSetLayoutBinding bindings[2]={{.binding=0,.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.descriptorCount=1,.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT},{.binding=1,.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.descriptorCount=1,.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT}};
    VkDescriptorSetLayoutCreateInfo slci={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,.bindingCount=2,.pBindings=bindings};VkDescriptorSetLayout set_layout;
    vkcheck(pCreateDescriptorSetLayout(device,&slci,NULL,&set_layout),"vkCreateDescriptorSetLayout");VkPushConstantRange push={.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT,.size=24};
    VkPipelineLayoutCreateInfo plci={.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,.setLayoutCount=1,.pSetLayouts=&set_layout,.pushConstantRangeCount=1,.pPushConstantRanges=&push};VkPipelineLayout pipeline_layout;
    vkcheck(pCreatePipelineLayout(device,&plci,NULL,&pipeline_layout),"vkCreatePipelineLayout");
    FILE *sf=fopen(argv[1],"rb");if(!sf)fail("open shader SPIR-V");fseek(sf,0,SEEK_END);long shader_size=ftell(sf);rewind(sf);if(shader_size<=0||shader_size%4)fail("invalid shader size");uint32_t *shader=malloc((size_t)shader_size);if(!shader||fread(shader,1,(size_t)shader_size,sf)!=(size_t)shader_size)fail("read shader SPIR-V");fclose(sf);
    VkShaderModuleCreateInfo smci={.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,.codeSize=(size_t)shader_size,.pCode=shader};VkShaderModule module;
    vkcheck(pCreateShaderModule(device,&smci,NULL,&module),"vkCreateShaderModule");free(shader);
    VkComputePipelineCreateInfo cpci={.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,.stage={.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_COMPUTE_BIT,.module=module,.pName="main"},.layout=pipeline_layout};VkPipeline pipeline;
    vkcheck(pCreateComputePipelines(device,VK_NULL_HANDLE,1,&cpci,NULL,&pipeline),"vkCreateComputePipelines");pDestroyShaderModule(device,module,NULL);
    VkDescriptorPoolSize pool_size={.type=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.descriptorCount=2};VkDescriptorPoolCreateInfo dpci={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,.maxSets=1,.poolSizeCount=1,.pPoolSizes=&pool_size};VkDescriptorPool pool;
    vkcheck(pCreateDescriptorPool(device,&dpci,NULL,&pool),"vkCreateDescriptorPool");VkDescriptorSetAllocateInfo dsai={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,.descriptorPool=pool,.descriptorSetCount=1,.pSetLayouts=&set_layout};VkDescriptorSet set;
    vkcheck(pAllocateDescriptorSets(device,&dsai,&set),"vkAllocateDescriptorSets");
    VkCommandPoolCreateInfo cpoci={.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,.queueFamilyIndex=family};VkCommandPool command_pool;
    vkcheck(pCreateCommandPool(device,&cpoci,NULL,&command_pool),"vkCreateCommandPool");VkCommandBufferAllocateInfo cbai={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,.commandPool=command_pool,.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,.commandBufferCount=1};VkCommandBuffer command;
    vkcheck(pAllocateCommandBuffers(device,&cbai,&command),"vkAllocateCommandBuffers");
    const uint32_t dims[]={1,3,4,5,7,9};uint64_t tested=0;const uint8_t formats[]={0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c};
    for(unsigned f=0;f<sizeof formats;f++)for(unsigned wi=0;wi<6;wi++)for(unsigned hi=0;hi<6;hi++){
        uint32_t format=formats[f],w=dims[wi],h=dims[hi],bpp=format<=0x48?4:(format<=0x4a?1:2),bb=(format==0x41||format==0x42||format==0x45||format==0x46||format==0x49||format==0x4a)?8:16;
        uint32_t blocks_x=(w+3)/4,blocks_y=(h+3)/4,source_pitch=blocks_x*bb;VkDeviceSize input_size=(VkDeviceSize)source_pitch*blocks_y,output_pitch=(w*bpp+3)&~3u,output_size=output_pitch*h;
        memset(mapped,0,(size_t)buffer_size);for(size_t i=0;i<(size_t)input_size;i++)((uint8_t*)mapped)[i]=random_byte();memset((uint8_t*)mapped+output_offset,0xcd,(size_t)output_size);
        VkDescriptorBufferInfo bi[2]={{.buffer=buffer,.offset=0,.range=input_size},{.buffer=buffer,.offset=output_offset,.range=output_size}};
        VkWriteDescriptorSet writes[2]={{.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=set,.dstBinding=0,.descriptorCount=1,.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.pBufferInfo=&bi[0]},{.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=set,.dstBinding=1,.descriptorCount=1,.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.pBufferInfo=&bi[1]}};
        pUpdateDescriptorSets(device,2,writes,0,NULL);vkcheck(pResetCommandBuffer(command,0),"vkResetCommandBuffer");VkCommandBufferBeginInfo begin={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};vkcheck(pBeginCommandBuffer(command,&begin),"vkBeginCommandBuffer");
        VkBufferMemoryBarrier host={.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT,.dstAccessMask=VK_ACCESS_SHADER_READ_BIT,.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.buffer=buffer,.offset=0,.size=input_size};pCmdPipelineBarrier(command,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,0,NULL,1,&host,0,NULL);
        uint32_t constants[6]={source_pitch,w,h,(uint32_t)output_pitch,format,bpp};pCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);pCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline_layout,0,1,&set,0,NULL);pCmdPushConstants(command,pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof constants,constants);pCmdDispatch(command,(blocks_x+7)/8,(blocks_y+7)/8,1);
        VkBufferMemoryBarrier output={.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT,.dstAccessMask=VK_ACCESS_HOST_READ_BIT,.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.buffer=buffer,.offset=output_offset,.size=output_size};pCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,NULL,1,&output,0,NULL);
        vkcheck(pEndCommandBuffer(command),"vkEndCommandBuffer");VkSubmitInfo submit={.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,.commandBufferCount=1,.pCommandBuffers=&command};vkcheck(pQueueSubmit(queue,1,&submit,VK_NULL_HANDLE),"vkQueueSubmit");vkcheck(pQueueWaitIdle(queue),"vkQueueWaitIdle");
        uint8_t *expected=malloc((size_t)output_pitch*h);if(!expected)fail("reference output allocation");memset(expected,0xcd,(size_t)output_pitch*h);
        if(nvn_bc_decode(format,mapped,(size_t)source_pitch,w,h,expected,(size_t)output_pitch))fail("CPU reference decode");
        for(uint32_t y=0;y<h;y++)if(memcmp((uint8_t*)mapped+output_offset+y*output_pitch,expected+y*output_pitch,(size_t)w*bpp)){fprintf(stderr,"Mismatch format=%#x dims=%ux%u row=%u\n",format,w,h,y);return 3;}
        free(expected);tested++;
    }
    printf("BC compute parity PASS: %llu cases (BC1-BC5, 1/3/4/5/7/9 edges) on %s; Vulkan driver %u\n",(unsigned long long)tested,properties.deviceName,properties.driverVersion);
    pQueueWaitIdle(queue);pUnmapMemory(device,memory);pDestroyBuffer(device,buffer,NULL);pFreeMemory(device,memory,NULL);
    pDestroyPipeline(device,pipeline,NULL);pDestroyPipelineLayout(device,pipeline_layout,NULL);
    pDestroyDescriptorPool(device,pool,NULL);pDestroyDescriptorSetLayout(device,set_layout,NULL);
    pDestroyCommandPool(device,command_pool,NULL);pDestroyDevice(device,NULL);
    PFN_vkDestroyInstance destroy_instance=(PFN_vkDestroyInstance)gipa(instance,"vkDestroyInstance");if(destroy_instance)destroy_instance(instance,NULL);bc_library_close(loader);return 0;
}
