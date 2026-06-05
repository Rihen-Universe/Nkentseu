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

#include "NKCanvas/Renderer/Resources/NkTexture.h"
#include "NKCanvas/Renderer/Resources/NkTextureBackend.h"
#include "NKCanvas/Renderer/Resources/NkShaderBackend.h"
#include "NKCanvas/Renderer/Targets/NkRenderTextureBackend.h"
#include "NKCanvas/Core/NkNativeContextAccess.h"
#include "NkVulkanShaderCompiler.h"
#include "NkRenderer2DVkSpv.inl"
#include "NKLogger/NkLog.h"
#include "NKContainers/Sequential/NkVector.h"
#include <cstring>

#define NK_VK2D_LOG(...) logger.Infof("[NkVk2D] " __VA_ARGS__)
#define NK_VK2D_ERR(...) logger.Errorf("[NkVk2D] " __VA_ARGS__)
#define NK_VK2D_CHECK(r) do { VkResult _r=(r); if(_r!=VK_SUCCESS){ NK_VK2D_ERR(#r " = %d",(int)_r); return false; } } while(0)

namespace nkentseu {
    namespace renderer {

        VkSampler NkVulkanRenderer2D::sSampler = VK_NULL_HANDLE;

        // =============================================================================
        // Registry globale des textures Vulkan crees via NkTextureBackend.
        //
        // Les callbacks publies dans NkTextureBackend sont statiques (signature
        // (uint32,uint32,const uint8*) -> uint32 etc.) — ils n'ont pas de this.
        // Pour exposer le VkDevice/VkQueue/VkCommandPool/Sampler aux callbacks,
        // on capture ces handles a la fin de Initialize() dans cette registry
        // globale ; chaque texture est conservee dans gVkTexRegistry.entries
        // a l'index (id - 1). Id 0 reste reserve "invalide".
        //
        // Note : pas de mutex — NKCanvas (comme OpenGL) attend que les ops GPU
        // se fassent sur le thread qui possede le contexte. Si un usage thread-
        // pool venait a apparaitre, ajouter un NkSpinLock autour de toutes les
        // operations sur entries (Push/Erase/lookup).
        // =============================================================================
        struct NkVulkanTextureEntry {
            uint32         width   = 0;
            uint32         height  = 0;
            VkImage        image   = VK_NULL_HANDLE;
            VkDeviceMemory memory  = VK_NULL_HANDLE;
            VkImageView    view    = VK_NULL_HANDLE;
        };

        struct NkVulkanTextureRegistry {
            VkDevice         device         = VK_NULL_HANDLE;
            VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
            VkQueue          graphicsQueue  = VK_NULL_HANDLE;
            VkCommandPool    commandPool    = VK_NULL_HANDLE;

            // Slot 0 reste intentionnellement vide pour que les ID > 0 = valides.
            // Les slots liberes par Delete sont marques image==NULL_HANDLE puis
            // reutilises par le prochain Create.
            NkVector<NkVulkanTextureEntry> entries;
        };

        static NkVulkanTextureRegistry gVkTexRegistry;

        // ── Helpers internes (anonymes au TU) ─────────────────────────────────────
        namespace {

            bool VkTexFindMemoryType(VkPhysicalDevice phys,
                                     uint32 filter,
                                     VkMemoryPropertyFlags props,
                                     uint32& out)
            {
                VkPhysicalDeviceMemoryProperties mp;
                vkGetPhysicalDeviceMemoryProperties(phys, &mp);
                for (uint32 i = 0; i < mp.memoryTypeCount; ++i) {
                    if ((filter & (1u << i)) &&
                        (mp.memoryTypes[i].propertyFlags & props) == props) {
                        out = i;
                        return true;
                    }
                }
                return false;
            }

