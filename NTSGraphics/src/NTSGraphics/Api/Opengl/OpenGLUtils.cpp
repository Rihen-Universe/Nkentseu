//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-05-19 at 10:55:46 AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "pch/ntspch.h"
#include "OpenGLUtils.h"

namespace nkentseu {

    std::string OpenGLStaticDebugInfo::file_call = "";
    uint32 OpenGLStaticDebugInfo::line_call = 0;
    std::string OpenGLStaticDebugInfo::methode_call = "";
    bool OpenGLStaticDebugInfo::success = true;

    OpenGLResult glCheckError_(const std::string& file, int32 line, const std::string& function)
    {
        OpenGLResult result = {true, GL_NO_ERROR};
        GLenum errorCode;
        while ((errorCode = glGetError()) != GL_NO_ERROR)
        {
            std::string error;
            switch (errorCode)
            {
            case GL_INVALID_ENUM:                  error = "INVALID_ENUM"; break;
            case GL_INVALID_VALUE:                 error = "INVALID_VALUE"; break;
            case GL_INVALID_OPERATION:             error = "INVALID_OPERATION"; break;
            case GL_STACK_OVERFLOW:                error = "STACK_OVERFLOW"; break;
            case GL_STACK_UNDERFLOW:               error = "STACK_UNDERFLOW"; break;
            case GL_OUT_OF_MEMORY:                 error = "OUT_OF_MEMORY"; break;
            case GL_INVALID_FRAMEBUFFER_OPERATION: error = "INVALID_FRAMEBUFFER_OPERATION"; break;
            }

            LogBaseReset(GraphicsNameLogger, file.c_str(), line, function.c_str()).Fatal("type = {0}", error);

            if (result.success) {
                result.result = errorCode;
                result.success = errorCode == GL_NO_ERROR;
            }
        }
        return result;
    }

    uint32 GLConvert::ShaderType(ShaderInternalType shaderType)
    {
        if (shaderType == ShaderInternalType::Enum::Boolean) return GL_BOOL;
        if (shaderType == ShaderInternalType::Enum::Float) return GL_FLOAT;
        if (shaderType == ShaderInternalType::Enum::Float2) return GL_FLOAT;
        if (shaderType == ShaderInternalType::Enum::Float3) return GL_FLOAT;
        if (shaderType == ShaderInternalType::Enum::Float4) return GL_FLOAT;
        if (shaderType == ShaderInternalType::Enum::Int) return GL_INT;
        if (shaderType == ShaderInternalType::Enum::Int2) return GL_INT;
        if (shaderType == ShaderInternalType::Enum::Int3) return GL_INT;
        if (shaderType == ShaderInternalType::Enum::Int4) return GL_INT;
        //if (shaderType == ShaderInternalType::Enum::UInt): return (vk::Format) VK_FORMAT_R32_UINT;
        //if (shaderType == ShaderInternalType::Enum::UInt2): return (vk::Format) VK_FORMAT_R32G32_UINT;
        //if (shaderType == ShaderInternalType::Enum::UInt3): return (vk::Format) VK_FORMAT_R32G32B32_UINT;
        //if (shaderType == ShaderInternalType::Enum::UInt4): return (vk::Format) VK_FORMAT_R32G32B32A32_UINT;
        if (shaderType == ShaderInternalType::Enum::Byte) return GL_BYTE;
        if (shaderType == ShaderInternalType::Enum::Byte2) return GL_BYTE;
        if (shaderType == ShaderInternalType::Enum::Byte3) return GL_BYTE;
        if (shaderType == ShaderInternalType::Enum::Byte4) return GL_BYTE;
        if (shaderType == ShaderInternalType::Enum::Mat2) return GL_FLOAT;
        if (shaderType == ShaderInternalType::Enum::Mat3) return GL_FLOAT;
        if (shaderType == ShaderInternalType::Enum::Mat4) return GL_FLOAT;

        return 0;
    }

    uint32 GLConvert::UsageType(BufferUsageType type)
    {
        if (type == BufferUsageType::Enum::StaticDraw) return GL_STATIC_DRAW;
        if (type == BufferUsageType::Enum::DynamicDraw) return GL_DYNAMIC_DRAW;
        if (type == BufferUsageType::Enum::StreamDraw) return GL_STREAM_DRAW;
        return 0;
    }

    uint32 GLConvert::ToCullModeType(CullModeType mode) {
        if (mode == CullModeType::Enum::Front) return GL_FRONT;
        if (mode == CullModeType::Enum::Back) return GL_BACK;
        if (mode == CullModeType::Enum::FrontBack) return GL_FRONT_AND_BACK;
        return GL_FRONT_AND_BACK;
    }

    uint32 GLConvert::ToPolygonModeType(PolygonModeType contentMode) {
        if (contentMode == PolygonModeType::Enum::Line) return GL_LINE;
        if (contentMode == PolygonModeType::Enum::Fill) return GL_FILL;
        if (contentMode == PolygonModeType::Enum::FillRectangle) return GL_FILL_RECTANGLE_NV;
        if (contentMode == PolygonModeType::Enum::Point) return GL_POINT;
        return 0;
    }

