// =============================================================================
// NKNetwork/HTTP/NkHTTPClient.h
// =============================================================================
// DESCRIPTION :
//   Client HTTP/HTTPS léger pour communications avec APIs REST externes :
//   CDN de assets, télémétrie, authentification, leaderboards, configuration.
//
// FONCTIONNALITÉS PRINCIPALES :
//   • Requêtes synchrones (bloquantes) : Get(), Post(), Send()
//   • Requêtes asynchrones (non-bloquantes) : SendAsync() avec callbacks
//   • Gestion des en-têtes HTTP : ajout, modification, authentification
//   • Support JSON natif : SetJSON(), parsing automatique Content-Type
//   • Redirections automatiques : suivi configurable des codes 3xx
//   • Téléchargement de fichiers : DownloadFile() avec callback de progression
//   • Encodages utilitaires : URLEncode(), URLDecode(), Base64Encode()
//
// SUPPORT PLATEFORME :
//   • Windows/Linux/macOS : TCP + TLS via mbedTLS ou OpenSSL (configurable)
//   • WebAssembly : Délégation à emscripten_fetch pour compatibilité navigateur
//   • Mobile (iOS/Android) : Intégration native via couches d'abstraction
//
// SÉCURITÉ :
//   • Vérification SSL/TLS configurable via Config::verifySSL
//   • Support certificats CA personnalisés via Config::caCertPath
//   • Authentification : Bearer tokens, Basic Auth, en-têtes personnalisés
//   • Timeout configurable pour éviter les blocages indéfinis
//
// UTILISATION TYPIQUE :
//   // Client HTTP avec configuration par défaut
//   NkHTTPClient http;
//
//   // Requête GET synchrone
//   NkHTTPResponse r = http.Get("https://api.example.com/version");
//   if (r.IsOK()) { NK_LOG_INFO("Version: {}", r.body.CStr()); }
//
//   // Requête POST asynchrone avec JSON
//   NkHTTPRequest req;
//   req.url = "https://api.example.com/score";
//   req.method = NkHTTPMethod::NK_HTTP_POST;
//   req.SetJSON(R"({"score":1000,"player":"Alice"})");
//   req.SetBearerToken("token123");
//   http.SendAsync(req, [](const NkHTTPResponse& r) {
//       if (r.IsOK()) { /* Traiter la réponse */ }
//   });
//
// DÉPENDANCES :
//   • NkConnection.h   : Gestion des connexions TCP sous-jacentes
//   • NkNetDefines.h   : Types fondamentaux et codes de retour réseau
//   • NKThreading/*    : Mutex, atomics, threads pour gestion asynchrone
//
// RÈGLES D'UTILISATION :
//   • Configurer le client via Configure() avant toute requête si besoin
//   • Vérifier IsOK()/HasError() sur les réponses avant traitement
//   • Utiliser SendAsync() pour ne pas bloquer le thread principal
//   • Annuler les requêtes asynchrones via Cancel() si nécessaire (cleanup)
//   • Les callbacks asynchrones sont invoqués depuis le thread réseau
//
// AUTEUR   : Rihen
// DATE     : 2026-02-10
// VERSION  : 1.0.0
// LICENCE  : Propriétaire - libre d'utilisation et de modification
// =============================================================================

#pragma once

