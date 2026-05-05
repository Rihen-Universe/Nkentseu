#ifndef NKENTSEU_EVENT_NKGAMEPADMAPPINGPERSISTENCE_H
#define NKENTSEU_EVENT_NKGAMEPADMAPPINGPERSISTENCE_H

#pragma once

// =============================================================================
// Fichier : NkGamepadMappingPersistence.h
// =============================================================================
// Description :
//   Interface et implémentation de persistance pour les profils de mapping 
//   de manettes de jeu au sein du moteur NkEntseu.
//
// Objectifs :
//   - Permettre un stockage des configurations utilisateur (per-user).
//   - Supporter plusieurs formats de sérialisation (texte, JSON, YAML, binaire).
//   - Fournir une interface abstraite pour extension par l'application cliente.
//   - Offrir un backend texte simple par défaut, lisible et sans dépendances.
//
// Responsabilités :
//   - Sérialisation/désérialisation des mappings boutons et axes.
//   - Gestion des répertoires de stockage par plateforme (Windows/Linux/macOS).
//   - Sanitisation des identifiants utilisateur pour la sécurité des chemins.
//   - Gestion robuste des erreurs d'E/S avec messages explicites.
//
// Structures définies :
//   - NkGamepadButtonMapEntry : Association bouton physique → logique
//   - NkGamepadAxisMapEntry   : Association axe physique → logique avec transformations
//   - NkGamepadMappingSlotData  : Données de mapping pour un slot joueur
//   - NkGamepadMappingProfileData : Profil complet avec version et backend
//
// Classes définies :
//   - NkIGamepadMappingPersistence    : Interface abstraite pour persistance
//   - NkTextGamepadMappingPersistence : Implémentation texte par défaut
//
// Auteur : Équipe NkEntseu
// Version : 1.0.0
// Date : 2026
// =============================================================================

    #include "NkGamepadEvent.h"
    #include "NKContainers/String/NkString.h"
    #include "NKContainers/Sequential/NkVector.h"

    namespace nkentseu {
        // =========================================================================
        // Section : Constantes et types de base pour le mapping gamepad
        // =========================================================================

        // -------------------------------------------------------------------------
        // Constante : NK_GAMEPAD_MAPPING_UNMAPPED
        // -------------------------------------------------------------------------
        // Description :
        //   Valeur sentinelle indiquant qu'un indice physique n'est pas mappé 
        //   à un indice logique (désactivation explicite ou absence de mapping).
        // Utilisation :
        //   - Comparaison avec les champs logicalButton/logicalAxis.
        //   - Configuration pour ignorer un contrôle physique spécifique.
        // -------------------------------------------------------------------------
        inline constexpr uint32 NK_GAMEPAD_MAPPING_UNMAPPED = 0xFFFFFFFFu;

        // -------------------------------------------------------------------------
        // Structure : NkGamepadButtonMapEntry
        // -------------------------------------------------------------------------
        // Description :
        //   Association simple entre un indice de bouton physique et un indice 
        //   logique pour la personnalisation des contrôles.
        // Champs :
        //   - physicalButton : Index du bouton sur le périphérique hardware (0-based).
        //   - logicalButton  : Index logique applicatif cible (ou UNMAPPED pour désactiver).
        // Utilisation :
        //   - Stockage dans NkGamepadMappingSlotData pour un slot joueur donné.
        //   - Chargement/sauvegarde via les backends de persistance.
        // -------------------------------------------------------------------------
        struct NkGamepadButtonMapEntry {
            uint32 physicalButton = 0;
            uint32 logicalButton = 0;
        };

        // -------------------------------------------------------------------------
        // Structure : NkGamepadAxisMapEntry
        // -------------------------------------------------------------------------
        // Description :
        //   Association entre un axe physique et un axe logique avec paramètres 
        //   de transformation optionnels pour l'adaptation du comportement.
        // Champs :
        //   - physicalAxis : Index de l'axe sur le périphérique hardware (0-based).
        //   - logicalAxis  : Index logique applicatif cible (ou UNMAPPED pour désactiver).
        //   - scale        : Facteur de multiplication appliqué à la valeur brute (défaut: 1.0).
        //   - invert       : Inversion du signe de la valeur avant application (défaut: false).
        // Ordre d'application des transformations :
        //   1. Inversion : v = -v (si invert=true)
        //   2. Scaling : v = v * scale
        //   3. Clamping : v = clamp(v, -1.0, +1.0) ou [0,1] selon l'axe
        // Utilisation :
        //   - Adaptation de la sensibilité des sticks ou gâchettes.
        //   - Inversion des axes Y pour compatibilité avec certaines préférences.
        // -------------------------------------------------------------------------
        struct NkGamepadAxisMapEntry {
            uint32 physicalAxis = 0;
            uint32 logicalAxis = NK_GAMEPAD_MAPPING_UNMAPPED;
            float32 scale = 1.f;
            bool invert = false;
        };

        // -------------------------------------------------------------------------
        // Structure : NkGamepadMappingSlotData
        // -------------------------------------------------------------------------
        // Description :
        //   Conteneur des mappings de boutons et axes pour un slot joueur unique.
        //   Permet de gérer plusieurs configurations par profil utilisateur.
        // Champs :
        //   - slotIndex : Identifiant du slot joueur (0 = joueur 1, etc.).
        //   - active    : Indicateur d'activation du slot (true = utilisé).
        //   - buttons   : Liste des associations bouton physique → logique.
        //   - axes      : Liste des associations axe physique → logique avec transformations.
        // Utilisation :
        //   - Regroupement des mappings par joueur dans un profil.
        //   - Activation/désactivation dynamique de configurations.
        // -------------------------------------------------------------------------
        struct NkGamepadMappingSlotData {
            uint32 slotIndex = 0;
            bool active = false;
            NkVector<NkGamepadButtonMapEntry> buttons;
            NkVector<NkGamepadAxisMapEntry> axes;
        };

        // -------------------------------------------------------------------------
        // Structure : NkGamepadMappingProfileData
        // -------------------------------------------------------------------------
        // Description :
        //   Profil complet de mapping gamepad avec métadonnées de version et 
        //   d'identification du backend utilisé pour la sérialisation.
        // Champs :
        //   - version     : Numéro de version du format de profil (pour migration).
        //   - backendName : Nom du backend de persistance ayant généré ce profil.
        //   - slots       : Collection des slots joueurs avec leurs mappings respectifs.
        // Gestion de version :
        //   - Permet la migration ascendante des profils entre versions du moteur.
        //   - Les anciens profils peuvent être chargés et mis à jour automatiquement.
        // -------------------------------------------------------------------------
        struct NkGamepadMappingProfileData {
            uint32 version = 1;
            NkString backendName;
            NkVector<NkGamepadMappingSlotData> slots;
        };

        // =========================================================================
        // Section : Interface abstraite pour la persistance des mappings
        // =========================================================================

        // -------------------------------------------------------------------------
        // Classe : NkIGamepadMappingPersistence
        // -------------------------------------------------------------------------
        // Description :
        //   Interface abstraite définissant le contrat pour les backends de 
        //   persistance des profils de mapping gamepad.
        // Responsabilités :
        //   - Fournir un point d'extension pour formats personnalisés (JSON, YAML...).
        //   - Isoler la logique de sérialisation du reste du système d'entrée.
        //   - Permettre le remplacement dynamique du backend à l'exécution.
        // Méthodes pures virtuelles :
        //   - GetFormatName() : Retourne l'identifiant du format supporté.
        //   - Save() : Sérialise un profil vers le stockage persistant.
        //   - Load() : Désérialise un profil depuis le stockage persistant.
        // Utilisation :
        //   - Hériter de cette classe pour implémenter un backend personnalisé.
        //   - Injecter l'implémentation via dépendance dans le système d'entrée.
        // -------------------------------------------------------------------------
        class NkIGamepadMappingPersistence {
            public:
                virtual ~NkIGamepadMappingPersistence() = default;

                virtual const char* GetFormatName() const noexcept = 0;

                virtual bool Save(
                    const NkString& userId,
                    const NkGamepadMappingProfileData& profile,
                    NkString* outError
                ) = 0;

                virtual bool Load(
                    const NkString& userId,
                    NkGamepadMappingProfileData& outProfile,
                    NkString* outError
                ) = 0;
        };

        // =========================================================================
        // Section : Implémentation texte par défaut pour la persistance
        // =========================================================================

        // -------------------------------------------------------------------------
        // Classe : NkTextGamepadMappingPersistence
        // -------------------------------------------------------------------------
        // Description :
        //   Implémentation de référence utilisant un format texte simple, lisible 
        //   et sans dépendances externes pour la persistance des mappings.
        // Format de fichier :
        //   - En-tête : "nkmap <version>" suivi de "backend <nom>"
        //   - Blocs slot : "slot <index> <active>" ... "end_slot"
        //   - Entrées bouton : "button <physique> <logique>"
        //   - Entrées axe : "axis <physique> <logique> <scale> <invert>"
        //   - Commentaires : lignes commençant par '#' ignorées (futur)
        // Gestion des chemins :
        //   - BaseDirectory configurable via constructeur ou setter.
        //   - Résolution automatique des répertoires par plateforme.
        //   - Création automatique des répertoires intermédiaires si nécessaire.
        // Sécurité :
        //   - Sanitisation des userId pour éviter l'injection de chemins.
        //   - Validation des tokens numériques avant conversion.
        // -------------------------------------------------------------------------
        class NkTextGamepadMappingPersistence final : public NkIGamepadMappingPersistence {
            public:
                explicit NkTextGamepadMappingPersistence(
                    NkString baseDirectory = {},
                    NkString fileExtension = ".nkmap"
                );

                const char* GetFormatName() const noexcept override {
                    return "text/nkmap";
                }

                bool Save(
                    const NkString& userId,
                    const NkGamepadMappingProfileData& profile,
                    NkString* outError
                ) override;

                bool Load(
                    const NkString& userId,
                    NkGamepadMappingProfileData& outProfile,
                    NkString* outError
                ) override;

                void SetBaseDirectory(const NkString& dir) {
                    mBaseDirectory = dir;
                }

                const NkString& GetBaseDirectory() const noexcept {
                    return mBaseDirectory;
                }

                static NkString ResolveDefaultBaseDirectory();

                static NkString ResolveCurrentUserId();

                static NkString SanitizeUserId(const NkString& raw);

            private:
                NkString BuildUserFilePath(const NkString& userId) const;

                NkString mBaseDirectory;
                NkString mExtension;
        };

    } // namespace nkentseu

