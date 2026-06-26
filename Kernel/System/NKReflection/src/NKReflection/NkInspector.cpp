// =============================================================================
// FICHIER  : Modules/System/NKReflection/src/NKReflection/NkInspector.cpp
// MODULE   : NKReflection
// AUTEUR   : Rihen
// DATE     : 2026-06-24
// VERSION  : 1.0.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Implementation de l'API inspecteur (Phase 5). Parcourt les proprietes d'une
//   classe reflechie (heritage inclus), construit des NkEditableProperty et
//   offre le live-edit Set/GetPropertyByName.
// =============================================================================

#include "NKReflection/NkInspector.h"

namespace nkentseu {

    namespace reflection {

        // =================================================================
        // DescribeEditableProperty
        // =================================================================
        NkEditableProperty DescribeEditableProperty(const NkProperty* prop,
                                                    const void* instance) {
            NkEditableProperty out;
            if (!prop) {
                return out;
            }

            const NkType& type = prop->GetType();

            // Identite + type.
            out.name        = prop->GetName();
            out.displayName = prop->GetDisplayName() ? prop->GetDisplayName()
                                                     : prop->GetName();
            out.type        = &type;
            out.category    = type.GetCategory();
            out.metaFlags   = prop->GetMetaFlags();

            // Hints d'edition (Phase 3).
            out.hasRange = prop->GetRange(out.rangeMin, out.rangeMax);
            out.tooltip  = prop->GetTooltip();
            out.group    = prop->GetCategory();
            out.readOnly = prop->IsReadOnly();
            out.hidden   = prop->IsHiddenInEditor();

            out.isContainer = prop->IsContainer();
            out.isObject    = (type.GetCategory() == NkTypeCategory::NK_CLASS) &&
                              (type.GetClass() != nullptr);

            // Valeur courante (copie type-erased). On NE lit pas les conteneurs
            // (NkVector) en variant : l'inspecteur les traite specifiquement.
            if (instance && !out.isContainer) {
                out.value = prop->GetValueGeneric(instance);
            }

            return out;
        }

        // =================================================================
        // EnumerateEditableProperties
        // =================================================================
        NkVector<NkEditableProperty> EnumerateEditableProperties(const NkClass* cls,
                                                                 const void* instance) {
            NkVector<NkEditableProperty> result;
            if (!cls) {
                return result;
            }

            // Parcours classe courante PUIS bases (la plus derivee d'abord).
            for (const NkClass* current = cls; current != nullptr;
                 current = current->GetBaseClass()) {
                const nk_usize count = current->GetPropertyCount();
                for (nk_usize i = 0; i < count; ++i) {
                    const NkProperty* prop = current->GetPropertyAt(i);
                    if (!prop) {
                        continue;
                    }
                    // On saute les proprietes explicitement cachees de l'editeur
                    // et les statiques (non liees a l'instance).
                    if (prop->IsHiddenInEditor() || prop->IsStatic()) {
                        continue;
                    }
                    result.PushBack(DescribeEditableProperty(prop, instance));
                }
            }

            return result;
        }

        // =================================================================
        // SetPropertyByName
        // =================================================================
        nk_bool SetPropertyByName(const NkClass* cls, void* instance,
                                  const nk_char* name, const NkReflectVariant& value) {
            if (!cls || !instance || !name) {
                return false;
            }
            const NkProperty* prop = cls->GetProperty(name);
            if (!prop) {
                return false;
            }
            // SetValueGeneric respecte deja read-only (no-op + false).
            return prop->SetValueGeneric(instance, value);
        }

        // =================================================================
        // GetPropertyByName
        // =================================================================
        NkReflectVariant GetPropertyByName(const NkClass* cls, const void* instance,
                                           const nk_char* name) {
            if (!cls || !instance || !name) {
                return NkReflectVariant();
            }
            const NkProperty* prop = cls->GetProperty(name);
            if (!prop) {
                return NkReflectVariant();
            }
            return prop->GetValueGeneric(instance);
        }

    } // namespace reflection

} // namespace nkentseu

// ============================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// ============================================================
