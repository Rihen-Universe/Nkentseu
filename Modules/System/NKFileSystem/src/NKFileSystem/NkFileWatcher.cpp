// =============================================================================
// NKFileSystem/NkFileWatcher.cpp
// Implémentation de la classe NkFileWatcher.
//
// Design :
//  - Implémentation plateforme via guards conditionnels (_WIN32, __EMSCRIPTEN__)
//  - Thread de surveillance dédié avec boucle d'événements native
//  - Gestion propre des ressources : handles, threads, buffers
//  - Conversion encodage Windows (UTF-16) vers UTF-8 pour cohérence interne
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

// -------------------------------------------------------------------------
// SECTION 1 : INCLUSIONS (ordre strict requis)
// -------------------------------------------------------------------------
// 1. Precompiled header en premier (obligatoire pour la compilation MSVC/Clang)
// 2. Header correspondant au fichier .cpp
// 3. Headers du projet NKEntseu
// 4. Headers système conditionnels selon la plateforme

#include "pch.h"
#include "NKFileSystem/NkFileWatcher.h"

// En-têtes C standard pour les fonctions système et temporelles
#include <cstring>
#include <ctime>

// En-têtes plateforme pour les APIs de surveillance de fichiers
#ifdef _WIN32
    #include <windows.h>
    #include <process.h>
    // Undef des macros Windows qui pourraient entrer en conflit
    #ifdef CreateFile
        #undef CreateFile
    #endif
    #ifdef ReadDirectoryChangesW
        #undef ReadDirectoryChangesW
    #endif
#elif defined(__EMSCRIPTEN__)
    // WebAssembly : pas d'accès aux API natives de surveillance de fichiers
    // La surveillance est désactivée sur cette plateforme (fallback silencieux)
#else
    #include <unistd.h>
    #include <pthread.h>
    #include <sys/inotify.h>
    #include <sys/select.h>
    #include <errno.h>
#endif

// -------------------------------------------------------------------------
// SECTION 2 : NAMESPACE PRINCIPAL
// -------------------------------------------------------------------------
// Implémentation des méthodes dans le namespace nkentseu.

namespace nkentseu {

    // =============================================================================
    //  Implémentation : NkFileChangeEvent
    // =============================================================================
    // Constructeurs de la structure d'événement.

    NkFileChangeEvent::NkFileChangeEvent()
        : Type(NkFileChangeType::NK_MODIFIED)
        , Path()
        , OldPath()
        , Timestamp(0)
    {
        // Initialisation par défaut : événement de type MODIFIED avec valeurs neutres
        // Timestamp à 0 signifie "non initialisé" pour le consommateur
    }

    NkFileChangeEvent::NkFileChangeEvent(NkFileChangeType type, const char* path)
        : Type(type)
        , Path(path ? path : "")
        , OldPath()
        , Timestamp(static_cast<nk_int64>(time(nullptr)))
    {
        // Construction avec type et chemin fournis
        // Gestion défensive : si path est null, utiliser chaîne vide
        // Timestamp capturé au moment de la construction de l'événement
    }

    // =============================================================================
    //  Implémentation : Constructeurs / Destructeur de NkFileWatcher
    // =============================================================================
    // Initialisation sécurisée des membres avec valeurs par défaut.

    NkFileWatcher::NkFileWatcher()
        : mHandle(nullptr)
        , mPath()
        , mCallback(nullptr)
        , mIsWatching(false)
        , mRecursive(false)
        , mThread(nullptr)
    {
        // Constructeur par défaut : tous les membres à l'état neutre
        // Prêt pour configuration ultérieure via SetPath/SetCallback
    }

    NkFileWatcher::NkFileWatcher(const char* path, NkFileWatcherCallback* callback, bool recursive)
        : mHandle(nullptr)
        , mPath(path ? path : "")
        , mCallback(callback)
        , mIsWatching(false)
        , mRecursive(recursive)
        , mThread(nullptr)
    {
        // Constructeur paramétré avec gestion null défensive pour path
        // Surveillance non démarrée : appel explicite à Start() requis
    }

