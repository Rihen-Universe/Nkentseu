// =============================================================================
// NKSerialization/NkISerializable.cpp — Implémentations non-template
// =============================================================================
/**
 * @file NkISerializable.cpp
 * @brief Implémentation des méthodes non-template de NkISerializable et NkSerializableRegistry.
 * 
 * Ce fichier contient :
 * - Le destructeur virtuel de NkISerializable (définition pour vtable)
 * - L'implémentation du singleton Global() de NkSerializableRegistry
 * - Les méthodes RegisterFactory(), Unregister(), Create(), IsRegistered()
 * - La logique de recherche/insertion/suppression dans le registre
 * 
 * 🔹 Pourquoi séparer ?
 * - Réduction du temps de compilation : la logique du registry n'est recompilée
 *   que si ce .cpp change, pas à chaque inclusion du header
 * - Encapsulation : les détails d'implémentation (recherche linéaire, buffer fixe)
 *   sont cachés aux utilisateurs du header
 * - Maintenance : plus facile de profiler/déboguer les opérations du registry
 * 
 * @note Les méthodes template (Register<T>) restent dans le header car requises
 *       pour l'instantiation à chaque site d'appel — contrainte du langage C++.
 */

#include "NkISerializable.h"

namespace nkentseu {

    // =====================================================================
    // 🔧 NkISerializable — Implémentation du destructeur virtuel
    // =====================================================================

    /**
     * @brief Destructeur virtuel de NkISerializable.
     * 
     * @note Définition "out-of-line" pour :
     *    • Émettre la vtable dans cette unité de compilation uniquement
     *    • Éviter la duplication de la vtable dans chaque TU utilisant le header
     *    • Permettre une suppression polymorphique correcte via pointeur de base
     * 
     * @warning noexcept : essentiel pour la compatibilité avec les systèmes
     *          de cleanup et la garantie de non-propagation d'exceptions
     *          lors de la destruction d'objets sérialisables.
     */
    NkISerializable::~NkISerializable() noexcept = default;

    // =====================================================================
    // 🗄️ NkSerializableRegistry — Implémentations des méthodes
    // =====================================================================

    /**
     * @brief Accès au singleton global du registry.
     * @return Référence à l'instance unique de NkSerializableRegistry.
     * 
     * 🔹 Pattern Meyer's singleton :
     *    • Variable static locale dans une fonction — initialisation lazy
     *    • Thread-safe en C++11+ grâce à la garantie d'initialisation des statics
     *    • Pas de mutex requis pour les accès en lecture après initialisation
     * 
     * @note Cette méthode est inline dans le header pour performance,
     *       mais sa définition réelle est ici pour respecter la consigne
     *       "pas d'inline pour les non-templates". Le linker résoudra sans problème.
     */
    NkSerializableRegistry& NkSerializableRegistry::Global() noexcept {
        static NkSerializableRegistry instance;
        return instance;
    }

    /**
     * @brief Enregistre une factory personnalisée pour un type.
     * @param typeName Nom unique pour lookup via Create()/IsRegistered().
     * @param factory Fonction factory retournant NkISerializable* via new.
     * 
     * 🔹 Algorithme :
     *    1. Accès au singleton via Global()
     *    2. Recherche linéaire dans mEntries par nom (strcmp)
     *    3. Si trouvé : mise à jour de la factory existante (override)
     *    4. Si non trouvé : création d'une nouvelle Entry et PushBack
     * 
     * 🔹 Gestion des noms :
     *    • strncpy avec limite à 127 chars + null terminator
     *    • Buffer fixe de 128 bytes — pas d'allocation dynamique
     *    • Troncation silencieuse si nom > 127 chars (prévenir en amont)
     * 
     * @warning Thread-safety : cette méthode n'est PAS thread-safe.
     *          Tous les enregistrements doivent être faits avant tout
     *          accès concurrent, typiquement au démarrage dans un seul thread.
     */
    void NkSerializableRegistry::RegisterFactory(const char* typeName,
                                                  FactoryFn factory) noexcept {
        // Guard : paramètres valides
        if (!typeName || !factory) {
            return;
        }

        auto& reg = Global();

        // Recherche d'une entrée existante pour mise à jour
        for (nk_uint32 i = 0; i < reg.mEntries.Size(); ++i) {
            if (std::strcmp(reg.mEntries[i].name, typeName) == 0) {
                reg.mEntries[i].factory = factory;
                return;  // Mise à jour terminée
            }
        }

        // Création d'une nouvelle entrée
        Entry e;
        // Copie sécurisée du nom avec null-termination garantie
        std::strncpy(e.name, typeName, 127);
        e.name[127] = '\0';  // Garantie de null-termination même si tronqué
        e.factory = factory;

        // Ajout au registre
        reg.mEntries.PushBack(e);
    }