#endif // NKENTSEU_EVENT_NKGAMEPADMAPPINGPERSISTENCE_H

// =============================================================================
// Section : Exemples d'utilisation (à titre informatif - en commentaire)
// =============================================================================

/*
// -----------------------------------------------------------------------------
// Exemple 1 : Initialisation du backend de persistance texte
// -----------------------------------------------------------------------------
void InitializeGamepadMappingPersistence() {
    // Création du backend avec répertoire par défaut
    auto persistence = std::make_unique<nkentseu::NkTextGamepadMappingPersistence>();

    // Optionnel : personnalisation du répertoire de stockage
    // persistence->SetBaseDirectory("/home/user/mygame/configs");

    // Enregistrement dans le système d'entrée global
    // InputSystem::SetMappingPersistence(std::move(persistence));

    NK_LOG_INFO("Backend de persistance gamepad initialisé");
}

// -----------------------------------------------------------------------------
// Exemple 2 : Sauvegarde d'un profil de mapping utilisateur
// -----------------------------------------------------------------------------
bool SaveUserGamepadProfile(const NkString& userId, const nkentseu::NkGamepadMappingProfileData& profile) {
    // Récupération du backend configuré
    // auto* persistence = InputSystem::GetMappingPersistence();
    // if (!persistence) {
    //     NK_LOG_ERROR("Aucun backend de persistance configuré");
    //     return false;
    // }

    // Tentative de sauvegarde avec gestion d'erreur
    nkentseu::NkString error;
    // bool success = persistence->Save(userId, profile, &error);

    // if (!success) {
    //     NK_LOG_ERROR("Échec de sauvegarde du profil : {}", error);
    //     // UIManager::ShowError("Impossible de sauvegarder les contrôles");
    //     return false;
    // }

    // NK_LOG_INFO("Profil gamepad sauvegardé pour utilisateur : {}", userId);
    return true;
}

// -----------------------------------------------------------------------------
// Exemple 3 : Chargement d'un profil de mapping utilisateur
// -----------------------------------------------------------------------------
bool LoadUserGamepadProfile(const NkString& userId, nkentseu::NkGamepadMappingProfileData& outProfile) {
    // Récupération du backend configuré
    // auto* persistence = InputSystem::GetMappingPersistence();
    // if (!persistence) {
    //     NK_LOG_WARN("Aucun backend de persistance - utilisation des défauts");
    //     return false;
    // }

    // Tentative de chargement avec gestion d'erreur optionnelle
    nkentseu::NkString error;
    // bool success = persistence->Load(userId, outProfile, &error);

    // if (!success) {
    //     // Fichier non trouvé ou corrompu : retour aux paramètres par défaut
    //     NK_LOG_DEBUG("Profil non trouvé ou invalide : {} (erreur: {})", userId, error);
    //     return false;
    // }

    // Validation de la version pour migration si nécessaire
    // if (outProfile.version < CURRENT_PROFILE_VERSION) {
    //     NK_LOG_INFO("Migration du profil de la version {} vers {}", 
    //         outProfile.version, CURRENT_PROFILE_VERSION);
    //     // MigrateProfile(outProfile);
    // }

    NK_LOG_INFO("Profil gamepad chargé pour utilisateur : {}", userId);
    return true;
}

// -----------------------------------------------------------------------------
// Exemple 4 : Construction d'un profil de mapping personnalisé
// -----------------------------------------------------------------------------
nkentseu::NkGamepadMappingProfileData BuildCustomProfile(uint32 playerSlot) {
    nkentseu::NkGamepadMappingProfileData profile;
    profile.version = 1;
    profile.backendName = "text/nkmap";

    // Configuration du slot joueur
    nkentseu::NkGamepadMappingSlotData slot;
    slot.slotIndex = playerSlot;
    slot.active = true;

    // Mapping des boutons principaux (layout personnalisé)
    slot.buttons.PushBack({0, 10});  // Bouton physique 0 → Action logique "Saut"
    slot.buttons.PushBack({1, 11});  // Bouton physique 1 → Action logique "Tir"
    slot.buttons.PushBack({2, 12});  // Bouton physique 2 → Action logique "Interagir"
    slot.buttons.PushBack({3, 13});  // Bouton physique 3 → Action logique "Capacité spéciale"

    // Désactivation d'un bouton non utilisé
    slot.buttons.PushBack({15, nkentseu::NK_GAMEPAD_MAPPING_UNMAPPED});

    // Mapping des axes avec transformations
    // Stick gauche : mouvement du personnage (sensibilité augmentée)
    slot.axes.PushBack({
        0,  // physicalAxis: LeftX
        20, // logicalAxis: MovementX
        1.2f, // scale: +20% de sensibilité
        false // invert: pas d'inversion
    });

    // Stick droit : caméra (inversion Y pour préférence "flight stick")
    slot.axes.PushBack({
        3,  // physicalAxis: RightY
        23, // logicalAxis: CameraY
        1.0f, // scale: sensibilité normale
        true  // invert: inversion pour contrôle inversé
    });

    // Gâchette droite : accélération avec courbe progressive
    slot.axes.PushBack({
        5,  // physicalAxis: TriggerRight
        25, // logicalAxis: Throttle
        1.5f, // scale: réponse plus sensible en début de course
        false
    });

    // Ajout du slot au profil
    profile.slots.PushBack(slot);

    return profile;
}

// -----------------------------------------------------------------------------
// Exemple 5 : Gestion de la sanitisation des identifiants utilisateur
// -----------------------------------------------------------------------------
void DemonstrateUserIdSanitization() {
    // Cas d'usage : nom d'utilisateur avec caractères spéciaux
    nkentseu::NkString rawUserId = "user@domain.com";
    nkentseu::NkString sanitized = nkentseu::NkTextGamepadMappingPersistence::SanitizeUserId(rawUserId);
    // Résultat attendu : "user_domain_com"

    NK_LOG_DEBUG("UserId sanitisé : '{}' → '{}'", rawUserId, sanitized);

    // Cas d'usage : identifiant vide ou nul
    nkentseu::NkString emptyUserId = "";
    nkentseu::NkString defaultUserId = nkentseu::NkTextGamepadMappingPersistence::SanitizeUserId(emptyUserId);
    // Résultat attendu : "default"

    NK_LOG_DEBUG("UserId vide résolu vers : '{}'", defaultUserId);

    // Utilisation dans un chemin de fichier
    auto persistence = nkentseu::NkTextGamepadMappingPersistence();
    // nkentseu::NkString filePath = persistence.BuildUserFilePath(sanitized);
    // NK_LOG_DEBUG("Chemin de fichier généré : {}", filePath);
}

// -----------------------------------------------------------------------------
// Exemple 6 : Résolution automatique des répertoires par plateforme
// -----------------------------------------------------------------------------
void DemonstrateDirectoryResolution() {
    // Résolution du répertoire par défaut selon la plateforme
    nkentseu::NkString defaultDir = nkentseu::NkTextGamepadMappingPersistence::ResolveDefaultBaseDirectory();
    NK_LOG_INFO("Répertoire par défaut : {}", defaultDir);

    // Résolution de l'identifiant utilisateur courant
    nkentseu::NkString currentUserId = nkentseu::NkTextGamepadMappingPersistence::ResolveCurrentUserId();
    NK_LOG_INFO("Utilisateur courant : {}", currentUserId);

    // Construction du chemin complet pour un utilisateur spécifique
    auto persistence = nkentseu::NkTextGamepadMappingPersistence();
    // nkentseu::NkString userFilePath = persistence.BuildUserFilePath("player1");
    // NK_LOG_DEBUG("Chemin utilisateur : {}", userFilePath);

    // Vérification de l'existence du répertoire (pseudo-code)
    // if (!nkentseu::FileSystem::DirectoryExists(defaultDir)) {
    //     NK_LOG_WARN("Répertoire de configuration non trouvé, création automatique...");
    //     // nkentseu::FileSystem::CreateDirectories(defaultDir);
    // }
}

// -----------------------------------------------------------------------------
// Exemple 7 : Implémentation d'un backend JSON personnalisé
// -----------------------------------------------------------------------------
class NkJsonGamepadMappingPersistence final : public nkentseu::NkIGamepadMappingPersistence {
public:
    const char* GetFormatName() const noexcept override {
        return "application/json";
    }

    bool Save(
        const nkentseu::NkString& userId,
        const nkentseu::NkGamepadMappingProfileData& profile,
        nkentseu::NkString* outError
    ) override {
        // Pseudo-code de sérialisation JSON
        // auto jsonDoc = JsonDocument::Create();
        // jsonDoc.Set("version", profile.version);
        // jsonDoc.Set("backend", profile.backendName);
        // 
        // auto slotsArray = jsonDoc.CreateArray("slots");
        // for (const auto& slot : profile.slots) {
        //     auto slotObj = slotsArray.CreateObject();
        //     slotObj.Set("index", slot.slotIndex);
        //     slotObj.Set("active", slot.active);
        //     // ... sérialisation des boutons et axes
        // }
        //
        // nkentseu::NkString filePath = BuildUserFilePath(userId);
        // return jsonDoc.SaveToFile(filePath, outError);
        
        return true; // Placeholder
    }

    bool Load(
        const nkentseu::NkString& userId,
        nkentseu::NkGamepadMappingProfileData& outProfile,
        nkentseu::NkString* outError
    ) override {
        // Pseudo-code de désérialisation JSON
        // nkentseu::NkString filePath = BuildUserFilePath(userId);
        // auto jsonDoc = JsonDocument::LoadFromFile(filePath, outError);
        // if (!jsonDoc.IsValid()) {
        //     return false;
        // }
        //
        // outProfile.version = jsonDoc.GetUInt("version", 1);
        // outProfile.backendName = jsonDoc.GetString("backend", "");
        // // ... désérialisation des slots, boutons et axes
        //
        // return true;
        
        return true; // Placeholder
    }

private:
    nkentseu::NkString BuildUserFilePath(const nkentseu::NkString& userId) const {
        // Logique de construction de chemin similaire au backend texte
        return {}; // Placeholder
    }
};

// -----------------------------------------------------------------------------
// Exemple 8 : Migration de profil entre versions
// -----------------------------------------------------------------------------
bool MigrateProfile(nkentseu::NkGamepadMappingProfileData& profile, uint32 targetVersion) {
    if (profile.version >= targetVersion) {
        return true; // Déjà à jour
    }

    NK_LOG_INFO("Migration du profil : v{} → v{}", profile.version, targetVersion);

    // Exemple de migration v1 → v2 : ajout d'un champ par défaut
    if (profile.version == 1 && targetVersion >= 2) {
        for (auto& slot : profile.slots) {
            // Initialisation de nouveaux champs avec valeurs par défaut
            // slot.newFeatureEnabled = false;
        }
        profile.version = 2;
    }

    // Chaînage des migrations successives
    if (profile.version < targetVersion) {
        return MigrateProfile(profile, targetVersion);
    }

    return true;
}

// -----------------------------------------------------------------------------
// Exemple 9 : Test unitaire de sauvegarde/chargement
// -----------------------------------------------------------------------------
void TestGamepadMappingPersistence() {
    // Configuration du test
    constexpr nkentseu::NkString TEST_USER_ID = "test_user_123";
    auto persistence = nkentseu::NkTextGamepadMappingPersistence(
        "Build/TestOutput/GamepadMappings",
        ".testmap"
    );

    // Création d'un profil de test
    nkentseu::NkGamepadMappingProfileData originalProfile;
    originalProfile.version = 1;
    originalProfile.backendName = "text/nkmap";

    nkentseu::NkGamepadMappingSlotData testSlot;
    testSlot.slotIndex = 0;
    testSlot.active = true;
    testSlot.buttons.PushBack({0, 10});
    testSlot.buttons.PushBack({1, nkentseu::NK_GAMEPAD_MAPPING_UNMAPPED});
    testSlot.axes.PushBack({0, 20, 1.5f, true});
    originalProfile.slots.PushBack(testSlot);

    // Test de sauvegarde
    nkentseu::NkString saveError;
    // bool saveSuccess = persistence.Save(TEST_USER_ID, originalProfile, &saveError);
    // NK_ASSERT(saveSuccess);
    // NK_ASSERT(saveError.Empty());

    // Test de chargement
    nkentseu::NkGamepadMappingProfileData loadedProfile;
    nkentseu::NkString loadError;
    // bool loadSuccess = persistence.Load(TEST_USER_ID, loadedProfile, &loadError);
    // NK_ASSERT(loadSuccess);
    // NK_ASSERT(loadError.Empty());

    // Validation des données chargées
    // NK_ASSERT(loadedProfile.version == originalProfile.version);
    // NK_ASSERT(loadedProfile.backendName == originalProfile.backendName);
    // NK_ASSERT(loadedProfile.slots.Size() == originalProfile.slots.Size());

    // if (!loadedProfile.slots.Empty()) {
    //     const auto& loadedSlot = loadedProfile.slots[0];
    //     const auto& originalSlot = originalProfile.slots[0];
    //     
    //     NK_ASSERT(loadedSlot.slotIndex == originalSlot.slotIndex);
    //     NK_ASSERT(loadedSlot.active == originalSlot.active);
    //     NK_ASSERT(loadedSlot.buttons.Size() == originalSlot.buttons.Size());
    //     NK_ASSERT(loadedSlot.axes.Size() == originalSlot.axes.Size());
    // }

    NK_LOG_INFO("Tests de persistance gamepad passés avec succès");
}

// -----------------------------------------------------------------------------
// Exemple 10 : Intégration dans un système de configuration utilisateur
// -----------------------------------------------------------------------------
class UserGamepadConfigManager {
public:
    void Initialize() {
        // Initialisation du backend de persistance
        mPersistence = std::make_unique<nkentseu::NkTextGamepadMappingPersistence>();

        // Chargement du profil de l'utilisateur courant
        nkentseu::NkString userId = nkentseu::NkTextGamepadMappingPersistence::ResolveCurrentUserId();
        nkentseu::NkGamepadMappingProfileData profile;
        
        // if (mPersistence->Load(userId, profile, nullptr)) {
        //     ApplyProfileToInputSystem(profile);
        //     NK_LOG_INFO("Configuration gamepad chargée pour : {}", userId);
        // } else {
        //     NK_LOG_INFO("Aucune configuration trouvée, utilisation des défauts");
        //     LoadDefaultProfile();
        // }
    }

    void SaveCurrentConfiguration() {
        // nkentseu::NkString userId = nkentseu::NkTextGamepadMappingPersistence::ResolveCurrentUserId();
        // auto currentProfile = ExtractProfileFromInputSystem();
        
        // if (mPersistence->Save(userId, currentProfile, nullptr)) {
        //     NK_LOG_INFO("Configuration gamepad sauvegardée");
        //     // UIManager::ShowNotification("Paramètres sauvegardés");
        // } else {
        //     NK_LOG_ERROR("Échec de sauvegarde de la configuration");
        //     // UIManager::ShowError("Impossible de sauvegarder");
        // }
    }

    void ResetToDefaults() {
        // nkentseu::NkString userId = nkentseu::NkTextGamepadMappingPersistence::ResolveCurrentUserId();
        // nkentseu::NkString filePath = mPersistence->BuildUserFilePath(userId);
        
        // if (nkentseu::FileSystem::FileExists(filePath)) {
        //     // nkentseu::FileSystem::DeleteFile(filePath);
        //     NK_LOG_INFO("Configuration réinitialisée pour : {}", userId);
        // }
        
        // LoadDefaultProfile();
    }

private:
    void ApplyProfileToInputSystem(const nkentseu::NkGamepadMappingProfileData& profile) {
        // Application des mappings au système d'entrée
        // for (const auto& slot : profile.slots) {
        //     if (!slot.active) continue;
        //     
        //     for (const auto& btn : slot.buttons) {
        //         if (btn.logicalButton != nkentseu::NK_GAMEPAD_MAPPING_UNMAPPED) {
        //             // InputMapper::SetButtonMapping(slot.slotIndex, btn.physicalButton, btn.logicalButton);
        //         }
        //     }
        //     
        //     for (const auto& axis : slot.axes) {
        //         if (axis.logicalAxis != nkentseu::NK_GAMEPAD_MAPPING_UNMAPPED) {
        //             // InputMapper::SetAxisMapping(slot.slotIndex, axis.physicalAxis, 
        //             //     axis.logicalAxis, axis.scale, axis.invert);
        //         }
        //     }
        // }
    }

    void LoadDefaultProfile() {
        // Chargement des mappings par défaut du moteur
        // auto defaultProfile = GamepadDefaults::GetFactoryProfile();
        // ApplyProfileToInputSystem(defaultProfile);
    }

    nkentseu::NkGamepadMappingProfileData ExtractProfileFromInputSystem() {
        // Extraction de la configuration courante pour sauvegarde
        nkentseu::NkGamepadMappingProfileData profile;
        profile.version = 1;
        profile.backendName = mPersistence->GetFormatName();
        // profile.slots = InputMapper::ExportAllSlots();
        return profile;
    }

    std::unique_ptr<nkentseu::NkIGamepadMappingPersistence> mPersistence;
};
*/