#ifndef NKENTSEU_NETWORK_NKHTTPCLIENT_H
#define NKENTSEU_NETWORK_NKHTTPCLIENT_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des modules requis dans l'ordre de dépendance hiérarchique.

    #include "NKNetwork/NkNetworkApi.h"
    #include "NKNetwork/Protocol/NkConnection.h"
    #include "NKCore/NkTypes.h"
    #include "NKCore/NkAtomic.h"
    #include "NKContainers/String/NkString.h"
    #include "NKContainers/Sequential/NkVector.h"
    #include "NKContainers/Functional/NkFunction.h"
    #include "NKThreading/NkMutex.h"
    #include "NKThreading/NkThread.h"
    #include "NKThreading/NkScopedLock.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : NAMESPACE PRINCIPAL ET DÉCLARATIONS
    // -------------------------------------------------------------------------
    // Toutes les déclarations du module résident dans nkentseu::net.

    namespace nkentseu {

        namespace net {

            // =================================================================
            // ÉNUMÉRATION : NkHTTPMethod — Méthodes HTTP supportées
            // =================================================================
            /**
             * @enum NkHTTPMethod
             * @brief Méthodes HTTP standard supportées par le client.
             *
             * Cette énumération définit les verbes HTTP disponibles pour
             * les requêtes vers des APIs REST :
             *   • NK_HTTP_GET     : Récupération de ressources (idempotent)
             *   • NK_HTTP_POST    : Création de ressources avec corps de requête
             *   • NK_HTTP_PUT     : Mise à jour complète de ressources
             *   • NK_HTTP_PATCH   : Mise à jour partielle de ressources
             *   • NK_HTTP_DELETE  : Suppression de ressources
             *   • NK_HTTP_HEAD    : Récupération des métadonnées uniquement
             *
             * @note Le choix de la méthode impacte la sémantique de la requête
             *       et le traitement côté serveur (REST conventions).
             * @see NkHTTPRequest Pour l'utilisation dans une requête complète
             */
            enum class NkHTTPMethod : uint8
            {
                /// Récupération de ressource : paramètres dans l'URL, corps vide.
                /// Idempotent : appels multiples = même résultat.
                NK_HTTP_GET = 0,

                /// Création de ressource : corps de requête contient les données.
                /// Non-idempotent : appels multiples peuvent créer plusieurs ressources.
                NK_HTTP_POST,

                /// Mise à jour complète : remplace la ressource existante.
                /// Idempotent : appels multiples = même état final.
                NK_HTTP_PUT,

                /// Mise à jour partielle : modifie uniquement les champs spécifiés.
                /// Non-idempotent selon l'implémentation serveur.
                NK_HTTP_PATCH,

                /// Suppression de ressource : retire l'entité identifiée par l'URL.
                /// Idempotent : supprimer une ressource déjà absente = succès.
                NK_HTTP_DELETE,

                /// Récupération des en-têtes uniquement : pas de corps de réponse.
                /// Utile pour vérifier l'existence ou les métadonnées sans télécharger.
                NK_HTTP_HEAD
            };

            // =================================================================
            // STRUCTURE : NkHTTPHeader — Paire clé/valeur HTTP
            // =================================================================
            /**
             * @struct NkHTTPHeader
             * @brief Représente un en-tête HTTP sous forme de paire clé/valeur.
             *
             * Les en-têtes HTTP transportent des métadonnées sur la requête
             * ou la réponse : authentification, type de contenu, cache, etc.
             *
             * FORMAT :
             *   • key   : Nom de l'en-tête (case-insensitive selon HTTP/1.1)
             *   • value : Valeur associée (peut contenir des espaces, quotes)
             *
             * EXEMPLES COURANTS :
             *   • "Content-Type": "application/json"
             *   • "Authorization": "Bearer abc123"
             *   • "Accept": "application/json, text/plain"
             *   • "User-Agent": "NKNetwork/1.0"
             *
             * @note Les clés sont comparées de façon case-insensitive pour conformité HTTP
             * @note Les valeurs sont conservées telles quelles (pas de trimming automatique)
             */
            struct NKENTSEU_NETWORK_CLASS_EXPORT NkHTTPHeader
            {
                /// Nom de l'en-tête (ex: "Content-Type", "Authorization").
                NkString key;

                /// Valeur associée à l'en-tête.
                NkString value;
            };

            // =================================================================
            // STRUCTURE : NkHTTPRequest — Requête HTTP complète
            // =================================================================
            /**
             * @struct NkHTTPRequest
             * @brief Conteneur de tous les paramètres d'une requête HTTP.
             *
             * Cette structure encapsule l'ensemble des informations nécessaires
             * pour construire et envoyer une requête HTTP vers un serveur distant :
             *   • url : Endpoint cible (doit inclure le schéma http:// ou https://)
             *   • method : Verbe HTTP (GET, POST, etc.) définissant l'action
             *   • headers : Liste d'en-têtes personnalisés pour authentification, format, etc.
             *   • body : Contenu de la requête (pour POST/PUT/PATCH)
             *   • timeoutMs : Délai maximal d'attente avant échec
             *   • followRedirects : Suivi automatique des redirections 3xx
             *   • maxRedirects : Limite de redirections pour éviter les boucles
             *
             * HELPERS DE CONSTRUCTION :
             *   • SetJSON() : Configure Content-Type et Accept pour JSON
             *   • SetFormData() : Configure pour application/x-www-form-urlencoded
             *   • AddHeader() : Ajoute un en-tête personnalisé
             *   • SetBearerToken() : Configure l'authentification OAuth2 Bearer
             *   • SetBasicAuth() : Configure l'authentification HTTP Basic
             *
             * @note Toutes les chaînes sont copiées en interne — sécurité mémoire
             * @note Les helpers modifient à la fois body et headers pour cohérence
             * @threadsafe Copie par valeur pour sécurité — pas de référence partagée
             *
             * @example
             * @code
             * // Requête POST JSON avec authentification
             * NkHTTPRequest req;
             * req.url = "https://api.example.com/users";
             * req.method = NkHTTPMethod::NK_HTTP_POST;
             * req.SetJSON(R"({"name":"Alice","email":"alice@example.com"})");
             * req.SetBearerToken("eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...");
             * req.timeoutMs = 5000;  // 5 secondes max
             *
             * // Envoi via client HTTP
             * NkHTTPResponse resp = httpClient.Send(req);
             * if (resp.IsOK()) { /\* Traiter la réponse *\/ }
             * @endcode
             */
            struct NKENTSEU_NETWORK_CLASS_EXPORT NkHTTPRequest
            {
                // -------------------------------------------------------------
                // Paramètres principaux de la requête
                // -------------------------------------------------------------

                /// URL complète de l'endpoint cible.
                /// @note Doit inclure le schéma : "http://" ou "https://"
                /// @note Exemple : "https://api.example.com/v1/users/123"
                NkString url;

                /// Méthode HTTP définissant l'action à effectuer.
                /// @note Défaut: NK_HTTP_GET pour compatibilité avec les URLs simples
                NkHTTPMethod method = NkHTTPMethod::NK_HTTP_GET;

                /// Liste des en-têtes HTTP personnalisés à inclure.
                /// @note Les en-têtes par défaut (User-Agent, etc.) sont ajoutés automatiquement
                NkVector<NkHTTPHeader> headers;

                /// Corps de la requête pour méthodes POST/PUT/PATCH.
                /// @note Ignoré pour GET/HEAD/DELETE selon conventions HTTP
                NkString body;

                // -------------------------------------------------------------
                // Options de comportement et timeout
                // -------------------------------------------------------------

                /// Délai maximal d'attente pour la réponse en millisecondes.
                /// @note Défaut: 10000ms (10 secondes) — ajuster selon l'API cible
                /// @note 0 = attente infinie (déconseillé — risque de blocage)
                uint32 timeoutMs = 10000;

                /// Indicateur de suivi automatique des redirections HTTP 3xx.
                /// @note Défaut: true — conforme au comportement navigateur standard
                /// @note Si false : la réponse 3xx est retournée telle quelle au caller
                bool followRedirects = true;

                /// Nombre maximal de redirections à suivre avant échec.
                /// @note Défaut: 5 — suffisant pour la plupart des chaînes de redirection
                /// @note Protège contre les boucles de redirection mal configurées
                uint32 maxRedirects = 5;

                // -------------------------------------------------------------
                // Helpers de construction — méthodes utilitaires
                // -------------------------------------------------------------

                /**
                 * @brief Configure la requête pour envoyer du JSON.
                 * @param json Chaîne JSON valide à envoyer dans le corps de la requête.
                 * @note Ajoute automatiquement :
                 *   • Content-Type: application/json
                 *   • Accept: application/json
                 * @note Remplace le body existant — appeler en dernier après configuration.
                 * @example
                 * @code
                 * req.SetJSON(R"({"action":"login","user":"alice"})");
                 * @endcode
                 */
                void SetJSON(const char* json) noexcept;

                /**
                 * @brief Configure la requête pour envoyer du formulaire URL-encodé.
                 * @param formData Chaîne au format "key1=value1&key2=value2".
                 * @note Ajoute automatiquement Content-Type: application/x-www-form-urlencoded
                 * @note Format attendu : pas d'encodage URL préalable — la méthode s'en charge.
                 * @example
                 * @code
                 * req.SetFormData("username=alice&password=secret123");
                 * @endcode
                 */
                void SetFormData(const char* formData) noexcept;

                /**
                 * @brief Ajoute un en-tête HTTP personnalisé à la requête.
                 * @param key Nom de l'en-tête (ex: "X-Custom-Header").
                 * @param value Valeur associée à l'en-tête.
                 * @note Les doublons sont autorisés — certains en-têtes peuvent apparaître plusieurs fois.
                 * @note La clé est conservée telle quelle — la comparaison HTTP est case-insensitive.
                 * @example
                 * @code
                 * req.AddHeader("X-Request-ID", "abc-123-def");
                 * req.AddHeader("Cache-Control", "no-cache");
                 * @endcode
                 */
                void AddHeader(const char* key, const char* value) noexcept;

                /**
                 * @brief Configure l'authentification OAuth2 Bearer Token.
                 * @param token Jeton d'accès OAuth2 (sans préfixe "Bearer ").
                 * @note Ajoute automatiquement l'en-tête : Authorization: Bearer <token>
                 * @note Le token ne doit PAS inclure le préfixe "Bearer " — ajouté automatiquement.
                 * @warning Les tokens sont sensibles — ne pas logger en clair en production.
                 * @example
                 * @code
                 * req.SetBearerToken("eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...");
                 * // Résultat : Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...
                 * @endcode
                 */
                void SetBearerToken(const char* token) noexcept;

                /**
                 * @brief Configure l'authentification HTTP Basic.
                 * @param user Nom d'utilisateur pour l'authentification.
                 * @param pass Mot de passe associé (sera encodé en Base64).
                 * @note Ajoute automatiquement l'en-tête : Authorization: Basic <base64(user:pass)>
                 * @note L'encodage Base64 est géré automatiquement — passer les valeurs en clair.
                 * @warning HTTP Basic n'est pas chiffré — utiliser uniquement sur HTTPS.
                 * @example
                 * @code
                 * req.SetBasicAuth("alice", "secret123");
                 * // Résultat : Authorization: Basic YWxpY2U6c2VjcmV0MTIz
                 * @endcode
                 */
                void SetBasicAuth(const char* user, const char* pass) noexcept;
            };

            // =================================================================
            // STRUCTURE : NkHTTPResponse — Réponse HTTP complète
            // =================================================================
            /**
             * @struct NkHTTPResponse
             * @brief Conteneur de tous les résultats d'une requête HTTP.
             *
             * Cette structure encapsule l'ensemble des informations retournées
             * par un serveur HTTP en réponse à une requête :
             *   • statusCode : Code HTTP numérique (200=OK, 404=Not Found, etc.)
             *   • statusText : Description textuelle du statut (optionnelle)
             *   • headers : Liste des en-têtes de réponse pour métadonnées
             *   • body : Contenu de la réponse (JSON, HTML, binaire, etc.)
             *   • error : Message d'erreur réseau (distinct des erreurs HTTP)
             *   • timeMs : Durée totale de la requête en millisecondes
             *
             * MÉTHODES D'INSPECTION :
             *   • IsOK() : true pour codes 2xx (succès HTTP)
             *   • IsRedirect() : true pour codes 3xx (redirection)
             *   • IsClientError() : true pour codes 4xx (erreur client)
             *   • IsServerError() : true pour codes 5xx (erreur serveur)
             *   • HasError() : true si erreur réseau (pas de réponse reçue)
             *   • GetHeader() : Accès rapide à un en-tête par nom
             *
             * DISTINCTION CRITIQUE :
             *   • HasError() = true → Échec réseau (timeout, DNS, connexion)
             *   • IsOK() = false → Réponse reçue mais statut HTTP en erreur
             *   → Toujours vérifier HasError() avant d'interpréter statusCode
             *
             * @note Toutes les chaînes sont copiées en interne — sécurité mémoire
             * @note Le body peut être vide pour certaines requêtes (HEAD, 204 No Content)
             * @threadsafe Lecture seule thread-safe après réception complète
             *
             * @example
             * @code
             * NkHTTPResponse resp = httpClient.Get("https://api.example.com/data");
             *
             * if (resp.HasError()) {
             *     NK_LOG_ERROR("Échec réseau : {}", resp.error.CStr());
             *     return;
             * }
             *
             * if (!resp.IsOK()) {
             *     NK_LOG_WARN("Erreur HTTP {} : {}", resp.statusCode, resp.statusText.CStr());
             *     return;
             * }
             *
             * // Traitement de la réponse réussie
             * const NkString* contentType = resp.GetHeader("Content-Type");
             * if (contentType && contentType->Find("json") != NkString::npos) {
             *     ParseJSON(resp.body);
             * }
             * @endcode
             */
            struct NKENTSEU_NETWORK_CLASS_EXPORT NkHTTPResponse
            {
                // -------------------------------------------------------------
                // Statut HTTP — code et description
                // -------------------------------------------------------------

                /// Code de statut HTTP numérique (ex: 200, 404, 500).
                /// @note 0 = aucune réponse reçue (erreur réseau)
                uint32 statusCode = 0;

                /// Description textuelle du statut (ex: "OK", "Not Found").
                /// @note Optionnelle — peut être vide selon le serveur
                NkString statusText;

                // -------------------------------------------------------------
                // Métadonnées de réponse — en-têtes et contenu
                // -------------------------------------------------------------

                /// Liste des en-têtes HTTP retournés par le serveur.
                /// @note Inclut Content-Type, Content-Length, Cache-Control, etc.
                NkVector<NkHTTPHeader> headers;

                /// Corps de la réponse contenant les données utiles.
                /// @note Peut être vide pour certaines réponses (HEAD, 204, erreurs)
                /// @note Encodage : UTF-8 pour texte, binaire brut pour autres types
                NkString body;

                // -------------------------------------------------------------
                // Diagnostics — erreurs réseau et timing
                // -------------------------------------------------------------

                /// Message d'erreur réseau (distinct des erreurs HTTP).
                /// @note Rempli uniquement si HasError() == true
                /// @note Exemples : "Connection timeout", "DNS resolution failed"
                NkString error;

                /// Durée totale de la requête en millisecondes.
                /// @note Inclut : DNS + connexion + envoi + attente + réception
                /// @note Utile pour monitoring de performance et debugging
                uint32 timeMs = 0;

                // -------------------------------------------------------------
                // Méthodes d'inspection — catégorisation du résultat
                // -------------------------------------------------------------

                /**
                 * @brief Teste si la réponse indique un succès HTTP (codes 2xx).
                 * @return true si 200 <= statusCode < 300.
                 * @note Ne vérifie PAS HasError() — appeler HasError() en premier.
                 * @example
                 * @code
                 * if (!resp.HasError() && resp.IsOK()) { /\* Succès complet *\/ }
                 * @endcode
                 */
                [[nodiscard]] bool IsOK() const noexcept;

                /**
                 * @brief Teste si la réponse est une redirection HTTP (codes 3xx).
                 * @return true si 300 <= statusCode < 400.
                 * @note Utile si followRedirects = false pour gestion manuelle.
                 */
                [[nodiscard]] bool IsRedirect() const noexcept;

                /**
                 * @brief Teste si la réponse indique une erreur client (codes 4xx).
                 * @return true si 400 <= statusCode < 500.
                 * @note Exemples : 400 Bad Request, 401 Unauthorized, 404 Not Found.
                 */
                [[nodiscard]] bool IsClientError() const noexcept;

                /**
                 * @brief Teste si la réponse indique une erreur serveur (codes 5xx).
                 * @return true si 500 <= statusCode < 600.
                 * @note Exemples : 500 Internal Server Error, 503 Service Unavailable.
                 */
                [[nodiscard]] bool IsServerError() const noexcept;

                /**
                 * @brief Teste si une erreur réseau est survenue (pas de réponse HTTP).
                 * @return true si le champ error n'est pas vide.
                 * @note Prioritaire sur les vérifications de statusCode — une erreur
                 *       réseau empêche toute interprétation du statut HTTP.
                 */
                [[nodiscard]] bool HasError() const noexcept;

                /**
                 * @brief Recherche un en-tête de réponse par nom (case-insensitive).
                 * @param key Nom de l'en-tête à rechercher (ex: "Content-Type").
                 * @return Pointeur vers la valeur si trouvé, nullptr sinon.
                 * @note Le pointeur retourné est valide tant que la réponse existe.
                 * @note Ne pas stocker le pointeur au-delà de la durée de vie de la réponse.
                 * @example
                 * @code
                 * if (const NkString* ct = resp.GetHeader("Content-Type")) {
                 *     if (ct->Find("application/json") != NkString::npos) {
                 *         /\* Parser JSON *\/
                 *     }
                 * }
                 * @endcode
                 */
                [[nodiscard]] const NkString* GetHeader(const char* key) const noexcept;
            };

            // =================================================================
            // CLASSE : NkHTTPClient — Client HTTP synchrone et asynchrone
            // =================================================================
            /**
             * @class NkHTTPClient
             * @brief Client HTTP/HTTPS léger pour communications avec APIs REST.
             *
             * Cette classe fournit une interface unifiée pour envoyer des requêtes
             * HTTP vers des services externes, avec support complet :
             *   • Requêtes synchrones : Send(), Get(), Post() — bloquantes
             *   • Requêtes asynchrones : SendAsync() — non-bloquantes avec callbacks
             *   • Gestion de configuration : timeouts, SSL, en-têtes par défaut
             *   • Authentification : Bearer tokens, Basic Auth, en-têtes personnalisés
             *   • Téléchargement de fichiers : DownloadFile() avec progression
             *   • Utilitaires : URLEncode(), URLDecode(), Base64Encode()
             *
             * MODÈLE D'EXÉCUTION :
             *   • Synchrone : bloque le thread appelant jusqu'à réception/réponse
             *     → Utiliser pour initialisation, config, ou threads dédiés
             *   • Asynchrone : retour immédiat, callback invoqué ultérieurement
             *     → Callback exécuté depuis le thread réseau — différer vers thread jeu si nécessaire
             *
             * SUPPORT TLS/SSL :
             *   • Windows/Linux/macOS : mbedTLS ou OpenSSL via compilation conditionnelle
             *   • WebAssembly : Délégation à emscripten_fetch (HTTPS natif navigateur)
             *   • Mobile : Intégration via couches d'abstraction plateforme
             *
             * USAGE TYPIQUE :
             * @code
             * // Initialisation avec configuration
             * NkHTTPClient http;
             * NkHTTPClient::Config cfg;
             * cfg.userAgent = "MyGame/1.0";
             * cfg.verifySSL = true;  // Toujours true en production
             * cfg.defaultTimeoutMs = 15000;
             * http.Configure(cfg);
             *
             * // En-tête par défaut pour toutes les requêtes
             * http.AddDefaultHeader("X-Game-Version", "1.2.3");
             * http.SetDefaultBearerToken("eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...");
             *
             * // Requête GET synchrone
             * auto resp = http.Get("https://api.example.com/config");
             * if (!resp.HasError() && resp.IsOK()) {
             *     LoadGameConfig(resp.body);
             * }
             *
             * // Requête POST asynchrone
             * NkHTTPRequest req;
             * req.url = "https://api.example.com/telemetry";
             * req.method = NkHTTPMethod::NK_HTTP_POST;
             * req.SetJSON(R"({"event":"level_complete","duration":120})");
             * http.SendAsync(req, [](const NkHTTPResponse& r) {
             *     if (!r.HasError() && r.IsOK()) {
             *         NK_LOG_DEBUG("Télémétrie envoyée");
             *     }
             * });
             * @endcode
             *
             * @note Thread-safe pour les méthodes publiques : protection via mutex internes
             * @note Les callbacks asynchrones sont invoqués depuis le thread réseau
             * @see NkHTTPRequest Pour la construction de requêtes complexes
             * @see NkHTTPResponse Pour l'interprétation des résultats
             */
            class NKENTSEU_NETWORK_CLASS_EXPORT NkHTTPClient
            {
            public:

                // -------------------------------------------------------------
                // Constructeur et destructeur
                // -------------------------------------------------------------

                /**
                 * @brief Constructeur par défaut — initialise un client avec config par défaut.
                 * @note Configuration par défaut :
                 *   • userAgent: "NKNetwork/1.0"
                 *   • defaultTimeoutMs: 10000 (10 secondes)
                 *   • verifySSL: true (recommandé pour production)
                 *   • maxAsyncThreads: 4 (équilibre performance/ressources)
                 */
                NkHTTPClient() noexcept = default;

                /**
                 * @brief Destructeur — annule toutes les requêtes asynchrones en cours.
                 * @note Attend la fin des requêtes en cours avec timeout court
                 * @note Libère les ressources réseau et mémoire associées
                 */
                ~NkHTTPClient() noexcept;

                // -------------------------------------------------------------
                // Configuration — paramètres globaux du client
                // -------------------------------------------------------------

                /**
                 * @struct Config
                 * @brief Paramètres de configuration globale du client HTTP.
                 *
                 * Cette structure encapsule les options configurables qui
                 * s'appliquent à toutes les requêtes envoyées par ce client :
                 *   • userAgent : Identifiant de l'application pour logging serveur
                 *   • defaultTimeoutMs : Délai par défaut si non spécifié dans la requête
                 *   • verifySSL : Validation des certificats TLS (toujours true en prod)
                 *   • caCertPath : Chemin vers un bundle CA personnalisé (vide = système)
                 *   • maxAsyncThreads : Nombre de threads pour le pool asynchrone
                 *
                 * @note Appliquer la configuration via Configure() avant toute requête
                 * @note Les modifications en cours d'exécution s'appliquent aux nouvelles requêtes uniquement
                 */
                struct Config
                {
                    /// Chaîne User-Agent envoyée dans toutes les requêtes.
                    /// @note Format recommandé : "AppName/Version (Platform)"
                    /// @note Défaut: "NKNetwork/1.0"
                    NkString userAgent = "NKNetwork/1.0";

                    /// Délai d'attente par défaut en millisecondes.
                    /// @note Utilisé si timeoutMs n'est pas spécifié dans NkHTTPRequest
                    /// @note Défaut: 10000ms (10 secondes)
                    uint32 defaultTimeoutMs = 10000;

                    /// Indicateur de validation des certificats SSL/TLS.
                    /// @note true = vérification stricte (recommandé)
                    /// @note false = accepte tous les certificats (développement uniquement)
                    /// @warning Ne jamais désactiver en production — risque MITM
                    bool verifySSL = true;

                    /// Chemin vers un bundle de certificats CA personnalisé.
                    /// @note Chaîne vide = utiliser les certificats système par défaut
                    /// @note Utile pour environnements avec CA interne ou offline
                    NkString caCertPath;

                    /// Nombre maximal de threads pour le pool asynchrone.
                    /// @note Défaut: 4 — équilibre entre parallélisme et consommation
                    /// @note Augmenter pour APIs à haute latence, réduire pour embarqué
                    uint32 maxAsyncThreads = 4;

                    /// Nombre maximal de connexions HTTP simultanées autorisées.
                    /// @note Défaut: 8 — limite le nombre de sockets ouverts en parallèle
                    /// @note Réduire pour environnements embarqués, augmenter pour serveurs
                    uint32 maxConnections = 8;
                };

                /**
                 * @brief Applique une configuration globale au client.
                 * @param cfg Structure Config contenant les nouveaux paramètres.
                 * @note Les modifications s'appliquent aux requêtes suivantes uniquement
                 * @note Thread-safe : copie atomique de la configuration
                 * @example
                 * @code
                 * NkHTTPClient::Config cfg;
                 * cfg.userAgent = "MyGame/2.1.0";
                 * cfg.verifySSL = true;
                 * cfg.defaultTimeoutMs = 20000;
                 * httpClient.Configure(cfg);
                 * @endcode
                 */
                void Configure(const Config& cfg) noexcept;

                // -------------------------------------------------------------
                // En-têtes par défaut — appliqués à toutes les requêtes
                // -------------------------------------------------------------

                /**
                 * @brief Ajoute un en-tête par défaut à toutes les requêtes futures.
                 * @param key Nom de l'en-tête (ex: "X-API-Key").
                 * @param value Valeur à associer.
                 * @note Les en-têtes par défaut sont ajoutés avant ceux de la requête
                 * @note Une requête peut surcharger un en-tête par défaut en l'ajoutant explicitement
                 * @example
                 * @code
                 * httpClient.AddDefaultHeader("X-Game-Version", "1.2.3");
                 * httpClient.AddDefaultHeader("Accept-Language", "fr-FR");
                 * @endcode
                 */
                void AddDefaultHeader(const char* key, const char* value) noexcept;

                /**
                 * @brief Définit un token Bearer par défaut pour authentification OAuth2.
                 * @param token Jeton d'accès OAuth2 (sans préfixe "Bearer ").
                 * @note Ajoute automatiquement Authorization: Bearer <token> à toutes les requêtes
                 * @note Remplace tout token précédemment défini
                 * @warning Les tokens sont sensibles — ne pas logger en clair
                 * @example
                 * @code
                 * httpClient.SetDefaultBearerToken("eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...");
                 * @endcode
                 */
                void SetDefaultBearerToken(const char* token) noexcept;

                // -------------------------------------------------------------
                // Requêtes synchrones — exécution bloquante
                // -------------------------------------------------------------

                /**
                 * @brief Envoie une requête HTTP complète et attend la réponse.
                 * @param req Requête configurée à envoyer.
                 * @return NkHTTPResponse contenant le résultat ou l'erreur.
                 * @note Bloque le thread appelant jusqu'à complétion ou timeout
                 * @note Gère automatiquement les redirections si req.followRedirects = true
                 * @note Thread-safe : peut être appelé depuis n'importe quel thread
                 * @warning Ne pas appeler depuis le thread principal/UI — préférer SendAsync()
                 */
                [[nodiscard]] NkHTTPResponse Send(const NkHTTPRequest& req) noexcept;

                /**
                 * @brief Envoie une requête GET simple et attend la réponse.
                 * @param url URL complète de l'endpoint à requêter.
                 * @return NkHTTPResponse contenant le résultat ou l'erreur.
                 * @note Méthode pratique pour les requêtes GET sans corps ni en-têtes complexes
                 * @note Utilise la configuration par défaut du client (timeout, headers, etc.)
                 * @example
                 * @code
                 * auto resp = httpClient.Get("https://api.example.com/version");
                 * if (!resp.HasError() && resp.IsOK()) {
                 *     NK_LOG_INFO("Server version: {}", resp.body.CStr());
                 * }
                 * @endcode
                 */
                [[nodiscard]] NkHTTPResponse Get(const char* url) noexcept;

                /**
                 * @brief Envoie une requête POST JSON simple et attend la réponse.
                 * @param url URL complète de l'endpoint à requêter.
                 * @param json Contenu JSON à envoyer dans le corps de la requête.
                 * @return NkHTTPResponse contenant le résultat ou l'erreur.
                 * @note Configure automatiquement Content-Type et Accept pour JSON
                 * @note Méthode pratique pour les APIs REST acceptant du JSON
                 * @example
                 * @code
                 * auto resp = httpClient.Post(
                 *     "https://api.example.com/users",
                 *     R"({"name":"Alice","email":"alice@example.com"})"
                 * );
                 * @endcode
                 */
                [[nodiscard]] NkHTTPResponse Post(
                    const char* url,
                    const char* json
                ) noexcept;

                // -------------------------------------------------------------
                // Requêtes asynchrones — exécution non-bloquante
                // -------------------------------------------------------------

                /**
                 * @typedef Callback
                 * @brief Signature des callbacks pour réponses asynchrones.
                 * @param resp Réponse HTTP complète (succès ou erreur).
                 * @note Invoqué depuis le thread réseau — différer vers thread jeu si nécessaire
                 * @note La réponse est copiée — sûre à utiliser dans le callback
                 */
                using Callback = NkFunction<void(const NkHTTPResponse&)>;

                /**
                 * @brief Envoie une requête HTTP asynchrone avec callback de réponse.
                 * @param req Requête configurée à envoyer.
                 * @param cb Callback invoqué quand la réponse est reçue ou en cas d'erreur.
                 * @return ID unique de la requête pour annulation ultérieure.
                 * @note Retour immédiat — la requête s'exécute en arrière-plan
                 * @note Le callback est invoqué depuis le thread réseau
                 * @warning Utiliser NkGameplayEventBus::Queue() pour ramener sur le thread jeu
                 * @example
                 * @code
                 * uint32 reqId = httpClient.SendAsync(myRequest,
                 *     [](const NkHTTPResponse& resp) {
                 *         if (!resp.HasError() && resp.IsOK()) {
                 *             // Ramener sur le thread jeu si nécessaire
                 *             NkGameplayEventBus::Queue([resp]() {
                 *                 ProcessApiResponse(resp.body);
                 *             });
                 *         }
                 *     }
                 * );
                 * // reqId peut être utilisé pour Cancel(reqId) si besoin
                 * @endcode
                 */
                [[nodiscard]] uint32 SendAsync(
                    const NkHTTPRequest& req,
                    Callback cb
                ) noexcept;

                /**
                 * @brief Annule une requête asynchrone en cours par son ID.
                 * @param requestId ID retourné par SendAsync() pour la requête à annuler.
                 * @note Si la requête est déjà terminée, l'annulation est ignorée
                 * @note Si la requête est en cours, elle est marquée pour annulation
                 * @note Le callback peut quand même être invoqué avec une réponse partiellement reçue
                 * @example
                 * @code
                 * // Annuler une requête si l'utilisateur change d'écran
                 * if (pendingRequestId != 0) {
                 *     httpClient.Cancel(pendingRequestId);
                 *     pendingRequestId = 0;
                 * }
                 * @endcode
                 */
                void Cancel(uint32 requestId) noexcept;

                /**
                 * @brief Annule toutes les requêtes asynchrones en cours.
                 * @note Utile pour cleanup lors de changement d'état ou déconnexion
                 * @note Les callbacks des requêtes annulées peuvent quand même être invoqués
                 * @note Thread-safe : peut être appelé depuis n'importe quel thread
                 */
                void CancelAll() noexcept;

                /**
                 * @brief Attend la complétion de toutes les requêtes asynchrones en cours.
                 * @param timeoutMs Délai maximal d'attente en millisecondes (défaut: infini).
                 * @note Bloque le thread appelant jusqu'à complétion ou timeout
                 * @note Utile pour shutdown propre ou tests synchrones
                 * @warning Ne pas appeler depuis le thread principal/UI avec timeout infini
                 */
                void WaitAll(uint32 timeoutMs = UINT32_MAX) noexcept;

                /**
                 * @brief Retourne le nombre de requêtes asynchrones actuellement en cours.
                 * @return Compteur de requêtes actives (0 si aucune).
                 * @note Utile pour monitoring, debugging ou décision de throttling
                 * @note Thread-safe : lecture atomique du compteur interne
                 */
                [[nodiscard]] uint32 PendingCount() const noexcept;

                // -------------------------------------------------------------
                // Téléchargement de fichiers — avec progression
                // -------------------------------------------------------------

                /**
                 * @brief Télécharge un fichier depuis une URL vers le système de fichiers local.
                 * @param url URL source du fichier à télécharger.
                 * @param destPath Chemin de destination local pour le fichier.
                 * @param onProgress Callback optionnel invoqué périodiquement avec la progression.
                 * @param onComplete Callback optionnel invoqué à la fin (succès ou échec).
                 * @return ID unique de la requête pour suivi ou annulation.
                 * @note Exécution asynchrone — retour immédiat
                 * @note onProgress : paramètres (bytesReceived, totalBytes), totalBytes = 0 si inconnu
                 * @note onComplete : réponse HTTP complète pour vérification du statut
                 * @note Le fichier est écrit de façon atomique (temp file + rename) pour intégrité
                 * @example
                 * @code
                 * uint32 dlId = httpClient.DownloadFile(
                 *     "https://cdn.example.com/assets/level1.dat",
                 *     "/game/data/level1.dat",
                 *     [](uint64 received, uint64 total) {
                 *         if (total > 0) {
                 *             float progress = static_cast<float>(received) / total;
                 *             UI::UpdateDownloadProgress(progress);
                 *         }
                 *     },
                 *     [](const NkHTTPResponse& resp) {
                 *         if (!resp.HasError() && resp.IsOK()) {
                 *             NK_LOG_INFO("Téléchargement terminé");
                 *         } else {
                 *             NK_LOG_ERROR("Échec : {}", resp.error.IsEmpty() ? resp.statusText.CStr() : resp.error.CStr());
                 *         }
                 *     }
                 * );
                 * @endcode
                 */
                [[nodiscard]] uint32 DownloadFile(
                    const char* url,
                    const char* destPath,
                    NkFunction<void(uint64, uint64)> onProgress = nullptr,
                    Callback onComplete = nullptr
                ) noexcept;

                // -------------------------------------------------------------
                // Utilitaires statiques — encodages et transformations
                // -------------------------------------------------------------

                /**
                 * @brief Encode une chaîne pour inclusion dans une URL (percent-encoding).
                 * @param s Chaîne à encoder.
                 * @return Chaîne encodée avec caractères spéciaux remplacés par %XX.
                 * @note Conforme à RFC 3986 : encode tout sauf alphanumérique et -._~
                 * @note Utile pour encoder des paramètres de requête ou des segments de chemin
                 * @example
                 * @code
                 * NkString encoded = NkHTTPClient::URLEncode("Hello World! @#$");
                 * // Résultat : "Hello%20World%21%20%40%23%24"
                 * @endcode
                 */
                [[nodiscard]] static NkString URLEncode(const char* s) noexcept;

                /**
                 * @brief Décode une chaîne URL-encoded vers sa forme originale.
                 * @param s Chaîne encodée à décoder.
                 * @return Chaîne décodée avec %XX remplacés par leurs caractères originaux.
                 * @note Gère les encodages %XX en hexadécimal
                 * @note Utile pour parser des paramètres de requête reçus
                 * @example
                 * @code
                 * NkString decoded = NkHTTPClient::URLDecode("Hello%20World%21");
                 * // Résultat : "Hello World!"
                 * @endcode
                 */
                [[nodiscard]] static NkString URLDecode(const char* s) noexcept;

                /**
                 * @brief Encode des données binaires en chaîne Base64.
                 * @param data Pointeur vers les données binaires à encoder.
                 * @param size Nombre d'octets à encoder.
                 * @return Chaîne Base64 représentant les données d'entrée.
                 * @note Conforme à RFC 4648 : alphabet standard, pas de line breaks
                 * @note Utile pour encoder des credentials HTTP Basic ou des payloads binaires
                 * @example
                 * @code
                 * const char* credentials = "user:pass";
                 * NkString b64 = NkHTTPClient::Base64Encode(
                 *     reinterpret_cast<const uint8*>(credentials),
                 *     static_cast<uint32>(std::strlen(credentials))
                 * );
                 * // Résultat : "dXNlcjpwYXNz"
                 * @endcode
                 */
                [[nodiscard]] static NkString Base64Encode(
                    const uint8* data,
                    uint32 size
                ) noexcept;

            private:

                // -------------------------------------------------------------
                // Parsing HTTP — construction et analyse des messages
                // -------------------------------------------------------------

                /**
                 * @brief Parse une réponse HTTP brute en structure NkHTTPResponse.
                 * @param raw Chaîne brute reçue du serveur (en-têtes + corps).
                 * @return NkHTTPResponse peuplé ou avec error si parsing échoue.
                 * @note Gère HTTP/1.1 avec en-têtes multi-lignes et chunked encoding basique
                 * @note Extrait statusCode, statusText, headers et body séparément
                 */
                [[nodiscard]] NkHTTPResponse ParseResponse(const NkString& raw) const noexcept;

                /**
                 * @brief Construit une chaîne de requête HTTP à partir de NkHTTPRequest.
                 * @param req Requête source à sérialiser.
                 * @return Chaîne HTTP valide prête à envoyer sur le socket.
                 * @note Format : "METHOD /path HTTP/1.1\r\n" + headers + "\r\n" + body
                 * @note Ajoute automatiquement Host, User-Agent, Content-Length si manquants
                 */
                [[nodiscard]] NkString BuildRequestStr(const NkHTTPRequest& req) const noexcept;

                /**
                 * @brief Parse une URL en ses composants : schéma, hôte, port, chemin.
                 * @param url URL complète à parser (ex: "https://api.example.com:443/v1/users").
                 * @param scheme [OUT] Schéma extrait ("http" ou "https").
                 * @param host [OUT] Nom d'hôte ou IP extrait.
                 * @param port [OUT] Port extrait (défaut: 80 pour http, 443 pour https).
                 * @param path [OUT] Chemin et query string extraits ("/v1/users?key=value").
                 * @return true si parsing réussi, false si URL mal formée.
                 * @note Gère les URLs avec ou sans port explicite
                 * @note Normalise les chemins avec slash initial si manquant
                 */
                [[nodiscard]] bool ParseURL(
                    const NkString& url,
                    NkString& scheme,
                    NkString& host,
                    uint16& port,
                    NkString& path
                ) const noexcept;

                // -------------------------------------------------------------
                // Transport — envoi via TCP ou TLS
                // -------------------------------------------------------------

                /**
                 * @brief Envoie une requête HTTP via socket TCP non-chiffré.
                 * @param req Requête à envoyer.
                 * @return NkHTTPResponse contenant la réponse ou l'erreur.
                 * @note Utilisé uniquement pour URLs http:// (non recommandé en production)
                 * @note Implémente la boucle send/recv avec timeout et gestion d'erreurs
                 */
                [[nodiscard]] NkHTTPResponse SendOverTCP(const NkHTTPRequest& req) noexcept;

                /**
                 * @brief Envoie une requête HTTP via socket TLS chiffré.
                 * @param req Requête à envoyer.
                 * @return NkHTTPResponse contenant la réponse ou l'erreur.
                 * @note Utilisé pour URLs https:// avec validation de certificat
                 * @note Implémentation via mbedTLS/OpenSSL selon plateforme (ifdefs)
                 * @note Gère handshake TLS, vérification de certificat, chiffrement/déchiffrement
                 */
                [[nodiscard]] NkHTTPResponse SendOverTLS(const NkHTTPRequest& req) noexcept;

                // -------------------------------------------------------------
                // Gestion asynchrone — pool de requêtes et threads
                // -------------------------------------------------------------

                /**
                 * @struct AsyncRequest
                 * @brief Conteneur interne pour suivi d'une requête asynchrone.
                 *
                 * Cette structure encapsule l'état d'une requête en cours
                 * d'exécution dans le pool asynchrone :
                 *   • id : Identifiant unique pour annulation/recherche
                 *   • req : Copie de la requête à envoyer
                 *   • callback : Fonction à invoquer à la complétion
                 *   • cancelled : Flag atomique pour annulation thread-safe
                 *   • done : Flag atomique indiquant la complétion
                 *
                 * @note Thread-safe via atomics pour flags de contrôle
                 * @note La requête et le callback sont copiés pour sécurité mémoire
                 */
                struct AsyncRequest
                {
                    /// Identifiant unique de la requête (généré automatiquement).
                    uint32 id = 0;

                    /// Copie de la requête HTTP à exécuter.
                    NkHTTPRequest req;

                    /// Callback à invoquer à la complétion (succès ou échec).
                    Callback callback;

                    /// Flag d'annulation — modifiable depuis n'importe quel thread.
                    NkAtomic<bool> cancelled{false};

                    /// Flag de complétion — indique que le callback a été invoqué.
                    NkAtomic<bool> done{false};
                };

                /// Liste des requêtes asynchrones actuellement en cours.
                NkVector<AsyncRequest*> mAsyncRequests;

                /// Mutex protégeant l'accès concurrent à mAsyncRequests.
                mutable threading::NkMutex mAsyncMutex;

                /// Compteur atomique pour génération d'IDs uniques de requêtes.
                NkAtomic<uint32> mNextReqId{1};

                // -------------------------------------------------------------
                // État interne — configuration et en-têtes par défaut
                // -------------------------------------------------------------

                /// Configuration globale appliquée à toutes les requêtes.
                Config mConfig;

                /// Liste des en-têtes à ajouter automatiquement à chaque requête.
                NkVector<NkHTTPHeader> mDefaultHeaders;

                // -------------------------------------------------------------
                // Méthodes internes — gestion du cycle de vie asynchrone
                // -------------------------------------------------------------

                /**
                 * @brief Corps d'exécution d'une requête asynchrone dans son thread.
                 * @param asyncReq Pointeur vers la requête à exécuter (non nul).
                 * @note Exécute Send(), invoque le callback, marque done, puis cleanup
                 * @note Vérifie cancelled avant et après Send() pour arrêt rapide
                 */
                void AsyncWorker(AsyncRequest* asyncReq) noexcept;

                /**
                 * @brief Supprime une requête terminée de la liste et libère sa mémoire.
                 * @param asyncReq Pointeur vers la requête à retirer.
                 * @note Thread-safe : verrouille mAsyncMutex avant modification de la liste
                 */
                void CleanupRequest(AsyncRequest* asyncReq) noexcept;

                /**
                 * @brief Supprime toutes les requêtes marquées done de la liste.
                 * @note Thread-safe : verrouille mAsyncMutex pendant le parcours
                 * @note Libère la mémoire des AsyncRequest complétés
                 */
                void CleanupFinishedRequests() noexcept;
            };

            // =================================================================
            // STRUCTURE : NkLeaderboardEntry — Entrée d'un classement en ligne
            // =================================================================
            /**
             * @struct NkLeaderboardEntry
             * @brief Représente une entrée unique dans un leaderboard distant.
             *
             * Cette structure stocke les données d'une ligne de classement :
             *   • rank : Position dans le classement (1 = premier)
             *   • score : Valeur numérique du score
             *   • playerName : Nom du joueur (jusqu'à 64 caractères)
             *   • extraData : Champ JSON optionnel pour métadonnées supplémentaires
             *
             * @note playerName est un tableau C fixe pour éviter les allocations heap
             * @note extraData peut contenir des données arbitraires (avatars, badges, etc.)
             * @see NkLeaderboard Pour les méthodes de récupération de classement
             */
            struct NKENTSEU_NETWORK_CLASS_EXPORT NkLeaderboardEntry
            {
                /// Position dans le classement (1 = meilleur score).
                uint32 rank = 0;

                /// Valeur numérique du score associé.
                uint64 score = 0;

                /// Nom du joueur (tableau C fixe, 64 caractères max).
                /// @note Utiliser NkString(e.playerName) pour conversion si nécessaire
                char playerName[64] = {};

                /// Données JSON supplémentaires optionnelles (avatars, badges, etc.).
                /// @note Peut être vide — vérifier via playerName[0] != '\0' pour validité
                char extraData[256] = {};
            };

            // =================================================================
            // CLASSE : NkLeaderboard — Client générique pour leaderboards REST
            // =================================================================
            /**
             * @class NkLeaderboard
             * @brief Client spécialisé pour interactions avec APIs de leaderboard.
             *
             * Cette classe fournit une interface de haut niveau pour les opérations
             * courantes sur des systèmes de classement en ligne :
             *   • SubmitScore() : Envoi d'un nouveau score pour un joueur
             *   • FetchTop() : Récupération du classement top-N
             *   • FetchPlayerRank() : Récupération du rang et score d'un joueur spécifique
             *
             * FORMAT DE DONNÉES :
             *   • Requêtes : JSON avec playerName, score, extraData optionnel
             *   • Réponses : JSON avec liste d'entrées NkLeaderboardEntry
             *   • ExtraData : Champ JSON libre pour métadonnées personnalisées
             *
             * INTÉGRATION :
             *   • Utilise NkHTTPClient en interne pour toutes les communications
             *   • Configurez baseUrl et apiKey via Configure() avant utilisation
             *   • Les callbacks sont invoqués depuis le thread réseau — différer si nécessaire
             *
             * USAGE TYPIQUE :
             * @code
             * // Configuration
             * NkLeaderboard leaderboard;
             * leaderboard.Configure(
             *     "https://api.example.com/leaderboards/global",
             *     "your-api-key-here"
             * );
             *
             * // Soumission de score
             * leaderboard.SubmitScore("Alice", 15420, [](bool success) {
             *     if (success) {
             *         NK_LOG_INFO("Score soumis avec succès");
             *     }
             * });
             *
             * // Récupération du top 10
             * leaderboard.FetchTop(10, [](NkVector<NkLeaderboardEntry>& entries) {
             *     for (const auto& entry : entries) {
             *         NK_LOG_INFO("#{} {} : {} pts", entry.rank, entry.playerName, entry.score);
             *     }
             * });
             *
             * // Récupération du rang d'un joueur
             * leaderboard.FetchPlayerRank("Alice", [](uint32 rank, uint64 score) {
             *     NK_LOG_INFO("Alice est #{} avec {} pts", rank, score);
             * });
             * @endcode
             *
             * @note Thread-safe pour les méthodes publiques : délégation vers NkHTTPClient thread-safe
             * @note Les callbacks sont invoqués depuis le thread réseau — différer vers thread jeu si nécessaire
             * @see NkLeaderboardEntry Pour la structure des données de classement
             */
            class NKENTSEU_NETWORK_CLASS_EXPORT NkLeaderboard
            {
            public:

                /**
                 * @brief Configure le client avec l'endpoint de l'API et la clé d'authentification.
                 * @param baseUrl URL de base de l'API leaderboard (ex: "https://api.example.com/leaderboards/global").
                 * @param apiKey Clé d'API pour authentification des requêtes.
                 * @note À appeler avant toute opération SubmitScore/FetchTop/FetchPlayerRank
                 * @note L'apiKey est ajoutée automatiquement comme en-tête X-API-Key
                 */
                void Configure(const char* baseUrl, const char* apiKey) noexcept;

                /**
                 * @brief Soumet un nouveau score pour un joueur vers le leaderboard.
                 * @param playerName Nom du joueur à enregistrer.
                 * @param score Valeur numérique du score à soumettre.
                 * @param cb Callback optionnel invoqué avec true=succès, false=échec.
                 * @note Envoie une requête POST JSON vers {baseUrl}/submit
                 * @note Le callback est invoqué depuis le thread réseau — différer si nécessaire
                 * @example
                 * @code
                 * leaderboard.SubmitScore("Alice", 15420, [](bool success) {
                 *     if (success) {
                 *         /\* Afficher confirmation UI *\/
                 *     } else {
                 *         /\* Gérer l'échec *\/
                 *     }
                 * });
                 * @endcode
                 */
                void SubmitScore(
                    const char* playerName,
                    uint64 score,
                    NkFunction<void(bool success)> cb = nullptr
                ) noexcept;

                /**
                 * @brief Récupère le classement des N meilleurs joueurs.
                 * @param count Nombre d'entrées à récupérer (typiquement 10, 50, 100).
                 * @param cb Callback invoqué avec la liste des entrées triées par rang.
                 * @note Envoie une requête GET vers {baseUrl}/top?count=N
                 * @note Le callback est invoqué depuis le thread réseau — différer si nécessaire
                 * @note La liste est déjà triée par rank croissant (1 = meilleur)
                 * @example
                 * @code
                 * leaderboard.FetchTop(10, [](NkVector<NkLeaderboardEntry>& entries) {
                 *     for (const auto& entry : entries) {
                 *         UI::AddLeaderboardRow(entry.rank, entry.playerName, entry.score);
                 *     }
                 * });
                 * @endcode
                 */
                void FetchTop(
                    uint32 count,
                    NkFunction<void(NkVector<NkLeaderboardEntry>&)> cb
                ) noexcept;

                /**
                 * @brief Récupère le rang et le score actuel d'un joueur spécifique.
                 * @param playerName Nom exact du joueur à rechercher.
                 * @param cb Callback invoqué avec le rang (0 si non classé) et le score.
                 * @note Envoie une requête GET vers {baseUrl}/player?name=<encoded>
                 * @note Le nom est automatiquement URL-encoded pour sécurité
                 * @note rank = 0 indique que le joueur n'est pas dans le classement
                 * @example
                 * @code
                 * leaderboard.FetchPlayerRank("Alice", [](uint32 rank, uint64 score) {
                 *     if (rank > 0) {
                 *         NK_LOG_INFO("Alice est #{} avec {} pts", rank, score);
                 *     } else {
                 *         NK_LOG_INFO("Alice n'est pas encore classée");
                 *     }
                 * });
                 * @endcode
                 */
                void FetchPlayerRank(
                    const char* playerName,
                    NkFunction<void(uint32 rank, uint64 score)> cb
                ) noexcept;

            private:

                /// Client HTTP interne pour toutes les communications réseau.
                NkHTTPClient mHttp;

                /// URL de base de l'API leaderboard (configurée via Configure()).
                NkString mBaseUrl;

                /// Clé d'API pour authentification (ajoutée comme en-tête X-API-Key).
                NkString mApiKey;
            };

        } // namespace net

    } // namespace nkentseu

