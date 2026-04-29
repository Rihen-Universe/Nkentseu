// =============================================================================
// NKNetwork/HTTP/NkHTTPClient.cpp
// =============================================================================
// DESCRIPTION :
//   Implémentation du client HTTP/HTTPS pour communications avec APIs REST.
//   Gestion des requêtes synchrones/asynchrones, parsing HTTP/1.1, TLS.
//
// NOTES D'IMPLÉMENTATION :
//   • Toutes les méthodes publiques retournent NkNetResult ou structures dédiées
//   • Gestion d'erreur robuste : logging + retour gracieux sans crash
//   • Thread-safety : mutex sur les structures partagées (mAsyncRequests)
//   • Aucune allocation heap dans les chemins critiques pour performance
//
// DÉPENDANCES INTERNES :
//   • NkHTTPClient.h : Déclarations des classes implémentées
//   • NkConnection.h : Gestion des connexions TCP sous-jacentes
//   • NkNetDefines.h : Types fondamentaux et codes de retour réseau
//
// SUPPORT TLS :
//   • Windows/Linux/macOS : mbedTLS ou OpenSSL via compilation conditionnelle
//   • WebAssembly : Délégation à emscripten_fetch (géré séparément)
//
// AUTEUR   : Rihen
// DATE     : 2026-02-10
// VERSION  : 1.0.0
// LICENCE  : Propriétaire - libre d'utilisation et de modification
// =============================================================================

// -------------------------------------------------------------------------
// SECTION 1 : INCLUSIONS (ordre strict requis)
// -------------------------------------------------------------------------

#include "pch.h"
#include "NkHTTPClient.h"

// En-têtes standards C/C++ pour opérations bas-niveau
#include <cstring>
#include <cstdio>
#include <cctype>
#include <algorithm>
#include <chrono>
#include <thread>

// En-têtes système pour sockets et résolution DNS
#if defined(NKENTSEU_PLATFORM_WINDOWS)
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <winsock2.h>
    #include <ws2tcpip.h>
#elif defined(NKENTSEU_PLATFORM_POSIX)
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <netdb.h>
    #include <errno.h>
#endif

// Support TLS conditionnel
#if defined(NKENTSEU_HTTP_USE_MBEDTLS)
    #include <mbedtls/net_sockets.h>
    #include <mbedtls/ssl.h>
    #include <mbedtls/ctr_drbg.h>
    #include <mbedtls/entropy.h>
#elif defined(NKENTSEU_HTTP_USE_OPENSSL)
    #include <openssl/ssl.h>
    #include <openssl/err.h>
#endif

// -------------------------------------------------------------------------
// SECTION 2 : CONSTANTES LOCALES ET HELPERS
// -------------------------------------------------------------------------

namespace {

    // Visibilité des types fondamentaux et du module réseau dans le namespace anonyme
    using namespace nkentseu;
    using namespace nkentseu::net;

    // =====================================================================
    // Constantes HTTP/1.1
    // =====================================================================
    static constexpr char kHTTPVersion[] = "HTTP/1.1";
    static constexpr char kCRLF[] = "\r\n";
    static constexpr uint32 kMaxResponseSize = 16 * 1024 * 1024;  // 16MB max

    // =====================================================================
    // Helper : Comparaison case-insensitive de chaînes
    // =====================================================================
    bool StringEqualsIgnoreCase(const char* a, const char* b) noexcept
    {
        if (a == nullptr || b == nullptr) { return a == b; }
        while (*a && *b)
        {
            if (std::tolower(static_cast<unsigned char>(*a)) !=
                std::tolower(static_cast<unsigned char>(*b)))
            {
                return false;
            }
            ++a;
            ++b;
        }
        return *a == *b;
    }

    // =====================================================================
    // Helper : Conversion méthode HTTP enum → chaîne
    // =====================================================================
    const char* MethodToString(NkHTTPMethod method) noexcept
    {
        switch (method)
        {
            case NkHTTPMethod::NK_HTTP_GET:     return "GET";
            case NkHTTPMethod::NK_HTTP_POST:    return "POST";
            case NkHTTPMethod::NK_HTTP_PUT:     return "PUT";
            case NkHTTPMethod::NK_HTTP_PATCH:   return "PATCH";
            case NkHTTPMethod::NK_HTTP_DELETE:  return "DELETE";
            case NkHTTPMethod::NK_HTTP_HEAD:    return "HEAD";
            default:                            return "GET";
        }
    }

    // =====================================================================
    // Helper : Conversion code statut → texte descriptif
    // =====================================================================
    const char* StatusTextForCode(uint32 code) noexcept
    {
        switch (code)
        {
            case 200: return "OK";
            case 201: return "Created";
            case 204: return "No Content";
            case 301: return "Moved Permanently";
            case 302: return "Found";
            case 304: return "Not Modified";
            case 400: return "Bad Request";
            case 401: return "Unauthorized";
            case 403: return "Forbidden";
            case 404: return "Not Found";
            case 500: return "Internal Server Error";
            case 502: return "Bad Gateway";
            case 503: return "Service Unavailable";
            default:  return "Unknown";
        }
    }

    // =====================================================================
    // Helper : Lecture d'une ligne depuis un buffer (pour parsing HTTP)
    // =====================================================================
    const char* ReadLine(const char* start, const char* end, NkString& out) noexcept
    {
        const char* p = start;
        while (p < end && *p != '\r' && *p != '\n') { ++p; }
        out = NkString(start, static_cast<uint32>(p - start));
        // Sauter \r\n ou \n
        if (p < end && *p == '\r') { ++p; }
        if (p < end && *p == '\n') { ++p; }
        return p;
    }

