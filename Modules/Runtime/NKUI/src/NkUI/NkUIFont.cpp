/**
 * @File    NkUIFont.cpp
 * @Brief   Implémentation du système de polices NkUI
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 * 
 * Ce fichier contient l'implémentation de :
 * - Police bitmap intégrée (6x10 pixels, ASCII 32-127)
 * - Gestion de l'atlas texture avec shelf packer
 * - Mesure et rendu de texte (supporte UTF-8)
 * - Fallback intelligent entre atlas et bitmap
 */

/*
 * NKUI_MAINTENANCE_GUIDE
 * Responsibility: Font rendering implementation and atlas integration.
 * Main data: Glyph lookup, text quad emission, atlas upload paths.
 * Change this file when: Text alignment, unicode support, or atlas behavior changes.
 */
#include "NKUI/NkUIFont.h"
#include "NKMemory/NkFunction.h"
#include "NKLogger/NkLog.h"

#include "NkUIFontBridge.h"
// #include "NKFont/NkFont.h"

namespace nkentseu {
    namespace nkui {

        static int32 NkDecodeUTF8Codepoint(const char* s, uint32& cp) noexcept {
            if (!s || !s[0]) {
                cp = 0;
                return 0;
            }
            const uint8 c0 = static_cast<uint8>(s[0]);
            if (c0 < 0x80) {
                cp = c0;
                return 1;
            }
            if ((c0 & 0xE0) == 0xC0 && s[1]) {
                cp = ((c0 & 0x1Fu) << 6) | (static_cast<uint8>(s[1]) & 0x3Fu);
                return 2;
            }
            if ((c0 & 0xF0) == 0xE0 && s[1] && s[2]) {
                cp = ((c0 & 0x0Fu) << 12) |
                     ((static_cast<uint8>(s[1]) & 0x3Fu) << 6) |
                     (static_cast<uint8>(s[2]) & 0x3Fu);
                return 3;
            }
            if ((c0 & 0xF8) == 0xF0 && s[1] && s[2] && s[3]) {
                cp = ((c0 & 0x07u) << 18) |
                     ((static_cast<uint8>(s[1]) & 0x3Fu) << 12) |
                     ((static_cast<uint8>(s[2]) & 0x3Fu) << 6) |
                     (static_cast<uint8>(s[3]) & 0x3Fu);
                return 4;
            }
            cp = '?';
            return 1;
        }

        // ============================================================================
        // Police bitmap intégrée (6x10 pixels, ASCII 32-127)
        // ============================================================================
        // 
        // Encodage : chaque octet représente une ligne de 6 pixels (bits 0-5)
        // bit0 = colonne 0 (gauche), bit5 = colonne 5 (droite)
        // 
        // Les données sont stockées avec bit0 à gauche, mais notre rendu
        // inverse les bits pour corriger l'orientation (voir Reverse6Bits)

