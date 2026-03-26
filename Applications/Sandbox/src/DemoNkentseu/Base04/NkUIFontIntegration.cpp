/**
 * @File    NkUIFontIntegration.cpp
 * @Brief   Implémentation de l'intégration NKFont avec NkUI
 */

#include "NkUIFontIntegration.h"
#include "NKMemory/NkFunction.h"
#include "NKLogger/NkLog.h"

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include <cstdio>
#include <cstring>
#include <cctype>

namespace nkentseu {
    namespace nkui {

        // ============================================================================
        //  NkUIFontLoader - Initialisation et destruction
        // ============================================================================

        bool NkUIFontLoader::Init(const NkUIFontConfig& config,
                                usize permanentBytes,
                                usize scratchBytes,
                                int32 atlasWidth,
                                int32 atlasHeight,
                                void* gpuDevice,
                                NkGPUUploadFunc uploadFunc) noexcept
        {
            if (mInitialized) return true;
            
            mConfig = config;
            mGPUDevice = gpuDevice;
            mGPUUploadFunc = uploadFunc;
            
            // Initialiser NKFont
            if (!mFontLib.Init(permanentBytes, scratchBytes, atlasWidth, atlasHeight)) {
                logger.Errorf("[NkUI] Failed to initialize NKFont library\n");
                return false;
            }
            
            // Créer l'atlas NkUI
            if (!CreateAtlas(atlasWidth, atlasHeight)) {
                logger.Errorf("[NkUI] Failed to create font atlas\n");
                return false;
            }
            
            mInitialized = true;
            logger.Infof("[NkUI] Font loader initialized (atlas=%dx%d, GPU=%s)\n", 
                        atlasWidth, atlasHeight, mGPUDevice ? "yes" : "no");
            
            return true;
        }

        void NkUIFontLoader::Destroy() noexcept {
            if (!mInitialized) return;
            
            // Nettoyer les polices
            for (uint32 i = 0; i < mNumFonts; ++i) {
                if (mFonts[i].nkFace) {
                    mFonts[i].nkFace = nullptr;
                }
                if (mFonts[i].ftFace) {
                    mFontLib.FreeFontFT(mFonts[i].ftFace);
                    mFonts[i].ftFace = nullptr;
                }
                mFonts[i].isValid = false;
            }
            mNumFonts = 0;
            
            // Nettoyer l'atlas
            if (mAtlas) {
                mAtlas->Clear();
                delete mAtlas;
                mAtlas = nullptr;
            }
            
            // Détruire NKFont
            mFontLib.Destroy();
            
            mInitialized = false;
            logger.Infof("[NkUI] Font loader destroyed\n");
        }

        bool NkUIFontLoader::CreateAtlas(int32 width, int32 height) noexcept {
            mAtlas = new NkUIFontAtlas();
            if (!mAtlas) return false;
            
            mAtlas->Clear();
            mAtlas->yAxisUp = mConfig.yAxisUp;
            return true;
        }

        // ============================================================================
        //  NkUIFontLoader - Chargement des polices
        // ============================================================================

