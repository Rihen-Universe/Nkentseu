/**
* @File Memory.cpp
* @Description Implémentation du système de gestion mémoire
*
* @Fonctionnalités :
* - Implémentation des méthodes du singleton MemorySystem
* - Tracking des allocations en temps réel
* - Génération de rapports de fuites
* - Intégration avec le système de logging
*/

#include "pch.h"
#include "Memory.h"
#include "Nkentseu/Logger/Logger.h"

namespace nkentseu {
    usize MemorySystem::AllocationSystemInfo::counter = 0;
    std::mutex MemorySystem::AllocationSystemInfo::counterMutex = std::mutex();
    /**
    * - Instance : Gestion de l'instance singleton
    *
    * @Description :
    * Fournit l'instance unique du MemorySystem avec verrouillage thread-safe.
    * Capture le contexte d'appel (fichier/ligne/fonction) pour le tracking.
    *
    * @param (const char*) file : Fichier source appelant
    * @param (int) line : Ligne d'appel
    * @param (const char*) func : Fonction appelante
    * @return (MemorySystem&) : Référence à l'instance unique
    */
    MemorySystem& MemorySystem::Instance(const char* file, int line, const char* func) {
        static MemorySystem instance;
        std::lock_guard<std::mutex> lock(instance.mutex);
        instance.currentFile = file;
        instance.currentLine = line;
        instance.currentFunc = func;
        return instance;
    }

    /**
    * - Initialize : Initialisation du système
    * @return (bool) : Toujours true (réservé pour extensions futures)
    */
    bool MemorySystem::Initialize() { return true; }

    /**
    * - Shutdown : Arrêt contrôlé du système
    *
    * @Description :
    * Génère un rapport des fuites mémoire et nettoie les ressources résiduelles.
    * Appelé automatiquement à la fin du programme.
    *
    * @return (bool) : true si succès, false si fuites détectées
    */
    bool MemorySystem::Shutdown() {
        std::lock_guard<std::mutex> lock(mutex);
        ReportLeaks();
        allocations.clear();
        return true;
    }

    /**
    * - RegisterAllocation : Enregistrement d'une allocation
    *
    * @Description :
    * Stocke les métadonnées d'allocation dans une map thread-safe
    *
    * @param (void*) ptr : Adresse mémoire allouée
    * @param (const AllocationSystemInfo&) info : Métadonnées de tracking
    */
    MemorySystem::AllocationSystemInfo& MemorySystem::RegisterAllocation(void* ptr, const AllocationSystemInfo& info) {
        std::lock_guard<std::mutex> lock(mutex);
        allocations[ptr] = info;
        return allocations[ptr];
    }

    /**
    * - ReportLeaks : Rapport des fuites mémoire
    *
    * @Description :
    * Loggue toutes les allocations non libérées avec leurs métadonnées.
    * Appelé automatiquement pendant le Shutdown().
    */
    void MemorySystem::ReportLeaks() {
        if (allocations.empty()) return;

        logger.Debug("*** MEMORY LEAK(S) DETECTED: {0} ***\n", allocations.size());
        for (auto& [ptr, info] : allocations) {
            logger.Debug("[Leak] {0}", info.ToString(ptr));
            // Libération optionnelle pour nettoyage final
            info.deleter(ptr);
            info.isValid = false;
        }
        allocations.clear();
    }

    /**
    * - ToString : Formattage des informations d'allocation
    *
    * @Description :
    * Produit une chaîne lisible contenant toutes les métadonnées pertinentes
    *
    * @param (const void*) ptr : Adresse mémoire à décrire
    * @return (std::string) : Description formatée
    */
    std::string MemorySystem::AllocationSystemInfo::ToString(const void* ptr) const {
        std::ostringstream oss;
        oss << name
            << " | Location: " << filename << ":" << line
            << " | Function: " << function
            << " | Address: " << ((ptr == nullptr) ? "empty" : ptr)
            << " | Count: " << elementCount
            << " | Type: " << (isArray ? "Array" : "Object");
        return oss.str();
    }
}

// Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
// Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
// © Rihen 2025 - Tous droits réservés.