#pragma once

/*
 * NKUI_MAINTENANCE_GUIDE
 * Responsibility: Font abstraction and text rendering interface for NKUI.
 * Main data: Glyph metrics, atlas hooks, text rendering API.
 * Change this file when: Text shaping, glyph atlas, or font fallback changes.
 */
/**
 * @File    NkUIFont.h
 * @Brief   Système de polices NkUI — atlas, mesure, rendu.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  NkUIFont est une couche de police agnostique. Elle peut être alimentée
 *  par n'importe quel système de font (FreeType, NKFont, DirectWrite, etc.)
 *  via son API d'atlas. La police bitmap intégrée sert de fallback.
 *
 *  Utilisation typique avec NKFont (dans l'application) :
 *    // 1. Créer l'atlas
 *    NkUIFontAtlas atlas;
 *    atlas.Clear();
 *    
 *    // 2. Rasteriser les glyphes avec NKFont
 *    NkFontFace* face = fontLib.LoadFont(ttfData, ttfSize, 16);
 *    for (uint32 cp = 32; cp < 128; ++cp) {
 *        NkGlyph glyph;
 *        if (face->GetGlyph(cp, glyph)) {
 *            int32 x, y;
 *            if (atlas.Alloc(glyph.metrics.width, glyph.metrics.height, x, y)) {
 *                // Copier pixels
 *                atlas.AddGlyph(cp, x, y, ...);
 *            }
 *        }
 *    }
 *    
 *    // 3. Upload sur GPU
 *    atlas.UploadToGPU(uploadFunc);
 *    
 *    // 4. Créer la police NkUI
 *    NkUIFont font;
 *    font.atlas = &atlas;
 *    font.size = 16;
 *    font.metrics = ...; // Récupérer depuis NKFont
 */
#include "NKUI/NkUIDrawList.h"
#include "NKFont/Embedded/NkFontEmbedded.h"
// #include "NkUIFontBridge.h"
// #include "NKFont/NkFont.h"

namespace nkentseu {
    namespace nkui {
        struct NkUIFontBridge;
        struct NkUIFontLoaderDesc;   // défini dans NkUIFontBridge.h
        // ============================================================================
        // Configuration globale du système de polices
        // ============================================================================
        
        /**
         * @struct NkUIFontConfig
         * @brief Configuration globale du système de polices
         */
        struct NkUIFontConfig {
            bool yAxisUp = false;               ///< false = Y vers le bas (OpenGL), true = Y vers le haut (math)
            bool enableAtlas = true;            ///< Activer l'utilisation de l'atlas texture
            bool enableBitmapFallback = true;   ///< Activer le fallback bitmap si l'atlas n'a pas le glyphe
            float32 defaultFontSize = 14.0f;    ///< Taille de police par défaut
        };

        // ============================================================================
        // Structures de données pour les glyphes et métriques
        // ============================================================================
        
        /**
         * @struct NkUIGlyph
         * @brief Description d'un glyphe dans l'atlas texture
         */
        struct NkUIGlyph {
            uint32  codepoint = 0;          ///< Code Unicode ou ID du caractère
            float32 x0=0, y0=0, x1=0, y1=0; ///< Position dans l'atlas (pixels)
            float32 u0=0, v0=0, u1=0, v1=0; ///< Coordonnées UV normalisées [0,1]
            float32 advanceX = 0;           ///< Avancement horizontal (largeur du caractère)
            float32 bearingX = 0;           ///< Décalage horizontal
            float32 bearingY = 0;           ///< Décalage vertical (baseline)
        };

        /**
         * @struct NkUIFontMetrics
         * @brief Métriques typographiques d'une police
         */
        struct NkUIFontMetrics {
            float32 ascender    = 10.f;     ///< Hauteur ascendante
            float32 descender   = 2.f;      ///< Profondeur descendante
            float32 lineGap     = 2.f;      ///< Espace entre lignes
            float32 lineHeight  = 14.f;     ///< Hauteur de ligne totale
            float32 spaceWidth  = 4.f;      ///< Largeur de l'espace
            float32 tabWidth    = 28.f;     ///< Largeur de la tabulation (4 espaces)
        };

        // ============================================================================
        // NkUIFontAtlas - Gestion de l'atlas texture (API publique)
        // ============================================================================
        
