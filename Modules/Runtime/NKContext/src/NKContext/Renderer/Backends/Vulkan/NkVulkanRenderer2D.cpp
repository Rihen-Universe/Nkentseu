// =============================================================================
// NkVulkanRenderer2D.cpp — Vulkan 2D renderer
//
// Shaders: compiled from GLSL at build time via glslangValidator / shaderc,
// or use the inline SPIR-V below (generated from the canonical GLSL source).
// The SPIR-V is embedded as uint32 arrays to avoid runtime compilation.
//
// Vertex shader GLSL (for reference):
//   layout(push_constant) uniform PC { mat4 proj; } u_PC;
//   layout(location=0) in vec2 a_Pos;
//   layout(location=1) in vec2 a_UV;
//   layout(location=2) in vec4 a_Color;
//   layout(location=0) out vec2 v_UV;
//   layout(location=1) out vec4 v_Color;
//   void main() { v_UV=a_UV; v_Color=a_Color; gl_Position=u_PC.proj*vec4(a_Pos,0,1); }
//
// Fragment shader GLSL:
//   layout(set=0,binding=0) uniform sampler2D u_Tex;
//   layout(location=0) in vec2 v_UV;
//   layout(location=1) in vec4 v_Color;
//   layout(location=0) out vec4 out_Color;
//   void main() { out_Color = texture(u_Tex, v_UV) * v_Color; }
// =============================================================================
#include "NkVulkanRenderer2D.h"

#if NKENTSEU_HAS_VULKAN_HEADERS

#include "NKContext/Renderer/Resources/NkTexture.h"
#include "NKContext/Core/NkNativeContextAccess.h"
#include "NkVulkanShaderCompiler.h"
#include "NkRenderer2DVkSpv.inl"
#include "NKLogger/NkLog.h"
#include <cstring>

#define NK_VK2D_LOG(...) logger.Infof("[NkVk2D] " __VA_ARGS__)
#define NK_VK2D_ERR(...) logger.Errorf("[NkVk2D] " __VA_ARGS__)
#define NK_VK2D_CHECK(r) do { VkResult _r=(r); if(_r!=VK_SUCCESS){ NK_VK2D_ERR(#r " = %d",(int)_r); return false; } } while(0)

namespace nkentseu {
    namespace renderer {

        VkSampler NkVulkanRenderer2D::sSampler = VK_NULL_HANDLE;

        // ── Inline SPIR-V (minimal passthrough — replace with shaderc output) ────────
        // These are placeholder word counts; real SPIR-V must be compiled from the
        // GLSL above using glslangValidator -V or shaderc. At build time, add a
        // CMake custom_command that runs glslangValidator and embeds the .spv into
        // NkRenderer2DVkSpv.inl (similar to NkGraphicsDemosVkSpv.inl in the demo).
        //
        // For now we declare external arrays and include the generated header:
        // #include "NkRenderer2DVkSpv.inl"
        //
        // To generate:
        //   glslangValidator -V nkrenderer2d.vert -o nkrenderer2d_vert.spv
        //   glslangValidator -V nkrenderer2d.frag -o nkrenderer2d_frag.spv
        //   xxd -i nkrenderer2d_vert.spv >> NkRenderer2DVkSpv.inl (adjust to uint32)
        //
        // Minimal valid SPIR-V (identity VS, constant PS) for a zero-footprint build:
        // #include "NkRenderer2DVkSpv.inl"   // defines kVk2DVertSpv[], kVk2DFragSpv[]

