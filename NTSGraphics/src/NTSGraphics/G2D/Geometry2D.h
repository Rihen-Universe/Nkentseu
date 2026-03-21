//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-07-16 at 11:45:38 AM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __GEOMETRY2_D_H__
#define __GEOMETRY2_D_H__

#pragma once

#include <NTSCore/System.h>
#include "Vertex2D.h"

namespace nkentseu {

    struct NKENTSEU_API Geometry2D {
        std::vector<Vertex2D> vertices; // Vertex de la géométrie 2D
        std::vector<uint32> indices; // Indices de la géométrie 2D
    };

}  //  nkentseu

#endif  // __GEOMETRY2_D_H__!