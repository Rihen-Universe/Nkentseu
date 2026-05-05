// =============================================================================
// Events/NkGameplayEventBus.cpp — Implémentations non-template
// =============================================================================
/**
 * @file NkGameplayEventBus.cpp
 * @brief Implémentation des méthodes non-template de NkGameplayEventBus.
 * 
 * Ce fichier contient :
 * - Le destructeur virtuel de IEventQueue (définition pour vtable)
 * - Les méthodes publiques non-template de NkGameplayEventBus (Drain, Clear, HasPending)
 * 
 * 🔹 Pourquoi si peu de code dans le .cpp ?
 *    • La majorité de la logique est dans les templates (inline dans le .h)
 *    • C'est une contrainte du C++ : les templates doivent être visibles
 *      au site d'instantiation pour la compilation.
 * 
 * 🔹 Avantages de cette séparation malgré tout :
 *    • IEventQueue::~IEventQueue() définie ici → vtable émise une seule fois
 *    • Réduction du code généré pour les unités de compilation n'utilisant
 *      pas tous les types d'événements
 *    • Meilleure organisation pour la maintenance et le profilage
 * 
 * @note Si vous ajoutez des méthodes non-template à NkGameplayEventBus
 *       (ex: métriques globales, sérialisation), les implémenter ici.
 */

#include "NkGameplayEventBus.h"

namespace nkentseu {
    namespace ecs {

        // =====================================================================
        // 🔧 IEventQueue — Définition du destructeur virtuel
        // =====================================================================

        /**
         * @brief Destructeur virtuel de IEventQueue.
         * 
         * @note Définition "out-of-line" pour :
         *    • Émettre la vtable dans cette unité de compilation uniquement
         *    • Éviter la duplication de la vtable dans chaque TU utilisant le header
         *    • Permettre une suppression polymorphique correcte via pointeur de base
         * 
         * @warning noexcept : essentiel pour la compatibilité avec les handlers
         *          et la garantie de non-propagation d'exceptions lors de la destruction.
         */
        IEventQueue::~IEventQueue() noexcept = default;

        // =====================================================================
        // 🚌 NkGameplayEventBus — Méthodes publiques non-template
        // =====================================================================

        /**
         * @brief Exécute tous les événements en file sur tous les canaux actifs.
         * 
         * 🔹 Algorithme :
         *    1. Acquiert le mutex global mMutex (NkLockGuard)
         *    2. Parcourt séquentiellement mEntries[0..mCount-1]
         *    3. Pour chaque entrée avec queue != nullptr :
         *       • Appelle queue->Drain() — exécute les événements du canal
         *       • La purge des handlers inactifs est gérée par chaque canal
         * 
         * 🔹 Ordre d'exécution :
         *    • Canaux : ordre de création (premier Subscribe<T> pour chaque type)
         *    • Événements dans un canal : ordre FIFO (premier Queue() exécuté en premier)
         *    • Handlers dans un canal : ordre d'abonnement (premier Subscribe() appelé en premier)
         * 
         * @note Thread-safe : bloque les autres threads pendant l'exécution.
         *       Pour des événements temps-réel critiques, envisager un bus dédié
         *       avec priorité ou exécution asynchrone.
         * 
         * @see NkEventChannel<T>::Drain() pour les détails d'exécution par canal.
         */
        void NkGameplayEventBus::Drain() noexcept {
            NkLockGuard lock(mMutex);
            for (uint32 i = 0; i < mCount; ++i) {
                if (mEntries[i].queue) {
                    mEntries[i].queue->Drain();
                }
            }
        }

        /**
         * @brief Vide toutes les files d'attente sans exécuter les événements.
         * 
         * 🔹 Usage typique :
         *    • Changement de niveau/scene : annuler les événements en transit
         *    • Debug : reset de l'état événementiel pour tests reproductibles
         *    • Optimisation : skip des événements non critiques en cas de lag sévère
         * 
         * @note N'affecte PAS les handlers souscrits — seule la file d'attente est vidée.
         *       Les futurs Queue() après Clear() seront traités normalement au prochain Drain().
         * 
         * @warning Thread-safe via NkLockGuard — peut être appelé depuis n'importe quel thread.
         *          Si appelé depuis un thread différent de celui qui fait Queue(),
         *          assurez-vous de la cohérence métier (ex: ne pas Clear() pendant
         *          qu'un autre thread ajoute des événements critiques).
         */
        void NkGameplayEventBus::Clear() noexcept {
            NkLockGuard lock(mMutex);
            for (uint32 i = 0; i < mCount; ++i) {
                if (mEntries[i].queue) {
                    mEntries[i].queue->Clear();
                }
            }
        }

        /**
         * @brief Vérifie s'il reste des événements en attente sur n'importe quel canal.
         * @return true si au moins un canal a !Empty(), false sinon.
         * 
         * 🔹 Complexité : O(n) où n = nombre de canaux actifs (≤ kMaxChannels = 256).
         *          Négligeable en pratique — peut être appelé fréquemment.
         * 
         * 🔹 Usage typique :
         *    • Déterminer si le jeu peut passer en état "idle" ou "pause"
         *    • Debug : vérifier qu'aucun événement n'est "perdu" ou oublié
         *    • Optimisation : skip de mises à jour coûteuses si aucun événement en attente
         * 
         * @note Thread-safe via NkLockGuard — retourne un snapshot cohérent
         *       de l'état des files au moment de l'appel.
         * 
         * @warning Le résultat peut devenir obsolète immédiatement après le retour
         *          si un autre thread appelle Queue() entre-temps. Pour une garantie
         *          stricte, appeler HasPending() et agir dans la même section critique.
         */
        bool NkGameplayEventBus::HasPending() const noexcept {
            NkLockGuard lock(mMutex);
            for (uint32 i = 0; i < mCount; ++i) {
                if (mEntries[i].queue && !mEntries[i].queue->Empty()) {
                    return true;
                }
            }
            return false;
        }

    } // namespace ecs
} // namespace nkentseu