        /**
         * @struct NkUIFontAtlas
         * @brief Atlas texture contenant tous les glyphes rasterisés
         * 
         * L'atlas utilise un shelf packer simple pour placer les glyphes
         * dans une texture 512x512 en niveaux de gris (Gray8).
         * 
         * Cette classe est conçue pour être remplie par l'application
         * avec n'importe quel système de font (FreeType, NKFont, etc.)
         */
        struct NKUI_API NkUIFontAtlas {
            static constexpr int32 ATLAS_W = 512;      ///< Largeur de l'atlas
            static constexpr int32 ATLAS_H = 512;      ///< Hauteur de l'atlas
            static constexpr int32 MAX_GLYPHS = 1024;  ///< Nombre max de glyphes

            uint8        pixels[ATLAS_W * ATLAS_H] = {}; ///< Données de texture (Gray8)
            NkUIGlyph    glyphs[MAX_GLYPHS]        = {}; ///< Tableau des glyphes
            int32        numGlyphs                 = 0;  ///< Nombre de glyphes chargés
            uint32       texId                     = 0;  ///< Handle GPU de la texture
            bool         dirty                     = true; ///< Texture modifiée (à uploader)
            bool         yAxisUp                   = false; ///< Orientation Y

            // Shelf packer pour l'allocation dans l'atlas
            int32        shelfX    = 1;           ///< Position X courante
            int32        shelfY    = 1;           ///< Position Y courante
            int32        shelfH    = 0;           ///< Hauteur de la ligne courante

            /**
             * @brief Alloue un rectangle dans l'atlas
             * @param w Largeur du rectangle
             * @param h Hauteur du rectangle
             * @param outX Position X allouée (output)
             * @param outY Position Y allouée (output)
             * @return true si allocation réussie
             */
            bool Alloc(int32 w, int32 h, int32& outX, int32& outY) noexcept;

            /**
             * @brief Recherche un glyphe par son code Unicode
             * @param cp Code Unicode
             * @return Pointeur vers le glyphe, ou nullptr si non trouvé
             */
            const NkUIGlyph* Find(uint32 cp) const noexcept;

            /**
             * @brief Ajoute un glyphe dans l'atlas
             * @param codepoint Code Unicode
             * @param x Position X dans l'atlas
             * @param y Position Y dans l'atlas
             * @param w Largeur du glyphe
             * @param h Hauteur du glyphe
             * @param advanceX Avancement horizontal
             * @param bearingX Décalage horizontal
             * @param bearingY Décalage vertical
             * @return true si ajout réussi
             */
            bool AddGlyph(uint32 codepoint, int32 x, int32 y, int32 w, int32 h,
                         float32 advanceX, float32 bearingX, float32 bearingY) noexcept;

            /**
             * @brief Vide l'atlas
             */
            void Clear() noexcept;

            /**
             * @brief Upload la texture sur le GPU si nécessaire
             * @param uploadFunc Fonction callback pour l'upload GPU (nullptr = pas d'upload)
             * 
             * Exemple d'uploadFunc pour OpenGL :
             *   [](uint32 texId, const uint8* data, int32 w, int32 h) {
             *       glBindTexture(GL_TEXTURE_2D, texId);
             *       glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, data);
             *   }
             */
            void UploadToGPU(void* uploadFunc) noexcept;
            
            /**
             * @brief Affiche les statistiques de l'atlas (debug)
             */
            void DumpStats() const noexcept;
        };

        // ============================================================================
        // NkUIFont - Police à une taille donnée
        // ============================================================================
        
        /**
         * @struct NkUIFont
         * @brief Représente une police à une taille spécifique
         * 
         * Une police peut utiliser soit l'atlas texture (rempli par l'application),
         * soit le bitmap fallback intégré (toujours disponible).
         */
        struct NKUI_API NkUIFont {
            char            name[64]  = "builtin";    ///< Nom de la police
            float32         size      = 14.f;         ///< Taille en pixels
            NkUIFontMetrics metrics;                  ///< Métriques typographiques
            NkUIFontAtlas*  atlas     = nullptr;      ///< Atlas associé (nullptr = bitmap)
            bool            isBuiltin = true;         ///< Police intégrée (bitmap)
            NkUIFontConfig  config;                   ///< Configuration