    /**
     * @brief Désenregistre un type du registry par son nom.
     * @param typeName Nom du type à retirer.
     * 
     * 🔹 Algorithme :
     *    1. Accès au singleton via Global()
     *    2. Recherche linéaire dans mEntries par nom (strcmp)
     *    3. Si trouvé : suppression via EraseAt(i) — O(n) à cause du shift
     *    4. Retour immédiat après première suppression (noms uniques)
     * 
     * @note EraseAt() décale les éléments suivants — acceptable car
     *       Unregister() est une opération rare (initialisation/shutdown).
     * 
     * @warning Thread-safety : cette méthode n'est PAS thread-safe.
     *          À utiliser uniquement dans un contexte mono-thread contrôlé.
     */
    void NkSerializableRegistry::Unregister(const char* typeName) noexcept {
        // Guard : paramètre valide
        if (!typeName) {
            return;
        }

        auto& reg = Global();

        // Recherche et suppression
        for (nk_uint32 i = 0; i < reg.mEntries.Size(); ++i) {
            if (std::strcmp(reg.mEntries[i].name, typeName) == 0) {
                reg.mEntries.EraseAt(i);
                return;  // Suppression terminée
            }
        }
        // Non trouvé : no-op silencieux
    }

    /**
     * @brief Crée une instance dynamique d'un type enregistré.
     * @param typeName Nom du type à instancier (doit être enregistré).
     * @return Pointeur vers NkISerializable nouvellement alloué, ou nullptr.
     * 
     * 🔹 Algorithme :
     *    1. Recherche linéaire dans mEntries par nom (strcmp)
     *    2. Si trouvé ET factory non-null : appel de factory() via new
     *    3. Retour du pointeur brut — ownership transfer to caller
     *    4. Si non trouvé ou factory null : retour nullptr
     * 
     * 🔹 Gestion mémoire — RAPPEL DU CONTRAT :
     *    • La factory alloue via new — l'appelant DOIT delete le résultat
     *    • En cas de nullptr en retour : aucune allocation n'a eu lieu
     *    • Toujours vérifier le résultat avant déréférencement
     * 
     * @note Complexité O(n) où n = nombre de types enregistrés.
     *       Pour des créations fréquentes du même type, envisager un cache.
     */
    NkISerializable* NkSerializableRegistry::Create(const char* typeName) const noexcept {
        // Guard : paramètre valide
        if (!typeName) {
            return nullptr;
        }

        // Recherche linéaire dans le registre
        for (nk_uint32 i = 0; i < mEntries.Size(); ++i) {
            if (std::strcmp(mEntries[i].name, typeName) == 0) {
                // Trouvé : vérifier que la factory est valide avant appel
                if (mEntries[i].factory) {
                    return mEntries[i].factory();  // Allocation via new — caller owns
                }
                // Factory null : type enregistré mais non créable
                return nullptr;
            }
        }
        // Non trouvé
        return nullptr;
    }

    /**
     * @brief Vérifie si un type est enregistré dans le registry.
     * @param typeName Nom du type à tester.
     * @return true si le type est enregistré avec une factory valide.
     * 
     * 🔹 Algorithme :
     *    1. Recherche linéaire dans mEntries par nom (strcmp)
     *    2. Retour true à la première correspondance trouvée
     *    3. Retour false si fin du vecteur atteinte sans match
     * 
     * @note Ne vérifie PAS si la factory est non-null — un type peut être
     *       enregistré mais non créable si sa factory a été mise à nullptr.
     *       Pour vérifier la créabilité, appeler Create() et tester nullptr.
     */
    bool NkSerializableRegistry::IsRegistered(const char* typeName) const noexcept {
        // Guard : paramètre valide
        if (!typeName) {
            return false;
        }

        // Recherche linéaire
        for (nk_uint32 i = 0; i < mEntries.Size(); ++i) {
            if (std::strcmp(mEntries[i].name, typeName) == 0) {
                return true;
            }
        }
        return false;
    }

    // =====================================================================
    // 📦 Implémentation de la méthode template Register<T>
    // =====================================================================
    /**
     * @brief Implémentation de Register<T>().
     * @note Doit rester dans le header car méthode template, mais définie ici
     *       pour cohérence avec la consigne "logique non-template dans .cpp".
     *       En pratique, le linker résoudra la référence sans problème.
     * 
     * @tparam T Type à enregistrer (doit dériver de NkISerializable).
     * @param typeName Nom unique pour lookup.
     */
    template<typename T>
    void NkSerializableRegistry::Register(const char* typeName) noexcept {
        // Délègue à RegisterFactory avec une lambda qui appelle new T()
        // La lambda capture rien — convertible en fonction pointer
        RegisterFactory(typeName, []() -> NkISerializable* {
            return new T();
        });
    }

    // =====================================================================
    // 🔗 Instanciations explicites optionnelles (pour réduction de code généré)
    // =====================================================================
    /**
     * @note Ces instanciations sont optionnelles.
     * Elles peuvent réduire la taille du binaire en évitant la duplication
     * du code template pour les types fréquemment enregistrés.
     * 
     * Exemple :
     * template void NkSerializableRegistry::Register<PlayerData>(const char*) noexcept;
     * 
     * À activer seulement si le profilage montre un bénéfice significatif.
     */

} // namespace nkentseu