        const uint8 NkUIFont::kBitmapFont[96 * 10] = {
            // ' ' (32)
            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            // '!' (33)
            0x04,0x04,0x04,0x04,0x04,0x00,0x04,0x00,0x00,0x00,
            // '"' (34)
            0x0A,0x0A,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            // '#' (35)
            0x0A,0x0A,0x1F,0x0A,0x1F,0x0A,0x0A,0x00,0x00,0x00,
            // '$' (36)
            0x04,0x0F,0x14,0x0E,0x05,0x1E,0x04,0x00,0x00,0x00,
            // '%' (37)
            0x18,0x19,0x02,0x04,0x08,0x13,0x03,0x00,0x00,0x00,
            // '&' (38)
            0x08,0x14,0x14,0x08,0x15,0x12,0x0D,0x00,0x00,0x00,
            // '\'' (39)
            0x04,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            // '(' (40)
            0x02,0x04,0x08,0x08,0x08,0x04,0x02,0x00,0x00,0x00,
            // ')' (41)
            0x08,0x04,0x02,0x02,0x02,0x04,0x08,0x00,0x00,0x00,
            // '*' (42)
            0x00,0x04,0x15,0x0E,0x15,0x04,0x00,0x00,0x00,0x00,
            // '+' (43)
            0x00,0x04,0x04,0x1F,0x04,0x04,0x00,0x00,0x00,0x00,
            // ',' (44)
            0x00,0x00,0x00,0x00,0x00,0x04,0x08,0x00,0x00,0x00,
            // '-' (45)
            0x00,0x00,0x00,0x1F,0x00,0x00,0x00,0x00,0x00,0x00,
            // '.' (46)
            0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x00,0x00,0x00,
            // '/' (47)
            0x01,0x01,0x02,0x04,0x08,0x10,0x10,0x00,0x00,0x00,
            // '0' (48)
            0x0E,0x11,0x13,0x15,0x19,0x11,0x0E,0x00,0x00,0x00,
            // '1' (49)
            0x04,0x0C,0x04,0x04,0x04,0x04,0x0E,0x00,0x00,0x00,
            // '2' (50)
            0x0E,0x11,0x01,0x02,0x04,0x08,0x1F,0x00,0x00,0x00,
            // '3' (51)
            0x1F,0x02,0x04,0x02,0x01,0x11,0x0E,0x00,0x00,0x00,
            // '4' (52)
            0x02,0x06,0x0A,0x12,0x1F,0x02,0x02,0x00,0x00,0x00,
            // '5' (53)
            0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E,0x00,0x00,0x00,
            // '6' (54)
            0x06,0x08,0x10,0x1E,0x11,0x11,0x0E,0x00,0x00,0x00,
            // '7' (55)
            0x1F,0x01,0x02,0x04,0x04,0x04,0x04,0x00,0x00,0x00,
            // '8' (56)
            0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E,0x00,0x00,0x00,
            // '9' (57)
            0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C,0x00,0x00,0x00,
            // ':' (58)
            0x00,0x04,0x00,0x00,0x04,0x00,0x00,0x00,0x00,0x00,
            // ';' (59)
            0x00,0x04,0x00,0x00,0x04,0x08,0x00,0x00,0x00,0x00,
            // '<' (60)
            0x02,0x04,0x08,0x10,0x08,0x04,0x02,0x00,0x00,0x00,
            // '=' (61)
            0x00,0x00,0x1F,0x00,0x1F,0x00,0x00,0x00,0x00,0x00,
            // '>' (62)
            0x10,0x08,0x04,0x02,0x04,0x08,0x10,0x00,0x00,0x00,
            // '?' (63)
            0x0E,0x11,0x01,0x06,0x04,0x00,0x04,0x00,0x00,0x00,
            // '@' (64)
            0x0E,0x11,0x17,0x15,0x16,0x10,0x0E,0x00,0x00,0x00,
            // 'A' (65)
            0x04,0x0A,0x11,0x11,0x1F,0x11,0x11,0x00,0x00,0x00,
            // 'B' (66)
            0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E,0x00,0x00,0x00,
            // 'C' (67)
            0x0E,0x11,0x10,0x10,0x10,0x11,0x0E,0x00,0x00,0x00,
            // 'D' (68)
            0x1C,0x12,0x11,0x11,0x11,0x12,0x1C,0x00,0x00,0x00,
            // 'E' (69)
            0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F,0x00,0x00,0x00,
            // 'F' (70)
            0x1F,0x10,0x10,0x1E,0x10,0x10,0x10,0x00,0x00,0x00,
            // 'G' (71)
            0x0E,0x11,0x10,0x17,0x11,0x11,0x0F,0x00,0x00,0x00,
            // 'H' (72)
            0x11,0x11,0x11,0x1F,0x11,0x11,0x11,0x00,0x00,0x00,
            // 'I' (73)
            0x0E,0x04,0x04,0x04,0x04,0x04,0x0E,0x00,0x00,0x00,
            // 'J' (74)
            0x01,0x01,0x01,0x01,0x11,0x11,0x0E,0x00,0x00,0x00,
            // 'K' (75)
            0x11,0x12,0x14,0x18,0x14,0x12,0x11,0x00,0x00,0x00,
            // 'L' (76)
            0x10,0x10,0x10,0x10,0x10,0x10,0x1F,0x00,0x00,0x00,
            // 'M' (77)
            0x11,0x1B,0x15,0x15,0x11,0x11,0x11,0x00,0x00,0x00,
            // 'N' (78)
            0x11,0x11,0x19,0x15,0x13,0x11,0x11,0x00,0x00,0x00,
            // 'O' (79)
            0x0E,0x11,0x11,0x11,0x11,0x11,0x0E,0x00,0x00,0x00,
            // 'P' (80)
            0x1E,0x11,0x11,0x1E,0x10,0x10,0x10,0x00,0x00,0x00,
            // 'Q' (81)
            0x0E,0x11,0x11,0x11,0x15,0x12,0x0D,0x00,0x00,0x00,
            // 'R' (82)
            0x1E,0x11,0x11,0x1E,0x14,0x12,0x11,0x00,0x00,0x00,
            // 'S' (83)
            0x0E,0x11,0x10,0x0E,0x01,0x11,0x0E,0x00,0x00,0x00,
            // 'T' (84)
            0x1F,0x04,0x04,0x04,0x04,0x04,0x04,0x00,0x00,0x00,
            // 'U' (85)
            0x11,0x11,0x11,0x11,0x11,0x11,0x0E,0x00,0x00,0x00,
            // 'V' (86)
            0x11,0x11,0x11,0x0A,0x0A,0x04,0x04,0x00,0x00,0x00,
            // 'W' (87)
            0x11,0x11,0x11,0x15,0x15,0x0A,0x0A,0x00,0x00,0x00,
            // 'X' (88)
            0x11,0x11,0x0A,0x04,0x0A,0x11,0x11,0x00,0x00,0x00,
            // 'Y' (89)
            0x11,0x11,0x0A,0x04,0x04,0x04,0x04,0x00,0x00,0x00,
            // 'Z' (90)
            0x1F,0x01,0x02,0x04,0x08,0x10,0x1F,0x00,0x00,0x00,
            // '[' (91)
            0x0E,0x08,0x08,0x08,0x08,0x08,0x0E,0x00,0x00,0x00,
            // '\\' (92)
            0x10,0x10,0x08,0x04,0x02,0x01,0x01,0x00,0x00,0x00,
            // ']' (93)
            0x0E,0x02,0x02,0x02,0x02,0x02,0x0E,0x00,0x00,0x00,
            // '^' (94)
            0x04,0x0A,0x11,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            // '_' (95)
            0x00,0x00,0x00,0x00,0x00,0x00,0x1F,0x00,0x00,0x00,
            // '`' (96)
            0x08,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            // 'a' (97)
            0x00,0x00,0x0E,0x01,0x0F,0x11,0x0F,0x00,0x00,0x00,
            // 'b' (98)
            0x10,0x10,0x1E,0x11,0x11,0x11,0x1E,0x00,0x00,0x00,
            // 'c' (99)
            0x00,0x00,0x0E,0x10,0x10,0x11,0x0E,0x00,0x00,0x00,
            // 'd' (100)
            0x01,0x01,0x0F,0x11,0x11,0x11,0x0F,0x00,0x00,0x00,
            // 'e' (101)
            0x00,0x00,0x0E,0x11,0x1F,0x10,0x0E,0x00,0x00,0x00,
            // 'f' (102)
            0x06,0x09,0x08,0x1C,0x08,0x08,0x08,0x00,0x00,0x00,
            // 'g' (103)
            0x00,0x00,0x0F,0x11,0x0F,0x01,0x0E,0x00,0x00,0x00,
            // 'h' (104)
            0x10,0x10,0x1E,0x11,0x11,0x11,0x11,0x00,0x00,0x00,
            // 'i' (105)
            0x04,0x00,0x0C,0x04,0x04,0x04,0x0E,0x00,0x00,0x00,
            // 'j' (106)
            0x02,0x00,0x06,0x02,0x02,0x12,0x0C,0x00,0x00,0x00,
            // 'k' (107)
            0x10,0x10,0x11,0x12,0x1C,0x12,0x11,0x00,0x00,0x00,
            // 'l' (108)
            0x0C,0x04,0x04,0x04,0x04,0x04,0x0E,0x00,0x00,0x00,
            // 'm' (109)
            0x00,0x00,0x1A,0x15,0x15,0x11,0x11,0x00,0x00,0x00,
            // 'n' (110)
            0x00,0x00,0x1E,0x11,0x11,0x11,0x11,0x00,0x00,0x00,
            // 'o' (111)
            0x00,0x00,0x0E,0x11,0x11,0x11,0x0E,0x00,0x00,0x00,
            // 'p' (112)
            0x00,0x00,0x1E,0x11,0x1E,0x10,0x10,0x00,0x00,0x00,
            // 'q' (113)
            0x00,0x00,0x0F,0x11,0x0F,0x01,0x01,0x00,0x00,0x00,
            // 'r' (114)
            0x00,0x00,0x16,0x19,0x10,0x10,0x10,0x00,0x00,0x00,
            // 's' (115)
            0x00,0x00,0x0E,0x10,0x0E,0x01,0x0E,0x00,0x00,0x00,
            // 't' (116)
            0x08,0x08,0x1C,0x08,0x08,0x09,0x06,0x00,0x00,0x00,
            // 'u' (117)
            0x00,0x00,0x11,0x11,0x11,0x13,0x0D,0x00,0x00,0x00,
            // 'v' (118)
            0x00,0x00,0x11,0x11,0x0A,0x0A,0x04,0x00,0x00,0x00,
            // 'w' (119)
            0x00,0x00,0x11,0x15,0x15,0x0A,0x0A,0x00,0x00,0x00,
            // 'x' (120)
            0x00,0x00,0x11,0x0A,0x04,0x0A,0x11,0x00,0x00,0x00,
            // 'y' (121)
            0x00,0x00,0x11,0x11,0x0F,0x01,0x0E,0x00,0x00,0x00,
            // 'z' (122)
            0x00,0x00,0x1F,0x02,0x04,0x08,0x1F,0x00,0x00,0x00,
            // '{' (123)
            0x02,0x04,0x04,0x08,0x04,0x04,0x02,0x00,0x00,0x00,
            // '|' (124)
            0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x00,0x00,0x00,
            // '}' (125)
            0x08,0x04,0x04,0x02,0x04,0x04,0x08,0x00,0x00,0x00,
            // '~' (126)
            0x08,0x15,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            // DEL (127)
            0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x00,0x00,0x00,
        };

