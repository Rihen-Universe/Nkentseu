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
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {

    inline constexpr uint32 NK_GAMEPAD_MAPPING_UNMAPPED = 0xFFFFFFFFu;

    struct NkGamepadButtonMapEntry {
        uint32 physicalButton = 0;
        uint32 logicalButton  = 0;
    };

    struct NkGamepadAxisMapEntry {
        uint32 physicalAxis = 0;
        uint32 logicalAxis  = NK_GAMEPAD_MAPPING_UNMAPPED;
        float32 scale        = 1.f;
        bool  invert       = false;
    };

    struct NkGamepadMappingSlotData {
        uint32 slotIndex = 0;
        bool  active    = false;
        NkVector<NkGamepadButtonMapEntry> buttons;
        NkVector<NkGamepadAxisMapEntry>   axes;
    };

    struct NkGamepadMappingProfileData {
        uint32 version = 1;
        NkString backendName;
        NkVector<NkGamepadMappingSlotData> slots;
    };

    class NkIGamepadMappingPersistence {
    public:
        virtual ~NkIGamepadMappingPersistence() = default;

        virtual const char* GetFormatName() const noexcept = 0;

        virtual bool Save(const NkString& userId,
                          const NkGamepadMappingProfileData& profile,
                          NkString* outError) = 0;

        virtual bool Load(const NkString& userId,
                          NkGamepadMappingProfileData& outProfile,
                          NkString* outError) = 0;
    };

    // Backend texte simple par défaut (lisible et sans dépendance externe).
    // Les applications peuvent le remplacer par JSON/YAML/custom.
    class NkTextGamepadMappingPersistence final : public NkIGamepadMappingPersistence {
    public:
        explicit NkTextGamepadMappingPersistence(NkString baseDirectory = {},
                                                 NkString fileExtension = ".nkmap");

        const char* GetFormatName() const noexcept override { return "text/nkmap"; }

        bool Save(const NkString& userId,
                  const NkGamepadMappingProfileData& profile,
                  NkString* outError) override;

        bool Load(const NkString& userId,
                  NkGamepadMappingProfileData& outProfile,
                  NkString* outError) override;

        void SetBaseDirectory(const NkString& dir) { mBaseDirectory = dir; }
        const NkString& GetBaseDirectory() const noexcept { return mBaseDirectory; }

        static NkString ResolveDefaultBaseDirectory();
        static NkString ResolveCurrentUserId();
        static NkString SanitizeUserId(const NkString& raw);

    private:
        NkString BuildUserFilePath(const NkString& userId) const;

        NkString mBaseDirectory;
        NkString mExtension;
    };

} // namespace nkentseu
