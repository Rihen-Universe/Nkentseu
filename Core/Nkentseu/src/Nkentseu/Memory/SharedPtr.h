/**
* @File SharedPtr.h
* @Description Implémentation d'un pointeur intelligent à possession partagée avec comptage de références
* 
* @Description :
* Ce fichier définit la classe SharedPtr permettant une gestion sécurisée et thread-safe
* de la mémoire via comptage atomique de références. Garantit une libération automatique
* lorsque la dernière référence est détruite.
*
* @Fonctionnalités :
* - Gestion thread-safe des références
* - Support des tableaux et objets simples
* - Intégration complète avec Memory.h
* - Tracking fichier/ligne des allocations
* - Sémantique de copie/déplacement sécurisée
*
* @Avertissements :
* - Ne pas mélanger avec des pointeurs bruts
* - Usage exclusif avec MemorySystem pour les allocations
*
* @Auteur : TEUGUIA TADJUIDJE Rodolf Séderis
* @Date : 2025-04-07
* @Licence : Rihen 2025 - Tous droits réservés
* 
* @Namespace : nkentseu
*/

#pragma once
#include "Nkentseu/Platform/Platform.h"
#include "Nkentseu/Platform/Export.h"
#include <atomic>
#include <functional>
#include <type_traits>

namespace nkentseu {

    /**
    * - SharedPtr : Pointeur intelligent à référence comptée
    *
    * @Description :
    * Gère automatiquement la durée de vie d'objets/allocation via comptage atomique.
    * Garantit une destruction sécurisée même en environnement multithread.
    * Intégré au système de mémoire personnalisé avec tracking complet.
    *
    * @Template :
    * - (typename T) : Type de la ressource managée (doit être complet à la destruction)
    */
    template <typename T>
    class NKENTSEU_TEMPLATE_API SharedPtr {
    private:
        template<typename U>
        friend class SharedPtr;

        T* m_ptr = nullptr;                     ///< Pointeur vers la ressource managée
        std::atomic<usize>* m_refCount = nullptr; ///< Compteur de références atomique
        std::function<void(T*)> m_deleter;       ///< Stratégie de libération personnalisée
        bool m_isArray = false;                  ///< Indicateur de tableau
        usize m_elementCount = 1;                ///< Nombre d'éléments dans le tableau

        bool *m_forceDelete = nullptr; ///< Indicateur de suppression forcée

        /**
        * - Cleanup : Gère la décrémentation et la libération des ressources
        * @Description :
        * Décrémente le compteur de références de manière thread-safe.
        * Si le compteur atteint 0, libère la mémoire via le deleter.
        */
        void Cleanup() noexcept {
            if (m_forceDelete == nullptr) {
                if (m_refCount) delete m_refCount;
            } else {
                if (m_refCount && m_refCount->fetch_sub(1, std::memory_order_acq_rel) == 1) {
                    if (m_deleter) m_deleter(m_ptr);
                    delete m_refCount;
                }
            }
            m_ptr = nullptr;
            m_refCount = nullptr;
            m_forceDelete = nullptr;
        }

        /**
        * - CopyFrom : Copie les métadonnées d'un autre SharedPtr
        * @Description :
        * Met à jour les références partagées et incrémente le compteur atomique.
        * Garantit la cohérence en environnement multithread.
        */
        void CopyFrom(const SharedPtr& other) noexcept {
            m_ptr = other.m_ptr;
            m_refCount = other.m_refCount;
            m_deleter = other.m_deleter;
            m_isArray = other.m_isArray;
            m_elementCount = other.m_elementCount;
            m_forceDelete = other.m_forceDelete;
            
            if (m_refCount) {
                m_refCount->fetch_add(1, std::memory_order_relaxed);
            }
        }

    public:
        /**
        * - Constructeur : Crée un SharedPtr gérant un objet simple
        * @param ptr : Pointeur brut à manager
        * @param deleter : Fonction de destruction personnalisée
        * @note Utilise delete par défaut pour la libération
        */
        SharedPtr(T* ptr = nullptr, std::function<void(T*)> deleter = [](T* p){ delete p; }, bool* forceDelete = nullptr)
            : m_ptr(ptr), m_refCount(new std::atomic<usize>(1)), m_deleter(deleter), m_forceDelete(forceDelete) {}

        /**
        * - Constructeur : Crée un SharedPtr gérant un tableau
        * @param ptr : Pointeur brut du tableau
        * @param deleter : Deleter spécialisé pour tableaux
        * @param count : Nombre d'éléments dans le tableau
        * @warning Utiliser delete[] pour les tableaux
        */
        SharedPtr(T* ptr, std::function<void(T*)> deleter, usize count, bool* forceDelete = nullptr)
            : m_ptr(ptr), m_refCount(new std::atomic<usize>(1)), 
                m_deleter(deleter), m_isArray(true), m_elementCount(count), m_forceDelete(forceDelete) {}

        /**
        * - Destructeur : Libère les ressources si nécessaire
        * @Description :
        * Décrémente le compteur de références et libère les ressources
        * si c'est la dernière référence.
        */
        ~SharedPtr() noexcept { Cleanup(); }