        // ============================================================================
        // NkUIFontAtlas - Gestion de l'atlas texture
        // ============================================================================

        bool NkUIFontAtlas::Alloc(int32 w, int32 h, int32& outX, int32& outY) noexcept {
            // Vérifier si on doit passer à la ligne suivante
            if (shelfX + w + 1 >= ATLAS_W) {
                shelfX = 1;
                shelfY += shelfH + 1;
                shelfH = 0;
            }
            
            // Vérifier si l'atlas est plein
            if (shelfY + h + 1 >= ATLAS_H) {
                logger.Errorf("[NkUI] Atlas full! Cannot allocate %dx%d glyph\n", w, h);
                return false;
            }
            
            outX = shelfX;
            outY = shelfY;
            shelfX += w + 1;  // +1 pour le padding
            if (h > shelfH) shelfH = h;
            
            return true;
        }

        const NkUIGlyph* NkUIFontAtlas::Find(uint32 cp) const noexcept {
            for (int32 i = 0; i < numGlyphs; ++i) {
                if (glyphs[i].codepoint == cp) return &glyphs[i];
            }
            return nullptr;
        }

        bool NkUIFontAtlas::AddGlyph(uint32 codepoint, int32 x, int32 y, int32 w, int32 h,
                                      float32 advanceX, float32 bearingX, float32 bearingY) noexcept {
            if (numGlyphs >= MAX_GLYPHS) {
                logger.Errorf("[NkUI] Too many glyphs! Max %d\n", MAX_GLYPHS);
                return false;
            }
            
            NkUIGlyph& glyph = glyphs[numGlyphs++];
            glyph.codepoint = codepoint;
            glyph.x0 = static_cast<float32>(x);
            glyph.y0 = static_cast<float32>(y);
            glyph.x1 = static_cast<float32>(x + w);
            glyph.y1 = static_cast<float32>(y + h);
            glyph.u0 = static_cast<float32>(x) / ATLAS_W;
            glyph.v0 = static_cast<float32>(y) / ATLAS_H;
            glyph.u1 = static_cast<float32>(x + w) / ATLAS_W;
            glyph.v1 = static_cast<float32>(y + h) / ATLAS_H;
            glyph.advanceX = advanceX;
            glyph.bearingX = bearingX;
            glyph.bearingY = bearingY;
            
            return true;
        }

