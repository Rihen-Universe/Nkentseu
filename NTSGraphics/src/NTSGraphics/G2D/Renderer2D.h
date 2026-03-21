//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-08-14 at 05:59:21 PM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __NKENTSEU_RENDERER2_D_H__
#define __NKENTSEU_RENDERER2_D_H__

#pragma once

#include <NTSCore/System.h>

namespace nkentseu {
    
    struct NKENTSEU_API Renderer2D {
        
Renderer2D();
~Renderer2D();

        std::string ToString() const;
        friend std::string ToString(const Renderer2D& renderer2D);
    };

}  //  nkentseu

#endif  // __NKENTSEU_RENDERER2_D_H__!