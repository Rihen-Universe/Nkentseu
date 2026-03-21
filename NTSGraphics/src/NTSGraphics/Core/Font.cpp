//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-07-27 at 09:07:26 AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "pch/ntspch.h"
#include "Font.h"

#include <ft2build.h>
#include "Log.h"
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H
#include FT_BITMAP_H
#include FT_STROKER_H

namespace nkentseu {

    struct NKENTSEU_API FontPrivate {
        FontPrivate() = default;

        ~FontPrivate() {
            FT_Stroker_Done(stroker);
            FT_Done_Face(face);
            FT_Done_FreeType(library);
        }

        FontPrivate(const FontPrivate&) = delete;
        FontPrivate& operator=(const FontPrivate&) = delete;

        FontPrivate(FontPrivate&&) = delete;
        FontPrivate& operator=(FontPrivate&&) = delete;

        FT_Library   library{};
        FT_StreamRec streamRec{};
        FT_Face      face{};
        FT_Stroker   stroker{};
    };

    FontNG::FontNG(const std::string& fontPath)
        : m_FontPrivate(Memory::AllocateShared<FontPrivate>()) {

        if (FT_Init_FreeType(&m_FontPrivate->library)) {
            // Handle error
        }
        if (FT_New_Face(m_FontPrivate->library, fontPath.c_str(), 0, &m_FontPrivate->face)) {
            // Handle error
        }
        if (FT_Stroker_New(m_FontPrivate->library, &m_FontPrivate->stroker)) {
            // Handle error
        }
    }

    FontNG::~FontNG() {
        for (auto& pair : glyphCache) {
            delete pair.second;
        }
        glyphCache.clear();
    }

    GlyphNG* FontNG::GetGlyph(uint32 character) {
        auto it = glyphCache.find(character);
        if (it != glyphCache.end()) {
            return it->second;
        }
        else {
            //LoadGlyph(character);
            LoadGlyph2(character);
            return glyphCache.count(character) ? glyphCache[character] : nullptr;
        }
    }

    void FontNG::LoadGlyph(uint32 character) {
        FT_UInt glyphIndex = FT_Get_Char_Index(m_FontPrivate->face, character);
        if (FT_Load_Glyph(m_FontPrivate->face, glyphIndex, FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING)) {
            // Handle error
            return;
        }

        FT_GlyphSlot slot = m_FontPrivate->face->glyph;
        FT_Outline outline = slot->outline;

        std::vector<maths::Vector2f> points;
        std::vector<uint32> indices;

        // Add points from the outline
        for (int i = 0; i < outline.n_points; ++i) {
            FT_Vector point = outline.points[i];
            points.emplace_back(static_cast<float>(point.x) / 64.0f, static_cast<float>(point.y) / 64.0f);
        }

        // Triangulate the contour
        for (int c = 0; c < outline.n_contours; ++c) {
            int contourStart = (c == 0) ? 0 : outline.contours[c - 1] + 1;
            int contourEnd = outline.contours[c];
            int numPoints = contourEnd - contourStart + 1;

            if (numPoints < 3) continue;  // Not enough points to form a triangle

            // Use a simple triangulation algorithm (ear clipping) for the contour
            std::vector<int> contourIndices(numPoints);
            for (int i = 0; i < numPoints; ++i) {
                contourIndices[i] = contourStart + i;
            }

            // Triangulation (ear clipping method)
            while (numPoints > 2) {
                bool earFound = false;

                for (int i = 0; i < numPoints; ++i) {
                    int prev = (i + numPoints - 1) % numPoints;
                    int next = (i + 1) % numPoints;

                    // Check if the triangle is a valid ear
                    if (IsEar(points, contourIndices[prev], contourIndices[i], contourIndices[next])) {
                        // Add the triangle indices
                        indices.push_back(contourIndices[prev]);
                        indices.push_back(contourIndices[i]);
                        indices.push_back(contourIndices[next]);

                        // Remove the ear vertex
                        contourIndices.erase(contourIndices.begin() + i);
                        numPoints--;
                        earFound = true;
                        break;
                    }
                }

                if (!earFound) break;  // No more ears found, exit
            }
        }

        glyphCache[character] = new GlyphNG(character, points, indices);
    }