        // =============================================================================
        bool NkVulkanRenderer2D::Initialize(NkIGraphicsContext* ctx) {
            if (mIsValid) return false;
            if (!ctx || ctx->GetApi() != NkGraphicsApi::NK_GFX_API_VULKAN) {
                NK_VK2D_ERR("Requires Vulkan context");
                return false;
            }
            mCtx    = ctx;
            mVkData = NkNativeContext::Vulkan(ctx);
            if (!mVkData || !mVkData->device) {
                NK_VK2D_ERR("Invalid Vulkan context data");
                return false;
            }

            if (!CreateSampler())             return false;
            if (!CreateDescriptorPool())      return false;
            if (!CreateDescriptorSetLayout()) return false;
            if (!CreatePipelineLayout())      return false;
            if (!CreatePipelines())           return false;
            if (!CreateBuffers())             return false;
            if (!CreateWhiteTexture())        return false;

            NkContextInfo info = ctx->GetInfo();
            const uint32 W = info.windowWidth  > 0 ? info.windowWidth  : 800;
            const uint32 H = info.windowHeight > 0 ? info.windowHeight : 600;
            mDefaultView.center = { W * 0.5f, H * 0.5f };
            mDefaultView.size   = { (float)W, (float)H };
            mCurrentView        = mDefaultView;
            mViewport           = { 0, 0, (int32)W, (int32)H };

            float proj[16];
            mCurrentView.ToProjectionMatrix(proj);
            memcpy(mProjection, proj, 64);

            mIsValid = true;
            NK_VK2D_LOG("Initialized");
            return true;
        }

        // =============================================================================
        bool NkVulkanRenderer2D::FindMemoryType(uint32 filter,
                                                VkMemoryPropertyFlags props,
                                                uint32& out) {
            VkPhysicalDeviceMemoryProperties mp;
            vkGetPhysicalDeviceMemoryProperties(mVkData->physicalDevice, &mp);
            for (uint32 i = 0; i < mp.memoryTypeCount; ++i) {
                if ((filter & (1u<<i)) && (mp.memoryTypes[i].propertyFlags & props) == props) {
                    out = i; return true;
                }
            }
            return false;
        }

        bool NkVulkanRenderer2D::CreateBuffer_Internal(VkDeviceSize size,
                                                        VkBufferUsageFlags usage,
                                                        VkMemoryPropertyFlags props,
                                                        VkBuffer& buf,
                                                        VkDeviceMemory& mem) {
            VkBufferCreateInfo bi{};
            bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bi.size  = size;
            bi.usage = usage;
            bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            NK_VK2D_CHECK(vkCreateBuffer(mVkData->device, &bi, nullptr, &buf));
            VkMemoryRequirements req;
            vkGetBufferMemoryRequirements(mVkData->device, buf, &req);
            uint32 memType = 0;
            if (!FindMemoryType(req.memoryTypeBits, props, memType)) return false;
            VkMemoryAllocateInfo ai{};
            ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            ai.allocationSize  = req.size;
            ai.memoryTypeIndex = memType;
            NK_VK2D_CHECK(vkAllocateMemory(mVkData->device, &ai, nullptr, &mem));
            vkBindBufferMemory(mVkData->device, buf, mem, 0);
            return true;
        }

        // =============================================================================
        bool NkVulkanRenderer2D::CreateSampler() {
            if (sSampler) return true;
            VkSamplerCreateInfo si{};
            si.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            si.magFilter    = VK_FILTER_LINEAR;
            si.minFilter    = VK_FILTER_LINEAR;
            si.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            si.maxLod       = VK_LOD_CLAMP_NONE;
            NK_VK2D_CHECK(vkCreateSampler(mVkData->device, &si, nullptr, &sSampler));
            return true;
        }

        // =============================================================================
        bool NkVulkanRenderer2D::CreateDescriptorPool() {
            VkDescriptorPoolSize poolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256 };
            VkDescriptorPoolCreateInfo pi{};
            pi.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            pi.maxSets       = 256;
            pi.poolSizeCount = 1;
            pi.pPoolSizes    = &poolSize;
            NK_VK2D_CHECK(vkCreateDescriptorPool(mVkData->device, &pi, nullptr, &mDescPool));
            return true;
        }

        // =============================================================================
        bool NkVulkanRenderer2D::CreateDescriptorSetLayout() {
            VkDescriptorSetLayoutBinding binding{};
            binding.binding            = 0;
            binding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            binding.descriptorCount    = 1;
            binding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
            binding.pImmutableSamplers = &sSampler;

            VkDescriptorSetLayoutCreateInfo li{};
            li.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            li.bindingCount = 1;
            li.pBindings    = &binding;
            NK_VK2D_CHECK(vkCreateDescriptorSetLayout(mVkData->device, &li, nullptr, &mSetLayout));
            return true;
        }