            // Mapping NkTextureFilter/NkTextureWrap -> Vk : disponibles si on
            // bascule un jour sur un sampler par texture (sSampler est immutable
            // dans cette version, voir SetVulkanTextureFilter/Wrap).
            //   NkTextureFilter::NK_NEAREST -> VK_FILTER_NEAREST (sinon LINEAR)
            //   NkTextureWrap::NK_REPEAT/MIRROR_REPEAT/CLAMP
            //   -> VK_SAMPLER_ADDRESS_MODE_REPEAT/MIRRORED_REPEAT/CLAMP_TO_EDGE

            // Upload sync : un-shot command buffer, queueSubmit + queueWaitIdle.
            // Pas optimal (stall complet), mais simple et correct pour A.7.
            // A revisiter avec un transfer queue + fences si on passe en hot path.
            bool VkTexUploadPixels(VkImage           image,
                                   uint32            dstX,
                                   uint32            dstY,
                                   uint32            width,
                                   uint32            height,
                                   const uint8*      rgba,
                                   bool              transitionFromUndefined)
            {
                VkDevice         dev   = gVkTexRegistry.device;
                VkPhysicalDevice phys  = gVkTexRegistry.physicalDevice;
                VkQueue          queue = gVkTexRegistry.graphicsQueue;
                VkCommandPool    pool  = gVkTexRegistry.commandPool;
                if (!dev || !phys || !queue || !pool || !image || !rgba || !width || !height)
                    return false;

                const VkDeviceSize byteCount = (VkDeviceSize)width * height * 4;

                // ── Staging buffer host-visible ──────────────────────────────────
                VkBuffer       staging    = VK_NULL_HANDLE;
                VkDeviceMemory stagingMem = VK_NULL_HANDLE;
                {
                    VkBufferCreateInfo bi{};
                    bi.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
                    bi.size        = byteCount;
                    bi.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
                    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
                    if (vkCreateBuffer(dev, &bi, nullptr, &staging) != VK_SUCCESS)
                        return false;

                    VkMemoryRequirements req;
                    vkGetBufferMemoryRequirements(dev, staging, &req);
                    uint32 memType = 0;
                    if (!VkTexFindMemoryType(phys, req.memoryTypeBits,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, memType)) {
                        vkDestroyBuffer(dev, staging, nullptr);
                        return false;
                    }
                    VkMemoryAllocateInfo ai{};
                    ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                    ai.allocationSize  = req.size;
                    ai.memoryTypeIndex = memType;
                    if (vkAllocateMemory(dev, &ai, nullptr, &stagingMem) != VK_SUCCESS) {
                        vkDestroyBuffer(dev, staging, nullptr);
                        return false;
                    }
                    vkBindBufferMemory(dev, staging, stagingMem, 0);
                }

                // Copy RGBA into staging
                void* mapped = nullptr;
                vkMapMemory(dev, stagingMem, 0, byteCount, 0, &mapped);
                memcpy(mapped, rgba, (size_t)byteCount);
                vkUnmapMemory(dev, stagingMem);

                // ── One-shot command buffer ─────────────────────────────────────
                VkCommandBufferAllocateInfo cbai{};
                cbai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                cbai.commandPool        = pool;
                cbai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                cbai.commandBufferCount = 1;
                VkCommandBuffer cmd = VK_NULL_HANDLE;
                if (vkAllocateCommandBuffers(dev, &cbai, &cmd) != VK_SUCCESS) {
                    vkDestroyBuffer(dev, staging, nullptr);
                    vkFreeMemory(dev, stagingMem, nullptr);
                    return false;
                }
                VkCommandBufferBeginInfo cbbi{};
                cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                vkBeginCommandBuffer(cmd, &cbbi);

                // src -> TRANSFER_DST.
                // Create path : UNDEFINED -> TRANSFER_DST.
                // Update path : SHADER_READ_ONLY -> TRANSFER_DST.
                VkImageMemoryBarrier b1{};
                b1.sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                b1.oldLayout     = transitionFromUndefined
                                    ? VK_IMAGE_LAYOUT_UNDEFINED
                                    : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                b1.newLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                b1.srcAccessMask = transitionFromUndefined ? 0 : VK_ACCESS_SHADER_READ_BIT;
                b1.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                b1.image         = image;
                b1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                b1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                b1.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
                vkCmdPipelineBarrier(cmd,
                    transitionFromUndefined
                        ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
                        : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    0, 0, nullptr, 0, nullptr, 1, &b1);

                VkBufferImageCopy region{};
                region.bufferOffset      = 0;
                region.bufferRowLength   = 0;
                region.bufferImageHeight = 0;
                region.imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
                region.imageOffset       = {(int32)dstX, (int32)dstY, 0};
                region.imageExtent       = {width, height, 1};
                vkCmdCopyBufferToImage(cmd, staging, image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

                // TRANSFER_DST -> SHADER_READ_ONLY (etat utilise par GetOrCreateDescSet)
                VkImageMemoryBarrier b2 = b1;
                b2.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                b2.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                b2.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                b2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                vkCmdPipelineBarrier(cmd,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    0, 0, nullptr, 0, nullptr, 1, &b2);

                vkEndCommandBuffer(cmd);

                VkSubmitInfo si{};
                si.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                si.commandBufferCount = 1;
                si.pCommandBuffers    = &cmd;
                vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE);
                vkQueueWaitIdle(queue);   // simple ; remplacer par VkFence si hot path

                vkFreeCommandBuffers(dev, pool, 1, &cmd);
                vkDestroyBuffer(dev, staging, nullptr);
                vkFreeMemory(dev, stagingMem, nullptr);
                return true;
            }

