//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-08-14 at 06:00:39 PM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "pch/ntspch.h"
#include "RenderCommand2D.h"

namespace nkentseu {
    
    // Constructor
    RenderCommand2D::RenderCommand2D() {
        // Ajoutez votre code de constructeur ici
    }

    // Destructor
    RenderCommand2D::~RenderCommand2D() {
        // Ajoutez votre code de destructeur ici
    }

    std::string RenderCommand2D::ToString() const {
        return ""; // mettez votre formatteur To string entre les guillemets
    }

    std::string ToString(const RenderCommand2D& renderCommand2D) {
        return renderCommand2D.ToString();
    }

}  //  nkentseu