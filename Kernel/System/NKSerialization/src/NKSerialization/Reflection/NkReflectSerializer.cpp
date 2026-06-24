// =============================================================================
// NKSerialization/Reflection/NkReflectSerializer.cpp
// Implementation du pont Reflection <-> Serialization.
// =============================================================================

#include "NKSerialization/Reflection/NkReflectSerializer.h"

#include "NKReflection/NkProperty.h"
#include "NKReflection/NkType.h"
#include "NKReflection/NkReflectVariant.h"
#include "NKReflection/NkEnumDescriptor.h"
#include "NKReflection/NkContainerTrait.h"

namespace nkentseu {

    using reflection::NkClass;
    using reflection::NkProperty;
    using reflection::NkType;
    using reflection::NkTypeCategory;
    using reflection::NkReflectVariant;
    using reflection::NkPropertyFlags;
    using reflection::NkEnumDescriptor;
    using reflection::NkEnumRegistry;
    using reflection::NkContainerDescriptor;

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

                // Enums : sauves comme NOM symbolique si un descripteur est
                // disponible (robuste au reordering), sinon comme entier.
                case NkTypeCategory::NK_ENUM: {
                    const nk_int64 ev = v.ToInt64();
                    const NkEnumDescriptor* ed =
                        NkEnumRegistry::Get().Find(type.GetName());
                    const nk_char* sym = ed ? ed->ToName(ev) : nullptr;
                    if (sym) {
                        ar.SetString(key, NkStringView(sym));
                    } else {
                        ar.SetInt64(key, ev);
                    }
                    return true;
                }

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

                case NkTypeCategory::NK_ENUM: {
                    // Enum : tente d'abord la lecture par NOM symbolique (string),
                    // sinon repli sur l'entier. Resoud la valeur sous-jacente puis
                    // ecrit en respectant la taille de la destination.
                    nk_int64 i = 0;
                    NkString sym;
                    if (ar.GetString(key, sym)) {
                        const NkEnumDescriptor* ed =
                            NkEnumRegistry::Get().Find(type.GetName());
                        if (!ed || !ed->ToValue(sym.CStr(), i)) {
                            return false;
                        }
                    } else if (!ar.GetInt64(key, i)) {
                        return false;
                    }
                    return WriteIntToProperty(prop, instance, type, i);
                }