            // Libere tout ce qui est dans un slot (image, view, memory) puis le
            // marque vide (handles VK_NULL_HANDLE) pour reutilisation.
            void VkTexFreeEntry(NkVulkanTextureEntry& e) {
                VkDevice dev = gVkTexRegistry.device;
                if (!dev) return;
                if (e.view)   vkDestroyImageView(dev, e.view,   nullptr);
                if (e.image)  vkDestroyImage    (dev, e.image,  nullptr);
                if (e.memory) vkFreeMemory      (dev, e.memory, nullptr);
                e.view   = VK_NULL_HANDLE;
                e.image  = VK_NULL_HANDLE;
                e.memory = VK_NULL_HANDLE;
                e.width  = 0;
                e.height = 0;
            }

        } // namespace anonyme

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
        // ── NkTextureBackend dispatch — implementations statiques ──────────────────
        // =============================================================================

        uint32 NkVulkanRenderer2D::CreateVulkanTexture(uint32 w, uint32 h, const uint8* rgba) {
            VkDevice         dev   = gVkTexRegistry.device;
            VkPhysicalDevice phys  = gVkTexRegistry.physicalDevice;
            if (!dev || !phys || !w || !h) return 0;

            // ── VkImage device-local (sampling + transfert dst) ──────────────────
            VkImageCreateInfo ici{};
            ici.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            ici.imageType     = VK_IMAGE_TYPE_2D;
            ici.format        = VK_FORMAT_R8G8B8A8_UNORM;
            ici.extent        = {w, h, 1};
            ici.mipLevels     = 1;
            ici.arrayLayers   = 1;
            ici.samples       = VK_SAMPLE_COUNT_1_BIT;
            ici.tiling        = VK_IMAGE_TILING_OPTIMAL;
            ici.usage         = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            ici.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
            ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            NkVulkanTextureEntry entry{};
            entry.width  = w;
            entry.height = h;

            if (vkCreateImage(dev, &ici, nullptr, &entry.image) != VK_SUCCESS)
                return 0;

            VkMemoryRequirements req;
            vkGetImageMemoryRequirements(dev, entry.image, &req);
            uint32 memType = 0;
            if (!VkTexFindMemoryType(phys, req.memoryTypeBits,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memType)) {
                vkDestroyImage(dev, entry.image, nullptr);
                return 0;
            }
            VkMemoryAllocateInfo mai{};
            mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            mai.allocationSize  = req.size;
            mai.memoryTypeIndex = memType;
            if (vkAllocateMemory(dev, &mai, nullptr, &entry.memory) != VK_SUCCESS) {
                vkDestroyImage(dev, entry.image, nullptr);
                return 0;
            }
            vkBindImageMemory(dev, entry.image, entry.memory, 0);

            // ── Upload des pixels (UNDEFINED -> TRANSFER_DST -> SHADER_READ_ONLY) ─
            if (rgba) {
                if (!VkTexUploadPixels(entry.image, 0, 0, w, h, rgba, true)) {
                    vkDestroyImage(dev, entry.image, nullptr);
                    vkFreeMemory  (dev, entry.memory, nullptr);
                    return 0;
                }
            } else {
                // Pas de pixels : on transitionne quand meme vers SHADER_READ_ONLY
                // pour que le descriptor set soit valide a l'usage.
                VkCommandBufferAllocateInfo cbai{};
                cbai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                cbai.commandPool        = gVkTexRegistry.commandPool;
                cbai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                cbai.commandBufferCount = 1;
                VkCommandBuffer cmd = VK_NULL_HANDLE;
                vkAllocateCommandBuffers(dev, &cbai, &cmd);
                VkCommandBufferBeginInfo cbbi{};
                cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                vkBeginCommandBuffer(cmd, &cbbi);
                VkImageMemoryBarrier b{};
                b.sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                b.oldLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
                b.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                b.srcAccessMask = 0;
                b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                b.image         = entry.image;
                b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
                b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                vkCmdPipelineBarrier(cmd,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    0, 0, nullptr, 0, nullptr, 1, &b);
                vkEndCommandBuffer(cmd);
                VkSubmitInfo si{};
                si.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                si.commandBufferCount = 1;
                si.pCommandBuffers    = &cmd;
                vkQueueSubmit(gVkTexRegistry.graphicsQueue, 1, &si, VK_NULL_HANDLE);
                vkQueueWaitIdle(gVkTexRegistry.graphicsQueue);
                vkFreeCommandBuffers(dev, gVkTexRegistry.commandPool, 1, &cmd);
            }

            // ── VkImageView pour sampling ────────────────────────────────────────
            VkImageViewCreateInfo ivci{};
            ivci.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            ivci.image    = entry.image;
            ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
            ivci.format   = VK_FORMAT_R8G8B8A8_UNORM;
            ivci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            if (vkCreateImageView(dev, &ivci, nullptr, &entry.view) != VK_SUCCESS) {
                vkDestroyImage(dev, entry.image, nullptr);
                vkFreeMemory  (dev, entry.memory, nullptr);
                return 0;
            }

            // ── Insertion dans la registry (reutilise un slot libere si possible) ─
            auto& entries = gVkTexRegistry.entries;
            // Slot 0 reste reserve "invalide" — l'initialiser au 1er appel.
            if (entries.Empty()) entries.PushBack(NkVulkanTextureEntry{});
            for (uint32 i = 1; i < (uint32)entries.Size(); ++i) {
                if (entries[i].image == VK_NULL_HANDLE) {
                    entries[i] = entry;
                    return i;       // ID = index ; 0 invalide
                }
            }
            entries.PushBack(entry);
            return (uint32)entries.Size() - 1;
        }