        // =============================================================================
        bool NkVulkanRenderer2D::CreatePipelineLayout() {
            // Push constant: 64-byte projection matrix
            VkPushConstantRange pc{};
            pc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            pc.offset     = 0;
            pc.size       = 64;

            VkPipelineLayoutCreateInfo pli{};
            pli.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pli.setLayoutCount         = 1;
            pli.pSetLayouts            = &mSetLayout;
            pli.pushConstantRangeCount = 1;
            pli.pPushConstantRanges    = &pc;
            NK_VK2D_CHECK(vkCreatePipelineLayout(mVkData->device, &pli, nullptr, &mPipeLayout));
            return true;
        }

        // =============================================================================
        static VkShaderModule MakeModule(VkDevice dev, const uint32* spv, uint32 wordCount) {
            VkShaderModuleCreateInfo ci{};
            ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            ci.codeSize = wordCount * sizeof(uint32);
            ci.pCode    = spv;
            VkShaderModule mod = VK_NULL_HANDLE;
            vkCreateShaderModule(dev, &ci, nullptr, &mod);
            return mod;
        }

        bool NkVulkanRenderer2D::CreatePipelines() {
            // ── Résoudre les modules shader ──────────────────────────────────────────
            VkShaderModule vs = VK_NULL_HANDLE;
            VkShaderModule fs = VK_NULL_HANDLE;
        
            if (kVk2DVertSpvWordCount > 0 && kVk2DFragSpvWordCount > 0) {
                // PATH A : SPIR-V pré-compilé (NkRenderer2DVkSpv.inl rempli)
                NK_VK2D_LOG("Using precompiled SPIR-V shaders");
                vs = MakeModule(mVkData->device, kVk2DVertSpv, kVk2DVertSpvWordCount);
                fs = MakeModule(mVkData->device, kVk2DFragSpv, kVk2DFragSpvWordCount);
            } else {
                // PATH B : Compilation GLSL à la volée
                NK_VK2D_LOG("Precompiled SPIR-V not available — compiling GLSL at runtime");
                NK_VK2D_LOG("(Run scripts/gen_spv.bat to generate precompiled shaders)");
        
                NkVector<uint32> vertSpv = NkCompileVertGLSL(kVk2DVertGLSL);
                NkVector<uint32> fragSpv = NkCompileFragGLSL(kVk2DFragGLSL);
        
                if (vertSpv.Empty() || fragSpv.Empty()) {
                    NK_VK2D_ERR(
                        "SPIR-V compilation failed.\n"
                        "Options :\n"
                        "  1. Run scripts/gen_spv.bat (Windows) or scripts/gen_spv.sh (Linux/macOS)\n"
                        "     to generate NkRenderer2DVkSpv.inl with precompiled SPIR-V.\n"
                        "  2. Add shaderc to your project and define NK_VK2D_USE_SHADERC.\n"
                        "  3. Add glslang to your project and define NK_VK2D_USE_GLSLANG."
                    );
                    return false;
                }
        
                vs = MakeModule(mVkData->device, vertSpv.Data(), (uint32)vertSpv.Size());
                fs = MakeModule(mVkData->device, fragSpv.Data(), (uint32)fragSpv.Size());
            }
        
            if (!vs || !fs) {
                NK_VK2D_ERR("Failed to create VkShaderModule");
                if (vs) vkDestroyShaderModule(mVkData->device, vs, nullptr);
                if (fs) vkDestroyShaderModule(mVkData->device, fs, nullptr);
                return false;
            }
        
            // ── Vertex input (NkVertex2D) ─────────────────────────────────────────────
            VkVertexInputBindingDescription vib{};
            vib.binding   = 0;
            vib.stride    = (uint32)sizeof(NkVertex2D);
            vib.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        
            VkVertexInputAttributeDescription attrs[3]{};
            attrs[0] = {0, 0, VK_FORMAT_R32G32_SFLOAT,  (uint32)offsetof(NkVertex2D, x)};
            attrs[1] = {1, 0, VK_FORMAT_R32G32_SFLOAT,  (uint32)offsetof(NkVertex2D, u)};
            attrs[2] = {2, 0, VK_FORMAT_R8G8B8A8_UNORM,  (uint32)offsetof(NkVertex2D, r)};
        
            VkPipelineVertexInputStateCreateInfo vi{};
            vi.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vi.vertexBindingDescriptionCount   = 1;
            vi.pVertexBindingDescriptions      = &vib;
            vi.vertexAttributeDescriptionCount = 3;
            vi.pVertexAttributeDescriptions    = attrs;
        
            VkPipelineInputAssemblyStateCreateInfo ia{};
            ia.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        
            VkPipelineViewportStateCreateInfo vps{};
            vps.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            vps.viewportCount = 1;
            vps.scissorCount  = 1;
        
            VkPipelineRasterizationStateCreateInfo rs{};
            rs.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            rs.polygonMode = VK_POLYGON_MODE_FILL;
            rs.cullMode    = VK_CULL_MODE_NONE;
            rs.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            rs.lineWidth   = 1.f;
        
            VkPipelineMultisampleStateCreateInfo ms{};
            ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        
            VkPipelineDepthStencilStateCreateInfo ds{};
            ds.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            ds.depthTestEnable  = VK_FALSE;
            ds.depthWriteEnable = VK_FALSE;
            ds.depthCompareOp   = VK_COMPARE_OP_ALWAYS;
        
            VkDynamicState dynStates[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
            VkPipelineDynamicStateCreateInfo dyn{};
            dyn.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dyn.dynamicStateCount = 2;
            dyn.pDynamicStates    = dynStates;
        
            VkPipelineShaderStageCreateInfo stages[2]{};
            stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
                        VK_SHADER_STAGE_VERTEX_BIT,   vs, "main", nullptr};
            stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
                        VK_SHADER_STAGE_FRAGMENT_BIT, fs, "main", nullptr};
        
