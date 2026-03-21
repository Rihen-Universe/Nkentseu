//
// Created by TEUGUIA TADJUIDJE Rodolf S�deris on 2024-07-27 at 09:07:12 AM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __NKENTSEU_GLYPH_H__
#define __NKENTSEU_GLYPH_H__

#pragma once

#include <NTSCore/System.h>
#include <NTSMaths/Shapes/Rectangle.h>
#include "Context.h"
#include "Texture.h"

namespace nkentseu {

    class NKENTSEU_API GlyphNG {
    public:
        char character;
        std::vector<maths::Vector2f> vertices;
        std::vector<uint32> indices;

        GlyphNG(char ch, const std::vector<maths::Vector2f>& verts, const std::vector<uint32>& inds)
            : character(ch), vertices(verts), indices(inds) {}
    };

    class NKENTSEU_API Glyph {
    public:
        maths::Vector2f     sbDelta;                ///< Offset horizontal et vertical apr�s autohint
        maths::Vector2f     bearing;                ///< bearing
        maths::Vector2f     advance;                ///< Offset pour se d�placer horizontalement et verticalement au prochain caract�re
        maths::FloatRect    bounds;                 ///< Rectangle englobant le glyphe, en coordonn�es relatives � la ligne de base
        maths::IntRect      textureRect;            ///< Coordonn�es de la texture du glyphe � l'int�rieur de la texture de la police
        uint32              pageIndex;              ///< Index de la page dans laquelle se trouve le glyphe
    };

    class GlyphPage {
    public:
        uint32              maxTextureSize;         ///< Taille des glyphes dans cette page
        Texture2DShared     texture;                ///< Texture associ�e � la page
        maths::Vector2i     nextPositionWrite;      ///< Position suivante o� sera dessin� le prochain glyphe dans la texture
        uint32              currentRowHeight;       ///< Hauteur de la ligne actuelle

        GlyphPage(uint32 maxTextureSize);
        ~GlyphPage();
        bool AddGlyph(Memory::Shared<Context> context, const uint8* glyphPixel, maths::IntRect& outTextureRect);
        bool IsFull(uint32 maxsize) const;
        bool Destroy();
    };

    class GlyphPages {
    public:
        uint32                                  maximumTextureSize;     ///< Taille maximale d'une texture
        std::vector<Memory::Shared<GlyphPage>>  pages;                  ///< Liste des pages de glyphes
        NkUnorderedMap<uint64, Glyph>       glyphs;                 ///< Tableau de glyphes index� par caract�re Unicode

        GlyphPages(uint32 maxTextureSize);
        Glyph* GetGlyph(uint64 character);
        Glyph* AddGlyph(Memory::Shared<Context> context, uint64 character, const Glyph& glyph, const uint8* glyphPixel);
        Texture2DShared GetTexture(uint64 character) const;
        bool Destroy();
    };

}  // namespace nkentseu

#endif  // __NKENTSEU_GLYPH_H__!