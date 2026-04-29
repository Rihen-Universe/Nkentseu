// =============================================================================
// NKFileSystem/NkFileWatcher.h
// Surveillance cross-platform des changements de fichiers/répertoires.
//
// Design :
//  - Abstraction des APIs natives : inotify (Linux), ReadDirectoryChangesW (Windows)
//  - Fallback silencieux sur Web/EMSCRIPTEN (pas de surveillance active)
//  - Callback orienté objet via interface NkFileWatcherCallback
//  - Adaptateur fonctionnel NkSimpleFileWatcher pour C++11+
//  - Thread de surveillance dédié avec gestion propre du cycle de vie
//
// Règles d'application des macros :
//  - NKENTSEU_FILESYSTEM_CLASS_EXPORT sur la classe uniquement
//  - PAS de NKENTSEU_FILESYSTEM_API sur les méthodes (l'export de classe suffit)
//  - PAS de macro sur les fonctions inline définies dans le header
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_FILESYSTEM_NKFILEWATCHER_H
#define NKENTSEU_FILESYSTEM_NKFILEWATCHER_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion de l'API d'export NKFileSystem
    // Inclusion des types de base NKCore et des conteneurs NKEntseu

    #include "NKFileSystem/NkFileSystemApi.h"
    #include "NKCore/NkTypes.h"
    #include "NKContainers/String/NkString.h"
    #include "NKContainers/Sequential/NkVector.h"
    #include "NKFileSystem/NkPath.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : NAMESPACE PRINCIPAL ET DÉCLARATIONS
    // -------------------------------------------------------------------------
    // Déclarations des types fondamentaux pour la surveillance de fichiers.

    namespace nkentseu {

        // =====================================================================
        //  Énumération : NkFileChangeType
        // =====================================================================
        // Types d'événements de changement de fichier supportés par le watcher.

        enum class NkFileChangeType
        {
            NK_CREATED,              ///< Fichier ou répertoire nouvellement créé.
            NK_DELETED,              ///< Fichier ou répertoire supprimé.
            NK_MODIFIED,             ///< Contenu du fichier modifié.
            NK_RENAMED,              ///< Fichier ou répertoire renommé ou déplacé.
            NK_ATTRIBUTE_CHANGED     ///< Métadonnées modifiées (permissions, date, etc.).
        };

        // =====================================================================
        //  Structure : NkFileChangeEvent
        // =====================================================================
        // Conteneur de données transportant les informations d'un événement détecté.

        struct NKENTSEU_FILESYSTEM_CLASS_EXPORT NkFileChangeEvent
        {
            NkFileChangeType Type;      ///< Type de l'événement survenu.
            NkString Path;              ///< Chemin complet de l'élément concerné.
            NkString OldPath;           ///< Ancien chemin (uniquement pour NK_RENAMED).
            nk_int64 Timestamp;         ///< Horodatage de l'événement (epoch Unix).

            // -------------------------------------------------------------
            // Constructeurs
            // -------------------------------------------------------------

            /// Constructeur par défaut : initialise tous les membres à des valeurs neutres.
            NkFileChangeEvent();

            /// Constructeur paramétré avec type et chemin.
            /// @param type Type de l'événement.
            /// @param path Chemin de l'élément concerné (peut être nullptr).
            NkFileChangeEvent(NkFileChangeType type, const char* path);
        };

        // =====================================================================
        //  Interface : NkFileWatcherCallback
        // =====================================================================
        // Interface abstraite pour recevoir les notifications de changement de fichiers.
        // Les utilisateurs doivent hériter de cette classe et implémenter OnFileChanged().

        class NKENTSEU_FILESYSTEM_CLASS_EXPORT NkFileWatcherCallback
        {
            public:

                // -------------------------------------------------------------
                // Destructeur
                // -------------------------------------------------------------

                /// Destructeur virtuel par défaut pour destruction correcte des dérivés.
                virtual ~NkFileWatcherCallback() {}

                // -------------------------------------------------------------
                // Méthodes pures virtuelles
                // -------------------------------------------------------------

                /// Callback invoqué lors de la détection d'un changement.
                /// @param event Référence constante vers l'événement déclenché.
                /// @warning Appelée depuis un thread dédié : implémentation thread-safe requise.
                virtual void OnFileChanged(const NkFileChangeEvent& event) = 0;
        };

        // =====================================================================
        //  Classe : NkFileWatcher
        // =====================================================================
        // Classe principale de surveillance asynchrone des changements de fichiers.
        // Encapsule la logique multi-plateforme et notifie via un callback utilisateur.

        class NKENTSEU_FILESYSTEM_CLASS_EXPORT NkFileWatcher
        {

        private:

            // -------------------------------------------------------------
            // SOUS-SECTION 2.1 : Membres privés
            // -------------------------------------------------------------
            // État interne et ressources plateforme pour la surveillance.

            /// Handle natif pour l'API de surveillance (spécifique à la plateforme).
            void* mHandle;

            /// Chemin absolu du répertoire surveillé (format normalisé).
            NkString mPath;

            /// Pointeur vers le callback utilisateur pour la notification.
            NkFileWatcherCallback* mCallback;

            /// Indicateur d'état : true si la surveillance est active.
            bool mIsWatching;

            /// Indicateur de récursivité : inclure les sous-répertoires dans la surveillance.
            bool mRecursive;

            /// Handle du thread de surveillance (type opaque plateforme).
            void* mThread;

            // -------------------------------------------------------------
            // SOUS-SECTION 2.2 : Méthodes privées
            // -------------------------------------------------------------
            // Logique interne de surveillance et gestion du thread.

            /// Boucle principale du thread de surveillance.
            /// @details Implémentation spécifique à chaque plateforme (Windows/Linux).
            void WatchThread();

            /// Fonction d'entrée statique pour le thread plateforme.
            /// @param param Pointeur vers l'instance NkFileWatcher (cast requis).
            /// @return Toujours nullptr (convention thread POSIX/Windows).
            static void* ThreadProc(void* param);

        public:

            // -------------------------------------------------------------
            // SOUS-SECTION 2.3 : Constructeurs / Destructeur
            // -------------------------------------------------------------
            // Gestion du cycle de vie avec initialisation sécurisée.

            /// Constructeur par défaut : initialise tous les membres à des valeurs neutres.
            NkFileWatcher();

            /// Constructeur avec chemin C-string et callback.
            /// @param path Chemin du répertoire à surveiller.
            /// @param callback Pointeur vers l'objet callback (non-null requis pour Start()).
            /// @param recursive Surveillance récursive des sous-répertoires (défaut: false).
            NkFileWatcher(const char* path, NkFileWatcherCallback* callback, bool recursive = false);

            /// Constructeur avec objet NkPath et callback.
            /// @param path Objet NkPath représentant le répertoire à surveiller.
            /// @param callback Pointeur vers l'objet callback (non-null requis pour Start()).
            /// @param recursive Surveillance récursive des sous-répertoires (défaut: false).
            NkFileWatcher(const NkPath& path, NkFileWatcherCallback* callback, bool recursive = false);

            /// Destructeur : arrête automatiquement la surveillance si active.
            /// @details Libère les ressources plateforme et joint le thread si nécessaire.
            ~NkFileWatcher();

            // -------------------------------------------------------------
            // SOUS-SECTION 2.4 : Gestion de la copie
            // -------------------------------------------------------------
            // Copie interdite pour éviter les duplications de ressources threadées.

            /// Constructeur de copie supprimé.
            NkFileWatcher(const NkFileWatcher&) = delete;

            /// Opérateur d'affectation supprimé.
            NkFileWatcher& operator=(const NkFileWatcher&) = delete;

            // -------------------------------------------------------------
            // SOUS-SECTION 2.5 : Contrôle du cycle de vie
            // -------------------------------------------------------------
            // Méthodes pour démarrer, arrêter et interroger l'état de surveillance.

            /// Démarre la surveillance asynchrone en arrière-plan.
            /// @return true si le démarrage a réussi, false en cas d'erreur (chemin invalide, callback null, etc.).
            /// @pre Un chemin valide et un callback non-null doivent être configurés.
            bool Start();

            /// Arrête la surveillance et libère les ressources associées.
            /// @details Attend la fin du thread de surveillance avant de retourner (blocant).
            void Stop();

            /// Vérifie si la surveillance est actuellement active.
            /// @return true si Start() a été appelé sans Stop() consécutif.
            bool IsWatching() const;

            // -------------------------------------------------------------
            // SOUS-SECTION 2.6 : Configuration
            // -------------------------------------------------------------
            // Méthodes pour modifier les paramètres du watcher à runtime.

            /// Définit le chemin à surveiller (version C-string).
            /// @param path Nouveau chemin absolu ou relatif.
            /// @note Si la surveillance est active, elle est redémarrée avec le nouveau chemin.
            void SetPath(const char* path);

            /// Définit le chemin à surveiller (version NkPath).
            /// @param path Objet NkPath représentant le nouveau chemin.
            void SetPath(const NkPath& path);

            /// Définit le callback de notification.
            /// @param callback Pointeur vers l'objet implémentant NkFileWatcherCallback.
            void SetCallback(NkFileWatcherCallback* callback);

            /// Active ou désactive la surveillance récursive.
            /// @param recursive true pour inclure les sous-répertoires, false sinon.
            /// @note Si la surveillance est active, elle est redémarrée avec la nouvelle configuration.
            void SetRecursive(bool recursive);

            /// Accède au chemin actuellement surveillé.
            /// @return Référence constante vers la chaîne contenant le chemin normalisé.
            const NkString& GetPath() const;

            /// Vérifie si le mode récursif est activé.
            /// @return true si la surveillance inclut les sous-répertoires.
            bool IsRecursive() const;

        }; // class NkFileWatcher

        // -------------------------------------------------------------------------
        // SECTION 3 : SUPPORT C++11 - ADAPTATEUR FONCTIONNEL
        // -------------------------------------------------------------------------
        // Adaptateur pratique pour utiliser des callbacks de type fonction/lambda.

        #if defined(NK_CPP11)

            // =================================================================
            //  Classe : NkSimpleFileWatcher
            // =================================================================
            // Adaptateur permettant d'utiliser NkFileWatcher avec une simple fonction
            // ou lambda, sans créer de classe dérivée de NkFileWatcherCallback.

            class NKENTSEU_FILESYSTEM_CLASS_EXPORT NkSimpleFileWatcher : public NkFileWatcherCallback
            {
                private:

                    // ---------------------------------------------------------
                    // Membres privés
                    // ---------------------------------------------------------

                    /// Instance interne du watcher réel qui effectue la surveillance.
                    NkFileWatcher mWatcher;

                public:

                    // ---------------------------------------------------------
                    // Types et callbacks
                    // ---------------------------------------------------------

                    /// Type de fonction callback compatible C-style.
                    using CallbackFunc = void(*)(const NkFileChangeEvent&);

                    /// Pointeur vers la fonction callback utilisateur.
                    CallbackFunc OnChanged;

                    // ---------------------------------------------------------
                    // Constructeur
                    // ---------------------------------------------------------

                    /// Constructeur avec chemin et option de récursivité.
                    /// @param path Chemin du répertoire à surveiller.
                    /// @param recursive Surveillance récursive des sous-répertoires (défaut: false).
                    NkSimpleFileWatcher(const char* path, bool recursive = false);

                    // ---------------------------------------------------------
                    // Implémentation du callback interne
                    // ---------------------------------------------------------

                    /// Implémentation du callback interne forwardant vers OnChanged.
                    /// @param event Événement détecté à forwarder vers le callback utilisateur.
                    void OnFileChanged(const NkFileChangeEvent& event) override;

                    // ---------------------------------------------------------
                    // Délégation des méthodes de contrôle
                    // ---------------------------------------------------------

                    /// Démarre la surveillance (délègue à mWatcher.Start()).
                    /// @return Résultat de NkFileWatcher::Start().
                    bool Start();

                    /// Arrête la surveillance (délègue à mWatcher.Stop()).
                    void Stop();

                    /// Vérifie l'état de surveillance (délègue à mWatcher.IsWatching()).
                    /// @return Résultat de NkFileWatcher::IsWatching().
                    bool IsWatching() const;

            }; // class NkSimpleFileWatcher

        #endif // defined(NK_CPP11)

        // -------------------------------------------------------------------------
        // SECTION 4 : ALIAS DE COMPATIBILITÉ LEGACY
        // -------------------------------------------------------------------------
        // Fournit des alias pour compatibilité avec l'ancien namespace nkentseu::entseu.

        namespace entseu
        {
            /// Alias pour compatibilité avec l'ancien namespace.
            using NkFileChangeType = ::nkentseu::NkFileChangeType;

            /// Alias pour compatibilité avec l'ancien namespace.
            using NkFileChangeEvent = ::nkentseu::NkFileChangeEvent;

            /// Alias pour compatibilité avec l'ancien namespace.
            using NkFileWatcherCallback = ::nkentseu::NkFileWatcherCallback;

            /// Alias pour compatibilité avec l'ancien namespace.
            using NkFileWatcher = ::nkentseu::NkFileWatcher;

            #if defined(NK_CPP11)
                /// Alias pour compatibilité avec l'ancien namespace (C++11).
                using NkSimpleFileWatcher = ::nkentseu::NkSimpleFileWatcher;
            #endif

        } // namespace entseu

    } // namespace nkentseu

