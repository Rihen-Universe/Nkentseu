#pragma once
// =============================================================================
// PongConfig.h — Lecture/ecriture du fichier pong.config (backend dynamique)
//
// Format texte ultra-simple, lisible et editable a la main :
//
//     backend=opengl
//
// Cles supportees :
//   backend  = opengl | vulkan | dx11 | dx12 | software | auto
//              (auto = laisser NkContextFactory::CreateWithFallback choisir)
//
// L'application lit pong.config a l'ouverture. Si absent, ecrit un fichier
// par defaut (backend=opengl) puis le relit. Permet de switcher de backend
// sans recompiler — pratique pour valider chaque backend NKCanvas.
//
// Header-only : pas de .cpp pour eviter de complexifier le build.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKPlatform/NkPlatformDetect.h"     // macros NKENTSEU_PLATFORM_*
#include "NKPlatform/NkCGXDetect.h"          // NkGraphicsApi enum (NK_GFX_API_*)
#include "NKCanvas/Core/NkGraphicsApi.h"     // NkGraphicsApiIsAvailable / Name
#include "NKFileSystem/NkFile.h"             // NkFile : abstraction multi-platform
#include <cstdio>                             // std::snprintf pour formater le contenu
#include <cstring>

namespace nkentseu {
    namespace pong {

        struct PongConfig {
            NkGraphicsApi backend = NkGraphicsApi::NK_GFX_API_OPENGL;
            /// debug=1 dans pong.config : active la validation GPU du backend
            /// (couche debug DX11/DX12, validation layers Vulkan) — messages
            /// forwardes vers NkLogger. Off par defaut (overhead). Pratique pour
            /// diagnostiquer un backend qui n'affiche rien sans recompiler.
            bool debug = false;

            /// Charge le fichier `pongConfigPath`. Si absent OU illisible OU
            /// ne contient pas la cle `backend=…`, sauvegarde un defaut puis
            /// retourne la config par defaut.
            ///
            /// Utilise `NkFile` (NKFileSystem) pour la portabilite multi-platform
            /// (Android : reroute via AAssetManager si necessaire ; Web : virtual
            /// FS Emscripten ; Harmony : NkFile sait gerer l'OHOS resource pack).
            static PongConfig LoadOrCreateDefault(const char* pongConfigPath = "pong.config") {
                PongConfig cfg{};
                NkFile f;
                if (!f.Open(pongConfigPath, NkFileMode::NK_READ_BINARY)) {
                    // Pas de config existante : on en cree une par defaut.
                    Save(cfg, pongConfigPath);
                    return cfg;
                }
                // Buffer large : le fichier contient des commentaires + plusieurs
                // cles (backend, debug, ...). 256 octets etaient trop justes et
                // tronquaient la cle debug= placee apres les commentaires.
                char buf[1024]{};
                const usize n = f.Read(buf, sizeof(buf) - 1);
                f.Close();
                buf[n] = '\0';

                // Parse simple : recherche de "backend=" (en debut de ligne ou
                // apres un newline). Tolere les commentaires lignes commencant
                // par #.
                const char* p = buf;
                while (p && *p) {
                    while (*p == ' ' || *p == '\t') ++p;
                    if (*p == '#') {
                        while (*p && *p != '\n') ++p;
                        if (*p == '\n') ++p;
                        continue;
                    }
                    if (std::strncmp(p, "backend=", 8) == 0) {
                        p += 8;
                        char value[32]{};
                        size_t k = 0;
                        while (*p && *p != '\n' && *p != '\r' && k < sizeof(value) - 1) {
                            value[k++] = *p++;
                        }
                        value[k] = '\0';
                        cfg.backend = ParseBackend(value);
                        continue;   // poursuivre : d'autres cles peuvent suivre (debug=)
                    }
                    if (std::strncmp(p, "debug=", 6) == 0) {
                        p += 6;
                        char value[16]{};
                        size_t k = 0;
                        while (*p && *p != '\n' && *p != '\r' && k < sizeof(value) - 1) {
                            value[k++] = *p++;
                        }
                        value[k] = '\0';
                        cfg.debug = (std::strcmp(value, "1") == 0
                                  || std::strcmp(value, "true") == 0
                                  || std::strcmp(value, "on")   == 0);
                        continue;
                    }
                    while (*p && *p != '\n') ++p;
                    if (*p == '\n') ++p;
                }
                return cfg;
            }

