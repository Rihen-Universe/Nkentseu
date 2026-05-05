// =============================================================================
// NKMemory/NkGc.h
// Garbage Collector mark-and-sweep avec tracking d'objets et racines externes.
//
// Design :
//  - Séparation claire déclarations (.h) / implémentations (.cpp)
//  - Réutilisation des allocateurs NKMemory (ZÉRO duplication de logique d'alloc)
//  - Thread-safe via NkSpinLock de NKCore avec guard RAII local
//  - Index hash table O(1) pour lookup/insert/erase d'objets
//  - API type-safe via templates New<T>/Delete<T> avec vérification d'héritage
//  - Support de racines externes (NkGcRoot) pour références depuis code non-GC
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_MEMORY_NKGC_H
#define NKENTSEU_MEMORY_NKGC_H

    // -------------------------------------------------------------------------
    // SECTION 1 : DÉPENDANCES
    // -------------------------------------------------------------------------
    #include "NKMemory/NkMemoryApi.h"      // NKENTSEU_MEMORY_API
    #include "NKMemory/NkAllocator.h"      // NkAllocator pour gestion mémoire
    #include "NKCore/NkTypes.h"            // nk_bool, nk_size, nk_uptr, etc.
    #include "NKCore/NkAtomic.h"           // NkSpinLock pour synchronisation
    #include "NKCore/NkTraits.h"           // traits::NkForward pour perfect forwarding

    // -------------------------------------------------------------------------
    // SECTION 2 : CONFIGURATION ET MACROS UTILITAIRES
    // -------------------------------------------------------------------------
    /**
     * @defgroup MemoryGC Garbage Collection Mémoire
     * @brief Système de garbage collection mark-and-sweep pour objets managés
     * 
     * Caractéristiques :
     *  - Algorithme mark-and-sweep : marquage depuis les racines, suppression des non-atteints
     *  - Thread-safe : toutes les opérations protégées par NkSpinLock
     *  - Lookup O(1) : hash table avec linear probing pour registration d'objets
     *  - API type-safe : templates New<T>/Delete<T> avec static_assert d'héritage
     *  - Racines externes : NkGcRoot pour références depuis code non-managé
     *  - Allocation personnalisée : utilisation de NkAllocator configurable
     * 
     * @note Les objets managés DOIVENT hériter de NkGcObject et implémenter Trace()
     * @note Non thread-safe par défaut pour l'accès aux objets : synchronisation externe requise
     * @note Le GC n'est PAS concurrent : Collect() bloque tous les threads pendant l'exécution
     * 
     * @example
     * @code
     * // Déclaration d'une classe managée
     * class MyNode : public nkentseu::memory::NkGcObject {
     * public:
     *     MyNode(int value) : mValue(value), mNext(nullptr) {}
     *     
     *     void Trace(nkentseu::memory::NkGcTracer& tracer) override {
     *         if (mNext) tracer.Mark(mNext);  // Déclare la référence à mNext
     *     }
     *     
     *     int GetValue() const { return mValue; }
     *     MyNode* GetNext() const { return mNext; }
     *     void SetNext(MyNode* next) { mNext = next; }
     *     
     * private:
     *     int mValue;
     *     MyNode* mNext;  // Référence vers un autre objet GC-managé
     * };
     * 
     * // Usage avec le garbage collector
     * nkentseu::memory::NkGarbageCollector gc;
     * 
     * // Création d'objets managés
     * auto* root = gc.New<MyNode>(42);
     * auto* child = gc.New<MyNode>(17);
     * root->SetNext(child);
     * 
     * // Déclaration d'une racine externe (empêche la collecte)
     * nkentseu::memory::NkGcRoot rootRef(&root);
     * gc.AddRoot(&rootRef);
     * 
     * // Collecte périodique
     * gc.Collect();  // child sera collecté si plus référencé, root protégé par rootRef
     * @endcode
     */

    namespace nkentseu {
        namespace memory {

            // Forward declarations pour amitiés et références circulaires
            class NkGcObject;
            class NkGarbageCollector;
            class NkGcTracer;

            // -------------------------------------------------------------------------
            // SECTION 3 : CLASSE NkGcTracer (API de tracing pour le marquage)
            // -------------------------------------------------------------------------
            /**
             * @class NkGcTracer
             * @brief Interface passée à Trace() pour déclarer les références sortantes
             * @ingroup MemoryGC
             * 
             * Utilisé exclusivement dans l'implémentation de NkGcObject::Trace().
             * Permet de notifier le GC des pointeurs vers d'autres objets managés.
             * 
             * @note Ne pas instancier manuellement : fourni par le GC pendant Collect()
             * @note Thread-safe : Mark() acquiert le lock interne du GC
             * 
             * @example
             * @code
             * void MyObject::Trace(NkGcTracer& tracer) {
             *     if (mChild) tracer.Mark(mChild);      // Référence unique
             *     for (auto* sibling : mSiblings) {     // Référence multiple
             *         if (sibling) tracer.Mark(sibling);
             *     }
             * }
             * @endcode
             */
            class NKENTSEU_MEMORY_CLASS_EXPORT NkGcTracer {
                public:
                    /**
                     * @brief Constructeur interne (ne pas appeler directement)
                     * @param gc Référence vers le garbage collector actif
                     */
                    explicit NkGcTracer(NkGarbageCollector& gc) noexcept;

                    /**
                     * @brief Marque un objet comme atteint depuis les racines
                     * @param object Pointeur vers l'objet à marquer (nullptr ignoré)
                     * @note Idempotent : marquer plusieurs fois le même objet est sans effet
                     * @note Déclenche récursivement Trace() sur l'objet marqué
                     */
                    void Mark(NkGcObject* object) noexcept;

                private:
                    NkGarbageCollector& mGc;  ///< Référence vers le GC (non-possédé)
            };

            // -------------------------------------------------------------------------
            // SECTION 4 : CLASSE DE BASE NkGcObject (objets managés par le GC)
            // -------------------------------------------------------------------------
            /**
             * @class NkGcObject
             * @brief Classe de base pour tous les objets gérés par le garbage collector
             * @ingroup MemoryGC
             * 
             * Caractéristiques :
             *  - Double-linked list pour itération pendant le sweep
             *  - Flag mGcMarked pour l'algorithme mark-and-sweep
             *  - Métadonnées pour destruction personnalisée (allocator, fonction de cleanup)
             *  - Méthode virtuelle Trace() à override pour déclarer les références
             * 
             * @note Héritage public requis : NkGarbageCollector::New<T> vérifie via static_assert
             * @note Ne pas delete directement : utiliser NkGarbageCollector::Delete() ou laisser le GC gérer
             * @note Thread-safe pour les opérations GC, mais l'accès aux données membres nécessite synchronisation externe
             * 
             * @example
             * @code
             * class Entity : public nkentseu::memory::NkGcObject {
             * public:
             *     Entity(const char* name) : mName(name) {}
             *     
             *     void Trace(NkGcTracer& tracer) override {
             *         // Déclarer toutes les références vers d'autres NkGcObject
             *         if (mParent) tracer.Mark(mParent);
             *         for (auto* child : mChildren) {
             *             if (child) tracer.Mark(child);
             *         }
             *     }
             *     
             * private:
             *     const char* mName;
             *     Entity* mParent;
             *     std::vector<Entity*> mChildren;
             * };
             * @endcode
             */
            class NKENTSEU_MEMORY_CLASS_EXPORT NkGcObject {
                public:
                    /**
                     * @typedef NkGcDestroyFn
                     * @brief Signature de fonction pour destruction personnalisée d'objet
                     * @param object Pointeur vers l'objet à détruire (casté vers NkGcObject*)
                     * @param allocator Allocateur à utiliser pour deallocation mémoire
                     * @note Appelé par le GC quand un objet est collecté et mGcOwned=true
                     */
                    using NkGcDestroyFn = void (*)(NkGcObject* object, NkAllocator* allocator) noexcept;

                    // =================================================================
                    // @name Constructeur / Destructeur
                    // =================================================================
                    
                    /**
                     * @brief Constructeur par défaut : initialise les métadonnées GC
                     * @note Les listes (mGcNext/mGcPrev) sont initialisées à nullptr
                     * @note mGcMarked=false : l'objet n'est pas atteint au départ
                     */
                    NkGcObject() noexcept;
                    
                    /** @brief Destructeur virtuel : requis pour destruction polymorphe sûre */
                    virtual ~NkGcObject() = default;

                    // =================================================================
                    // @name Méthode de Tracing (à override dans les dérivées)
                    // =================================================================
                    
                    /**
                     * @brief Déclare les références sortantes vers d'autres objets managés
                     * @param tracer Interface pour notifier le GC des références
                     * @note Par défaut : no-op (objet sans références sortantes)
                     * @note Doit appeler tracer.Mark() pour chaque pointeur vers NkGcObject
                     * @note Ne doit PAS allouer de mémoire ni modifier l'état de l'objet
                     * 
                     * @warning Cette méthode est appelée pendant la phase de mark du GC
                     *          avec le lock du GC acquis : éviter les opérations bloquantes
                     */
                    virtual void Trace(NkGcTracer& tracer);

                    // =================================================================
                    // @name Interrogation d'État (pour debugging)
                    // =================================================================
                    
                    /**
                     * @brief Teste si l'objet a été marqué pendant la dernière collecte
                     * @return true si marqué, false sinon
                     * @note Utile pour debugging : ne pas utiliser pour la logique métier
                     * @note La valeur est reset après chaque Collect()
                     */
                    [[nodiscard]] nk_bool IsMarked() const noexcept;

                private:
                    // =================================================================
                    // @name Métadonnées internes (gérées par NkGarbageCollector)
                    // =================================================================
                    
                    /** @brief Prochain objet dans la liste doublement chaînée */
                    NkGcObject* mGcNext;
                    
                    /** @brief Objet précédent dans la liste doublement chaînée */
                    NkGcObject* mGcPrev;
                    
                    /** @brief Flag de marquage pendant la phase mark du GC */
                    nk_bool mGcMarked;
                    
                    /** @brief true si l'objet a été alloué via GC::New<T> (destruction personnalisée) */
                    nk_bool mGcOwned;
                    
                    /** @brief Allocateur utilisé pour cet objet (si mGcOwned=true) */
                    NkAllocator* mGcAllocator;
                    
                    /** @brief Fonction de destruction personnalisée (si mGcOwned=true) */
                    NkGcDestroyFn mGcDestroy;

                    // Amitiés pour accès aux membres privés
                    friend class NkGarbageCollector;
                    friend class NkGcTracer;
            };

            // -------------------------------------------------------------------------
            // SECTION 5 : CLASSE NkGcRoot (racines externes pour le GC)
            // -------------------------------------------------------------------------
            /**
             * @class NkGcRoot
             * @brief Wrapper pour références externes vers objets managés (racines du GC)
             * @ingroup MemoryGC
             * 
             * Permet de protéger un objet de la collecte en le déclarant comme racine.
             * Typiquement utilisé pour :
             *  - Variables globales pointant vers des objets managés
             *  - Pointeurs dans du code non-managé (C, scripts, etc.)
             *  - Cache ou registres temporaires hors du graphe d'objets
             * 
             * @note La racine doit être ajoutée au GC via AddRoot() pour être effective
             * @note La racine peut être mise à jour via Bind() pour suivre les changements de pointeur
             * @note Thread-safe : AddRoot/RemoveRoot acquièrent le lock du GC
             * 
             * @example
             * @code
             * // Variable globale protégée
             * static MyObject* gGlobalObject = nullptr;
             * static nkentseu::memory::NkGcRoot gGlobalRoot(&gGlobalObject);
             * 
             * void Init() {
             *     gc.AddRoot(&gGlobalRoot);  // Protège gGlobalObject de la collecte
             *     gGlobalObject = gc.New<MyObject>();
             * }
             * 
             * // Mise à jour dynamique
             * void UpdateReference(MyObject* newObj) {
             *     gGlobalRoot.Bind(&newObj);  // Le GC suivra le nouveau pointeur
             *     gGlobalObject = newObj;
             * }
             * @endcode
             */
            class NKENTSEU_MEMORY_CLASS_EXPORT NkGcRoot {
                public:
                    /**
                     * @brief Constructeur avec slot optionnel
                     * @param slot Adresse du pointeur à protéger (peut être nullptr, à binder plus tard)
                     */
                    explicit NkGcRoot(NkGcObject** slot = nullptr) noexcept;

                    /**
                     * @brief Met à jour le pointeur protégé par cette racine
                     * @param slot Nouvelle adresse du pointeur à protéger
                     * @note Utile quand le pointeur lui-même est déplacé (ex: réallocation de tableau)
                     */
                    void Bind(NkGcObject** slot) noexcept;

                    /**
                     * @brief Retourne l'adresse du pointeur protégé
                     * @return Pointeur vers pointeur (NkGcObject**) ou nullptr si non bindé
                     */
                    [[nodiscard]] NkGcObject** Slot() const noexcept;

                private:
                    /** @brief Adresse du pointeur protégé (double indirection) */
                    NkGcObject** mSlot;
                    
                    /** @brief Prochaine racine dans la liste chaînée interne du GC */
                    NkGcRoot* mNext;

                    // Amitié pour accès à mNext par le GC
                    friend class NkGarbageCollector;
            };

            // -------------------------------------------------------------------------
            // SECTION 6 : CLASSE PRINCIPALE NkGarbageCollector
            // -------------------------------------------------------------------------
            /**
             * @class NkGarbageCollector
             * @brief Garbage collector mark-and-sweep thread-safe avec index hash table
             * @ingroup MemoryGC
             * 
             * Fonctionnalités :
             *  - Algorithme mark-and-sweep : marquage depuis les racines, suppression des orphelins
             *  - Registration O(1) : hash table avec linear probing pour lookup rapide d'objets
             *  - Thread-safe : toutes les opérations publiques protégées par NkSpinLock
             *  - API type-safe : templates New<T>/Delete<T> avec vérification d'héritage à la compilation
             *  - Allocation personnalisée : utilisation configurable de NkAllocator
             *  - Racines externes : support de NkGcRoot pour références depuis code non-managé
             * 
             * @note Instance unique recommandée par contexte d'exécution (singleton ou injection)
             * @note Collect() est bloquant : tous les threads sont suspendus pendant l'exécution
             * @note Ne pas appeler New/Delete depuis Trace() : risque de deadlock ou corruption
             * 
             * @example
             * @code
             * // Initialisation
             * nkentseu::memory::NkGarbageCollector gc;
             * 
             * // Création d'objets managés
             * auto* obj1 = gc.New<MyClass>(arg1, arg2);
             * auto* obj2 = gc.New<MyClass>(arg3);
             * obj1->SetChild(obj2);  // obj1 référence obj2
             * 
             * // Protection d'une référence externe
             * MyClass* externalRef = obj1;
             * nkentseu::memory::NkGcRoot root(&externalRef);
             * gc.AddRoot(&root);
             * 
             * // Collecte périodique (ex: en fin de frame)
             * gc.Collect();  // obj2 sera collecté si plus référencé que par obj1
             * 
             * // Suppression explicite (optionnel)
             * gc.Delete(obj1);  // Libère immédiatement, bypass le GC
             * @endcode
             */
            class NKENTSEU_MEMORY_CLASS_EXPORT NkGarbageCollector {
                public:
                    // =================================================================
                    // @name Constructeur / Destructeur
                    // =================================================================
                    
                    /**
                     * @brief Constructeur avec allocateur optionnel
                     * @param allocator Allocateur à utiliser pour les buffers internes et objets managés
                     * @note Si nullptr : utilise NkGetDefaultAllocator() en fallback
                     */
                    explicit NkGarbageCollector(NkAllocator* allocator = nullptr) noexcept;
                    
                    /**
                     * @brief Destructeur : libère tous les objets managés restants
                     * @note Appelle DestroyCollectedObject() sur chaque objet non-libéré
                     * @note Thread-safe : acquiert le lock avant cleanup
                     */
                    ~NkGarbageCollector();

                    // Non-copiable, non-movable : sémantique de possession unique
                    NkGarbageCollector(const NkGarbageCollector&) = delete;
                    NkGarbageCollector& operator=(const NkGarbageCollector&) = delete;
                    NkGarbageCollector(NkGarbageCollector&&) = delete;
                    NkGarbageCollector& operator=(NkGarbageCollector&&) = delete;

                    // =================================================================
                    // @name Configuration de l'Allocateur
                    // =================================================================
                    
                    /**
                     * @brief Définit l'allocateur à utiliser pour les futures allocations
                     * @param allocator Nouvel allocateur (nullptr = fallback vers défaut)
                     * @note Thread-safe : acquiert le lock pour mise à jour
                     * @note Ne modifie pas les objets déjà alloués : seulement les futures allocations
                     * @note Échoue silencieusement si des objets sont déjà enregistrés (sécurité)
                     */
                    void SetAllocator(NkAllocator* allocator) noexcept;

                    /** @brief Retourne l'allocateur actuellement configuré */
                    [[nodiscard]] NkAllocator* GetAllocator() const noexcept;

                    // =================================================================
                    // @name Registration d'Objets et Racines
                    // =================================================================
                    
                    /**
                     * @brief Enregistre un objet managé dans le GC
                     * @param object Pointeur vers l'objet à tracker (doit hériter de NkGcObject)
                     * @note Thread-safe : acquiert le lock pour insertion dans la hash table
                     * @note Idempotent : enregistrer deux fois le même objet est sans effet
                     * @note L'objet est ajouté à la liste doublement chaînée pour itération pendant sweep
                     */
                    void RegisterObject(NkGcObject* object) noexcept;

                    /**
                     * @brief Désenregistre un objet managé du GC
                     * @param object Pointeur vers l'objet à retirer du tracking
                     * @note Thread-safe : acquiert le lock pour suppression de la hash table
                     * @note No-op si l'objet n'était pas enregistré
                     * @note N'appelle PAS le destructeur : responsabilité de l'appelant pour cleanup si nécessaire
                     */
                    void UnregisterObject(NkGcObject* object) noexcept;

                    /**
                     * @brief Ajoute une racine externe au GC
                     * @param root Pointeur vers la racine à protéger
                     * @note Thread-safe : acquiert le lock pour insertion dans la liste des racines
                     * @note La racine sera consultée pendant MarkRoots() pour démarrer le marquage
                     */
                    void AddRoot(NkGcRoot* root) noexcept;

                    /**
                     * @brief Retire une racine externe du GC
                     * @param root Pointeur vers la racine à retirer
                     * @note Thread-safe : acquiert le lock pour suppression de la liste des racines
                     * @note No-op si la racine n'était pas enregistrée
                     */
                    void RemoveRoot(NkGcRoot* root) noexcept;

                    // =================================================================
                    // @name API Type-Safe pour Création/Suppression d'Objets
                    // =================================================================
                    
                    /**
                     * @brief Alloue et construit un objet managé de type T
                     * @tparam T Type de l'objet (doit hériter publiquement de NkGcObject)
                     * @tparam Args Types des arguments du constructeur de T
                     * @param args Arguments forwardés au constructeur de T
                     * @return Pointeur vers l'objet nouvellement créé, ou nullptr en cas d'échec
                     * @note Utilise placement new : l'objet est construit dans la mémoire allouée par mAllocator
                     * @note Enregistre automatiquement l'objet dans le GC via RegisterObject()
                     * @note Thread-safe : acquiert le lock pour registration
                     * 
                     * @warning T DOIT hériter de NkGcObject : vérifié par static_assert à la compilation
                     * @warning Ne pas appeler depuis Trace() : risque de deadlock avec le lock du GC
                     * 
                     * @example
                     * @code
                     * auto* node = gc.New<MyNode>(42, "label");
                     * if (node) {
                     *     node->DoSomething();
                     * } // Pas besoin de delete : le GC gérera la destruction
                     * @endcode
                     */
                    template <typename T, typename... Args>
                    T* New(Args&&... args);

                    /**
                     * @brief Détruit et désenregistre un objet managé de type T
                     * @tparam T Type de l'objet (doit hériter de NkGcObject)
                     * @param object Pointeur vers l'objet à détruire (nullptr accepté, no-op)
                     * @note Désenregistre d'abord l'objet du GC, puis appelle le destructeur
                     * @note Si l'objet a été créé via New<T> : utilise l'allocateur configuré pour deallocation
                     * @note Sinon : utilise delete standard (compatibilité avec objets non-GC)
                     * @note Thread-safe : acquiert le lock pour unregistration
                     */
                    template <typename T>
                    void Delete(T* object) noexcept;

                    /**
                     * @brief Alloue un tableau d'objets managés de type T
                     * @tparam T Type des éléments (doit hériter de NkGcObject)
                     * @param count Nombre d'éléments à allouer
                     * @return Pointeur vers le tableau, ou nullptr en cas d'échec
                     * @note Chaque élément est enregistré individuellement dans le GC
                     * @note Utilise new T[count] : pas de placement new, donc pas d'allocateur personnalisé
                     * @note Thread-safe : acquiert le lock pour chaque registration
                     */
                    template <typename T>
                    T* NewArray(nk_size count);

                    /**
                     * @brief Détruit un tableau d'objets managés de type T
                     * @tparam T Type des éléments
                     * @param objects Pointeur vers le tableau (nullptr accepté, no-op)
                     * @param count Nombre d'éléments dans le tableau
                     * @note Désenregistre chaque élément individuellement avant delete[]
                     * @note Thread-safe : acquiert le lock pour chaque unregistration
                     */
                    template <typename T>
                    void DeleteArray(T* objects, nk_size count) noexcept;

                    // =================================================================
                    // @name Allocation Mémoire Brute (non-managée)
                    // =================================================================
                    
                    /**
                     * @brief Alloue de la mémoire brute via l'allocateur configuré
                     * @param size Nombre d'octets à allouer
                     * @param alignment Alignement requis (défaut: alignof(void*))
                     * @return Pointeur vers la mémoire allouée, ou nullptr en cas d'échec
                     * @note Cette mémoire N'EST PAS managée par le GC : responsabilité de l'appelant pour Free()
                     * @note Utile pour buffers temporaires, metadata, ou objets non-héritant de NkGcObject
                     */
                    void* Allocate(nk_size size, nk_size alignment = alignof(void*)) noexcept;

                    /**
                     * @brief Libère de la mémoire allouée via Allocate()
                     * @param pointer Pointeur à libérer (nullptr accepté, no-op)
                     * @note Délègue à l'allocateur configuré
                     * @note Ne pas utiliser pour des objets créés via New<T> : utiliser Delete<T>() à la place
                     */
                    void Free(void* pointer) noexcept;

                    // =================================================================
                    // @name Interrogation et Contrôle du GC
                    // =================================================================
                    
                    /**
                     * @brief Teste si un objet est actuellement managé par ce GC
                     * @param object Pointeur vers l'objet à tester
                     * @return true si l'objet est enregistré, false sinon
                     * @note Thread-safe : acquiert le lock pour lookup dans la hash table
                     * @note O(1) moyen grâce à l'index hash table
                     */
                    [[nodiscard]] 
                    nk_bool ContainsObject(const NkGcObject* object) const noexcept;

                    /**
                     * @brief Déclenche une collecte garbage (mark-and-sweep)
                     * @note Thread-safe : acquiert le lock pour toute la durée de l'opération
                     * @note Bloquant : tous les threads doivent attendre la fin de Collect()
                     * @note Étapes : 1) MarkRoots(), 2) propagation récursive via Trace(), 3) Sweep() des non-marqués
                     * @note Après Collect() : tous les objets non-atteints sont détruits et désenregistrés
                     * 
                     * @warning Ne pas appeler depuis Trace() : risque de recursion infinie ou deadlock
                     * @warning Ne pas modifier le graphe d'objets depuis un autre thread pendant Collect()
                     */
                    void Collect() noexcept;

                    /** @brief Retourne le nombre d'objets actuellement managés */
                    [[nodiscard]] nk_size ObjectCount() const noexcept;

                private:
                    // =================================================================
                    // @name Implémentation Interne du GC (détaillée dans NkGc.cpp)
                    // =================================================================
                    
                    /** @brief Amitié pour que NkGcTracer puisse appeler Mark() privé */
                    friend class NkGcTracer;

                    /**
                     * @brief Marque un objet et propage récursivement via Trace()
                     * @param object Objet à marquer (nullptr ou déjà marqué = no-op)
                     * @note Appelé pendant la phase mark : acquiert le lock via NkGcTracer
                     * @note Récursif : peut appeler Trace() qui appelle Mark() sur d'autres objets
                     */
                    void Mark(NkGcObject* object) noexcept;

                    /**
                     * @brief Marque toutes les racines externes enregistrées
                     * @note Parcourt la liste chaînée mRoots et appelle Mark() sur chaque slot valide
                     * @note Point d'entrée du marquage : tous les objets atteints le seront depuis ici
                     */
                    void MarkRoots() noexcept;

                    /**
                     * @brief Phase sweep : détruit les objets non-marqués
                     * @note Parcourt la liste mObjects et appelle DestroyCollectedObject() sur les non-marqués
                     * @note Reset le flag mGcMarked pour les objets conservés (préparation prochaine collecte)
                     */
                    void Sweep() noexcept;

                    /**
                     * @brief Retire un objet de la liste doublement chaînée interne
                     * @param object Objet à unlinker
                     * @note Mise à jour des pointeurs prev/next des voisins
                     * @note Gestion spéciale si object est la tête de liste (mObjects)
                     */
                    void UnlinkObjectLocked(NkGcObject* object) noexcept;

                    /**
                     * @brief Détruit un objet collecté en utilisant la bonne méthode
                     * @param object Objet à détruire
                     * @note Si mGcOwned && mGcDestroy : appelle la fonction de destruction personnalisée
                     * @note Sinon : utilise delete standard (objets créés manuellement)
                     */
                    void DestroyCollectedObject(NkGcObject* object) noexcept;

                    /**
                     * @brief Fonction de destruction générique pour objets créés via New<T>
                     * @tparam T Type de l'objet à détruire
                     * @param object Pointeur vers l'objet (casté vers NkGcObject*)
                     * @param allocator Allocateur à utiliser pour deallocation
                     * @note Appelle explicitement le destructeur ~T() puis allocator->Deallocate()
                     */
                    template <typename T>
                    static void DestroyOwnedObject(NkGcObject* object, NkAllocator* allocator) noexcept;

                    // =================================================================
                    // @name Gestion de l'Index Hash Table (lookup O(1))
                    // =================================================================
                    
                    /** @brief Valeur sentinelle pour entrée supprimée (tombstone) */
                    static NkGcObject* HashTombstone() noexcept;
                    
                    /** @brief Fonction de hachage pour pointeurs d'objets */
                    [[nodiscard]] nk_size HashPointer(const void* pointer) const noexcept;
                    
                    /** @brief Calcule la prochaine puissance de 2 >= value */
                    [[nodiscard]] nk_size NextPow2(nk_size value) const noexcept;
                    
                    /**
                     * @brief Garantit que la hash table a assez de capacité
                     * @note Déclenche RebuildIndexLocked() si load factor > 70% ou tombstones > 20%
                     * @note Précondition : lock déjà acquis par l'appelant
                     */
                    void EnsureIndexCapacityLocked();
                    
                    /**
                     * @brief Reconstruit la hash table avec une nouvelle capacité
                     * @param requestedCapacity Capacité cible (arrondie à puissance de 2)
                     * @note Ré-hache tous les objets valides dans un nouveau buffer
                     * @note Libère l'ancien buffer après migration réussie
                     * @note Précondition : lock déjà acquis par l'appelant
                     */
                    void RebuildIndexLocked(nk_size requestedCapacity) noexcept;
                    
                    /** @brief Lookup dans la hash table (version const) */
                    [[nodiscard]] nk_bool ContainsIndexLocked(const NkGcObject* object) const noexcept;
                    
                    /** @brief Insertion dans la hash table */
                    [[nodiscard]] nk_bool InsertIndexLocked(NkGcObject* object) noexcept;
                    
                    /** @brief Suppression dans la hash table (marquage tombstone) */
                    [[nodiscard]] nk_bool EraseIndexLocked(const NkGcObject* object) noexcept;

                    // =================================================================
                    // @name Membres Privés
                    // =================================================================
                    
                    /** @brief Tête de la liste doublement chaînée des objets managés */
                    NkGcObject* mObjects;
                    
                    /** @brief Tête de la liste chaînée des racines externes */
                    NkGcRoot* mRoots;
                    
                    /** @brief Nombre d'objets actuellement managés */
                    nk_size mObjectCount;
                    
                    /** @brief Hash table pour lookup O(1) des objets (linear probing) */
                    NkGcObject** mObjectIndex;
                    
                    /** @brief Capacité de la hash table (toujours puissance de 2) */
                    nk_size mObjectIndexCapacity;
                    
                    /** @brief Nombre d'entrées valides dans la hash table */
                    nk_size mObjectIndexSize;
                    
                    /** @brief Nombre d'entrées tombstone dans la hash table */
                    nk_size mObjectIndexTombstones;
                    
                    /** @brief Allocateur utilisé pour les buffers internes et objets managés */
                    NkAllocator* mAllocator;
                    
                    /** @brief Spinlock pour synchronisation thread-safe (NKCore) */
                    mutable NkSpinLock mLock;
            };

        } // namespace memory
    } // namespace nkentseu

    // -------------------------------------------------------------------------
    // SECTION 7 : IMPLÉMENTATIONS TEMPLATES INLINE
    // -------------------------------------------------------------------------
    // Les templates doivent être dans le header pour l'instantiation.

    namespace nkentseu {
        namespace memory {

            // ====================================================================
            // Implémentations Inline - API Type-Safe
            // ====================================================================

            template <typename T, typename... Args>
            T* NkGarbageCollector::New(Args&&... args) {
                // Vérification à la compilation que T hérite de NkGcObject
                #if defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER)
                    static_assert(std::is_base_of<NkGcObject, T>::value, 
                                 "T must derive from NkGcObject");
                #endif

                // Allocation de la mémoire brute via l'allocateur configuré
                void* storage = mAllocator ? mAllocator->Allocate(sizeof(T), alignof(T)) : nullptr;
                if (!storage) {
                    return nullptr;  // Échec d'allocation
                }

                // Construction via placement new avec perfect forwarding
                T* object = nullptr;
                #if NKENTSEU_EXCEPTIONS_ENABLED
                    try {
                        object = new (storage) T(traits::NkForward<Args>(args)...);
                    } catch (...) {
                        // En cas d'exception dans le constructeur : cleanup et retour nullptr
                        mAllocator->Deallocate(storage);
                        return nullptr;
                    }
                #else
                    // Mode sans exceptions : construction directe
                    object = new (storage) T(traits::NkForward<Args>(args)...);
                    if (!object) {
                        mAllocator->Deallocate(storage);
                        return nullptr;
                    }
                #endif

                // Configuration des métadonnées GC pour destruction personnalisée
                object->mGcOwned = true;
                object->mGcAllocator = mAllocator;
                object->mGcDestroy = &DestroyOwnedObject<T>;
                
                // Enregistrement automatique dans le GC
                RegisterObject(object);
                return object;
            }

            template <typename T>
            void NkGarbageCollector::Delete(T* object) noexcept {
                if (!object) {
                    return;  // Safe : no-op sur nullptr
                }
                
                // Désenregistrement d'abord pour éviter que le GC ne tente de détruire l'objet
                UnregisterObject(object);
                
                // Destruction via la méthode appropriée
                if (object->mGcOwned && object->mGcAllocator && object->mGcDestroy) {
                    // Objet créé via New<T> : utiliser la fonction de destruction personnalisée
                    object->mGcDestroy(static_cast<NkGcObject*>(object), object->mGcAllocator);
                } else {
                    // Objet créé manuellement : fallback vers delete standard
                    delete object;
                }
            }

            template <typename T>
            T* NkGarbageCollector::NewArray(nk_size count) {
                if (count == 0u) {
                    return nullptr;  // Pas d'allocation pour tableau vide
                }
                
                #if defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER)
                    static_assert(std::is_base_of<NkGcObject, T>::value, 
                                 "T must derive from NkGcObject");
                #endif

                // Allocation du tableau via new[] (pas de placement new pour les tableaux)
                T* objects = nullptr;
                #if NKENTSEU_EXCEPTIONS_ENABLED
                    try {
                        objects = new T[count];
                    } catch (...) {
                        return nullptr;  // Échec d'allocation ou construction
                    }
                #else
                    objects = new T[count];
                    if (!objects) {
                        return nullptr;
                    }
                #endif

                // Enregistrement de chaque élément individuellement dans le GC
                for (nk_size i = 0u; i < count; ++i) {
                    RegisterObject(objects + i);
                }
                return objects;
            }

            template <typename T>
            void NkGarbageCollector::DeleteArray(T* objects, nk_size count) noexcept {
                if (!objects || count == 0u) {
                    return;  // Safe : no-op sur nullptr ou count nul
                }
                
                // Désenregistrement de chaque élément avant destruction
                for (nk_size i = 0u; i < count; ++i) {
                    UnregisterObject(objects + i);
                }
                
                // Destruction du tableau via delete[]
                delete[] objects;
            }

            template <typename T>
            void NkGarbageCollector::DestroyOwnedObject(NkGcObject* object, NkAllocator* allocator) noexcept {
                if (!object || !allocator) {
                    return;  // Guards de sécurité
                }
                
                // Cast vers le type concret pour appel du destructeur spécifique
                T* typedObject = static_cast<T*>(object);
                
                // Appel explicite du destructeur (placement delete)
                typedObject->~T();
                
                // Deallocation de la mémoire via l'allocateur configuré
                allocator->Deallocate(typedObject);
            }

        } // namespace memory
    } // namespace nkentseu

