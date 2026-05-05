//
// NkThread.cpp
// =============================================================================
// Description :
//   Implémentation des méthodes non-template et non-inline de NkThread.
//   Ce fichier contient les définitions séparées pour :
//   - Réduire les dépendances de compilation (inclusions système isolées)
//   - Minimiser la taille des objets compilés utilisant uniquement le header
//   - Permettre une compilation incrémentale optimisée
//
// Note architecturale :
//   Le constructeur template NkThread(Func&&) reste inline dans le header
//   car le compilateur doit accéder au code source pour l'instanciation.
//   Toutes les autres méthodes sont implémentées ici pour isolation.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

// =====================================================================
// Inclusion de l'en-tête de déclaration
// =====================================================================
#include "NKThreading/NkThread.h"

// =====================================================================
// Namespace : nkentseu::threading
// =====================================================================
namespace nkentseu {
    namespace threading {

        // =================================================================
        // IMPLÉMENTATION : NkThread (méthodes non-inline)
        // =================================================================

        // -----------------------------------------------------------------
        // Constructeur par défaut : initialisation des membres natifs
        // -----------------------------------------------------------------
        // Windows : mHandle = nullptr, mThreadId = 0 → thread non-initialisé
        // POSIX   : mThread non-initialisé (valeur indéfinie), mJoinable = false
        // -----------------------------------------------------------------
        NkThread::NkThread() noexcept
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                : mHandle(nullptr)
                , mThreadId(0)
            #else
                : mThread()
                , mJoinable(false)
            #endif
        {
            // Initialisation via liste d'initialisation :
            // - Windows : handles nuls pour état non-initialisé
            // - POSIX   : pthread_t default-constructed, flag joinable à false
        }

        // -----------------------------------------------------------------
        // Destructeur : détachement automatique si thread joinable
        // -----------------------------------------------------------------
        // Comportement RAII :
        //   - Si le thread est encore joinable (ni Join() ni Detach() appelé),
        //     alors Detach() est appelé pour éviter les fuites de ressources.
        //   - Si le thread est déjà joint ou détaché, rien n'est fait.
        //
        // Garantie noexcept :
        //   - Detach() est noexcept : aucune exception ne peut être propagée
        //   - Safe dans les destructeurs : pas de risque de termination
        // -----------------------------------------------------------------
        NkThread::~NkThread()
        {
            if (Joinable()) {
                Detach();
            }
        }

        // -----------------------------------------------------------------
        // Opérateur de déplacement : transfert de propriété avec cleanup
        // -----------------------------------------------------------------
        // Algorithme :
        //   1. Vérifier l'auto-affectation (this != &other)
        //   2. Si ce thread est joinable : Detach() pour libérer les ressources
        //   3. Transférer les handles natifs via MoveFrom()
        //   4. Retourner *this pour chaînage d'affectation
        //
        // Sécurité :
        //   - Le Detach() préalable évite les fuites si l'instance cible
        //     détenait déjà un thread actif
        //   - MoveFrom() invalide l'instance source pour éviter double-libération
        // -----------------------------------------------------------------
        // NkThread& NkThread::operator=(NkThread&& other) noexcept
        // {
        //     if (this != &other) {
        //         if (Joinable()) {
        //             Detach();
        //         }
        //         MoveFrom(other);
        //     }
        //     return *this;
        // }

        // -----------------------------------------------------------------
        // Méthode : Join (attente bloquante de fin d'exécution)
        // -----------------------------------------------------------------
        // Comportement plateforme :
        //   - Windows : WaitForSingleObject() bloque jusqu'à terminaison,
        //               puis CloseHandle() libère le handle kernel.
        //   - POSIX   : pthread_join() bloque et récupère le code de retour
        //               (ignoré ici car void), puis marque non-joinable.
        //
        // Postconditions :
        //   - Après Join() réussi : Joinable() retourne false
        //   - Les ressources système (handle/pthread) sont libérées
        //   - Le thread OS a terminé son exécution
        //
        // Préconditions (non vérifiées en release) :
        //   - Le thread doit être joinable
        //   - Ne pas appeler depuis le thread lui-même (deadlock)
        // -----------------------------------------------------------------
        void NkThread::Join() noexcept
        {
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                if (mHandle != nullptr) {
                    (void)WaitForSingleObject(mHandle, INFINITE);
                    (void)CloseHandle(mHandle);
                    mHandle = nullptr;
                    mThreadId = 0;
                }
            #else
                if (mJoinable) {
                    (void)pthread_join(mThread, nullptr);
                    mJoinable = false;
                }
            #endif
        }

