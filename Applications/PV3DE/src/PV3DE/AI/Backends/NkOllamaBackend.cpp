#include "NkOllamaBackend.h"
#include "NKLogger/NkLog.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

// HTTP minimal via socket POSIX / WinSock
// En production : remplacer par NKStream::HttpClient quand disponible
#if defined(_WIN32)
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <arpa/inet.h>
#endif

namespace nkentseu {
    namespace humanoid {

        bool NkOllamaBackend::Init(const char* endpoint,
                                    const char* modelName,
                                    const char* /*apiKey*/) noexcept {
            mEndpoint  = endpoint  ? NkString(endpoint)   : "http://localhost:11434";
            mModelName = modelName ? NkString(modelName)  : "mistral:7b-instruct";

            // Test de connectivité : ping /api/tags
            NkString pingUrl = mEndpoint + "/api/tags";
            NkString resp;
            if (HttpPost(pingUrl.CStr(), "{}", resp)) {
                mAvailable = true;
                logger.Infof("[OllamaBackend] Connecté à {} — modèle: {}\n",
                             mEndpoint.CStr(), mModelName.CStr());
            } else {
                mAvailable = false;
                logger.Warnf("[OllamaBackend] Ollama non disponible à {}\n",
                             mEndpoint.CStr());
            }
            return mAvailable;
        }

        // =====================================================================
        bool NkOllamaBackend::Complete(const NkVector<NkConvMessage>& messages,
                                        nk_float32 temperature,
                                        nk_uint32  maxTokens,
                                        NkConvResponse& out) noexcept {
            NkString body = BuildRequestJSON(messages, temperature, maxTokens);
            NkString url  = mEndpoint + "/api/chat";
            NkString resp;

            if (!HttpPost(url.CStr(), body, resp)) {
                out.error = "Ollama HTTP request failed";
                return false;
            }

            return ParseResponse(resp, out);
        }

        // =====================================================================
        // Construction du JSON Ollama Chat API
        // Format : { "model": "...", "messages": [...], "stream": false, "options": {...} }
        NkString NkOllamaBackend::BuildRequestJSON(
            const NkVector<NkConvMessage>& msgs,
            nk_float32 temp, nk_uint32 maxTok) const noexcept {

            NkString json = "{";
            json += "\"model\":\"" + mModelName + "\",";
            json += "\"stream\":false,";

            // Messages
            json += "\"messages\":[";
            for (nk_usize i = 0; i < msgs.Size(); ++i) {
                if (i > 0) json += ",";
                json += "{\"role\":\"";
                switch (msgs[i].role) {
                    case NkConvRole::System:    json += "system"; break;
                    case NkConvRole::User:      json += "user";   break;
                    case NkConvRole::Assistant: json += "assistant"; break;
                }
                json += "\",\"content\":\"";
                // Échapper les caractères JSON
                const char* c = msgs[i].content.CStr();
                while (*c) {
                    if (*c == '"')  json += "\\\"";
                    else if (*c == '\n') json += "\\n";
                    else if (*c == '\\') json += "\\\\";
                    else json += *c;
                    ++c;
                }
                json += "\"}";
            }
            json += "],";

            // Options
            char optBuf[128];
            snprintf(optBuf, sizeof(optBuf),
                     "\"options\":{\"temperature\":%.2f,\"num_predict\":%u}",
                     (double)temp, maxTok);
            json += optBuf;
            json += "}";

            return json;
        }

        // =====================================================================
        // Parse de la réponse JSON Ollama
        // Réponse : { "message": { "role": "assistant", "content": "..." }, ... }
        bool NkOllamaBackend::ParseResponse(const NkString& json,
                                             NkConvResponse& out) const noexcept {
            // Recherche simple de "content":"..."
            const char* p = json.CStr();
            const char* contentKey = "\"content\":\"";
            const char* found = strstr(p, contentKey);
            if (!found) {
                out.error = "No content field in response";
                return false;
            }

            found += strlen(contentKey);

            // Lire jusqu'au prochain " non échappé
            NkString content;
            while (*found && !(*found == '"' && *(found-1) != '\\')) {
                if (*found == '\\' && *(found+1) == 'n') {
                    content += '\n';
                    found += 2;
                } else if (*found == '\\' && *(found+1) == '"') {
                    content += '"';
                    found += 2;
                } else {
                    content += *found;
                    ++found;
                }
            }

            out.text    = content;
            out.success = !content.IsEmpty();
            return out.success;
        }

        // =====================================================================
        // HTTP POST minimal (socket POSIX)
        // En production : utiliser libcurl ou NKStream::HttpClient
        bool NkOllamaBackend::HttpPost(const char* url,
                                        const NkString& body,
                                        NkString& responseOut) const noexcept {
            // Parser l'URL : http://host:port/path
            const char* p = url;
            if (strncmp(p, "http://", 7) == 0) p += 7;

            char host[128] = {};
            int  port      = 80;
            char path[256] = "/";

            // Extraire host:port
            const char* slash = strchr(p, '/');
            const char* colon = strchr(p, ':');
            if (colon && (!slash || colon < slash)) {
                nk_usize hlen = (nk_usize)(colon - p);
                strncpy(host, p, NkMin(hlen, (nk_usize)127));
                port = atoi(colon + 1);
            } else if (slash) {
                nk_usize hlen = (nk_usize)(slash - p);
                strncpy(host, p, NkMin(hlen, (nk_usize)127));
            } else {
                strncpy(host, p, 127);
            }
            if (slash) strncpy(path, slash, 255);

#if defined(_WIN32)
            WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa);
#endif
            // Résolution DNS
            struct hostent* he = gethostbyname(host);
            if (!he) return false;

            // Création socket
#if defined(_WIN32)
            SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock == INVALID_SOCKET) return false;
#else
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) return false;
#endif
            struct sockaddr_in addr = {};
            addr.sin_family = AF_INET;
            addr.sin_port   = htons((uint16_t)port);
            memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

            if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
#if defined(_WIN32)
                closesocket(sock);
#else
                close(sock);
#endif
                return false;
            }

            // Construire la requête HTTP
            char req[8192];
            snprintf(req, sizeof(req),
                     "POST %s HTTP/1.1\r\n"
                     "Host: %s:%d\r\n"
                     "Content-Type: application/json\r\n"
                     "Content-Length: %zu\r\n"
                     "Connection: close\r\n"
                     "\r\n%s",
                     path, host, port, body.Length(), body.CStr());

            send(sock, req, (int)strlen(req), 0);

            // Lire la réponse
            char buf[4096];
            NkString fullResp;
            int n;
            while ((n = recv(sock, buf, sizeof(buf)-1, 0)) > 0) {
                buf[n] = '\0';
                fullResp += buf;
            }
#if defined(_WIN32)
            closesocket(sock);
            WSACleanup();
#else
            close(sock);
#endif
            // Extraire le body HTTP (après \r\n\r\n)
            const char* bodyStart = strstr(fullResp.CStr(), "\r\n\r\n");
            if (bodyStart) {
                responseOut = NkString(bodyStart + 4);
                return true;
            }
            return false;
        }

    } // namespace humanoid
} // namespace nkentseu
