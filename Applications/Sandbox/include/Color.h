#pragma once
#include "NKImage.h"


namespace NkMath { 
    // Accès sub-pixel avec interpolation bilinéaire 
    // Retourne la couleur interpolée à la position flottante (fx, fy) 
    struct Color4 { double r, g, b, a; }; 
    
    Color4 SampleBilinear(const NkImage& img, double fx, double fy) { 
        // Coordonnées des 4 pixels voisins 
        int x0 = (int)fx; 
        int y0 = (int)fy;

        int x1 = std::min(x0 + 1, img.Width() - 1); 
        int y1 = std::min(y0 + 1, img.Height() - 1); 
        x0 = std::max(0, x0); 
        y0 = std::max(0, y0);
    
        // Fractions sub-pixel 
        double tx = fx - (int)fx;  // [0, 1) 
        double ty = fy - (int)fy; 
    
        // Les 4 pixels voisins 
        const uint8_t* p00 = img.At(x0, y0); 
        const uint8_t* p10 = img.At(x1, y0); 
        const uint8_t* p01 = img.At(x0, y1); 
        const uint8_t* p11 = img.At(x1, y1); 

        if (!p00 || !p10 || !p01 || !p11) {
            return {0, 0, 0, 255}; // ou une autre couleur par défaut
        }
    
        // Interpolation bilinéaire pour chaque canal 
        Color4 c; 
        for(int i=0; i<4; i++) { 
            double v00 = p00[i], v10 = p10[i]; 
            double v01 = p01[i], v11 = p11[i]; 

            // Interpoler horizontalement, puis verticalement 
            double top    = v00 * (1 - tx) + v10 * tx; 
            double bottom = v01 * (1 - tx) + v11 * tx; 
            (&c.r)[i] = top * (1 - ty) + bottom * ty; 
        } 
        return c; 
    }
}