            /**
             * @brief Mesure la largeur d'un texte
             * @param text Texte à mesurer
             * @param maxLen Longueur max (-1 = tout)
             * @return Largeur en pixels
             */
            float32 MeasureWidth(const char* text, int32 maxLen = -1) const noexcept;

            /**
             * @brief Calcule combien de caractères tiennent dans une largeur max
             * @param text Texte
             * @param maxWidth Largeur maximale
             * @return Nombre de caractères
             */
            int32 FitChars(const char* text, float32 maxWidth) const noexcept;

            /**
             * @brief Rendu d'un texte simple
             * @param dl DrawList
             * @param pos Position de départ
             * @param text Texte à afficher
             * @param col Couleur
             * @param maxWidth Largeur max (-1 = illimitée)
             * @param ellipsis Ajouter "..." si trop long
             */
            void RenderText(NkUIDrawList& dl, NkVec2 pos, const char* text, 
                           NkColor col, float32 maxWidth = -1.f, bool ellipsis = false) const noexcept;

            /**
             * @brief Rendu d'un texte avec wrapping automatique
             * @param dl DrawList
             * @param bounds Rectangle de délimitation
             * @param text Texte à afficher
             * @param col Couleur
             * @param lineSpacing Espacement entre lignes (1.0 = normal)
             */
            void RenderTextWrapped(NkUIDrawList& dl, NkRect bounds, const char* text,
                                  NkColor col, float32 lineSpacing = 1.f) const noexcept;

            /**
             * @brief Rendu d'un caractère unique
             * @param dl DrawList
             * @param pos Position
             * @param codepoint Code Unicode
             * @param col Couleur
             */
            void RenderChar(NkUIDrawList& dl, NkVec2 pos, uint32 codepoint, NkColor col) const noexcept;

            /**
             * @brief Debug : affiche le bitmap d'un caractère (utile pour diagnostiquer)
             * @param cp Code Unicode
             */
            void DebugBitmap(uint32 cp) const noexcept;

            /**
             * @brief Retourne la hauteur du bitmap fallback (pour les métriques)
             */
            static const int32 GetBFH() noexcept { return kBitmapFontH; }

        private:
            // Police bitmap intégrée (6x10 pixels, ASCII 32-127)
            static const uint8  kBitmapFont[];
            static const int32  kBitmapFontW = 6;      ///< Largeur bitmap
            static const int32  kBitmapFontH = 10;     ///< Hauteur bitmap
            static const int32  kBitmapFirst = 32;     ///< Premier caractère ASCII
            static const int32  kBitmapCount = 96;     ///< Nombre de caractères

            /**
             * @brief Rendu avec le bitmap fallback (pixel art)
             */
            void RenderCharBitmap(NkUIDrawList& dl, NkVec2 pos, uint32 cp, NkColor col) const noexcept;

            /**
             * @brief Rendu avec l'atlas texture (haute qualité)
             */
            void RenderCharAtlas(NkUIDrawList& dl, NkVec2 pos, const NkUIGlyph* g, NkColor col) const noexcept;
        };

        // ============================================================================
        // NkUIFontManager - Gestionnaire de polices (optionnel)
        // ============================================================================
        
        /**
         * @struct NkUIFontManager
         * @brief Gestionnaire de polices simplifié
         * 
         * Ce gestionnaire est optionnel. Vous pouvez gérer vos propres polices
         * et atlas si vous préférez. Il fournit juste un conteneur pratique.
         */
        struct NKUI_API NkUIFontManager {
                static constexpr int32 MAX_FONTS  = 16;   ///< Nombre max de polices
                static constexpr int32 MAX_ATLAS  = 4;    ///< Nombre max d'atlas

                NkUIFont       fonts[MAX_FONTS]  = {};    ///< Tableau des polices
                int32          numFonts          = 0;     ///< Nombre de polices chargées
                NkUIFontAtlas  atlases[MAX_ATLAS]= {};    ///< Tableau des atlas
                int32          numAtlases        = 0;     ///< Nombre d'atlas créés
                NkUIFontConfig defaultConfig;              ///< Configuration par défaut

                /**
                 * @brief Initialise le gestionnaire avec la police bitmap intégrée
                 * @param config Configuration
                 * @return true si succès
                 */
                bool Init(const NkUIFontConfig& config = NkUIFontConfig()) noexcept;

                /**
                 * @brief Détruit toutes les polices et atlas
                 */
                void Destroy() noexcept;