#endif // NKENTSEU_MEMORY_NKGC_H

// =============================================================================
// NOTES D'UTILISATION ET BONNES PRATIQUES
// =============================================================================
/*
    [Cycle de vie typique d'un objet managé]
    
    1. Création : gc.New<MyClass>(args...) → allocation + construction + registration
    2. Utilisation : accès normal via le pointeur retourné
    3. Référence : si un objet A référence B, implémenter A::Trace() pour appeler tracer.Mark(B)
    4. Protection externe : si une variable hors GC référence un objet, utiliser NkGcRoot + AddRoot()
    5. Collecte : gc.Collect() → marquage depuis les racines, suppression des orphelins
    6. Destruction : soit automatique par le GC, soit explicite via gc.Delete(obj)

    [Implémentation correcte de Trace()]
    
    void MyNode::Trace(NkGcTracer& tracer) {
        // Toujours vérifier nullptr avant Mark()
        if (mParent) tracer.Mark(mParent);
        
        // Pour les conteneurs : itérer et marquer chaque élément non-null
        for (auto* child : mChildren) {
            if (child) tracer.Mark(child);
        }
        
        // NE PAS :
        // - Allouer de mémoire (risk de deadlock avec lock du GC)
        // - Modifier l'état de l'objet (la phase mark est en cours)
        // - Appeler des méthodes virtuelles complexes (risque de recursion infinie)
    }

    [Gestion des racines externes]
    
    // Pattern recommandé pour variable globale :
    static MyClass* gGlobal = nullptr;
    static NkGcRoot gGlobalRoot(&gGlobal);  // Bind au constructeur
    
    void Init() {
        gc.AddRoot(&gGlobalRoot);  // Activer la protection
        gGlobal = gc.New<MyClass>();  // Maintenant protégé de la collecte
    }
    
    // Si le pointeur peut être réassigné :
    void UpdateGlobal(MyClass* newObj) {
        gGlobal = newObj;  // Le root pointe toujours vers gGlobal, donc suit automatiquement
    }

    [Performance et tuning]
    
    - Load factor hash table : 70% max avant rehash → compromis mémoire/performance
    - Tombstone threshold : 20% max avant cleanup → évite la dégradation des lookups
    - Collect() frequency : appeler périodiquement (ex: fin de frame) plutôt qu'à chaque allocation
    - Trace() complexity : garder léger et déterministe pour éviter les pauses longues

    [Debugging tips]
    
    - Si un objet est collecté trop tôt : vérifier que Trace() déclare toutes les références
    - Si fuite mémoire : vérifier que les racines externes sont correctement AddRoot/RemoveRoot
    - Si crash dans Collect() : vérifier qu'aucun thread ne modifie le graphe pendant le GC
    - Utiliser ContainsObject() pour vérifier si un objet est toujours managé

    [Thread-safety guarantees]
    
    ✓ Toutes les méthodes publiques de NkGarbageCollector acquièrent mLock
    ✓ Mark()/Trace() sont appelés avec le lock acquis : pas de modification concurrente possible
    ✓ Les objets eux-mêmes ne sont PAS thread-safe : synchronisation externe requise pour accès concurrent
    
    ✗ Ne pas appeler New/Delete depuis Trace() : risque de deadlock (lock déjà acquis)
    ✗ Ne pas modifier les pointeurs d'un objet depuis un autre thread pendant Collect()
    ✗ Ne pas delete manuellement un objet enregistré : utiliser UnregisterObject + Delete ou laisser le GC faire
*/

