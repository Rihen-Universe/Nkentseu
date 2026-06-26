// =============================================================================
// FICHIER: NKECS/Reflect/NkReflectBridge.h
// MODULE : NKECS (Runtime) -> NKReflection / NKSerialization (System)
// =============================================================================
// DESCRIPTION (Phase 4 du chantier NKReflection) :
//   Pont (adaptateur) entre le systeme de reflexion PROPRE a NKECS
//   (NKECS/Reflect/NkReflect.h : NkFieldInfo / NkTypeInfo / NkReflectRegistry,
//   declares via NK_REFLECT_BEGIN / NK_FIELD_* / NK_REFLECT_END) et la SOURCE
//   DE VERITE UNIQUE de la reflexion : NKReflection (NkClass / NkProperty /
//   NkType, couche System).
//
//   Decision actee : NKReflection est la verite ; NKECS/Reflect devient un
//   FOURNISSEUR de champs. Ce pont GENERE, pour un composant T deja decrit par
//   ses NkFieldInfo, une NkClass NKReflection equivalente (1 NkFieldInfo ->
//   1 NkProperty : offset, NkType de categorie, nom, + metaflags/range/tooltip/
//   categorie). Le NkClass genere est ensuite stocke dans
//   ComponentMeta.reflectClass et les hooks serialize/deserialize sont branches
//   sur NkReflectSerializer (pont generique Phase 2).
//
//   Usage :
//       struct Transform { nk_float32 px, py, pz; nk_float32 scale; };
//       NK_COMPONENT(Transform)
//       NK_REFLECT_BEGIN(Transform)
//           NK_FIELD_EX(px, NkFieldType::Float32)
//           ...
//       NK_REFLECT_END()
//       // au demarrage :
//       nkentseu::ecs::reflect::NkRegisterComponentReflection<Transform>();
//
//   Apres l'enregistrement :
//       const ComponentMeta* m = NkMetaOf<Transform>();
//       m->serialize(&t, archive);    // -> NkReflectSerializer::SerializeReflected
//       m->deserialize(&t2, archive);
//
//   Zero-STL : aucune dependance std:: ; allocations via les conteneurs maison
//   et les registres a stockage statique de NKReflection.
//
//   Auteur : Rihen — 2026-06-24 — Proprietary, free to use and modify.
// =============================================================================

#pragma once