                /**
                 * @brief Ajoute la police bitmap intégrée
                 * @param size Taille en pixels
                 * @return Index de la police
                 */
                uint32 AddBuiltin(float32 size = 14.f) noexcept;

                /**
                 * @brief Crée une police à partir d'un atlas existant
                 * @param name Nom de la police
                 * @param size Taille
                 * @param atlas Atlas associé
                 * @param metrics Métriques
                 * @return Index de la police
                 */
                uint32 AddFromAtlas(const char* name, float32 size, 
                                    NkUIFontAtlas* atlas, const NkUIFontMetrics& metrics) noexcept;

                /**
                 * @brief Récupère une police par son index
                 */
                NkUIFont* Get(uint32 idx) noexcept {
                    return (idx < static_cast<uint32>(numFonts)) ? &fonts[idx] : 
                        (numFonts > 0 ? &fonts[0] : nullptr);
                }

                /**
                 * @brief Récupère la police par défaut
                 */
                NkUIFont* Default() noexcept { return numFonts > 0 ? &fonts[0] : nullptr; }

                /**
                 * @brief Upload tous les atlas sales sur le GPU
                 */
                void UploadDirtyAtlases(void* uploadFunc = nullptr) noexcept;

                /**
                 * @brief Définit globalement l'orientation Y
                 */
                void SetGlobalYAxisUp(bool yUp) noexcept;

                // Tableau des bridges (géré en interne, un par police TTF chargée)
                // On utilise un stockage opaque pour ne pas exposer NkUIFontBridge dans ce header.
                static constexpr int32 MAX_BRIDGES = MAX_FONTS;

            private:
                // Stockage opaque — aligné sur NkUIFontBridge, taille vérifiée dans le .cpp
                alignas(8) uint8 mBridgeStorage[MAX_BRIDGES][256] = {};
                bool             mBridgeUsed[MAX_BRIDGES]          = {};
                int32            mNumBridges                       = 0;

                static int32 AllocFontSlot(NkUIFontManager& mgr);
                static NkUIFontBridge* BridgeAt(NkUIFontManager& mgr, int32 idx);
                static int32 CommitFontSlot(NkUIFontManager& mgr, int32 idx, const char* name, float32 sizePx);
            public:
                /**
                 * @brief Charge une police TTF/OTF depuis un fichier via NKFont.
                 * @param path    Chemin vers le fichier de police.
                 * @param sizePx  Taille en pixels.
                 * @param name    Nom de la police (optionnel).
                 * @param ranges  Plages Unicode (nullptr = ASCII + Latin-1).
                 * @return Index de la police dans fonts[], ou -1 si échec.
                 */
                int32 LoadFromFile(const char* path, float32 sizePx,
                                const char* name   = nullptr,
                                const uint32* ranges = nullptr) noexcept;

                /**
                 * @brief Charge une police depuis un buffer mémoire via NKFont.
                 */
                int32 LoadFromMemory(const uint8* data, usize dataSize,
                                    float32 sizePx,
                                    const char* name   = nullptr,
                                    const uint32* ranges = nullptr) noexcept;

                /**
                 * @brief Charge une police via un backend custom (FreeType, DirectWrite…).
                 * @param path    Chemin passé tel quel au loader custom.
                 * @param sizePx  Taille en pixels.
                 * @param desc    Descripteur du loader custom.
                 * @param name    Nom de la police (optionnel).
                 * @param ranges  Plages Unicode.
                 * @return Index de la police, ou -1 si échec.
                 */
                int32 LoadCustom(const char* path, float32 sizePx,
                                const NkUIFontLoaderDesc& desc,
                                const char* name   = nullptr,
                                const uint32* ranges = nullptr) noexcept;

                /**
                 * @brief Charge une police embarquée (TTF compressé dans le binaire).
                 * @param id      Identifiant de la police embarquée (ex: NkEmbeddedFontId::Roboto).
                 * @param sizePx  Taille en pixels.
                 * @param name    Nom de la police (optionnel).
                 * @param ranges  Plages Unicode (nullptr = ASCII + Latin-1).
                 * @return Index de la police dans fonts[], ou -1 si échec.
                 */
                int32 LoadEmbedded(NkEmbeddedFontId id, float32 sizePx,
                                const char* name = nullptr,
                                const uint32* ranges = nullptr) noexcept;
        };
    }
}