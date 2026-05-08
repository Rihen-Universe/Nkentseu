// =============================================================================
// NkRendererResult.cpp  — NKRenderer v5.0
// Stockage thread-local du dernier code d'erreur + message.
// =============================================================================
#include "NkRendererResult.h"
#include <cstring>

namespace nkentseu {
    namespace renderer {

        // Buffer fixe par thread pour eviter toute allocation depuis les chemins
        // d'erreur (qui peuvent etre invoques depuis OOM).
        static constexpr size_t kMsgBufSize = 256;

        static thread_local NkRResult tLastError      = NkRResult::NK_OK;
        static thread_local char      tLastMsg[kMsgBufSize] = {0};

        NkRResult NkRGetLastError() noexcept {
            return tLastError;
        }

        const char* NkRGetLastErrorMessage() noexcept {
            return tLastMsg[0] ? tLastMsg : NkRString(tLastError);
        }

        void NkRSetLastError(NkRResult r, const char* msg) noexcept {
            tLastError = r;
            if (msg) {
                size_t n = strlen(msg);
                if (n >= kMsgBufSize) n = kMsgBufSize - 1;
                memcpy(tLastMsg, msg, n);
                tLastMsg[n] = 0;
            } else {
                tLastMsg[0] = 0;
            }
        }

        void NkRClearLastError() noexcept {
            tLastError = NkRResult::NK_OK;
            tLastMsg[0] = 0;
        }

    } // namespace renderer
} // namespace nkentseu