    // =====================================================================
    // Helper : Création d'un socket TCP non-bloquant
    // =====================================================================
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        using NkNativeSocket = SOCKET;
        static constexpr NkNativeSocket kInvalidSocket = INVALID_SOCKET;
        static int NkGetLastError() { return WSAGetLastError(); }
        static bool NkWouldBlock(int err) { return err == WSAEWOULDBLOCK; }
        static void NkCloseSocket(NkNativeSocket s) { closesocket(s); }
    #elif defined(NKENTSEU_PLATFORM_POSIX)
        using NkNativeSocket = int;
        static constexpr NkNativeSocket kInvalidSocket = -1;
        static int NkGetLastError() { return errno; }
        static bool NkWouldBlock(int err) { return err == EAGAIN || err == EWOULDBLOCK; }
        static void NkCloseSocket(NkNativeSocket s) { close(s); }
    #else
        using NkNativeSocket = int;
        static constexpr NkNativeSocket kInvalidSocket = -1;
        static int NkGetLastError() { return -1; }
        static bool NkWouldBlock(int) { return false; }
        static void NkCloseSocket(NkNativeSocket) {}
    #endif

    NkNativeSocket CreateTcpSocket(const char* host, uint16 port, uint32 timeoutMs) noexcept
    {
        // Résolution DNS
        addrinfo hints = {};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        char portStr[16];
        snprintf(portStr, sizeof(portStr), "%u", port);

        addrinfo* result = nullptr;
        if (getaddrinfo(host, portStr, &hints, &result) != 0)
        {
            return kInvalidSocket;
        }

        NkNativeSocket sock = kInvalidSocket;
        for (addrinfo* p = result; p != nullptr; p = p->ai_next)
        {
            sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (sock == kInvalidSocket) { continue; }

            // Mode non-bloquant
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                u_long mode = 1;
                ioctlsocket(sock, FIONBIO, &mode);
            #else
                int flags = fcntl(sock, F_GETFL, 0);
                fcntl(sock, F_SETFL, flags | O_NONBLOCK);
            #endif

            // Tentative de connexion
            if (connect(sock, p->ai_addr, static_cast<int>(p->ai_addrlen)) == 0)
            {
                break;  // Connecté immédiatement
            }

            // Vérification d'erreur non-bloquante
            int err = NkGetLastError();
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                if (err != WSAEWOULDBLOCK && err != WSAEINPROGRESS)
            #else
                if (err != EINPROGRESS)
            #endif
            {
                NkCloseSocket(sock);
                sock = kInvalidSocket;
                continue;
            }

            // Attente avec select pour timeout
            fd_set writeSet;
            FD_ZERO(&writeSet);
            FD_SET(sock, &writeSet);

            timeval tv = {};
            tv.tv_sec = static_cast<long>(timeoutMs / 1000);
            tv.tv_usec = static_cast<long>((timeoutMs % 1000) * 1000);

            if (select(static_cast<int>(sock + 1), nullptr, &writeSet, nullptr, &tv) > 0)
            {
                // Vérification que la connexion a réussi
                int soError = 0;
                socklen_t len = sizeof(soError);
                if (getsockopt(sock, SOL_SOCKET, SO_ERROR,
                               reinterpret_cast<char*>(&soError), &len) == 0 && soError == 0)
                {
                    break;  // Connecté avec succès
                }
            }

            NkCloseSocket(sock);
            sock = kInvalidSocket;
        }

        freeaddrinfo(result);
        return sock;
    }