        void NkVulkanRenderer2D::UpdateVulkanTexture(uint32 id, uint32 x, uint32 y,
                                                    uint32 w, uint32 h, const uint8* rgba) {
            if (!id || !rgba || !w || !h) return;
            auto& entries = gVkTexRegistry.entries;
            if ((size_t)id >= entries.Size()) return;
            NkVulkanTextureEntry& e = entries[id];
            if (e.image == VK_NULL_HANDLE) return;
            // Bornes — la sub-region doit tenir dans l'image existante.
            if (x + w > e.width || y + h > e.height) return;
            VkTexUploadPixels(e.image, x, y, w, h, rgba, false);
        }

        void NkVulkanRenderer2D::DeleteVulkanTexture(uint32 id) {
            if (!id) return;
            auto& entries = gVkTexRegistry.entries;
            if ((size_t)id >= entries.Size()) return;
            if (!gVkTexRegistry.device) return;
            // vkDeviceWaitIdle serait plus sur mais bloque toute la GPU.
            // Pour A.7 on suppose que la texture n'est plus referencee par un
            // descriptor set lie a un command buffer en cours d'execution
            // (NkTexture::Destroy est appele en dehors d'un frame submit).
            VkTexFreeEntry(entries[id]);
        }

        void NkVulkanRenderer2D::SetVulkanTextureFilter(uint32 id, NkTextureFilter f) {
            // sSampler est immutable (declare immutable dans CreateDescriptorSetLayout)
            // et partage entre toutes les textures. Modifier le filter par-texture
            // exigerait : (1) sampler non-immutable, (2) un sampler cache par
            // (filter,wrap), (3) recreation des descriptor sets concernes.
            //
            // Pour A.7 on conserve mFilter cote NkTexture (utile en lecture) et on
            // log si l'API est appelee avec une valeur non-LINEAR — comportement
            // attendu pour le scope actuel (toutes les textures = LINEAR/CLAMP).
            (void)id; (void)f;
            // Pas de log spammy : l'appel arrive a chaque SetFilter() utilisateur.
        }

