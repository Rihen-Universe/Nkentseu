//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-08-14 at 05:56:56 PM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __NKENTSEU_VERTEX_ARRAY2_D_H__
#define __NKENTSEU_VERTEX_ARRAY2_D_H__

#pragma once

#include <NTSCore/System.h>

namespace nkentseu {
    
    struct NKENTSEU_API VertexArray2D {
        
VertexArray2D();
~VertexArray2D();

        std::string ToString() const;
        friend std::string ToString(const VertexArray2D& vertexArray2D);
    };

}  //  nkentseu

#endif  // __NKENTSEU_VERTEX_ARRAY2_D_H__!