    // Helper function to determine if a triangle is a valid ear
    bool FontNG::IsEar(const std::vector<maths::Vector2f>& points, int p1, int p2, int p3) {
        // Simple check to ensure that the triangle is convex
        // You can add more sophisticated checks here (e.g., checking if the triangle is inside the contour)
        return true;  // Assume it's an ear for simplicity
    }

    void FontNG::LoadGlyph2(uint32 character) {
        FT_UInt glyphIndex = FT_Get_Char_Index(m_FontPrivate->face, character);
        if (FT_Load_Glyph(m_FontPrivate->face, glyphIndex, FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING)) {
            // Handle error
            return;
        }

        FT_GlyphSlot slot = m_FontPrivate->face->glyph;
        FT_Outline outline = slot->outline;

        std::vector<maths::Vector2f> vertices;
        std::vector<uint32> indices;

        // Triangulate the outline
        for (int i = 0; i < outline.n_points; ++i) {
            FT_Vector point = outline.points[i];
            vertices.emplace_back(static_cast<float>(point.x) / 64.0f, static_cast<float>(point.y) / 64.0f);
        }

        // Use the Ear Clipping algorithm to triangulate the polygon
        std::vector<int> earIndices;
        for (int i = 0; i < outline.n_contours; ++i) {
            int contourStart = earIndices.size();
            int contourEnd = outline.contours[i];
            for (int j = contourStart; j < contourEnd; ++j) {
                earIndices.push_back(j);
            }
            earIndices.push_back(contourEnd);
        }

        while (earIndices.size() > 3) {
            int earIndex = FindEar(earIndices, vertices);
            if (earIndex == -1) break; // No more ears to clip

            int prevIndex = (earIndex - 1 + earIndices.size()) % earIndices.size();
            int nextIndex = (earIndex + 1) % earIndices.size();

            indices.push_back(earIndices[prevIndex]);
            indices.push_back(earIndices[earIndex]);
            indices.push_back(earIndices[nextIndex]);

            earIndices.erase(earIndices.begin() + earIndex);
        }

        // Add the remaining triangle
        if (earIndices.size() == 3) {
            indices.push_back(earIndices[0]);
            indices.push_back(earIndices[1]);
            indices.push_back(earIndices[2]);
        }

        glyphCache[character] = new GlyphNG(character, vertices, indices);
    }

    int FontNG::FindEar(std::vector<int>& earIndices, const std::vector<maths::Vector2f>& vertices) {
        for (int i = 0; i < earIndices.size(); ++i) {
            int prevIndex = (i - 1 + earIndices.size()) % earIndices.size();
            int nextIndex = (i + 1) % earIndices.size();

            maths::Vector2f prevVertex = vertices[earIndices[prevIndex]];
            maths::Vector2f vertex = vertices[earIndices[i]];
            maths::Vector2f nextVertex = vertices[earIndices[nextIndex]];

            if (IsEar(prevVertex, vertex, nextVertex)) {
                return i;
            }
        }

        return -1; // No ear found
    }

    bool FontNG::IsEar(const maths::Vector2f& prevVertex, const maths::Vector2f& vertex, const maths::Vector2f& nextVertex) {
        // Check if the vertex is an ear (i.e., it's a convex vertex)
        float crossProduct = (nextVertex.x - vertex.x) * (prevVertex.y - vertex.y) - (nextVertex.y - vertex.y) * (prevVertex.x - vertex.x);
        return crossProduct > 0.0f;
    }

    maths::Vector2f FontNG::CalculateScale(float32 fontSize) {
        return maths::Vector2f(fontSize / static_cast<float32>(m_FontPrivate->face->units_per_EM));
    }

    Font::Font(Memory::Shared<Context> context)
        : m_Context(context), m_FontPrivate(nullptr) {
    }

