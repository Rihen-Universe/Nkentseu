//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-08-14 at 06:00:39 PM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __NKENTSEU_RENDER_COMMAND2_D_H__
#define __NKENTSEU_RENDER_COMMAND2_D_H__

#pragma once

#include <NTSCore/System.h>

namespace nkentseu {
    
    struct NKENTSEU_API RenderCommand2D {
        
RenderCommand2D();
~RenderCommand2D();

        std::string ToString() const;
        friend std::string ToString(const RenderCommand2D& renderCommand2D);
    };

}  //  nkentseu

#endif  // __NKENTSEU_RENDER_COMMAND2_D_H__!