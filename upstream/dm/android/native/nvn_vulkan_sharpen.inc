// 锐化通道实现 - 使用图形管线在 present 路径中替换 vkCmdBlitImage
// Sharpen pass implementation - uses graphics pipeline in present path to replace vkCmdBlitImage

#include "sharpen_vert_spv.h"
#include "sharpen_frag_spv.h"

static VkDescriptorSetLayout sharpen_set_layout;
static VkPipelineLayout sharpen_pipeline_layout;
static VkPipeline sharpen_pipeline;
static VkRenderPass sharpen_render_pass;
static int sharpen_ready;
static VkSampler sharpen_sampler;
static VkDescriptorPool sharpen_desc_pool;

struct sharpen_push {
    float strength;
    int radius;
    float threshold;
    float _pad;  // alignment to 16 bytes
    float src_rect[4];
    float dst_rect[4];
};
static VkDescriptorSet sharpen_desc_set;
static VkImageView *sharpen_dst_views;
static VkFramebuffer *sharpen_framebuffers;
static uint32_t sharpen_framebuffer_count;
static VkExtent2D sharpen_framebuffer_extent;
static VkFormat sharpen_framebuffer_format;

static void sharpen_swapchain_cleanup(void) {
    if (sharpen_framebuffers) {
        for (uint32_t i = 0; i < sharpen_framebuffer_count; ++i)
            if (sharpen_framebuffers[i]) vkDestroyFramebuffer(device, sharpen_framebuffers[i], NULL);
    }
    if (sharpen_dst_views) {
        for (uint32_t i = 0; i < sharpen_framebuffer_count; ++i)
            if (sharpen_dst_views[i]) vkDestroyImageView(device, sharpen_dst_views[i], NULL);
    }
    free(sharpen_framebuffers);
    free(sharpen_dst_views);
    sharpen_framebuffers = NULL;
    sharpen_dst_views = NULL;
    sharpen_framebuffer_count = 0;
    sharpen_framebuffer_extent = (VkExtent2D){0, 0};
    sharpen_framebuffer_format = VK_FORMAT_UNDEFINED;
}

static int sharpen_swapchain_prepare(void) {
    if (!screen_images || !screen_count) return 0;
    if (sharpen_framebuffers && sharpen_framebuffer_count == screen_count &&
        sharpen_framebuffer_extent.width == screen_extent.width &&
        sharpen_framebuffer_extent.height == screen_extent.height &&
        sharpen_framebuffer_format == screen_format) return 1;
    sharpen_swapchain_cleanup();
    sharpen_dst_views = calloc(screen_count, sizeof *sharpen_dst_views);
    sharpen_framebuffers = calloc(screen_count, sizeof *sharpen_framebuffers);
    if (!sharpen_dst_views || !sharpen_framebuffers) {
        sharpen_swapchain_cleanup();
        return 0;
    }
    sharpen_framebuffer_count = screen_count;
    for (uint32_t i = 0; i < screen_count; ++i) {
        VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = screen_images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = screen_format,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        };
        VkResult result = vkCreateImageView(device, &view_info, NULL, &sharpen_dst_views[i]);
        if (result != VK_SUCCESS) {
            sharpen_swapchain_cleanup();
            return 0;
        }
        VkFramebufferCreateInfo fb_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = sharpen_render_pass,
            .attachmentCount = 1,
            .pAttachments = &sharpen_dst_views[i],
            .width = screen_extent.width,
            .height = screen_extent.height,
            .layers = 1
        };
        result = vkCreateFramebuffer(device, &fb_info, NULL, &sharpen_framebuffers[i]);
        if (result != VK_SUCCESS) {
            sharpen_swapchain_cleanup();
            return 0;
        }
    }
    sharpen_framebuffer_count = screen_count;
    sharpen_framebuffer_extent = screen_extent;
    sharpen_framebuffer_format = screen_format;
    return 1;
}

