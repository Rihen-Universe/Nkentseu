// -----------------------------------------------------------------------------
// FICHIER: NkUIFontBridge.h
// DESCRIPTION: Pont entre NKUIFont et NKFont.
//              Permet à NKUIFont d'utiliser NKFont comme backend par défaut,
//              tout en laissant l'utilisateur brancher son propre système
//              via des fonctions de callback.
//
// ARCHITECTURE :
//
//   NkUIFont (UI layer)
//       │
//       ├── Backend par défaut : NkUIFontBridge → NKFont
//       │        • Chargement TTF/OTF/WOFF
//       │        • Rastérisation vers atlas NkUIFontAtlas
//       │        • Métriques automatiques
//       │
//       └── Backend utilisateur : NkUIFontLoaderDesc (callbacks)
//                • LoadFont(path, size, ...) → opaque handle
//                • GetGlyph(handle, codepoint, ...) → NkUIGlyph
//                • GetMetrics(handle, ...) → NkUIFontMetrics
//                • DestroyFont(handle)
//
// USAGE — Backend NKFont (défaut) :
//   NkUIFontBridge bridge;
//   bridge.Init(&atlas, "Resources/Fonts/Roboto.ttf", 16.f);
//   // atlas est rempli, fonts[0].atlas = &atlas
//
// USAGE — Backend custom (FreeType, DirectWrite...) :
//   NkUIFontLoaderDesc desc;
//   desc.LoadFont   = [](const char* path, float size, void* user, void** handle) { ... };
//   desc.GetGlyph   = [](void* handle, uint32 cp, NkUIGlyph* out, void* user) { ... };
//   desc.GetMetrics = [](void* handle, NkUIFontMetrics* out, void* user) { ... };
//   desc.Destroy    = [](void* handle, void* user) { ... };
//   desc.userData   = myFreeTypeLib;
//   bridge.InitCustom(&atlas, "arial.ttf", 16.f, &desc);
// -----------------------------------------------------------------------------
#pragma once

#include "NKUI/NkUIFont.h"        // NkUIFontAtlas, NkUIGlyph, NkUIFontMetrics
#include "NKFont/Core/NkFontParser.h"  // NKFont backend

namespace nkentseu {
    namespace nkui {

        // ─────────────────────────────────────────────────────────────────────────────
        // Callbacks — interface pour un système de police tiers
        // ─────────────────────────────────────────────────────────────────────────────

        /**
         * @brief Callback de chargement d'une police.
         * @param path     Chemin vers le fichier TTF/OTF (peut être nullptr si data fournie).
         * @param data     Buffer mémoire de la police (nullptr si path fourni).
         * @param dataSize Taille du buffer (0 si path fourni).
         * @param sizePx   Taille en pixels.
         * @param userData Données utilisateur passées au loader.
         * @param outHandle [out] Handle opaque vers la police chargée.
         * @return true si succès.
         */
        using NkUIFontLoadFn = bool(*)(const char*    path,
                                        const nkft_uint8* data, nkft_size dataSize,
                                        nkft_float32   sizePx,
                                        void*          userData,
                                        void**         outHandle);

        /**
         * @brief Callback de récupération d'un glyphe.
         * @param handle   Handle retourné par LoadFont.
         * @param cp       Codepoint Unicode.
         * @param atlasPixels Buffer de l'atlas (Gray8, largeur atlasW).
         * @param atlasW   Largeur de l'atlas.
         * @param atlasX   Coordonnée X allouée dans l'atlas pour ce glyphe.
         * @param atlasY   Coordonnée Y allouée dans l'atlas pour ce glyphe.
         * @param userData Données utilisateur.
         * @param outGlyph [out] Métriques du glyphe.
         * @return true si le glyphe existe.
         */
        using NkUIFontGetGlyphFn = bool(*)(void*         handle,
                                            nkft_uint32   cp,
                                            nkft_uint8*   atlasPixels,
                                            nkft_int32    atlasW,
                                            nkft_int32    atlasX,
                                            nkft_int32    atlasY,
                                            nkft_int32    glyphW,
                                            nkft_int32    glyphH,
                                            void*         userData,
                                            NkUIGlyph*    outGlyph);

        /**
         * @brief Callback de récupération des métriques globales.
         * @param handle   Handle retourné par LoadFont.
         * @param userData Données utilisateur.
         * @param outMetrics [out] Métriques de la police.
         */
        using NkUIFontGetMetricsFn = void(*)(void*             handle,
                                            void*             userData,
                                            NkUIFontMetrics*  outMetrics);

        /**
         * @brief Callback de bounding box d'un glyphe (pour allouer dans l'atlas).
         * @param handle   Handle retourné par LoadFont.
         * @param cp       Codepoint Unicode.
         * @param userData Données utilisateur.
         * @param outW     [out] Largeur du glyphe en pixels.
         * @param outH     [out] Hauteur du glyphe en pixels.
         * @return true si le glyphe existe.
         */
        using NkUIFontGetBBoxFn = bool(*)(void*       handle,
                                        nkft_uint32 cp,
                                        void*       userData,
                                        nkft_int32* outW,
                                        nkft_int32* outH);