            // ── Helper lambda : crée un pipeline avec un blend attachment donné ────────
            auto MakePipeline = [&](VkPipelineColorBlendAttachmentState blend,
                                    VkPipeline& out) -> bool {
                VkPipelineColorBlendStateCreateInfo cb{};
                cb.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
                cb.attachmentCount = 1;
                cb.pAttachments    = &blend;
        
                VkGraphicsPipelineCreateInfo gci{};
                gci.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
                gci.stageCount          = 2;
                gci.pStages             = stages;
                gci.pVertexInputState   = &vi;
                gci.pInputAssemblyState = &ia;
                gci.pViewportState      = &vps;
                gci.pRasterizationState = &rs;
                gci.pMultisampleState   = &ms;
                gci.pDepthStencilState  = &ds;
                gci.pColorBlendState    = &cb;
                gci.pDynamicState       = &dyn;
                gci.layout              = mPipeLayout;
                gci.renderPass          = mVkData->renderPass;
                gci.subpass             = 0;
        
                return vkCreateGraphicsPipelines(
                    mVkData->device, VK_NULL_HANDLE, 1, &gci, nullptr, &out) == VK_SUCCESS;
            };
        
            // ── Blend states ──────────────────────────────────────────────────────────
            const uint32 kColorMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        
            VkPipelineColorBlendAttachmentState blendAlpha{};
            blendAlpha.blendEnable         = VK_TRUE;
            blendAlpha.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            blendAlpha.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blendAlpha.colorBlendOp        = VK_BLEND_OP_ADD;
            blendAlpha.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            blendAlpha.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blendAlpha.alphaBlendOp        = VK_BLEND_OP_ADD;
            blendAlpha.colorWriteMask      = kColorMask;
        
            VkPipelineColorBlendAttachmentState blendAdd = blendAlpha;
            blendAdd.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
            blendAdd.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        
            VkPipelineColorBlendAttachmentState blendMul = blendAlpha;
            blendMul.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
            blendMul.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
            blendMul.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            blendMul.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        
            VkPipelineColorBlendAttachmentState blendNone = blendAlpha;
            blendNone.blendEnable    = VK_FALSE;
            blendNone.colorWriteMask = kColorMask;
        
