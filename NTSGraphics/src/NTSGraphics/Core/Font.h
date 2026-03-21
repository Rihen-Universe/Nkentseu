//
// Created by TEUGUIA TADJUIDJE Rodolf S�deris on 2024-07-27 at 09:07:26 AM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __NKENTSEU_FONT_H__
#define __NKENTSEU_FONT_H__

#pragma once

#include <NTSCore/System.h>

#include "Glyph.h"
#include "Texture.h"

namespace nkentseu {

    struct FontPrivate;

    class NKENTSEU_API FontNG {
    public:
        FontNG(const std::string& fontPath);
        ~FontNG();

        GlyphNG* GetGlyph(uint32 character);
        maths::Vector2f CalculateScale(float32 fontSize);

    private:
        std::string family;
        Memory::Shared<FontPrivate> m_FontPrivate;
        NkUnorderedMap<uint32, GlyphNG*> glyphCache;

        void LoadGlyph2(uint32 character);
        void LoadGlyph(uint32 character);
        bool IsEar(const std::vector<maths::Vector2f>& points, int p1, int p2, int p3);
        bool IsEar(const maths::Vector2f& prevVertex, const maths::Vector2f& vertex, const maths::Vector2f& nextVertex);
        int FindEar(std::vector<int>& earIndices, const std::vector<maths::Vector2f>& vertices);
    };

    class NKENTSEU_API Font {
    public:
        Font(Memory::Shared<Context> context);
        ~Font();

        bool LoadFromFile(const std::string& filename);
        std::string GetFamily() const;
        bool HasGlyph(uint32 character) const;
        const Glyph* GetGlyph(uint32 character, uint32 characterSize, bool bold, float32 outlineThickness = 0);
        float32 GetKerning(uint32 first, uint32 second, uint32 characterSize, bool bold = false);
        float32 GetLineSpacing(uint32 characterSize) const;
        float32 GetUnderlinePosition(uint32 characterSize) const;
        float32 GetUnderlineThickness(uint32 characterSize) const;
        Texture2DShared GetTexture(uint32 character, uint32 characterSize) const;

        bool Destroy();

    private:
        std::string family;
        Memory::Shared<Context> m_Context;
        Memory::Shared<FontPrivate> m_FontPrivate = nullptr;
        NkUnorderedMap<uint32, Memory::Shared<GlyphPages>> m_GlyphPages; // Map from character size to GlyphPages

        Glyph* LoadGlyph(uint32 codePoint, uint32 characterSize, bool bold, float32 outlineThickness, std::vector<uint8>& pixels);
        bool SetCurrentSize(uint32 characterSize) const;
    };

}  //  nkentseu

#endif  // __NKENTSEU_FONT_H__!