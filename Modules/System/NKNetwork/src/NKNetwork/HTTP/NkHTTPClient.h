#pragma once
// =============================================================================
// NKNetwork/HTTP/NkHTTPClient.h
// =============================================================================
// Client HTTP/HTTPS pour les APIs REST (CDN, telemetry, auth, leaderboards).
//
// UTILISATION TYPIQUE :
//   NkHTTPClient http;
//
//   // GET synchrone
//   NkHTTPResponse r = http.Get("https://api.example.com/version");
//   if (r.IsOK()) { printf("%s\n", r.body.CStr()); }
//
//   // POST asynchrone
//   NkHTTPRequest req;
//   req.url    = "https://api.example.com/score";
//   req.method = NkHTTPMethod::POST;
//   req.SetJSON("{\"score\":1000,\"player\":\"Alice\"}");
//   req.AddHeader("Authorization", "Bearer token123");
//   http.SendAsync(req, [](const NkHTTPResponse& r) {
//       if (r.IsOK()) { /* traiter la réponse */ }
//   });
//
// NOTES IMPL :
//   Implémenté via TCP + parsing HTTP/1.1 à la main (pas de libcurl).
//   HTTPS via mbedTLS ou OpenSSL selon la plateforme (ifdefs).
//   Wasm : délégué à emscripten_fetch.
// =============================================================================

#include "Protocol/NkConnection.h"

namespace nkentseu {
    namespace net {

        // =====================================================================
        // NkHTTPMethod
        // =====================================================================
        enum class NkHTTPMethod : uint8 { GET, POST, PUT, PATCH, DELETE, HEAD };

        // =====================================================================
        // NkHTTPHeader — en-tête HTTP clé=valeur
        // =====================================================================
        struct NkHTTPHeader {
            NkString key;
            NkString value;
        };

        // =====================================================================
        // NkHTTPRequest — requête HTTP
        // =====================================================================
        struct NkHTTPRequest {
            NkString              url;
            NkHTTPMethod          method   = NkHTTPMethod::GET;
            NkVector<NkHTTPHeader> headers;
            NkString              body;      ///< Corps de la requête
            uint32                timeoutMs = 10000;
            bool                  followRedirects = true;
            uint32                maxRedirects    = 5;

            // ── Helpers ───────────────────────────────────────────────────────
            void SetJSON(const char* json) noexcept {
                body = json;
                AddHeader("Content-Type", "application/json");
                AddHeader("Accept", "application/json");
            }

            void SetFormData(const char* formData) noexcept {
                body = formData;
                AddHeader("Content-Type", "application/x-www-form-urlencoded");
            }

            void AddHeader(const char* key, const char* value) noexcept {
                headers.PushBack({NkString(key), NkString(value)});
            }

            void SetBearerToken(const char* token) noexcept {
                char buf[512];
                snprintf(buf, sizeof(buf), "Bearer %s", token);
                AddHeader("Authorization", buf);
            }

            void SetBasicAuth(const char* user, const char* pass) noexcept;
        };

        // =====================================================================
        // NkHTTPResponse — réponse HTTP
        // =====================================================================
        struct NkHTTPResponse {
            uint32   statusCode = 0;
            NkString statusText;
            NkVector<NkHTTPHeader> headers;
            NkString body;
            NkString error;      ///< Message d'erreur réseau (pas HTTP)
            uint32   timeMs = 0; ///< Temps de la requête en ms

            [[nodiscard]] bool IsOK()         const noexcept { return statusCode >= 200 && statusCode < 300; }
            [[nodiscard]] bool IsRedirect()   const noexcept { return statusCode >= 300 && statusCode < 400; }
            [[nodiscard]] bool IsClientError()const noexcept { return statusCode >= 400 && statusCode < 500; }
            [[nodiscard]] bool IsServerError()const noexcept { return statusCode >= 500 && statusCode < 600; }
            [[nodiscard]] bool HasError()     const noexcept { return !error.IsEmpty(); }

            [[nodiscard]] const NkString* GetHeader(const char* key) const noexcept {
                for (const auto& h : headers)
                    if (h.key == key) return &h.value;
                return nullptr;
            }
        };

        // =====================================================================
        // NkHTTPClient — client HTTP synchrone + asynchrone
        // =====================================================================
        class NkHTTPClient {
        public:
            NkHTTPClient()  noexcept = default;
            ~NkHTTPClient() noexcept = default;

            // ── Configuration ─────────────────────────────────────────────────
            struct Config {
                NkString  userAgent = "NKNetwork/1.0";
                uint32    defaultTimeoutMs = 10000;
                bool      verifySSL   = true;
                NkString  caCertPath;           ///< CA bundle (vide = system)
                uint32    maxAsyncThreads = 4;
            };

            void Configure(const Config& cfg) noexcept { mConfig = cfg; }

            // ── En-têtes par défaut ────────────────────────────────────────────
            void AddDefaultHeader(const char* key, const char* value) noexcept {
                mDefaultHeaders.PushBack({NkString(key), NkString(value)});
            }
            void SetDefaultBearerToken(const char* token) noexcept;