            bool ok =
                MakePipeline(blendAlpha, mPipeAlpha) &&
                MakePipeline(blendAdd,   mPipeAdd)   &&
                MakePipeline(blendMul,   mPipeMul)   &&
                MakePipeline(blendNone,  mPipeNone);
        
            vkDestroyShaderModule(mVkData->device, vs, nullptr);
            vkDestroyShaderModule(mVkData->device, fs, nullptr);
        
            if (!ok) NK_VK2D_ERR("Pipeline creation failed");
            return ok;
        }

        // =============================================================================
        bool NkVulkanRenderer2D::CreateBuffers() {
            constexpr VkDeviceSize vbSize = (VkDeviceSize)kMaxVertices * sizeof(NkVertex2D);
            constexpr VkDeviceSize ibSize = (VkDeviceSize)kMaxIndices  * sizeof(uint32);

            const VkMemoryPropertyFlags hostVis =
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

            if (!CreateBuffer_Internal(vbSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, hostVis, mVB, mVBMem)) return false;
            if (!CreateBuffer_Internal(ibSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,  hostVis, mIB, mIBMem)) return false;

            vkMapMemory(mVkData->device, mVBMem, 0, vbSize, 0, &mVBMap);
            vkMapMemory(mVkData->device, mIBMem, 0, ibSize, 0, &mIBMap);
            return true;
        }

        // =============================================================================
        bool NkVulkanRenderer2D::CreateWhiteTexture() {
            // Create a 1x1 white RGBA image
            VkImageCreateInfo ici{};
            ici.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            ici.imageType     = VK_IMAGE_TYPE_2D;
            ici.format        = VK_FORMAT_R8G8B8A8_UNORM;
            ici.extent        = {1, 1, 1};
            ici.mipLevels     = 1;
            ici.arrayLayers   = 1;
            ici.samples       = VK_SAMPLE_COUNT_1_BIT;
            ici.tiling        = VK_IMAGE_TILING_OPTIMAL;
            ici.usage         = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            NK_VK2D_CHECK(vkCreateImage(mVkData->device, &ici, nullptr, &mWhiteImage));

            VkMemoryRequirements req;
            vkGetImageMemoryRequirements(mVkData->device, mWhiteImage, &req);
            uint32 memType = 0;
            FindMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memType);
            VkMemoryAllocateInfo ai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, req.size, memType };
            vkAllocateMemory(mVkData->device, &ai, nullptr, &mWhiteMem);
            vkBindImageMemory(mVkData->device, mWhiteImage, mWhiteMem, 0);

            // Upload white pixel via staging buffer
            VkBuffer   staging;   VkDeviceMemory stagingMem;
            const uint32 white = 0xFFFFFFFFu;
            CreateBuffer_Internal(4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                staging, stagingMem);
            void* mapped = nullptr;
            vkMapMemory(mVkData->device, stagingMem, 0, 4, 0, &mapped);
            memcpy(mapped, &white, 4);
            vkUnmapMemory(mVkData->device, stagingMem);

            // Transition + copy + transition via a one-shot command buffer
            VkCommandBufferAllocateInfo cbai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
            cbai.commandPool        = mVkData->commandPool;
            cbai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            cbai.commandBufferCount = 1;
            VkCommandBuffer cmd = VK_NULL_HANDLE;
            vkAllocateCommandBuffers(mVkData->device, &cbai, &cmd);

            VkCommandBufferBeginInfo cbbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkBeginCommandBuffer(cmd, &cbbi);

            // UNDEFINED → TRANSFER_DST
            VkImageMemoryBarrier b1{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            b1.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; b1.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            b1.srcAccessMask = 0; b1.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            b1.image = mWhiteImage;
            b1.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b1);

            VkBufferImageCopy region{};
            region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            region.imageExtent      = {1, 1, 1};
            vkCmdCopyBufferToImage(cmd, staging, mWhiteImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

            // TRANSFER_DST → SHADER_READ_ONLY
            VkImageMemoryBarrier b2 = b1;
            b2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; b2.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            b2.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; b2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b2);

            vkEndCommandBuffer(cmd);
            VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
            si.commandBufferCount = 1; si.pCommandBuffers = &cmd;
            vkQueueSubmit(mVkData->graphicsQueue, 1, &si, VK_NULL_HANDLE);
            vkQueueWaitIdle(mVkData->graphicsQueue);
            vkFreeCommandBuffers(mVkData->device, mVkData->commandPool, 1, &cmd);
            vkDestroyBuffer(mVkData->device, staging, nullptr);
            vkFreeMemory(mVkData->device, stagingMem, nullptr);

            // Image view
            VkImageViewCreateInfo ivci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            ivci.image    = mWhiteImage;
            ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
            ivci.format   = VK_FORMAT_R8G8B8A8_UNORM;
            ivci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            vkCreateImageView(mVkData->device, &ivci, nullptr, &mWhiteView);

            // Descriptor set for white texture
            VkDescriptorSetAllocateInfo dsai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            dsai.descriptorPool     = mDescPool;
            dsai.descriptorSetCount = 1;
            dsai.pSetLayouts        = &mSetLayout;
            vkAllocateDescriptorSets(mVkData->device, &dsai, &mWhiteSet);

            VkDescriptorImageInfo dii{ sSampler, mWhiteView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
            VkWriteDescriptorSet wd{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            wd.dstSet          = mWhiteSet;
            wd.dstBinding      = 0;
            wd.descriptorCount = 1;
            wd.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            wd.pImageInfo      = &dii;
            vkUpdateDescriptorSets(mVkData->device, 1, &wd, 0, nullptr);

            return true;
        }

        // =============================================================================
        void NkVulkanRenderer2D::Shutdown() {
            if (!mIsValid) return;
            vkDeviceWaitIdle(mVkData->device);

            if (mVBMap) vkUnmapMemory(mVkData->device, mVBMem);
            if (mIBMap) vkUnmapMemory(mVkData->device, mIBMem);
            if (mVB) vkDestroyBuffer(mVkData->device, mVB, nullptr);
            if (mIB) vkDestroyBuffer(mVkData->device, mIB, nullptr);
            if (mVBMem) vkFreeMemory(mVkData->device, mVBMem, nullptr);
            if (mIBMem) vkFreeMemory(mVkData->device, mIBMem, nullptr);

            if (mWhiteView)  vkDestroyImageView(mVkData->device, mWhiteView, nullptr);
            if (mWhiteImage) vkDestroyImage(mVkData->device, mWhiteImage, nullptr);
            if (mWhiteMem)   vkFreeMemory(mVkData->device, mWhiteMem, nullptr);

            for (VkPipeline p : {mPipeAlpha, mPipeAdd, mPipeMul, mPipeNone})
                if (p) vkDestroyPipeline(mVkData->device, p, nullptr);

            if (mPipeLayout) vkDestroyPipelineLayout(mVkData->device, mPipeLayout, nullptr);
            if (mSetLayout)  vkDestroyDescriptorSetLayout(mVkData->device, mSetLayout, nullptr);
            if (mDescPool)   vkDestroyDescriptorPool(mVkData->device, mDescPool, nullptr);
            if (sSampler)    { vkDestroySampler(mVkData->device, sSampler, nullptr); sSampler = VK_NULL_HANDLE; }

            mIsValid = false;
            NK_VK2D_LOG("Shutdown");
        }

        // =============================================================================
        void NkVulkanRenderer2D::Clear(const NkColor2D& /*col*/) {
            // BeginFrame already sets the clear color in the render pass descriptor.
            // Nothing extra needed here — the render pass load op handles it.
        }

        // =============================================================================
        void NkVulkanRenderer2D::BeginBackend() {}
        void NkVulkanRenderer2D::EndBackend()   {}

        // =============================================================================
        VkPipeline NkVulkanRenderer2D::GetPipelineForBlend(NkBlendMode mode) {
            switch (mode) {
                case NkBlendMode::NK_ADD:      return mPipeAdd;
                case NkBlendMode::NK_MULTIPLY: return mPipeMul;
                case NkBlendMode::NK_NONE:     return mPipeNone;
                default:                    return mPipeAlpha;
            }
        }

        // =============================================================================
        VkDescriptorSet NkVulkanRenderer2D::GetOrCreateDescSet(const NkTexture* tex) {
            if (!tex || !tex->IsValid()) return mWhiteSet;

            for (const auto& e : mTexDescCache) {
                if (e.texture == tex) return e.set;
            }

            // Allocate a new descriptor set for this texture
            VkDescriptorSetAllocateInfo dsai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            dsai.descriptorPool     = mDescPool;
            dsai.descriptorSetCount = 1;
            dsai.pSetLayouts        = &mSetLayout;
            VkDescriptorSet ds = VK_NULL_HANDLE;
            if (vkAllocateDescriptorSets(mVkData->device, &dsai, &ds) != VK_SUCCESS)
                return mWhiteSet;

            // tex->GetHandle() should be a VkImageView* (set by the Vulkan texture upload path)
            VkImageView view = tex->GetHandle() ? *static_cast<VkImageView*>(tex->GetHandle())
                                                : mWhiteView;

            VkDescriptorImageInfo dii{ sSampler, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
            VkWriteDescriptorSet wd{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            wd.dstSet          = ds;
            wd.dstBinding      = 0;
            wd.descriptorCount = 1;
            wd.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            wd.pImageInfo      = &dii;
            vkUpdateDescriptorSets(mVkData->device, 1, &wd, 0, nullptr);

            TexDescEntry entry{ tex, ds };
            mTexDescCache.PushBack(entry);
            return ds;
        }

        // =============================================================================
        void NkVulkanRenderer2D::SubmitBatches(const NkBatchGroup* groups, uint32 groupCount,
                                                const NkVertex2D* verts, uint32 vCount,
                                                const uint32*     idx,   uint32 iCount) {
            if (!mIsValid || !vCount || !iCount) return;

            VkCommandBuffer cmd = NkNativeContext::GetVkCurrentCommandBuffer(mCtx);
            if (!cmd) return;

            // Upload to persistently mapped buffers
            memcpy(mVBMap, verts, vCount * sizeof(NkVertex2D));
            memcpy(mIBMap, idx,   iCount * sizeof(uint32));

            // Viewport + scissor
            VkViewport vp{
                (float)mViewport.left,  (float)mViewport.top,
                (float)mViewport.width, (float)mViewport.height,
                0.f, 1.f
            };
            VkRect2D scissor{ {mViewport.left, mViewport.top},
                            {(uint32)mViewport.width, (uint32)mViewport.height} };
            vkCmdSetViewport(cmd, 0, 1, &vp);
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, &mVB, offsets);
            vkCmdBindIndexBuffer(cmd, mIB, 0, VK_INDEX_TYPE_UINT32);

            // Push projection constant
            vkCmdPushConstants(cmd, mPipeLayout, VK_SHADER_STAGE_VERTEX_BIT,
                            0, 64, mProjection);

            VkPipeline currentPipe = VK_NULL_HANDLE;
            for (uint32 g = 0; g < groupCount; ++g) {
                const auto& group = groups[g];

                VkPipeline pipe = GetPipelineForBlend(group.blendMode);
                if (pipe != currentPipe) {
                    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
                    currentPipe = pipe;
                }

                VkDescriptorSet ds = GetOrCreateDescSet(group.texture);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        mPipeLayout, 0, 1, &ds, 0, nullptr);

                vkCmdDrawIndexed(cmd, group.indexCount, 1, group.indexStart, 0, 0);
            }
        }

        // =============================================================================
        void NkVulkanRenderer2D::UploadProjection(const float32 proj[16]) {
            memcpy(mProjection, proj, 64);
        }

    } // namespace renderer
} // namespace nkentseu

#endif // NKENTSEU_HAS_VULKAN_HEADERS