        void NkUIFontAtlas::Clear() noexcept {
            memory::NkZero(pixels, sizeof(pixels));
            numGlyphs = 0;
            shelfX = 1;
            shelfY = 1;
            shelfH = 0;
            dirty = true;
        }

        // Dans NkUIFontAtlas::UploadToGPU, assurez-vous que texId n'est pas 0
        void NkUIFontAtlas::UploadToGPU(void* uploadFunc) noexcept {
            if (!dirty) return;
            
            // Si texId est 0, générer un ID temporaire
            if (texId == 0) {
                static uint32 s_nextTexId = 1;
                texId = s_nextTexId++;
                logger.Infof("[NkUI] Generated temporary texId=%u for atlas\n", texId);
            }
            
            logger.Infof("[NkUI] Uploading atlas to GPU, texId=%u, size=%dx%d, glyphs=%d\n", 
                        texId, ATLAS_W, ATLAS_H, numGlyphs);
            
            // Vérifier que les pixels ne sont pas tous vides
            bool hasNonZero = false;
            for (int i = 0; i < 100; i++) {
                if (pixels[i] != 0) {
                    hasNonZero = true;
                    break;
                }
            }
            if (!hasNonZero) {
                logger.Warn("[NkUI] Atlas pixels appear to be all zero!\n");
            }
            
            if (uploadFunc) {
                using UploadFn = void(*)(uint32 texId, const uint8* data, int32 w, int32 h);
                (*reinterpret_cast<UploadFn*>(&uploadFunc))(texId, pixels, ATLAS_W, ATLAS_H);
            }
            
            dirty = false;
            logger.Infof("[NkUI] Atlas uploaded to GPU (texId=%u, glyphs=%d)\n", texId, numGlyphs);
        }

        void NkUIFontAtlas::DumpStats() const noexcept {
            logger.Infof("[NkUI] Atlas stats: texId=%u, glyphs=%d/%d, dirty=%d\n",
                       texId, numGlyphs, MAX_GLYPHS, dirty);
        }

        // ============================================================================
        // NkUIFontManager - Gestionnaire optionnel
        // ============================================================================