        /**
         * @brief Callback de destruction.
         * @param handle   Handle à libérer.
         * @param userData Données utilisateur.
         */
        using NkUIFontDestroyFn = void(*)(void* handle, void* userData);

        // ─────────────────────────────────────────────────────────────────────────────
        // Descripteur complet d'un loader custom
        // ─────────────────────────────────────────────────────────────────────────────

        struct NkUIFontLoaderDesc {
            NkUIFontLoadFn       LoadFont   = nullptr; ///< Charge la police
            NkUIFontGetGlyphFn   GetGlyph   = nullptr; ///< Rastérise un glyphe dans l'atlas
            NkUIFontGetBBoxFn    GetBBox    = nullptr; ///< Donne la taille d'un glyphe
            NkUIFontGetMetricsFn GetMetrics = nullptr; ///< Retourne les métriques globales
            NkUIFontDestroyFn    Destroy    = nullptr; ///< Libère la police
            void*                userData   = nullptr; ///< Données utilisateur

            bool IsValid() const {
                return LoadFont && GetGlyph && GetBBox && GetMetrics && Destroy;
            }
        };

        // ─────────────────────────────────────────────────────────────────────────────
        // NkUIFontBridge — pont NKUIFont ↔ NKFont (ou backend custom)
        // ─────────────────────────────────────────────────────────────────────────────

        /**
         * @struct NkUIFontBridge
         * @brief Charge une police dans NkUIFontAtlas via NKFont ou un backend custom.
         *
         * Ce bridge remplit automatiquement un NkUIFontAtlas à partir d'une police
         * TTF/OTF, en utilisant NKFont comme backend par défaut.
         *
         * Il génère automatiquement le NkUIFont associé avec les bonnes métriques.
         */
        struct NkUIFontBridge {

            // ── État ──────────────────────────────────────────────────────────────
            NkUIFontAtlas*   atlas       = nullptr;
            NkUIFont*        font        = nullptr; ///< Police NkUI résultante
            void*            fontHandle  = nullptr; ///< Handle opaque (NKFont ou custom)
            bool             useNKFont   = true;    ///< true = backend NKFont intégré
            NkUIFontLoaderDesc customDesc;           ///< Loader custom (si useNKFont=false)

            // Backend NKFont interne
            nkfont::NkFontFaceInfo nkfontFace;      ///< Face NKFont (si useNKFont=true)
            nkft_uint8*            nkfontData = nullptr; ///< Données TTF allouées
            nkft_size              nkfontSize = 0;

            // ── Init avec NKFont (backend par défaut) ─────────────────────────────

            /**
             * @brief Initialise le bridge avec NKFont depuis un fichier.
             * @param atlas     Atlas à remplir.
             * @param fontOut   Police NkUI à configurer.
             * @param path      Chemin vers le fichier TTF/OTF/WOFF.
             * @param sizePx    Taille en pixels.
             * @param ranges    Plages Unicode à rastériser (nullptr = ASCII+Latin-1).
             * @return true si succès.
             */
            bool InitFromFile(NkUIFontAtlas* atlas, NkUIFont* fontOut,
                            const char* path, nkft_float32 sizePx,
                            const nkft_uint32* ranges = nullptr);

            /**
             * @brief Initialise le bridge avec NKFont depuis un buffer mémoire.
             */
            bool InitFromMemory(NkUIFontAtlas* atlas, NkUIFont* fontOut,
                                const nkft_uint8* data, nkft_size dataSize,
                                nkft_float32 sizePx,
                                const nkft_uint32* ranges = nullptr);

            // ── Init avec un backend custom ────────────────────────────────────────

            /**
             * @brief Initialise le bridge avec un loader custom.
             * @param atlas     Atlas à remplir.
             * @param fontOut   Police NkUI à configurer.
             * @param path      Chemin (passé tel quel au loader).
             * @param sizePx    Taille en pixels.
             * @param desc      Descripteur du loader custom.
             * @param ranges    Plages Unicode à rastériser.
             * @return true si succès.
             */
            bool InitCustom(NkUIFontAtlas* atlas, NkUIFont* fontOut,
                            const char* path, nkft_float32 sizePx,
                            const NkUIFontLoaderDesc& desc,
                            const nkft_uint32* ranges = nullptr);

            // ── Destruction ────────────────────────────────────────────────────────

            void Destroy();

            // ── Plages Unicode prédéfinies ─────────────────────────────────────────
            static const nkft_uint32* RangesASCII();
            static const nkft_uint32* RangesLatinExtended();
            static const nkft_uint32* RangesDefault(); ///< ASCII + Latin-1 supplement

        private:
            bool BuildAtlas(const nkft_uint32* ranges, nkft_float32 sizePx);
            bool BuildAtlasCustom(const nkft_uint32* ranges, nkft_float32 sizePx);
        };

    } // namespace nkui
} // namespace nkentseu