    uint32 GLConvert::VerticesPerType(uint32 vertexType) {
        if (vertexType == GL_TRIANGLES) return 3;
        return 0;
    }

    uint32 GLConvert::IndexType(IndexBufferType drawIndex)
    {
        if (drawIndex == IndexBufferType::Enum::UInt8) return GL_UNSIGNED_INT;
        if (drawIndex == IndexBufferType::Enum::UInt16) return GL_UNSIGNED_INT;
        if (drawIndex == IndexBufferType::Enum::UInt32) return GL_UNSIGNED_INT;
        if (drawIndex == IndexBufferType::Enum::UInt64) return GL_UNSIGNED_INT;
        return 0;
    }

    uint32 GLConvert::GetModuleType(ShaderStage shaderStage)
    {
        if (shaderStage.HasFlag(ShaderStage::Enum::Vertex)) return GL_VERTEX_SHADER;
        if (shaderStage.HasFlag(ShaderStage::Enum::Fragment)) return GL_FRAGMENT_SHADER;
        if (shaderStage.HasFlag(ShaderStage::Enum::Geometry)) return GL_GEOMETRY_SHADER;
        if (shaderStage.HasFlag(ShaderStage::Enum::TesControl)) return GL_TESS_CONTROL_SHADER;
        if (shaderStage.HasFlag(ShaderStage::Enum::TesEvaluation)) return GL_TESS_EVALUATION_SHADER;
        if (shaderStage.HasFlag(ShaderStage::Enum::Compute)) return GL_COMPUTE_SHADER;

        return GL_VERTEX_SHADER;
    }

    uint32 GLConvert::GetModernModuleType(ShaderStage shaderStage) {

        if (shaderStage.HasFlag(ShaderStage::Enum::Vertex)) return GL_VERTEX_SHADER_BIT;
        if (shaderStage.HasFlag(ShaderStage::Enum::Fragment)) return GL_FRAGMENT_SHADER_BIT;
        if (shaderStage.HasFlag(ShaderStage::Enum::Geometry)) return GL_GEOMETRY_SHADER_BIT;
        if (shaderStage.HasFlag(ShaderStage::Enum::TesControl)) return GL_TESS_CONTROL_SHADER_BIT;
        if (shaderStage.HasFlag(ShaderStage::Enum::TesEvaluation)) return GL_TESS_EVALUATION_SHADER_BIT;
        if (shaderStage.HasFlag(ShaderStage::Enum::Compute)) return GL_COMPUTE_SHADER_BIT;

        return GL_VERTEX_SHADER_BIT;
    }

    uint32 GLConvert::GetPrimitiveType(RenderPrimitive primitive)
    {
        if (primitive == RenderPrimitive::Enum::Points) return GL_POINTS;
        if (primitive == RenderPrimitive::Enum::Lines) return GL_LINES;
        if (primitive == RenderPrimitive::Enum::LineStrip) return GL_LINE_STRIP;
        if (primitive == RenderPrimitive::Enum::Triangles) return GL_TRIANGLES;
        if (primitive == RenderPrimitive::Enum::TriangleStrip) return GL_TRIANGLE_STRIP;
        if (primitive == RenderPrimitive::Enum::TriangleFan) return GL_TRIANGLE_FAN;
        return GL_TRIANGLES;
    }

    GLenum GLConvert::ToTextureFormat(TextureFormat format) {
        if (format == TextureFormat::Enum::RGBA8) return GL_RGBA8;
        if (format == TextureFormat::Enum::RGB8) return GL_RGB8;
        if (format == TextureFormat::Enum::SRGB8_ALPHA8) return GL_SRGB8_ALPHA8;
        if (format == TextureFormat::Enum::DEPTH_COMPONENT16) return GL_DEPTH_COMPONENT16;
        if (format == TextureFormat::Enum::DEPTH_COMPONENT24) return GL_DEPTH_COMPONENT24;
        if (format == TextureFormat::Enum::DEPTH_COMPONENT32F) return GL_DEPTH_COMPONENT32F;
        if (format == TextureFormat::Enum::STENCIL_INDEX8) return GL_STENCIL_INDEX8;
        if (format == TextureFormat::Enum::DEPTH24_STENCIL8) return GL_DEPTH24_STENCIL8;
        if (format == TextureFormat::Enum::DEPTH32F_STENCIL8) return GL_DEPTH32F_STENCIL8;
        if (format == TextureFormat::Enum::RED_INTEGER) return GL_RED_INTEGER;
        return GL_RGBA8;
    }