        // -----------------------------------------------------------------
        // Méthode : Detach (exécution indépendante)
        // -----------------------------------------------------------------
        // Comportement plateforme :
        //   - Windows : CloseHandle() libère le handle kernel sans attendre ;
        //               le thread OS continue jusqu'à completion naturelle.
        //   - POSIX   : pthread_detach() marque le thread comme détaché ;
        //               les ressources sont libérées automatiquement à la fin.
        //
        // Postconditions :
        //   - Après Detach() : Joinable() retourne false
        //   - Join() n'est plus possible sur ce thread
        //   - Le thread OS continue son exécution indépendamment
        //
        // Avertissements :
        //   - Ne pas accéder aux données locales du thread créateur
        //     après Detach() : elles peuvent être détruites avant la fin
        //   - Utiliser des données thread-safe ou allouées dynamiquement
        //     pour communiquer avec un thread détaché
        // -----------------------------------------------------------------
        void NkThread::Detach() noexcept
        {
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                if (mHandle != nullptr) {
                    (void)CloseHandle(mHandle);
                    mHandle = nullptr;
                    mThreadId = 0;
                }
            #else
                if (mJoinable) {
                    (void)pthread_detach(mThread);
                    mJoinable = false;
                }
            #endif
        }

        // -----------------------------------------------------------------
        // Méthode : Joinable (requête d'état)
        // -----------------------------------------------------------------
        // Retourne l'état de joinabilité du thread.
        //
        // Thread-safety :
        //   - Lecture simple d'un flag/handle : safe sans mutex
        //   - Mais la valeur peut changer concurrentiellement si un autre
        //     thread appelle Join()/Detach() simultanément
        //   - Utiliser uniquement pour le debugging ou la logique de contrôle,
        //     pas pour la synchronisation critique
        // -----------------------------------------------------------------
        nk_bool NkThread::Joinable() const noexcept
        {
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                return mHandle != nullptr;
            #else
                return mJoinable;
            #endif
        }

        // -----------------------------------------------------------------
        // Méthode : GetID (identifiant du thread)
        // -----------------------------------------------------------------
        // Extrait un identifiant portable depuis le handle natif.
        //
        // Gestion plateforme :
        //   - Windows : retourne directement mThreadId (DWORD → nk_uint64)
        //   - POSIX   : copie les bytes de pthread_t dans ThreadId
        //               (gestion de la taille variable de pthread_t)
        //
        // Retour :
        //   - ThreadId opaque : à utiliser pour logging/comparaison uniquement
        //   - 0 si le thread n'est pas joinable (non-initialisé ou détaché)
        // -----------------------------------------------------------------
        NkThread::ThreadId NkThread::GetID() const noexcept
        {
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                return static_cast<ThreadId>(mThreadId);
            #else
                if (!mJoinable) {
                    return 0;
                }
                ThreadId id = 0;
                const nk_size copySize =
                    (sizeof(pthread_t) < sizeof(ThreadId))
                        ? sizeof(pthread_t)
                        : sizeof(ThreadId);
                memcpy(&id, &mThread, static_cast<size_t>(copySize));
                return id;
            #endif
        }

