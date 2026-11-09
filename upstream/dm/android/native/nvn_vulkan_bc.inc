#include "bc_decode_spv.h"


static VkDescriptorSetLayout bc_gpu_set_layout;
static VkPipelineLayout bc_gpu_pipeline_layout;
static VkPipeline bc_gpu_pipeline;
static int bc_gpu_ready;

static void bc_gpu_prepare(void) {
    if(gpu_supports_bc()){
        LOG("[vulkan-bc-gpu] enabled=0 reason=hardware-bc");return;
    }
    if(!perf_config_int("bc_decode_gpu",0)){
        LOG("[vulkan-bc-gpu] enabled=0 reason=perf.conf-default-or-bc_decode_gpu-0");return;
    }
    VkDescriptorSetLayoutBinding bindings[2]={
        {.binding=0,.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.descriptorCount=1,.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT},
        {.binding=1,.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.descriptorCount=1,.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT}};
    VkDescriptorSetLayoutCreateInfo set_info={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,.bindingCount=2,.pBindings=bindings};
    VkResult result=vkCreateDescriptorSetLayout(device,&set_info,NULL,&bc_gpu_set_layout);
    if(result!=VK_SUCCESS)goto failed;
    VkPushConstantRange push={.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT,.size=6*sizeof(uint32_t)};
    VkPipelineLayoutCreateInfo layout_info={.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,.setLayoutCount=1,.pSetLayouts=&bc_gpu_set_layout,.pushConstantRangeCount=1,.pPushConstantRanges=&push};
    result=vkCreatePipelineLayout(device,&layout_info,NULL,&bc_gpu_pipeline_layout);
    if(result!=VK_SUCCESS)goto failed;
    VkShaderModule module=VK_NULL_HANDLE;
    VkShaderModuleCreateInfo module_info={.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,.codeSize=DM_VK_BC_DECODE_SPV_BYTES,.pCode=dm_vk_bc_decode_spv};
    result=vkCreateShaderModule(device,&module_info,NULL,&module);
    if(result!=VK_SUCCESS)goto failed;
    VkComputePipelineCreateInfo pipeline_info={.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage={.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_COMPUTE_BIT,.module=module,.pName="main"},.layout=bc_gpu_pipeline_layout};
    result=pipeline_create_compute(&pipeline_info,&bc_gpu_pipeline);vkDestroyShaderModule(device,module,NULL);
    if(result!=VK_SUCCESS)goto failed;
    bc_gpu_ready=1;
    LOG("[vulkan-bc-gpu] enabled=1 formats=BC1-BC5 local_size=8x8x1 path=compute-buffer-then-image-copy fallback=cpu");
    return;
failed:
    if(bc_gpu_pipeline)vkDestroyPipeline(device,bc_gpu_pipeline,NULL);
    if(bc_gpu_pipeline_layout)vkDestroyPipelineLayout(device,bc_gpu_pipeline_layout,NULL);
    if(bc_gpu_set_layout)vkDestroyDescriptorSetLayout(device,bc_gpu_set_layout,NULL);
    bc_gpu_pipeline=VK_NULL_HANDLE;bc_gpu_pipeline_layout=VK_NULL_HANDLE;bc_gpu_set_layout=VK_NULL_HANDLE;
    LOG("[vulkan-bc-gpu] enabled=0 reason=pipeline-init VkResult=%d fallback=cpu",(int)result);
}