    Font::~Font() {
    }

    bool Font::LoadFromFile(const std::string& filename) {
        Destroy();

        m_FontPrivate = Memory::AllocateShared<FontPrivate>();

        if (m_FontPrivate == nullptr) {
            gLog.Error("ERROR::MEMORY: Could not create memory for free type", filename);
            return false;
        }

        if (FT_Init_FreeType(&m_FontPrivate->library) != 0) {
            gLog.Error("ERROR::FREETYPE: Could not init FreeType Library {0}", filename);
            return false;
        }

        FT_Face face;
        if (FT_New_Face(m_FontPrivate->library, filename.c_str(), 0, &face) != 0) {
            gLog.Error("ERROR::FREETYPE: Failed to load font {0}", filename);
            return false;
        }
        m_FontPrivate->face = face;

        if (FT_Stroker_New(m_FontPrivate->library, &m_FontPrivate->stroker) != 0) {
            gLog.Error("ERROR::FREETYPE: Failed to create the stroker {0}", filename);
            return false;
        }

        if (FT_Select_Charmap(face, FT_ENCODING_UNICODE) != 0) {
            gLog.Error("ERROR::FREETYPE: Failed to set the Unicode character set {0}", filename);
            return false;
        }
        
        family = face->family_name ? face->family_name : std::string();
        return true;
    }

    std::string Font::GetFamily() const {
        return family;
    }

    const Glyph* Font::GetGlyph(uint32 character, uint32 characterSize, bool bold, float32 outlineThickness) {
        
        auto it = m_GlyphPages.find(characterSize);
        if (it != m_GlyphPages.end() && it->second != nullptr) {
            const Glyph* glyph = it->second->GetGlyph(character);
            if (glyph != nullptr) {
                //gLog.Trace();
                return glyph;
            }
            
        }
        else {
            static uint32 maxTextureSize = Texture2D::GetMaximumSize(m_Context);

            if (maxTextureSize == 0) {
                gLog.Trace();
                return nullptr;
            }

            m_GlyphPages[characterSize] = Memory::AllocateShared<GlyphPages>(maxTextureSize);
            auto it = m_GlyphPages.find(characterSize);
            
            if (it == m_GlyphPages.end()) {
                gLog.Trace();
                return nullptr;
            }
        }

        std::vector<uint8> pixels;
        // Si le glyphe n'est pas trouvé, tenter de le charger
        return LoadGlyph(character, characterSize, bold, outlineThickness, pixels);
        /*Glyph newGlyph = LoadGlyph(character, characterSize, bold, outlineThickness, pixels);

        std::string format = "\n";
        for (uint32 y = 0; y < newGlyph.bounds.height; ++y) {
            for (uint32 x = 0; x < newGlyph.bounds.width; ++x) {
                format = FORMATTER.Format("{0} {1}", format, maths::Color(0, 0, 0, pixels[y * newGlyph.bounds.width + x]).ToUint32A());
            }
            format += "\n";
        }
        gLog.Trace("{0}", format);

        if (newGlyph.textureRect != maths::IntRect{}) { // Assurez-vous que ce code vérifie si le glyphe a été chargé avec succès
            // Ajouter le glyphe chargé à la carte
            Glyph* glyph = m_GlyphPages[characterSize]->AddGlyph(m_Context, character, newGlyph, pixels.data());
            if (glyph == nullptr) gLog.Info();
            gLog.Trace();
            return glyph;
        }

        gLog.Trace();
        return nullptr; // Retourner nullptr si le glyphe ne peut pas être chargé*/
    }

    bool Font::HasGlyph(uint32 character) const
    {
        return FT_Get_Char_Index(m_FontPrivate ? m_FontPrivate->face : nullptr, character) != 0;
    }