            /// Ecrit la config au format texte simple via NkFile (multi-platform).
            static void Save(const PongConfig& cfg, const char* path = "pong.config") {
                char buf[512];
                const int n = std::snprintf(buf, sizeof(buf),
                    "# Pong Ultra Arena - config dynamique du backend graphique.\n"
                    "# Valeurs : opengl | vulkan | dx11 | dx12 | software | auto.\n"
                    "# Editable a la main. Re-lue a chaque ouverture du jeu.\n"
                    "backend=%s\n"
                    "# debug=1 : active la validation GPU du backend (debug layer\n"
                    "# DX11/DX12, validation layers Vulkan) -> messages dans le log.\n"
                    "debug=%d\n",
                    BackendName(cfg.backend), cfg.debug ? 1 : 0);
                if (n <= 0) return;
                NkFile f;
                if (!f.Open(path, NkFileMode::NK_WRITE_BINARY)) return;
                f.Write(buf, static_cast<usize>(n));
                f.Close();
            }

            static const char* BackendName(NkGraphicsApi api) {
                switch (api) {
                    case NkGraphicsApi::NK_GFX_API_OPENGL:   return "opengl";
                    case NkGraphicsApi::NK_GFX_API_VULKAN:   return "vulkan";
                    case NkGraphicsApi::NK_GFX_API_DX11:     return "dx11";
                    case NkGraphicsApi::NK_GFX_API_DX12:     return "dx12";
                    case NkGraphicsApi::NK_GFX_API_SOFTWARE: return "software";
                    case NkGraphicsApi::NK_GFX_API_METAL:    return "metal";
                    case NkGraphicsApi::NK_GFX_API_AUTO:
                    default:                                  return "auto";
                }
            }

            static NkGraphicsApi ParseBackend(const char* value) {
                if (!value || !*value) return NkGraphicsApi::NK_GFX_API_OPENGL;
                if (std::strcmp(value, "opengl")   == 0) return NkGraphicsApi::NK_GFX_API_OPENGL;
                if (std::strcmp(value, "vulkan")   == 0) return NkGraphicsApi::NK_GFX_API_VULKAN;
                if (std::strcmp(value, "dx11")     == 0) return NkGraphicsApi::NK_GFX_API_DX11;
                if (std::strcmp(value, "dx12")     == 0) return NkGraphicsApi::NK_GFX_API_DX12;
                if (std::strcmp(value, "software") == 0) return NkGraphicsApi::NK_GFX_API_SOFTWARE;
                if (std::strcmp(value, "metal")    == 0) return NkGraphicsApi::NK_GFX_API_METAL;
                if (std::strcmp(value, "auto")     == 0) return NkGraphicsApi::NK_GFX_API_AUTO;
                return NkGraphicsApi::NK_GFX_API_OPENGL;
            }

