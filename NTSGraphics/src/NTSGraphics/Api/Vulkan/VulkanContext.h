//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 5/3/2024 at 12:51:42 PM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __VULKAN_INTERNAL_CONTEXT_H__
#define __VULKAN_INTERNAL_CONTEXT_H__

#pragma once

#include <vulkan/vulkan.hpp>

#include "NTSCore/System.h"
#include "NTSGraphics/Core/Context.h"
#include "NTSGraphics/Core/ShaderInputLayout.h"
#include "NTSGraphics/Platform/WindowInfo.h"

#include <NTSGraphics/Core/Log.h>

#include <functional>
#include <vector>

namespace nkentseu {
    class VulkanContext;

    using RerecreateCallBackFn = std::function<bool(bool)>;
    using CleanUpCallBackFn = RerecreateCallBackFn;
    #define SWAPCHAIN_CALLBACK_FN(method) std::bind(&method, this, STDPH(1))

    struct NKENTSEU_API VkQueueFamilyIndices {
        int32   graphicsIndex = -1;
        int32   presentIndex = -1;
        int32   computeIndex = -1;

        bool    hasGraphicsFamily = false;
        bool    hasPresentFamily = false;
        bool    hasComputeFamily = false;

        bool    IsComplete() { return hasGraphicsFamily && hasPresentFamily; }
        bool    FindQueueFamilies(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface);
    };

    struct NKENTSEU_API VkQueueFamily {
        VkQueueFamilyIndices queueFamilyIndices;
        vk::Queue            graphicsQueue = nullptr;
        vk::Queue            presentQueue = nullptr;
        vk::Queue            computeQueue = nullptr;

        bool GetQeue(vk::Device device);
    };

    struct NKENTSEU_API VulkanDynamicMode {
        vk::PrimitiveTopology primitiveTopology = vk::PrimitiveTopology::eTriangleList;
        vk::FrontFace frontFace = vk::FrontFace::eCounterClockwise;
        vk::CullModeFlags cullMode = vk::CullModeFlagBits::eNone;
        vk::PolygonMode polygoneMode = vk::PolygonMode::eLine;
    };

    struct NKENTSEU_API VkSurfaceSupportDetails {
        vk::SurfaceCapabilitiesKHR        capabilities = {};
        std::vector<vk::SurfaceFormatKHR> formats = {};
        std::vector<vk::PresentModeKHR>   presentModes = {};

        bool QuerySwapChainSupport(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface);
    };

    struct NKENTSEU_API VkBufferInternal {
        bool WriteToBuffer(const void* data, usize size, usize offset);
        bool Destroy(VulkanContext* context);
        bool Mapped(VulkanContext* context, usize size, usize offset = 0, vk::MemoryMapFlags flag = {});
        bool UnMapped(VulkanContext* context);
        bool Flush(VulkanContext* context, usize size, usize offset = 0);

        static int64 FindMemoryType(VulkanContext* context, uint32 typeFilter, vk::MemoryPropertyFlags properties);
        static bool CreateBuffer(VulkanContext* context, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::SharingMode sharingMode, vk::MemoryPropertyFlags properties, vk::Buffer& buffer, vk::DeviceMemory& bufferMemory);
        static bool CopyBuffer(VulkanContext* context, vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size);
        static bool CopyBufferToImage(VulkanContext* context, vk::Buffer srcBuffer, vk::Image dstImage, const maths::Vector2u& size);

        vk::Buffer buffer = nullptr;
        vk::DeviceMemory bufferMemory = nullptr;
        void* mappedData = nullptr;
        vk::DeviceSize size = 0;
    };

    struct NKENTSEU_API VkUniformBufferInternal {
        bool Create(VulkanContext* context, const UniformInputAttribute& uba, vk::BufferUsageFlags usage, uint32 descriptorSetCount, vk::DescriptorType descriptorType);
        bool Destroy(VulkanContext* context);

        bool Binds(VulkanContext* context, void* data, usize size, uint32 instanceIndex);
        bool Bind(VulkanContext* context, void* data, usize size, uint32 index, uint32 instanceIndex);

        std::vector<VkBufferInternal> uniformBuffers;
        std::vector<vk::WriteDescriptorSet> writeDescriptorSets;
        std::vector<vk::DescriptorBufferInfo> descriptorBufferInfos;
        vk::BufferUsageFlags usage;
        UniformInputAttribute uniformInput;
        uint32 dynamicAlignment = 0;
        uint32 range = 0;

        uint32 currentOffset = 0;
        void* dataModel = nullptr;
        uint32 currentIndex = 0;
    };

    class NKENTSEU_API VulkanContext : public Context
    {
        public:
            VulkanContext(WindowInfo* window, const ContextProperties& contextProperties);
            ~VulkanContext();

            bool Initialize(const maths::Vector2f& size, const maths::Vector2f& dpiSize) override;
            bool Deinitialize() override;
            bool IsInitialize() override;