        bool NkUIFontManager::Init(const NkUIFontConfig& config) noexcept {
            defaultConfig = config;
            numFonts = 0;
            numAtlases = 0;
            
            // Créer un atlas par défaut si demandé
            if (defaultConfig.enableAtlas && numAtlases < MAX_ATLAS) {
                NkUIFontAtlas* atlas = &atlases[numAtlases++];
                atlas->Clear();
                atlas->yAxisUp = defaultConfig.yAxisUp;
                logger.Infof("[NkUI] Created default atlas (%dx%d)\n", 
                           NkUIFontAtlas::ATLAS_W, NkUIFontAtlas::ATLAS_H);
            }
            
            // Ajouter la police bitmap intégrée
            AddBuiltin(defaultConfig.defaultFontSize);
            
            logger.Infof("[NkUI] Font manager initialized (atlas=%d, yUp=%d)\n", 
                       numAtlases, defaultConfig.yAxisUp);
            
            return numFonts > 0;
        }

        void NkUIFontManager::Destroy() noexcept {
            for (int32 i = 0; i < MAX_BRIDGES; ++i) {
                if (mBridgeUsed[i]) {
                    BridgeAt(*this, i)->Destroy();
                    BridgeAt(*this, i)->~NkUIFontBridge();
                    mBridgeUsed[i] = false;
                }
            }
            mNumBridges = 0;

            for (int32 i = 0; i < numAtlases; ++i) {
                atlases[i].Clear();
            }
            numFonts = 0;
            numAtlases = 0;
            logger.Infof("[NkUI] Font manager destroyed\n");
        }

        uint32 NkUIFontManager::AddBuiltin(float32 size) noexcept {
            if (numFonts >= MAX_FONTS) return 0;
            
            NkUIFont& f = fonts[numFonts];
            memory::NkCopy(f.name, "builtin", sizeof("builtin"));
            f.size = size;
            f.isBuiltin = true;
            f.config = defaultConfig;
            
            // Utiliser le premier atlas si disponible
            if (numAtlases > 0 && defaultConfig.enableAtlas) {
                f.atlas = &atlases[0];
            } else {
                f.atlas = nullptr;
            }
            
            // Métriques pour le bitmap
            const float32 scale = size / f.GetBFH();
            f.metrics.ascender = size * 0.8f;
            f.metrics.descender = size * 0.2f;
            f.metrics.lineHeight = size * 1.2f;
            f.metrics.spaceWidth = size * 0.45f;
            f.metrics.tabWidth = size * 1.6f;
            
            logger.Infof("[NkUI] Added builtin font '%s' size=%.1f (atlas=%s)\n", 
                       f.name, size, f.atlas ? "yes" : "no");
            
            return static_cast<uint32>(numFonts++);
        }

        uint32 NkUIFontManager::AddFromAtlas(const char* name, float32 size,
                                              NkUIFontAtlas* atlas,
                                              const NkUIFontMetrics& metrics) noexcept {
            if (numFonts >= MAX_FONTS) return 0;
            if (!atlas) return 0;
            
            NkUIFont& f = fonts[numFonts];
            if (name) {
                memory::NkCopy(f.name, name, sizeof(f.name));
            } else {
                memory::NkCopy(f.name, "custom", sizeof("custom"));
            }
            f.size = size;
            f.isBuiltin = false;
            f.atlas = atlas;
            f.config = defaultConfig;
            f.metrics = metrics;
            
            logger.Infof("[NkUI] Added font '%s' from atlas size=%.1f\n", f.name, size);
            
            return static_cast<uint32>(numFonts++);
        }

        void NkUIFontManager::UploadDirtyAtlases(void* uploadFunc) noexcept {
            for (int32 i = 0; i < numAtlases; ++i) {
                if (atlases[i].dirty) {
                    atlases[i].UploadToGPU(uploadFunc);
                }
            }
        }

        void NkUIFontManager::SetGlobalYAxisUp(bool yUp) noexcept {
            defaultConfig.yAxisUp = yUp;
            for (int32 i = 0; i < numAtlases; ++i) {
                atlases[i].yAxisUp = yUp;
            }
            logger.Infof("[NkUI] Global Y axis set to %s\n", yUp ? "up" : "down");
        }

        // Vérifie à la compilation que notre stockage opaque est assez grand
        static_assert(sizeof(NkUIFontBridge) <= 256,
            "bridgeStorage_ trop petit : augmentez la taille à sizeof(NkUIFontBridge)");
        static_assert(alignof(NkUIFontBridge) <= 8,
            "Alignement NkUIFontBridge incompatible");

        // ── Helpers internes ──────────────────────────────────────────────────────

        // Alloue un slot (atlas + police + bridge) et retourne l'index, ou -1
        int32 NkUIFontManager::AllocFontSlot(NkUIFontManager& mgr) {
            if (mgr.numFonts  >= NkUIFontManager::MAX_FONTS)  return -1;
            if (mgr.numAtlases >= NkUIFontManager::MAX_ATLAS)  return -1;
            if (mgr.mNumBridges >= NkUIFontManager::MAX_BRIDGES) return -1;
            return mgr.numFonts;   // index commun fonts/atlases/bridges
        }

