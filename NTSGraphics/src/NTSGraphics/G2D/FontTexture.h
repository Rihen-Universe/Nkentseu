//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-08-14 at 05:59:53 PM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __NKENTSEU_FONT_TEXTURE_H__
#define __NKENTSEU_FONT_TEXTURE_H__

#pragma once

#include <NTSCore/System.h>

namespace nkentseu {
    
    struct NKENTSEU_API FontTexture {
        
FontTexture();
~FontTexture();

        std::string ToString() const;
        friend std::string ToString(const FontTexture& fontTexture);
    };

}  //  nkentseu

#endif  // __NKENTSEU_FONT_TEXTURE_H__!