static void sharpen_prepare(void) {
    if (sharpen_ready) return;
    
    // Create sampler for the 3D image
    VkSamplerCreateInfo sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    };
    VkResult result = vkCreateSampler(device, &sampler_info, NULL, &sharpen_sampler);
    if (result != VK_SUCCESS) goto failed;
    
    // Descriptor set layout: one combined image sampler at binding 0
    VkDescriptorSetLayoutBinding binding = {
        .binding = 0, 
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
        .descriptorCount = 1, 
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
    };
    VkDescriptorSetLayoutCreateInfo set_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &binding
    };
    result = vkCreateDescriptorSetLayout(device, &set_info, NULL, &sharpen_set_layout);
    if (result != VK_SUCCESS) goto failed;
    
    // Create descriptor pool (1 set with 1 combined image sampler)
    VkDescriptorPoolSize pool_size = {
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 4  // Allow a few sets
    };
    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 4,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size
    };
    result = vkCreateDescriptorPool(device, &pool_info, NULL, &sharpen_desc_pool);
    if (result != VK_SUCCESS) goto failed;
    
    // Pipeline layout with push constants
    VkPushConstantRange push = {
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .size = sizeof(struct sharpen_push)
    };
    VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &sharpen_set_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push
    };
    result = vkCreatePipelineLayout(device, &layout_info, NULL, &sharpen_pipeline_layout);
    if (result != VK_SUCCESS) goto failed;
    
    // Create vertex shader module
    VkShaderModule vert_module = VK_NULL_HANDLE;
    VkShaderModuleCreateInfo vert_module_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = DM_VK_SHARPEN_VERT_SPV_BYTES,
        .pCode = dm_vk_sharpen_vert_spv
    };
    result = vkCreateShaderModule(device, &vert_module_info, NULL, &vert_module);
    if (result != VK_SUCCESS) goto failed;
    
    // Create fragment shader module
    VkShaderModule frag_module = VK_NULL_HANDLE;
    VkShaderModuleCreateInfo frag_module_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = DM_VK_SHARPEN_FRAG_SPV_BYTES,
        .pCode = dm_vk_sharpen_frag_spv
    };
    result = vkCreateShaderModule(device, &frag_module_info, NULL, &frag_module);
    if (result != VK_SUCCESS) {
        vkDestroyShaderModule(device, vert_module, NULL);
        goto failed;
    }
    
    // Preserve the cleared/bar-blitted pixels outside the destination rectangle.
    VkAttachmentDescription color_attachment = {
        .format = screen_format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };
    VkAttachmentReference color_ref = {.attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_ref
    };
    // Subpass dependency for the preceding transfer clear/bar blit.
    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
    };
    VkRenderPassCreateInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency
    };
    result = vkCreateRenderPass(device, &render_pass_info, NULL, &sharpen_render_pass);
    if (result != VK_SUCCESS) {
        vkDestroyShaderModule(device, vert_module, NULL);
        vkDestroyShaderModule(device, frag_module, NULL);
        goto failed;
    }
    
    // Graphics pipeline create info
    VkPipelineShaderStageCreateInfo stages[2] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vert_module, .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = frag_module, .pName = "main"}
    };
    
    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 0,
        .vertexAttributeDescriptionCount = 0
    };
    
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };
    
    VkViewport viewport = {0, 0, 1, 1, 0, 1};
    VkRect2D scissor = {{0, 0}, {1, 1}};
    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor
    };
    
    VkPipelineRasterizationStateCreateInfo raster = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.0f
    };
    
    VkPipelineMultisampleStateCreateInfo multisample = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };
    
    VkPipelineDepthStencilStateCreateInfo depth_stencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_FALSE,
        .depthWriteEnable = VK_FALSE
    };
    
    VkPipelineColorBlendAttachmentState blend_attachment = {
        .blendEnable = VK_FALSE,
        .colorWriteMask = 0xF
    };
    VkPipelineColorBlendStateCreateInfo color_blend = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blend_attachment
    };
    
    VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamic_states
    };
    
    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = stages,
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &raster,
        .pMultisampleState = &multisample,
        .pDepthStencilState = &depth_stencil,
        .pColorBlendState = &color_blend,
        .pDynamicState = &dynamic_state,
        .layout = sharpen_pipeline_layout,
        .renderPass = sharpen_render_pass,
        .subpass = 0
    };
    
    result = pipeline_create_graphics(&pipeline_info, &sharpen_pipeline);
    vkDestroyShaderModule(device, vert_module, NULL);
    vkDestroyShaderModule(device, frag_module, NULL);
    if (result != VK_SUCCESS) goto failed;
    VkDescriptorSetAllocateInfo descriptor_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = sharpen_desc_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &sharpen_set_layout
    };
    result = vkAllocateDescriptorSets(device, &descriptor_info, &sharpen_desc_set);
    if (result != VK_SUCCESS) goto failed;
    
    sharpen_ready = 1;
    LOG("[vulkan-sharpen] ready");
    return;
    
failed:
    if (sharpen_pipeline) vkDestroyPipeline(device, sharpen_pipeline, NULL);
    if (sharpen_pipeline_layout) vkDestroyPipelineLayout(device, sharpen_pipeline_layout, NULL);
    if (sharpen_render_pass) vkDestroyRenderPass(device, sharpen_render_pass, NULL);
    if (sharpen_set_layout) vkDestroyDescriptorSetLayout(device, sharpen_set_layout, NULL);
    if (sharpen_desc_pool) vkDestroyDescriptorPool(device, sharpen_desc_pool, NULL);
    if (sharpen_sampler) vkDestroySampler(device, sharpen_sampler, NULL);
    sharpen_pipeline = VK_NULL_HANDLE;
    sharpen_pipeline_layout = VK_NULL_HANDLE;
    sharpen_render_pass = VK_NULL_HANDLE;
    sharpen_set_layout = VK_NULL_HANDLE;
    sharpen_desc_pool = VK_NULL_HANDLE;
    sharpen_sampler = VK_NULL_HANDLE;
    sharpen_desc_set = VK_NULL_HANDLE;
    LOG("[vulkan-sharpen] init failed VkResult=%d", (int)result);
}

