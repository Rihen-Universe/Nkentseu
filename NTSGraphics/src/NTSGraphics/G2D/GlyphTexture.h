//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-08-15 at 05:43:12 AM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __NKENTSEU_GLYPH_TEXTURE_H__
#define __NKENTSEU_GLYPH_TEXTURE_H__

#pragma once

#include <NTSCore/System.h>

namespace nkentseu {
    
    struct NKENTSEU_API GlyphTexture {
        
GlyphTexture();
~GlyphTexture();

        std::string ToString() const;
        friend std::string ToString(const GlyphTexture& glyphTexture);
    };

}  //  nkentseu

#endif  // __NKENTSEU_GLYPH_TEXTURE_H__!