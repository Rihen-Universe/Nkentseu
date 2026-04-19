#pragma once
// =============================================================================
// NKECS/Serialization/NkReflectComponents.h
// =============================================================================
// Métadonnées de réflexion pour les composants ECS standard.
// Utilisées par InspectorPanel dans Unkeny pour afficher et éditer
// les champs de chaque composant sélectionné.
//
// Architecture :
//   NkComponentMeta       — description d'un composant (nom + liste de champs)
//   NkFieldMeta           — description d'un champ (nom, type, offset, range)
//   NkComponentRegistry   — registre global, accès par nom de type
//
// Macros de déclaration (à placer dans un .cpp) :
//   NK_COMPONENT_BEGIN(NkTransform, "Transform")
//     NK_FIELD_VEC3(position, "Position")
//     NK_FIELD_QUAT(rotation, "Rotation")
//     NK_FIELD_VEC3(scale,    "Scale", 0.001f, 100.f)
//   NK_COMPONENT_END(NkTransform)
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
    namespace ecs {

        // =====================================================================
        // Types de champs réfléchis
        // =====================================================================
        enum class NkFieldType : nk_uint8 {
            Unknown = 0,
            Bool,
            Int8, Int16, Int32, Int64,
            UInt8, UInt16, UInt32, UInt64,
            Float32, Float64,
            Vec2f, Vec3f, Vec4f,
            Quatf,
            Mat4f,
            String,       // NkString
            StringFixed,  // char[N]
            Enum,         // entier + liste de noms
            AssetPath,    // NkString avec browser d'asset
            Color,        // NkVec4f interprété comme RGBA [0,1]
        };

        // =====================================================================
        // NkFieldMeta — métadonnée d'un champ
        // =====================================================================
        struct NkFieldMeta {
            const char* name       = nullptr;    // nom affiché dans l'inspecteur
            const char* tooltip    = nullptr;    // info-bulle optionnelle
            NkFieldType type       = NkFieldType::Unknown;
            nk_usize    offset     = 0;          // offsetof(Struct, field)
            nk_usize    size       = 0;          // sizeof(field)

            // Pour les numériques : plage de l'éditeur
            float32     rangeMin   = 0.f;
            float32     rangeMax   = 0.f;
            float32     step       = 0.f;

            // Pour StringFixed
            nk_usize    strMaxLen  = 0;

            // Pour Enum
            const char* const* enumNames  = nullptr;
            nk_uint32          enumCount  = 0;

            // Flags
            bool readOnly  = false;
            bool hidden    = false;
        };

        // =====================================================================
        // NkComponentMeta — métadonnée d'un composant
        // =====================================================================
        struct NkComponentMeta {
            const char*          typeName    = nullptr;
            const char*          displayName = nullptr;
            nk_usize             sizeBytes   = 0;
            NkVector<NkFieldMeta> fields;

            // Callback d'ajout du composant à une entité (pour le menu "Add Component")
            using AddFn = void(*)(NkWorld& world, NkEntityId id);
            AddFn addFn = nullptr;
        };

        // =====================================================================
        // NkComponentMetaRegistry — registre global
        // =====================================================================
        class NkComponentMetaRegistry {
        public:
            static NkComponentMetaRegistry& Get() noexcept {
                static NkComponentMetaRegistry r;
                return r;
            }

            void Register(const NkComponentMeta& meta) noexcept {
                for (nk_usize i = 0; i < mMetas.Size(); ++i) {
                    if (NkStrEqual(mMetas[i].typeName, meta.typeName)) {
                        mMetas[i] = meta; return;
                    }
                }
                mMetas.PushBack(meta);
            }

            const NkComponentMeta* Find(const char* typeName) const noexcept {
                for (nk_usize i = 0; i < mMetas.Size(); ++i)
                    if (NkStrEqual(mMetas[i].typeName, typeName)) return &mMetas[i];
                return nullptr;
            }

            const NkVector<NkComponentMeta>& All() const noexcept { return mMetas; }

        private:
            NkVector<NkComponentMeta> mMetas;
        };

    } // namespace ecs
} // namespace nkentseu

// =============================================================================
// Macros de déclaration de réflexion
// =============================================================================

#define NK_COMPONENT_BEGIN(Type, DisplayName) \
    static struct _NkMetaAutoReg_##Type { \
        _NkMetaAutoReg_##Type() noexcept { \
            nkentseu::ecs::NkComponentMeta _meta; \
            _meta.typeName    = #Type; \
            _meta.displayName = DisplayName; \
            _meta.sizeBytes   = sizeof(Type); \
            _meta.addFn       = [](nkentseu::ecs::NkWorld& w, \
                                   nkentseu::ecs::NkEntityId id) { \
                w.Add<Type>(id); \
            };

#define NK_FIELD(field, display, ftype) { \
    nkentseu::ecs::NkFieldMeta _f; \
    _f.name   = display; \
    _f.type   = nkentseu::ecs::NkFieldType::ftype; \
    _f.offset = offsetof(Type, field); \
    _f.size   = sizeof(((Type*)0)->field); \
    _meta.fields.PushBack(_f); }

#define NK_FIELD_RANGE(field, display, ftype, mn, mx, st) { \
    nkentseu::ecs::NkFieldMeta _f; \
    _f.name = display; _f.type = nkentseu::ecs::NkFieldType::ftype; \
    _f.offset = offsetof(Type, field); _f.size = sizeof(((Type*)0)->field); \
    _f.rangeMin = (float)(mn); _f.rangeMax = (float)(mx); _f.step = (float)(st); \
    _meta.fields.PushBack(_f); }

#define NK_FIELD_VEC3(field, display) \
    NK_FIELD(field, display, Vec3f)

#define NK_FIELD_QUAT(field, display) \
    NK_FIELD(field, display, Quatf)

#define NK_FIELD_STR_FIXED(field, display, maxLen) { \
    nkentseu::ecs::NkFieldMeta _f; \
    _f.name = display; _f.type = nkentseu::ecs::NkFieldType::StringFixed; \
    _f.offset = offsetof(Type, field); _f.size = sizeof(((Type*)0)->field); \
    _f.strMaxLen = maxLen; \
    _meta.fields.PushBack(_f); }

#define NK_FIELD_BOOL(field, display) \
    NK_FIELD(field, display, Bool)

#define NK_FIELD_ASSET(field, display) \
    NK_FIELD(field, display, AssetPath)

#define NK_COMPONENT_END(Type) \
            nkentseu::ecs::NkComponentMetaRegistry::Get().Register(_meta); \
        } \
    } _nk_meta_autoreg_##Type;

// =============================================================================
// Réflexion des composants standard — à placer dans NkCoreComponents.reflect.cpp
// =============================================================================
//
// NK_COMPONENT_BEGIN(NkTransform, "Transform")
//   NK_FIELD_VEC3(position, "Position")
//   NK_FIELD_QUAT(rotation, "Rotation")
//   NK_FIELD_RANGE(scale, "Scale", Vec3f, 0.001f, 100.f, 0.01f)
// NK_COMPONENT_END(NkTransform)
//
// NK_COMPONENT_BEGIN(NkName, "Nom")
//   NK_FIELD_STR_FIXED(name, "Nom de l'entité", NkName::kMaxLen)
// NK_COMPONENT_END(NkName)
//
// NK_COMPONENT_BEGIN(NkMeshComponent, "Mesh")
//   NK_FIELD_ASSET(meshPath, "Fichier mesh")
//   NK_FIELD_BOOL(visible,    "Visible")
//   NK_FIELD_BOOL(castShadow, "Ombre portée")
// NK_COMPONENT_END(NkMeshComponent)
