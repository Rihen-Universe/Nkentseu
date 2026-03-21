//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-05-15 at 04:57:08 PM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "pch/ntspch.h"
#include "InternalShader.h"

#ifdef NKENTSEU_GRAPHICS_API_DIRECTX11

#include <Logger/Formatter.h>
#include "InternalShader.h"

namespace nkentseu {
    
    // Constructor
    InternalShader::InternalShader() {
        // Ajoutez votre code de constructeur ici
    }

    // Destructor
    InternalShader::~InternalShader() {
        // Ajoutez votre code de destructeur ici
    }

    std::string InternalShader::ToString() const {
        return FORMATTER.Format(""); // mettez votre formatteur To string entre les guillemets
    }

    std::string ToString(const InternalShader& internalShader) {
        return internalShader.ToString();
    }

}  //  nkentseu

#endif