    GLenum GLConvert::ToTextureDataFormat(TextureFormat format) {
        if (format == TextureFormat::Enum::RGBA8) return GL_RGBA;
        if (format == TextureFormat::Enum::RED_INTEGER) return GL_RED_INTEGER;
        if (format == TextureFormat::Enum::RGB8) return GL_RGB;
        if (format == TextureFormat::Enum::SRGB8_ALPHA8) return GL_RGBA;
        if (format == TextureFormat::Enum::DEPTH24_STENCIL8) return GL_DEPTH_STENCIL_ATTACHMENT;
        if (format == TextureFormat::Enum::DEPTH32F_STENCIL8) return GL_DEPTH_STENCIL_ATTACHMENT;
        if (format == TextureFormat::Enum::DEPTH_COMPONENT16) return GL_DEPTH_ATTACHMENT;
        if (format == TextureFormat::Enum::DEPTH_COMPONENT24) return GL_DEPTH_ATTACHMENT;
        if (format == TextureFormat::Enum::DEPTH_COMPONENT32F) return GL_DEPTH_ATTACHMENT;
        if (format == TextureFormat::Enum::STENCIL_INDEX8) return GL_STENCIL_ATTACHMENT;
        return GL_RGBA;
    }

    bool OpenglBuffer::Create(const std::string& uniforName, uint32 bufferType, uint32 usage, const void* data, usize size, usize binding, int64 offset, uint32 instance, bool usedsa)
    {
        OpenGLResult result;
        bool first = true;

        this->usage = usage;
        this->bufferType = bufferType;
        this->useDAS = usedsa;
        this->instance = instance;
        this->name = uniforName;
        this->binding = binding;
        this->size = size;

        uint32 sizes = size;

        if (this->instance > 1 && bufferType == GL_UNIFORM_BUFFER) {
            int32 minUboAlignment = 0;
            glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &minUboAlignment);

            if (minUboAlignment > 0) {
                dynamicAlignment = (size + minUboAlignment - 1) & ~(minUboAlignment - 1);
                sizes = dynamicAlignment * instance;
            }
        }

        if (this->useDAS) {
            glCheckError(first, result, glCreateBuffers(1, &buffer), "cannot create dsa buffer : {0}", name);
            glCheckError(first, result, glNamedBufferData(buffer, sizes, data, usage), "cannot set data buffer : {0}", name);
        }
        else {
            glCheckError(first, result, glGenBuffers(1, &buffer), "cannot gen buffer : {0}", name);
            glCheckError(first, result, glBindBuffer(bufferType, buffer), "cannot bind buffer : {0}", name);
            glCheckError(first, result, glBufferData(bufferType, sizes, data, usage), "cannot set data buffer : {0}", name);
            //Log_nts.Debug("type = {0}, name = {1}", bufferType, name);
        }   

        if (result.success){
            Unbind();
        }

        if (bufferType == GL_UNIFORM_BUFFER) {
            glCheckError(first, result, glBindBufferRange(bufferType, binding, buffer, offset, size), "cannot bind buffer range : {0}", name);
        }

        return result.success;
    }

    bool OpenglBuffer::Destroy()
    {
        if (buffer == 0) return false;

        OpenGLResult result;
        bool first = true;

        glCheckError(first, result, glDeleteBuffers(1, &buffer), "cannot delete buffer : {0}", name);
        buffer = 0;

        return result.success;
    }

    bool OpenglBuffer::WriteToBuffer(const void* data, usize size, usize offset)
    {
        if (buffer == 0) return false;

        OpenGLResult result;
        bool first = true;

        if (this->useDAS) {

            currentOffset = offset;

            if (uType == BufferUsageType::Enum::DynamicDraw && bufferType == GL_UNIFORM_BUFFER) {
                currentIndex++;

                if (currentIndex >= instance) {
                    currentIndex = 0;
                }
                currentOffset = currentIndex * dynamicAlignment;
            }

            glCheckError(first, result, glNamedBufferSubData(buffer, offset, size, data), "cannot bind buffer : {0}", name);
        }
        else {
            glCheckError(first, result, glBindBuffer(bufferType, buffer), "cannot bind buffer : {0}", name);

            if (usage == GL_DYNAMIC_DRAW && bufferType != GL_UNIFORM_BUFFER && size != 0) {
                //glBufferData(bufferType, size, nullptr, GL_DYNAMIC_DRAW);
                //Log_nts.Debug();
            }

            glCheckError(first, result, glBufferSubData(bufferType, offset, size, data), "cannot bind buffer : {0}", name);

            if (bufferType == GL_UNIFORM_BUFFER) {
                glCheckError(first, result, glBindBufferRange(bufferType, binding, buffer, offset, size), "cannot bind buffer range : {0}", name);
            }

            glCheckError(first, result, glBindBuffer(bufferType, 0), "cannot bind buffer : {0}", name);
        }

        return result.success;
    }

    bool OpenglBuffer::Bind() const
    {
        if (buffer == 0) return false;

        OpenGLResult result;
        bool first = true;

        glCheckError(first, result, glBindBuffer(bufferType, buffer), "cannot bind buffer : {0}", name);

        return result.success;
    }

    bool OpenglBuffer::Unbind() const
    {
        if (buffer == 0) return false;

        OpenGLResult result;
        bool first = true;

        glCheckError(first, result, glBindBuffer(bufferType, 0), "cannot unbind buffer : {0}", name);

        return result.success;
    }

}  //  nkentseu