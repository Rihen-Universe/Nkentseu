// =============================================================================
// FICHIER  : Modules/System/NKReflection/src/NKReflection/NkReflection.cpp
// MODULE   : NKReflection
// AUTEUR   : Rihen
// DATE     : 2026-02-07
// VERSION  : 1.0.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Implementation des fonctions utilitaires de haut niveau du module NKReflection.
//   Ce fichier sert de point d'entree pour l'initialisation et les operations
//   globales du systeme de reflexion.
// =============================================================================

#include "NKReflection/NkReflection.h"

// -------------------------------------------------------------------------
// DEPENDANCES INTERNES
// -------------------------------------------------------------------------
// Inclusion des composants internes pour l'implementation des utilitaires.

#include "NKLogger/NkLog.h"

// -------------------------------------------------------------------------
// ESPACE DE NOMS PRINCIPAL
// -------------------------------------------------------------------------

namespace nkentseu {

    namespace reflection {

        // =====================================================================
        // IMPLEMENTATION : INITIALISATION / FINALISATION
        // =====================================================================

        nk_bool Initialize() {
            // Le registre s'initialise automatiquement via Meyer's Singleton.
            // Cette fonction est fournie pour :
            // 1. Forcer l'initialisation explicite si besoin
            // 2. Pre-enregistrer des types systemes
            // 3. Configurer des callbacks globaux
            // 4. Valider l'integrite au startup

            auto& registry = NkRegistry::Get();

            // Pre-enregistrement des types primitifs si necessaire
            // (optionnel, car NkTypeOf<T>() les cree a la demande)

            // Validation optionnelle en mode debug
            #ifdef NKENTSEU_DEBUG
            if (!ValidateRegistry()) {
                logger.Errorf("Reflection registry validation failed at startup");
                return false;
            }
            #endif

            logger.Debugf("NKReflection module initialized");
            return true;
        }

        void Shutdown() {
            // Nettoyage optionnel : le registre se detruit automatiquement
            // a la fin du programme via les destructeurs de static.

            // Si des callbacks allouent des ressources, les liberer ici :
            auto& registry = NkRegistry::Get();
            registry.SetOnRegisterCallback(NkRegistry::OnRegisterCallback());

            logger.Debugf("NKReflection module shutdown");
        }

        // =====================================================================
        // IMPLEMENTATION : ACCES RAPIDE AUX META-DONNEES
        // =====================================================================

        const NkClass* FindClass(const nk_char* className) {
            return NkRegistry::Get().FindClass(className);
        }

        const NkType* FindType(const nk_char* typeName) {
            return NkRegistry::Get().FindType(typeName);
        }

        // =====================================================================
        // IMPLEMENTATION : CREATION / DESTRUCTION D'INSTANCES
        // =====================================================================

        void* CreateInstanceByName(const nk_char* className) {
            const NkClass* classMeta = NkRegistry::Get().FindClass(className);
            if (!classMeta || !classMeta->HasConstructor()) {
                return nullptr;
            }
            return classMeta->CreateInstance();
        }

        void DestroyInstanceByName(const nk_char* className, void* instance) {
            if (!instance) {
                return;
            }
            const NkClass* classMeta = NkRegistry::Get().FindClass(className);
            if (classMeta && classMeta->HasDestructor()) {
                classMeta->DestroyInstance(instance);
            }
            // Note : la deallocation memoire est a la charge de l'appelant
            // si le destructeur reflexif ne la gere pas
        }

        // =====================================================================
        // IMPLEMENTATION : INSPECTION ET DEBUGGING
        // =====================================================================

        nk_usize GetRegisteredClassCount() {
            return NkRegistry::Get().GetClassCount();
        }

        nk_usize GetRegisteredTypeCount() {
            return NkRegistry::Get().GetTypeCount();
        }

        void DumpRegistry() {
            const auto& registry = NkRegistry::Get();

            logger.Infof("=== NKReflection Registry Dump ===");
            logger.Infof("Types: %zu / %zu",
                registry.GetTypeCount(),
                static_cast<nk_usize>(512)); // NK_MAX_TYPES
            logger.Infof("Classes: %zu / %zu",
                registry.GetClassCount(),
                static_cast<nk_usize>(512)); // NK_MAX_CLASSES

            // Dump des classes
            for (nk_usize i = 0; i < registry.GetClassCount(); ++i) {
                const auto* cls = registry.GetClassAt(i);
                if (cls) {
                    logger.Infof("Class[%zu]: %s (size=%zu, base=%s, props=%zu, methods=%zu)",
                        i,
                        cls->GetName(),
                        cls->GetSize(),
                        cls->GetBaseClass() ? cls->GetBaseClass()->GetName() : "none",
                        cls->GetPropertyCount(),
                        cls->GetMethodCount());
                }
            }

            logger.Infof("=== End Registry Dump ===");
        }

        nk_bool ValidateRegistry() {
            const auto& registry = NkRegistry::Get();
            nk_bool allValid = true;

            // Validation des types
            for (nk_usize i = 0; i < registry.GetTypeCount(); ++i) {
                const auto* type = registry.GetTypeAt(i);
                if (!type || !type->GetName() || type->GetName()[0] == '\0') {
                    logger.Errorf("Invalid type at registry index %zu", i);
                    allValid = false;
                }
            }

            // Validation des classes
            for (nk_usize i = 0; i < registry.GetClassCount(); ++i) {
                const auto* cls = registry.GetClassAt(i);
                if (!cls || !cls->GetName() || cls->GetName()[0] == '\0') {
                    logger.Errorf("Invalid class at registry index %zu", i);
                    allValid = false;
                    continue;
                }

                // Detection de cycles d'heritage (limitee a 128 niveaux)
                const NkClass* cursor = cls;
                nk_usize depth = 0;
                while (cursor && depth < 128) {
                    cursor = cursor->GetBaseClass();
                    ++depth;
                }
                if (depth >= 128) {
                    logger.Errorf("Possible inheritance cycle involving class '%s'", cls->GetName());
                    allValid = false;
                }
            }

            return allValid;
        }

    } // namespace reflection

} // namespace nkentseu

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================