#endif // NKENTSEU_FILESYSTEM_NKFILEWATCHER_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKFILEWATCHER.H
// =============================================================================
// La classe NkFileWatcher permet de surveiller les changements de fichiers
// et répertoires de manière asynchrone et cross-platform, avec notification
// via callback pour un traitement personnalisé des événements.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Callback orienté objet (recommandé pour production)
// -----------------------------------------------------------------------------
/*
    #include "NKFileSystem/NkFileWatcher.h"
    #include "NKCore/Logger/NkLogger.h"

    class MonObservateur : public nkentseu::NkFileWatcherCallback
    {
        public:
            void OnFileChanged(const nkentseu::NkFileChangeEvent& event) override
            {
                using namespace nkentseu;

                switch (event.Type)
                {
                    case NkFileChangeType::NK_CREATED:
                        NK_LOG_INFO("Fichier créé : {}", event.Path.CStr());
                        // Logique de chargement de nouvelle ressource
                        break;

                    case NkFileChangeType::NK_DELETED:
                        NK_LOG_INFO("Fichier supprimé : {}", event.Path.CStr());
                        // Nettoyage des références internes
                        break;

                    case NkFileChangeType::NK_MODIFIED:
                        NK_LOG_INFO("Fichier modifié : {}", event.Path.CStr());
                        // Rechargement à chaud de la ressource
                        ReloadResource(event.Path.CStr());
                        break;

                    case NkFileChangeType::NK_RENAMED:
                        NK_LOG_INFO("Fichier renommé : {} -> {}",
                            event.OldPath.CStr(), event.Path.CStr());
                        // Mise à jour des mappings de chemins
                        break;

                    case NkFileChangeType::NK_ATTRIBUTE_CHANGED:
                        NK_LOG_INFO("Attributs modifiés : {}", event.Path.CStr());
                        // Vérification des permissions si nécessaire
                        break;
                }
            }

        private:
            void ReloadResource(const char* path)
            {
                // Implémentation du rechargement...
            }
    };

    // Utilisation dans le code principal
    void SetupFileWatching()
    {
        using namespace nkentseu;

        MonObservateur observateur;
        NkFileWatcher watcher("assets/textures", &observateur, true);

        if (watcher.Start())
        {
            NK_LOG_INFO("Surveillance active sur : {}", watcher.GetPath().CStr());
            // ... exécution principale de l'application ...
            watcher.Stop();
        }
        else
        {
            NK_LOG_ERROR("Échec du démarrage de la surveillance");
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Callback simplifié avec C++11 (lambda/fonction)
// -----------------------------------------------------------------------------
/*
    #include "NKFileSystem/NkFileWatcher.h"

    #if defined(NK_CPP11)

    void SetupSimpleWatching()
    {
        using namespace nkentseu;

        NkSimpleFileWatcher watcher("assets/config", true);

        // Assignation d'un lambda comme callback
        watcher.OnChanged = [](const NkFileChangeEvent& event)
        {
            if (event.Type == NkFileChangeType::NK_MODIFIED)
            {
                // Recharger la configuration automatiquement
                if (event.Path.EndsWith(".json"))
                {
                    ReloadConfigFile(event.Path.CStr());
                }
            }
        };

        if (watcher.Start())
        {
            // Application en cours d'exécution avec surveillance active
            // ...
            watcher.Stop();
        }
    }

    // Fonction de rechargement externe
    void ReloadConfigFile(const char* path)
    {
        // Implémentation du rechargement...
    }

    #endif // NK_CPP11
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Gestion thread-safe avec file de messages
// -----------------------------------------------------------------------------
/*
    #include "NKFileSystem/NkFileWatcher.h"
    #include "NKContainers/Sequential/NkVector.h"
    #include <mutex>  // Ou NKThreading/NkMutex.h selon l'architecture

    class ObservateurSecurise : public nkentseu::NkFileWatcherCallback
    {
        private:
            // File d'attente thread-safe pour les événements
            nkentseu::NkVector<nkentseu::NkFileChangeEvent> mEventQueue;
            std::mutex mQueueMutex;  // Ou nkentseu::NkMutex

        public:
            void OnFileChanged(const nkentseu::NkFileChangeEvent& event) override
            {
                // Verrouillage pour ajout thread-safe à la file
                std::lock_guard<std::mutex> lock(mQueueMutex);
                mEventQueue.PushBack(event);
            }

            void TraiterEvenementsEnAttente()
            {
                // Traitement dans le thread principal (ou thread dédié)
                std::lock_guard<std::mutex> lock(mQueueMutex);

                for (const auto& event : mEventQueue)
                {
                    // Traitement thread-safe des événements
                    ProcesserEvenement(event);
                }

                // Vidage de la file après traitement
                mEventQueue.Clear();
            }

        private:
            void ProcesserEvenement(const nkentseu::NkFileChangeEvent& event)
            {
                // Logique métier de traitement d'événement...
            }
    };

    // Utilisation avec boucle de traitement
    void MainLoopWithWatching()
    {
        using namespace nkentseu;

        ObservateurSecurise observateur;
        NkFileWatcher watcher("assets", &observateur, true);
        watcher.Start();

        while (ApplicationIsRunning())
        {
            // Traitement des événements en attente
            observateur.TraiterEvenementsEnAttente();

            // ... autre logique de boucle principale ...
            SleepFrame();
        }

        watcher.Stop();
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Surveillance multiple avec gestion centralisée
// -----------------------------------------------------------------------------
/*
    #include "NKFileSystem/NkFileWatcher.h"
    #include "NKContainers/Sequential/NkVector.h"

    class GestionnaireSurveillance
    {
        public:
            // Ajout d'un nouveau répertoire à surveiller
            bool AjouterSurveillance(const nkentseu::NkPath& path,
                                     nkentseu::NkFileWatcherCallback* callback,
                                     bool recursive = false)
            {
                using namespace nkentseu;

                // Création et démarrage du watcher
                auto watcher = new NkFileWatcher(path, callback, recursive);

                if (watcher->Start())
                {
                    mWatchers.PushBack(watcher);
                    return true;
                }

                delete watcher;
                return false;
            }

            // Arrêt de toutes les surveillances
            void ArreterToutes()
            {
                for (auto* watcher : mWatchers)
                {
                    if (watcher)
                    {
                        watcher->Stop();
                        delete watcher;
                    }
                }
                mWatchers.Clear();
            }

            // Destructeur avec nettoyage automatique
            ~GestionnaireSurveillance()
            {
                ArreterToutes();
            }

        private:
            nkentseu::NkVector<nkentseu::NkFileWatcher*> mWatchers;
    };
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Pattern Hot-Reload pour ressources de jeu/application
// -----------------------------------------------------------------------------
/*
    #include "NKFileSystem/NkFileWatcher.h"
    #include "NKCore/Logger/NkLogger.h"

    class HotReloadManager : public nkentseu::NkFileWatcherCallback
    {
        public:
            HotReloadManager(const nkentseu::NkPath& assetsPath)
            {
                using namespace nkentseu;

                mWatcher = new NkFileWatcher(assetsPath, this, true);

                if (!mWatcher->Start())
                {
                    NK_LOG_WARN("Hot-reload désactivé : impossible de surveiller '{}'",
                        assetsPath.CStr());
                }
            }

            void OnFileChanged(const nkentseu::NkFileChangeEvent& event) override
            {
                using namespace nkentseu;

                // Filtrage par extension pour optimisation
                if (!event.Path.EndsWith(".shader") &&
                    !event.Path.EndsWith(".material") &&
                    !event.Path.EndsWith(".config"))
                {
                    return;
                }

                switch (event.Type)
                {
                    case NkFileChangeType::NK_MODIFIED:
                        NK_LOG_INFO("Hot-reload : {}", event.Path.CStr());
                        ReloadAsset(event.Path.CStr());
                        break;

                    case NkFileChangeType::NK_DELETED:
                        NK_LOG_INFO("Ressource supprimée : {}", event.Path.CStr());
                        UnloadAsset(event.Path.CStr());
                        break;

                    case NkFileChangeType::NK_CREATED:
                        NK_LOG_INFO("Nouvelle ressource : {}", event.Path.CStr());
                        LoadNewAsset(event.Path.CStr());
                        break;

                    default:
                        break;
                }
            }

            ~HotReloadManager()
            {
                if (mWatcher)
                {
                    mWatcher->Stop();
                    delete mWatcher;
                }
            }

        private:
            void ReloadAsset(const char* path) { /* Implémentation... *\/ }
            void UnloadAsset(const char* path) { /* Implémentation... *\/ }
            void LoadNewAsset(const char* path) { /* Implémentation... *\/ }

            nkentseu::NkFileWatcher* mWatcher = nullptr;
    };
*/

