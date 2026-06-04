#pragma once
#include "Color.h"

namespace NkMath {
        
    // Image intégrale (Summed Area Table) 
    // Construction O(WH), requête rectangulaire O(1) 
    class IntegralImage { 
    public: 
        explicit IntegralImage(const std::vector<uint8_t>& gray, int w, int h) 
            : m_w(w), m_h(h), m_table(w*h, 0) { 
            // Construire la table ligne par ligne 
            for(int y=0; y<h; y++) 
                for(int x=0; x<w; x++) { 
                    int idx = y*w + x; 
                    m_table[idx] = gray[idx] 
                        + (x>0 ? m_table[idx-1]   : 0) 
                        + (y>0 ? m_table[idx-w]   : 0) 
                        - (x>0&&y>0 ? m_table[idx-w-1] : 0); 
                } 
        } 
    
        // Somme du rectangle [x0,y0] → [x1,y1] en O(1) 
        long long RectSum(int x0, int y0, int x1, int y1) const { 
            x0 = std::max(0, x0); 
            y0 = std::max(0, y0); 
            x1 = std::min(m_w - 1, x1); 
            y1 = std::min(m_h - 1, y1); 
            long long s = m_table[y1 * m_w + x1]; 

            if(x0>0) 
                s -= m_table[y1 * m_w + (x0 - 1)]; 

            if(y0>0) 
                s -= m_table[(y0 - 1) * m_w + x1]; 
            
            if(x0 > 0 && y0 > 0) 
                s += m_table[(y0 - 1) * m_w + (x0 - 1)]; 

            return s; 
        } 
    
        int Area(int x0, int y0, int x1, int y1) const { 
            return (x1 - x0 + 1) * (y1 - y0 + 1); 
        } 
    
    private: 
        int m_w, m_h; 
        std::vector<long long> m_table; 
    }; 
    
    // Seuillage adaptatif via image intégrale 
    NkImage AdaptiveThreshold(const NkImage& src, int blockSize=31, int offset=7) { 
        auto gray = src.ToGrayscale(); 
        int w = src.Width(), 
        h = src.Height(); 
        
        IntegralImage ii(gray, w, h); 
        NkImage result(w, h); 
        int half = blockSize / 2; 
        for(int y=0; y<h; y++) 
            for(int x=0; x<w; x++) { 
                long long sum  = ii.RectSum(x-half, y-half, x+half, y+half); 
                int       area = ii.Area(x-half, y-half, x+half, y+half); 
                int       mean = (int)(sum / area); 
                uint8_t   pix  = gray[y*w+x]; 
                uint8_t   out  = (pix < mean - offset) ? 0 : 255; 
                result.SetPixelGray(x, y, out); 
            } 
        return result; 
    } 
}