        // -----------------------------------------------------------------
        // Méthode statique : GetCurrentThreadId
        // -----------------------------------------------------------------
        // Récupère l'identifiant du thread qui exécute cet appel.
        //
        // Implémentation plateforme :
        //   - Windows : ::GetCurrentThreadId() → DWORD natif
        //   - Linux   : ::syscall(SYS_gettid) → tid kernel précis
        //   - Autres POSIX : pthread_self() + copie byte-à-byte
        //
        // Usage typique :
        //   - Logging : inclure l'ID thread dans les messages de debug
        //   - Auto-détection : comparer avec GetID() pour savoir si on
        //     s'exécute dans le thread cible
        //   - Profiling : corréler les métriques par thread
        // -----------------------------------------------------------------
        NkThread::ThreadId NkThread::GetCurrentThreadId() noexcept
        {
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                return static_cast<ThreadId>(::GetCurrentThreadId());
            #elif defined(NKENTSEU_PLATFORM_LINUX)
                return static_cast<ThreadId>(::syscall(SYS_gettid));
            #else
                ThreadId id = 0;
                pthread_t thread = pthread_self();
                const nk_size copySize =
                    (sizeof(pthread_t) < sizeof(ThreadId))
                        ? sizeof(pthread_t)
                        : sizeof(ThreadId);
                memcpy(&id, &thread, static_cast<size_t>(copySize));
                return id;
            #endif
        }

        // -----------------------------------------------------------------
        // Méthode : SetName (nommage du thread)
        // -----------------------------------------------------------------
        // Définit un nom lisible pour le thread dans les outils de debugging.
        //
        // Implémentation plateforme :
        //   - Windows :
        //       • Charge dynamiquement SetThreadDescription() de Kernel32.dll
        //       • Convertit UTF-8 → UTF-16 via MultiByteToWideChar()
        //       • Appelle SetThreadDescription() si disponible (Windows 10+)
        //       • Échec silencieux si API non disponible (graceful degradation)
        //
        //   - Linux :
        //       • Tronque le nom à 15 caractères + null (limite pthread)
        //       • Appelle pthread_setname_np() sur le thread cible
        //       • Visible dans /proc/[pid]/task/[tid]/comm
        //
        //   - macOS :
        //       • pthread_setname_np() ne fonctionne que sur le thread courant
        //       • Vérifie pthread_equal(mThread, pthread_self()) avant d'appeler
        //       • Limitation de l'API macOS : pas de nommage à distance
        //
        // Gestion d'erreur :
        //   - Retour silencieux si name est null/vide
        //   - Retour silencieux si le thread n'est pas joinable
        //   - Retour silencieux si l'API plateforme n'est pas disponible
        // -----------------------------------------------------------------
        void NkThread::SetName(const nk_char* name) noexcept
        {
            if (name == nullptr || name[0] == '\0') {
                return;
            }

            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                if (mHandle == nullptr) {
                    return;
                }

                using SetThreadDescriptionFn = HRESULT (WINAPI*)(HANDLE, PCWSTR);
                HMODULE kernel32 = GetModuleHandleW(L"Kernel32.dll");
                if (kernel32 == nullptr) {
                    return;
                }

                SetThreadDescriptionFn setThreadDescription =
                    reinterpret_cast<SetThreadDescriptionFn>(
                        GetProcAddress(kernel32, "SetThreadDescription"));
                if (setThreadDescription == nullptr) {
                    return;
                }

                wchar_t wideName[64] = {};
                const int charsWritten = MultiByteToWideChar(
                    CP_UTF8,
                    0,
                    name,
                    -1,
                    wideName,
                    64);
                if (charsWritten > 0) {
                    (void)setThreadDescription(mHandle, wideName);
                }

            #elif defined(NKENTSEU_PLATFORM_LINUX)
                if (!mJoinable) {
                    return;
                }

                char shortName[16] = {};
                strncpy(shortName, name, sizeof(shortName) - 1);
                shortName[sizeof(shortName) - 1] = '\0';
                (void)pthread_setname_np(mThread, shortName);

            #elif defined(NKENTSEU_PLATFORM_MACOS)
                if (mJoinable && pthread_equal(mThread, pthread_self())) {
                    (void)pthread_setname_np(name);
                }
            #else
                (void)name;
            #endif
        }