            bool EnableVSync() override;
            bool DisableVSync() override;

            const maths::Vector2f& GetSize() override;
            const maths::Vector2f& GetDpiSize() override;

            const GraphicsInfos& GetGraphicsInfo() override;
            const ContextProperties& GetProperties() override;
            WindowInfo* GetWindowInfo() override;
            bool OnWindowResized(const maths::Vector2f& size, const maths::Vector2f& dpiSize) override;

            bool AddCleanUpCallback(CleanUpCallBackFn func);
            bool RemoveCleanUpCallback(CleanUpCallBackFn func);

            bool AddRecreateCallback(RerecreateCallBackFn func);
            bool RemoveRecreateCallback(RerecreateCallBackFn func);

            bool IsValidContext();
            bool CanUse();

            vk::CommandBuffer GetCommandBuffer(uint32 id);
            vk::CommandBuffer GetCommandBuffer();

            bool Prepare() override;
            bool Present() override;

            uint32 currentImageIndex = 0;
            bool renderThisFrame = false;

            static bool FindSupportedFormat(vk::PhysicalDevice physicalDevice, const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features, vk::Format* format);
            static bool FindDepthFormat(vk::PhysicalDevice physicalDevice, vk::Format* format);
            static uint32 FindMemoryType(vk::PhysicalDevice physicalDevice, uint32 typeFilter, vk::MemoryPropertyFlags properties);

            static vk::CommandBuffer BeginSingleTimeCommands(VulkanContext* context);
            static void EndSingleTimeCommands(VulkanContext* context, vk::CommandBuffer commandBuffer);

            static inline uint64 AlignBufferSize(uint64 size, uint64 alignment)
            {
                return (size + alignment - 1) & ~(alignment - 1);
            }

            vk::PhysicalDeviceProperties GetPhysicalDeviceProperty() {
                return properties;
            }
        private:

            friend struct VkBufferInternal;

            WindowInfo* m_Window;
            maths::Vector2f size;
            maths::Vector2f dpiSize;
            ContextProperties m_ContextProperties;

            bool m_IsInitialize = false;
            bool m_CanUse = false;

            std::vector<RerecreateCallBackFn> m_RecreateList;
            std::vector<CleanUpCallBackFn> m_CleanList;

        public:
            bool CleanupSwapChain();
            bool RecreateSwapChain();

            bool AddRecreateCallback(std::vector<RerecreateCallBackFn>& callbackListe, RerecreateCallBackFn func);
            bool RemoveRecreateCallback(std::vector<RerecreateCallBackFn>& callbackListe, RerecreateCallBackFn func);
            bool CallRecreateCallback(std::vector<RerecreateCallBackFn>& callbackListe);

        private:
            friend class VulkanTexture2DBinding;
            friend class VulkanRenderWindow;
            friend class VulkanRenderTexture;
            friend class VulkanShader;
            friend class VulkanTexture2D;
            friend class VulkanShaderInputLayout;
            friend class VulkanIndexBuffer;
            friend class VulkanUniformBuffer;
            friend struct VkUniformBufferInternal;
            friend struct VulkanVertexBuffer;
            friend struct VulkanIndexBuffer;
            friend struct DescriptorSetInternal;

            // vulkan properties and methode
            std::vector<const char*> instanceExtension = {};
            std::vector<const char*> deviceExtension = {};
            std::vector<const char*> requiredDeviceExtensions = {};
            std::vector<const char*> layers = {};
            bool InitInstanceExtensionAndLayer();
            bool InitDeviceExtension(vk::PhysicalDevice physicalDevice);
            bool FindBestExtensions(const std::vector<vk::ExtensionProperties>& installed, const std::vector<const char*>& wanted, std::vector<const char*>& out);
            bool FindBestLayers(const std::vector<vk::LayerProperties>& installed, const std::vector<const char*>& wanted, std::vector<const char*>& out);

            // debug
            vk::DebugUtilsMessengerEXT          callback = nullptr;
            bool                                minimized = false;
            bool SetupDebugMessenger();

            // vulkan instance
            vk::AllocationCallbacks*            allocator = nullptr;
            vk::Instance                        instance = nullptr;
            bool CreateInstance();
            bool DestroyInstance();

            // surface
            vk::SurfaceKHR                      surface = nullptr;
            bool CreateSurface();
            bool DestroySurface();

            // queue
            VkQueueFamily                       queueFamilly;
            std::vector<vk::PhysicalDevice>     installedPhysicalDevice = {};
            vk::PhysicalDevice                  physicalDevice = nullptr;
            vk::PhysicalDeviceProperties        properties = {};
            vk::PhysicalDeviceMemoryProperties  memoryProperties;
            vk::Device                          device = nullptr;
            PFN_vkCmdSetPolygonModeEXT          cmdSetPolygonModeEXT = nullptr;
            PFN_vkCmdSetColorBlendEnableEXT     cmdSetColorBlendEnableEXT = nullptr;
            PFN_vkCmdSetColorBlendEquationEXT   cmdSetColorBlendEquationEXT = nullptr;
            PFN_vkCmdSetColorWriteMaskEXT       cmdSetColorWriteMaskEXT = nullptr;

