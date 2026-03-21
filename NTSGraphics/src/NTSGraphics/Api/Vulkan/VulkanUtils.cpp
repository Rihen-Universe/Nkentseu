//
// Created by TEUGUIA TADJUIDJE Rodolf S�deris on 2024-05-20 at 09:28:10 AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "pch/ntspch.h"
#include "VulkanUtils.h"
#include <NTSGraphics/Core/Log.h>

namespace nkentseu {
    std::string VulkanStaticDebugInfo::file_call;
    uint32 VulkanStaticDebugInfo::line_call = 0;
    std::string VulkanStaticDebugInfo::methode_call;
    bool VulkanStaticDebugInfo::success = true;

	const char* VulkanConvert::VulkanResultToString(VkResult result) {
        switch (result) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_TIMEOUT: return "VK_TIMEOUT";
        case VK_EVENT_SET: return "VK_EVENT_SET";
        case VK_EVENT_RESET: return "VK_EVENT_RESET";
        case VK_INCOMPLETE: return "VK_INCOMPLETE";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
        case VK_ERROR_UNKNOWN: return "VK_ERROR_UNKNOWN";
        case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
        case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
        case VK_ERROR_FRAGMENTATION: return "VK_ERROR_FRAGMENTATION";
        case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
        case VK_PIPELINE_COMPILE_REQUIRED: return "VK_PIPELINE_COMPILE_REQUIRED";
        case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
        case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
        case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
        case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
        case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
        case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR: return "VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR";
        case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR";
        case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR";
        case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR";
        case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR";
        case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR";
        case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
        case VK_ERROR_NOT_PERMITTED_KHR: return "VK_ERROR_NOT_PERMITTED_KHR";
        case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
        case VK_THREAD_IDLE_KHR: return "VK_THREAD_IDLE_KHR";
        case VK_THREAD_DONE_KHR: return "VK_THREAD_DONE_KHR";
        case VK_OPERATION_DEFERRED_KHR: return "VK_OPERATION_DEFERRED_KHR";
        case VK_OPERATION_NOT_DEFERRED_KHR: return "VK_OPERATION_NOT_DEFERRED_KHR";
        case VK_ERROR_COMPRESSION_EXHAUSTED_EXT: return "VK_ERROR_COMPRESSION_EXHAUSTED_EXT";
        default: return "VK_UNKNOWN_ERROR";
        }
    }

    void VulkanConvert::GetResourceLimits(VkPhysicalDevice physicalDevice) {
        VkPhysicalDeviceProperties deviceProperties = {};
        vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
        VkPhysicalDeviceLimits limits = deviceProperties.limits;

        // Limites minimales des ressources
        uint64 minUniformBufferOffsetAlignment = limits.minUniformBufferOffsetAlignment;
        uint64 minStorageBufferOffsetAlignment = limits.minStorageBufferOffsetAlignment;
        uint64 minTexelBufferOffsetAlignment = limits.minTexelBufferOffsetAlignment;
        uint64 minTexelOffset = limits.minTexelOffset;
        uint64 minTexelGatherOffset = limits.minTexelGatherOffset;

        // Limites maximales des ressources
        uint64 maxMemoryAllocationCount = limits.maxMemoryAllocationCount;
        uint64 maxSamplerAllocationCount = limits.maxSamplerAllocationCount;
        uint64 maxStorageBufferRange = limits.maxStorageBufferRange;
        uint64 maxPerStageDescriptorStorageBuffers = limits.maxPerStageDescriptorStorageBuffers;
        uint64 maxPerStageDescriptorSampledImages = limits.maxPerStageDescriptorSampledImages;

        // Affichage des limites
        gLog.Info("Limites minimales des ressources :");
        gLog.Info("    {0} : {1}", "minUniformBufferOffsetAlignment", minUniformBufferOffsetAlignment);
        gLog.Info("    {0} : {1}", "minStorageBufferOffsetAlignment", minStorageBufferOffsetAlignment);
        gLog.Info("    {0} : {1}", "minTexelBufferOffsetAlignment", minTexelBufferOffsetAlignment);
        gLog.Info("    {0} : {1}", "minTexelOffset", minTexelOffset);
        gLog.Info("    {0} : {1}", "minTexelGatherOffset", minTexelGatherOffset);

        gLog.Info("Limites maximales des ressources :");
        gLog.Info("    {0} : {1}", "maxMemoryAllocationCount", maxMemoryAllocationCount);
        gLog.Info("    {0} : {1}", "maxSamplerAllocationCount", maxSamplerAllocationCount);
        gLog.Info("    {0} : {1}", "maxStorageBufferRange", maxStorageBufferRange);
        gLog.Info("    {0} : {1}", "maxPerStageDescriptorStorageBuffers", maxPerStageDescriptorStorageBuffers);
        gLog.Info("    {0} : {1}", "maxPerStageDescriptorSampledImages", maxPerStageDescriptorSampledImages);
    }

    VkShaderStageFlagBits VulkanConvert::GetshaderStageType(ShaderStage shaderStage) {
        if (shaderStage.HasFlag(ShaderStage::Enum::Vertex)) return VK_SHADER_STAGE_VERTEX_BIT;
        if (shaderStage.HasFlag(ShaderStage::Enum::Fragment)) return VK_SHADER_STAGE_FRAGMENT_BIT;
        if (shaderStage.HasFlag(ShaderStage::Enum::Geometry)) return VK_SHADER_STAGE_GEOMETRY_BIT;
        if (shaderStage.HasFlag(ShaderStage::Enum::TesControl)) return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        if (shaderStage.HasFlag(ShaderStage::Enum::TesEvaluation)) return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        if (shaderStage.HasFlag(ShaderStage::Enum::Compute)) return VK_SHADER_STAGE_COMPUTE_BIT;

        return VK_SHADER_STAGE_VERTEX_BIT;
    }

    vk::ShaderStageFlagBits VulkanConvert::GetshaderStageType2(ShaderStage shaderStage) {
        if (shaderStage.HasFlag(ShaderStage::Enum::Vertex)) return vk::ShaderStageFlagBits::eVertex;
        if (shaderStage.HasFlag(ShaderStage::Enum::Fragment)) return vk::ShaderStageFlagBits::eFragment;
        if (shaderStage.HasFlag(ShaderStage::Enum::Geometry)) return vk::ShaderStageFlagBits::eGeometry;
        if (shaderStage.HasFlag(ShaderStage::Enum::TesControl)) return vk::ShaderStageFlagBits::eTessellationControl;
        if (shaderStage.HasFlag(ShaderStage::Enum::TesEvaluation)) return vk::ShaderStageFlagBits::eTessellationEvaluation;
        if (shaderStage.HasFlag(ShaderStage::Enum::Compute)) return vk::ShaderStageFlagBits::eCompute;

        return vk::ShaderStageFlagBits::eVertex;
    }

    vk::CullModeFlags VulkanConvert::ToCullModeType(CullModeType mode) {
        if (mode == CullModeType::Enum::Front) return (vk::CullModeFlags)VK_CULL_MODE_FRONT_BIT;
        if (mode == CullModeType::Enum::Back) return (vk::CullModeFlags)VK_CULL_MODE_BACK_BIT;
        if (mode == CullModeType::Enum::FrontBack) return (vk::CullModeFlags)VK_CULL_MODE_FRONT_AND_BACK;
        return (vk::CullModeFlags)VK_CULL_MODE_NONE;
    }

    vk::PolygonMode VulkanConvert::ToPolygonModeType(PolygonModeType contentMode) {
        if (contentMode == PolygonModeType::Enum::Line) return (vk::PolygonMode)VK_POLYGON_MODE_LINE;
        if (contentMode == PolygonModeType::Enum::Fill) return (vk::PolygonMode)VK_POLYGON_MODE_FILL;
        if (contentMode == PolygonModeType::Enum::Point) return (vk::PolygonMode)VK_POLYGON_MODE_POINT;
        if (contentMode == PolygonModeType::Enum::FillRectangle) return (vk::PolygonMode)VK_POLYGON_MODE_FILL_RECTANGLE_NV;
        return (vk::PolygonMode)VK_POLYGON_MODE_FILL;
    }

    vk::FrontFace VulkanConvert::ToFrontFaceType(FrontFaceType mode)
    {
        if (mode == FrontFaceType::Enum::Clockwise) return (vk::FrontFace)VK_FRONT_FACE_CLOCKWISE;
        if (mode == FrontFaceType::Enum::CounterClockwise) return (vk::FrontFace)VK_FRONT_FACE_COUNTER_CLOCKWISE;
        return (vk::FrontFace)VK_FRONT_FACE_CLOCKWISE;
    }

    vk::PrimitiveTopology VulkanConvert::GetPrimitiveType(RenderPrimitive primitive)
    {
        if (primitive == RenderPrimitive::Enum::Points) return (vk::PrimitiveTopology)VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        if (primitive == RenderPrimitive::Enum::Lines) return (vk::PrimitiveTopology)VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        if (primitive == RenderPrimitive::Enum::LineStrip) return (vk::PrimitiveTopology)VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        if (primitive == RenderPrimitive::Enum::Triangles) return (vk::PrimitiveTopology)VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        if (primitive == RenderPrimitive::Enum::TriangleStrip) return (vk::PrimitiveTopology)VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        if (primitive == RenderPrimitive::Enum::TriangleFan) return (vk::PrimitiveTopology)VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
        return (vk::PrimitiveTopology)VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }

    bool VulkanConvert::EndsWith(const std::string& s, const std::string& part) {
        if (s.size() >= part.size()) {
            return (s.compare(s.size() - part.size(), part.size(), part) == 0);
        }
        return false;
    }

    VkFormat VulkanConvert::ShaderInternalToVkFormat(ShaderInternalType shaderType)
    {
        if (shaderType == ShaderInternalType::Enum::Boolean) return VK_FORMAT_R32_UINT;
        if (shaderType == ShaderInternalType::Enum::Float) return VK_FORMAT_R32_SFLOAT;
        if (shaderType == ShaderInternalType::Enum::Float2) return VK_FORMAT_R32G32_SFLOAT;
        if (shaderType == ShaderInternalType::Enum::Float3) return VK_FORMAT_R32G32B32_SFLOAT;
        if (shaderType == ShaderInternalType::Enum::Float4) return VK_FORMAT_R32G32B32A32_SFLOAT;
        if (shaderType == ShaderInternalType::Enum::Int) return VK_FORMAT_R32_SINT;
        if (shaderType == ShaderInternalType::Enum::Int2) return VK_FORMAT_R32G32_SINT;
        if (shaderType == ShaderInternalType::Enum::Int3) return VK_FORMAT_R32G32B32_SINT;
        if (shaderType == ShaderInternalType::Enum::Int4) return VK_FORMAT_R32G32B32A32_SINT;
        //if (shaderType == ShaderInternalType::Enum::UInt): return VK_FORMAT_R32_UINT;
        //if (shaderType == ShaderInternalType::Enum::UInt2): return VK_FORMAT_R32G32_UINT;
        //if (shaderType == ShaderInternalType::Enum::UInt3): return VK_FORMAT_R32G32B32_UINT;
        //if (shaderType == ShaderInternalType::Enum::UInt4): return VK_FORMAT_R32G32B32A32_UINT;
        if (shaderType == ShaderInternalType::Enum::Byte) return VK_FORMAT_R8_UNORM;
        if (shaderType == ShaderInternalType::Enum::Byte2) return VK_FORMAT_R8G8_UNORM;
        if (shaderType == ShaderInternalType::Enum::Byte3) return VK_FORMAT_R8G8B8_UNORM;
        if (shaderType == ShaderInternalType::Enum::Byte4) return VK_FORMAT_R8G8B8A8_UNORM;
        if (shaderType == ShaderInternalType::Enum::Mat2) return VK_FORMAT_R32G32_SFLOAT;
        if (shaderType == ShaderInternalType::Enum::Mat3) return VK_FORMAT_R32G32B32_SFLOAT;
        if (shaderType == ShaderInternalType::Enum::Mat4) return VK_FORMAT_R32G32B32A32_SFLOAT;

        return VK_FORMAT_UNDEFINED;
    }

    vk::Format VulkanConvert::ShaderInternalToVkFormat2(ShaderInternalType shaderType)
    {
        if (shaderType == ShaderInternalType::Enum::Boolean) return (vk::Format)VK_FORMAT_R32_UINT;
        if (shaderType == ShaderInternalType::Enum::Float) return (vk::Format)VK_FORMAT_R32_SFLOAT;
        if (shaderType == ShaderInternalType::Enum::Float2) return (vk::Format)VK_FORMAT_R32G32_SFLOAT;
        if (shaderType == ShaderInternalType::Enum::Float3) return (vk::Format)VK_FORMAT_R32G32B32_SFLOAT;
        if (shaderType == ShaderInternalType::Enum::Float4) return (vk::Format)VK_FORMAT_R32G32B32A32_SFLOAT;
        if (shaderType == ShaderInternalType::Enum::Int) return (vk::Format)VK_FORMAT_R32_SINT;
        if (shaderType == ShaderInternalType::Enum::Int2) return (vk::Format)VK_FORMAT_R32G32_SINT;
        if (shaderType == ShaderInternalType::Enum::Int3) return (vk::Format)VK_FORMAT_R32G32B32_SINT;
        if (shaderType == ShaderInternalType::Enum::Int4) return (vk::Format)VK_FORMAT_R32G32B32A32_SINT;
        //if (shaderType == ShaderInternalType::Enum::UInt): return (vk::Format) VK_FORMAT_R32_UINT;
        //if (shaderType == ShaderInternalType::Enum::UInt2): return (vk::Format) VK_FORMAT_R32G32_UINT;
        //if (shaderType == ShaderInternalType::Enum::UInt3): return (vk::Format) VK_FORMAT_R32G32B32_UINT;
        //if (shaderType == ShaderInternalType::Enum::UInt4): return (vk::Format) VK_FORMAT_R32G32B32A32_UINT;
        if (shaderType == ShaderInternalType::Enum::Byte) return (vk::Format)VK_FORMAT_R8_UNORM;
        if (shaderType == ShaderInternalType::Enum::Byte2) return (vk::Format)VK_FORMAT_R8G8_UNORM;
        if (shaderType == ShaderInternalType::Enum::Byte3) return (vk::Format)VK_FORMAT_R8G8B8_UNORM;
        if (shaderType == ShaderInternalType::Enum::Byte4) return (vk::Format)VK_FORMAT_R8G8B8A8_UNORM;
        if (shaderType == ShaderInternalType::Enum::Mat2) return (vk::Format)VK_FORMAT_R32G32_SFLOAT;
        if (shaderType == ShaderInternalType::Enum::Mat3) return (vk::Format)VK_FORMAT_R32G32B32_SFLOAT;
        if (shaderType == ShaderInternalType::Enum::Mat4) return (vk::Format)VK_FORMAT_R32G32B32A32_SFLOAT;

        return (vk::Format)VK_FORMAT_UNDEFINED;
    }

    vk::Format VulkanConvert::ToTextureFormat(TextureFormat format)
    {
        if (format == TextureFormat::Enum::RGBA8) return (vk::Format)VK_FORMAT_R8G8B8A8_UNORM;
        if (format == TextureFormat::Enum::RGB8) return (vk::Format)VK_FORMAT_R8G8B8_UNORM;
        if (format == TextureFormat::Enum::SRGB8_ALPHA8) return (vk::Format)VK_FORMAT_R8G8B8A8_SRGB;
        if (format == TextureFormat::Enum::RED_INTEGER) return (vk::Format)VK_FORMAT_R8_UINT;
        if (format == TextureFormat::Enum::DEPTH_COMPONENT16) return (vk::Format)VK_FORMAT_D16_UNORM;
        if (format == TextureFormat::Enum::DEPTH_COMPONENT24) return (vk::Format)VK_FORMAT_X8_D24_UNORM_PACK32;
        if (format == TextureFormat::Enum::DEPTH_COMPONENT32F) return (vk::Format)VK_FORMAT_D32_SFLOAT;
        if (format == TextureFormat::Enum::STENCIL_INDEX8) return (vk::Format)VK_FORMAT_S8_UINT;
        if (format == TextureFormat::Enum::DEPTH24_STENCIL8) return (vk::Format)VK_FORMAT_D24_UNORM_S8_UINT;
        if (format == TextureFormat::Enum::DEPTH32F_STENCIL8) return (vk::Format)VK_FORMAT_D32_SFLOAT_S8_UINT;
        return (vk::Format)VK_FORMAT_UNDEFINED;
    }

    vk::DescriptorType VulkanConvert::SamplerInputAttributType(SamplerType sampler)
    {
        if (sampler == SamplerType::Enum::CombineImage) {
            return vk::DescriptorType::eCombinedImageSampler;
        }
        else if (sampler == SamplerType::Enum::SamplerImage) {
            return vk::DescriptorType::eSampledImage;
        }
        else if (sampler == SamplerType::Enum::StorageImage) {
            return vk::DescriptorType::eStorageImage;
        }
        return vk::DescriptorType::eCombinedImageSampler;
    }

    vk::DescriptorType VulkanConvert::BufferUsageAttributType(BufferUsageType usage)
    {
        if (usage == BufferUsageType::Enum::StaticDraw) {
            return vk::DescriptorType::eUniformBuffer;
        }
        else if (usage == BufferUsageType::Enum::DynamicDraw) {
            return vk::DescriptorType::eUniformBufferDynamic;
        }
        else if (usage == BufferUsageType::Enum::StreamDraw) {
            return vk::DescriptorType::eStorageBuffer;
        }
        return vk::DescriptorType::eUniformBuffer;
    }

    vk::ShaderStageFlagBits VulkanConvert::ShaderStageToVkShaderStage(ShaderStage shaderStage) {

        if (shaderStage.HasFlag(ShaderStage::Enum::Vertex)) return vk::ShaderStageFlagBits::eVertex;
        if (shaderStage.HasFlag(ShaderStage::Enum::Fragment)) return vk::ShaderStageFlagBits::eFragment;
        if (shaderStage.HasFlag(ShaderStage::Enum::Geometry)) return vk::ShaderStageFlagBits::eGeometry;
        if (shaderStage.HasFlag(ShaderStage::Enum::TesControl)) return vk::ShaderStageFlagBits::eTessellationControl;
        if (shaderStage.HasFlag(ShaderStage::Enum::TesEvaluation)) return vk::ShaderStageFlagBits::eTessellationEvaluation;
        if (shaderStage.HasFlag(ShaderStage::Enum::Compute)) return vk::ShaderStageFlagBits::eCompute;

        return vk::ShaderStageFlagBits::eVertex;
    }

}  //  nkentseu