        uint32 NkUIFontLoader::LoadFontFromFile(const char* filepath,
                                                float32 fontSize,
                                                bool preloadASCII) noexcept
        {
            if (!mInitialized || !filepath || mNumFonts >= MAX_LOADED_FONTS) {
                return 0;
            }
            
            logger.Infof("[NkUI] Loading font from file: %s (size=%.1f)\n", filepath, fontSize);
            
            // Vérifier si le fichier existe
            FILE* testFile = fopen(filepath, "rb");
            if (!testFile) {
                logger.Errorf("[NkUI] Font file not found: %s\n", filepath);
                return 0;
            }
            fclose(testFile);
            
            // Lire le fichier en mémoire
            uint8* fileData = nullptr;
            usize fileSize = 0;
            
            FILE* f = fopen(filepath, "rb");
            if (!f) return 0;
            
            fseek(f, 0, SEEK_END);
            fileSize = ftell(f);
            fseek(f, 0, SEEK_SET);
            fileData = new uint8[fileSize];
            fread(fileData, 1, fileSize, f);
            fclose(f);
            
            // Essayer d'abord avec NKFont
            NkFontFace* face = mFontLib.LoadFont(fileData, fileSize, static_cast<uint16>(fontSize));
            
            LoadedFont& lf = mFonts[mNumFonts];
            lf.isValid = false;
            
            if (face) {
                // Succès avec NKFont
                lf.nkFace = face;
                lf.ftFace = nullptr;
                
                const char* familyName = face->GetFamilyName();
                if (familyName && familyName[0]) {
                    memory::NkCopy(lf.name, familyName, sizeof(lf.name));
                } else {
                    // Extraire le nom du fichier
                    const char* filename = strrchr(filepath, '\\');
                    if (!filename) filename = strrchr(filepath, '/');
                    if (!filename) filename = filepath;
                    else filename++;
                    memory::NkCopy(lf.name, filename, sizeof(lf.name));
                    char* dot = strrchr(lf.name, '.');
                    if (dot) *dot = 0;
                }
                
                // Configurer la police NkUI
                lf.uiFont.atlas = mAtlas;
                lf.uiFont.size = fontSize;
                lf.uiFont.isBuiltin = false;
                lf.uiFont.config = mConfig;
                memory::NkCopy(lf.uiFont.name, lf.name, sizeof(lf.uiFont.name));
                
                // Calculer les métriques
                ComputeMetricsFromNKFont(face, lf.metrics);
                lf.uiFont.metrics = lf.metrics;
                
                lf.isValid = true;
                
                if (preloadASCII) {
                    PreloadASCIIFromNKFont(face);
                }
                
                logger.Infof("[NkUI] Font loaded via NKFont: %s\n", lf.name);
            } else {
                // NKFont a échoué, essayer avec FreeType
                logger.Warnf("[NkUI] NKFont failed, trying FreeType backend...\n");
                
                NkFTFontFace* ftFace = mFontLib.LoadFontFT(fileData, fileSize, static_cast<uint16>(fontSize));
                if (ftFace) {
                    logger.Infof("[NkUI] FreeType loaded font successfully\n");
                    
                    lf.nkFace = nullptr;
                    lf.ftFace = ftFace;
                    
                    // Extraire le nom du fichier
                    const char* filename = strrchr(filepath, '\\');
                    if (!filename) filename = strrchr(filepath, '/');
                    if (!filename) filename = filepath;
                    else filename++;
                    memory::NkCopy(lf.name, filename, sizeof(lf.name));
                    char* dot = strrchr(lf.name, '.');
                    if (dot) *dot = 0;
                    
                    // Configurer la police NkUI
                    lf.uiFont.atlas = mAtlas;
                    lf.uiFont.size = fontSize;
                    lf.uiFont.isBuiltin = false;
                    lf.uiFont.config = mConfig;
                    memory::NkCopy(lf.uiFont.name, lf.name, sizeof(lf.uiFont.name));
                    
                    // Calculer les métriques depuis FreeType
                    ComputeMetricsFromFTFont(ftFace, lf.metrics);
                    lf.uiFont.metrics = lf.metrics;
                    
                    lf.isValid = true;
                    
                    if (preloadASCII) {
                        PreloadASCIIFromFTFont(ftFace);
                    }
                    
                    logger.Infof("[NkUI] Font loaded via FreeType: %s\n", lf.name);
                } else {
                    logger.Errorf("[NkUI] Both NKFont and FreeType failed to load font: %s\n", filepath);
                    delete[] fileData;
                    return 0;
                }
            }
            
            delete[] fileData;
            
            if (lf.isValid) {
                // Marquer l'atlas comme sale pour upload
                if (mAtlas) {
                    mAtlas->dirty = true;
                    
                    // GÉNÉRER UN TEXID UNIQUE POUR CET ATLAS
                    // Dans une vraie application, vous devriez créer une texture GPU ici
                    // Pour l'instant, on utilise un ID fixe non nul
                    static uint32 nextTexId = 1;
                    if (mAtlas->texId == 0) {
                        mAtlas->texId = nextTexId++;
                    }
                }
                return mNumFonts++;
            }
            
            return 0;
        }

