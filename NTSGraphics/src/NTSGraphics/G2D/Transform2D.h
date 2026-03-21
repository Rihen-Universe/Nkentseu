//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-08-14 at 05:57:39 PM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __NKENTSEU_TRANSFORM2_D_H__
#define __NKENTSEU_TRANSFORM2_D_H__

#pragma once

#include <NTSCore/System.h>

namespace nkentseu {
    
    struct NKENTSEU_API Transform2D {
        
Transform2D();
~Transform2D();

        std::string ToString() const;
        friend std::string ToString(const Transform2D& transform2D);
    };

}  //  nkentseu

#endif  // __NKENTSEU_TRANSFORM2_D_H__!