    NkFileWatcher::NkFileWatcher(const NkPath& path, NkFileWatcherCallback* callback, bool recursive)
        : mHandle(nullptr)
        , mPath(path.ToString())
        , mCallback(callback)
        , mIsWatching(false)
        , mRecursive(recursive)
        , mThread(nullptr)
    {
        // Constructeur avec NkPath : conversion explicite vers NkString interne
        // Maintient la cohérence du format de chemin normalisé
    }

    NkFileWatcher::~NkFileWatcher()
    {
        // Destructeur : garantie de libération des ressources
        // Stop() est idempotent : sûr d'appeler même si déjà arrêté
        Stop();
    }

    // =============================================================================
    //  Implémentation Windows : Thread de surveillance
    // =============================================================================
    #ifdef _WIN32

        void* NkFileWatcher::ThreadProc(void* param)
        {
            // Fonction d'entrée statique pour compatibilité avec _beginthread
            // Cast sécurisé vers l'instance et délégation à WatchThread()
            NkFileWatcher* watcher = static_cast<NkFileWatcher*>(param);
            watcher->WatchThread();
            return nullptr;
        }

        void NkFileWatcher::WatchThread()
        {
            // Ouverture du handle sur le répertoire à surveiller
            // FILE_FLAG_BACKUP_SEMANTICS requis pour ouvrir un répertoire
            // FILE_FLAG_OVERLAPPED pour opérations asynchrones
            HANDLE hDir = CreateFileA(
                mPath.CStr(),
                FILE_LIST_DIRECTORY,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                NULL,
                OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                NULL
            );

            // Gestion d'erreur : retour silencieux si ouverture échoue
            if (hDir == INVALID_HANDLE_VALUE)
            {
                return;
            }

            // Configuration du buffer pour recevoir les notifications
            // Taille de 4Ko suffisante pour la majorité des batches d'événements
            const DWORD BUFFER_SIZE = 4096;
            char buffer[BUFFER_SIZE];
            DWORD bytesReturned = 0;

            // Configuration de la structure OVERLAPPED pour I/O asynchrone
            OVERLAPPED overlapped = {};
            overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

            // Boucle principale de surveillance : tant que mIsWatching est true
            while (mIsWatching)
            {
                // Lancement de la lecture asynchrone des changements
                BOOL success = ReadDirectoryChangesW(
                    hDir,
                    buffer,
                    BUFFER_SIZE,
                    mRecursive ? TRUE : FALSE,
                    FILE_NOTIFY_CHANGE_FILE_NAME |
                    FILE_NOTIFY_CHANGE_DIR_NAME |
                    FILE_NOTIFY_CHANGE_ATTRIBUTES |
                    FILE_NOTIFY_CHANGE_SIZE |
                    FILE_NOTIFY_CHANGE_LAST_WRITE,
                    &bytesReturned,
                    &overlapped,
                    NULL
                );

                // Sortie silencieuse en cas d'échec de la requête
                if (!success)
                {
                    break;
                }

                // Attente du signal de complétion avec timeout de 1 seconde
                // Permet de vérifier régulièrement mIsWatching pour arrêt propre
                DWORD waitResult = WaitForSingleObject(overlapped.hEvent, 1000);
                if (waitResult == WAIT_OBJECT_0)
                {
                    // Récupération du résultat de l'opération asynchrone
                    GetOverlappedResult(hDir, &overlapped, &bytesReturned, FALSE);

                    // Traitement des événements si des données sont disponibles
                    if (bytesReturned > 0 && mCallback)
                    {
                        // Parcours de la liste chaînée d'événements FILE_NOTIFY_INFORMATION
                        FILE_NOTIFY_INFORMATION* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer);

                        while (true)
                        {
                            // Buffer temporaire pour conversion UTF-16 → UTF-8
                            char filename[MAX_PATH] = {};

                            // Conversion du nom de fichier depuis WCHAR* vers char*
                            WideCharToMultiByte(
                                CP_UTF8,
                                0,
                                info->FileName,
                                info->FileNameLength / static_cast<int>(sizeof(WCHAR)),
                                filename,
                                MAX_PATH,
                                NULL,
                                NULL
                            );

                            // Construction de l'événement à notifier
                            NkFileChangeEvent event;
                            event.Path = (NkPath(mPath.CStr()) / filename).ToString();
                            event.Timestamp = static_cast<nk_int64>(time(nullptr));

                            // Mapping des actions Windows vers NkFileChangeType
                            switch (info->Action)
                            {
                                case FILE_ACTION_ADDED:
                                    event.Type = NkFileChangeType::NK_CREATED;
                                    break;
                                case FILE_ACTION_REMOVED:
                                    event.Type = NkFileChangeType::NK_DELETED;
                                    break;
                                case FILE_ACTION_MODIFIED:
                                    event.Type = NkFileChangeType::NK_MODIFIED;
                                    break;
                                case FILE_ACTION_RENAMED_OLD_NAME:
                                case FILE_ACTION_RENAMED_NEW_NAME:
                                    event.Type = NkFileChangeType::NK_RENAMED;
                                    break;
                                default:
                                    event.Type = NkFileChangeType::NK_MODIFIED;
                                    break;
                            }

                            // Notification du callback utilisateur
                            mCallback->OnFileChanged(event);

                            // Avance vers le prochain événement dans la liste chaînée
                            if (info->NextEntryOffset == 0)
                            {
                                break;
                            }

                            info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                                reinterpret_cast<char*>(info) + info->NextEntryOffset
                            );
                        }
                    }

                    // Réinitialisation de l'événement pour la prochaine itération
                    ResetEvent(overlapped.hEvent);
                }
            }

            // Nettoyage des ressources Windows : événement et handle de répertoire
            CloseHandle(overlapped.hEvent);
            CloseHandle(hDir);
        }

    #elif !defined(__EMSCRIPTEN__)

        // =====================================================================
        //  Implémentation Linux/POSIX : Thread de surveillance avec inotify
        // =====================================================================

        void* NkFileWatcher::ThreadProc(void* param)
        {
            // Fonction d'entrée statique pour compatibilité avec pthread_create
            // Cast sécurisé vers l'instance et délégation à WatchThread()
            NkFileWatcher* watcher = static_cast<NkFileWatcher*>(param);
            watcher->WatchThread();
            return nullptr;
        }

        void NkFileWatcher::WatchThread()
        {
            // Initialisation du descripteur inotify
            int fd = inotify_init();
            if (fd < 0)
            {
                // Échec d'initialisation : retour silencieux
                return;
            }

            // Masque d'événements à surveiller : création, suppression, modification, etc.
            const uint32_t mask = IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO | IN_ATTRIB;

            // Ajout du watch sur le chemin configuré
            int wd = inotify_add_watch(fd, mPath.CStr(), mask);
            if (wd < 0)
            {
                // Nettoyage en cas d'échec d'ajout du watch
                close(fd);
                return;
            }

            // Configuration du buffer de réception des événements inotify
            // Taille calculée pour contenir plusieurs événements batchés
            const size_t eventSize = sizeof(struct inotify_event);
            const size_t bufferLen = 1024 * (eventSize + 16);
            char buffer[1024 * (sizeof(struct inotify_event) + 16)];

            // Boucle principale de surveillance avec polling via select()
            while (mIsWatching)
            {
                // Configuration de fd_set pour select()
                fd_set fds;
                FD_ZERO(&fds);
                FD_SET(fd, &fds);

                // Timeout de 1 seconde pour vérification périodique de mIsWatching
                struct timeval tv;
                tv.tv_sec = 1;
                tv.tv_usec = 0;

                // Appel bloquant avec timeout sur le descripteur inotify
                int ret = select(fd + 1, &fds, NULL, NULL, &tv);

                // Gestion des erreurs de select()
                if (ret < 0)
                {
                    // Interruption par signal : continuer la boucle
                    if (errno == EINTR)
                    {
                        continue;
                    }
                    // Autre erreur : sortie de la boucle
                    break;
                }

                // Timeout expiré : pas d'événement, continuer la boucle
                if (ret == 0)
                {
                    continue;
                }

                // Lecture des événements disponibles sur le descripteur
                ssize_t length = read(fd, buffer, bufferLen);
                if (length < 0)
                {
                    // Erreur de lecture : sortie de la boucle
                    break;
                }

                // Vérification de la présence d'un callback avant traitement
                if (!mCallback)
                {
                    continue;
                }

                // Parcours des événements dans le buffer
                size_t i = 0;
                while (i < static_cast<size_t>(length))
                {
                    // Accès à la structure inotify_event courante
                    struct inotify_event* event = reinterpret_cast<struct inotify_event*>(&buffer[i]);

                    // Traitement uniquement si un nom de fichier est présent
                    if (event->len > 0)
                    {
                        // Construction de l'événement à notifier
                        NkFileChangeEvent changeEvent;
                        changeEvent.Path = (NkPath(mPath.CStr()) / event->name).ToString();
                        changeEvent.Timestamp = static_cast<nk_int64>(time(nullptr));

                        // Mapping des masks inotify vers NkFileChangeType
                        if (event->mask & IN_CREATE)
                        {
                            changeEvent.Type = NkFileChangeType::NK_CREATED;
                        }
                        else if (event->mask & IN_DELETE)
                        {
                            changeEvent.Type = NkFileChangeType::NK_DELETED;
                        }
                        else if (event->mask & IN_MODIFY)
                        {
                            changeEvent.Type = NkFileChangeType::NK_MODIFIED;
                        }
                        else if (event->mask & (IN_MOVED_FROM | IN_MOVED_TO))
                        {
                            changeEvent.Type = NkFileChangeType::NK_RENAMED;
                        }
                        else if (event->mask & IN_ATTRIB)
                        {
                            changeEvent.Type = NkFileChangeType::NK_ATTRIBUTE_CHANGED;
                        }
                        else
                        {
                            // Fallback pour événements non catégorisés
                            changeEvent.Type = NkFileChangeType::NK_MODIFIED;
                        }

                        // Notification du callback utilisateur
                        mCallback->OnFileChanged(changeEvent);
                    }

                    // Avance vers le prochain événement dans le buffer
                    i += eventSize + event->len;
                }
            }

            // Nettoyage des ressources POSIX : retrait du watch et fermeture du fd
            inotify_rm_watch(fd, wd);
            close(fd);
        }

    #endif // !defined(__EMSCRIPTEN__) && !defined(_WIN32)

    // =============================================================================
    //  Implémentation : Méthodes de contrôle du cycle de vie
    // =============================================================================

    bool NkFileWatcher::Start()
    {
        // Guard : retour immédiat si déjà en cours de surveillance
        if (mIsWatching)
        {
            return true;
        }

        // Validation des prérequis : chemin non vide et callback valide
        if (mPath.Empty() || !mCallback)
        {
            return false;
        }

        // Mise à jour de l'état avant création du thread
        mIsWatching = true;

        // Création du thread plateforme-specific
        #ifdef _WIN32
            // Windows : utilisation de _beginthread pour gestion CRT correcte
            mThread = reinterpret_cast<void*>(_beginthread(
                reinterpret_cast<void(*)(void*)>(ThreadProc),
                0,
                this
            ));
        #elif defined(__EMSCRIPTEN__)
            // Web : pas de thread natif, retour de succès simulé
            mThread = nullptr;
            return true;
        #else
            // POSIX : création de thread avec pthread
            pthread_t* thread = new pthread_t;

            if (pthread_create(thread, NULL, ThreadProc, this) == 0)
            {
                // Succès : stockage du handle de thread
                mThread = thread;
            }
            else
            {
                // Échec : nettoyage et retour d'erreur
                delete thread;
                mIsWatching = false;
                return false;
            }
        #endif

        // Retour du statut de démarrage
        return true;
    }

    void NkFileWatcher::Stop()
    {
        // Guard : retour immédiat si pas en cours de surveillance
        if (!mIsWatching)
        {
            return;
        }

        // Mise à jour de l'état pour signaler l'arrêt au thread
        mIsWatching = false;

        // Guard : rien à faire si pas de thread actif
        if (!mThread)
        {
            return;
        }

        // Attente et nettoyage du thread plateforme-specific
        #ifdef _WIN32
            // Windows : attente de fin puis fermeture du handle
            WaitForSingleObject(mThread, INFINITE);
            CloseHandle(mThread);
        #elif !defined(__EMSCRIPTEN__)
            // POSIX : join du thread puis libération mémoire
            pthread_t* thread = static_cast<pthread_t*>(mThread);
            pthread_join(*thread, NULL);
            delete thread;
        #endif

        // Réinitialisation du handle de thread
        mThread = nullptr;
    }

    bool NkFileWatcher::IsWatching() const
    {
        // Accès en lecture seule à l'état de surveillance
        return mIsWatching;
    }

    // =============================================================================
    //  Implémentation : Méthodes de configuration
    // =============================================================================

    void NkFileWatcher::SetPath(const char* path)
    {
        // Sauvegarde de l'état courant pour restauration si nécessaire
        const bool wasWatching = mIsWatching;

        // Arrêt temporaire si surveillance active
        if (wasWatching)
        {
            Stop();
        }

        // Mise à jour du chemin avec gestion null défensive
        mPath = (path ? path : "");

        // Redémarrage si la surveillance était active
        if (wasWatching)
        {
            Start();
        }
    }

    void NkFileWatcher::SetPath(const NkPath& path)
    {
        // Délégation à la version C-string pour éviter la duplication de code
        SetPath(path.CStr());
    }

    void NkFileWatcher::SetCallback(NkFileWatcherCallback* callback)
    {
        // Assignation directe du pointeur de callback
        mCallback = callback;
    }

    void NkFileWatcher::SetRecursive(bool recursive)
    {
        // Sauvegarde de l'état courant pour restauration si nécessaire
        const bool wasWatching = mIsWatching;

        // Arrêt temporaire si surveillance active
        if (wasWatching)
        {
            Stop();
        }

        // Mise à jour du flag de récursivité
        mRecursive = recursive;

        // Redémarrage si la surveillance était active
        if (wasWatching)
        {
            Start();
        }
    }

    const NkString& NkFileWatcher::GetPath() const
    {
        // Accès en lecture seule au chemin interne
        return mPath;
    }

    bool NkFileWatcher::IsRecursive() const
    {
        // Accès en lecture seule au flag de récursivité
        return mRecursive;
    }

} // namespace nkentseu