        NkUIFontBridge* NkUIFontManager::BridgeAt(NkUIFontManager& mgr, int32 idx) {
            // Reinterpret le stockage opaque en NkUIFontBridge
            return reinterpret_cast<NkUIFontBridge*>(&mgr.mBridgeStorage[idx]);
        }

        // Finalise l'ajout d'un slot après un Init réussi
        int32 NkUIFontManager::CommitFontSlot(NkUIFontManager& mgr, int32 idx,
                                    const char* name, float32 sizePx)
        {
            // Le bridge a déjà rempli fonts[idx] et atlases[idx] —
            // on incrémente juste les compteurs et on pose le nom.
            NkUIFont& f = mgr.fonts[idx];
            if (name) memory::NkCopy(f.name, name, sizeof(f.name));
            f.config = mgr.defaultConfig;

            mgr.numFonts++;
            mgr.numAtlases++;
            mgr.mBridgeUsed[idx] = true;
            mgr.mNumBridges++;

            logger.Infof("[NkUIFontManager] Police '%s' %.0fpx chargée (idx=%d)\n",
                        f.name, (double)sizePx, idx);
            return idx;
        }

        // ── LoadFromFile ──────────────────────────────────────────────────────────

        int32 NkUIFontManager::LoadFromFile(const char* path, float32 sizePx,
                                            const char* name,
                                            const nkft_uint32* ranges) noexcept
        {
            const int32 idx = AllocFontSlot(*this);
            if (idx < 0) {
                logger.Errorf("[NkUIFontManager] Slots pleins, impossible de charger '%s'\n",
                            path ? path : "?");
                return -1;
            }

            // Initialise le bridge dans le stockage opaque (placement new)
            NkUIFontBridge* bridge = new (BridgeAt(*this, idx)) NkUIFontBridge{};

            atlases[numAtlases].Clear();
            atlases[numAtlases].yAxisUp = defaultConfig.yAxisUp;

            if (!bridge->InitFromFile(&atlases[numAtlases], &fonts[numFonts],
                                    path, sizePx, ranges))
            {
                logger.Errorf("[NkUIFontManager] Échec LoadFromFile: %s\n",
                            path ? path : "?");
                bridge->~NkUIFontBridge();   // destruction manuelle (placement new)
                return -1;
            }

            return CommitFontSlot(*this, idx, name, sizePx);
        }

        // ── LoadFromMemory ────────────────────────────────────────────────────────

        int32 NkUIFontManager::LoadFromMemory(const nkft_uint8* data, nkft_size dataSize,
                                            float32 sizePx,
                                            const char* name,
                                            const nkft_uint32* ranges) noexcept
        {
            const int32 idx = AllocFontSlot(*this);
            if (idx < 0) return -1;

            NkUIFontBridge* bridge = new (BridgeAt(*this, idx)) NkUIFontBridge{};

            atlases[numAtlases].Clear();
            atlases[numAtlases].yAxisUp = defaultConfig.yAxisUp;

            if (!bridge->InitFromMemory(&atlases[numAtlases], &fonts[numFonts],
                                        data, dataSize, sizePx, ranges))
            {
                bridge->~NkUIFontBridge();
                return -1;
            }

            return CommitFontSlot(*this, idx, name, sizePx);
        }

        // ── LoadCustom ────────────────────────────────────────────────────────────

        int32 NkUIFontManager::LoadCustom(const char* path, float32 sizePx,
                                        const NkUIFontLoaderDesc& desc,
                                        const char* name,
                                        const nkft_uint32* ranges) noexcept
        {
            if (!desc.IsValid()) {
                logger.Error("[NkUIFontManager] LoadCustom: descripteur invalide\n");
                return -1;
            }

            const int32 idx = AllocFontSlot(*this);
            if (idx < 0) return -1;

            NkUIFontBridge* bridge = new (BridgeAt(*this, idx)) NkUIFontBridge{};

            atlases[numAtlases].Clear();
            atlases[numAtlases].yAxisUp = defaultConfig.yAxisUp;

            if (!bridge->InitCustom(&atlases[numAtlases], &fonts[numFonts],
                                    path, sizePx, desc, ranges))
            {
                bridge->~NkUIFontBridge();
                return -1;
            }

            return CommitFontSlot(*this, idx, name, sizePx);
        }