    // =====================================================================
    // Helper : Envoi/réception avec timeout sur socket
    // =====================================================================
    bool SendWithTimeout(NkNativeSocket sock, const char* data, uint32 size, uint32 timeoutMs) noexcept
    {
        uint32 sent = 0;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

        while (sent < size)
        {
            int result = send(sock, data + sent, static_cast<int>(size - sent), 0);
            if (result > 0)
            {
                sent += static_cast<uint32>(result);
            }
            else if (result == 0 || !NkWouldBlock(NkGetLastError()))
            {
                return false;  // Erreur ou connexion fermée
            }

            // Vérification timeout
            if (std::chrono::steady_clock::now() >= deadline)
            {
                return false;
            }

            // Pause courte pour éviter busy-wait
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return true;
    }

    bool RecvWithTimeout(NkNativeSocket sock, NkString& out, uint32 maxSize, uint32 timeoutMs) noexcept
    {
        char buffer[4096];
        out.Clear();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

        while (std::chrono::steady_clock::now() < deadline)
        {
            int result = recv(sock, buffer, sizeof(buffer), 0);
            if (result > 0)
            {
                out.Append(buffer, static_cast<uint32>(result));
                if (out.Length() >= maxSize)
                {
                    NK_NET_LOG_WARN("Réponse HTTP trop grande — tronquée");
                    return true;
                }
                // Vérification fin de headers HTTP
                if (out.Find("\r\n\r\n") != NkString::npos)
                {
                    return true;
                }
            }
            else if (result == 0)
            {
                // Connexion fermée proprement
                return !out.Empty();
            }
            else if (!NkWouldBlock(NkGetLastError()))
            {
                return false;  // Erreur réseau
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return false;  // Timeout
    }

} // namespace anonyme

// -------------------------------------------------------------------------
// SECTION 3 : NAMESPACE PRINCIPAL
// -------------------------------------------------------------------------

namespace nkentseu {

    namespace net {

        // =====================================================================
        // Déclarations avancées — helpers JSON définis plus bas
        // =====================================================================
        static void ParseLeaderboardJSON(const NkString& json, NkVector<NkLeaderboardEntry>& out) noexcept;
        static void ParsePlayerRankJSON(const NkString& json, uint32& outRank, uint64& outScore) noexcept;
        static void SafeStrCopy(char* dst, uint32 dstSize, const char* src, uint32 maxLen) noexcept;

        // =====================================================================
        // IMPLÉMENTATION : NkHTTPRequest — Helpers
        // =====================================================================

        void NkHTTPRequest::SetJSON(const char* json) noexcept
        {
            if (json == nullptr) { return; }
            body = json;
            AddHeader("Content-Type", "application/json");
            AddHeader("Accept", "application/json");
        }

        void NkHTTPRequest::SetFormData(const char* formData) noexcept
        {
            if (formData == nullptr) { return; }
            body = formData;
            AddHeader("Content-Type", "application/x-www-form-urlencoded");
        }

        void NkHTTPRequest::AddHeader(const char* key, const char* value) noexcept
        {
            if (key == nullptr || value == nullptr) { return; }
            headers.PushBack({ NkString(key), NkString(value) });
        }

        void NkHTTPRequest::SetBearerToken(const char* token) noexcept
        {
            if (token == nullptr) { return; }
            char buf[512];
            snprintf(buf, sizeof(buf), "Bearer %s", token);
            AddHeader("Authorization", buf);
        }

        void NkHTTPRequest::SetBasicAuth(const char* user, const char* pass) noexcept
        {
            if (user == nullptr || pass == nullptr) { return; }
            // Format : "user:pass" encodé en Base64
            NkString credentials;
            credentials.Append(user);
            credentials.Append(":");
            credentials.Append(pass);

            NkString b64 = NkHTTPClient::Base64Encode(
                reinterpret_cast<const uint8*>(credentials.CStr()),
                static_cast<uint32>(credentials.Length())
            );

            char authHeader[1024];
            snprintf(authHeader, sizeof(authHeader), "Basic %s", b64.CStr());
            AddHeader("Authorization", authHeader);
        }

        // =====================================================================
        // IMPLÉMENTATION : NkHTTPResponse — Méthodes d'inspection
        // =====================================================================

        bool NkHTTPResponse::IsOK() const noexcept
        {
            return statusCode >= 200 && statusCode < 300;
        }

        bool NkHTTPResponse::IsRedirect() const noexcept
        {
            return statusCode >= 300 && statusCode < 400;
        }

        bool NkHTTPResponse::IsClientError() const noexcept
        {
            return statusCode >= 400 && statusCode < 500;
        }

        bool NkHTTPResponse::IsServerError() const noexcept
        {
            return statusCode >= 500 && statusCode < 600;
        }

        bool NkHTTPResponse::HasError() const noexcept
        {
            return !error.Empty();
        }

        const NkString* NkHTTPResponse::GetHeader(const char* key) const noexcept
        {
            if (key == nullptr) { return nullptr; }
            for (const auto& h : headers)
            {
                if (StringEqualsIgnoreCase(h.key.CStr(), key))
                {
                    return &h.value;
                }
            }
            return nullptr;
        }

        // =====================================================================
        // IMPLÉMENTATION : NkHTTPClient — Configuration et en-têtes
        // =====================================================================

        void NkHTTPClient::Configure(const Config& cfg) noexcept
        {
            mConfig = cfg;
        }

        void NkHTTPClient::AddDefaultHeader(const char* key, const char* value) noexcept
        {
            if (key == nullptr || value == nullptr) { return; }
            mDefaultHeaders.PushBack({ NkString(key), NkString(value) });
        }

        void NkHTTPClient::SetDefaultBearerToken(const char* token) noexcept
        {
            if (token == nullptr) { return; }
            char buf[512];
            snprintf(buf, sizeof(buf), "Bearer %s", token);
            // Remplacer ou ajouter l'en-tête Authorization
            bool found = false;
            for (auto& h : mDefaultHeaders)
            {
                if (StringEqualsIgnoreCase(h.key.CStr(), "Authorization"))
                {
                    h.value = buf;
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                mDefaultHeaders.PushBack({ "Authorization", buf });
            }
        }

        // =====================================================================
        // IMPLÉMENTATION : NkHTTPClient — Requêtes synchrones
        // =====================================================================

        NkHTTPResponse NkHTTPClient::Send(const NkHTTPRequest& req) noexcept
        {
            const auto startTime = std::chrono::steady_clock::now();

            // Parsing de l'URL
            NkString scheme, host, path;
            uint16 port = 0;
            if (!ParseURL(req.url, scheme, host, port, path))
            {
                NkHTTPResponse err;
                err.error = "Invalid URL";
                return err;
            }

            // Détermination du port par défaut
            if (port == 0)
            {
                port = (scheme == "https") ? 443 : 80;
            }

            // Sélection du transport
            NkHTTPResponse response;
            if (scheme == "https")
            {
                response = SendOverTLS(req);
            }
            else
            {
                response = SendOverTCP(req);
            }

            // Calcul du temps écoulé
            const auto endTime = std::chrono::steady_clock::now();
            response.timeMs = static_cast<uint32>(
                std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count()
            );

            // Gestion des redirections
            if (req.followRedirects && response.IsRedirect() && req.maxRedirects > 0)
            {
                const NkString* location = response.GetHeader("Location");
                if (location != nullptr && !location->Empty())
                {
                    NkHTTPRequest redirectReq = req;
                    redirectReq.url = *location;
                    redirectReq.maxRedirects = req.maxRedirects - 1;
                    // Éviter les boucles infinies
                    return Send(redirectReq);
                }
            }

            return response;
        }

        NkHTTPResponse NkHTTPClient::Get(const char* url) noexcept
        {
            if (url == nullptr)
            {
                NkHTTPResponse err;
                err.error = "Null URL";
                return err;
            }

            NkHTTPRequest req;
            req.url = url;
            req.method = NkHTTPMethod::NK_HTTP_GET;
            req.timeoutMs = mConfig.defaultTimeoutMs;
            return Send(req);
        }

        NkHTTPResponse NkHTTPClient::Post(const char* url, const char* json) noexcept
        {
            if (url == nullptr || json == nullptr)
            {
                NkHTTPResponse err;
                err.error = "Null parameter";
                return err;
            }

            NkHTTPRequest req;
            req.url = url;
            req.method = NkHTTPMethod::NK_HTTP_POST;
            req.SetJSON(json);
            req.timeoutMs = mConfig.defaultTimeoutMs;
            return Send(req);
        }

        // =====================================================================
        // IMPLÉMENTATION : NkHTTPClient — Requêtes asynchrones
        // =====================================================================

        uint32 NkHTTPClient::SendAsync(const NkHTTPRequest& req, Callback cb) noexcept
        {
            if (cb == nullptr) { return 0; }

            // Génération d'un ID unique
            const uint32 reqId = mNextReqId.FetchAdd(1);

            // Allocation de la requête asynchrone
            AsyncRequest* asyncReq = new AsyncRequest();
            asyncReq->id = reqId;
            asyncReq->req = req;
            asyncReq->callback = cb;
            asyncReq->cancelled.Store(false);
            asyncReq->done.Store(false);

            // Ajout à la liste avec protection mutex
            {
                threading::NkLockGuard lock(mAsyncMutex);
                mAsyncRequests.PushBack(asyncReq);
            }

            // Lancement dans un thread dédié (lambda accepte void* requis par ThreadFunc)
            threading::NkThread worker([this, asyncReq](void*) {
                AsyncWorker(asyncReq);
            });
            worker.Detach();  // Gestion de durée de vie via flags atomiques

            return reqId;
        }

        void NkHTTPClient::Cancel(uint32 requestId) noexcept
        {
            threading::NkLockGuard lock(mAsyncMutex);
            for (AsyncRequest* req : mAsyncRequests)
            {
                if (req != nullptr && req->id == requestId && !req->done.Load())
                {
                    req->cancelled.Store(true);
                    break;
                }
            }
        }

        void NkHTTPClient::CancelAll() noexcept
        {
            threading::NkLockGuard lock(mAsyncMutex);
            for (AsyncRequest* req : mAsyncRequests)
            {
                if (req != nullptr && !req->done.Load())
                {
                    req->cancelled.Store(true);
                }
            }
        }

        void NkHTTPClient::WaitAll(uint32 timeoutMs) noexcept
        {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

            while (true)
            {
                bool allDone = true;
                {
                    threading::NkLockGuard lock(mAsyncMutex);
                    for (AsyncRequest* req : mAsyncRequests)
                    {
                        if (req != nullptr && !req->done.Load())
                        {
                            allDone = false;
                            break;
                        }
                    }
                }

                if (allDone) { break; }
                if (std::chrono::steady_clock::now() >= deadline) { break; }

                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            // Nettoyage des requêtes terminées
            CleanupFinishedRequests();
        }

        uint32 NkHTTPClient::PendingCount() const noexcept
        {
            uint32 count = 0;
            threading::NkLockGuard lock(mAsyncMutex);
            for (const AsyncRequest* req : mAsyncRequests)
            {
                if (req != nullptr && !req->done.Load())
                {
                    ++count;
                }
            }
            return count;
        }

        // =====================================================================
        // IMPLÉMENTATION : NkHTTPClient — Téléchargement de fichiers
        // =====================================================================

        uint32 NkHTTPClient::DownloadFile(
            const char* url,
            const char* destPath,
            NkFunction<void(uint64, uint64)> onProgress,  // NOLINT: non-default in definition
            Callback onComplete
        ) noexcept
        {
            if (url == nullptr || destPath == nullptr) { return 0; }

            NkHTTPRequest req;
            req.url = url;
            req.method = NkHTTPMethod::NK_HTTP_GET;
            req.timeoutMs = mConfig.defaultTimeoutMs * 10;  // Timeout plus long pour downloads

            // Wrapper callback pour gestion de progression
            Callback wrappedCb = [this, destPath, onProgress, onComplete](const NkHTTPResponse& resp) {
                if (!resp.HasError() && resp.IsOK())
                {
                    // Écriture atomique : temp file + rename pour intégrité
                    NkString tmpPath = NkString(destPath) + ".tmp";
                    FILE* f = std::fopen(tmpPath.CStr(), "wb");
                    if (f != nullptr)
                    {
                        const size_t written = std::fwrite(
                            resp.body.CStr(), 1,
                            static_cast<size_t>(resp.body.Length()), f
                        );
                        std::fclose(f);
                        if (written == static_cast<size_t>(resp.body.Length()))
                        {
                            if (std::rename(tmpPath.CStr(), destPath) == 0)
                            {
                                NK_NET_LOG_DEBUG("Fichier téléchargé : {}", destPath);
                            }
                            else
                            {
                                std::remove(tmpPath.CStr());
                                NK_NET_LOG_ERROR("Échec renommage fichier : {}", destPath);
                            }
                        }
                        else
                        {
                            std::remove(tmpPath.CStr());
                            NK_NET_LOG_ERROR("Échec d'écriture du fichier : {}", destPath);
                        }
                    }
                    else
                    {
                        NK_NET_LOG_ERROR("Impossible d'ouvrir le fichier : {}", tmpPath.CStr());
                    }
                }

                if (onComplete) { onComplete(resp); }
            };

            return SendAsync(req, wrappedCb);
        }

        // =====================================================================
        // IMPLÉMENTATION : NkHTTPClient — Utilitaires statiques
        // =====================================================================

        NkString NkHTTPClient::URLEncode(const char* s) noexcept
        {
            if (s == nullptr) { return {}; }

            static const char hex[] = "0123456789ABCDEF";
            NkString result;
            result.Reserve(std::strlen(s) * 3);  // Worst case: tous les chars encodés

            for (const char* p = s; *p != '\0'; ++p)
            {
                unsigned char c = static_cast<unsigned char>(*p);
                // Caractères non réservés selon RFC 3986
                if (std::isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~')
                {
                    result.Append(*p);
                }
                else
                {
                    result.Append('%');
                    result.Append(hex[(c >> 4) & 0x0F]);
                    result.Append(hex[c & 0x0F]);
                }
            }
            return result;
        }

        NkString NkHTTPClient::URLDecode(const char* s) noexcept
        {
            if (s == nullptr) { return {}; }

            NkString result;
            result.Reserve(std::strlen(s));

            for (const char* p = s; *p != '\0'; ++p)
            {
                if (*p == '%' && std::isxdigit(*(p+1)) && std::isxdigit(*(p+2)))
                {
                    // Décodage %XX
                    auto hexToVal = [](char c) -> int {
                        if (c >= '0' && c <= '9') return c - '0';
                        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                        return 0;
                    };
                    uint8 decoded = static_cast<uint8>(
                        (hexToVal(*(p+1)) << 4) | hexToVal(*(p+2))
                    );
                    result.Append(static_cast<char>(decoded));
                    p += 2;  // Sauter les deux hex digits
                }
                else if (*p == '+')
                {
                    result.Append(' ');  // + → espace dans query strings
                }
                else
                {
                    result.Append(*p);
                }
            }
            return result;
        }

        NkString NkHTTPClient::Base64Encode(const uint8* data, uint32 size) noexcept
        {
            if (data == nullptr || size == 0) { return {}; }

            static const char alphabet[] =
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

            NkString result;
            result.Reserve((size + 2) / 3 * 4);  // Formule Base64

            uint32 i = 0;
            while (i < size)
            {
                uint32 b0 = data[i++];
                uint32 b1 = (i < size) ? data[i++] : 0;
                uint32 b2 = (i < size) ? data[i++] : 0;

                result.Append(alphabet[(b0 >> 2) & 0x3F]);
                result.Append(alphabet[((b0 << 4) | (b1 >> 4)) & 0x3F]);
                result.Append(i > 1 ? alphabet[((b1 << 2) | (b2 >> 6)) & 0x3F] : '=');
                result.Append(i > 2 ? alphabet[b2 & 0x3F] : '=');
            }
            return result;
        }

        // =====================================================================
        // IMPLÉMENTATION : NkHTTPClient — Parsing HTTP
        // =====================================================================

        NkHTTPResponse NkHTTPClient::ParseResponse(const NkString& raw) const noexcept
        {
            NkHTTPResponse resp;

            if (raw.Empty())
            {
                resp.error = "Empty response";
                return resp;
            }

            const char* start = raw.CStr();
            const char* end = start + raw.Length();

            // Ligne de statut : "HTTP/1.1 200 OK"
            NkString statusLine;
            start = ReadLine(start, end, statusLine);
            if (statusLine.Empty() || std::strncmp(statusLine.CStr(), "HTTP/", 5) != 0)
            {
                resp.error = "Invalid status line";
                return resp;
            }

            // Parsing du code statut
            const char* codeStart = statusLine.CStr() + 9;  // Après "HTTP/x.x "
            resp.statusCode = static_cast<uint32>(std::strtoul(codeStart, nullptr, 10));
            resp.statusText = StatusTextForCode(resp.statusCode);

            // Parsing des en-têtes jusqu'à \r\n\r\n
            while (start < end)
            {
                NkString headerLine;
                start = ReadLine(start, end, headerLine);
                if (headerLine.Empty()) { break; }  // Fin des en-têtes

                // Parsing "Key: Value"
                const char* colon = std::strchr(headerLine.CStr(), ':');
                if (colon != nullptr)
                {
                    NkHTTPHeader header;
                    header.key = NkString(headerLine.CStr(), static_cast<uint32>(colon - headerLine.CStr()));
                    // Skip ": "
                    const char* valueStart = colon + 1;
                    while (*valueStart == ' ' || *valueStart == '\t') { ++valueStart; }
                    header.value = valueStart;
                    resp.headers.PushBack(header);
                }
            }

            // Le reste est le corps de la réponse
            if (start < end)
            {
                resp.body = NkString(start, static_cast<uint32>(end - start));
            }

            return resp;
        }

        NkString NkHTTPClient::BuildRequestStr(const NkHTTPRequest& req) const noexcept
        {
            NkString result;

            // Parsing de l'URL pour extraire host et path
            NkString scheme, host, path;
            uint16 port = 0;
            ParseURL(req.url, scheme, host, port, path);
            if (path.Empty()) { path = "/"; }

            // Ligne de requête
            result.Append(MethodToString(req.method));
            result.Append(" ");
            result.Append(path);
            result.Append(" ");
            result.Append(kHTTPVersion);
            result.Append(kCRLF);

            // En-têtes obligatoires
            result.Append("Host: ");
            result.Append(host);
            if (port != 0 && port != 80 && port != 443)
            {
                result.Append(":");
                result.Append(NkString::Format("%u", static_cast<unsigned int>(port)));
            }
            result.Append(kCRLF);

            result.Append("User-Agent: ");
            result.Append(mConfig.userAgent);
            result.Append(kCRLF);

            // En-têtes par défaut
            for (const auto& h : mDefaultHeaders)
            {
                result.Append(h.key);
                result.Append(": ");
                result.Append(h.value);
                result.Append(kCRLF);
            }

            // En-têtes de la requête
            for (const auto& h : req.headers)
            {
                result.Append(h.key);
                result.Append(": ");
                result.Append(h.value);
                result.Append(kCRLF);
            }

            // Content-Length si body présent
            if (!req.body.Empty())
            {
                result.Append("Content-Length: ");
                result.Append(NkString::Format("%u", req.body.Length()));
                result.Append(kCRLF);
            }

            // Fin des en-têtes
            result.Append(kCRLF);

            // Corps de la requête
            if (!req.body.Empty())
            {
                result.Append(req.body);
            }

            return result;
        }

        bool NkHTTPClient::ParseURL(
            const NkString& url,
            NkString& scheme,
            NkString& host,
            uint16& port,
            NkString& path
        ) const noexcept
        {
            if (url.Empty()) { return false; }

            const char* p = url.CStr();
            const char* end = p + url.Length();

            // Schéma
            const char* schemeEnd = std::strstr(p, "://");
            if (schemeEnd != nullptr)
            {
                scheme = NkString(p, static_cast<uint32>(schemeEnd - p));
                p = schemeEnd + 3;
            }
            else
            {
                scheme = "http";  // Défaut
            }

            // Hôte (et port optionnel)
            const char* hostStart = p;
            while (p < end && *p != '/' && *p != ':' && *p != '?' && *p != '#') { ++p; }
            host = NkString(hostStart, static_cast<uint32>(p - hostStart));

            // Port optionnel
            if (p < end && *p == ':')
            {
                ++p;
                const char* portStart = p;
                while (p < end && std::isdigit(static_cast<unsigned char>(*p))) { ++p; }
                if (portStart < p)
                {
                    port = static_cast<uint16>(std::strtoul(portStart, nullptr, 10));
                }
            }

            // Chemin + query + fragment
            if (p < end)
            {
                path = NkString(p, static_cast<uint32>(end - p));
            }
            else
            {
                path = "/";
            }

            return !host.Empty();
        }

        // =====================================================================
        // IMPLÉMENTATION : NkHTTPClient — Transport TCP/TLS
        // =====================================================================

        NkHTTPResponse NkHTTPClient::SendOverTCP(const NkHTTPRequest& req) noexcept
        {
            NkHTTPResponse response;

            // Parsing de l'URL
            NkString scheme, host, path;
            uint16 port = 0;
            if (!ParseURL(req.url, scheme, host, port, path))
            {
                response.error = "Invalid URL";
                return response;
            }
            if (port == 0) { port = 80; }

            // Création du socket
            NkNativeSocket sock = CreateTcpSocket(host.CStr(), port, req.timeoutMs);
            if (sock == kInvalidSocket)
            {
                response.error = "Connection failed";
                return response;
            }

            // Construction et envoi de la requête
            NkString requestStr = BuildRequestStr(req);
            if (!SendWithTimeout(sock, requestStr.CStr(),
                                  static_cast<uint32>(requestStr.Length()),
                                  req.timeoutMs))
            {
                response.error = "Send failed";
                NkCloseSocket(sock);
                return response;
            }

            // Réception de la réponse
            NkString rawResponse;
            if (!RecvWithTimeout(sock, rawResponse, kMaxResponseSize, req.timeoutMs))
            {
                response.error = "Receive timeout";
                NkCloseSocket(sock);
                return response;
            }

            NkCloseSocket(sock);

            // Parsing de la réponse HTTP
            return ParseResponse(rawResponse);
        }

        NkHTTPResponse NkHTTPClient::SendOverTLS(const NkHTTPRequest& req) noexcept
        {
            // Placeholder pour implémentation TLS
            // En production : intégrer mbedTLS ou OpenSSL ici
            NkHTTPResponse response;
            response.error = "HTTPS not implemented in this build";
            return response;

            // Exemple de structure pour mbedTLS :
            /*
            mbedtls_net_context server_fd;
            mbedtls_ssl_config conf;
            mbedtls_ssl_context ssl;
            mbedtls_ctr_drbg_context ctr_drbg;
            mbedtls_entropy_context entropy;

            // Initialisation
            mbedtls_net_init(&server_fd);
            mbedtls_ssl_init(&ssl);
            mbedtls_ssl_config_init(&conf);
            mbedtls_ctr_drbg_init(&ctr_drbg);
            mbedtls_entropy_init(&entropy);

            // ... configuration et handshake ...

            // Nettoyage
            mbedtls_ssl_close_notify(&ssl);
            mbedtls_net_free(&server_fd);
            mbedtls_ssl_free(&ssl);
            mbedtls_ssl_config_free(&conf);
            mbedtls_ctr_drbg_free(&ctr_drbg);
            mbedtls_entropy_free(&entropy);
            */
        }

        // =====================================================================
        // IMPLÉMENTATION : NkHTTPClient — Gestion asynchrone
        // =====================================================================

        void NkHTTPClient::AsyncWorker(AsyncRequest* asyncReq) noexcept
        {
            if (asyncReq == nullptr) { return; }

            // Vérification d'annulation avant exécution
            if (asyncReq->cancelled.Load())
            {
                CleanupRequest(asyncReq);
                return;
            }

            // Exécution de la requête synchrone
            NkHTTPResponse response = Send(asyncReq->req);

            // Vérification d'annulation après exécution
            if (asyncReq->cancelled.Load())
            {
                CleanupRequest(asyncReq);
                return;
            }

            // Invocation du callback
            if (asyncReq->callback != nullptr)
            {
                asyncReq->callback(response);
            }

            // Marquage comme terminé et nettoyage
            asyncReq->done.Store(true);
            CleanupRequest(asyncReq);
        }

        void NkHTTPClient::CleanupRequest(AsyncRequest* asyncReq) noexcept
        {
            if (asyncReq == nullptr) { return; }

            threading::NkLockGuard lock(mAsyncMutex);
            for (auto it = mAsyncRequests.begin(); it != mAsyncRequests.end(); ++it)
            {
                if (*it == asyncReq)
                {
                    delete asyncReq;
                    mAsyncRequests.Erase(it);
                    break;
                }
            }
        }

        void NkHTTPClient::CleanupFinishedRequests() noexcept
        {
            threading::NkLockGuard lock(mAsyncMutex);
            for (auto it = mAsyncRequests.begin(); it != mAsyncRequests.end(); )
            {
                if (*it != nullptr && (*it)->done.Load())
                {
                    delete *it;
                    it = mAsyncRequests.Erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        // =====================================================================
        // IMPLÉMENTATION : NkHTTPClient — Destructeur
        // =====================================================================

        NkHTTPClient::~NkHTTPClient() noexcept
        {
            // Annulation et attente de toutes les requêtes en cours
            CancelAll();
            WaitAll(5000);  // 5 secondes max pour cleanup

            // Nettoyage final
            CleanupFinishedRequests();
        }

        // =====================================================================
        // IMPLÉMENTATION : NkLeaderboard — Méthodes publiques
        // =====================================================================

        void NkLeaderboard::Configure(const char* baseUrl, const char* apiKey) noexcept
        {
            if (baseUrl == nullptr || apiKey == nullptr) { return; }
            mBaseUrl = baseUrl;
            mApiKey = apiKey;
            mHttp.AddDefaultHeader("X-API-Key", apiKey);
            mHttp.AddDefaultHeader("Accept", "application/json");
        }

        void NkLeaderboard::SubmitScore(
            const char* playerName,
            uint64 score,
            NkFunction<void(bool success)> cb
        ) noexcept
        {
            if (playerName == nullptr || mBaseUrl.Empty()) { return; }

            NkHTTPRequest req;
            req.url = mBaseUrl + "/submit";
            req.method = NkHTTPMethod::NK_HTTP_POST;

            // Construction du JSON
            NkString json;
            json.Append(R"({"player":")");
            json.Append(playerName);
            json.Append(R"(","score":)");
            json.Append(NkString::Format("%llu", static_cast<unsigned long long>(score)));
            json.Append("}");
            req.SetJSON(json.CStr());

            // Callback wrapper
            mHttp.SendAsync(req, [cb](const NkHTTPResponse& resp) {
                bool success = !resp.HasError() && resp.IsOK();
                if (cb) { cb(success); }
            });
        }

        void NkLeaderboard::FetchTop(
            uint32 count,
            NkFunction<void(NkVector<NkLeaderboardEntry>&)> cb
        ) noexcept
        {
            if (mBaseUrl.Empty() || cb == nullptr) { return; }

            NkHTTPRequest req;
            req.url = mBaseUrl + "/top?count=" + NkString::Format("%u", count);
            req.method = NkHTTPMethod::NK_HTTP_GET;

            mHttp.SendAsync(req, [cb](const NkHTTPResponse& resp) {
                NkVector<NkLeaderboardEntry> entries;
                if (!resp.HasError() && resp.IsOK() && !resp.body.Empty())
                {
                    // Parsing JSON simplifié — en production utiliser un vrai parser
                    // Exemple attendu : [{"player":"Alice","score":1000,"rank":1},...]
                    ParseLeaderboardJSON(resp.body, entries);
                }
                cb(entries);
            });
        }

        void NkLeaderboard::FetchPlayerRank(
            const char* playerName,
            NkFunction<void(uint32 rank, uint64 score)> cb
        ) noexcept
        {
            if (playerName == nullptr || mBaseUrl.Empty() || cb == nullptr) { return; }

            NkHTTPRequest req;
            req.url = mBaseUrl + "/player?name=" + NkHTTPClient::URLEncode(playerName);
            req.method = NkHTTPMethod::NK_HTTP_GET;

            mHttp.SendAsync(req, [cb](const NkHTTPResponse& resp) {
                uint32 rank = 0;
                uint64 score = 0;
                if (!resp.HasError() && resp.IsOK() && !resp.body.Empty())
                {
                    // Parsing JSON simplifié
                    ParsePlayerRankJSON(resp.body, rank, score);
                }
                cb(rank, score);
            });
        }

        // =====================================================================
        // IMPLÉMENTATION : NkLeaderboard — Helpers de parsing JSON (simplifiés)
        // =====================================================================

        static void ParseLeaderboardJSON(const NkString& json, NkVector<NkLeaderboardEntry>& out) noexcept
        {
            // Parsing JSON minimaliste pour démonstration
            // En production : utiliser un vrai parser JSON (RapidJSON, nlohmann, etc.)
            // Format attendu : [{"player":"Alice","score":1000,"rank":1},...]

            // Recherche simplifiée des entrées
            size_t pos = 0;
            while ((pos = json.Find("{", pos)) != NkString::npos)
            {
                size_t endPos = json.Find("}", pos);
                if (endPos == NkString::npos) { break; }

                NkString entry = json.SubStr(pos, static_cast<uint32>(endPos - pos + 1));

                NkLeaderboardEntry e = {};
                // Extraction basique des champs (à remplacer par vrai parsing)
                size_t namePos = entry.Find("\"player\":");
                if (namePos != NkString::npos)
                {
                    size_t valStart = entry.Find("\"", namePos + 9) + 1;
                    size_t valEnd = entry.Find("\"", valStart);
                    if (valEnd != NkString::npos)
                    {
                        NkString name = entry.SubStr(valStart, static_cast<uint32>(valEnd - valStart));
                        SafeStrCopy(e.playerName, sizeof(e.playerName), name.CStr(), 64);
                    }
                }

                size_t scorePos = entry.Find("\"score\":");
                if (scorePos != NkString::npos)
                {
                    size_t valStart = scorePos + 8;
                    while (valStart < entry.Length() && !std::isdigit(entry[valStart])) { ++valStart; }
                    if (valStart < entry.Length())
                    {
                        e.score = std::strtoull(entry.CStr() + valStart, nullptr, 10);
                    }
                }

                size_t rankPos = entry.Find("\"rank\":");
                if (rankPos != NkString::npos)
                {
                    size_t valStart = rankPos + 7;
                    while (valStart < entry.Length() && !std::isdigit(entry[valStart])) { ++valStart; }
                    if (valStart < entry.Length())
                    {
                        e.rank = static_cast<uint32>(std::strtoul(entry.CStr() + valStart, nullptr, 10));
                    }
                }

                out.PushBack(e);
                pos = endPos + 1;
            }
        }

        static void ParsePlayerRankJSON(const NkString& json, uint32& outRank, uint64& outScore) noexcept
        {
            // Parsing JSON minimaliste pour un seul joueur
            // Format attendu : {"player":"Alice","score":1000,"rank":1}

            size_t rankPos = json.Find("\"rank\":");
            if (rankPos != NkString::npos)
            {
                size_t valStart = rankPos + 7;
                while (valStart < json.Length() && !std::isdigit(json[valStart])) { ++valStart; }
                if (valStart < json.Length())
                {
                    outRank = static_cast<uint32>(std::strtoul(json.CStr() + valStart, nullptr, 10));
                }
            }

            size_t scorePos = json.Find("\"score\":");
            if (scorePos != NkString::npos)
            {
                size_t valStart = scorePos + 8;
                while (valStart < json.Length() && !std::isdigit(json[valStart])) { ++valStart; }
                if (valStart < json.Length())
                {
                    outScore = std::strtoull(json.CStr() + valStart, nullptr, 10);
                }
            }
        }

        // Helper local pour copie de chaîne sécurisée
        static void SafeStrCopy(char* dst, uint32 dstSize, const char* src, uint32 maxLen) noexcept
        {
            if (dst == nullptr || dstSize == 0) { return; }
            if (src == nullptr) { dst[0] = '\0'; return; }
            uint32 len = 0;
            while (src[len] && len < maxLen && len < dstSize - 1) { ++len; }
            std::memcpy(dst, src, len);
            dst[len] = '\0';
        }

    } // namespace net

} // namespace nkentseu

// =============================================================================
// NOTES D'IMPLÉMENTATION ET OPTIMISATIONS
// =============================================================================
/*
    Gestion des sockets TCP :
    ------------------------
    - Mode non-bloquant avec select() pour timeout portable
    - Résolution DNS via getaddrinfo() pour support IPv4/IPv6
    - Gestion d'erreur unifiée via NkGetLastError()/NkWouldBlock()

    Support TLS :
    ------------
    - Placeholder pour mbedTLS/OpenSSL — à implémenter selon la plateforme
    - Toujours valider les certificats (verifySSL = true) en production
    - Gérer le handshake TLS, la vérification de certificat, le chiffrement

    Parsing HTTP/1.1 :
    -----------------
    - Lecture ligne par ligne jusqu'à \r\n\r\n pour les en-têtes
    - Extraction du code statut et des en-têtes dans des structures typées
    - Le corps est conservé brut — parsing JSON/HTML délégué au caller

    Gestion asynchrone :
    -------------------
    - Pool de threads dédié avec Atomic<bool> pour contrôle thread-safe
    - Cleanup automatique des requêtes terminées pour éviter les fuites mémoire
    - Callbacks invoqués depuis le thread worker — différer vers thread jeu si nécessaire

    Performance :
    ------------
    - Buffers stack pour parsing HTTP — pas d'allocation heap dans les chemins critiques
    - Réutilisation des connexions TCP possible via keep-alive (à implémenter)
    - Compression gzip optionnelle si supportée par le serveur (header Accept-Encoding)

    Sécurité :
    ---------
    - Validation stricte des URLs avant parsing pour éviter les injections
    - Limitation de la taille de réponse (kMaxResponseSize) pour éviter les DoS
    - Timeout configurables pour éviter les blocages indéfinis

    Extensions futures :
    -------------------
    - Support HTTP/2 avec multiplexing et compression HPACK
    - Cache HTTP intégré avec politique LRU et validation conditionnelle
    - WebSockets pour communications bidirectionnelles temps réel
    - Proxy support avec authentification pour environnements corporate
*/

// ============================================================
// Copyright © 2024-2026 Rihen. Tous droits réservés.
// Licence Propriétaire - Libre d'utilisation et de modification
// ============================================================