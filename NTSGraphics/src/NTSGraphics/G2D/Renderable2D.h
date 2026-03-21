//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-08-14 at 05:57:54 PM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __NKENTSEU_RENDERABLE2_D_H__
#define __NKENTSEU_RENDERABLE2_D_H__

#pragma once

#include <NTSCore/System.h>

namespace nkentseu {
    
    struct NKENTSEU_API Renderable2D {
        
Renderable2D();
~Renderable2D();

        std::string ToString() const;
        friend std::string ToString(const Renderable2D& renderable2D);
    };

}  //  nkentseu

#endif  // __NKENTSEU_RENDERABLE2_D_H__!