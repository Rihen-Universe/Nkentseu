//
// Created by TEUGUIA TADJUIDJE Rodolf S�deris on 2024-06-16 at 11:22:57 AM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __TEXTURE_H__
#define __TEXTURE_H__

#pragma once

#include <NTSCore/System.h>
#include <NTSCore/Stream/InputStream.h>

#include <string>
#include <atomic>
#include <filesystem>

#include <NTSMaths/Vector/Vector2.h>
#include <NTSMaths/Shapes/Rectangle.h>
#include <NTSMaths/Matrix/Matrix4.h>
#include "NTSMaths/Color.h"

#include "Image.h"
#include "Context.h"
#include "ShaderInputLayout.h"
#include "GraphicsEnum.h"

namespace nkentseu {

    class Texture2D;

    class NKENTSEU_API Texture {
        public:
            virtual bool                Update(const uint8* datas, const maths::Vector2i& offset, const maths::Vector2u& size) = 0;
            virtual bool                Update(const Image& image, const maths::Vector2i& offset = maths::Vector2i()) = 0;
            virtual bool                Update(const Texture2D& texture, const maths::Vector2i& offset = maths::Vector2i()) = 0;
            virtual maths::Vector2u     GetSize() const = 0;
            virtual uint8*              ExtractDatas(maths::Vector2u* size) = 0;
            virtual Image&              GetImage() = 0;
            virtual const Image&        GetImage() const = 0;
            virtual void                SetSmooth(bool smooth) = 0;
            virtual bool                IsSmooth() const = 0;
            virtual TextureFormat       GetTextureFormat() const = 0;
            virtual void                SetRepeated(bool repeated) = 0;
            virtual bool                IsRepeated() const = 0;
            virtual bool                GenerateMipmap() = 0;
            virtual void                Swap(Texture& right) noexcept = 0;
            virtual void                InvalidateMipmap() = 0;

            virtual const std::filesystem::path& GetPath() const = 0;
        private:
        protected:
    };

    class Texture2DBinding;

    using TextureID = void*;

    class NKENTSEU_API Texture2D : public Texture {
    public:
        virtual bool Create(TextureFormat textureFormat, const maths::Vector2u& size, uint32 channels = 4) = 0;
        virtual bool Create(TextureFormat textureFormat, const std::filesystem::path& filename) = 0;
        virtual bool Create(TextureFormat textureFormat, const Image& image) = 0;
        virtual bool Create(TextureFormat textureFormat, const uint8* data, const maths::Vector2u& size, uint32 channels = 4) = 0;

        virtual bool Destroy() = 0;

        static uint32 GetMaximumSize(Memory::Shared<Context> context);
        static uint32 GetValidSize(Memory::Shared<Context> context, uint32 size);

        static Memory::Shared<Texture2D> Create(Memory::Shared<Context> context);
        static Memory::Shared<Texture2D> Create(Memory::Shared<Context> context, TextureFormat textureFormat, const maths::Vector2u& size);
        static Memory::Shared<Texture2D> Create(Memory::Shared<Context> context, TextureFormat textureFormat, const std::filesystem::path& filename);
        static Memory::Shared<Texture2D> Create(Memory::Shared<Context> context, TextureFormat textureFormat, const Image& image);
        static Memory::Shared<Texture2D> Create(Memory::Shared<Context> context, TextureFormat textureFormat, const uint8* data, const maths::Vector2u& size, uint32 channels = 4);
    };

    class NKENTSEU_API Texture2DBinding {
    public:
        virtual bool Initialize(Memory::Shared<Texture2D> texture) = 0;
        virtual bool Destroy() = 0;
        virtual bool Bind(const std::string& name) = 0;
        virtual bool Bind(uint32 binding) = 0;
        virtual bool Equal(Memory::Shared<Texture2DBinding> binding) = 0;
        virtual bool IsDefined(Memory::Shared<Texture2D> binding) = 0;
        virtual bool IsValide() = 0;

        static Memory::Shared<Texture2DBinding> Create(Memory::Shared<Context> context, Memory::Shared<ShaderInputLayout> sil);
        static Memory::Shared<Texture2DBinding> CreateInitialize(Memory::Shared<Context> context, Memory::Shared<ShaderInputLayout> sil, Memory::Shared<Texture2D> texture);
    };

    using Texture2DShared = Memory::Shared<Texture2D>;
    using Texture2DBindingShared = Memory::Shared<Texture2DBinding>;

}  //  nkentseu

#endif  // __TEXTURE_H__!