        // ============================================================================
        // NkUIFont - Rendu avec atlas texture
        // ============================================================================
        void NkUIFont::RenderCharAtlas(NkUIDrawList& dl, NkVec2 pos,
                                const NkUIGlyph* g, NkColor col) const noexcept {
            if (!g || !atlas) return;
            if (atlas->texId == 0) {
                RenderCharBitmap(dl, pos, g->codepoint, col);
                return;
            }
        
            // pos.y == baseline Y (coordonnées Y-down)
            // top du glyphe = baseline - bearingY
            const float32 glyphTop  = pos.y - g->bearingY;
            const float32 glyphLeft = pos.x + g->bearingX;
            const float32 glyphW    = g->x1 - g->x0;
            const float32 glyphH    = g->y1 - g->y0;
        
            dl.AddImage(atlas->texId,
                        { glyphLeft, glyphTop, glyphW, glyphH },
                        { g->u0, g->v0 },
                        { g->u1, g->v1 },
                        col);
        }
        // ============================================================================
        // NkUIFont - Rendu avec bitmap fallback
        // ============================================================================

        static inline uint8 Reverse6Bits(uint8 x) noexcept {
            // Inverse les 6 premiers bits (bit0 <-> bit5, bit1 <-> bit4, bit2 <-> bit3)
            return ((x & 0x20) >> 5) | ((x & 0x10) >> 3) | ((x & 0x08) >> 1) |
                   ((x & 0x04) << 1) | ((x & 0x02) << 3) | ((x & 0x01) << 5);
        }

        void NkUIFont::RenderCharBitmap(NkUIDrawList& dl, NkVec2 pos, uint32 cp, NkColor col) const noexcept {
            if (cp < 32 || cp > 127) cp = '?';
            const int32 idx = static_cast<int32>(cp) - kBitmapFirst;
            if (idx < 0 || idx >= kBitmapCount) return;
        
            const uint8* bmp = kBitmapFont + idx * kBitmapFontH;
            const float32 scale = size / static_cast<float32>(kBitmapFontH);
            // Taille exacte d'un pixel bitmap → pas d'overlap
            const float32 pw = scale;
            const float32 ph = scale;
        
            if (!config.yAxisUp) {
                // Y vers le bas (UI standard)
                for (int32 row = 0; row < kBitmapFontH; ++row) {
                    const uint8 line = Reverse6Bits(bmp[row]);
                    const float32 yPos = pos.y + row * ph;
                    for (int32 col2 = 0; col2 < kBitmapFontW; ++col2) {
                        if ((line >> col2) & 1) {
                            dl.AddRectFilled(
                                { pos.x + col2 * pw, yPos, pw, ph },
                                col);
                        }
                    }
                }
            } else {
                // Y vers le haut
                for (int32 row = 0; row < kBitmapFontH; ++row) {
                    const uint8 line = Reverse6Bits(bmp[row]);
                    const float32 yPos = pos.y - (row + 1) * ph;
                    for (int32 col2 = 0; col2 < kBitmapFontW; ++col2) {
                        if ((line >> col2) & 1) {
                            dl.AddRectFilled(
                                { pos.x + col2 * pw, yPos, pw, ph },
                                col);
                        }
                    }
                }
            }
        }

        // ============================================================================
        // NkUIFont - Rendu unifié
        // ============================================================================
        void NkUIFont::RenderChar(NkUIDrawList& dl, NkVec2 pos,
                                uint32 cp, NkColor col) const noexcept {
            // pos.y = baseline
        
            // Atlas en priorité
            if (atlas && config.enableAtlas) {
                const NkUIGlyph* g = atlas->Find(cp);
                if (g) {
                    RenderCharAtlas(dl, pos, g, col);
                    return;
                }
            }
        
            // Bitmap fallback : convertit baseline → top
            if (config.enableBitmapFallback) {
                const float32 asc = metrics.ascender > 0.f ? metrics.ascender : (size * 0.8f);
                NkVec2 bitmapPos = { pos.x, pos.y - asc };
                RenderCharBitmap(dl, bitmapPos, cp, col);
            }
        }

        void NkUIFont::DebugBitmap(uint32 cp) const noexcept {
            static bool debugEnabled = true;
            if (!debugEnabled) return;
            
            if (cp < 32 || cp > 127) cp = '?';
            const int32 idx = static_cast<int32>(cp) - kBitmapFirst;
            const uint8* bmp = kBitmapFont + idx * kBitmapFontH;
            
            logger.Infof("Bitmap for '%c' (U+%04X):\n", cp, cp);
            for (int32 row = 0; row < kBitmapFontH; ++row) {
                char normalLine[16] = {0};
                char reversedLine[16] = {0};
                for (int32 col = 0; col < kBitmapFontW; ++col) {
                    bool bitNormal = (bmp[row] >> col) & 1;
                    bool bitReversed = (bmp[row] >> (kBitmapFontW - 1 - col)) & 1;
                    normalLine[col] = bitNormal ? '#' : '.';
                    reversedLine[col] = bitReversed ? '#' : '.';
                }
                logger.Infof("  Normal  : %s\n", normalLine);
                logger.Infof("  Reversed: %s\n", reversedLine);
            }
            
            debugEnabled = false;
        }

