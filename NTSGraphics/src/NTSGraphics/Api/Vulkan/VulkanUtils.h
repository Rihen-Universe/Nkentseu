//
// Created by TEUGUIA TADJUIDJE Rodolf S�deris on 2024-05-20 at 09:28:10 AM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __VULKAN_UTILS_H__
#define __VULKAN_UTILS_H__

#pragma once

#include <vulkan/vulkan.hpp>
#include <string>

#include <NTSCore/System.h>
#include <NTSLogger/Formatter.h>

#include "NTSGraphics/Core/ShaderInputLayout.h"
#include "NTSGraphics/Core/Log.h"

namespace nkentseu {
    struct VulkanResult {
        bool success = false;
        vk::Result result;
    };

    struct VulkanStaticDebugInfo {
        static std::string file_call;
        static uint32 line_call;
        static std::string methode_call;

        static void SetInfo(const std::string& file, uint32 line, const std::string& method) {
            file_call = file;
            line_call = line;
            methode_call = method;
            //success = false;
        }

        static bool Result() {
            bool result = success;
            success = true;
            return result;
        }

        static void Result(bool result) {
            success = result;
        }
    private:
        static bool success;
    };

    class NKENTSEU_API VulkanConvert {
    public:
        static const char* VulkanResultToString(VkResult result);
        static void GetResourceLimits(VkPhysicalDevice physicalDevice);
        static VkShaderStageFlagBits GetshaderStageType(ShaderStage type);
        static vk::ShaderStageFlagBits GetshaderStageType2(ShaderStage shaderStage);

        static vk::CullModeFlags ToCullModeType(CullModeType mode);
        static vk::PolygonMode ToPolygonModeType(PolygonModeType mode);
        static vk::FrontFace ToFrontFaceType(FrontFaceType mode);
        static vk::PrimitiveTopology GetPrimitiveType(RenderPrimitive primitive);


        static bool EndsWith(const std::string& s, const std::string& part);

        static VkFormat ShaderInternalToVkFormat(ShaderInternalType shaderType);
        static vk::Format ShaderInternalToVkFormat2(ShaderInternalType shaderType);
        static vk::Format ToTextureFormat(TextureFormat format);

        static vk::DescriptorType SamplerInputAttributType(SamplerType sampler);
        static vk::DescriptorType BufferUsageAttributType(BufferUsageType usage);

        static vk::ShaderStageFlagBits ShaderStageToVkShaderStage(ShaderStage shaderStage);
    };

    template<typename... Args>
    VulkanResult vkCheckError_(vk::Result result, const char* format, Args&&... args) {
        VulkanResult result_;
        result_.result = result;
        result_.success = result == vk::Result::eSuccess;

        std::string file = VulkanStaticDebugInfo::file_call;
        std::string methode = VulkanStaticDebugInfo::methode_call;
        uint32 line = VulkanStaticDebugInfo::line_call;

        VulkanStaticDebugInfo::Result(true);

        if (!result_.success) {
            std::string message = FORMATTER.Format(format, args...);
            message = FORMATTER.Format("code : {0}({1}); {2}", VulkanConvert::VulkanResultToString((VkResult)result), (VkResult)result, message);
            LogBaseReset(GraphicsNameLogger, file.c_str(), line, methode.c_str()).Error("{0}", message);
            VulkanStaticDebugInfo::Result(false);
        }

        return result_;
    }

    template<typename... Args>
    VulkanResult vkCheckError_(VkResult result, const char* format, Args&&... args) {
        return vkCheckError_((vk::Result)result, format, args...);
    }

#define vkCheckError(first, presult, result, format, ... ) VulkanStaticDebugInfo::SetInfo(__FILE__, __LINE__, __FUNCTION__); presult = (presult.success || first) ? vkCheckError_(result, format, ##__VA_ARGS__) : presult; first = false
#define vkCheckErrorVoid(function) VulkanStaticDebugInfo::SetInfo(__FILE__, __LINE__, __FUNCTION__); function
}  //  nkentseu

#endif  // __VULKAN_UTILS_H__!