        void NkVulkanRenderer2D::SetVulkanTextureWrap(uint32 id, NkTextureWrap w) {
            // Cf SetVulkanTextureFilter : sSampler immutable, donc no-op.
            (void)id; (void)w;
        }

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

            // ── Enregistrement dispatch table NkTexture (cf NkTextureBackend.h) ──
            // Capture des handles Vulkan dans la registry globale pour que les
            // callbacks statiques puissent operer sans this. Une seule instance
            // active a la fois est assumee (sinon Initialize ulterieur ecrase).
            gVkTexRegistry.device         = mVkData->device;
            gVkTexRegistry.physicalDevice = mVkData->physicalDevice;
            gVkTexRegistry.graphicsQueue  = mVkData->graphicsQueue;
            gVkTexRegistry.commandPool    = mVkData->commandPool;
            {
                NkTextureBackend backend{};
                backend.Create    = &NkVulkanRenderer2D::CreateVulkanTexture;
                backend.Update    = &NkVulkanRenderer2D::UpdateVulkanTexture;
                backend.Destroy   = &NkVulkanRenderer2D::DeleteVulkanTexture;
                backend.SetFilter = &NkVulkanRenderer2D::SetVulkanTextureFilter;
                backend.SetWrap   = &NkVulkanRenderer2D::SetVulkanTextureWrap;
                NkTextureSetBackend(backend);
            }

            // NkShader sur Vulkan : un shader user-custom necessite la
            // reconstruction du VkPipeline (shader stages immutables apres
            // creation du pipeline). Implementation differee — stub installe
            // pour preserver la consistance API (NkShader::Compile retourne
            // false sur Vulkan tant que le pipeline-cache user n'est pas la).
            NkShaderInstallUnsupportedBackend("Vulkan");
            NkRenderTextureInstallUnsupportedBackend("Vulkan");

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
        static VkShaderModule MakeModule(VkDevice dev, const uint32* spv, uint32 wordCount, const char* tag) {
            if (!dev || !spv || wordCount == 0) {
                NK_VK2D_ERR("MakeModule[%s] : invalid args (dev=%p spv=%p words=%u)",
                            tag ? tag : "?", (void*)dev, (const void*)spv, wordCount);
                return VK_NULL_HANDLE;
            }
            // Sanity check SPIR-V magic word (0x07230203 little-endian).
            if (spv[0] != 0x07230203u) {
                NK_VK2D_ERR("MakeModule[%s] : SPIR-V magic mismatch (got 0x%08X)", tag ? tag : "?", spv[0]);
                return VK_NULL_HANDLE;
            }
            VkShaderModuleCreateInfo ci{};
            ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            ci.codeSize = wordCount * sizeof(uint32);
            ci.pCode    = spv;
            VkShaderModule mod = VK_NULL_HANDLE;
            const VkResult r = vkCreateShaderModule(dev, &ci, nullptr, &mod);
            if (r != VK_SUCCESS) {
                NK_VK2D_ERR("MakeModule[%s] : vkCreateShaderModule failed VkResult=%d (size=%zu bytes)",
                            tag ? tag : "?", (int)r, (size_t)ci.codeSize);
                return VK_NULL_HANDLE;
            }
            NK_VK2D_LOG("MakeModule[%s] OK (handle=%p size=%zu)", tag ? tag : "?", (void*)mod, (size_t)ci.codeSize);
            return mod;
        }

