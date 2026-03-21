//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-08-15 at 12:16:43 PM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __NKENTSEU_FONT_GLYPH_H__
#define __NKENTSEU_FONT_GLYPH_H__

#pragma once

#include <NTSCore/System.h>

namespace nkentseu {
    
    struct NKENTSEU_API FontGlyph {
        
FontGlyph();
~FontGlyph();

        std::string ToString() const;
        friend std::string ToString(const FontGlyph& fontGlyph);
    };

}  //  nkentseu

#endif  // __NKENTSEU_FONT_GLYPH_H__!