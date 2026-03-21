//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-07-27 at 09:07:12 AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "pch/ntspch.h"
#include "Glyph.h"
#include "Log.h"

namespace nkentseu {

    GlyphPage::GlyphPage(uint32 maxTextureSize)
        : maxTextureSize(maxTextureSize), nextPositionWrite(0, 0), currentRowHeight(0) {
        //gLog.Debug("max = {0}", maxTextureSize);
    }

    GlyphPage::~GlyphPage()
    {
    }

    bool GlyphPage::AddGlyph(Memory::Shared<Context> context, const uint8* glyphPixel, maths::IntRect& outTextureRect) {
        if (context == nullptr || glyphPixel == nullptr) return false;

        // Si le glyphe ne rentre pas dans la ligne actuelle, passer à la ligne suivante
        if (nextPositionWrite.x + outTextureRect.width > maxTextureSize) {
            nextPositionWrite.x = 0;
            nextPositionWrite.y += currentRowHeight;
            currentRowHeight = 0;
        }

        // Si le glyphe ne rentre pas dans la texture, la texture est pleine
        if (nextPositionWrite.y + outTextureRect.height > maxTextureSize) {
            return false;  // La texture est pleine
        }
        bool success = true;

        if (texture == nullptr) {
            texture = Texture2D::Create(context, TextureFormat::Enum::RGBA8, glyphPixel, maths::Vector2u(outTextureRect.width, outTextureRect.height));
            if (texture == nullptr) return false;
        }
        else {
            success = texture->Update(glyphPixel, nextPositionWrite, maths::Vector2u(outTextureRect.width, outTextureRect.height));
        }
        outTextureRect.x = nextPositionWrite.x;
        outTextureRect.y = nextPositionWrite.y;

        nextPositionWrite.x += outTextureRect.width;
        currentRowHeight = maths::Max<uint32>(currentRowHeight, nextPositionWrite.y + outTextureRect.height);

        texture->GetImage().Save("test.png");

        return success;
    }

    bool GlyphPage::IsFull(uint32 maxTextureSize) const {
        return nextPositionWrite.y + currentRowHeight > maxTextureSize;
    }

    bool GlyphPage::Destroy()
    {
        if (texture != nullptr) {
            bool success = texture->Destroy();
            Memory::Reset(texture);
            texture = nullptr;
            return success;
        }
        return true;
    }

    GlyphPages::GlyphPages(uint32 maxTextureSize) : maximumTextureSize(maxTextureSize) {
        //gLog.Debug("max = {0}", maximumTextureSize);
    }

    Glyph* GlyphPages::GetGlyph(uint64 character) {
        auto it = glyphs.find(character);
        if (it != glyphs.end()) {
            return &it->second;
        }
        return nullptr;
    }

    Glyph* GlyphPages::AddGlyph(Memory::Shared<Context> context, uint64 character, const Glyph& glyph, const uint8* glyphPixel) {
        if (context == nullptr) return nullptr;
        maths::IntRect textureRect(glyph.textureRect);

        if (!pages.empty()) {
            auto& lastPage = pages.back();
            if (!lastPage->IsFull(maximumTextureSize)) {
                if (lastPage->AddGlyph(context, glyphPixel, textureRect)) {
                    Glyph newGlyph = glyph;
                    newGlyph.textureRect = textureRect;
                    newGlyph.pageIndex = pages.size() - 1;
                    glyphs[character] = newGlyph;
                    return &glyphs[character];
                }
            }
        }

        // Si la dernière page est pleine ou si aucune page n'existe, créer une nouvelle page
        auto newPage = Memory::AllocateShared<GlyphPage>(maximumTextureSize);
        if (newPage->AddGlyph(context, glyphPixel, textureRect)) {
            Glyph newGlyph = glyph;
            newGlyph.textureRect = textureRect;
            newGlyph.pageIndex = pages.size();
            glyphs[character] = newGlyph;
            pages.push_back(std::move(newPage));
            
            return &glyphs[character];
        }
        
        return nullptr;  // Impossible d'ajouter le glyphe
    }

    Texture2DShared GlyphPages::GetTexture(uint64 character) const {
        auto it = glyphs.find(character);
        if (it != glyphs.end()) {
            uint32 pageIndex = it->second.pageIndex;
            if (pageIndex < pages.size()) {
                auto texture = pages[pageIndex]->texture;
                return texture;
            }
        }
        return nullptr;
    }

    bool GlyphPages::Destroy()
    {
        bool success = true;
        for (auto page : pages) {
            success = page != nullptr ? page->Destroy() && success : success;
            Memory::Reset(page);
            page = nullptr;
        }
        pages.clear();
        return success;
    }

}  //  nkentseu