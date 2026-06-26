// =============================================================================
// FICHIER  : Modules/System/NKReflection/src/NKReflection/NkReflectVariant.h
// MODULE   : NKReflection
// AUTEUR   : Rihen
// DATE     : 2026-06-24
// VERSION  : 1.0.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Valeur type-erased portant un NkType* + un storage. Permet de lire/ecrire
//   une propriete reflechie SANS connaitre son type T a la compilation.
//   Utilise un small-buffer (SBO ~32 octets) pour les primitifs et petits types
//   et une allocation NKMemory (NkAlloc) pour les types plus volumineux.
//
//   Zero-STL : aucune dependance std::, allocations via nkentseu::memory::NkAlloc
//   / NkFree, conteneurs et chaines maison (NkString).
// =============================================================================

#pragma once

#ifndef NK_REFLECTION_NKREFLECTVARIANT_H
#define NK_REFLECTION_NKREFLECTVARIANT_H

    // -------------------------------------------------------------------------
    // SECTION 1 : DEPENDANCES INTERNES
    // -------------------------------------------------------------------------

    #include "NKReflection/NkType.h"
    #include "NKCore/NkTypes.h"
    #include "NKCore/NkTraits.h"
    #include "NKMemory/NkAllocator.h"
    #include "NKContainers/String/NkString.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : EN-TETE STANDARD MINIMAL
    // -------------------------------------------------------------------------
    // <new> pour le placement new (construction in-place du NkString sur un bloc
    // alloue via NkAlloc). Aucune autre dependance STL.

    #include <new>

    namespace nkentseu {

        namespace reflection {

            // =================================================================
            // CLASSE : NkReflectVariant
            // =================================================================
            /**
             * @class NkReflectVariant
             * @brief Conteneur de valeur type-erased pour la reflexion runtime.
             *
             * Porte un pointeur NkType* (decrivant la valeur stockee) et un
             * storage. Les types tenant dans le buffer interne (NK_SBO_SIZE octets)
             * et trivialement copiables sont stockes inline (aucune allocation).
             * Les types plus gros sont copies dans un bloc alloue via NkAlloc.
             *
             * @note NkString recoit un traitement special (categorie NK_STRING) :
             *       une copie profonde est faite dans un NkString alloue sur le heap
             *       reflexion, de sorte que ToString()/Get<NkString> fonctionnent.
             *
             * @note Cette classe respecte la regle des 5 : copie/move profonds,
             *       destructeur liberant le storage hors-SBO via NkFree.
             */
            class NKENTSEU_REFLECTION_API NkReflectVariant {
                public:
                    // Taille du small-buffer interne (suffisant pour bool/int/float/
                    // double/vec2/vec3/vec4<float> = 16 octets, marge a 32).
                    static constexpr nk_usize NK_SBO_SIZE = 32;

                    // ---------------------------------------------------------
                    // CONSTRUCTION / DESTRUCTION
                    // ---------------------------------------------------------

                    /** @brief Construit un variant invalide (vide). */
                    NkReflectVariant()
                        : mType(nullptr)
                        , mHeap(nullptr)
                        , mSize(0)
                        , mIsString(false) {
                        ZeroBuffer();
                    }

                    /** @brief Destructeur : libere le storage hors-SBO. */
                    ~NkReflectVariant() {
                        Clear();
                    }

                    /** @brief Constructeur de copie (deep copy). */
                    NkReflectVariant(const NkReflectVariant& other)
                        : mType(nullptr)
                        , mHeap(nullptr)
                        , mSize(0)
                        , mIsString(false) {
                        ZeroBuffer();
                        CopyFrom(other);
                    }

                    /** @brief Operateur d'affectation par copie. */
                    NkReflectVariant& operator=(const NkReflectVariant& other) {
                        if (this != &other) {
                            Clear();
                            CopyFrom(other);
                        }
                        return *this;
                    }

                    /** @brief Constructeur de deplacement. */
                    NkReflectVariant(NkReflectVariant&& other) noexcept
                        : mType(other.mType)
                        , mHeap(other.mHeap)
                        , mSize(other.mSize)
                        , mIsString(other.mIsString) {
                        CopyBufferRaw(other);
                        other.mType = nullptr;
                        other.mHeap = nullptr;
                        other.mSize = 0;
                        other.mIsString = false;
                        other.ZeroBuffer();
                    }

                    /** @brief Operateur d'affectation par deplacement. */
                    NkReflectVariant& operator=(NkReflectVariant&& other) noexcept {
                        if (this != &other) {
                            Clear();
                            mType = other.mType;
                            mHeap = other.mHeap;
                            mSize = other.mSize;
                            mIsString = other.mIsString;
                            CopyBufferRaw(other);
                            other.mType = nullptr;
                            other.mHeap = nullptr;
                            other.mSize = 0;
                            other.mIsString = false;
                            other.ZeroBuffer();
                        }
                        return *this;
                    }

                    // ---------------------------------------------------------
                    // FABRIQUE TYPEE
                    // ---------------------------------------------------------

                    /**
                     * @brief Cree un variant a partir d'une valeur typee.
                     * @tparam T Type C++ de la valeur source.
                     * @param value Valeur a copier dans le variant.
                     * @return Variant contenant une copie de value.
                     */
                    template<typename T>
                    static NkReflectVariant From(const T& value) {
                        NkReflectVariant v;
                        v.Set<T>(value);
                        return v;
                    }

                    // ---------------------------------------------------------
                    // SET / GET TYPES
                    // ---------------------------------------------------------

                    /**
                     * @brief Stocke une copie d'une valeur typee dans le variant.
                     * @tparam T Type C++ de la valeur.
                     */
                    template<typename T>
                    void Set(const T& value) {
                        Clear();
                        mType = &NkTypeOf<T>();
                        mSize = sizeof(T);
                        StoreTyped<T>(value);
                    }

                    /**
                     * @brief Recupere la valeur sous la forme T si compatible.
                     * @tparam T Type C++ attendu.
                     * @param out Reference recevant la valeur en cas de succes.
                     * @return true si le type stocke correspond a T.
                     */
                    template<typename T>
                    nk_bool Get(T& out) const {
                        if (!IsValid()) {
                            return false;
                        }
                        if (mType != &NkTypeOf<T>() && sizeof(T) != mSize) {
                            return false;
                        }
                        const void* p = DataPtr();
                        if (!p) {
                            return false;
                        }
                        out = *static_cast<const T*>(p);
                        return true;
                    }

                    /**
                     * @brief Recupere la valeur T, ou retourne def si incompatible.
                     */
                    template<typename T>
                    T GetOr(T def) const {
                        T tmp = def;
                        if (Get<T>(tmp)) {
                            return tmp;
                        }
                        return def;
                    }

                    // ---------------------------------------------------------
                    // ACCES BAS-NIVEAU (pour NkProperty::SetValueGeneric)
                    // ---------------------------------------------------------

                    /**
                     * @brief Pointeur const vers les octets de la valeur stockee.
                     * @return Pointeur ou nullptr si invalide.
                     */
                    const void* DataPtr() const {
                        if (!IsValid()) {
                            return nullptr;
                        }
                        if (mIsString) {
                            return mHeap;
                        }
                        return mHeap ? mHeap : static_cast<const void*>(mBuffer);
                    }

                    /**
                     * @brief Construit un variant a partir d'octets bruts + NkType.
                     *        Effectue une copie binaire (memcpy). A reserver aux
                     *        types trivialement copiables.
                     * @param type Meta-type decrivant la valeur.
                     * @param src Source des octets (taille = type->GetSize()).
                     */
                    static NkReflectVariant FromRaw(const NkType* type, const void* src) {
                        NkReflectVariant v;
                        if (!type || !src) {
                            return v;
                        }
                        v.mType = type;
                        v.mSize = type->GetSize();
                        v.StoreRaw(src, v.mSize);
                        return v;
                    }

                    // ---------------------------------------------------------
                    // INSPECTION
                    // ---------------------------------------------------------

                    /** @brief Meta-type de la valeur stockee (ou nullptr). */
                    const NkType* GetType() const {
                        return mType;
                    }

                    /** @brief Categorie de la valeur (NK_UNKNOWN si vide). */
                    NkTypeCategory GetCategory() const {
                        return mType ? mType->GetCategory() : NkTypeCategory::NK_UNKNOWN;
                    }

                    /** @brief Vrai si le variant porte une valeur. */
                    nk_bool IsValid() const {
                        return mType != nullptr;
                    }

                    /** @brief Libere le storage et remet le variant a vide. */
                    void Clear() {
                        if (mHeap) {
                            if (mIsString) {
                                DestroyHeapString();
                            } else {
                                memory::NkFree(mHeap);
                            }
                            mHeap = nullptr;
                        }
                        mType = nullptr;
                        mSize = 0;
                        mIsString = false;
                        ZeroBuffer();
                    }

                    // ---------------------------------------------------------
                    // COERCIONS BEST-EFFORT (alignees sur NkArchive)
                    // ---------------------------------------------------------

                    /** @brief Convertit la valeur en entier signe 64 bits. */
                    nk_int64 ToInt64() const {
                        if (!IsValid()) {
                            return 0;
                        }
                        switch (mType->GetCategory()) {
                            case NkTypeCategory::NK_BOOL:    return ReadAs<nk_bool>() ? 1 : 0;
                            case NkTypeCategory::NK_INT8:    return static_cast<nk_int64>(ReadAs<nk_int8>());
                            case NkTypeCategory::NK_INT16:   return static_cast<nk_int64>(ReadAs<nk_int16>());
                            case NkTypeCategory::NK_INT32:   return static_cast<nk_int64>(ReadAs<nk_int32>());
                            case NkTypeCategory::NK_INT64:   return ReadAs<nk_int64>();
                            case NkTypeCategory::NK_UINT8:   return static_cast<nk_int64>(ReadAs<nk_uint8>());
                            case NkTypeCategory::NK_UINT16:  return static_cast<nk_int64>(ReadAs<nk_uint16>());
                            case NkTypeCategory::NK_UINT32:  return static_cast<nk_int64>(ReadAs<nk_uint32>());
                            case NkTypeCategory::NK_UINT64:  return static_cast<nk_int64>(ReadAs<nk_uint64>());
                            case NkTypeCategory::NK_FLOAT32: return static_cast<nk_int64>(ReadAs<nk_float32>());
                            case NkTypeCategory::NK_FLOAT64: return static_cast<nk_int64>(ReadAs<nk_float64>());
                            // Enum : lecture de la valeur sous-jacente selon la
                            // taille du type (1/2/4/8 octets). On suppose un type
                            // sous-jacent signe (cas usuel des enum class).
                            case NkTypeCategory::NK_ENUM:    return ReadEnumUnderlying();
                            default: return 0;
                        }
                    }

                    /** @brief Convertit la valeur en flottant double precision. */
                    nk_float64 ToFloat64() const {
                        if (!IsValid()) {
                            return 0.0;
                        }
                        switch (mType->GetCategory()) {
                            case NkTypeCategory::NK_BOOL:    return ReadAs<nk_bool>() ? 1.0 : 0.0;
                            case NkTypeCategory::NK_INT8:    return static_cast<nk_float64>(ReadAs<nk_int8>());
                            case NkTypeCategory::NK_INT16:   return static_cast<nk_float64>(ReadAs<nk_int16>());
                            case NkTypeCategory::NK_INT32:   return static_cast<nk_float64>(ReadAs<nk_int32>());
                            case NkTypeCategory::NK_INT64:   return static_cast<nk_float64>(ReadAs<nk_int64>());
                            case NkTypeCategory::NK_UINT8:   return static_cast<nk_float64>(ReadAs<nk_uint8>());
                            case NkTypeCategory::NK_UINT16:  return static_cast<nk_float64>(ReadAs<nk_uint16>());
                            case NkTypeCategory::NK_UINT32:  return static_cast<nk_float64>(ReadAs<nk_uint32>());
                            case NkTypeCategory::NK_UINT64:  return static_cast<nk_float64>(ReadAs<nk_uint64>());
                            case NkTypeCategory::NK_FLOAT32: return static_cast<nk_float64>(ReadAs<nk_float32>());
                            case NkTypeCategory::NK_FLOAT64: return ReadAs<nk_float64>();
                            case NkTypeCategory::NK_ENUM:    return static_cast<nk_float64>(ReadEnumUnderlying());
                            default: return 0.0;
                        }
                    }

                    /** @brief Convertit la valeur en booleen. */
                    nk_bool ToBool() const {
                        if (!IsValid()) {
                            return false;
                        }
                        if (mType->GetCategory() == NkTypeCategory::NK_FLOAT32) {
                            return ReadAs<nk_float32>() != 0.0f;
                        }
                        if (mType->GetCategory() == NkTypeCategory::NK_FLOAT64) {
                            return ReadAs<nk_float64>() != 0.0;
                        }
                        if (mType->GetCategory() == NkTypeCategory::NK_STRING) {
                            const NkString* s = HeapString();
                            return s && !s->Empty();
                        }
                        return ToInt64() != 0;
                    }

                    /**
                     * @brief Convertit la valeur en NkString (representation texte).
                     * @return NkString best-effort selon la categorie.
                     */
                    NkString ToString() const {
                        if (!IsValid()) {
                            return NkString();
                        }
                        if (mType->GetCategory() == NkTypeCategory::NK_STRING) {
                            const NkString* s = HeapString();
                            return s ? *s : NkString();
                        }
                        if (mType->GetCategory() == NkTypeCategory::NK_BOOL) {
                            return NkString(ReadAs<nk_bool>() ? "true" : "false");
                        }
                        if (mType->GetCategory() == NkTypeCategory::NK_FLOAT32 ||
                            mType->GetCategory() == NkTypeCategory::NK_FLOAT64) {
                            return NkString::Format("%g", ToFloat64());
                        }
                        if (mType->IsPrimitive()) {
                            return NkString::Format("%lld", static_cast<long long>(ToInt64()));
                        }
                        // Type non textuel : nom du type comme repli.
                        return NkString(mType->GetName());
                    }

                private:
                    // ---------------------------------------------------------
                    // STOCKAGE TYPE
                    // ---------------------------------------------------------

                    // NkString : copie profonde dans un NkString alloue (heap reflexion).
                    template<typename T>
                    void StoreTyped(const T& value) {
                        StoreTypedImpl<T>(value, traits::NkIsSame<T, NkString>());
                    }

                    // Specialisation NkString.
                    template<typename T>
                    void StoreTypedImpl(const T& value, traits::NkTrueType) {
                        mIsString = true;
                        void* mem = memory::NkAlloc(sizeof(NkString));
                        mHeap = new (mem) NkString(value);
                    }

                    // Cas general : copie binaire (SBO ou heap).
                    template<typename T>
                    void StoreTypedImpl(const T& value, traits::NkFalseType) {
                        StoreRaw(&value, sizeof(T));
                    }

                    // Copie binaire brute (types trivialement copiables).
                    void StoreRaw(const void* src, nk_usize size) {
                        mIsString = false;
                        if (size <= NK_SBO_SIZE) {
                            CopyBytes(mBuffer, src, size);
                            mHeap = nullptr;
                        } else {
                            mHeap = memory::NkAlloc(size);
                            CopyBytes(mHeap, src, size);
                        }
                    }

                    // ---------------------------------------------------------
                    // COPIE / DEPLACEMENT INTERNE
                    // ---------------------------------------------------------

                    void CopyFrom(const NkReflectVariant& other) {
                        mType = other.mType;
                        mSize = other.mSize;
                        mIsString = other.mIsString;
                        if (!other.IsValid()) {
                            return;
                        }
                        if (other.mIsString) {
                            const NkString* s = other.HeapString();
                            void* mem = memory::NkAlloc(sizeof(NkString));
                            mHeap = new (mem) NkString(s ? *s : NkString());
                        } else if (other.mHeap) {
                            mHeap = memory::NkAlloc(mSize);
                            CopyBytes(mHeap, other.mHeap, mSize);
                        } else {
                            CopyBytes(mBuffer, other.mBuffer, mSize);
                            mHeap = nullptr;
                        }
                    }

                    // Recopie brute du buffer SBO lors d'un move (pointeur heap deja
                    // transfere par le caller).
                    void CopyBufferRaw(const NkReflectVariant& other) {
                        CopyBytes(mBuffer, other.mBuffer, NK_SBO_SIZE);
                    }

                    // ---------------------------------------------------------
                    // ACCES NkString HEAP
                    // ---------------------------------------------------------

                    NkString* HeapString() {
                        return mIsString ? static_cast<NkString*>(mHeap) : nullptr;
                    }
                    const NkString* HeapString() const {
                        return mIsString ? static_cast<const NkString*>(mHeap) : nullptr;
                    }

                    void DestroyHeapString() {
                        NkString* s = static_cast<NkString*>(mHeap);
                        s->~NkString();
                        memory::NkFree(mHeap);
                    }

                    // ---------------------------------------------------------
                    // LECTURE TYPEE INTERNE (coercions)
                    // ---------------------------------------------------------

                    template<typename T>
                    T ReadAs() const {
                        const void* p = DataPtr();
                        if (!p || mSize < sizeof(T)) {
                            return T();
                        }
                        T out;
                        CopyBytes(&out, p, sizeof(T));
                        return out;
                    }

                    // Lit la valeur sous-jacente d'un enum (taille 1/2/4/8 octets)
                    // en supposant un type sous-jacent signe. Retourne 0 si vide.
                    nk_int64 ReadEnumUnderlying() const {
                        switch (mSize) {
                            case 1: return static_cast<nk_int64>(ReadAs<nk_int8>());
                            case 2: return static_cast<nk_int64>(ReadAs<nk_int16>());
                            case 4: return static_cast<nk_int64>(ReadAs<nk_int32>());
                            case 8: return ReadAs<nk_int64>();
                            default: return 0;
                        }
                    }

                    // ---------------------------------------------------------
                    // UTILITAIRES OCTETS (zero-STL, pas de memcpy/memset std::)
                    // ---------------------------------------------------------

                    static void CopyBytes(void* dst, const void* src, nk_usize n) {
                        nk_uint8* d = static_cast<nk_uint8*>(dst);
                        const nk_uint8* s = static_cast<const nk_uint8*>(src);
                        for (nk_usize i = 0; i < n; ++i) {
                            d[i] = s[i];
                        }
                    }

                    void ZeroBuffer() {
                        for (nk_usize i = 0; i < NK_SBO_SIZE; ++i) {
                            mBuffer[i] = 0;
                        }
                    }

                    // ---------------------------------------------------------
                    // MEMBRES
                    // ---------------------------------------------------------

                    const NkType* mType;   ///< Meta-type de la valeur (nullptr = vide)
                    void*         mHeap;    ///< Bloc heap (NkString* si mIsString, sinon octets bruts)
                    nk_usize      mSize;    ///< Taille en octets de la valeur stockee
                    nk_bool       mIsString;///< Vrai si la valeur est un NkString sur le heap
                    alignas(16) nk_uint8 mBuffer[NK_SBO_SIZE]; ///< Small-buffer inline
            };

        } // namespace reflection

    } // namespace nkentseu

#endif // NK_REFLECTION_NKREFLECTVARIANT_H

// ============================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// ============================================================