    float32 Font::GetKerning(uint32 first, uint32 second, uint32 characterSize, bool bold)
    {
        if (first == 0 || second == 0)
            return 0.f;

        if (!FT_HAS_KERNING(m_FontPrivate->face))
            return 0.0f;

        FT_Face face = m_FontPrivate ? m_FontPrivate->face : nullptr;

        if (face && SetCurrentSize(characterSize))
        {
            const FT_UInt index1 = FT_Get_Char_Index(face, first);
            const FT_UInt index2 = FT_Get_Char_Index(face, second);

            const Glyph* glyph = GetGlyph(first, characterSize, bold);

            if (glyph != nullptr) {
                const auto sbDelta = glyph->sbDelta;

                FT_Vector kerning{ 0, 0 };
                if (FT_HAS_KERNING(face))
                    FT_Get_Kerning(face, index1, index2, FT_KERNING_UNFITTED, &kerning);

                if (!FT_IS_SCALABLE(face))
                    return static_cast<float32>(kerning.x);

                return std::floor((sbDelta.y - sbDelta.x + static_cast<float32>(kerning.x) + 32) / static_cast<float32>(1 << 6));
            }
        }
        return 0.f;
    }

    float32 Font::GetLineSpacing(uint32 characterSize) const
    {
        FT_Face face = m_FontPrivate ? m_FontPrivate->face : nullptr;

        if (face && SetCurrentSize(characterSize))
        {
            return static_cast<float32>(face->size->metrics.height) / static_cast<float32>(1 << 6);
        }
        else
        {
            return 0.f;
        }
    }

    float32 Font::GetUnderlinePosition(uint32 characterSize) const
    {
        FT_Face face = m_FontPrivate ? m_FontPrivate->face : nullptr;

        if (face && SetCurrentSize(characterSize))
        {
            if (!FT_IS_SCALABLE(face))
                return static_cast<float32>(characterSize) / 10.f;

            return -static_cast<float32>(FT_MulFix(face->underline_position, face->size->metrics.y_scale)) / static_cast<float32>(1 << 6);
        }
        else
        {
            return 0.f;
        }
    }

    float32 Font::GetUnderlineThickness(uint32 characterSize) const
    {
        FT_Face face = m_FontPrivate ? m_FontPrivate->face : nullptr;

        if (face && SetCurrentSize(characterSize))
        {
            if (!FT_IS_SCALABLE(face))
                return static_cast<float32>(characterSize) / 14.f;

            return static_cast<float32>(FT_MulFix(face->underline_thickness, face->size->metrics.y_scale)) /
                static_cast<float32>(1 << 6);
        }
        else
        {
            return 0.f;
        }
    }

    Texture2DShared Font::GetTexture(uint32 character, uint32 characterSize) const {
        auto it = m_GlyphPages.find(characterSize);
        if (it != m_GlyphPages.end()) {
            return it->second->GetTexture(character);
        }
        return nullptr;
    }

    bool Font::Destroy()
    {
        Memory::Reset(m_FontPrivate);
        bool success = true;
        for (auto [cs, pages] : m_GlyphPages) {
            success = pages->Destroy() && success;
        }
        m_GlyphPages.clear();
        return success;
    }