// -----------------------------------------------------------------------------
// Notes d'utilisation et bonnes pratiques
// -----------------------------------------------------------------------------
/*
    Thread-safety :
    --------------
    - Le callback OnFileChanged() est invoqué depuis un thread dédié
    - Éviter les opérations bloquantes ou longues dans le callback
    - Pour accéder à des ressources partagées, utiliser une file de messages
      ou un mécanisme de synchronisation (mutex, semaphore, etc.)

    Gestion du cycle de vie :
    ------------------------
    - Toujours vérifier le retour de Start() avant de considérer la surveillance active
    - Appeler Stop() avant la destruction de l'objet pour libération propre des ressources
    - Le destructeur appelle automatiquement Stop() si la surveillance est active

    Performance :
    ------------
    - La surveillance récursive peut générer beaucoup d'événements sur de grands arbres
    - Filtrer les événements par extension ou pattern dans le callback pour optimiser
    - Sur Linux, inotify a une limite de watches par processus (fs.inotify.max_user_watches)

    Plateformes :
    ------------
    - Windows : ReadDirectoryChangesW avec buffer de 4Ko, événements batchés
    - Linux : inotify avec select() pour polling non-bloquant
    - Web/EMSCRIPTEN : fallback sans surveillance active (retourne true mais ne détecte rien)

    Débogage :
    ---------
    - Utiliser NK_LOG_* dans le callback pour tracer les événements en développement
    - Vérifier les permissions d'accès au répertoire surveillé
    - Sur Linux, vérifier que le filesystem supporte inotify (ex: pas sur NFS par défaut)
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================