        // ============================================================================
        //  NkUIFontLoader - Méthodes privées pour NKFont
        // ============================================================================

        void NkUIFontLoader::ComputeMetricsFromNKFont(NkFontFace* face, NkUIFontMetrics& outMetrics) noexcept {
            if (!face) return;
            
            outMetrics.ascender = static_cast<float32>(face->GetAscender());
            outMetrics.descender = static_cast<float32>(face->GetDescender());
            outMetrics.lineHeight = static_cast<float32>(face->GetLineHeight());
            outMetrics.spaceWidth = outMetrics.ascender * 0.35f;
            outMetrics.tabWidth = outMetrics.spaceWidth * 4;
            outMetrics.lineGap = outMetrics.lineHeight * 0.2f;
        }

        void NkUIFontLoader::PreloadASCIIFromNKFont(NkFontFace* face) noexcept {
            if (!face || !mAtlas) return;
            
            logger.Infof("[NkUI] Preloading ASCII glyphs via NKFont...\n");
            int32 loaded = 0;
            
            for (uint32 cp = 32; cp <= 126; ++cp) {
                NkUIGlyph glyph;
                if (RasterizeGlyphToAtlas(face, cp, glyph)) {
                    loaded++;
                }
            }
            
            mAtlas->dirty = true;
            logger.Infof("[NkUI] Preloaded %d ASCII glyphs\n", loaded);
        }

        bool NkUIFontLoader::RasterizeGlyphToAtlas(NkFontFace* face, uint32 codepoint, NkUIGlyph& outGlyph) noexcept {
            if (!face || !mAtlas) return false;
            
            NkGlyph nkGlyph;
            if (!face->GetGlyph(codepoint, nkGlyph)) {
                return false;
            }
            
            if (nkGlyph.isEmpty) {
                return false;
            }
            
            int32 x, y;
            if (!mAtlas->Alloc(nkGlyph.metrics.width, nkGlyph.metrics.height, x, y)) {
                logger.Warnf("[NkUI] Atlas full, cannot add glyph U+%04X\n", codepoint);
                return false;
            }
            
            const uint8* src = nkGlyph.bitmap.pixels;
            const int32 srcStride = nkGlyph.bitmap.stride;
            const int32 width = nkGlyph.metrics.width;
            const int32 height = nkGlyph.metrics.height;
            
            for (int32 row = 0; row < height; ++row) {
                memory::NkCopy(
                    mAtlas->pixels + (y + row) * NkUIFontAtlas::ATLAS_W + x,
                    src + row * srcStride,
                    width
                );
            }
            
            mAtlas->AddGlyph(
                codepoint, x, y, width, height,
                nkGlyph.metrics.xAdvance.ToFloat(),
                static_cast<float32>(nkGlyph.metrics.bearingX),
                static_cast<float32>(nkGlyph.metrics.bearingY)
            );
            
            return true;
        }

        // ============================================================================
        //  NkUIFontLoader - Méthodes privées pour FreeType
        // ============================================================================

        void NkUIFontLoader::ComputeMetricsFromFTFont(NkFTFontFace* face, NkUIFontMetrics& outMetrics) noexcept {
            if (!face) return;
            
            outMetrics.ascender = static_cast<float32>(face->GetAscender());
            outMetrics.descender = static_cast<float32>(face->GetDescender());
            outMetrics.lineHeight = static_cast<float32>(face->GetLineHeight());
            outMetrics.spaceWidth = outMetrics.ascender * 0.35f;
            outMetrics.tabWidth = outMetrics.spaceWidth * 4;
            outMetrics.lineGap = outMetrics.lineHeight * 0.2f;
        }