            /// Selectionne le meilleur backend disponible pour la plateforme
            /// courante (utilise quand l'utilisateur a choisi `backend=auto`).
            /// Strategie :
            ///   Windows  : DX12 → DX11 → Vulkan → OpenGL → Software
            ///   Linux    : Vulkan → OpenGL → Software
            ///   macOS    : Metal → OpenGL → Software (Metal Renderer2D pas
            ///              implemente encore, repli OpenGL)
            ///   Android  : Vulkan → OpenGL ES → Software
            ///   HarmonyOS: Vulkan → OpenGL ES → Software
            ///   iOS      : Metal → Software (idem macOS, repli)
            ///   Web      : OpenGL (WebGL) → Software
            ///   Xbox/UWP : DX12
            /// On RETOURNE le candidat #1. Si NkRenderWindow::IsValid() est
            /// false apres init, l'app peut iterer manuellement les fallbacks
            /// suivants (cf. AppsCpp PickFallback()).
            static NkGraphicsApi PickBestForPlatform() {
            #if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP)
                return NkGraphicsApi::NK_GFX_API_DX12;
            #elif defined(NKENTSEU_PLATFORM_UWP) || defined(NKENTSEU_PLATFORM_XBOX)
                return NkGraphicsApi::NK_GFX_API_DX12;
            #elif defined(NKENTSEU_PLATFORM_LINUX)
                return NkGraphicsApi::NK_GFX_API_VULKAN;
            #elif defined(NKENTSEU_PLATFORM_MACOS)
                return NkGraphicsApi::NK_GFX_API_METAL;
            #elif defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_PLATFORM_HARMONYOS)
                return NkGraphicsApi::NK_GFX_API_VULKAN;
            #elif defined(NKENTSEU_PLATFORM_IOS)
                return NkGraphicsApi::NK_GFX_API_METAL;
            #else
                return NkGraphicsApi::NK_GFX_API_OPENGL;
            #endif
            }

            /// Liste ordonnee des fallbacks pour la plateforme courante (taille
            /// fixe : on remplit jusqu'a 6, le reste reste NK_GFX_API_AUTO).
            /// Utile pour iterer si le premier backend echoue l'init.
            struct FallbackChain {
                NkGraphicsApi apis[6];
                uint32        count;
            };
            
            static FallbackChain PlatformFallbackChain() {
                FallbackChain c{};
            #if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP)
                c.apis[0] = NkGraphicsApi::NK_GFX_API_DX12;
                c.apis[1] = NkGraphicsApi::NK_GFX_API_DX11;
                c.apis[2] = NkGraphicsApi::NK_GFX_API_VULKAN;
                c.apis[3] = NkGraphicsApi::NK_GFX_API_OPENGL;
                c.apis[4] = NkGraphicsApi::NK_GFX_API_SOFTWARE;
                c.count   = 5;
            #elif defined(NKENTSEU_PLATFORM_UWP) || defined(NKENTSEU_PLATFORM_XBOX)
                c.apis[0] = NkGraphicsApi::NK_GFX_API_DX12;
                c.apis[1] = NkGraphicsApi::NK_GFX_API_DX11;
                c.count   = 2;
            #elif defined(NKENTSEU_PLATFORM_LINUX)
                c.apis[0] = NkGraphicsApi::NK_GFX_API_VULKAN;
                c.apis[1] = NkGraphicsApi::NK_GFX_API_OPENGL;
                c.apis[2] = NkGraphicsApi::NK_GFX_API_SOFTWARE;
                c.count   = 3;
            #elif defined(NKENTSEU_PLATFORM_MACOS)
                c.apis[0] = NkGraphicsApi::NK_GFX_API_METAL;
                c.apis[1] = NkGraphicsApi::NK_GFX_API_OPENGL;
                c.apis[2] = NkGraphicsApi::NK_GFX_API_SOFTWARE;
                c.count   = 3;
            #elif defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_PLATFORM_HARMONYOS)
                c.apis[0] = NkGraphicsApi::NK_GFX_API_VULKAN;
                c.apis[1] = NkGraphicsApi::NK_GFX_API_OPENGL;
                c.apis[2] = NkGraphicsApi::NK_GFX_API_SOFTWARE;
                c.count   = 3;
            #elif defined(NKENTSEU_PLATFORM_IOS)
                c.apis[0] = NkGraphicsApi::NK_GFX_API_METAL;
                c.apis[1] = NkGraphicsApi::NK_GFX_API_SOFTWARE;
                c.count   = 2;
            #else
                c.apis[0] = NkGraphicsApi::NK_GFX_API_OPENGL;
                c.apis[1] = NkGraphicsApi::NK_GFX_API_SOFTWARE;
                c.count   = 2;
            #endif
                return c;
            }
        };

    } // namespace pong
} // namespace nkentseu
