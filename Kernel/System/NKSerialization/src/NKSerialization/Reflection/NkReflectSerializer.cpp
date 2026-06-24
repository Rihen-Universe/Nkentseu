// =============================================================================
// NKSerialization/Reflection/NkReflectSerializer.cpp
// Implementation du pont Reflection <-> Serialization.
// =============================================================================

#include "NKSerialization/Reflection/NkReflectSerializer.h"

#include "NKReflection/NkProperty.h"
#include "NKReflection/NkType.h"
#include "NKReflection/NkReflectVariant.h"

namespace nkentseu {

    using reflection::NkClass;
    using reflection::NkProperty;
    using reflection::NkType;
    using reflection::NkTypeCategory;
    using reflection::NkReflectVariant;
    using reflection::NkPropertyFlags;

    // -------------------------------------------------------------------------
    // UTILITAIRES INTERNES
    // -------------------------------------------------------------------------
    namespace {

        // Vrai si la categorie est un entier signe ou non signe (incluant bool
        // traite ailleurs). Sert a router vers SetInt64.
        nk_bool IsIntegerCategory(NkTypeCategory c) {
            switch (c) {
                case NkTypeCategory::NK_INT8:
                case NkTypeCategory::NK_INT16:
                case NkTypeCategory::NK_INT32:
                case NkTypeCategory::NK_INT64:
                case NkTypeCategory::NK_UINT8:
                case NkTypeCategory::NK_UINT16:
                case NkTypeCategory::NK_UINT32:
                case NkTypeCategory::NK_UINT64:
                    return true;
                default:
                    return false;
            }
        }

        // Ecrit une propriete primitive/string/enum dans l'archive.
        // Retourne true si la categorie a ete prise en charge.
        nk_bool WriteScalarProperty(const NkProperty* prop,
                                    const void* instance,
                                    NkArchive& ar) {
            const NkType& type = prop->GetType();
            const NkTypeCategory cat = type.GetCategory();
            const nk_char* key = prop->GetName();

            NkReflectVariant v = prop->GetValueGeneric(instance);

            switch (cat) {
                case NkTypeCategory::NK_BOOL:
                    ar.SetBool(key, v.ToBool());
                    return true;

                case NkTypeCategory::NK_FLOAT32:
                case NkTypeCategory::NK_FLOAT64:
                    ar.SetFloat64(key, v.ToFloat64());
                    return true;

                case NkTypeCategory::NK_STRING:
                    ar.SetString(key, v.ToString().View());
                    return true;

                // Enums : sauves comme entier (Phase 3 : noms symboliques).
                case NkTypeCategory::NK_ENUM:
                    ar.SetInt64(key, v.ToInt64());
                    return true;

                default:
                    if (IsIntegerCategory(cat)) {
                        ar.SetInt64(key, v.ToInt64());
                        return true;
                    }
                    return false;
            }
        }

        // Aide : ecrit un entier 64 bits vers une propriete entiere/enum en
        // respectant la categorie destination (la coercition finale est faite
        // par SetValueGeneric/WritePrimitiveCoerced cote NkProperty).
        // On passe par un variant de meme categorie que la destination quand
        // possible, sinon un variant int64 que NkProperty saura coercer.
        nk_bool WriteIntToProperty(const NkProperty* prop,
                                   void* instance,
                                   const NkType& type,
                                   nk_int64 value) {
            switch (type.GetCategory()) {
                case NkTypeCategory::NK_INT8:
                    return prop->SetValueGeneric(instance, NkReflectVariant::From<nk_int8>(static_cast<nk_int8>(value)));
                case NkTypeCategory::NK_INT16:
                    return prop->SetValueGeneric(instance, NkReflectVariant::From<nk_int16>(static_cast<nk_int16>(value)));
                case NkTypeCategory::NK_INT32:
                    return prop->SetValueGeneric(instance, NkReflectVariant::From<nk_int32>(static_cast<nk_int32>(value)));
                case NkTypeCategory::NK_INT64:
                    return prop->SetValueGeneric(instance, NkReflectVariant::From<nk_int64>(value));
                case NkTypeCategory::NK_UINT8:
                    return prop->SetValueGeneric(instance, NkReflectVariant::From<nk_uint8>(static_cast<nk_uint8>(value)));
                case NkTypeCategory::NK_UINT16:
                    return prop->SetValueGeneric(instance, NkReflectVariant::From<nk_uint16>(static_cast<nk_uint16>(value)));
                case NkTypeCategory::NK_UINT32:
                    return prop->SetValueGeneric(instance, NkReflectVariant::From<nk_uint32>(static_cast<nk_uint32>(value)));
                case NkTypeCategory::NK_UINT64:
                    return prop->SetValueGeneric(instance, NkReflectVariant::From<nk_uint64>(static_cast<nk_uint64>(value)));
                default:
                    // Enum ou autre : on ecrit en int64, SetValueGeneric coerce
                    // vers la taille reelle de la destination.
                    return prop->SetValueGeneric(instance, NkReflectVariant::From<nk_int64>(value));
            }
        }