        // -----------------------------------------------------------------
        // Méthode : SetAffinity (pinning CPU)
        // -----------------------------------------------------------------
        // Configure l'affinité CPU pour pinner le thread sur un core spécifique.
        //
        // Implémentation plateforme :
        //   - Windows :
        //       • Calcule un bitmask avec un seul bit positionné (1 << cpuCore)
        //       • Vérifie que cpuCore < nombre de bits dans DWORD_PTR
        //       • Appelle SetThreadAffinityMask() pour appliquer le mask
        //
        //   - Linux :
        //       • Vérifie que cpuCore < CPU_SETSIZE (typiquement 1024)
        //       • Initialise un cpu_set_t avec CPU_ZERO() + CPU_SET()
        //       • Appelle pthread_setaffinity_np() avec le set configuré
        //
        // Considérations performance :
        //   • Avantage : réduit les migrations de cache entre cores
        //   • Risque : peut créer du load imbalance si mal configuré
        //   • Recommandation : utiliser pour workloads compute-intensive
        //     avec localité de données, pas pour I/O-bound threads
        //
        // Gestion d'erreur :
        //   • Retour silencieux si cpuCore hors limites
        //   • Retour silencieux si le thread n'est pas joinable
        //   • Retour silencieux si l'API plateforme échoue
        // -----------------------------------------------------------------
        void NkThread::SetAffinity(nk_uint32 cpuCore) noexcept
        {
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                if (mHandle == nullptr) {
                    return;
                }
                const nk_uint32 maxBits = static_cast<nk_uint32>(sizeof(DWORD_PTR) * 8U);
                if (cpuCore >= maxBits) {
                    return;
                }
                const DWORD_PTR mask = (static_cast<DWORD_PTR>(1) << cpuCore);
                (void)SetThreadAffinityMask(mHandle, mask);

            #elif defined(NKENTSEU_PLATFORM_LINUX)
                if (!mJoinable || cpuCore >= static_cast<nk_uint32>(CPU_SETSIZE)) {
                    return;
                }
                cpu_set_t set;
                CPU_ZERO(&set);
                CPU_SET(static_cast<int>(cpuCore), &set);
                (void)pthread_setaffinity_np(mThread, sizeof(set), &set);
            #else
                (void)cpuCore;
            #endif
        }

        // -----------------------------------------------------------------
        // Méthode : SetPriority (priorité de scheduling)
        // -----------------------------------------------------------------
        // Ajuste la priorité de scheduling du thread dans la plage [-2, +2].
        //
        // Mapping des priorités :
        //   Valeur NK  | Windows                 | POSIX (interpolé)
        //   -----------|-------------------------|---------------------------
        //   -2         | THREAD_PRIORITY_LOWEST  | minPriority
        //   -1         | THREAD_PRIORITY_BELOW_NORMAL | min + 25% du range
        //    0         | THREAD_PRIORITY_NORMAL  | min + 50% du range (médian)
        //   +1         | THREAD_PRIORITY_ABOVE_NORMAL | min + 75% du range
        //   +2         | THREAD_PRIORITY_HIGHEST | maxPriority
        //
        // Implémentation POSIX :
        //   1. Récupère la politique de scheduling courante via pthread_getschedparam()
        //   2. Obtient min/max priority pour cette politique via sched_get_priority_min/max()
        //   3. Clamp la valeur NK dans [-2, +2] pour sécurité
        //   4. Interpole linéairement : mapped = min + ((clamped + 2) * span) / 4
        //   5. Applique via pthread_setschedparam()
        //
        // Avertissements :
        //   • Priorités élevées peuvent affamer d'autres threads : utiliser avec précaution
        //   • Sur Linux, les priorités > 0 peuvent nécessiter CAP_SYS_NICE ou root
        //   • Les priorités temps-réel (SCHED_FIFO/RR) peuvent bloquer le système si mal utilisées
        // -----------------------------------------------------------------
        void NkThread::SetPriority(nk_int32 priority) noexcept
        {
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                if (mHandle == nullptr) {
                    return;
                }

                int nativePriority = THREAD_PRIORITY_NORMAL;
                if (priority <= -2) {
                    nativePriority = THREAD_PRIORITY_LOWEST;
                } else if (priority == -1) {
                    nativePriority = THREAD_PRIORITY_BELOW_NORMAL;
                } else if (priority == 1) {
                    nativePriority = THREAD_PRIORITY_ABOVE_NORMAL;
                } else if (priority >= 2) {
                    nativePriority = THREAD_PRIORITY_HIGHEST;
                }
                (void)SetThreadPriority(mHandle, nativePriority);

            #elif defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_MACOS)
                if (!mJoinable) {
                    return;
                }

                int policy = 0;
                sched_param param{};
                if (pthread_getschedparam(mThread, &policy, &param) != 0) {
                    return;
                }

                const int minPriority = sched_get_priority_min(policy);
                const int maxPriority = sched_get_priority_max(policy);
                if (minPriority < 0 || maxPriority < 0) {
                    return;
                }

                nk_int32 clamped = priority;
                if (clamped < -2) {
                    clamped = -2;
                } else if (clamped > 2) {
                    clamped = 2;
                }

                const int span = maxPriority - minPriority;
                const int mapped = minPriority + ((clamped + 2) * span) / 4;
                param.sched_priority = mapped;
                (void)pthread_setschedparam(mThread, policy, &param);
            #else
                (void)priority;
            #endif
        }