// =============================================================================
// NOTES D'IMPLÉMENTATION
// =============================================================================
/*
    Gestion des threads :
    --------------------
    - Windows : _beginthread pour gestion correcte du CRT (vs CreateThread)
    - POSIX : pthread_create avec allocation dynamique du pthread_t
    - Le thread est joint dans Stop() : appel bloquant mais nécessaire pour cleanup

    Buffer et performance :
    ----------------------
    - Windows : buffer de 4Ko pour ReadDirectoryChangesW, suffisant pour la majorité des cas
    - Linux : buffer calculé dynamiquement pour contenir plusieurs événements inotify
    - Les événements sont batchés par l'OS : traitement en boucle dans WatchThread()

    Encodage et chemins :
    --------------------
    - Windows : ReadDirectoryChangesW retourne UTF-16, conversion vers UTF-8 via WideCharToMultiByte
    - Linux : chemins déjà en encodage système (généralement UTF-8), pas de conversion nécessaire
    - Interne : tous les chemins stockés en UTF-8 via NkString pour cohérence cross-platform

    Gestion d'erreurs :
    ------------------
    - Échecs d'API natives : retour silencieux depuis WatchThread(), Start() retourne false
    - Callback null : vérifié avant invocation, évite les crashes
    - Chemin invalide : vérifié dans Start(), pas de tentative de surveillance

    Fallback Web/EMSCRIPTEN :
    ------------------------
    - Pas d'API native de file watching dans les navigateurs
    - Start() retourne true mais WatchThread() n'est jamais appelé
    - Alternative : utiliser des WebSockets ou polling HTTP côté application

    Thread-safety du callback :
    --------------------------
    - OnFileChanged() est appelé depuis le thread de surveillance
    - L'implémentation utilisateur doit être thread-safe ou utiliser une file de messages
    - Éviter les allocations dynamiques ou opérations bloquantes dans le callback

    Extensions futures possibles :
    -----------------------------
    - Support des chemins UNC Windows (\\server\share)
    - Filtrage par pattern/glob au niveau du watcher pour réduire le bruit
    - Priorité d'événements pour traitement différencié (ex: .shader > .txt)
    - Statistiques de surveillance : nombre d'événements, latence moyenne, etc.
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================