static int bc_gpu_plan_make(uint32_t format,uint32_t target,uint32_t width,uint32_t height,uint32_t source_pitch,
                            uint64_t input_size,int bytes_per_pixel,bc_gpu_plan *plan) {
    if(!bc_gpu_ready||!plan||!width||!height||!source_pitch||!input_size||
       width>UINT32_MAX-3u||height>UINT32_MAX-3u||
       target==NVN_TEXTURE_TARGET_1D||target==NVN_TEXTURE_TARGET_1D_ARRAY||target==NVN_TEXTURE_TARGET_3D)return 0;
    switch(format){
    case 0x41:case 0x42:case 0x43:case 0x44:case 0x45:case 0x46:case 0x47:case 0x48:
    case 0x49:case 0x4a:case 0x4b:case 0x4c:break;
    default:return 0;
    }
    uint32_t unpacked_format;int expected_bpp;
    if(!nvn_bc_unpacked(format,&unpacked_format,&expected_bpp)||expected_bpp!=bytes_per_pixel||
       (bytes_per_pixel!=1&&bytes_per_pixel!=2&&bytes_per_pixel!=4))return 0;
    uint32_t block_bytes=(format==0x41||format==0x42||format==0x45||format==0x46||format==0x49||format==0x4a)?8u:16u;
    uint64_t blocks_x=((uint64_t)width+3)/4,blocks_y=((uint64_t)height+3)/4;
    if(blocks_x>UINT64_MAX/block_bytes||source_pitch<blocks_x*block_bytes||
       blocks_y>UINT64_MAX/source_pitch||input_size<(uint64_t)source_pitch*blocks_y)return 0;
    uint64_t raw_pitch=(uint64_t)width*(unsigned)bytes_per_pixel;
    if(raw_pitch>UINT32_MAX-3u)return 0;
    uint64_t output_pitch=(raw_pitch+3u)&~3ull;
    if(output_pitch>UINT32_MAX||output_pitch>UINT64_MAX/height)return 0;
    uint64_t output_size=output_pitch*height;
    VkDeviceSize alignment=gpu_limits.minStorageBufferOffsetAlignment;
    if(alignment<4)alignment=4;
    if((alignment&(alignment-1))||input_size>UINT64_MAX-(alignment-1))return 0;
    uint64_t output_offset=(input_size+alignment-1)&~(uint64_t)(alignment-1);
    if(output_offset>UINT64_MAX-output_size||input_size>gpu_limits.maxStorageBufferRange||
       output_size>gpu_limits.maxStorageBufferRange||output_offset+output_size>(1ull<<32))return 0;
    plan->input_size=input_size;plan->output_offset=output_offset;plan->output_size=output_size;plan->output_pitch=(uint32_t)output_pitch;
    return 1;
}

static int bc_gpu_dispatch(VkBuffer buffer,const bc_gpu_plan *plan,uint32_t source_pitch,uint32_t width,uint32_t height,
                           uint32_t format,uint32_t bytes_per_pixel) {
    if(!bc_gpu_ready||!buffer||!plan)return 0;
    VkDescriptorSet set=descriptor_allocate(bc_gpu_set_layout);
    VkDescriptorBufferInfo buffers[2]={{buffer,0,plan->input_size},{buffer,plan->output_offset,plan->output_size}};
    VkWriteDescriptorSet writes[2]={
        {.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=set,.dstBinding=0,.descriptorCount=1,
         .descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.pBufferInfo=&buffers[0]},
        {.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=set,.dstBinding=1,.descriptorCount=1,
         .descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.pBufferInfo=&buffers[1]}};
    vkUpdateDescriptorSets(device,2,writes,0,NULL);
    VkBufferMemoryBarrier host_barrier={.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT,
        .dstAccessMask=VK_ACCESS_SHADER_READ_BIT,.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.buffer=buffer,.offset=0,.size=plan->input_size};
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,0,NULL,1,&host_barrier,0,NULL);
    uint32_t push[6]={source_pitch,width,height,plan->output_pitch,format,(uint32_t)bytes_per_pixel};
    vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_COMPUTE,bc_gpu_pipeline);
    vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_COMPUTE,bc_gpu_pipeline_layout,0,1,&set,0,NULL);
    vkCmdPushConstants(command,bc_gpu_pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof push,push);
    uint32_t blocks_x=(width+3u)/4u,blocks_y=(height+3u)/4u;
    vkCmdDispatch(command,(blocks_x+7u)/8u,(blocks_y+7u)/8u,1);
    VkBufferMemoryBarrier transfer_barrier={.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT,.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.buffer=buffer,.offset=plan->output_offset,.size=plan->output_size};
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,NULL,1,&transfer_barrier,0,NULL);
    return 1;
}