        void NkUIFontLoader::PreloadASCIIFromFTFont(NkFTFontFace* face) noexcept {
            if (!face || !mAtlas) return;
            
            logger.Infof("[NkUI] Preloading ASCII glyphs via FreeType...\n");
            int32 loaded = 0;
            
            for (uint32 cp = 32; cp <= 126; ++cp) {
                NkUIGlyph glyph;
                if (RasterizeGlyphToAtlas(face, cp, glyph)) {
                    loaded++;
                }
            }
            
            mAtlas->dirty = true;
            logger.Infof("[NkUI] Preloaded %d ASCII glyphs\n", loaded);
        }

        bool NkUIFontLoader::RasterizeGlyphToAtlas(NkFTFontFace* face, uint32 codepoint, NkUIGlyph& outGlyph) noexcept {
            if (!face || !mAtlas) return false;
            
            NkFTGlyph ftGlyph;
            if (!face->GetGlyph(codepoint, ftGlyph) || !ftGlyph.valid) {
                return false;
            }
            
            if (ftGlyph.isEmpty) {
                return false;
            }
            
            logger.Infof("[NkUI] Rasterizing glyph U+%04X: %dx%d, bearing=(%d,%d), advance=%d\n",
                        codepoint, ftGlyph.width, ftGlyph.height,
                        ftGlyph.bearingX, ftGlyph.bearingY, ftGlyph.xAdvance);
            
            int32 x, y;
            if (!mAtlas->Alloc(ftGlyph.width, ftGlyph.height, x, y)) {
                logger.Warnf("[NkUI] Atlas full, cannot add glyph U+%04X\n", codepoint);
                return false;
            }
            
            const uint8* src = ftGlyph.bitmap;
            const int32 srcStride = ftGlyph.pitch;
            const int32 width = ftGlyph.width;
            const int32 height = ftGlyph.height;
            
            // Vérifier que src n'est pas nul
            if (!src) {
                logger.Errorf("[NkUI] Null bitmap for glyph U+%04X\n", codepoint);
                return false;
            }
            
            // Copier les pixels dans l'atlas
            for (int32 row = 0; row < height; ++row) {
                const uint8* srcRow = src + row * srcStride;
                uint8* dstRow = mAtlas->pixels + (y + row) * NkUIFontAtlas::ATLAS_W + x;
                memory::NkCopy(dstRow, srcRow, width);
            }
            
            // Ajouter le glyphe à l'atlas
            mAtlas->AddGlyph(
                codepoint, x, y, width, height,
                static_cast<float32>(ftGlyph.xAdvance),
                static_cast<float32>(ftGlyph.bearingX),
                static_cast<float32>(ftGlyph.bearingY)
            );
            
            // DEBUG: Vérifier que le glyphe a été ajouté
            const NkUIGlyph* checkGlyph = mAtlas->Find(codepoint);
            if (checkGlyph) {
                logger.Infof("[NkUI] Glyph U+%04X added to atlas at (%d,%d) size=%dx%d\n",
                            codepoint, x, y, width, height);
            }
            
            return true;
        }

        // ============================================================================
        //  NkUIFontLoader - Autres méthodes
        // ============================================================================