        /**
        * - Constructeur de conversion : Depuis un type dérivé
        * @Template :
        * - (typename U) : Type source (doit hériter de T)
        * @param other : SharedPtr du type dérivé à convertir
        */
        template<typename U, typename = std::enable_if_t<std::is_base_of<T, U>::value>>
        SharedPtr(const SharedPtr<U>& other) noexcept
            : m_ptr(static_cast<T*>(other.m_ptr)),
                m_refCount(other.m_refCount),
                m_deleter([other_deleter = other.m_deleter](T* ptr) { 
                    other_deleter(static_cast<U*>(ptr)); 
                }),
                m_isArray(other.m_isArray),
                m_elementCount(other.m_elementCount), m_forceDelete(other.m_forceDelete) {
            m_refCount->fetch_add(1, std::memory_order_relaxed);
        }

        /**
        * - Constructeur par copie : Partage la propriété
        * @param other : Référence à copier
        */
        SharedPtr(const SharedPtr& other) noexcept { CopyFrom(other); }

        /**
        * - Constructeur par déplacement : Transfert de propriété
        * @param other : Référence rvalue à déplacer
        */
        SharedPtr(SharedPtr&& other) noexcept 
            : m_ptr(other.m_ptr), m_refCount(other.m_refCount),
                m_deleter(std::move(other.m_deleter)), 
                m_isArray(other.m_isArray), m_elementCount(other.m_elementCount), m_forceDelete(other.m_forceDelete) {
            other.m_ptr = nullptr;
            other.m_refCount = nullptr;
            other.m_forceDelete = nullptr;
        }

        /**
        * - Opérateur d'affectation par copie
        * @param other : Référence à copier
        * @return (SharedPtr&) : Référence courante
        */
        SharedPtr& operator=(const SharedPtr& other) noexcept {
            if (this != &other) {
                Cleanup();
                CopyFrom(other);
            }
            return *this;
        }

        /**
        * - Opérateur d'affectation par déplacement
        * @param other : Référence rvalue à déplacer
        * @return (SharedPtr&) : Référence courante
        */
        SharedPtr& operator=(SharedPtr&& other) noexcept {
            if (this != &other) {
                Cleanup();
                m_ptr = other.m_ptr;
                m_refCount = other.m_refCount;
                m_deleter = std::move(other.m_deleter);
                m_isArray = other.m_isArray;
                m_elementCount = other.m_elementCount;
                m_forceDelete = other.m_forceDelete;
                other.m_forceDelete = nullptr;
                other.m_ptr = nullptr;
                other.m_refCount = nullptr;
            }
            return *this;
        }

        /**
        * - Get : Accède au pointeur brut
        * @return (T*) : Pointeur managé (non propriétaire)
        * @warning Ne pas libérer manuellement
        */
        T* Get() const noexcept { 
            if (m_forceDelete == nullptr) {
                // Cleanup();
                return nullptr;
            }
            return m_ptr; 
        }

        /**
        * - UseCount : Donne le nombre de références actuelles
        * @return (usize) : Compteur atomique de références
        * @note 0 si non initialisé
        */
        usize UseCount() const noexcept { return m_refCount ? m_refCount->load() : 0; }

        /**
        * - IsArray : Vérifie si la ressource est un tableau
        * @return (bool) : true pour les tableaux, false sinon
        */
        bool IsArray() const noexcept { return m_isArray; }

        /**
        * - Length : Taille du tableau managé
        * @return (usize) : Nombre d'éléments (1 pour les objets simples)
        */
        usize Length() const noexcept { return m_elementCount; }

        /**
        * - Reset : Réinitialise le gestionnaire
        * @Description : Libère la référence actuelle et réinitialise les membres
        */
        void Reset() noexcept {
            Cleanup();
            m_ptr = nullptr;
            m_refCount = nullptr;
            m_deleter = nullptr;
            m_isArray = false;
            m_elementCount = 1;
            m_forceDelete = nullptr;
        }

        /**
        * - operator[] : Accès indexé sécurisé
        * @param index : Position dans le tableau
        * @return (T&) : Référence à l'élément
        * @throws std::out_of_range : Si index >= Length()
        * @precondition : IsArray() == true
        */
        T& operator[](usize index) { 
            if (index >= m_elementCount) throw std::out_of_range("SharedPtr index out of range");
            if (m_ptr == nullptr) throw std::out_of_range("SharedPtr is null");
            return m_ptr[index]; 
        }

        // ---------------------------
        // Opérateurs d'accès standard
        // ---------------------------

        /**
        * - operator* : Déréférencement
        * @return (T&) : Référence à la ressource
        * @precondition : Get() != nullptr
        */
        T& operator*() const noexcept { return *m_ptr; }

        /**
        * - operator-> : Accès membre
        * @return (T*) : Pointeur pour accès membre
        * @precondition : Get() != nullptr
        */
        T* operator->() const noexcept { return m_ptr; }

        /**
        * - operator bool : Vérification d'initialisation
        * @return (bool) : true si ressource valide
        */
        explicit operator bool() const noexcept { return m_ptr != nullptr; }

        /**
        * - friend MemorySystem : Accès privilégié
        * @Description : Contrôle exclusif des instanciations
        */
        friend class MemorySystem;
    };

} // namespace nkentseu

// Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
// Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
// © Rihen 2025 - Tous droits réservés.