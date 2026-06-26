// =============================================================================
// FICHIER  : Modules/System/NKReflection/src/NKReflection/NkEnumDescriptor.h
// MODULE   : NKReflection
// AUTEUR   : Rihen
// DATE     : 2026-06-24
// VERSION  : 1.0.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Reflexion des enumerations (Phase 3). Un NkEnumDescriptor porte le nom de
//   l'enum + la liste des paires {nom, valeur sous-jacente nk_int64}. Permet de
//   serialiser un enum par son NOM symbolique (lisible/robuste au reordering) et
//   de retrouver sa valeur. Macro NKENTSEU_REFLECT_ENUM(EnumType, V1, V2, ...).
//
//   Un registre d'enums (singleton) indexe les descripteurs par nom de type
//   (typeid). NkRegistry expose des helpers de confort qui delegent ici.
//
//   Zero-STL : NkVector / nk_char* (literaux), aucune dependance std::.
// =============================================================================

#pragma once

#ifndef NK_REFLECTION_NKENUMDESCRIPTOR_H
#define NK_REFLECTION_NKENUMDESCRIPTOR_H

    #include "NKCore/NkTypes.h"
    #include "NKReflection/NkType.h"
    #include "NKContainers/Sequential/NkVector.h"

    #include <cstring>
    #include <typeinfo>

    namespace nkentseu {

        namespace reflection {

            // =================================================================
            // STRUCTURE : NkEnumEntry (paire nom <-> valeur)
            // =================================================================
            struct NkEnumEntry {
                const nk_char* name = nullptr;  ///< Litteral du nom de l'enumerateur
                nk_int64       value = 0;       ///< Valeur sous-jacente (nk_int64)
            };

            // =================================================================
            // CLASSE : NkEnumDescriptor
            // =================================================================
            /**
             * @class NkEnumDescriptor
             * @brief Descripteur runtime d'une enumeration reflechie.
             *
             * Contient le nom du type d'enum + les paires {nom, valeur}. Fournit
             * la conversion nom<->valeur dans les deux sens. Les noms pointent
             * vers des literaux (duree de vie statique) ; aucune copie de chaine.
             */
            class NkEnumDescriptor {
                public:
                    NkEnumDescriptor() : mName(nullptr), mEntries() {}

                    explicit NkEnumDescriptor(const nk_char* enumName)
                        : mName(enumName), mEntries() {}

                    /** @brief Nom du type d'enum (literal ou typeid). */
                    const nk_char* GetName() const { return mName; }

                    void SetName(const nk_char* name) { mName = name; }

                    /** @brief Ajoute une paire {nom, valeur}. */
                    void Add(const nk_char* name, nk_int64 value) {
                        NkEnumEntry e;
                        e.name = name;
                        e.value = value;
                        mEntries.PushBack(e);
                    }

                    /** @brief Nombre d'enumerateurs. */
                    nk_usize GetCount() const { return mEntries.Size(); }

                    /** @brief Acces a l'entree i (i < GetCount()). */
                    const NkEnumEntry& GetEntryAt(nk_usize i) const { return mEntries[i]; }

                    /**
                     * @brief Convertit une valeur en nom symbolique.
                     * @param value Valeur sous-jacente recherchee.
                     * @return Nom de l'enumerateur ou nullptr si absent.
                     */
                    const nk_char* ToName(nk_int64 value) const {
                        for (nk_usize i = 0; i < mEntries.Size(); ++i) {
                            if (mEntries[i].value == value) {
                                return mEntries[i].name;
                            }
                        }
                        return nullptr;
                    }

                    /**
                     * @brief Convertit un nom symbolique en valeur.
                     * @param name Nom recherche.
                     * @param outValue Recoit la valeur si trouvee.
                     * @return true si le nom existe.
                     */
                    nk_bool ToValue(const nk_char* name, nk_int64& outValue) const {
                        if (!name) {
                            return false;
                        }
                        for (nk_usize i = 0; i < mEntries.Size(); ++i) {
                            if (mEntries[i].name && ::strcmp(mEntries[i].name, name) == 0) {
                                outValue = mEntries[i].value;
                                return true;
                            }
                        }
                        return false;
                    }

                private:
                    const nk_char* mName;
                    NkVector<NkEnumEntry> mEntries;
            };

            // =================================================================
            // CLASSE : NkEnumRegistry (singleton)
            // =================================================================
            /**
             * @class NkEnumRegistry
             * @brief Registre global des descripteurs d'enum, indexe par nom de
             *        type (typeid(E).name()).
             *
             * @note Thread-unsafe en ecriture (comme NkRegistry). Les
             *       enregistrements se font au demarrage via NKENTSEU_REFLECT_ENUM.
             */
            class NkEnumRegistry {
                public:
                    static NkEnumRegistry& Get() {
                        static NkEnumRegistry instance;
                        return instance;
                    }

                    /**
                     * @brief Enregistre (ou retourne) le descripteur pour le type T.
                     * @tparam E Type d'enum.
                     * @return Reference vers le descripteur persistant pour E.
                     *
                     * @note Le descripteur est un static local par type : sa duree
                     *       de vie couvre tout le programme. Idempotent.
                     */
                    template<typename E>
                    NkEnumDescriptor& GetOrCreate() {
                        static NkEnumDescriptor desc(typeid(E).name());
                        // Indexation (deduplication par adresse) pour la recherche
                        // dynamique par nom de type.
                        RegisterPtr(&desc);
                        return desc;
                    }

                    /** @brief Recherche un descripteur par nom de type (typeid). */
                    const NkEnumDescriptor* Find(const nk_char* typeName) const {
                        if (!typeName) {
                            return nullptr;
                        }
                        for (nk_usize i = 0; i < mCount; ++i) {
                            if (mDescs[i] && mDescs[i]->GetName() &&
                                ::strcmp(mDescs[i]->GetName(), typeName) == 0) {
                                return mDescs[i];
                            }
                        }
                        return nullptr;
                    }

                    /** @brief Recherche le descripteur pour le type C++ E. */
                    template<typename E>
                    const NkEnumDescriptor* FindFor() const {
                        return Find(typeid(E).name());
                    }

                    nk_usize GetCount() const { return mCount; }
                    const NkEnumDescriptor* GetAt(nk_usize i) const {
                        return i < mCount ? mDescs[i] : nullptr;
                    }

                private:
                    static constexpr nk_usize NK_MAX_ENUMS = 256;

                    NkEnumRegistry() : mCount(0) {
                        for (nk_usize i = 0; i < NK_MAX_ENUMS; ++i) {
                            mDescs[i] = nullptr;
                        }
                    }
                    NkEnumRegistry(const NkEnumRegistry&) = delete;
                    NkEnumRegistry& operator=(const NkEnumRegistry&) = delete;

                    void RegisterPtr(const NkEnumDescriptor* d) {
                        for (nk_usize i = 0; i < mCount; ++i) {
                            if (mDescs[i] == d) {
                                return;
                            }
                        }
                        if (mCount < NK_MAX_ENUMS) {
                            mDescs[mCount++] = d;
                        }
                    }

                    const NkEnumDescriptor* mDescs[NK_MAX_ENUMS];
                    nk_usize mCount;
            };

            // =================================================================
            // HELPERS LIBRES : NkEnumToString / NkEnumFromString
            // =================================================================
            /**
             * @brief Retourne le nom symbolique d'une valeur d'enum.
             * @tparam E Type d'enum reflechi (via NKENTSEU_REFLECT_ENUM).
             * @return Nom ou nullptr si non reflechi / valeur inconnue.
             */
            template<typename E>
            const nk_char* NkEnumToString(E value) {
                const NkEnumDescriptor* d = NkEnumRegistry::Get().FindFor<E>();
                if (!d) {
                    return nullptr;
                }
                return d->ToName(static_cast<nk_int64>(value));
            }

            /**
             * @brief Convertit un nom symbolique en valeur d'enum.
             * @tparam E Type d'enum reflechi.
             * @param name Nom recherche.
             * @param outValue Recoit la valeur (typee E) si trouvee.
             * @return true si succes.
             */
            template<typename E>
            nk_bool NkEnumFromString(const nk_char* name, E& outValue) {
                const NkEnumDescriptor* d = NkEnumRegistry::Get().FindFor<E>();
                if (!d) {
                    return false;
                }
                nk_int64 v = 0;
                if (!d->ToValue(name, v)) {
                    return false;
                }
                outValue = static_cast<E>(v);
                return true;
            }

        } // namespace reflection

    } // namespace nkentseu

    // =========================================================================
    // MACRO : NKENTSEU_REFLECT_ENUM(EnumType, V1, V2, ...)
    // =========================================================================
    // Enregistre un descripteur d'enum avec ses enumerateurs. A placer dans un
    // .cpp (une seule TU). Supporte jusqu'a 16 enumerateurs (extensible).
    //
    // Implementation : on utilise une expansion par comptage d'arguments. Chaque
    // enumerateur V devient un appel desc.Add("V", (nk_int64)EnumType::V).
    // =========================================================================

    // -- Helper : ajoute un enumerateur au descripteur ------------------------
    #define NK_REFLECT_ENUM_ENTRY(EnumType, V) \
        desc.Add(#V, static_cast<::nkentseu::nk_int64>(EnumType::V));

    // -- Comptage d'arguments (jusqu'a 16) ------------------------------------
    #define NK_REFLECT_ENUM_NARG(...) \
        NK_REFLECT_ENUM_NARG_(__VA_ARGS__, NK_REFLECT_ENUM_RSEQ())
    #define NK_REFLECT_ENUM_NARG_(...) NK_REFLECT_ENUM_ARG_N(__VA_ARGS__)
    #define NK_REFLECT_ENUM_ARG_N( \
        _1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,N,...) N
    #define NK_REFLECT_ENUM_RSEQ() 16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0

    // -- Application recursive de NK_REFLECT_ENUM_ENTRY -----------------------
    #define NK_REFLECT_ENUM_FE_1(E,V)        NK_REFLECT_ENUM_ENTRY(E,V)
    #define NK_REFLECT_ENUM_FE_2(E,V,...)    NK_REFLECT_ENUM_ENTRY(E,V) NK_REFLECT_ENUM_FE_1(E,__VA_ARGS__)
    #define NK_REFLECT_ENUM_FE_3(E,V,...)    NK_REFLECT_ENUM_ENTRY(E,V) NK_REFLECT_ENUM_FE_2(E,__VA_ARGS__)
    #define NK_REFLECT_ENUM_FE_4(E,V,...)    NK_REFLECT_ENUM_ENTRY(E,V) NK_REFLECT_ENUM_FE_3(E,__VA_ARGS__)
    #define NK_REFLECT_ENUM_FE_5(E,V,...)    NK_REFLECT_ENUM_ENTRY(E,V) NK_REFLECT_ENUM_FE_4(E,__VA_ARGS__)
    #define NK_REFLECT_ENUM_FE_6(E,V,...)    NK_REFLECT_ENUM_ENTRY(E,V) NK_REFLECT_ENUM_FE_5(E,__VA_ARGS__)
    #define NK_REFLECT_ENUM_FE_7(E,V,...)    NK_REFLECT_ENUM_ENTRY(E,V) NK_REFLECT_ENUM_FE_6(E,__VA_ARGS__)
    #define NK_REFLECT_ENUM_FE_8(E,V,...)    NK_REFLECT_ENUM_ENTRY(E,V) NK_REFLECT_ENUM_FE_7(E,__VA_ARGS__)
    #define NK_REFLECT_ENUM_FE_9(E,V,...)    NK_REFLECT_ENUM_ENTRY(E,V) NK_REFLECT_ENUM_FE_8(E,__VA_ARGS__)
    #define NK_REFLECT_ENUM_FE_10(E,V,...)   NK_REFLECT_ENUM_ENTRY(E,V) NK_REFLECT_ENUM_FE_9(E,__VA_ARGS__)
    #define NK_REFLECT_ENUM_FE_11(E,V,...)   NK_REFLECT_ENUM_ENTRY(E,V) NK_REFLECT_ENUM_FE_10(E,__VA_ARGS__)
    #define NK_REFLECT_ENUM_FE_12(E,V,...)   NK_REFLECT_ENUM_ENTRY(E,V) NK_REFLECT_ENUM_FE_11(E,__VA_ARGS__)
    #define NK_REFLECT_ENUM_FE_13(E,V,...)   NK_REFLECT_ENUM_ENTRY(E,V) NK_REFLECT_ENUM_FE_12(E,__VA_ARGS__)
    #define NK_REFLECT_ENUM_FE_14(E,V,...)   NK_REFLECT_ENUM_ENTRY(E,V) NK_REFLECT_ENUM_FE_13(E,__VA_ARGS__)
    #define NK_REFLECT_ENUM_FE_15(E,V,...)   NK_REFLECT_ENUM_ENTRY(E,V) NK_REFLECT_ENUM_FE_14(E,__VA_ARGS__)
    #define NK_REFLECT_ENUM_FE_16(E,V,...)   NK_REFLECT_ENUM_ENTRY(E,V) NK_REFLECT_ENUM_FE_15(E,__VA_ARGS__)

    #define NK_REFLECT_ENUM_FE_CAT(n)        NK_REFLECT_ENUM_FE_##n
    #define NK_REFLECT_ENUM_FE_EXPAND(n)     NK_REFLECT_ENUM_FE_CAT(n)
    #define NK_REFLECT_ENUM_FOREACH(E, ...) \
        NK_REFLECT_ENUM_FE_EXPAND(NK_REFLECT_ENUM_NARG(__VA_ARGS__))(E, __VA_ARGS__)

    /**
     * @def NKENTSEU_REFLECT_ENUM(EnumType, ...)
     * @brief Enregistre un descripteur d'enum + ses enumerateurs au demarrage.
     *
     * @example
     * @code
     * enum class Color : nk_uint8 { Red = 0, Green = 1, Blue = 2 };
     * NKENTSEU_REFLECT_ENUM(Color, Red, Green, Blue)   // dans un .cpp
     * @endcode
     */
    #define NKENTSEU_REFLECT_ENUM(EnumType, ...) \
        namespace { \
            struct EnumType##_NkEnumReg { \
                EnumType##_NkEnumReg() { \
                    ::nkentseu::reflection::NkEnumDescriptor& desc = \
                        ::nkentseu::reflection::NkEnumRegistry::Get().GetOrCreate<EnumType>(); \
                    /* Lie aussi le NkType de l'enum a sa categorie NK_ENUM. */ \
                    if (desc.GetCount() == 0) { \
                        NK_REFLECT_ENUM_FOREACH(EnumType, __VA_ARGS__) \
                    } \
                } \
            }; \
            static EnumType##_NkEnumReg g_##EnumType##_nkEnumReg{}; \
        }

#endif // NK_REFLECTION_NKENUMDESCRIPTOR_H

// ============================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// ============================================================