static void sharpen_cleanup(void) {
    sharpen_swapchain_cleanup();
    if (sharpen_pipeline) vkDestroyPipeline(device, sharpen_pipeline, NULL);
    if (sharpen_pipeline_layout) vkDestroyPipelineLayout(device, sharpen_pipeline_layout, NULL);
    if (sharpen_render_pass) vkDestroyRenderPass(device, sharpen_render_pass, NULL);
    if (sharpen_set_layout) vkDestroyDescriptorSetLayout(device, sharpen_set_layout, NULL);
    if (sharpen_desc_pool) vkDestroyDescriptorPool(device, sharpen_desc_pool, NULL);
    if (sharpen_sampler) vkDestroySampler(device, sharpen_sampler, NULL);
    sharpen_pipeline = VK_NULL_HANDLE;
    sharpen_pipeline_layout = VK_NULL_HANDLE;
    sharpen_render_pass = VK_NULL_HANDLE;
    sharpen_set_layout = VK_NULL_HANDLE;
    sharpen_desc_pool = VK_NULL_HANDLE;
    sharpen_sampler = VK_NULL_HANDLE;
    sharpen_desc_set = VK_NULL_HANDLE;
    sharpen_ready = 0;
}

// Called from present path when dm_sharpen_enabled() is true
// Replaces the vkCmdBlitImage at line ~1414
// src_image: 3D render target (e->image)
// dst_image: swapchain image (screen_images[index])
// src_rect: sx, sy, sw, sh in source image coordinates
// dst_rect: x, y, dw, dh in destination image coordinates
static int sharpen_draw(VkCommandBuffer command, image_entry *src_image, VkImage dst_image,
                        uint32_t dst_index, uint32_t dst_width, uint32_t dst_height,
                        int sx, int sy, int sw, int sh,
                        int dx, int dy, int dw, int dh) {
    if (!dm_sharpen_enabled()) return 0;
    sharpen_prepare();
    if (!sharpen_ready) return 0;
    
    float strength = 0.0f;
    int radius = 1;
    float threshold = 0.0f;
    dm_sharpen_get(&strength, &radius, &threshold);
    if (strength <= 0.0f) return 0;
    if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return 0;
    if (!sharpen_swapchain_prepare() || dst_index >= sharpen_framebuffer_count ||
        screen_images[dst_index] != dst_image) return 0;

    VkImageSubresourceRange source_range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageView src_view = image_view_get(src_image, VK_IMAGE_VIEW_TYPE_2D, source_range);
    VkDescriptorImageInfo src_image_info = {
        .sampler = sharpen_sampler,
        .imageView = src_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = sharpen_desc_set,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &src_image_info
    };
    vkUpdateDescriptorSets(device, 1, &write, 0, NULL);
    // Transition source image only after all fallback-prone resource creation succeeds.
    VkImageMemoryBarrier src_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = src_image->image,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &src_barrier);
    
    // Begin render pass
    VkRenderPassBeginInfo rp_begin = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = sharpen_render_pass,
        .framebuffer = sharpen_framebuffers[dst_index],
        .renderArea = {{0, 0}, {dst_width, dst_height}},
        .clearValueCount = 0,
        .pClearValues = NULL
    };
    vkCmdBeginRenderPass(command, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
    
    // Bind pipeline
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, sharpen_pipeline);
    
    // Set viewport and scissor to destination rect
    VkViewport viewport = {
        .x = (float)dx,
        .y = (float)dy,
        .width = (float)dw,
        .height = (float)dh,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    VkRect2D scissor = {
        .offset = {dx, dy},
        .extent = {dw, dh}
    };
    vkCmdSetViewport(command, 0, 1, &viewport);
    vkCmdSetScissor(command, 0, 1, &scissor);
    
    // Bind descriptor set
    vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, sharpen_pipeline_layout, 0, 1, &sharpen_desc_set, 0, NULL);
    
    // Push constants
    struct sharpen_push push = {
        .strength = strength,
        .radius = radius,
        .threshold = threshold,
        ._pad = 0.0f,
        .src_rect = {(float)sx, (float)sy, (float)sw, (float)sh},
        .dst_rect = {(float)dx, (float)dy, (float)dw, (float)dh}
    };
    vkCmdPushConstants(command, sharpen_pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
    
    // Draw fullscreen triangle (3 vertices)
    vkCmdDraw(command, 3, 1, 0, 0);
    
    // End render pass
    vkCmdEndRenderPass(command);
    
    // The render pass performs the final transition to PRESENT_SRC_KHR.
    
    return 1;
}