        // -----------------------------------------------------------------
        // Méthode : GetCurrentCore (requête de core d'exécution)
        // -----------------------------------------------------------------
        // Retourne l'index du core CPU sur lequel le thread courant s'exécute.
        //
        // Implémentation plateforme :
        //   - Windows : GetCurrentProcessorNumber() → index 0-based
        //   - Linux   : sched_getcpu() → index 0-based, -1 si erreur
        //   - Autres  : fallback à 0 (non-supporté ou erreur)
        //
        // Usage :
        //   • Profiling : corréler les métriques de performance par core
        //   • Debugging : identifier sur quel core un thread s'exécute
        //   • Optimisation : ajuster dynamiquement l'affinité en fonction
        //     de la charge observée
        //
        // Note importante :
        //   • Retourne le core du thread APPELANT, pas nécessairement
        //     celui du thread représenté par l'instance NkThread
        //   • Pour obtenir le core d'un thread distant, il faudrait
        //     une IPC ou un mécanisme de requête via le thread cible
        // -----------------------------------------------------------------
        nk_uint32 NkThread::GetCurrentCore() const noexcept
        {
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                return static_cast<nk_uint32>(GetCurrentProcessorNumber());
            #elif defined(NKENTSEU_PLATFORM_LINUX)
                const int cpu = sched_getcpu();
                return (cpu >= 0) ? static_cast<nk_uint32>(cpu) : 0;
            #else
                return 0;
            #endif
        }

        // =================================================================
        // IMPLÉMENTATION : Méthodes privées internes
        // =================================================================

        // -----------------------------------------------------------------
        // Constructeur interne : ThreadStartData
        // -----------------------------------------------------------------
        NkThread::ThreadStartData::ThreadStartData(ThreadFunc&& function) noexcept
            : Function(traits::NkMove(function))
        {
            // Déplacement de la fonction dans le contexte de démarrage
            // La fonction est transférée par move pour éviter les copies
            // inutiles, surtout pour les lambdas avec captures coûteuses
        }

        // -----------------------------------------------------------------
        // Méthode privée : Start (création du thread OS)
        // -----------------------------------------------------------------
        // Algorithme :
        //   1. Si un thread est déjà joinable : Detach() pour cleanup
        //   2. Allouer ThreadStartData sur le heap avec la fonction à exécuter
        //   3. Créer le thread OS avec ThreadEntry comme point d'entrée
        //   4. En cas d'échec : libérer startData et marquer non-joinable
        //   5. En cas de succès : marquer joinable pour gestion future
        //
        // Transfert de contexte :
        //   • ThreadStartData est allouée sur le heap car la pile du thread
        //     créateur n'est pas accessible depuis le nouveau thread
        //   • ThreadEntry() est responsable de libérer startData après
        //     avoir transféré la fonction dans une variable locale
        //
        // Gestion d'erreur :
        //   • Windows : CreateThread() retourne nullptr en cas d'échec
        //   • POSIX   : pthread_create() retourne un code d'erreur != 0
        //   • Dans les deux cas : cleanup de startData pour éviter les fuites
        // -----------------------------------------------------------------
        void NkThread::Start(ThreadFunc&& function) noexcept
        {
            if (Joinable()) {
                Detach();
            }

            ThreadStartData* startData = new ThreadStartData(traits::NkMove(function));

            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                mHandle = CreateThread(
                    nullptr,                    // Security attributes (default)
                    0,                          // Stack size (default)
                    &NkThread::ThreadEntry,     // Entry point function
                    startData,                  // Parameter passed to entry point
                    0,                          // Creation flags (run immediately)
                    &mThreadId);                // Receives thread ID

                if (mHandle == nullptr) {
                    delete startData;
                    mThreadId = 0;
                }
            #else
                const int result = pthread_create(
                    &mThread,                   // Receives pthread_t handle
                    nullptr,                    // Thread attributes (default)
                    &NkThread::ThreadEntry,     // Entry point function
                    startData);                 // Parameter passed to entry point

                if (result != 0) {
                    delete startData;
                    mJoinable = false;
                } else {
                    mJoinable = true;
                }
            #endif
        }