            // ── Requêtes synchrones (bloquantes) ─────────────────────────────
            [[nodiscard]] NkHTTPResponse Send(const NkHTTPRequest& req) noexcept;
            [[nodiscard]] NkHTTPResponse Get (const char* url) noexcept;
            [[nodiscard]] NkHTTPResponse Post(const char* url,
                                               const char* json) noexcept;

            // ── Requêtes asynchrones (non-bloquantes) ─────────────────────────
            using Callback = NkFunction<void(const NkHTTPResponse&)>;

            /**
             * @brief Envoie une requête asynchrone.
             * Le callback est appelé depuis le thread réseau.
             * → Utiliser NkGameplayEventBus::Queue() pour ramener sur le thread jeu.
             * @return ID de la requête (pour l'annuler si besoin).
             */
            uint32 SendAsync(const NkHTTPRequest& req, Callback cb) noexcept;

            /**
             * @brief Annule une requête asynchrone en cours.
             */
            void Cancel(uint32 requestId) noexcept;

            /**
             * @brief Annule toutes les requêtes en cours.
             */
            void CancelAll() noexcept;

            /**
             * @brief Attend la fin de toutes les requêtes asynchrones.
             */
            void WaitAll(uint32 timeoutMs = UINT32_MAX) noexcept;

            /**
             * @brief Nombre de requêtes asynchrones en cours.
             */
            [[nodiscard]] uint32 PendingCount() const noexcept;

            // ── Téléchargement de fichiers ────────────────────────────────────
            /**
             * @brief Télécharge un fichier vers le disque.
             * @param url         URL source.
             * @param destPath    Chemin de destination local.
             * @param onProgress  Appelé régulièrement (bytesReceived, totalBytes).
             */
            uint32 DownloadFile(const char* url,
                                  const char* destPath,
                                  NkFunction<void(uint64, uint64)> onProgress = {},
                                  Callback onComplete = {}) noexcept;

            // ── Utilitaires statiques ─────────────────────────────────────────
            /**
             * @brief Encode une chaîne pour l'URL (percent-encoding).
             */
            [[nodiscard]] static NkString URLEncode(const char* s) noexcept;

            /**
             * @brief Décode une chaîne URL.
             */
            [[nodiscard]] static NkString URLDecode(const char* s) noexcept;

            /**
             * @brief Encode en Base64.
             */
            [[nodiscard]] static NkString Base64Encode(const uint8* data,
                                                         uint32 size) noexcept;

        private:
            // ── Parsing HTTP ──────────────────────────────────────────────────
            NkHTTPResponse ParseResponse(const NkString& raw) const noexcept;
            NkString       BuildRequestStr(const NkHTTPRequest& req) const noexcept;
            bool           ParseURL(const NkString& url,
                                     NkString& scheme, NkString& host,
                                     uint16& port, NkString& path) const noexcept;

            // ── TCP + TLS ─────────────────────────────────────────────────────
            NkHTTPResponse SendOverTCP(const NkHTTPRequest& req) noexcept;
            NkHTTPResponse SendOverTLS(const NkHTTPRequest& req) noexcept;

            // ── Async pool ────────────────────────────────────────────────────
            struct AsyncRequest {
                uint32         id       = 0;
                NkHTTPRequest  req;
                Callback       callback;
                threading::NkAtomic<bool> cancelled{false};
                threading::NkAtomic<bool> done{false};
            };

            NkVector<AsyncRequest*>   mAsyncRequests;
            threading::NkMutex        mAsyncMutex;
            threading::NkAtomic<uint32> mNextReqId{1};

            Config                    mConfig;
            NkVector<NkHTTPHeader>    mDefaultHeaders;
        };

        // =====================================================================
        // NkLeaderboard — leaderboard REST générique
        // =====================================================================
        struct NkLeaderboardEntry {
            char    playerName[64] = {};
            uint64  score          = 0;
            uint32  rank           = 0;
            char    extraData[256] = {};  ///< JSON additionnel
        };

        class NkLeaderboard {
        public:
            void Configure(const char* baseUrl,
                            const char* apiKey) noexcept;

            /**
             * @brief Soumet un score.
             */
            void SubmitScore(const char* playerName,
                              uint64 score,
                              NkFunction<void(bool success)> cb = {}) noexcept;

            /**
             * @brief Récupère le top N.
             */
            void FetchTop(uint32 count,
                           NkFunction<void(NkVector<NkLeaderboardEntry>&)> cb) noexcept;

            /**
             * @brief Récupère le rang d'un joueur.
             */
            void FetchPlayerRank(const char* playerName,
                                  NkFunction<void(uint32 rank, uint64 score)> cb) noexcept;

        private:
            NkHTTPClient mHttp;
            NkString     mBaseUrl;
            NkString     mApiKey;
        };

    } // namespace net
} // namespace nkentseu