        // ============================================================================
        // NkUIFont - Mesure et rendu de texte
        // ============================================================================

        float32 NkUIFont::MeasureWidth(const char* text, int32 maxLen) const noexcept {
            if (!text) return 0.f;
            
            const float32 charW = (atlas && config.enableAtlas) ? metrics.spaceWidth : size * 0.55f;
            float32 w = 0;
            
            for (int32 i = 0; text[i] && (maxLen < 0 || i < maxLen); ) {
                uint32 cp = '?';
                const int32 charLen = NkDecodeUTF8Codepoint(&text[i], cp);
                if (charLen <= 0) break;

                if (cp == ' ') {
                    w += metrics.spaceWidth;
                } else if (cp == '\t') {
                    w += metrics.tabWidth;
                } else if (cp == '\n' || cp == '\r') {
                    break;
                } else if (isBuiltin || !atlas) {
                    w += charW;
                } else {
                    const NkUIGlyph* g = atlas->Find(cp);
                    w += g ? g->advanceX : charW;
                }

                i += charLen;
            }
            return w;
        }

        int32 NkUIFont::FitChars(const char* text, float32 maxWidth) const noexcept {
            if (!text) return 0;
            const float32 charW = size * 0.55f;
            float32 w = 0;
            int32 n = 0;
            while (text[n] && w + charW <= maxWidth) {
                w += charW;
                ++n;
            }
            return n;
        }

        void NkUIFont::RenderText(NkUIDrawList& dl, NkVec2 pos, const char* text,
                                   NkColor col, float32 maxWidth, bool ellipsis) const noexcept {
            if (!text || !*text) return;
            
            const float32 charW = (atlas && config.enableAtlas) ? metrics.spaceWidth : size * 0.55f;
            float32 x = pos.x;
            const float32 y = pos.y;
            const float32 xMax = (maxWidth > 0) ? pos.x + maxWidth : 1e9f;
            const float32 ellW = ellipsis ? charW * 3 : 0;
            
            for (int32 i = 0; text[i]; ) {
                uint32 cp = '?';
                const int32 charLen = NkDecodeUTF8Codepoint(&text[i], cp);
                if (charLen <= 0) break;
                
                // Traitement des caractères spéciaux
                if (cp == '\n' || cp == '\r') {
                    if (cp == '\n') break;
                    i += charLen;
                    continue;
                }
                if (cp == '\t') {
                    x += metrics.tabWidth;
                    i += charLen;
                    continue;
                }
                
                // Calculer la largeur du caractère
                float32 charWidth = charW;
                if (cp == ' ') {
                    charWidth = metrics.spaceWidth;
                } else if (atlas && config.enableAtlas) {
                    const NkUIGlyph* g = atlas->Find(cp);
                    if (g) charWidth = g->advanceX;
                }
                
                // Vérifier si on doit afficher l'ellipsis
                if (ellipsis && x + charWidth + ellW > xMax && text[i + charLen]) {
                    const float32 ellipsisX = x;
                    for (int32 k = 0; k < 3; ++k) {
                        RenderChar(dl, {ellipsisX + k * charW, y}, '.', col);
                    }
                    return;
                }
                
                if (x + charWidth > xMax) return;
                
                if (cp != ' ') {
                    RenderChar(dl, {x, y}, cp, col);
                }
                
                static int caracterSpace = 0;
                x += charWidth + caracterSpace;
                i += charLen;
            }
        }

        void NkUIFont::RenderTextWrapped(NkUIDrawList& dl, NkRect bounds, const char* text,
                                          NkColor col, float32 lineSpacing) const noexcept {
            if (!text || !*text) return;
            
            const float32 charW = (atlas && config.enableAtlas) ? metrics.spaceWidth : size * 0.6f;
            float32 x = bounds.x;
            float32 y = bounds.y;
            const float32 lh = metrics.lineHeight * lineSpacing;
            const float32 yMax = bounds.y + bounds.h;
            
            for (int32 i = 0; text[i] && y + size <= yMax; ) {
                uint32 cp = '?';
                const int32 charLen = NkDecodeUTF8Codepoint(&text[i], cp);
                if (charLen <= 0) break;
                i += charLen;
                
                if (cp == '\n') {
                    x = bounds.x;
                    y += lh;
                    continue;
                }
                if (cp == '\r') continue;
                if (cp == '\t') {
                    x += metrics.tabWidth;
                    continue;
                }
                
                const float32 w = (cp == ' ') ? metrics.spaceWidth : charW;
                
                if (x + w > bounds.x + bounds.w && x > bounds.x) {
                    x = bounds.x;
                    y += lh;
                    if (y + size > yMax) break;
                    if (cp == ' ') continue;
                }
                
                if (cp != ' ') {
                    RenderChar(dl, {x, y}, cp, col);
                }
                x += w;
            }
        }

    }
}