        bool NkVulkanRenderer2D::CreatePipelines() {
            // ── Résoudre les modules shader ──────────────────────────────────────────
            VkShaderModule vs = VK_NULL_HANDLE;
            VkShaderModule fs = VK_NULL_HANDLE;
        
            if (kVk2DVertSpvWordCount > 0 && kVk2DFragSpvWordCount > 0) {
                // PATH A : SPIR-V pré-compilé (NkRenderer2DVkSpv.inl rempli)
                NK_VK2D_LOG("Using precompiled SPIR-V shaders (vert=%u words, frag=%u words)",
                            kVk2DVertSpvWordCount, kVk2DFragSpvWordCount);
                NK_VK2D_LOG("CreatePipelines : device=%p renderPass=%p pipeLayout=%p",
                            (void*)mVkData->device, (void*)mVkData->renderPass, (void*)mPipeLayout);
                vs = MakeModule(mVkData->device, kVk2DVertSpv, kVk2DVertSpvWordCount, "VERT");
                fs = MakeModule(mVkData->device, kVk2DFragSpv, kVk2DFragSpvWordCount, "FRAG");
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
        
                vs = MakeModule(mVkData->device, vertSpv.Data(), (uint32)vertSpv.Size(), "VERT-runtime");
                fs = MakeModule(mVkData->device, fragSpv.Data(), (uint32)fragSpv.Size(), "FRAG-runtime");
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
                                    VkPipeline& out,
                                    const char* tag) -> bool {
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
        
                NK_VK2D_LOG("MakePipeline[%s] : vkCreateGraphicsPipelines...", tag);
                const VkResult r = vkCreateGraphicsPipelines(
                    mVkData->device, VK_NULL_HANDLE, 1, &gci, nullptr, &out);
                if (r != VK_SUCCESS) {
                    NK_VK2D_ERR("MakePipeline[%s] FAIL VkResult=%d", tag, (int)r);
                    return false;
                }
                NK_VK2D_LOG("MakePipeline[%s] OK (handle=%p)", tag, (void*)out);
                return true;
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
                MakePipeline(blendAlpha, mPipeAlpha, "Alpha") &&
                MakePipeline(blendAdd,   mPipeAdd,   "Add")   &&
                MakePipeline(blendMul,   mPipeMul,   "Mul")   &&
                MakePipeline(blendNone,  mPipeNone,  "None");

            vkDestroyShaderModule(mVkData->device, vs, nullptr);
            vkDestroyShaderModule(mVkData->device, fs, nullptr);

            if (!ok) NK_VK2D_ERR("Pipeline creation failed");
            else     NK_VK2D_LOG("CreatePipelines DONE (4/4 pipelines OK)");
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

            // ── Desenregistrer le dispatch NkTexture + liberer les textures live ─
            // Apres ce point, NkTexture::Destroy devient no-op (Destroy=null) et
            // les ID restants stoques cote NkTexture deviendront orphelins. C'est
            // attendu : on attend que NkTexture soit detruit avant le renderer
            // dans le cas nominal ; le Free ci-dessous protege contre les fuites
            // si l'ordre est inverse.
            {
                NkTextureBackend empty{};
                NkTextureSetBackend(empty);
            }
            for (uint32 i = 1; i < (uint32)gVkTexRegistry.entries.Size(); ++i) {
                VkTexFreeEntry(gVkTexRegistry.entries[i]);
            }
            gVkTexRegistry.entries.Clear();
            gVkTexRegistry.device         = VK_NULL_HANDLE;
            gVkTexRegistry.physicalDevice = VK_NULL_HANDLE;
            gVkTexRegistry.graphicsQueue  = VK_NULL_HANDLE;
            gVkTexRegistry.commandPool    = VK_NULL_HANDLE;

            // ── Invalider le cache de descriptor sets (les VkDescriptorSet sont
            // liberes par vkDestroyDescriptorPool plus bas, donc rien a faire ici
            // a part vider la table pour eviter qu'un futur Initialize ne reutilise
            // des entries pointant vers des textures liberees).
            mTexDescCache.Clear();

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
        // Le cycle de frame Vulkan DOIT etre pilote ici. NkRenderWindow ne fait que
        // Clear/Display + Present ; la base Begin()->BeginBackend() puis
        // End()->Flush()->EndBackend() nous donne le bon ordre (begin renderpass
        // AVANT les draws, end renderpass APRES). Sans ca, le renderpass n'est
        // JAMAIS begun -> GetVkCurrentCommandBuffer() == null -> SubmitBatches skip
        // (aucun draw) + clear jamais applique -> swapchain non initialisee = ECRAN
        // BLANC. (OpenGL rend en immediat, d'ou son fonctionnement sans BeginFrame.)
        // BeginFrame() peut retourner false (minimise/resize) : isAcquire protege
        // alors EndFrame()/Present(), et SubmitBatches skip sur cmd null.
        void NkVulkanRenderer2D::BeginBackend() { if (mCtx) mCtx->BeginFrame(); }
        void NkVulkanRenderer2D::EndBackend()   { if (mCtx) mCtx->EndFrame(); }

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

            // Recupere la VkImageView depuis la registry indexee par mGPUId
            // (renseigne par CreateVulkanTexture via NkTextureBackend). Si la
            // texture n'a pas de GPUId valide on retombe sur la white texture.
            VkImageView view = mWhiteView;
            const uint32 id = tex->GetGPUId();
            if (id && (size_t)id < gVkTexRegistry.entries.Size()) {
                VkImageView v = gVkTexRegistry.entries[id].view;
                if (v != VK_NULL_HANDLE) view = v;
            }

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

            // Viewport + scissor.
            // Y-FLIP Vulkan : le NDC Vulkan a +Y vers le BAS (inverse d'OpenGL).
            // On utilise un viewport a HAUTEUR NEGATIVE (origine en bas) — la facon
            // idiomatique de retourner Y sans toucher la matrice de projection
            // (VK_KHR_maintenance1 / Vulkan 1.1, universel sur GPU modernes). Sans
            // ca, tout le rendu 2D est a l'envers.
            VkViewport vp{
                (float)mViewport.left,
                (float)mViewport.top + (float)mViewport.height,
                (float)mViewport.width,
                -(float)mViewport.height,
                0.f, 1.f
            };
            // Scissor : plein viewport par defaut ; si un clip est actif, on le
            // restreint (intersection avec le viewport — VkRect2D est en pixels,
            // origine haut-gauche, donc pas de flip Y).
            VkRect2D scissor{ {mViewport.left, mViewport.top},
                            {(uint32)mViewport.width, (uint32)mViewport.height} };
            if (mHasClip) {
                const int32 vx1 = mViewport.left + mViewport.width;
                const int32 vy1 = mViewport.top  + mViewport.height;
                int32 x0 = mClipRect.x > mViewport.left ? mClipRect.x : mViewport.left;
                int32 y0 = mClipRect.y > mViewport.top  ? mClipRect.y : mViewport.top;
                int32 x1 = (mClipRect.x + mClipRect.width)  < vx1 ? (mClipRect.x + mClipRect.width)  : vx1;
                int32 y1 = (mClipRect.y + mClipRect.height) < vy1 ? (mClipRect.y + mClipRect.height) : vy1;
                if (x1 < x0) x1 = x0;
                if (y1 < y0) y1 = y0;
                scissor.offset = { x0, y0 };
                scissor.extent = { (uint32)(x1 - x0), (uint32)(y1 - y0) };
            }
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