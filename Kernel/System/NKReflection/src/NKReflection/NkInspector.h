// =============================================================================
// FICHIER  : Modules/System/NKReflection/src/NKReflection/NkInspector.h
// MODULE   : NKReflection
// AUTEUR   : Rihen
// DATE     : 2026-06-24
// VERSION  : 1.0.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   API "inspecteur" de la Phase 5 : transforme une classe reflechie + une
//   instance en une liste de descripteurs editables (NkEditableProperty) que
//   l'Editor Kit consomme pour generer un widget par propriete. Fournit aussi
//   le live-edit type-erased (Set/GetPropertyByName) respectant read-only.
//
//   C'est la couche que l'inspecteur de l'editeur appelle :
//
//       for (auto& p : EnumerateEditableProperties(obj)) {
//           // genere un widget selon p.category (slider si p.hasRange, ...)
//           // edition -> SetPropertyByName(cls, &obj, p.name, newValue);
//       }
//
//   Zero-STL : conteneurs/chaines NKContainers, allocations NKMemory.
// =============================================================================

#pragma once

#ifndef NK_REFLECTION_NKINSPECTOR_H
#define NK_REFLECTION_NKINSPECTOR_H

    // -------------------------------------------------------------------------
    // SECTION 1 : DEPENDANCES INTERNES
    // -------------------------------------------------------------------------

    #include "NKReflection/NkType.h"
    #include "NKReflection/NkClass.h"
    #include "NKReflection/NkProperty.h"
    #include "NKReflection/NkReflectVariant.h"
    #include "NKReflection/NkReflectMeta.h"
    #include "NKContainers/Sequential/NkVector.h"

    namespace nkentseu {

        namespace reflection {

            // =================================================================
            // STRUCTURE : NkEditableProperty
            // =================================================================
            /**
             * @struct NkEditableProperty
             * @brief Descripteur complet d'une propriete editable dans l'inspecteur.
             *
             * Agrege en un seul bloc tout ce dont un panneau d'inspecteur a besoin
             * pour generer un widget : identite (name/displayName), type (type/
             * category), valeur courante (value, copie type-erased), et hints
             * d'edition issus des metadonnees (range, tooltip, groupe, read-only).
             *
             * @note Les pointeurs de chaine (name/displayName/tooltip/group) ont une
             *       duree de vie statique (litteraux portes par NkProperty/NkType).
             * @note 'value' est une COPIE de la valeur au moment de l'enumeration.
             *       Pour relire/ecrire en live, utiliser Get/SetPropertyByName.
             */
            struct NkEditableProperty {
                const nk_char*  name        = nullptr;   ///< Nom du membre reflechi
                const nk_char*  displayName = nullptr;   ///< Nom lisible (ou name si absent)
                const NkType*   type        = nullptr;   ///< Meta-type de la propriete
                NkTypeCategory  category    = NkTypeCategory::NK_UNKNOWN; ///< Categorie (widget)
                NkReflectVariant value;                  ///< Copie de la valeur courante
                nk_uint64       metaFlags   = 0;         ///< Drapeaux NkReflectMeta bruts

                // Hints d'edition derives des metadonnees (Phase 3).
                nk_bool         hasRange    = false;     ///< true si une plage slider existe
                nk_float32      rangeMin    = 0.0f;      ///< Borne min (si hasRange)
                nk_float32      rangeMax    = 0.0f;      ///< Borne max (si hasRange)
                const nk_char*  tooltip     = nullptr;   ///< Aide au survol (ou nullptr)
                const nk_char*  group       = nullptr;   ///< Categorie/regroupement UI (ou nullptr)
                nk_bool         readOnly    = false;     ///< true si non editable (grise)
                nk_bool         hidden      = false;     ///< true si cachee (ne devrait pas apparaitre)
                nk_bool         isContainer = false;     ///< true si NkVector<...> reflechi
                nk_bool         isObject    = false;     ///< true si sous-objet NK_CLASS reflechi
            };

            // =================================================================
            // FONCTIONS LIBRES : ENUMERATION / LIVE-EDIT
            // =================================================================

            /**
             * @brief Construit le descripteur editable d'une seule propriete.
             * @param prop     Propriete reflechie source (non nul).
             * @param instance Instance lue pour capturer la valeur courante (ou nullptr).
             * @return Descripteur rempli (value invalide si instance == nullptr).
             */
            NKENTSEU_REFLECTION_API NkEditableProperty
            DescribeEditableProperty(const NkProperty* prop, const void* instance);

            /**
             * @brief Enumere TOUTES les proprietes editables d'une classe + ses bases.
             * @param cls      Classe reflechie (non nul).
             * @param instance Instance dont on capture les valeurs (peut etre nullptr).
             * @return Vecteur de descripteurs ; les proprietes IsHiddenInEditor et
             *         statiques sont sautees. Ordre : classe la plus derivee d'abord,
             *         puis remontee dans l'heritage.
             *
             * @note C'est la fonction maitresse consommee par l'inspecteur.
             */
            NKENTSEU_REFLECTION_API NkVector<NkEditableProperty>
            EnumerateEditableProperties(const NkClass* cls, const void* instance);

            /**
             * @brief Live-edit : ecrit une valeur dans une propriete par son nom.
             * @param cls      Classe reflechie (recherche heritage incluse).
             * @param instance Instance a modifier (non nul).
             * @param name     Nom de la propriete cible.
             * @param value    Nouvelle valeur (variant). Coercion best-effort.
             * @return true si la propriete existe, est editable et l'ecriture a eu
             *         lieu ; false sinon (introuvable, read-only, type incompatible).
             */
            NKENTSEU_REFLECTION_API nk_bool
            SetPropertyByName(const NkClass* cls, void* instance,
                              const nk_char* name, const NkReflectVariant& value);

            /**
             * @brief Live-read : lit la valeur courante d'une propriete par son nom.
             * @param cls      Classe reflechie (recherche heritage incluse).
             * @param instance Instance a lire (non nul).
             * @param name     Nom de la propriete cible.
             * @return Variant portant une copie de la valeur (invalide si introuvable).
             */
            NKENTSEU_REFLECTION_API NkReflectVariant
            GetPropertyByName(const NkClass* cls, const void* instance,
                              const nk_char* name);

            // =================================================================
            // HELPERS TEMPLATES (resolution automatique du NkClass)
            // =================================================================
            // Detection SFINAE de T::GetStaticClass() (injecte par
            // NKENTSEU_REFLECT_CLASS). La surcharge (int) est preferee quand la
            // classe est reflechie, sinon repli (...) retournant nullptr.

            template<typename T>
            auto NkResolveStaticClassImpl(int)
                -> decltype(&T::GetStaticClass(), (const NkClass*)nullptr) {
                return &T::GetStaticClass();
            }

            template<typename T>
            const NkClass* NkResolveStaticClassImpl(...) {
                return nullptr;
            }

            /**
             * @brief Resout le NkClass d'un type T reflechi (ou nullptr).
             * @tparam T Type C++ (idealement marque NKENTSEU_REFLECT_CLASS).
             */
            template<typename T>
            const NkClass* NkResolveStaticClass() {
                return NkResolveStaticClassImpl<T>(0);
            }

            /**
             * @brief Surcharge template : enumere directement depuis un objet.
             * @tparam T Type reflechi de l'instance.
             * @param obj Instance a inspecter.
             * @return Liste de descripteurs editables (vide si T non reflechi).
             *
             * @example
             * @code
             * Player p;
             * for (auto& d : EnumerateEditableProperties(p)) {
             *     // widget selon d.category ...
             * }
             * @endcode
             */
            template<typename T>
            NkVector<NkEditableProperty> EnumerateEditableProperties(const T& obj) {
                const NkClass* cls = NkResolveStaticClass<T>();
                return EnumerateEditableProperties(cls, static_cast<const void*>(&obj));
            }

            /** @brief Surcharge template de SetPropertyByName (resout T). */
            template<typename T>
            nk_bool SetPropertyByName(T& obj, const nk_char* name,
                                      const NkReflectVariant& value) {
                const NkClass* cls = NkResolveStaticClass<T>();
                return SetPropertyByName(cls, static_cast<void*>(&obj), name, value);
            }

            /** @brief Surcharge template de GetPropertyByName (resout T). */
            template<typename T>
            NkReflectVariant GetPropertyByName(const T& obj, const nk_char* name) {
                const NkClass* cls = NkResolveStaticClass<T>();
                return GetPropertyByName(cls, static_cast<const void*>(&obj), name);
            }

            /**
             * @brief Helper typed : ecrit une valeur de type V dans une propriete.
             * @tparam T Type reflechi de l'objet.
             * @tparam V Type de la valeur a affecter.
             */
            template<typename T, typename V>
            nk_bool SetPropertyValue(T& obj, const nk_char* name, const V& value) {
                return SetPropertyByName<T>(obj, name, NkReflectVariant::From<V>(value));
            }

        } // namespace reflection

    } // namespace nkentseu

#endif // NK_REFLECTION_NKINSPECTOR_H

// ============================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// ============================================================