    Glyph* Font::LoadGlyph(uint32 codePoint, uint32 characterSize, bool bold, float32 outlineThickness, std::vector<uint8>& pixels)
    {
        Glyph glyph;

        if (m_FontPrivate == nullptr || !SetCurrentSize(characterSize)) {
            if (m_FontPrivate == nullptr) 
            return nullptr;
        }

        FT_Face face = m_FontPrivate->face;
        FT_Error error = FT_Load_Char(face, codePoint, FT_LOAD_RENDER);
        if (error) {
            gLog.Error("ERROR::FREETYPE: Failed to load glyph {0}", codePoint);
            return nullptr;
        }

        // Appliquer l'épaisseur et l'ombrage si nécessaire
        if (bold) {
            FT_Outline_Embolden(&face->glyph->outline, 1 << 6);
        }

        FT_Glyph glyphDesc;
        if (FT_Get_Glyph(face->glyph, &glyphDesc)) {
            gLog.Error("ERROR::FREETYPE: Failed to get glyph description");
            return nullptr;
        }

        if (outlineThickness > 0) {
            FT_Stroker stroker = m_FontPrivate->stroker;
            FT_Stroker_Set(stroker, static_cast<FT_Fixed>(outlineThickness * (1 << 6)), FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);
            FT_Glyph_StrokeBorder(&glyphDesc, stroker, true, false);
        }

        FT_Glyph_To_Bitmap(&glyphDesc, FT_RENDER_MODE_NORMAL, nullptr, 1);
        FT_BitmapGlyph bitmapGlyph = reinterpret_cast<FT_BitmapGlyph>(glyphDesc);
        FT_Bitmap& bitmap = bitmapGlyph->bitmap;

        // Configurer les données du glyphe
        glyph.advance = maths::Vector2f(bitmapGlyph->root.advance.x >> 16, bitmapGlyph->root.advance.y >> 16);
        glyph.sbDelta = maths::Vector2f(static_cast<int32>(face->glyph->lsb_delta), static_cast<int32>(face->glyph->rsb_delta));
        glyph.bearing = maths::Vector2f(bitmapGlyph->left, bitmapGlyph->top);

        uint32 space = 1;
        uint32 width = bitmap.width + space * 2;
        uint32 height = bitmap.rows + space * 2;
        glyph.textureRect.size = { (int32)width, (int32)height };
        glyph.bounds = { static_cast<float32>(bitmapGlyph->left), static_cast<float32>(-bitmapGlyph->top), static_cast<float32>(width), static_cast<float32>(height) };

        //pixels.resize(width * height * 4, 0);
        //uint8* dest = pixels.data();

        uint8* pixelsData = new uint8[height * width * 4];

        // Remplir les pixels
        for (uint32 y = 0; y < height; ++y) {
            for (uint32 x = 0; x < width; ++x) {
                uint8 alpha = y < space || x < space || x >= width - space - 1 || y >= height - space - 1 ? 0 : bitmap.buffer[y * bitmap.pitch + x];
                pixelsData[(y * width + x) * 4 + 0] = 255; // Red
                pixelsData[(y * width + x) * 4 + 1] = 255; // Green
                pixelsData[(y * width + x) * 4 + 2] = 255; // Blue
                pixelsData[(y * width + x) * 4 + 3] = alpha; // Alpha channel
            }
        }

        FT_Done_Glyph(glyphDesc);

        if (glyph.textureRect != maths::IntRect{}) { // Assurez-vous que ce code vérifie si le glyphe a été chargé avec succès
            // Ajouter le glyphe chargé à la carte
            Glyph* glyphRender = m_GlyphPages[characterSize]->AddGlyph(m_Context, codePoint, glyph, pixelsData);
            if (glyphRender == nullptr) gLog.Info();

            delete pixelsData;
            pixelsData = nullptr;
            return glyphRender;
        }

        delete pixelsData;
        pixelsData = nullptr;
        return nullptr;
    }

    bool Font::SetCurrentSize(uint32 characterSize) const
    {
        FT_Face face = m_FontPrivate ? m_FontPrivate->face : nullptr;
        if (!face) return false;

        const FT_UShort currentSize = face->size->metrics.x_ppem;

        if (currentSize != characterSize)
        {
            const FT_Error result = FT_Set_Pixel_Sizes(face, 0, characterSize);

            if (result == FT_Err_Invalid_Pixel_Size)
            {
                if (!FT_IS_SCALABLE(face))
                {
                    std::string infos = FORMATTER.Format("Failed to set bitmap font size to {0}\nAvailable sizes are:", characterSize);
                    for (int i = 0; i < face->num_fixed_sizes; ++i)
                    {
                        const long size = (face->available_sizes[i].y_ppem + 32) >> 6;
                        infos = FORMATTER.Format("{0} {1}", infos, size);
                    }
                    gLog.Error("{0}", infos);
                }
                else
                {
                    gLog.Error("Failed to set font size to {0}", characterSize);
                }
            }

            return result == FT_Err_Ok;
        }

        return true;
    }

}  //  nkentseu