        // -----------------------------------------------------------------
        // Méthode privée : MoveFrom (transfert de handles natifs)
        // -----------------------------------------------------------------
        // Transfère les membres natifs depuis 'other' vers 'this'.
        //
        // Comportement plateforme :
        //   - Windows : copie mHandle et mThreadId, puis invalide other
        //   - POSIX   : copie mThread et mJoinable, puis invalide other
        //
        // Invariant après transfert :
        //   - 'this' détient maintenant le thread OS
        //   - 'other' est dans un état non-initialisé (Joinable() == false)
        //   - Aucune double-libération possible car other est invalidé
        // -----------------------------------------------------------------
        void NkThread::MoveFrom(NkThread& other) noexcept
        {
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                mHandle = other.mHandle;
                mThreadId = other.mThreadId;

                other.mHandle = nullptr;
                other.mThreadId = 0;
            #else
                mThread = other.mThread;
                mJoinable = other.mJoinable;

                other.mJoinable = false;
            #endif
        }

        // -----------------------------------------------------------------
        // Méthode statique : ThreadEntry (point d'entrée du thread OS)
        // -----------------------------------------------------------------
        // Point d'entrée commun pour tous les threads créés via NkThread.
        //
        // Algorithme :
        //   1. Cast userData vers ThreadStartData*
        //   2. Vérifier nullptr (sécurité en cas d'erreur de création)
        //   3. Déplacer la fonction dans une variable locale
        //   4. Libérer startData (allouée sur le heap dans Start())
        //   5. Exécuter la fonction si non-null
        //   6. Retourner 0/nullptr selon la convention de la plateforme
        //
        // Gestion mémoire :
        //   • startData est libérée AVANT d'exécuter la fonction pour
        //     éviter les fuites même si la fonction lance une exception
        //   • La fonction est déplacée (pas copiée) pour éviter les copies
        //     inutiles de lambdas avec captures coûteuses
        //
        // Exception safety :
        //   • Si la fonction lancée lance une exception non catchée :
        //     - C++11+ : std::terminate() est appelé (comportement standard)
        //     - Le thread se termine, mais l'application peut continuer
        //   • Pour gérer les exceptions : wrapper la fonction dans un try/catch
        //     au niveau de l'appelant ou dans la fonction elle-même
        // -----------------------------------------------------------------
        #if defined(NKENTSEU_PLATFORM_WINDOWS)
            DWORD WINAPI NkThread::ThreadEntry(LPVOID userData)
        #else
            void* NkThread::ThreadEntry(void* userData)
        #endif
        {
            ThreadStartData* startData = static_cast<ThreadStartData*>(userData);
            if (startData == nullptr) {
                #if defined(NKENTSEU_PLATFORM_WINDOWS)
                    return 0;
                #else
                    return nullptr;
                #endif
            }

            // Transférer la fonction dans une variable locale avant de libérer
            ThreadFunc function = traits::NkMove(startData->Function);
            delete startData;  // Libération du contexte de démarrage

            // Exécuter la fonction si valide
            if (function) {
                function(nullptr);
            }

            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                return 0;
            #else
                return nullptr;
            #endif
        }

    }
}