// =============================================================================
// COMPARAISON AVEC AUTRES APPROCHES DE GESTION MÉMOIRE
// =============================================================================
/*
    | Approche              | NkGarbageCollector      | NkUniquePtr           | NkSharedPtr          | Manual delete      |
    |----------------------|-------------------------|-----------------------|---------------------|-------------------|
    | Cycle de vie         | Automatique (GC)        | Ownership unique      | Référence comptée   | Manuel            |
    | Cycles de référence  | Gérés (mark depuis racines) | Non possible     | Nécessite weak ptr  | Fuites possibles  |
    | Performance runtime  | Pause pendant Collect() | Zéro overhead         | Atomic inc/dec      | Zéro overhead     |
    | Performance mémoire  | Overhead métadonnées    | +4/8 bytes par objet  | +bloc de contrôle   | Aucun             |
    | Complexité code      | Trace() à implémenter   | Simple                | Simple              | Risque d'erreurs  |
    | Thread-safety        | GC thread-safe, objets non | Externe requis   | Comptage thread-safe| Externe requis    |
    | Usage recommandé     | Graphes d'objets complexes | Ressources uniques | Partage temporaire  | Code simple/legacy|

    Quand utiliser NkGarbageCollector :
    ✓ Graphes d'objets avec références circulaires potentielles
    ✓ Scénarios où la libération manuelle est complexe ou error-prone
    ✓ Applications avec phases de collecte bien définies (ex: fin de frame jeu vidéo)
    ✓ Systèmes avec beaucoup d'objets à courte durée de vie

    Quand éviter :
    ✗ Performance temps-réal critique : les pauses de Collect() peuvent être inacceptables
    ✗ Objets avec cycle de vie simple et déterministe : préférer NkUniquePtr
    ✗ Partage temporaire entre composants : préférer NkSharedPtr avec weak si cycles possibles
    ✗ Code legacy ou interop avec bibliothèques externes : manual delete peut être plus simple
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================