        uint32 NkUIFontLoader::LoadSystemFont(const char* fontName,
                                            float32 fontSize,
                                            bool preloadASCII) noexcept
        {
            if (!mInitialized || !fontName || mNumFonts >= MAX_LOADED_FONTS) {
                return 0;
            }
            
            logger.Infof("[NkUI] Loading system font: %s (size=%.1f)\n", fontName, fontSize);
            
        #ifdef _WIN32
            const char* systemPaths[] = {
                "C:\\Windows\\Fonts\\",
                nullptr
            };
        #else
            const char* systemPaths[] = {
                "/usr/share/fonts/",
                "/usr/local/share/fonts/",
                nullptr
            };
        #endif
            
            char fullPath[512];
            
            // Créer une version sans espaces
            char noSpaceName[256];
            strncpy(noSpaceName, fontName, sizeof(noSpaceName) - 1);
            noSpaceName[sizeof(noSpaceName) - 1] = 0;
            for (char* c = noSpaceName; *c; ++c) {
                if (*c == ' ') *c = '-';
            }
            
            // Créer une version en minuscules
            char lowerName[256];
            strncpy(lowerName, fontName, sizeof(lowerName) - 1);
            lowerName[sizeof(lowerName) - 1] = 0;
            for (char* c = lowerName; *c; ++c) {
                *c = tolower(*c);
            }
            
            const char* namesToTry[] = {
                fontName,
                noSpaceName,
                lowerName,
                nullptr
            };
            
            const char* extensions[] = { ".ttf", ".otf", ".ttc", nullptr };
            
            for (int p = 0; systemPaths[p]; ++p) {
                for (int n = 0; namesToTry[n]; ++n) {
                    for (int e = 0; extensions[e]; ++e) {
                        snprintf(fullPath, sizeof(fullPath), "%s%s%s", 
                                systemPaths[p], namesToTry[n], extensions[e]);
                        
                        uint32 idx = LoadFontFromFile(fullPath, fontSize, preloadASCII);
                        if (idx != 0) {
                            logger.Infof("[NkUI] System font loaded: %s\n", fullPath);
                            return idx;
                        }
                    }
                }
            }
            
            logger.Errorf("[NkUI] Failed to load system font: %s\n", fontName);
            return 0;
        }

        void NkUIFontLoader::UploadAtlas() noexcept {
            if (!mAtlas || !mAtlas->dirty) return;
            
            // Vérifier que le device GPU est défini
            if (!mGPUDevice) {
                logger.Warnf("[NkUI] No GPU device set, atlas not uploaded\n");
                return;
            }
            
            if (mGPUUploadFunc) {
                // Utiliser le callback RHI
                logger.Infof("[NkUI] Uploading atlas via callback, texId=%u\n", mAtlas->texId);
                
                // Convertir en RGBA8 si demandé
                uint8* uploadData = nullptr;
                int32 dataSize = 0;
                
                if (uploadRGBA8) {
                    // Convertir R8 → RGBA8
                    const int32 size = NkUIFontAtlas::ATLAS_W * NkUIFontAtlas::ATLAS_H;
                    uploadData = new uint8[size * 4];
                    for (int32 i = 0; i < size; ++i) {
                        uint8 alpha = mAtlas->pixels[i];
                        uploadData[i * 4 + 0] = 255;  // R
                        uploadData[i * 4 + 1] = 255;  // G
                        uploadData[i * 4 + 2] = 255;  // B
                        uploadData[i * 4 + 3] = alpha; // A
                    }
                    dataSize = size * 4;
                    logger.Infof("[NkUI] Converted atlas to RGBA8 (%d bytes)\n", dataSize);
                } else {
                    uploadData = mAtlas->pixels;
                    dataSize = NkUIFontAtlas::ATLAS_W * NkUIFontAtlas::ATLAS_H;
                }
                
                // Appeler le callback
                bool success = mGPUUploadFunc(mGPUDevice, mAtlas->texId, uploadData, 
                                            NkUIFontAtlas::ATLAS_W, NkUIFontAtlas::ATLAS_H);
                
                if (uploadRGBA8) {
                    delete[] uploadData;
                }
                
                if (success) {
                    mAtlas->dirty = false;
                    logger.Infof("[NkUI] Atlas uploaded via RHI, texId=%u\n", mAtlas->texId);
                } else {
                    logger.Errorf("[NkUI] Failed to upload atlas via RHI\n");
                }
            } else {
                logger.Warnf("[NkUI] No GPU upload function set, atlas not uploaded\n");
            }
        }

        NkUIFont* NkUIFontLoader::GetFont(uint32 idx) noexcept {
            if (idx < mNumFonts && mFonts[idx].isValid) {
                return &mFonts[idx].uiFont;
            }
            return nullptr;
        }

        NkUIFont* NkUIFontLoader::DefaultFont() noexcept {
            for (uint32 i = 0; i < mNumFonts; ++i) {
                if (mFonts[i].isValid) {
                    return &mFonts[i].uiFont;
                }
            }
            return nullptr;
        }

    } // namespace nkui
} // namespace nkentseu
