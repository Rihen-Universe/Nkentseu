#pragma once
// =============================================================================
// NkRendererResult.h  — NKRenderer v5.0  (Core/)
//
// Codes d'erreur unifies + helpers d'assertion. Toute fonction publique du
// renderer renvoie soit `NkRResult` (operations sans valeur), soit un handle
// dont `IsValid()` == false signifie echec ; dans ce dernier cas, le code
// d'erreur de la derniere operation est consultable via `NkRGetLastError()`.
//
// Conception inspiree de l'API VkResult (Vulkan) et HRESULT (DirectX) :
//  - Codes positifs = succes / info
//  - Codes negatifs = erreur recoverable
//  - Codes severement negatifs = erreur fatale (perte de device, OOM, etc.)
// =============================================================================
#include "NKCore/NkTypes.h"

namespace nkentseu {
    namespace renderer {

        // =====================================================================
        // Codes de resultat
        // =====================================================================
        enum class NkRResult : int32 {
            // Succes / Info
            NK_OK                         =  0,
            NK_NOT_READY                  =  1,   // operation pending (compile async, upload async)
            NK_PARTIAL                    =  2,   // succes partiel (ex : mipmaps generes pour quelques niveaux seulement)
            NK_FALLBACK_USED              =  3,   // le path principal a echoue, le fallback a reussi

            // Erreurs recoverables
            NK_ERR_INVALID_DEVICE         = -1,   // device invalide ou non-initialise
            NK_ERR_INVALID_HANDLE         = -2,   // handle null ou inexistant
            NK_ERR_INVALID_ARGUMENT       = -3,   // parametre hors borne / nullptr requis
            NK_ERR_NOT_SUPPORTED          = -4,   // feature non disponible sur ce backend
            NK_ERR_NOT_FOUND              = -5,   // ressource introuvable (asset, descripteur)
            NK_ERR_ALREADY_EXISTS         = -6,   // tentative de creation de ressource avec un nom deja pris
            NK_ERR_COMPILE_FAILED         = -7,   // shader/pipeline compile fail
            NK_ERR_LINK_FAILED            = -8,   // shader program link fail
            NK_ERR_IO                     = -9,   // erreur fichier / reseau
            NK_ERR_BAD_FORMAT             = -10,  // format pixel/vertex non gere

            // Erreurs fatales
            NK_ERR_OUT_OF_MEMORY          = -100, // OOM CPU ou GPU
            NK_ERR_DEVICE_LOST            = -101, // device reset / driver crash
            NK_ERR_DEVICE_REMOVED         = -102, // GPU debranche / desactive
            NK_ERR_VALIDATION_FAILED      = -103, // validation layer a tue le device
            NK_ERR_UNKNOWN                = -999,
        };

        inline constexpr bool NkROk(NkRResult r) noexcept {
            return static_cast<int32>(r) >= 0;
        }
        inline constexpr bool NkRFatal(NkRResult r) noexcept {
            return static_cast<int32>(r) <= -100;
        }

        // String d'affichage (utile pour les logs)
        inline const char* NkRString(NkRResult r) noexcept {
            switch (r) {
                case NkRResult::NK_OK:                    return "NK_OK";
                case NkRResult::NK_NOT_READY:             return "NK_NOT_READY";
                case NkRResult::NK_PARTIAL:               return "NK_PARTIAL";
                case NkRResult::NK_FALLBACK_USED:         return "NK_FALLBACK_USED";
                case NkRResult::NK_ERR_INVALID_DEVICE:    return "NK_ERR_INVALID_DEVICE";
                case NkRResult::NK_ERR_INVALID_HANDLE:    return "NK_ERR_INVALID_HANDLE";
                case NkRResult::NK_ERR_INVALID_ARGUMENT:  return "NK_ERR_INVALID_ARGUMENT";
                case NkRResult::NK_ERR_NOT_SUPPORTED:     return "NK_ERR_NOT_SUPPORTED";
                case NkRResult::NK_ERR_NOT_FOUND:         return "NK_ERR_NOT_FOUND";
                case NkRResult::NK_ERR_ALREADY_EXISTS:    return "NK_ERR_ALREADY_EXISTS";
                case NkRResult::NK_ERR_COMPILE_FAILED:    return "NK_ERR_COMPILE_FAILED";
                case NkRResult::NK_ERR_LINK_FAILED:       return "NK_ERR_LINK_FAILED";
                case NkRResult::NK_ERR_IO:                return "NK_ERR_IO";
                case NkRResult::NK_ERR_BAD_FORMAT:        return "NK_ERR_BAD_FORMAT";
                case NkRResult::NK_ERR_OUT_OF_MEMORY:     return "NK_ERR_OUT_OF_MEMORY";
                case NkRResult::NK_ERR_DEVICE_LOST:       return "NK_ERR_DEVICE_LOST";
                case NkRResult::NK_ERR_DEVICE_REMOVED:    return "NK_ERR_DEVICE_REMOVED";
                case NkRResult::NK_ERR_VALIDATION_FAILED: return "NK_ERR_VALIDATION_FAILED";
                default:                                  return "NK_ERR_UNKNOWN";
            }
        }

        // =====================================================================
        // Last-error TLS access (set par les fonctions internes via NkRSetLastError)
        // =====================================================================
        NkRResult   NkRGetLastError() noexcept;
        const char* NkRGetLastErrorMessage() noexcept;
        void        NkRSetLastError(NkRResult r, const char* msg = nullptr) noexcept;
        void        NkRClearLastError() noexcept;

        // =====================================================================
        // Macros helpers
        // =====================================================================
        // Retourne r si r est une erreur, sinon continue
        #define NK_R_TRY(expr) do { ::nkentseu::renderer::NkRResult _r = (expr); \
            if (!::nkentseu::renderer::NkROk(_r)) return _r; } while(0)

        // Set last error si la condition est fausse, et return false
        #define NK_R_REQUIRE(cond, code, msg) do { if (!(cond)) { \
            ::nkentseu::renderer::NkRSetLastError(code, msg); return false; } } while(0)

    } // namespace renderer
} // namespace nkentseu