        // Lit une propriete primitive/string/enum depuis l'archive et l'ecrit
        // dans l'instance via SetValueGeneric. Retourne true si la cle existait
        // et la categorie etait geree.
        nk_bool ReadScalarProperty(const NkProperty* prop,
                                   void* instance,
                                   const NkArchive& ar) {
            const NkType& type = prop->GetType();
            const NkTypeCategory cat = type.GetCategory();
            const nk_char* key = prop->GetName();

            switch (cat) {
                case NkTypeCategory::NK_BOOL: {
                    nk_bool b = false;
                    if (!ar.GetBool(key, b)) {
                        return false;
                    }
                    return prop->SetValueGeneric(instance, NkReflectVariant::From<nk_bool>(b));
                }

                case NkTypeCategory::NK_FLOAT32: {
                    nk_float64 d = 0.0;
                    if (!ar.GetFloat64(key, d)) {
                        return false;
                    }
                    return prop->SetValueGeneric(
                        instance, NkReflectVariant::From<nk_float32>(static_cast<nk_float32>(d)));
                }

                case NkTypeCategory::NK_FLOAT64: {
                    nk_float64 d = 0.0;
                    if (!ar.GetFloat64(key, d)) {
                        return false;
                    }
                    return prop->SetValueGeneric(instance, NkReflectVariant::From<nk_float64>(d));
                }

                case NkTypeCategory::NK_STRING: {
                    NkString s;
                    if (!ar.GetString(key, s)) {
                        return false;
                    }
                    return prop->SetValueGeneric(instance, NkReflectVariant::From<NkString>(s));
                }

                case NkTypeCategory::NK_ENUM:
                default: {
                    if (cat == NkTypeCategory::NK_ENUM || IsIntegerCategory(cat)) {
                        nk_int64 i = 0;
                        if (!ar.GetInt64(key, i)) {
                            return false;
                        }
                        // On construit un variant DE LA BONNE TAILLE/TYPE de la
                        // propriete a partir des octets de i. SetValueGeneric
                        // recoit donc un variant primitif coerce ; il sait
                        // ecrire en respectant la taille destination.
                        return WriteIntToProperty(prop, instance, type, i);
                    }
                    return false;
                }
            }
        }

    } // namespace anonyme

    // -------------------------------------------------------------------------
    // SerializeReflected
    // -------------------------------------------------------------------------
    nk_bool NkReflectSerializer::SerializeReflected(
        const NkClass* cls,
        const void* instance,
        NkArchive& ar) {

        if (!cls || !instance) {
            return false;
        }

        // Parcours de la chaine d'heritage : on traite la classe courante PUIS
        // ses bases, de sorte que toutes les proprietes (heritees comprises)
        // soient serialisees. La deduplication par nom est assuree en amont par
        // NkClass::AddProperty.
        for (const NkClass* current = cls; current != nullptr; current = current->GetBaseClass()) {
            const nk_usize count = current->GetPropertyCount();
            for (nk_usize i = 0; i < count; ++i) {
                const NkProperty* prop = current->GetPropertyAt(i);
                if (!prop) {
                    continue;
                }

                // Exclusion des proprietes transientes et statiques (les
                // statiques ne sont pas liees a l'instance).
                if (prop->IsTransient() || prop->IsStatic()) {
                    continue;
                }

                const NkType& type = prop->GetType();
                const NkTypeCategory cat = type.GetCategory();

                // Cas objet imbrique reflechi : NK_CLASS avec NkClass associe.
                if (cat == NkTypeCategory::NK_CLASS) {
                    const NkClass* subCls = type.GetClass();
                    if (subCls) {
                        const void* subInstance = prop->GetValuePtr(instance);
                        NkArchive subAr;
                        if (SerializeReflected(subCls, subInstance, subAr)) {
                            ar.SetObject(prop->GetName(), subAr);
                        }
                    }
                    // NK_CLASS sans NkClass associe (non reflechi) : ignore P2.
                    continue;
                }

                // Cas scalaires/string/enum.
                WriteScalarProperty(prop, instance, ar);
                // Categorie non geree (pointeur/vecteur/...) : silencieusement
                // ignoree en Phase 2 (repoussee Phase 3).
            }
        }

        return true;
    }

    // -------------------------------------------------------------------------
    // DeserializeReflected
    // -------------------------------------------------------------------------
    nk_bool NkReflectSerializer::DeserializeReflected(
        const NkClass* cls,
        void* instance,
        const NkArchive& ar) {

        if (!cls || !instance) {
            return false;
        }

        for (const NkClass* current = cls; current != nullptr; current = current->GetBaseClass()) {
            const nk_usize count = current->GetPropertyCount();
            for (nk_usize i = 0; i < count; ++i) {
                const NkProperty* prop = current->GetPropertyAt(i);
                if (!prop) {
                    continue;
                }

                // Transient/static/read-only : non ecrites.
                if (prop->IsTransient() || prop->IsStatic() || prop->IsReadOnly()) {
                    continue;
                }

                const NkType& type = prop->GetType();
                const NkTypeCategory cat = type.GetCategory();

                // Objet imbrique reflechi.
                if (cat == NkTypeCategory::NK_CLASS) {
                    const NkClass* subCls = type.GetClass();
                    if (subCls) {
                        NkArchive subAr;
                        if (ar.GetObject(prop->GetName(), subAr)) {
                            void* subInstance = prop->GetValuePtr(instance);
                            DeserializeReflected(subCls, subInstance, subAr);
                        }
                    }
                    continue;
                }

                // Scalaires/string/enum.
                ReadScalarProperty(prop, instance, ar);
            }
        }

        return true;
    }

} // namespace nkentseu

// ============================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// ============================================================
