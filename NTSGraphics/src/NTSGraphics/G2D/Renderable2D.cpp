//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-08-14 at 05:57:54 PM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "pch/ntspch.h"
#include "Renderable2D.h"

namespace nkentseu {
    
    // Constructor
    Renderable2D::Renderable2D() {
        // Ajoutez votre code de constructeur ici
    }

    // Destructor
    Renderable2D::~Renderable2D() {
        // Ajoutez votre code de destructeur ici
    }

    std::string Renderable2D::ToString() const {
        return ""; // mettez votre formatteur To string entre les guillemets
    }

    std::string ToString(const Renderable2D& renderable2D) {
        return renderable2D.ToString();
    }

}  //  nkentseu