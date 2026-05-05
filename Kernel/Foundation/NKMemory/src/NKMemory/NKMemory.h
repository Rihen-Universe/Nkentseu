// =============================================================================
// NKMemory/NkMemory.h
// Point d'entrée unifié du module NKMemory : allocation, tracking, GC, profiling.
//
// Design :
//  - Séparation claire déclarations (.h) / implémentations (.cpp)
//  - Singleton thread-safe via Meyer's pattern (C++11+)
//  - Réutilisation maximale des sous-modules NKMemory (ZÉRO duplication)
//  - API macro-friendly pour intégration transparente dans le code legacy
//  - Debug metadata : file/line/function/tag pour chaque allocation
//  - Gestion centralisée des garbage collectors multiples
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_MEMORY_NKMEMORY_H
#define NKENTSEU_MEMORY_NKMEMORY_H

    // -------------------------------------------------------------------------
    // SECTION 1 : DÉPENDANCES
    // -------------------------------------------------------------------------
    #include "NKMemory/NkMemoryApi.h"      // NKENTSEU_MEMORY_API
    #include "NKMemory/NkAllocator.h"      // NkAllocator, NK_MEMORY_DEFAULT_ALIGNMENT
    #include "NKMemory/NkTracker.h"        // NkMemoryTracker pour leak detection
    #include "NKMemory/NkProfiler.h"       // NkMemoryProfiler pour runtime hooks
    #include "NKMemory/NkGc.h"             // NkGarbageCollector pour mark-and-sweep
    #include "NKMemory/NkHash.h"           // NkPointerHashMap pour index O(1)
    #include "NKCore/NkTypes.h"            // nk_size, nk_uint64, nk_char, etc.
    #include "NKCore/NkAtomic.h"           // NkSpinLock pour synchronisation
    #include "NKCore/NkTraits.h"           // traits::NkForward pour perfect forwarding

    // -------------------------------------------------------------------------
    // SECTION 2 : CONFIGURATION ET STRUCTURES DE DONNÉES
    // -------------------------------------------------------------------------
    /**
     * @defgroup MemorySystem Système Mémoire Unifié NKMemory
     * @brief Point d'entrée centralisé pour toutes les opérations mémoire
     * 
     * Fonctionnalités :
     *  - Singleton thread-safe : NkMemorySystem::Instance()
     *  - Allocation avec metadata : file/line/function/tag pour debugging
     *  - Tracking automatique : intégration avec NkMemoryTracker pour leak detection
     *  - Profiling runtime : hooks via NkMemoryProfiler pour monitoring
     *  - Garbage collection : gestion centralisée de multiples NkGarbageCollector
     *  - API macro-friendly : NK_MEM_ALLOC, NK_MEM_NEW, etc. pour intégration facile
     * 
     * @note Initialisation lazy : Initialize() appelée automatiquement si nécessaire
     * @note Thread-safe : toutes les méthodes publiques acquièrent un spinlock interne
     * @note Shutdown() doit être appelé avant la fin de l'application pour rapport de fuites
     * 
     * @example
     * @code
     * // Initialisation (optionnelle, lazy par défaut)
     * nkentseu::memory::NkMemorySystem::Instance().Initialize();
     * 
     * // Allocation avec tracking
     * void* buffer = NK_MEM_ALLOC(1024);
     * if (buffer) {
     *     // ... utilisation ...
     *     NK_MEM_FREE(buffer);
     * }
     * 
     * // Création d'objets C++ avec metadata
     * auto* obj = NK_MEM_NEW(MyClass, arg1, arg2);
     * if (obj) {
     *     obj->DoWork();
     *     NK_MEM_DELETE(obj);
     * }
     * 
     * // Rapport de fuites à la fermeture
     * nkentseu::memory::NkMemorySystem::Instance().Shutdown(true);
     * @endcode
     */

    namespace nkentseu {
        namespace memory {

            // -------------------------------------------------------------------------
            // SECTION 3 : STRUCTURES DE STATISTIQUES ET PROFILAGE
            // -------------------------------------------------------------------------
            /**
             * @struct NkMemoryStats
             * @brief Snapshot des statistiques mémoire globales
             * @ingroup MemorySystem
             * 
             * Toutes les valeurs sont en bytes sauf indication contraire.
             * Les stats sont mises à jour atomiquement pour thread-safety.
             */
            struct NkMemoryStats {
                nk_size liveBytes;          ///< Octets actuellement alloués
                nk_size peakBytes;          ///< Pic historique d'octets alloués
                nk_size liveAllocations;    ///< Nombre d'allocations actuellement actives
                nk_size totalAllocations;   ///< Nombre total d'allocations depuis le démarrage
                
                /** @brief Constructeur par défaut : zero-initialisation */
                constexpr NkMemoryStats() noexcept 
                    : liveBytes(0), peakBytes(0), liveAllocations(0), totalAllocations(0) {}
            };

            /**
             * @struct NkGcProfile
             * @brief Profil d'un garbage collector managé par le système
             * @ingroup MemorySystem
             * 
             * Utilisé pour interroger l'état d'un GC via GetGcProfile().
             */
            struct NkGcProfile {
                nk_uint64 Id;               ///< Identifiant unique du GC
                nk_char Name[64];           ///< Nom lisible pour debugging
                nk_size ObjectCount;        ///< Nombre d'objets actuellement managés
                NkAllocator* Allocator;     ///< Allocateur utilisé par ce GC
                nk_bool IsDefault;          ///< true si c'est le GC par défaut
                
                /** @brief Constructeur par défaut : zero-initialisation */
                constexpr NkGcProfile() noexcept 
                    : Id(0), Name{0}, ObjectCount(0), Allocator(nullptr), IsDefault(false) {}
            };

            // -------------------------------------------------------------------------
            // SECTION 4 : CLASSE PRINCIPALE NkMemorySystem (Singleton)
            // -------------------------------------------------------------------------
            /**
             * @class NkMemorySystem
             * @brief Système mémoire centralisé : allocation, tracking, GC, profiling
             * @ingroup MemorySystem
             * 
             * Caractéristiques :
             *  - Singleton thread-safe : Instance() via Meyer's pattern (C++11+)
             *  - Allocation avec metadata : file/line/function/tag pour chaque allocation
             *  - Tracking automatique : intégration avec NkMemoryTracker pour leak detection
             *  - Profiling runtime : hooks configurables via NkMemoryProfiler
             *  - Gestion GC : création/destruction/nommage de multiples NkGarbageCollector
             *  - API macro-friendly : NK_MEM_ALLOC, NK_MEM_NEW, etc. pour intégration legacy
             *  - Thread-safe : toutes les opérations publiques protégées par NkSpinLock
             * 
             * @note Initialisation lazy : Initialize() appelée automatiquement si nécessaire
             * @note Shutdown() doit être appelé avant la fin de l'application pour rapport de fuites
             * @note Les macros NK_MEM_* utilisent __FILE__, __LINE__, __func__ automatiquement
             * 
             * @example
             * @code
             * // Usage basique avec macros
             * void* buffer = NK_MEM_ALLOC(1024);
             * if (buffer) {
             *     memset(buffer, 0, 1024);
             *     NK_MEM_FREE(buffer);
             * }
             * 
             * // Création d'objets C++ avec tracking
             * class MyEntity : public nkentseu::memory::NkGcObject {
             * public:
             *     MyEntity(int id) : mId(id) {}
             *     void Trace(NkGcTracer& tracer) override {
             *         if (mChild) tracer.Mark(mChild);
             *     }
             * private:
             *     int mId;
             *     MyEntity* mChild;
             * };
             * 
             * auto* entity = NK_MEM_NEW(MyEntity, 42);
             * if (entity) {
             *     // ... utilisation ...
             *     NK_MEM_DELETE(entity);  // Ou laisser le GC gérer
             * }
             * 
             * // Gestion de garbage collectors multiples
             * auto* customGc = NK_MEMORY_SYSTEM.CreateGc();
             * NK_MEMORY_SYSTEM.SetGcName(customGc, "RenderObjects");
             * // ... utilisation ...
             * NK_MEMORY_SYSTEM.DestroyGc(customGc);
             * 
             * // Rapport de fuites à la fermeture
             * NK_MEMORY_SYSTEM.Shutdown(true);  // true = logger les fuites détectées
             * @endcode
             */
            class NKENTSEU_MEMORY_CLASS_EXPORT NkMemorySystem {
                public:
                    /**
                     * @typedef NkLogCallback
                     * @brief Signature de callback pour logging personnalisé
                     * @param message Message à logger (string null-terminated)
                     * @note Utilisé par SetLogCallback() pour rediriger les logs mémoire
                     * @ingroup MemorySystem
                     */
                    using NkLogCallback = void (*)(const nk_char*);

                    // =================================================================
                    // @name Accès au Singleton (thread-safe)
                    // =================================================================
                    
                    /**
                     * @brief Retourne l'instance unique du système mémoire
                     * @return Référence vers le singleton NkMemorySystem
                     * @note Thread-safe à l'initialisation (Meyer's singleton, C++11+)
                     * @note L'instance est créée à la première appel, détruite à la fin du programme
                     */
                    static 
                    NkMemorySystem& Instance() noexcept;

                    // =================================================================
                    // @name Initialisation et Cycle de Vie
                    // =================================================================
                    
                    /**
                     * @brief Initialise le système mémoire avec un allocateur optionnel
                     * @param allocator Allocateur à utiliser pour les allocations internes
                     * @note Thread-safe : acquiert un lock pour configuration
                     * @note Idempotent : appels répétés sont no-op si déjà initialisé
                     * @note Si nullptr : utilise NkGetDefaultAllocator() en fallback
                     * @note Lazy : appelée automatiquement par Allocate() si nécessaire
                     */
                    void Initialize(NkAllocator* allocator = nullptr) noexcept;

                    /**
                     * @brief Shut down le système et rapporte les fuites mémoire
                     * @param reportLeaks Si true, logger toutes les allocations non-libérées
                     * @note Thread-safe : acquiert un lock pour cleanup
                     * @note Détruit tous les GC personnalisés (sauf le default)
                     * @note Libère toutes les allocations trackées restantes
                     * @note Doit être appelé avant la fin de l'application pour rapport complet
                     */
                    void Shutdown(nk_bool reportLeaks = true) noexcept;

                    /** @brief Teste si le système est initialisé */
                    [[nodiscard]] nk_bool IsInitialized() const noexcept;

                    // =================================================================
                    // @name Configuration du Logging
                    // =================================================================
                    
                    /**
                     * @brief Définit un callback personnalisé pour les logs mémoire
                     * @param callback Fonction à appeler pour chaque log (nullptr = fallback vers NK_FOUNDATION_LOG)
                     * @note Thread-safe : mise à jour atomique du pointeur de callback
                     * @note Utile pour rediriger les logs vers un fichier, console personnalisée, etc.
                     */ 
                    void SetLogCallback(NkLogCallback callback) noexcept;

                    // =================================================================
                    // @name API d'Allocation Mémoire (avec metadata)
                    // =================================================================
                    
                    /**
                     * @brief Alloue de la mémoire avec tracking complet
                     * @param size Nombre d'octets à allouer
                     * @param alignment Alignement requis (puissance de 2)
                     * @param file Fichier source de l'appel (__FILE__)
                     * @param line Ligne source de l'appel (__LINE__)
                     * @param function Fonction contenant l'appel (__func__)
                     * @param tag Catégorie mémoire optionnelle pour filtering/debugging
                     * @return Pointeur vers la mémoire allouée, ou nullptr en cas d'échec
                     * @note Thread-safe : acquiert un lock pour registration dans le tracker
                     * @note Intègre automatiquement avec NkMemoryTracker et NkMemoryProfiler
                     * 
                     * @example
                     * @code
                     * // Usage direct (préférer les macros NK_MEM_ALLOC)
                     * void* ptr = NK_MEMORY_SYSTEM.Allocate(
                     *     1024, alignof(void*), __FILE__, __LINE__, __func__, "MyTag");
                     * if (ptr) {
                     *     // ... utilisation ...
                     *     NK_MEMORY_SYSTEM.Free(ptr);
                     * }
                     * @endcode
                     */ 
                    void* Allocate(nk_size size,
                                nk_size alignment,
                                const nk_char* file,
                                nk_int32 line,
                                const nk_char* function,
                                const nk_char* tag = nullptr);

                    /**
                     * @brief Libère de la mémoire précédemment allouée via Allocate()
                     * @param pointer Pointeur à libérer (nullptr accepté, no-op)
                     * @note Thread-safe : acquiert un lock pour unregistration du tracker
                     * @note Si le pointeur n'est pas tracké : fallback vers deallocation directe
                     * @note Appelle automatiquement les hooks de profiling si configurés
                     */
                    void Free(void* pointer) noexcept;

                    // =================================================================
                    // @name API Type-Safe pour Objets C++ (Templates)
                    // =================================================================
                    
                    /**
                     * @brief Alloue et construit un objet de type T avec tracking
                     * @tparam T Type de l'objet à créer
                     * @tparam Args Types des arguments du constructeur de T
                     * @param file Fichier source (__FILE__)
                     * @param line Ligne source (__LINE__)
                     * @param function Fonction source (__func__)
                     * @param tag Catégorie mémoire pour filtering
                     * @param args Arguments forwardés au constructeur de T
                     * @return Pointeur vers l'objet nouvellement créé, ou nullptr en cas d'échec
                     * @note Utilise placement new : construction dans la mémoire allouée
                     * @note Enregistre automatiquement l'objet pour tracking et destruction
                     * @note Thread-safe : acquiert un lock pour registration
                     * 
                     * @warning Ne pas appeler depuis un contexte avec lock déjà acquis (risque de deadlock)
                     * @warning L'objet doit être libéré via Delete<T>() ou NK_MEM_DELETE
                     * 
                     * @example
                     * @code
                     * // Usage direct (préférer la macro NK_MEM_NEW)
                     * auto* obj = NK_MEMORY_SYSTEM.New<MyClass>(
                     *     __FILE__, __LINE__, __func__, "MyTag", arg1, arg2);
                     * if (obj) {
                     *     obj->DoWork();
                     *     NK_MEMORY_SYSTEM.Delete(obj);  // Appelle ~MyClass() + deallocate
                     * }
                     * @endcode
                     */
                    template <typename T, typename... Args>
                    T* New(const nk_char* file,
                        nk_int32 line,
                        const nk_char* function,
                        const nk_char* tag,
                        Args&&... args);

                    /**
                     * @brief Détruit et libère un objet créé via New<T>()
                     * @tparam T Type de l'objet à détruire
                     * @param pointer Pointeur vers l'objet (nullptr accepté, no-op)
                     * @note Appelle explicitement le destructeur ~T() avant deallocation
                     * @note Désenregistre automatiquement l'objet du tracker
                     * @note Thread-safe : acquiert un lock pour unregistration
                     */
                    template <typename T>
                    void Delete(T* pointer) noexcept;

                    /**
                     * @brief Alloue et construit un tableau d'objets de type T
                     * @tparam T Type des éléments du tableau
                     * @param count Nombre d'éléments à allouer
                     * @param file Fichier source (__FILE__)
                     * @param line Ligne source (__LINE__)
                     * @param function Fonction source (__func__)
                     * @param tag Catégorie mémoire pour filtering
                     * @return Pointeur vers le tableau nouvellement créé, ou nullptr en cas d'échec
                     * @note Chaque élément est construit avec le constructeur par défaut de T
                     * @note Enregistre le tableau complet pour tracking (pas élément par élément)
                     * @note Thread-safe : acquiert un lock pour registration
                     */
                    template <typename T>
                    T* NewArray(nk_size count,
                            const nk_char* file,
                            nk_int32 line,
                            const nk_char* function,
                            const nk_char* tag);

                    /**
                     * @brief Détruit et libère un tableau créé via NewArray<T>()
                     * @tparam T Type des éléments du tableau
                     * @param pointer Pointeur vers le tableau (nullptr accepté, no-op)
                     * @note Appelle explicitement le destructeur ~T() sur chaque élément
                     * @note Désenregistre automatiquement le tableau du tracker
                     * @note Thread-safe : acquiert un lock pour unregistration
                     */
                    template <typename T>
                    void DeleteArray(T* pointer) noexcept;

                    // =================================================================
                    // @name Interrogation des Statistiques
                    // =================================================================
                    
                    /**
                     * @brief Obtient un snapshot thread-safe des statistiques mémoire
                     * @return Structure NkMemoryStats avec les métriques courantes
                     * @note Thread-safe : acquiert un lock pour lecture cohérente
                     * @note Snapshot : les valeurs peuvent changer juste après l'appel
                     * @note Utile pour affichage HUD, logging périodique, ou prise de décision runtime
                     */
                    [[nodiscard]] 
                    NkMemoryStats GetStats() const noexcept;

                    /**
                     * @brief Dump formaté de toutes les allocations non-libérées (fuites)
                     * @note Thread-safe : acquiert un lock pour lecture cohérente
                     * @note Output vers NK_FOUNDATION_LOG_INFO ou callback personnalisé
                     * @note Format : une ligne par fuite avec contexte complet (file/line/function/tag)
                     */ 
                    void DumpLeaks() noexcept;

                    // =================================================================
                    // @name Gestion des Garbage Collectors
                    // =================================================================
                    
                    /**
                     * @brief Crée un nouveau garbage collector managé par le système
                     * @param allocator Allocateur à utiliser pour ce GC (nullptr = défaut)
                     * @return Pointeur vers le nouveau NkGarbageCollector, ou nullptr en cas d'échec
                     * @note Thread-safe : acquiert un lock pour registration dans la liste interne
                     * @note Le GC créé est automatiquement tracké et peut être nommé/profilé
                     * @note L'appelant conserve l'ownership du pointeur retourné
                     * 
                     * @example
                     * @code
                     * auto* renderGc = NK_MEMORY_SYSTEM.CreateGc();
                     * if (renderGc) {
                     *     NK_MEMORY_SYSTEM.SetGcName(renderGc, "RenderObjects");
                     *     // ... utilisation ...
                     *     NK_MEMORY_SYSTEM.DestroyGc(renderGc);  // Cleanup obligatoire
                     * }
                     * @endcode
                     */ 
                    NkGarbageCollector* CreateGc(NkAllocator* allocator = nullptr) noexcept;

                    /**
                     * @brief Détruit un garbage collector précédemment créé via CreateGc()
                     * @param collector Pointeur vers le GC à détruire (nullptr = false)
                     * @return true si succès, false si collector invalide ou est le GC par défaut
                     * @note Thread-safe : acquiert un lock pour unregistration
                     * @note Ne peut PAS détruire le GC par défaut (mGc) : retourne false dans ce cas
                     * @note Détruit tous les objets managés par ce GC avant libération
                     */
                    nk_bool DestroyGc(NkGarbageCollector* collector) noexcept;

                    /**
                     * @brief Définit un nom lisible pour un garbage collector
                     * @param collector Pointeur vers le GC à nommer
                     * @param name Nom à assigner (max 63 caractères + null terminator)
                     * @return true si succès, false si collector invalide ou name vide
                     * @note Thread-safe : acquiert un lock pour mise à jour
                     * @note Utile pour debugging et profiling : les noms apparaissent dans les logs
                     */
                    nk_bool SetGcName(NkGarbageCollector* collector, const nk_char* name) noexcept;

                    /**
                     * @brief Obtient le profil d'un garbage collector managé
                     * @param collector Pointeur vers le GC à interroger
                     * @param outProfile Pointeur vers structure de sortie pour recevoir le profil
                     * @return true si succès, false si collector invalide ou outProfile nullptr
                     * @note Thread-safe : acquiert un lock pour lecture cohérente
                     * @note outProfile est toujours écrite : valeurs valides si retour true
                     */
                    nk_bool GetGcProfile(const NkGarbageCollector* collector,
                                        NkGcProfile* outProfile) const noexcept;

                    /**
                     * @brief Retourne le nombre de garbage collectors actuellement managés
                     * @return Nombre total de GC (inclut le GC par défaut)
                     * @note Thread-safe : acquiert un lock pour lecture
                     */
                    [[nodiscard]] 
                    nk_size GetGcCount() const noexcept;

                    /**
                     * @brief Retourne une référence vers le garbage collector par défaut
                     * @return Référence vers le GC par défaut (toujours valide après Initialize())
                     * @note Non thread-safe pour l'accès au GC lui-même : synchronisation externe requise
                     * @note Utile pour usage direct sans passer par CreateGc()
                     */
                    [[nodiscard]] NkGarbageCollector& Gc() noexcept;

                private:
                    // =================================================================
                    // @name Types et Structures Internes
                    // =================================================================
                    
                    /**
                     * @typedef NkDestroyFn
                     * @brief Signature de fonction pour destruction personnalisée d'allocation
                     * @param userPtr Pointeur utilisateur retourné par Allocate()
                     * @param basePtr Pointeur brut alloué (peut différer de userPtr si alignement)
                     * @param allocator Allocateur à utiliser pour deallocation
                     * @param count Nombre d'éléments (pour tableaux)
                     * @note Appelé par Free() pour destruction type-safe des objets C++
                     */
                    using NkDestroyFn = void (*)(void* userPtr, void* basePtr, 
                                                NkAllocator* allocator, nk_size count);

                    /**
                     * @struct NkAllocationNode
                     * @brief Noeud de tracking pour une allocation individuelle
                     * @note Stocké dans une liste doublement chaînée + hash table pour lookup O(1)
                     * @note Alloué via NkGetMallocAllocator() pour éviter la récursion
                     */
                    struct NkAllocationNode {
                        void* userPtr;          ///< Pointeur retourné à l'utilisateur
                        void* basePtr;          ///< Pointeur brut alloué (base réelle)
                        nk_size size;           ///< Taille de l'allocation en bytes
                        nk_size count;          ///< Nombre d'éléments (pour tableaux)
                        nk_size alignment;      ///< Alignement requis
                        NkDestroyFn destroy;    ///< Fonction de destruction personnalisée
                        const nk_char* file;    ///< Fichier source de l'allocation
                        const nk_char* function;///< Fonction source de l'allocation
                        const nk_char* tag;     ///< Catégorie mémoire pour filtering
                        nk_int32 line;          ///< Ligne source de l'allocation
                        NkAllocationNode* prev; ///< Précédent dans la liste chaînée
                        NkAllocationNode* next; ///< Suivant dans la liste chaînée
                    };

                    /**
                     * @struct NkGcNode
                     * @brief Noeud de tracking pour un garbage collector managé
                     * @note Stocké dans une liste doublement chaînée pour itération
                     * @note Alloué via NkGetMallocAllocator() pour éviter la récursion
                     */
                    struct NkGcNode {
                        NkGarbageCollector* Collector;  ///< Pointeur vers le GC managé
                        NkAllocator* Allocator;          ///< Allocateur utilisé par ce GC
                        nk_uint64 Id;                    ///< Identifiant unique incrémental
                        nk_char Name[64];                ///< Nom lisible pour debugging
                        nk_bool IsDefault;               ///< true si c'est le GC par défaut
                        NkGcNode* Prev;                  ///< Précédent dans la liste chaînée
                        NkGcNode* Next;                  ///< Suivant dans la liste chaînée
                    };

                    // =================================================================
                    // @name Constructeur / Destructeur (privés pour singleton)
                    // =================================================================
                    
                    /** @brief Constructeur privé : initialisation des membres */
                    NkMemorySystem() noexcept;
                    
                    /** @brief Destructeur privé : par défaut (Shutdown() doit être appelé explicitement) */
                    ~NkMemorySystem() = default;

                    // Non-copiable, non-movable : sémantique de singleton
                    NkMemorySystem(const NkMemorySystem&) = delete;
                    NkMemorySystem& operator=(const NkMemorySystem&) = delete;

                    // =================================================================
                    // @name Fonctions de Destruction Type-Safe (Templates)
                    // =================================================================
                    
                    /**
                     * @brief Destruction générique pour allocations brutes (DestroyRaw)
                     * @note Appelle simplement allocator->Deallocate(basePtr)
                     */
                    static void DestroyRaw(void* userPtr, void* basePtr, 
                                        NkAllocator* allocator, nk_size count);

                    /**
                     * @brief Destruction générique pour objets uniques de type T
                     * @tparam T Type de l'objet à détruire
                     * @note Appelle explicitement ~T() puis allocator->Deallocate(basePtr)
                     */
                    template <typename T>
                    static void DestroyObject(void* userPtr, void* basePtr, 
                                            NkAllocator* allocator, nk_size /*count*/);

                    /**
                     * @brief Destruction générique pour tableaux d'objets de type T
                     * @tparam T Type des éléments du tableau
                     * @note Appelle ~T() sur chaque élément puis allocator->Deallocate(basePtr)
                     */
                    template <typename T>
                    static void DestroyArray(void* userPtr, void* basePtr, 
                                            NkAllocator* allocator, nk_size count);

                    // =================================================================
                    // @name Méthodes Internes de Tracking
                    // =================================================================
                    
                    /**
                     * @brief Enregistre une nouvelle allocation dans le système
                     * @note Thread-safe : acquiert un lock pour insertion dans liste + hash table
                     * @note Intègre avec NkMemoryTracker et NkMemoryProfiler automatiquement
                     */
                    void RegisterAllocation(void* userPtr,
                                        void* basePtr,
                                        nk_size size,
                                        nk_size count,
                                        nk_size alignment,
                                        NkDestroyFn destroy,
                                        const nk_char* file,
                                        nk_int32 line,
                                        const nk_char* function,
                                        const nk_char* tag);

                    /**
                     * @brief Recherche et détache un noeud d'allocation par pointeur utilisateur
                     * @param userPtr Pointeur utilisateur à rechercher
                     * @return Pointeur vers le NkAllocationNode si trouvé, nullptr sinon
                     * @note Thread-safe : acquiert un lock pour recherche et modification
                     * @note Utilise la hash table pour lookup O(1) si initialisée
                     */
                    NkAllocationNode* FindAndDetach(void* userPtr) noexcept;

                    /**
                     * @brief Libère la mémoire d'un noeud d'allocation
                     * @param node Pointeur vers le noeud à libérer (nullptr = no-op)
                     * @note Utilise NkGetMallocAllocator() pour éviter la récursion
                     */
                    void ReleaseNode(NkAllocationNode* node) noexcept;

                    /**
                     * @brief Logge un message via callback personnalisé ou fallback
                     * @param message Message à logger (null-terminated)
                     * @note Thread-safe : lit mLogCallback sans lock (pointeur atomique)
                     */
                    void LogLine(const nk_char* message) noexcept;

                    // =================================================================
                    // @name Méthodes Internes de Gestion GC
                    // =================================================================
                    
                    /**
                     * @brief Recherche un noeud GC par pointeur de collector (version mutable)
                     * @param collector Pointeur vers le GC à rechercher
                     * @return Pointeur vers NkGcNode si trouvé, nullptr sinon
                     * @note Précondition : lock déjà acquis par l'appelant
                     */
                    NkGcNode* FindGcNodeLocked(NkGarbageCollector* collector) noexcept;
                    
                    /** @brief Version const de FindGcNodeLocked */
                    [[nodiscard]] const NkGcNode* FindGcNodeLocked(const NkGarbageCollector* collector) const noexcept;

                    /**
                     * @brief Copie un nom de GC avec bounds checking
                     * @param destination Buffer de destination
                     * @param destinationSize Taille du buffer de destination
                     * @param source Nom source à copier (peut être nullptr)
                     * @note Garantit null-termination et évite buffer overflow
                     */
                    static void CopyGcName(nk_char* destination,
                                        nk_size destinationSize,
                                        const nk_char* source) noexcept;

                    // =================================================================
                    // @name Membres Privés
                    // =================================================================
                    
                    /** @brief Allocateur principal utilisé pour les allocations utilisateur */
                    NkAllocator* mAllocator;
                    
                    /** @brief Tête de la liste doublement chaînée des allocations trackées */
                    NkAllocationNode* mHead;
                    
                    /** @brief Octets actuellement alloués et trackés */
                    nk_size mLiveBytes;
                    
                    /** @brief Pic historique d'octets alloués */
                    nk_size mPeakBytes;
                    
                    /** @brief Nombre d'allocations actuellement actives */
                    nk_size mLiveAllocations;
                    
                    /** @brief Nombre total d'allocations depuis le démarrage (cumulatif) */
                    nk_size mTotalAllocations;
                    
                    /** @brief Callback personnalisé pour logging (nullptr = fallback) */
                    NkLogCallback mLogCallback;
                    
                    /** @brief Flag d'initialisation : true après Initialize() */
                    nk_bool mInitialized;
                    
                    /** @brief Spinlock pour synchronisation thread-safe (NKCore) */
                    mutable NkSpinLock mLock;
                    
                    /** @brief Hash table pour lookup O(1) des allocations par pointeur */
                    NkPointerHashMap mAllocIndex;
                    
                    /** @brief Tracker de fuites mémoire pour debugging */
                    NkMemoryTracker mTracker;
                    
                    /** @brief Garbage collector par défaut (toujours présent) */
                    NkGarbageCollector mGc;
                    
                    /** @brief Noeud interne pour le GC par défaut (évite allocation dynamique) */
                    NkGcNode mDefaultGcNode;
                    
                    /** @brief Tête de la liste chaînée des GC managés */
                    NkGcNode* mGcHead;
                    
                    /** @brief Nombre total de GC managés (inclut le default) */
                    nk_size mGcCount;
                    
                    /** @brief Prochain identifiant unique pour nouveau GC */
                    nk_uint64 mNextGcId;
            };

        } // namespace memory
    } // namespace nkentseu

    // -------------------------------------------------------------------------
    // SECTION 5 : FONCTIONS GLOBALES ET MACROS UTILITAIRES
    // -------------------------------------------------------------------------
    namespace nkentseu {
        namespace memory {
            /**
             * @brief Accès rapide au garbage collector par défaut
             * @return Référence vers le GC par défaut via NkMemorySystem::Instance().Gc()
             * @ingroup MemorySystem
             */
            inline NkGarbageCollector& NkGeneralGc() noexcept {
                return NkMemorySystem::Instance().Gc();
            }
        } // namespace memory
    } // namespace nkentseu

    // -------------------------------------------------------------------------
    // SECTION 6 : MACROS POUR INTÉGRATION FACILE (legacy-friendly)
    // -------------------------------------------------------------------------
    /**
     * @defgroup MemoryMacros Macros d'Accès Rapide au Système Mémoire
     * @brief Macros pour intégration transparente dans le code existant
     * 
     * Ces macros injectent automatiquement __FILE__, __LINE__, __func__ 
     * pour un tracking complet sans verbosité dans le code appelant.
     * 
     * @note En release avec NKENTSEU_DISABLE_MEMORY_TRACKING, certaines 
     *       macros peuvent devenir des wrappers directs vers malloc/free
     * @note Thread-safe : toutes les macros délèguent à des méthodes thread-safe de NkMemorySystem
     */

    /** @brief Accès court au singleton NkMemorySystem */
    #define NK_MEMORY_SYSTEM (::nkentseu::memory::NkMemorySystem::Instance())
    
    /** @brief Accès court au garbage collector par défaut */
    #define NK_GC_GENERAL (::nkentseu::memory::NkGeneralGc())

    /**
     * @brief Alloue de la mémoire brute avec tracking automatique
     * @param bytes Nombre d'octets à allouer
     * @return Pointeur vers la mémoire allouée, ou nullptr en cas d'échec
     * @note Injecte automatiquement __FILE__, __LINE__, __func__, tag="raw"
     * @ingroup MemoryMacros
     * 
     * @example
     * @code
     * void* buffer = NK_MEM_ALLOC(1024);
     * if (buffer) {
     *     // ... utilisation ...
     *     NK_MEM_FREE(buffer);
     * }
     * @endcode
     */
    #define NK_MEM_ALLOC(bytes)                                                                            \
        NK_MEMORY_SYSTEM.Allocate((bytes), alignof(void*), __FILE__, __LINE__, __func__, "raw")

    /**
     * @brief Libère de la mémoire allouée via NK_MEM_ALLOC
     * @param ptr Pointeur à libérer
     * @note Thread-safe et safe sur nullptr
     * @ingroup MemoryMacros
     */
    #define NK_MEM_FREE(ptr) NK_MEMORY_SYSTEM.Free((ptr))

    /**
     * @brief Alloue et construit un objet de type T avec tracking
     * @param T Type de l'objet à créer
     * @param ... Arguments forwardés au constructeur de T
     * @return Pointeur vers l'objet nouvellement créé, ou nullptr en cas d'échec
     * @note Injecte automatiquement __FILE__, __LINE__, __func__, tag=#T
     * @ingroup MemoryMacros
     * 
     * @example
     * @code
     * auto* obj = NK_MEM_NEW(MyClass, arg1, arg2);
     * if (obj) {
     *     obj->DoWork();
     *     NK_MEM_DELETE(obj);
     * }
     * @endcode
     */
    #define NK_MEM_NEW(T, ...)                                                                            \
        NK_MEMORY_SYSTEM.New<T>(__FILE__, __LINE__, __func__, #T, ##__VA_ARGS__)

    /**
     * @brief Détruit et libère un objet créé via NK_MEM_NEW
     * @param ptr Pointeur vers l'objet à détruire
     * @note Appelle ~T() puis deallocate automatiquement
     * @ingroup MemoryMacros
     */
    #define NK_MEM_DELETE(ptr) NK_MEMORY_SYSTEM.Delete((ptr))

    /**
     * @brief Alloue et construit un tableau d'objets de type T
     * @param T Type des éléments du tableau
     * @param count Nombre d'éléments à allouer
     * @return Pointeur vers le tableau nouvellement créé, ou nullptr en cas d'échec
     * @note Injecte automatiquement __FILE__, __LINE__, __func__, tag=#T "[]"
     * @ingroup MemoryMacros
     */
    #define NK_MEM_NEW_ARRAY(T, count)                                                                    \
        NK_MEMORY_SYSTEM.NewArray<T>((count), __FILE__, __LINE__, __func__, #T "[]")

    /**
     * @brief Détruit et libère un tableau créé via NK_MEM_NEW_ARRAY
     * @param ptr Pointeur vers le tableau à détruire
     * @note Appelle ~T() sur chaque élément puis deallocate
     * @ingroup MemoryMacros
     */
    #define NK_MEM_DELETE_ARRAY(ptr) NK_MEMORY_SYSTEM.DeleteArray((ptr))

    // -------------------------------------------------------------------------
    // SECTION 7 : IMPLÉMENTATIONS TEMPLATES INLINE
    // -------------------------------------------------------------------------
    // Les templates doivent être dans le header pour l'instantiation.

    namespace nkentseu {
        namespace memory {

            // ====================================================================
            // Implémentations Inline - API Type-Safe pour Objets C++
            // ====================================================================

            template <typename T, typename... Args>
            T* NkMemorySystem::New(const nk_char* file,
                                  nk_int32 line,
                                  const nk_char* function,
                                  const nk_char* tag,
                                  Args&&... args) {
                // Allocation de la mémoire brute via l'allocateur configuré
                void* memory = mAllocator->Allocate(sizeof(T), alignof(T));
                if (!memory) {
                    return nullptr;  // Échec d'allocation
                }

                // Construction via placement new avec perfect forwarding
                T* object = nullptr;
                #if NKENTSEU_EXCEPTIONS_ENABLED
                    try {
                        object = new (memory) T(traits::NkForward<Args>(args)...);
                    } catch (...) {
                        // En cas d'exception : cleanup et retour nullptr
                        mAllocator->Deallocate(memory);
                        return nullptr;
                    }
                #else
                    // Mode sans exceptions : construction directe
                    object = new (memory) T(traits::NkForward<Args>(args)...);
                    if (!object) {
                        mAllocator->Deallocate(memory);
                        return nullptr;
                    }
                #endif

                // Enregistrement pour tracking et destruction automatique
                RegisterAllocation(object, memory, sizeof(T), 1u, alignof(T),
                                &DestroyObject<T>, file, line, function, tag);
                return object;
            }

            template <typename T>
            void NkMemorySystem::Delete(T* pointer) noexcept {
                // Délègue à Free() qui gère le tracking + destruction type-safe
                Free(static_cast<void*>(pointer));
            }

            template <typename T>
            T* NkMemorySystem::NewArray(nk_size count,
                                       const nk_char* file,
                                       nk_int32 line,
                                       const nk_char* function,
                                       const nk_char* tag) {
                if (count == 0u) {
                    return nullptr;  // Pas d'allocation pour tableau vide
                }

                // Allocation du buffer brut pour le tableau
                void* memory = mAllocator->Allocate(sizeof(T) * count, alignof(T));
                if (!memory) {
                    return nullptr;  // Échec d'allocation
                }

                // Construction de chaque élément via placement new
                T* array = static_cast<T*>(memory);
                for (nk_size i = 0u; i < count; ++i) {
                    #if NKENTSEU_EXCEPTIONS_ENABLED
                        try {
                            new (array + i) T();
                        } catch (...) {
                            // Cleanup partiel en cas d'exception : détruire les éléments déjà construits
                            for (nk_size j = 0u; j < i; ++j) {
                                array[j].~T();
                            }
                            mAllocator->Deallocate(memory);
                            return nullptr;
                        }
                    #else
                        new (array + i) T();
                    #endif
                }

                // Enregistrement pour tracking et destruction automatique
                RegisterAllocation(array, memory, sizeof(T) * count, count, alignof(T),
                                &DestroyArray<T>, file, line, function, tag);
                return array;
            }

            template <typename T>
            void NkMemorySystem::DeleteArray(T* pointer) noexcept {
                // Délègue à Free() qui gère le tracking + destruction type-safe du tableau
                Free(static_cast<void*>(pointer));
            }

            // ====================================================================
            // Implémentations Inline - Fonctions de Destruction Type-Safe
            // ====================================================================

            template <typename T>
            void NkMemorySystem::DestroyObject(void* userPtr, void* basePtr, 
                                              NkAllocator* allocator, nk_size /*count*/) {
                if (!userPtr || !allocator) {
                    return;  // Guards de sécurité
                }
                // Cast vers le type concret pour appel du destructeur spécifique
                T* object = static_cast<T*>(userPtr);
                // Appel explicite du destructeur (placement delete)
                object->~T();
                // Deallocation de la mémoire via l'allocateur configuré
                allocator->Deallocate(basePtr);
            }

            template <typename T>
            void NkMemorySystem::DestroyArray(void* userPtr, void* basePtr, 
                                             NkAllocator* allocator, nk_size count) {
                if (!userPtr || !allocator) {
                    return;  // Guards de sécurité
                }
                // Cast vers le type concret du tableau
                T* array = static_cast<T*>(userPtr);
                // Appel explicite du destructeur sur chaque élément (ordre inverse recommandé)
                for (nk_size i = count; i-- > 0u;) {
                    array[i].~T();
                }
                // Deallocation de la mémoire via l'allocateur configuré
                allocator->Deallocate(basePtr);
            }

            // ====================================================================
            // Implémentations Inline - Méthodes d'Interrogation
            // ====================================================================

            inline nk_bool NkMemorySystem::IsInitialized() const noexcept {
                return mInitialized;
            }

            inline NkGarbageCollector& NkMemorySystem::Gc() noexcept {
                return mGc;
            }

        } // namespace memory
    } // namespace nkentseu

#endif // NKENTSEU_MEMORY_NKMEMORY_H

// =============================================================================
// NOTES DE CONFIGURATION
// =============================================================================
/*
    [Désactiver le tracking en release pour performance]
    
    Dans votre configuration de build release, définir :
    
        #define NKENTSEU_DISABLE_MEMORY_TRACKING
    
    Effets :
    - Les allocations via NK_MEM_ALLOC deviennent des wrappers directs vers malloc
    - NkMemoryTracker et NkMemoryProfiler deviennent des no-ops
    - Overhead CPU : ~0%, Overhead mémoire : 0 bytes pour metadata
    - Les macros NK_MEM_* restent fonctionnelles mais sans tracking détaillé
    
    [Intégration avec code legacy]
    
    Pour instrumenter progressivement un codebase existant :
    
    1. Remplacer malloc/free par NK_MEM_ALLOC/NK_MEM_FREE via search/replace
    2. Ajouter NK_MEM_NEW/NK_MEM_DELETE pour les new/delete C++
    3. Activer le tracking en debug, désactiver en release via config
    4. Utiliser DumpLeaks() en fin de tests pour valider l'absence de fuites
    
    [Thread-safety guarantees]
    
    ✓ Toutes les méthodes publiques de NkMemorySystem acquièrent mLock via SpinLockGuard
    ✓ Les allocations/désallocations sont sérialisées : pas de corruption possible
    ✓ Les objets alloués eux-mêmes ne sont PAS thread-safe : synchronisation externe requise
    ✓ Les GC managés sont thread-safe pour leurs opérations internes, mais l'accès aux objets nécessite synchronisation
    
    ✗ Ne pas appeler NK_MEM_NEW depuis un contexte avec lock déjà acquis sur le même thread
    ✗ Ne pas modifier les pointeurs trackés depuis un autre thread sans synchronisation
    ✗ Ne pas free/delete manuellement un pointeur alloué via NK_MEM_* : utiliser les macros correspondantes
*/

// =============================================================================
// EXEMPLES D'UTILISATION AVANCÉS
// =============================================================================
/*
    // Exemple 1 : Logging personnalisé pour les fuites mémoire
    void MyLogCallback(const nk_char* message) {
        // Rediriger vers un fichier de log dédié
        FILE* f = fopen("memory_leaks.log", "a");
        if (f) {
            fprintf(f, "%s\n", message);
            fclose(f);
        }
        // Ou vers un système de logging externe
        ExternalLogger::Warn("Memory", message);
    }
    
    void InitLogging() {
        NK_MEMORY_SYSTEM.SetLogCallback(MyLogCallback);
    }
    
    // Exemple 2 : Profiling mémoire par frame (jeu vidéo)
    void EndFrameProfiling() {
        static nk_size lastFrameBytes = 0;
        
        auto stats = NK_MEMORY_SYSTEM.GetStats();
        nk_int64 delta = static_cast<nk_int64>(stats.liveBytes) - static_cast<nk_int64>(lastFrameBytes);
        
        if (delta > 0) {
            NK_FOUNDATION_LOG_DEBUG("[Memory] Frame growth: +%lld bytes (total: %zu)", 
                                   delta, stats.liveBytes);
        }
        
        lastFrameBytes = stats.liveBytes;
        
        // Dump toutes les 60 frames
        static int frameCount = 0;
        if (++frameCount >= 60) {
            NK_MEMORY_SYSTEM.DumpLeaks();  // Rapport complet des fuites
            frameCount = 0;
        }
    }
    
    // Exemple 3 : Garbage collectors multiples par sous-système
    void InitSubsystems() {
        // GC pour les objets de rendu (collecte fréquente)
        auto* renderGc = NK_MEMORY_SYSTEM.CreateGc();
        NK_MEMORY_SYSTEM.SetGcName(renderGc, "RenderObjects");
        
        // GC pour les assets chargés dynamiquement (collecte rare)
        auto* assetGc = NK_MEMORY_SYSTEM.CreateGc();
        NK_MEMORY_SYSTEM.SetGcName(assetGc, "DynamicAssets");
        
        // Usage : créer des objets avec le GC approprié
        auto* texture = renderGc->New<Texture>("icon.png");  // Si Texture hérite de NkGcObject
        auto* level = assetGc->New<Level>("level_01.dat");
        
        // Collecte sélective : collectRenderGc.Collect() sans affecter assetGc
    }
    
    // Exemple 4 : Debugging d'une fuite mémoire spécifique
    void DebugLeakInvestigation() {
        // Allouer avec tag spécifique pour filtering
        void* suspect = NK_MEM_ALLOC(1024);  // Tag implicite "raw"
        
        // ... code suspect ...
        
        // Si suspect n'est pas libéré, il apparaîtra dans DumpLeaks() avec :
        // - File/line/function exacts de l'allocation
        // - Tag "raw" pour identification
        // - Taille 1024 bytes
        
        // Pour un tracking plus fin, utiliser NkMemoryTracker directement :
        nkentseu::memory::NkAllocationInfo info{};
        info.userPtr = suspect;
        info.size = 1024;
        info.tag = static_cast<nk_uint8>(nkentseu::memory::NkMemoryTag::NK_MEMORY_DEBUG);
        info.file = __FILE__;
        info.line = __LINE__;
        info.function = __func__;
        NK_MEMORY_SYSTEM.mTracker.Register(info);  // Accès direct si nécessaire
    }
    
    // Exemple 5 : Migration progressive depuis malloc/free
    // Étape 1 : Wrapper compatible
    #define malloc(size) NK_MEM_ALLOC(size)
    #define free(ptr) NK_MEM_FREE(ptr)
    
    // Étape 2 : Compiler et tester en debug avec tracking activé
    // Étape 3 : Corriger les fuites rapportées par DumpLeaks()
    // Étape 4 : En release, définir NKENTSEU_DISABLE_MEMORY_TRACKING pour performance
    // Étape 5 : Optionnellement, remplacer les macros par appels directs pour plus de contrôle
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================