/**
* @File NkMemory.cpp
* @Description Implémentation du système de gestion mémoire
*
* @Fonctionnalités :
* - Implémentation des méthodes du singleton NkMemorySystem
* - Tracking des allocations en temps réel
* - Génération de rapports de fuites
* - Intégration avec le système de logging
*/

#include "pch.h"
#include "NkMemory.h"
#include "Nkentseu/Logger/Logger.h"

namespace nkentseu {
    usize NkMemorySystem::NkAllocationInfo::counter = 0;
    std::mutex NkMemorySystem::NkAllocationInfo::counterMutex = std::mutex();
    /**
    * - Instance : Gestion de l'instance singleton
    *
    * @Description :
    * Fournit l'instance unique du NkMemorySystem avec verrouillage thread-safe.
    * Capture le contexte d'appel (fichier/ligne/fonction) pour le tracking.
    *
    * @param (const char*) file : Fichier source appelant
    * @param (int) line : Ligne d'appel
    * @param (const char*) func : Fonction appelante
    * @return (NkMemorySystem&) : Référence à l'instance unique
    */
    NkMemorySystem& NkMemorySystem::Instance(const char* file, int line, const char* func) {
        static NkMemorySystem instance;
        std::lock_guard<std::mutex> lock(instance.mMutex);
        instance.mCurrentFile = file;
        instance.mCurrentLine = line;
        instance.mCurrentFunc = func;
        return instance;
    }

    /**
    * - Initialize : Initialisation du système
    * @return (bool) : Toujours true (réservé pour extensions futures)
    */
    bool NkMemorySystem::Initialize() { return true; }

    /**
    * - Shutdown : Arrêt contrôlé du système
    *
    * @Description :
    * Génère un rapport des fuites mémoire et nettoie les ressources résiduelles.
    * Appelé automatiquement à la fin du programme.
    *
    * @return (bool) : true si succès, false si fuites détectées
    */
    bool NkMemorySystem::Shutdown() {
        std::lock_guard<std::mutex> lock(mMutex);
        ReportLeaks();
        mAllocations.clear();
        return true;
    }

    /**
    * - RegisterAllocation : Enregistrement d'une allocation
    *
    * @Description :
    * Stocke les métadonnées d'allocation dans une map thread-safe
    *
    * @param (void*) ptr : Adresse mémoire allouée
    * @param (const NkAllocationInfo&) info : Métadonnées de tracking
    */
    NkMemorySystem::NkAllocationInfo& NkMemorySystem::RegisterAllocation(void* ptr, const NkAllocationInfo& info) {
        std::lock_guard<std::mutex> lock(mMutex);
        mAllocations[ptr] = info;
        return mAllocations[ptr];
    }

    /**
    * - ReportLeaks : Rapport des fuites mémoire
    *
    * @Description :
    * Loggue toutes les allocations non libérées avec leurs métadonnées.
    * Appelé automatiquement pendant le Shutdown().
    */
    void NkMemorySystem::ReportLeaks() {
        if (mAllocations.empty()) return;

        logger.Debug("*** MEMORY LEAK(S) DETECTED: {0} ***\n", mAllocations.size());
        for (auto& [ptr, info] : mAllocations) {
            logger.Debug("[Leak] {0}", info.ToString(ptr));
            // Libération optionnelle pour nettoyage final
            info.deleter(ptr);
            info.isValid = false;
        }
        mAllocations.clear();
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
    std::string NkMemorySystem::NkAllocationInfo::ToString(const void* ptr) const {
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
