#include "NkClaudeBackend.h"
#include "NKLogger/NkLog.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>

// HTTPS via libcurl (à linker avec -lcurl)
// Si libcurl non disponible, utiliser le backend Ollama local
#if defined(NK_HAVE_CURL)
    #include <curl/curl.h>
#endif

namespace nkentseu {
    namespace humanoid {

        bool NkClaudeBackend::Init(const char* /*endpoint*/,
                                    const char* modelName,
                                    const char* apiKey) noexcept {
            if (modelName) mModel = NkString(modelName);

            // Chercher la clé API
            if (apiKey && *apiKey) {
                mApiKey = NkString(apiKey);
            } else {
                const char* envKey = getenv("ANTHROPIC_API_KEY");
                if (envKey) mApiKey = NkString(envKey);
            }

            mApiKeySet = !mApiKey.IsEmpty();

            if (mApiKeySet) {
                logger.Infof("[ClaudeBackend] Clé API trouvée — modèle: {}\n",
                             mModel.CStr());
            } else {
                logger.Warnf("[ClaudeBackend] Clé API ANTHROPIC_API_KEY non définie\n");
            }

            return mApiKeySet;
        }

        NkString NkClaudeBackend::BuildRequestJSON(
            const NkVector<NkConvMessage>& msgs,
            nk_float32 temp, nk_uint32 maxTok) const noexcept {

            NkString systemContent;
            NkString json = "{";

            char buf[256];
            snprintf(buf, sizeof(buf),
                     "\"model\":\"%s\","
                     "\"max_tokens\":%u,"
                     "\"temperature\":%.2f,",
                     mModel.CStr(), maxTok, (double)temp);
            json += buf;

            // Extraire le system prompt (Claude sépare system des messages)
            json += "\"messages\":[";
            bool first = true;
            for (nk_usize i = 0; i < msgs.Size(); ++i) {
                if (msgs[i].role == NkConvRole::System) {
                    systemContent = msgs[i].content;
                    continue;
                }
                if (!first) json += ",";
                first = false;
                json += "{\"role\":\"";
                json += (msgs[i].role == NkConvRole::User) ? "user" : "assistant";
                json += "\",\"content\":\"";
                // Échapper
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
            json += "]";

            if (!systemContent.IsEmpty()) {
                json += ",\"system\":\"";
                const char* c = systemContent.CStr();
                while (*c) {
                    if (*c == '"')  json += "\\\"";
                    else if (*c == '\n') json += "\\n";
                    else if (*c == '\\') json += "\\\\";
                    else json += *c;
                    ++c;
                }
                json += "\"";
            }

            json += "}";
            return json;
        }

        bool NkClaudeBackend::Complete(const NkVector<NkConvMessage>& messages,
                                        nk_float32 temperature,
                                        nk_uint32  maxTokens,
                                        NkConvResponse& out) noexcept {
            if (!mApiKeySet) { out.error = "No API key"; return false; }

            NkString body = BuildRequestJSON(messages, temperature, maxTokens);
            NkString resp;

            if (!HttpsPost(body, resp)) {
                out.error = "HTTPS request failed";
                return false;
            }

            return ParseResponse(resp, out);
        }

        bool NkClaudeBackend::ParseResponse(const NkString& json,
                                             NkConvResponse& out) const noexcept {
            // Réponse Claude : {"content":[{"type":"text","text":"..."}],...}
            const char* textKey = "\"text\":\"";
            const char* found = strstr(json.CStr(), textKey);
            if (!found) { out.error = "No text in response"; return false; }
            found += strlen(textKey);

            NkString content;
            while (*found && !(*found == '"' && *(found-1) != '\\')) {
                if (*found == '\\' && *(found+1) == 'n')  { content += '\n'; found += 2; }
                else if (*found == '\\' && *(found+1) == '"') { content += '"'; found += 2; }
                else { content += *found; ++found; }
            }

            out.text    = content;
            out.success = !content.IsEmpty();
            return out.success;
        }

        bool NkClaudeBackend::HttpsPost(const NkString& body,
                                         NkString& out) const noexcept {
#if defined(NK_HAVE_CURL)
            CURL* curl = curl_easy_init();
            if (!curl) return false;

            NkString url = mEndpoint + "/v1/messages";
            NkString responseData;

            curl_easy_setopt(curl, CURLOPT_URL, url.CStr());
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.CStr());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.Length());

            struct curl_slist* headers = nullptr;
            NkString authHeader = "x-api-key: " + mApiKey;
            headers = curl_slist_append(headers, authHeader.CStr());
            headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");
            headers = curl_slist_append(headers, "Content-Type: application/json");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                +[](char* data, size_t, size_t nmemb, void* userdata) -> size_t {
                    NkString* s = static_cast<NkString*>(userdata);
                    *s += NkString(data, nmemb);
                    return nmemb;
                });
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseData);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

            CURLcode res = curl_easy_perform(curl);
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);

            if (res != CURLE_OK) {
                logger.Errorf("[ClaudeBackend] cURL error: {}\n", curl_easy_strerror(res));
                return false;
            }
            out = responseData;
            return true;
#else
            // Sans libcurl : logger un warning et retourner false
            (void)body;
            logger.Errorf("[ClaudeBackend] libcurl non disponible. "
                          "Compiler avec -DNK_HAVE_CURL et linker -lcurl\n"
                          "Ou utiliser NkOllamaBackend pour un LLM local.\n");
            out = "";
            return false;
#endif
        }

    } // namespace humanoid
} // namespace nkentseu