#endif // NKENTSEU_NETWORK_NKHTTPCLIENT_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKHTTPCLIENT.H
// =============================================================================
// Ce fichier fournit le client HTTP/HTTPS pour APIs REST.
// Les exemples suivants illustrent les cas d'usage courants.

// -----------------------------------------------------------------------------
// Exemple 1 : Requête GET synchrone pour récupération de configuration
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/HTTP/NkHTTPClient.h"
    #include "NKCore/Logger/NkLogger.h"

    bool LoadRemoteConfig()
    {
        using namespace nkentseu::net;

        NkHTTPClient http;

        // Configuration optionnelle
        NkHTTPClient::Config cfg;
        cfg.userAgent = "MyGame/1.2.3";
        cfg.defaultTimeoutMs = 15000;  // 15 secondes
        cfg.verifySSL = true;  // Toujours true en production
        http.Configure(cfg);

        // Requête GET synchrone
        NkHTTPResponse resp = http.Get("https://config.example.com/game.json");

        // Vérification d'erreur réseau
        if (resp.HasError())
        {
            NK_LOG_ERROR("Échec de récupération de config : {}", resp.error.CStr());
            return false;
        }

        // Vérification de statut HTTP
        if (!resp.IsOK())
        {
            NK_LOG_WARN("Erreur HTTP {} : {}", resp.statusCode, resp.statusText.CStr());
            return false;
        }

        // Parsing de la réponse JSON (pseudo-code)
        if (ParseGameConfigJSON(resp.body))
        {
            NK_LOG_INFO("Configuration chargée avec succès");
            return true;
        }
        else
        {
            NK_LOG_ERROR("Échec de parsing du JSON de configuration");
            return false;
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Requête POST asynchrone avec authentification Bearer
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/HTTP/NkHTTPClient.h"

    void SubmitTelemetry(const char* eventName, uint32 value)
    {
        using namespace nkentseu::net;

        static NkHTTPClient sHttp;  // Singleton pour réutilisation de connexions
        static bool sConfigured = false;

        // Configuration unique au premier appel
        if (!sConfigured)
        {
            NkHTTPClient::Config cfg;
            cfg.userAgent = "MyGame/1.2.3";
            cfg.verifySSL = true;
            sHttp.Configure(cfg);
            sHttp.SetDefaultBearerToken("eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...");
            sConfigured = true;
        }

        // Construction de la requête
        NkHTTPRequest req;
        req.url = "https://telemetry.example.com/events";
        req.method = NkHTTPMethod::NK_HTTP_POST;
        req.SetJSON(
            NkString::Format(R"({"event":"%s","value":%u,"timestamp":%llu})",
                eventName, value, static_cast<unsigned long long>(NkNetNowMs()))
        );

        // Envoi asynchrone
        sHttp.SendAsync(req, [](const NkHTTPResponse& resp) {
            // Callback invoqué depuis le thread réseau
            if (resp.HasError())
            {
                NK_LOG_DEBUG("Échec télémétrie (réseau) : {}", resp.error.CStr());
                return;
            }

            if (!resp.IsOK())
            {
                NK_LOG_DEBUG("Échec télémétrie (HTTP {}) : {}",
                    resp.statusCode, resp.statusText.CStr());
                return;
            }

            NK_LOG_DEBUG("Télémétrie envoyée avec succès");
        });
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Téléchargement de fichier avec progression
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/HTTP/NkHTTPClient.h"
    #include "NKCore/Logger/NkLogger.h"

    class AssetDownloader
    {
    public:
        void StartDownload(const char* assetUrl, const char* destPath)
        {
            using namespace nkentseu::net;

            mDownloadId = mHttp.DownloadFile(
                assetUrl,
                destPath,
                // Callback de progression
                [this](uint64 received, uint64 total) {
                    if (total > 0)
                    {
                        float progress = static_cast<float>(received) / static_cast<float>(total);
                        // Mise à jour de l'UI de progression (thread-safe)
                        UI::SetDownloadProgress(progress);
                    }
                },
                // Callback de complétion
                [this, destPath](const NkHTTPResponse& resp) {
                    if (!resp.HasError() && resp.IsOK())
                    {
                        NK_LOG_INFO("Téléchargement terminé : {}", destPath);
                        // Notification au système d'assets
                        AssetManager::OnAssetDownloaded(destPath);
                    }
                    else
                    {
                        NK_LOG_ERROR("Échec de téléchargement : {}",
                            resp.error.IsEmpty() ? resp.statusText.CStr() : resp.error.CStr());
                        // Nettoyage du fichier partiel
                        PlatformFile::Delete(destPath);
                    }
                    mDownloadId = 0;
                }
            );
        }

        void CancelDownload()
        {
            if (mDownloadId != 0)
            {
                mHttp.Cancel(mDownloadId);
                mDownloadId = 0;
                NK_LOG_DEBUG("Téléchargement annulé par l'utilisateur");
            }
        }

    private:
        NkHTTPClient mHttp;
        uint32 mDownloadId = 0;
    };
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Utilisation de NkLeaderboard pour classement en ligne
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/HTTP/NkLobby.h"  // Inclut NkHTTPClient

    class GameLeaderboard
    {
    public:
        void Initialize()
        {
            using namespace nkentseu::net;

            mLb.Configure(
                "https://api.example.com/leaderboards/global_ranked",
                "your-api-key-here"  // Stocker de façon sécurisée en production
            );
        }

        void OnMatchEnd(uint64 playerScore, const char* playerName)
        {
            // Soumission du score après une partie
            mLb.SubmitScore(playerName, playerScore, [this](bool success) {
                if (success)
                {
                    NK_LOG_INFO("Score soumis : {} pts", playerScore);
                    // Rafraîchir l'affichage du classement
                    RefreshLeaderboard();
                }
                else
                {
                    NK_LOG_WARN("Échec de soumission du score");
                }
            });
        }

        void RefreshLeaderboard()
        {
            // Récupération du top 10 pour affichage UI
            mLb.FetchTop(10, [](NkVector<NkLeaderboardEntry>& entries) {
                UI::ClearLeaderboard();
                for (const auto& entry : entries)
                {
                    UI::AddLeaderboardRow(
                        entry.rank,
                        entry.playerName,
                        entry.score,
                        entry.extraData  // JSON optionnel pour avatars, badges, etc.
                    );
                }
            });
        }

        void ShowPlayerRank(const char* playerName)
        {
            // Affichage du rang personnel
            mLb.FetchPlayerRank(playerName, [](uint32 rank, uint64 score) {
                if (rank > 0)
                {
                    UI::ShowPersonalRank(rank, score);
                }
                else
                {
                    UI::ShowMessage("Jouez plus pour apparaître au classement !");
                }
            });
        }

    private:
        nkentseu::net::NkLeaderboard mLb;
    };
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Gestion robuste des erreurs et retry logique
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/HTTP/NkHTTPClient.h"
    #include "NKCore/Logger/NkLogger.h"

    NkHTTPResponse SendWithRetry(
        NkHTTPClient& http,
        const NkHTTPRequest& req,
        uint32 maxAttempts = 3
    )
    {
        using namespace nkentseu::net;

        for (uint32 attempt = 1; attempt <= maxAttempts; ++attempt)
        {
            NkHTTPResponse resp = http.Send(req);

            // Succès immédiat
            if (!resp.HasError() && resp.IsOK())
            {
                return resp;
            }

            // Erreurs réseau transitoires : retry avec backoff
            if (resp.HasError())
            {
                NK_LOG_WARN("Tentative {}/{} échouée (réseau) : {}",
                    attempt, maxAttempts, resp.error.CStr());

                if (attempt < maxAttempts)
                {
                    // Backoff exponentiel simple : 1s, 2s, 4s...
                    PlatformThread::Sleep(1000u * (1u << (attempt - 1)));
                    continue;
                }
            }

            // Erreurs HTTP 5xx : potentiellement transitoires
            if (resp.IsServerError() && attempt < maxAttempts)
            {
                NK_LOG_WARN("Tentative {}/{} échouée (HTTP {})",
                    attempt, maxAttempts, resp.statusCode);

                // Attendre avant retry pour éviter de surcharger le serveur
                PlatformThread::Sleep(2000);
                continue;
            }

            // Erreurs non-récupérables ou dernière tentative
            if (resp.HasError())
            {
                NK_LOG_ERROR("Échec définitif (réseau) : {}", resp.error.CStr());
            }
            else
            {
                NK_LOG_ERROR("Échec définitif (HTTP {}) : {}",
                    resp.statusCode, resp.statusText.CStr());
            }

            return resp;
        }

        // Fallback : retour d'une réponse d'erreur générique
        NkHTTPResponse fallback;
        fallback.error = "Max retries exceeded";
        return fallback;
    }
*/

// -----------------------------------------------------------------------------
// Bonnes pratiques et notes d'utilisation
// -----------------------------------------------------------------------------
/*
    Sécurité SSL/TLS :
    -----------------
    - Toujours garder verifySSL = true en production
    - Utiliser caCertPath uniquement pour environnements avec CA interne
    - Ne jamais désactiver la validation de certificat — risque d'attaques MITM

    Gestion des timeouts :
    ---------------------
    - Ajuster timeoutMs selon l'API cible : 5-10s pour APIs rapides, 30s+ pour uploads
    - Éviter timeout = 0 (infini) — risque de blocage permanent en cas de problème réseau
    - Implémenter une logique de retry avec backoff pour les erreurs transitoires

    Requêtes asynchrones :
    ---------------------
    - Préférer SendAsync() pour ne pas bloquer le thread principal/UI
    - Les callbacks sont invoqués depuis le thread réseau — différer vers thread jeu via NkGameplayEventBus
    - Annuler les requêtes en cours (Cancel/CancelAll) lors de changements d'état pour éviter les callbacks obsolètes

    Authentification :
    -----------------
    - Stocker les tokens/keys de façon sécurisée (pas en clair dans le code)
    - Utiliser SetBearerToken() ou AddHeader("Authorization", ...) pour OAuth2
    - Renouveler les tokens expirés automatiquement via logique de retry + refresh token

    Gestion de la bande passante :
    -----------------------------
    - Compresser les payloads JSON si possible (gzip, mais nécessite support serveur)
    - Utiliser des formats binaires (protobuf, flatbuffers) pour les données fréquentes
    - Mettre en cache les réponses statiques (config, assets metadata) avec validation ETag/Last-Modified

    Logging et debugging :
    ---------------------
    - Logger les URLs et statuts HTTP (sans corps ni tokens) pour tracing
    - Inclure timeMs dans les logs de performance pour détection de latence anormale
    - Activer le logging détaillé des en-têtes en développement uniquement

    Thread-safety :
    --------------
    - NkHTTPClient est thread-safe pour les appels publics via mutex internes
    - Les callbacks asynchrones sont invoqués depuis le thread réseau — protéger l'accès aux données partagées
    - Éviter les allocations heap dans les callbacks pour minimiser la pression sur l'allocateur

    Extensions futures :
    -------------------
    - Support HTTP/2 pour multiplexing et compression HPACK
    - WebSockets pour communications bidirectionnelles temps réel
    - Cache HTTP intégré avec politique LRU et validation conditionnelle
    - Proxy support pour environnements corporate avec filtrage réseau
*/

// ============================================================
// Copyright © 2024-2026 Rihen. Tous droits réservés.
// Licence Propriétaire - Libre d'utilisation et de modification
// ============================================================