#pragma once
// =============================================================================
// NkWGLPixelFormat.h
// Structure configurable côté utilisateur pour le PIXELFORMATDESCRIPTOR WGL.
//
// Deux niveaux de configuration :
//
//  1. NkWGLFallbackPixelFormat
//     → Pilote le PIXELFORMATDESCRIPTOR du contexte BOOTSTRAP (factice).
//     → Ce contexte ne sert qu'à charger wglCreateContextAttribsARB.
//     → L'utilisateur peut le personnaliser mais les valeurs par défaut
//       sont suffisantes dans 99% des cas.
//
//  2. NkOpenGLDesc (dans NkContextDesc.h)
//     → Pilote le pixel format ARB FINAL (via wglChoosePixelFormatARB).
//     → C'est le vrai format utilisé pour le rendu.
//     → Ses champs (colorBits, depthBits, stencilBits, msaaSamples…)
//       alimentent directement les attributs ARB.
//
// Relation :
//   NkContextDesc::opengl        → pixel format ARB final (priorité)
//   NkContextDesc::wglFallback   → PIXELFORMATDESCRIPTOR bootstrap
//
// Windows uniquement — ignoré sur toutes les autres plateformes.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKPlatform/NkPlatformDetect.h"

namespace nkentseu {

    #if defined(NKENTSEU_PLATFORM_WINDOWS)

        // -------------------------------------------------------------------------
        // Flags dwFlags du PIXELFORMATDESCRIPTOR
        // Miroir des constantes WinGDI pour éviter d'inclure <windows.h> ici.
        // -------------------------------------------------------------------------
        enum class NkPFDFlags : uint32 {
            DrawToWindow   = 0x00000004,  // PFD_DRAW_TO_WINDOW
            DrawToBitmap   = 0x00000008,  // PFD_DRAW_TO_BITMAP
            SupportOpenGL  = 0x00000020,  // PFD_SUPPORT_OPENGL
            DoubleBuffer   = 0x00000001,  // PFD_DOUBLEBUFFER
            Stereo         = 0x00000002,  // PFD_STEREO
            SwapExchange   = 0x00000200,  // PFD_SWAP_EXCHANGE
            SwapCopy       = 0x00000400,  // PFD_SWAP_COPY
            SwapLayerBuffers = 0x00000800,// PFD_SWAP_LAYER_BUFFERS
            GenericFormat  = 0x00000040,  // PFD_GENERIC_FORMAT
            NeedPalette    = 0x00000080,  // PFD_NEED_PALETTE

            // Combinaisons usuelles prêtes à l'emploi
            Default = (uint32)DrawToWindow | (uint32)SupportOpenGL | (uint32)DoubleBuffer,
            DefaultSingleBuffer = (uint32)DrawToWindow | (uint32)SupportOpenGL,
        };

        inline NkPFDFlags operator|(NkPFDFlags a, NkPFDFlags b) {
            return static_cast<NkPFDFlags>(static_cast<uint32>(a) | static_cast<uint32>(b));
        }
        inline NkPFDFlags operator&(NkPFDFlags a, NkPFDFlags b) {
            return static_cast<NkPFDFlags>(static_cast<uint32>(a) & static_cast<uint32>(b));
        }

        // -------------------------------------------------------------------------
        // Type de pixel
        // -------------------------------------------------------------------------
        enum class NkPFDPixelType : uint8 {
            RGBA       = 0,  // PFD_TYPE_RGBA
            ColorIndex = 1,  // PFD_TYPE_COLORINDEX (obsolète, usage rare)
        };

        // -------------------------------------------------------------------------
        // NkWGLFallbackPixelFormat
        //
        // Contrôle le PIXELFORMATDESCRIPTOR utilisé pour le contexte bootstrap.
        // Ce contexte est TEMPORAIRE — il est créé, utilisé pour charger les
        // extensions WGL ARB, puis immédiatement détruit.
        //
        // Dans 99% des cas, les valeurs par défaut suffisent.
        // Ne personnaliser que si le driver échoue avec les valeurs par défaut
        // (certains drivers anciens ou virtualisés sont pointilleux).
        // -------------------------------------------------------------------------
        struct NkWGLFallbackPixelFormat {

            // --- Champs principaux ---
            uint8          colorBits   = 32;   ///< cColorBits  — bits couleur (24 ou 32)
            uint8          alphaBits   = 8;    ///< cAlphaBits
            uint8          depthBits   = 24;   ///< cDepthBits
            uint8          stencilBits = 8;    ///< cStencilBits
            uint8          accumBits   = 0;    ///< cAccumBits (accumulation buffer, obsolète)
            uint8          auxBuffers  = 0;    ///< cAuxBuffers (rarement utilisé)
            NkPFDPixelType pixelType   = NkPFDPixelType::RGBA;
            NkPFDFlags     flags       = NkPFDFlags::Default;

            // --- Version du descripteur ---
            // Toujours 1 selon la spec Microsoft.
            // Ne changer que si un driver très ancien l'exige autrement.
            uint16 version = 1;

            // --- Presets ---
            static NkWGLFallbackPixelFormat Minimal() {
                // Minimum absolu — compatible avec les drivers les plus anciens
                NkWGLFallbackPixelFormat f;
                f.colorBits   = 24;
                f.alphaBits   = 0;
                f.depthBits   = 16;
                f.stencilBits = 0;
                return f;
            }

            static NkWGLFallbackPixelFormat Standard() {
                // Valeurs par défaut — couvre 99% des cas
                return {}; // Constructeur par défaut
            }

            static NkWGLFallbackPixelFormat HighPrecision() {
                // Pour les rendus haute précision ou HDR
                NkWGLFallbackPixelFormat f;
                f.colorBits   = 32;
                f.alphaBits   = 8;
                f.depthBits   = 32;
                f.stencilBits = 8;
                return f;
            }

            static NkWGLFallbackPixelFormat SingleBuffer() {
                // Rendu dans un bitmap, pas de swap
                NkWGLFallbackPixelFormat f;
                f.flags = NkPFDFlags::DefaultSingleBuffer;
                return f;
            }
        };

    #else
        // Stub vide sur les autres plateformes
        struct NkWGLFallbackPixelFormat {};
    #endif

} // namespace nkentseu