                default: {
                    if (IsIntegerCategory(cat)) {
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

        // ---------------------------------------------------------------------
        // CONTENEURS (Phase 3) — NkVector<primitif/string> via SetArray/GetArray
        // ---------------------------------------------------------------------

        // Lit la valeur de l'element a elemPtr (de categorie elemCat) et la
        // convertit en NkArchiveValue scalaire. Retourne false si non gere.
        nk_bool ElementToValue(const void* elemPtr,
                               const NkType& elemType,
                               NkArchiveValue& out) {
            const NkTypeCategory cat = elemType.GetCategory();

            if (cat == NkTypeCategory::NK_STRING) {
                const NkString* s = static_cast<const NkString*>(elemPtr);
                out = NkArchiveValue::FromString(s->View());
                return true;
            }

            // Primitifs / enum : copie binaire dans un variant pour coercion.
            NkReflectVariant v = NkReflectVariant::FromRaw(&elemType, elemPtr);
            if (!v.IsValid()) {
                return false;
            }
            switch (cat) {
                case NkTypeCategory::NK_BOOL:
                    out = NkArchiveValue::FromBool(v.ToBool());
                    return true;
                case NkTypeCategory::NK_FLOAT32:
                case NkTypeCategory::NK_FLOAT64:
                    out = NkArchiveValue::FromFloat64(v.ToFloat64());
                    return true;
                case NkTypeCategory::NK_ENUM:
                    out = NkArchiveValue::FromInt64(v.ToInt64());
                    return true;
                default:
                    if (IsIntegerCategory(cat)) {
                        out = NkArchiveValue::FromInt64(v.ToInt64());
                        return true;
                    }
                    return false;
            }
        }

        // Ecrit une NkArchiveValue scalaire dans l'element a elemPtr (categorie
        // elemCat), avec coercion. Retourne false si non gere.
        nk_bool ValueToElement(const NkArchiveValue& val,
                               void* elemPtr,
                               const NkType& elemType) {
            const NkTypeCategory cat = elemType.GetCategory();

            if (cat == NkTypeCategory::NK_STRING) {
                NkString* s = static_cast<NkString*>(elemPtr);
                *s = val.text; // representation textuelle canonique
                return true;
            }

            // Recupere une valeur numerique depuis la NkArchiveValue.
            nk_int64 iv = 0;
            nk_float64 fv = 0.0;
            nk_bool isFloat = false;
            if (val.IsFloat()) { fv = val.raw.f; isFloat = true; }
            else if (val.IsInt()) { iv = val.raw.i; }
            else if (val.IsUInt()) { iv = static_cast<nk_int64>(val.raw.u); }
            else if (val.IsBool()) { iv = val.raw.b ? 1 : 0; }
            else { return false; }

            nk_uint8* dst = static_cast<nk_uint8*>(elemPtr);
            auto putBytes = [&](const void* src, nk_usize n) {
                const nk_uint8* s = static_cast<const nk_uint8*>(src);
                for (nk_usize k = 0; k < n; ++k) { dst[k] = s[k]; }
            };

            switch (cat) {
                case NkTypeCategory::NK_BOOL:   { nk_bool b = isFloat ? (fv != 0.0) : (iv != 0); putBytes(&b, sizeof(b)); return true; }
                case NkTypeCategory::NK_INT8:   { nk_int8  v = static_cast<nk_int8>(isFloat ? (nk_int64)fv : iv);   putBytes(&v, sizeof(v)); return true; }
                case NkTypeCategory::NK_INT16:  { nk_int16 v = static_cast<nk_int16>(isFloat ? (nk_int64)fv : iv);  putBytes(&v, sizeof(v)); return true; }
                case NkTypeCategory::NK_INT32:  { nk_int32 v = static_cast<nk_int32>(isFloat ? (nk_int64)fv : iv);  putBytes(&v, sizeof(v)); return true; }
                case NkTypeCategory::NK_INT64:  { nk_int64 v = isFloat ? (nk_int64)fv : iv;                          putBytes(&v, sizeof(v)); return true; }
                case NkTypeCategory::NK_UINT8:  { nk_uint8  v = static_cast<nk_uint8>(isFloat ? (nk_int64)fv : iv);  putBytes(&v, sizeof(v)); return true; }
                case NkTypeCategory::NK_UINT16: { nk_uint16 v = static_cast<nk_uint16>(isFloat ? (nk_int64)fv : iv); putBytes(&v, sizeof(v)); return true; }
                case NkTypeCategory::NK_UINT32: { nk_uint32 v = static_cast<nk_uint32>(isFloat ? (nk_int64)fv : iv); putBytes(&v, sizeof(v)); return true; }
                case NkTypeCategory::NK_UINT64: { nk_uint64 v = static_cast<nk_uint64>(isFloat ? (nk_int64)fv : iv); putBytes(&v, sizeof(v)); return true; }
                case NkTypeCategory::NK_FLOAT32:{ nk_float32 v = static_cast<nk_float32>(isFloat ? fv : (nk_float64)iv); putBytes(&v, sizeof(v)); return true; }
                case NkTypeCategory::NK_FLOAT64:{ nk_float64 v = isFloat ? fv : (nk_float64)iv;                         putBytes(&v, sizeof(v)); return true; }
                case NkTypeCategory::NK_ENUM: {
                    // Ecrit selon la taille du type d'enum.
                    nk_int64 v = isFloat ? (nk_int64)fv : iv;
                    putBytes(&v, elemType.GetSize() <= sizeof(nk_int64) ? elemType.GetSize() : sizeof(nk_int64));
                    return true;
                }
                default: return false;
            }
        }

        // Serialise une propriete conteneur (NkVector<primitif/string>) en
        // tableau de scalaires. Retourne true si pris en charge.
        nk_bool WriteContainerProperty(const NkProperty* prop,
                                       const void* instance,
                                       NkArchive& ar) {
            const NkContainerDescriptor* desc = prop->GetContainer();
            if (!desc || !desc->IsValid() || !desc->elementType) {
                return false;
            }
            const NkType& elemType = *desc->elementType;
            // Conteneur d'objets reflechis : repousse (Phase 4). On ne gere ici
            // que les elements scalaires/string/enum via SetArray.
            if (elemType.GetCategory() == NkTypeCategory::NK_CLASS) {
                return false;
            }

            const void* container = prop->GetValuePtr(instance);
            const nk_usize count = desc->GetCount(container);

            NkVector<NkArchiveValue> arr;
            for (nk_usize i = 0; i < count; ++i) {
                const void* elemPtr = desc->GetElementPtr(container, i);
                NkArchiveValue val;
                if (elemPtr && ElementToValue(elemPtr, elemType, val)) {
                    arr.PushBack(val);
                }
            }
            ar.SetArray(prop->GetName(), arr);
            return true;
        }

        // Deserialise une propriete conteneur depuis un tableau de scalaires.
        nk_bool ReadContainerProperty(const NkProperty* prop,
                                      void* instance,
                                      const NkArchive& ar) {
            const NkContainerDescriptor* desc = prop->GetContainer();
            if (!desc || !desc->IsValid() || !desc->elementType) {
                return false;
            }
            const NkType& elemType = *desc->elementType;
            if (elemType.GetCategory() == NkTypeCategory::NK_CLASS) {
                return false;
            }

            NkVector<NkArchiveValue> arr;
            if (!ar.GetArray(prop->GetName(), arr)) {
                return false;
            }

            void* container = prop->GetValuePtr(instance);
            desc->Clear(container);
            for (nk_usize i = 0; i < arr.Size(); ++i) {
                void* elemPtr = desc->PushBackDefault(container);
                if (elemPtr) {
                    ValueToElement(arr[i], elemPtr, elemType);
                }
            }
            return true;
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

                // Cas conteneur reflechi (NkVector<...>) : tableau de scalaires.
                if (prop->IsContainer()) {
                    if (WriteContainerProperty(prop, instance, ar)) {
                        continue;
                    }
                    // Conteneur d'objets non gere : ignore en P3.
                    continue;
                }

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

                // Conteneur reflechi (NkVector<...>).
                if (prop->IsContainer()) {
                    ReadContainerProperty(prop, instance, ar);
                    continue;
                }

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
