#pragma once 
#include <cstdint> 
#include <vector> 
#include <cassert> 
#include <cstring> 
#include <string> 
#include <filesystem>
 
namespace NkMath { 
 
    class NkImage { 
    public: 
        // Constructeurs 
        NkImage() : m_width(0), m_height(0) {} 
        NkImage(int w, int h) : m_width(w), m_height(h), m_data(w*h*4, 0) {} 
    
        // Accesseur bounds-checked en debug 
        uint8_t* At(int x, int y) { 
            if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
                return nullptr;
            }
            return m_data.data() + (y * m_width + x) * 4; 
        } 

        const uint8_t* At(int x, int y) const { 
            if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
                return nullptr;
            }
            return m_data.data() + (y * m_width + x) * 4; 
        } 
    
        // Setters pratiques 
        void SetPixelRGBA(int x, int y, uint8_t r, uint8_t g, uint8_t b, 
            uint8_t a=255) 
        { 
            uint8_t* p = At(x,y); 
            if (p) {
                p[0]=r; p[1]=g; p[2]=b; p[3]=a; 
            }
        } 

        void SetPixelGray(int x, int y, uint8_t v) { 
            SetPixelRGBA(x,y,v,v,v,255); 
        } 
    
        // Dimensions 
        int Width()  const { return m_width; } 
        int Height() const { return m_height; } 
    
        // Pointer direct vers les données (pour glTexImage2D) 
        const uint8_t* Data() const { return m_data.data(); } 
        uint8_t*       Data()       { return m_data.data(); } 
        size_t         DataSize() const { return m_data.size(); } 
    
        // Remplissage 
        void Fill(uint8_t r, uint8_t g, uint8_t b, uint8_t a=255){
            for(int y = 0; y < m_height; y++) 
                for(int x = 0; x < m_width; x++) 
                    SetPixelRGBA(x, y, r, g, b, a); 
        }
    
        // Sauvegarde PPM (format simple sans dépendance) 
        bool SavePPM(const std::string& path) const{ 
            std::string fullPath = "generatedImages/" + path;

            // Crée le dossier s'il n'existe pas
            std::filesystem::create_directories("generatedImages");

            // Ensuite tu peux ouvrir ton fichier
            FILE* f = fopen(fullPath.c_str(), "wb");
            
            if(!f) 
                return false; 

            fprintf(f, "P6\n%d %d\n255\n", m_width, m_height); 

            // PPM P6 = RGB sans alpha 
            for(int y = 0; y < m_height; y++) 
                for(int x = 0; x < m_width; x++) { 
                    const uint8_t* p = At(x,y); 
                    if (p) {
                        fwrite(p, 1, 3, f);  // R, G, B seulement (pas A) 
                    } else {
                        uint8_t black[3] = {0, 0, 0};
                        fwrite(black, 1, 3, f);
                    }
                } 

            fclose(f); 
            return true; 
        } 
        
        bool LoadPPM(const std::string& path){ 
            FILE* f = fopen(path.c_str(), "rb"); 

            if(!f) 
                return false; 

            char magic[3]; 
            int w, h, maxVal; 
            fscanf(f, "%s %d %d %d ", magic, &w, &h, &maxVal); 
            
            if(magic[0]!='P' || magic[1]!='6') { 
                fclose(f); 
                return false; 
            } 

            *this = NkImage(w, h); 
            for(int y=0; y<h; y++) 
                for(int x=0; x<w; x++) { 
                    uint8_t rgb[3]; 
                    fread(rgb, 1, 3, f); 
                    SetPixelRGBA(x, y, rgb[0], rgb[1], rgb[2], 255); 
                } 

            fclose(f); 
            return true; 
        }
    
        // Conversion niveaux de gris (vers buffer séparé pour le pipeline AR) 
        std::vector<uint8_t> ToGrayscale() const { 
            std::vector<uint8_t> gray(m_width * m_height); 
            for(int y=0; y<m_height; y++) 
                for(int x=0; x<m_width; x++) { 
                    const uint8_t* p = At(x,y); 
                    if (p) {
                        // moyenne simple des canaux R, G, B (ignorer alpha) 
                        gray[y * m_width + x] = (p[0] + p[1] + p[2]) / 3; 
                    } else {
                        gray[y * m_width + x] = 0; // ou une autre valeur par défaut
                    }
                } 
            return gray; 
        }

        NkImage Convolve(const std::vector<double>& kernel, int kSize){
            int half = kSize/2;
            NkImage out(m_width, m_height);

            for(int y = 0; y < m_height; y++){
                for(int x = 0; x < m_width; x++){

                    double sum[4] = {0, 0, 0, 0};

                    for(int ky = -half; ky <= half; ky++){
                        for(int kx = -half; kx <= half; kx++){
                            int ix = std::clamp(x + kx, 0, m_width - 1);
                            int iy = std::clamp(y + ky, 0, m_height - 1);

                            double k = kernel[(ky + half) * kSize + (kx + half)];

                            const uint8_t* p = At(ix, iy);

                            if (p){
                                sum[0] += p[0] * k;
                                sum[1] += p[1] * k;
                                sum[2] += p[2] * k;
                                sum[3] += p[3] * k;
                            }
                        }
                    }

                    out.SetPixelRGBA(
                        x,y,
                        (uint8_t)std::clamp(sum[0], 0.0, 255.0),
                        (uint8_t)std::clamp(sum[1], 0.0, 255.0),
                        (uint8_t)std::clamp(sum[2], 0.0, 255.0),
                        (uint8_t)std::clamp(sum[3], 0.0, 255.0)
                    );
                }
            }

            return out;
        }

        void DrawLine(int x0, int y0, int x1, int y1, unsigned char r = 0, 
            unsigned char g = 0, unsigned char b = 0, unsigned char a = 255) 
        {
            int dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0);
            int sx = x0 < x1 ? 1 : -1;
            int sy = y0 < y1 ? 1 : -1;
            int err = dx + dy;
            while(true) {
                SetPixelRGBA(x0, y0, r, g, b, a);
                if(x0 == x1 && y0 == y1) break;
                int e2 = 2 * err;
                if(e2 >= dy) { err += dy; x0 += sx; }
                if(e2 <= dx) { err += dx; y0 += sy; }
            }
        }

        static NkImage CombineGradient(const NkImage& gx, const NkImage& gy){
            assert(gx.Width() == gy.Width());
            assert(gx.Height() == gy.Height());

            int w = gx.Width();
            int h = gx.Height();

            NkImage out(w,h);

            for(int y = 0; y < h; y++){
                for(int x = 0; x < w; x++){
                    const uint8_t* px = gx.At(x,y);
                    const uint8_t* py = gy.At(x,y);

                    if (!px || !py) {
                        out.SetPixelGray(x, y, 0);
                        continue;
                    }

                    double mag = std::sqrt(
                        px[0]*px[0] + py[0]*py[0]
                    );

                    uint8_t v = (uint8_t)std::clamp(mag, 0.0, 255.0);
                    out.SetPixelGray(x, y, v);
                }
            }
            return out;
        }

    private: 
        int m_width, m_height; 
        std::vector<uint8_t> m_data;  // layout RGBA contigu 
    }; 
    
} // namespace NkMath
