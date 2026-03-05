#pragma once

// =============================================================================
// NkGamepadMappingPersistence.h
// Persistance des profils de mapping gamepad.
//
// Objectif:
//   - Permettre un stockage utilisateur (per-user).
//   - Permettre plusieurs formats (texte, JSON, YAML, format propriétaire).
//   - L'application peut brancher sa propre implémentation via interface.
// =============================================================================

#include "NkGamepadEvent.h"

#include <string>
#include <vector>
#include <memory>

namespace nkentseu {

    inline constexpr NkU32 NK_GAMEPAD_MAPPING_UNMAPPED = 0xFFFFFFFFu;

    struct NkGamepadButtonMapEntry {
        NkU32 physicalButton = 0;
        NkU32 logicalButton  = 0;
    };

    struct NkGamepadAxisMapEntry {
        NkU32 physicalAxis = 0;
        NkU32 logicalAxis  = NK_GAMEPAD_MAPPING_UNMAPPED;
        NkF32 scale        = 1.f;
        bool  invert       = false;
    };

    struct NkGamepadMappingSlotData {
        NkU32 slotIndex = 0;
        bool  active    = false;
        std::vector<NkGamepadButtonMapEntry> buttons;
        std::vector<NkGamepadAxisMapEntry>   axes;
    };

    struct NkGamepadMappingProfileData {
        NkU32 version = 1;
        std::string backendName;
        std::vector<NkGamepadMappingSlotData> slots;
    };

    class NkIGamepadMappingPersistence {
    public:
        virtual ~NkIGamepadMappingPersistence() = default;

        virtual const char* GetFormatName() const noexcept = 0;

        virtual bool Save(const std::string& userId,
                          const NkGamepadMappingProfileData& profile,
                          std::string* outError) = 0;

        virtual bool Load(const std::string& userId,
                          NkGamepadMappingProfileData& outProfile,
                          std::string* outError) = 0;
    };

    // Backend texte simple par défaut (lisible et sans dépendance externe).
    // Les applications peuvent le remplacer par JSON/YAML/custom.
    class NkTextGamepadMappingPersistence final : public NkIGamepadMappingPersistence {
    public:
        explicit NkTextGamepadMappingPersistence(std::string baseDirectory = {},
                                                 std::string fileExtension = ".nkmap");

        const char* GetFormatName() const noexcept override { return "text/nkmap"; }

        bool Save(const std::string& userId,
                  const NkGamepadMappingProfileData& profile,
                  std::string* outError) override;

        bool Load(const std::string& userId,
                  NkGamepadMappingProfileData& outProfile,
                  std::string* outError) override;

        void SetBaseDirectory(const std::string& dir) { mBaseDirectory = dir; }
        const std::string& GetBaseDirectory() const noexcept { return mBaseDirectory; }

        static std::string ResolveDefaultBaseDirectory();
        static std::string ResolveCurrentUserId();
        static std::string SanitizeUserId(const std::string& raw);

    private:
        std::string BuildUserFilePath(const std::string& userId) const;

        std::string mBaseDirectory;
        std::string mExtension;
    };

} // namespace nkentseu
