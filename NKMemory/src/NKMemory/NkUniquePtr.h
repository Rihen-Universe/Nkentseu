/**
* @File NkUniquePtr.h
* @Description Implémentation d'un pointeur intelligent à possession unique
*
* @Description :
* Ce fichier définit la classe UniquePtr permettant une gestion exclusive et sécurisée
* de la mémoire via le système mémoire personnalisé. Garantit une destruction automatique
* et un transfert de propriété contrôlé.
*
* @Fonctionnalités :
* - Possession exclusive unique
* - Support des tableaux et objets simples
* - Intégration complète avec NkMemory.h
* - Tracking fichier/ligne des allocations
* - Sémantique de déplacement sécurisée
*
* @Avertissements :
* - Usage exclusif avec le système NkMemory.h
* - Aucune copie autorisée (sémantique unique)
*
* @Auteur : TEUGUIA TADJUIDJE Rodolf Séderis
* @Date : 2025-04-06
* @Licence : Rihen 2025 - Tous droits réservés
*
* @Namespace : nkentseu
*/

#pragma once
#include "Nkentseu/Platform/Types.h"
#include "Nkentseu/Platform/Export.h"
#include <functional>
#include <stdexcept>

namespace nkentseu {

    /**
    * - UniquePtr : Pointeur intelligent à possession unique
    *
    * @Description :
    * Gère une ressource mémoire de manière exclusive avec garantie de libération.
    * Implémente la sémantique de déplacement pour les transferts de propriété.
    *
    * @Template :
    * - (typename T) : Type de la ressource managée
    */
    template <typename T>
    class NKENTSEU_TEMPLATE_API UniquePtr {
    private:
        T* m_ptr;           ///< Pointeur brut vers la ressource managée
        std::function<void(T*)> m_deleter; ///< Stratégie de libération personnalisée
        usize m_elementCount; ///< Nombre d'éléments dans le tableau (1 pour un objet unique)

        /**
        * - Constructeur privé : Initialisation complète
        * @Description : Réservé au système Memory pour création sécurisée
        * @param ptr : Pointeur brut à prendre en charge
        * @param deleter : Fonction de libération spécifique à la ressource
        * @param count : Nombre d'éléments managés (défaut: 1)
        */
        explicit UniquePtr(T* ptr, std::function<void(T*)> deleter, usize count = 1)
            : m_ptr(ptr), m_deleter(deleter), m_elementCount(count) {}

    public:
        /**
        * - Destructeur : Libération garantie de la ressource
        * @Description : Appelle automatiquement le deleter configuré
        */
        ~UniquePtr() noexcept { Reset(); }

        /**
        * - Constructeur de déplacement
        * @param other : Référence rvalue à déplacer
        * @Description : Transfère la propriété de manière non-throw
        */
        UniquePtr(UniquePtr&& other) noexcept
            : m_ptr(other.m_ptr),
              m_deleter(std::move(other.m_deleter)),
              m_elementCount(other.m_elementCount) {
            other.m_ptr = nullptr;
            other.m_elementCount = 0;
        }

        /**
        * - Opérateur d'affectation par déplacement
        * @param other : Référence rvalue à déplacer
        * @return (UniquePtr&) : Référence à l'instance courante
        * @Description : Transfère la propriété en libérant les ressources existantes
        */
        UniquePtr& operator=(UniquePtr&& other) noexcept {
            if (this != &other) {
                Reset();
                m_ptr = other.m_ptr;
                m_deleter = std::move(other.m_deleter);
                m_elementCount = other.m_elementCount;
                other.m_ptr = nullptr;
                other.m_elementCount = 0;
            }
            return *this;
        }

        /**
        * - Release : Abandonne la propriété de la ressource
        * @return (T*) : Pointeur brut libéré du management
        * @warning Après l'appel, la ressource n'est plus gérée
        */
        T* Release() noexcept {
            T* released = m_ptr;
            m_ptr = nullptr;
            m_elementCount = 0;
            return released;
        }

        /**
        * - Reset : Réinitialise le gestionnaire
        * @param ptr : Nouvelle ressource à manager (optionnel)
        * @Description : Libère la ressource actuelle si existante
        */
        void Reset(T* ptr = nullptr) noexcept {
            if (m_ptr) {
                m_deleter(m_ptr);
            }
            m_ptr = ptr;
            m_elementCount = ptr ? 1 : 0;
        }

        /**
        * - Get : Accès direct au pointeur managé
        * @return (T*) : Pointeur brut sans transfert de propriété
        */
        T* Get() const noexcept { return m_ptr; }

        /**
        * - Size : Taille mémoire unitaire
        * @return (usize) : Taille en octets du type T
        */
        usize Size() const noexcept { return sizeof(T); }

        /**
        * - Leng : Capacité en éléments
        * @return (usize) : Nombre d'éléments alloués
        */
        usize Leng() const noexcept { return m_elementCount; }

        /**
        * - IsArray : Vérifie le type d'allocation
        * @return (bool) : true si gestion d'un tableau
        */
        bool IsArray() const noexcept { return m_elementCount > 1; }

        /**
        * - operator[] : Accès indexé sécurisé
        * @param index : Position dans le tableau
        * @return (T&) : Référence à l'élément
        * @throws std::out_of_range : Si index >= Leng()
        */
        T& operator[](usize index) {
            if (index >= Leng()) {
                throw std::out_of_range("UniquePtr::operator[] - Index out of range");
            }
            if (m_ptr == nullptr) {
                throw std::out_of_range("UniquePtr::operator[] - ptr is null");
            }
            return m_ptr[index];
        }

        // ---------------------------
        // Opérateurs d'accès standard
        // ---------------------------

        /**
        * - operator* : Déréférencement
        * @return (T&) : Référence à la ressource
        * @precondition : m_ptr != nullptr
        */
        T& operator*() const noexcept { return *m_ptr; }

        /**
        * - operator-> : Accès membre
        * @return (T*) : Pointeur pour accès membre
        * @precondition : m_ptr != nullptr
        */
        T* operator->() const noexcept { return m_ptr; }

        /**
        * - operator T* : Conversion implicite
        * @return (T*) : Pointeur brut managé
        */
        operator T*() const noexcept { return m_ptr; }

        // ---------------------------
        // Opérateurs de comparaison
        // ---------------------------

        /**
        * - operator== : Comparaison d'égalité
        * @param other : Autre UniquePtr à comparer
        * @return (bool) : true si mêmes ressources
        */
        bool operator==(const UniquePtr& other) const noexcept { return m_ptr == other.m_ptr; }

        /**
        * - operator!= : Comparaison de différence
        * @param other : Autre UniquePtr à comparer
        * @return (bool) : true si ressources différentes
        */
        bool operator!=(const UniquePtr& other) const noexcept { return !(*this == other); }

        /**
        * - operator== : Comparaison avec nullptr
        * @return (bool) : true si non initialisé
        */
        bool operator==(std::nullptr_t) const noexcept { return m_ptr == nullptr; }

        /**
        * - operator!= : Comparaison avec nullptr
        * @return (bool) : true si ressource valide
        */
        bool operator!=(std::nullptr_t) const noexcept { return !(*this == nullptr); }

        // ---------------------------
        // Sécurité de copie
        // ---------------------------

        /**
        * - Suppression des copies
        * @Description : Garantit l'unicité de possession
        */
        UniquePtr(const UniquePtr&) = delete;
        UniquePtr& operator=(const UniquePtr&) = delete;

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