            template<typename T>
            T LoadExtenssion(T command, const std::string& name) {
                if (command == nullptr) {
                    T cmd = (T)vkGetDeviceProcAddr(device, name.c_str());

                    if (!cmd) {
                        return nullptr;
                    }
                    return cmd;
                }
                return command;
            }

            bool IsDeviceSuitable(vk::PhysicalDevice device, vk::SurfaceKHR surface, std::vector<const char*>& extension);
            bool GetPhysicalDevice();
            bool CreateLogicalDevice();
            bool DestroyDevice();

            // create swapchain
            vk::PresentModeKHR presentMode = vk::PresentModeKHR::eFifo;
            vk::SwapchainKHR swapchain = nullptr;
            vk::SurfaceFormatKHR surfaceFormat = {};
            std::vector<vk::Image> swapchainImages = {};
            std::vector<vk::ImageView> imageView = {};
            bool CreateSwapchaine(const maths::Vector2u& size);
            vk::PresentModeKHR SelectPresentMode();
            uint32 GetMinImageCountFromPresentMode(vk::PresentModeKHR present_mode);
            bool DestroySwapchaine();

            // command pool
            vk::CommandPool commandPool = nullptr;
            bool CreateCommandPool();
            bool DestroyCommandPool();

            // semaphore
            vk::Semaphore submitSemaphore = nullptr;
            vk::Semaphore aquireSemaphore = nullptr;
            bool CreateSemaphores();
            bool DestroySemaphores();

            // command buffer
            std::vector<vk::CommandBuffer> commandBuffers = {};
            vk::CommandBuffer defaultCommandBuffer = nullptr;
            bool CreateCommandBuffer();
            bool DestroyCommandBuffer();

            // tools
            uint32  currentFrameIndex                     = 0;
            bool isPresent = true;
            bool isAcquiere = false;
            bool CanAquire();

            // dynamic rendering
            //vk::PhysicalDeviceDynamicRenderingFeaturesKHR enabledDynamicRenderingFeaturesKHR{};

            // render pass
            vk::RenderPass renderPass = nullptr;
            bool CreateRenderPass();
            bool DestroyRenderPass();

            friend class DescriptorSetBase;
    };

    using WriteDescriptorSetInfos = std::function<bool(uint32, vk::WriteDescriptorSet*)>;
    class DescriptorSetBase {
    public:

        virtual ~DescriptorSetBase() = default;
        virtual bool SetWriteDescriptorSetInfos(WriteDescriptorSetInfos descriptorWriteInfos);
        virtual bool Create(Memory::Shared<VulkanContext> context, const std::vector<uint32>& bindings, const std::vector<vk::DescriptorSetLayout>& descriptorLayouts, const std::vector<vk::DescriptorPoolSize>& poolSizes, uint32 descriptor_count = 1);
        virtual bool Destroy(Memory::Shared<VulkanContext> context);
        virtual bool Bind(Memory::Shared<VulkanContext> context, vk::PipelineLayout pipelineLayout, const std::string& name, uint32 slot, uint32 set, uint32 imageIndex = 0, std::vector<uint32> dynamicOffsets = {});

    protected:
        std::vector<vk::DescriptorPool> descriptorPools;
        std::vector<vk::DescriptorPoolSize> poolSizes;
        NkUnorderedMap<uint32, std::vector<vk::DescriptorSet>> descriptorSets;
        WriteDescriptorSetInfos descriptorWriteInfos;
        NkUnorderedMap<uint32, uint32> slots;

        virtual bool CreateDescriptorPool(Memory::Shared<VulkanContext> context);
        virtual bool DestroyDescriptorPool(Memory::Shared<VulkanContext> context);
    };

    class TextureDescriptorSet : public DescriptorSetBase {
    public:
        bool SetDataBindings(vk::Sampler sampler, vk::ImageView imageView);
        virtual bool Bind(Memory::Shared<VulkanContext> context, vk::PipelineLayout pipelineLayout, const std::string& name, uint32 slot, uint32 set, uint32 imageIndex = 0, std::vector<uint32> dynamicOffsets = {});

    private:
        vk::DescriptorImageInfo imageInfo{};
    };

    class UniformBufferDescriptorSet : public DescriptorSetBase {
    public:
        bool SetDataBindings(VkUniformBufferInternal* uniforms);

        std::vector<vk::DescriptorBufferInfo> bufferInfos{};
        virtual bool Bind(Memory::Shared<VulkanContext> context, vk::PipelineLayout pipelineLayout, const std::string& name, uint32 slot, uint32 set, uint32 imageIndex = 0, std::vector<uint32> dynamicOffsets = {});
    };

} // namespace nkentseu

#endif    // __NKENTSEU_INTERNA_LCONTEXT_H__