#ifndef NKECS_REFLECT_NKREFLECTBRIDGE_H
#define NKECS_REFLECT_NKREFLECTBRIDGE_H

    // -------------------------------------------------------------------------
    // SECTION 1 : DEPENDANCES
    // -------------------------------------------------------------------------

    #include "NKECS/Core/NkTypeRegistry.h"
    #include "NKECS/Reflect/NkReflect.h"

    // Source de verite (couche System).
    #include "NKReflection/NkType.h"
    #include "NKReflection/NkProperty.h"
    #include "NKReflection/NkClass.h"
    #include "NKReflection/NkReflectMeta.h"
    #include "NKReflection/NkRegistry.h"

    // Pont generique reflexion <-> archive (Phase 2).
    #include "NKSerialization/Reflection/NkReflectSerializer.h"
    #include "NKSerialization/NkArchive.h"

    namespace nkentseu {
        namespace ecs {
            namespace reflect {

                // =============================================================
                // 1. MAPPING : NkFieldType (NKECS) -> NkTypeCategory (NKReflection)
                // =============================================================
                /**
                 * @brief Convertit une categorie de champ ECS en categorie de
                 *        type NKReflection.
                 *
                 * Les types mathematiques (Vec2/Vec3/Vec4/Quat/Mat4) et les
                 * identifiants ECS (Entity/Component/Archetype) n'ont pas de
                 * categorie scalaire NKReflection dediee : ils sont traites comme
                 * des objets/structs (NK_STRUCT) ou des primitifs selon leur
                 * representation. Ici on les mappe sur la categorie la plus
                 * proche pour la (de)serialisation generique.
                 */
                NKECS_INLINE ::nkentseu::reflection::NkTypeCategory
                FieldTypeToCategory(NkFieldType t) noexcept {
                    using C = ::nkentseu::reflection::NkTypeCategory;
                    switch (t) {
                        case NkFieldType::Bool:        return C::NK_BOOL;
                        case NkFieldType::Int8:        return C::NK_INT8;
                        case NkFieldType::Int16:       return C::NK_INT16;
                        case NkFieldType::Int32:       return C::NK_INT32;
                        case NkFieldType::Int64:       return C::NK_INT64;
                        case NkFieldType::UInt8:       return C::NK_UINT8;
                        case NkFieldType::UInt16:      return C::NK_UINT16;
                        case NkFieldType::UInt32:      return C::NK_UINT32;
                        case NkFieldType::UInt64:      return C::NK_UINT64;
                        case NkFieldType::Float32:     return C::NK_FLOAT32;
                        case NkFieldType::Float64:     return C::NK_FLOAT64;
                        case NkFieldType::String:      return C::NK_STRING;

                        // Identifiants ECS : entiers non signes 64/32 bits.
                        case NkFieldType::EntityId:    return C::NK_UINT64;
                        case NkFieldType::ComponentId: return C::NK_UINT32;
                        case NkFieldType::ArchetypeId: return C::NK_UINT32;

                        // Enum : traite comme entier (le serializer ecrit la valeur).
                        case NkFieldType::Enum:
                        case NkFieldType::Flags:       return C::NK_ENUM;

                        // Structures composites (vecteurs/mat/objets imbriques).
                        case NkFieldType::Vec2:
                        case NkFieldType::Vec3:
                        case NkFieldType::Vec4:
                        case NkFieldType::Quat:
                        case NkFieldType::Mat4:
                        case NkFieldType::Object:      return C::NK_STRUCT;

                        case NkFieldType::Array:       return C::NK_ARRAY;

                        case NkFieldType::Unknown:
                        default:                       return C::NK_UNKNOWN;
                    }
                }

                /**
                 * @brief Vrai si le champ est un scalaire primitif/chaine/enum
                 *        directement (de)serialisable par NkReflectSerializer.
                 */
                NKECS_INLINE bool FieldTypeIsScalar(NkFieldType t) noexcept {
                    using C = ::nkentseu::reflection::NkTypeCategory;
                    const C c = FieldTypeToCategory(t);
                    return (c >= C::NK_BOOL && c <= C::NK_FLOAT64)
                        || c == C::NK_STRING
                        || c == C::NK_ENUM;
                }

                // =============================================================
                // 2. STOCKAGE STATIQUE DES META-DONNEES GENEREES (par T)
                // =============================================================
                // NkClass conserve des const NkProperty* a duree de vie longue.
                // On heberge donc, pour chaque composant T, un bloc statique
                // contenant : le tableau de NkType (un par champ), le tableau de
                // NkProperty, et l'instance NkClass. Le tout est instancie une
                // seule fois (premiere invocation de NkRegisterComponentReflection<T>()).

                template<typename T>
                struct NkComponentReflectionStore {
                    static constexpr nk_usize kMaxFields = 64u;

                    // NkType n'a pas de constructeur par defaut : on reserve un
                    // buffer aligne et on construit via placement new.
                    alignas(::nkentseu::reflection::NkType)
                        unsigned char typeBuf[kMaxFields * sizeof(::nkentseu::reflection::NkType)];
                    alignas(::nkentseu::reflection::NkProperty)
                        unsigned char propBuf[kMaxFields * sizeof(::nkentseu::reflection::NkProperty)];
                    alignas(::nkentseu::reflection::NkClass)
                        unsigned char classBuf[sizeof(::nkentseu::reflection::NkClass)];

                    ::nkentseu::reflection::NkClass* cls = nullptr;
                    nk_usize fieldCount = 0;
                    bool built = false;

                    ::nkentseu::reflection::NkType* TypeAt(nk_usize i) {
                        return reinterpret_cast<::nkentseu::reflection::NkType*>(typeBuf)
                             + i;
                    }
                    ::nkentseu::reflection::NkProperty* PropAt(nk_usize i) {
                        return reinterpret_cast<::nkentseu::reflection::NkProperty*>(propBuf)
                             + i;
                    }
                };

                template<typename T>
                NKECS_INLINE NkComponentReflectionStore<T>& NkComponentStore() {
                    static NkComponentReflectionStore<T> store;
                    return store;
                }

                // =============================================================
                // 3. HOOKS (DE)SERIALISATION TYPE-ERASED PAR COMPOSANT
                // =============================================================
                // Branche ComponentMeta.serialize/deserialize sur le pont
                // generique NkReflectSerializer en passant le reflectClass genere.

                template<typename T>
                NKECS_INLINE void NkComponentSerializeHook(
                    const void* comp, ::nkentseu::NkArchive& ar) {
                    const ::nkentseu::reflection::NkClass* cls =
                        NkComponentStore<T>().cls;
                    if (cls) {
                        ::nkentseu::NkReflectSerializer::SerializeReflected(cls, comp, ar);
                    }
                }

                template<typename T>
                NKECS_INLINE void NkComponentDeserializeHook(
                    void* comp, const ::nkentseu::NkArchive& ar) {
                    const ::nkentseu::reflection::NkClass* cls =
                        NkComponentStore<T>().cls;
                    if (cls) {
                        ::nkentseu::NkReflectSerializer::DeserializeReflected(cls, comp, ar);
                    }
                }

                // =============================================================
                // 4. CONSTRUCTION DU NkClass A PARTIR DES NkFieldInfo
                // =============================================================
                /**
                 * @brief Genere (une fois) le NkClass NKReflection d'un composant
                 *        T a partir de ses NkFieldInfo (NKECS/Reflect), puis
                 *        renseigne ComponentMeta.reflectClass + hooks serialize/
                 *        deserialize.
                 *
                 * @tparam T Type de composant deja decrit par NK_REFLECT_BEGIN/END
                 *           (present dans NkReflectRegistry) et idealement enregistre
                 *           via NK_COMPONENT.
                 * @return Pointeur const vers le NkClass genere, ou nullptr si T
                 *         n'a pas de NkTypeInfo (pas de bloc NK_REFLECT_*).
                 *
                 * @note Idempotent : les invocations suivantes renvoient le NkClass
                 *       deja construit sans recreer les objets.
                 * @note Les champs non scalaires (Vec/Mat/Object/Array) recoivent
                 *       quand meme une NkProperty (categorie struct/array) afin que
                 *       l'inspecteur les voie ; leur (de)serialisation profonde est
                 *       repoussee a P5 (necessite la reflexion du sous-type).
                 */
                template<typename T>
                const ::nkentseu::reflection::NkClass* NkRegisterComponentReflection() {
                    using namespace ::nkentseu::reflection;

                    NkComponentReflectionStore<T>& store = NkComponentStore<T>();
                    if (store.built) {
                        return store.cls;
                    }

                    // Recupere la description ECS du type (NkFieldInfo[]).
                    const NkComponentId id = ::nkentseu::ecs::NkIdOf<T>();
                    const NkTypeInfo* info = NkReflectRegistry::Global().Get(id);
                    if (!info || info->fieldCount == 0) {
                        // Pas de champs reflechis ECS : rien a generer.
                        store.built = true;
                        store.cls   = nullptr;
                        return nullptr;
                    }

                    // Construit le NkClass (utilise le NkType NKReflection de T).
                    NkType& selfType = const_cast<NkType&>(NkTypeOf<T>());
                    NkClass* cls = new (store.classBuf)
                        NkClass(info->name, sizeof(T), selfType);
                    selfType.SetClass(cls);   // auto-link pour la recursion d'objet

                    const nk_usize n =
                        (info->fieldCount < NkComponentReflectionStore<T>::kMaxFields)
                            ? info->fieldCount
                            : NkComponentReflectionStore<T>::kMaxFields;

                    for (nk_usize i = 0; i < n; ++i) {
                        const NkFieldInfo& f = info->fields[i];

                        // 4.a — NkType du champ (categorie mappee, size/align du champ).
                        const NkTypeCategory cat = FieldTypeToCategory(f.type);
                        NkType* fieldType = new (store.TypeAt(i)) NkType(
                            f.name,                 // nom du champ comme nom de type proxy
                            f.size,                 // sizeof du champ
                            f.size ? f.size : 1,    // alignement best-effort
                            cat);

                        // 4.b — NkProperty (acces direct par offset).
                        NkProperty* prop = new (store.PropAt(i)) NkProperty(
                            f.name,
                            *fieldType,
                            static_cast<nk_usize>(f.offset),
                            0u);

                        // 4.c — Metaflags : les bits NkMetaFlag (ECS) sont IDENTIQUES
                        // a NkReflectMeta (NKReflection) — pont 1:1 (cf. NkReflectMeta.h).
                        prop->AddMetaFlags(static_cast<nk_uint64>(f.metaFlags));

                        // 4.d — Range / tooltip / categorie / displayName.
                        if (f.HasFlag(NkMeta_Range)) {
                            prop->SetRange(f.minValue, f.maxValue);
                        }
                        if (f.tooltip)     { prop->SetTooltip(f.tooltip); }
                        if (f.category)    { prop->SetCategory(f.category); }
                        if (f.displayName) { prop->SetDisplayName(f.displayName); }

                        cls->AddProperty(prop);
                    }

                    store.fieldCount = n;
                    store.cls        = cls;
                    store.built      = true;

                    // Enregistre le NkClass dans le registre global NKReflection
                    // (visibilite par nom pour l'editeur / la deserialisation).
                    NkRegistry::Get().RegisterClass(cls);

                    // Branche les hooks (de)serialisation dans ComponentMeta.
                    NkTypeRegistry::Global().SetReflection(
                        id,
                        cls,
                        &NkComponentSerializeHook<T>,
                        &NkComponentDeserializeHook<T>);

                    return cls;
                }

            } // namespace reflect
        } // namespace ecs
    } // namespace nkentseu

#endif // NKECS_REFLECT_NKREFLECTBRIDGE_H

// ============================================================
// Copyright (c) 2025-2026 Rihen. Tous droits reserves.
// ============================================================
