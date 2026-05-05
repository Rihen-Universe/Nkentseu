// =============================================================================
// FICHIER  : Modules/System/NKReflection/src/NKReflection/NkProperty.cpp
// MODULE   : NKReflection
// AUTEUR   : Rihen
// DATE     : 2026-02-07
// VERSION  : 1.1.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Unite de compilation pour les extensions runtime de NkProperty.
//   Contient les implementations non-inline et les utilitaires avances.
//
// AMELIORATIONS (v1.1.0) :
//   - Utilisation de NkFunction pour la gestion memoire automatique des accesseurs
//   - Exemples d'extensions avec NkBind pour la liaison simplifiee
//   - Documentation des patterns d'acces type-safe
// =============================================================================

// -------------------------------------------------------------------------
// INCLUSION DE L'EN-TETE CORRESPONDANT
// -------------------------------------------------------------------------
// Inclusion du fichier d'en-tete pour verifier la coherence des declarations.

#include "NKReflection/NkProperty.h"

// -------------------------------------------------------------------------
// DEPENDANCES COMPLEMENTAIRES
// -------------------------------------------------------------------------
// Inclusion optionnelle pour les extensions de logging et validation.

#include "NKLogger/NkLog.h"

// -------------------------------------------------------------------------
// ESPACE DE NOMS PRINCIPAL
// -------------------------------------------------------------------------
// Implementation dans le namespace reflection de nkentseu.

namespace nkentseu {

    namespace reflection {

        // -----------------------------------------------------------------
        // NOTE D'ARCHITECTURE : NkFunction vs Raw Pointer pour les Accesseurs
        // -----------------------------------------------------------------
        // L'utilisation de NkFunction<void*(void*)> et NkFunction<void(void*, void*)>
        // pour mGetter/mSetter apporte :
        //
        // 1. Small Buffer Optimization (SBO) :
        //    - Les petits lambdas (< 64 bytes) sont stockes inline
        //    - Aucune allocation heap pour les accesseurs simples
        //
        // 2. Gestion automatique de la memoire :
        //    - Destruction automatique via le destructeur de NkFunction
        //    - Pas de risque de fuite memoire lors des copies/deplacements
        //
        // 3. Type-safety :
        //    - Verification a la compilation de la signature des callables
        //    - Erreurs de type detectees tot dans le cycle de compilation
        //
        // 4. Support noexcept :
        //    - Specialisation NkFunction<R(Args...) noexcept> pour les
        //      accesseurs garantis sans exception
        //
        // 5. Copy/move semantics :
        //    - Les NkProperty sont copiables/deplacables sans code additionnel
        //      grace aux operateurs par defaut de NkFunction

        // -----------------------------------------------------------------
        // ESPACE RESERVE POUR FUTURES IMPLEMENTATIONS
        // -----------------------------------------------------------------
        // Les fonctions suivantes pourraient etre implementees ici si leur
        // complexite justifie une separation de l'en-tete :

        /*
        // Exemple : Wrapper generique pour getter membre via NkBind
        template<typename ClassType, typename ValueType>
        NkProperty::GetterFn MakeMemberGetter(
            ClassType* instance,
            ValueType (ClassType::*getter)() const
        ) {
            // Utilisation de NkBind pour lier l'instance au getter
            auto bound = nkentseu::NkBind(instance, getter);

            // Adaptation vers la signature void*(void*)
            return NkProperty::GetterFn(
                [bound = traits::NkMove(bound)](void* obj) -> void* {
                    NKENTSEU_UNUSED(obj);
                    static ValueType result;
                    result = bound();
                    return &result;
                }
            );
        }

        // Exemple : Validation de coherence type a l'acces
        template<typename ExpectedType>
        nk_bool ValidatePropertyType(const NkProperty& prop) {
            // Comparaison des identifiants de type via NkType
            const NkType& declared = prop.GetType();
            const NkType& expected = NkTypeOf<ExpectedType>();

            return declared.GetName() == expected.GetName()
                && declared.GetSize() == expected.GetSize();
        }

        // Exemple : Logging d'acces pour le debugging
        template<typename T>
        const T& DebugGetValue(const NkProperty& prop, void* instance) {
            NK_FOUNDATION_LOG_DEBUG(
                "Reading property '%s' of type '%s'",
                prop.GetName(),
                prop.GetType().GetName()
            );

            const T& value = prop.GetValue<T>(instance);

            NK_FOUNDATION_LOG_DEBUG(
                "Property '%s' read value: %s",
                prop.GetName(),
                ToString(value).c_str() // Hypothetique, necessite un ToString generic
            );

            return value;
        }
        */

        // -----------------------------------------------------------------
        // UTILITAIRE : Helper pour proprietes serialisables
        // -----------------------------------------------------------------
        /**
         * @brief Determine si une propriete doit etre incluse dans la serialisation
         * @param prop Reference constante vers la propriete a evaluer
         * @return nk_bool vrai si la propriete n'est ni transiente ni read-only/write-only incompatible
         *
         * @note Cette fonction est utile pour filtrer les proprietes lors de la
         *       generation de JSON, binaire, ou autre format de persistence.
         *
         * @example
         * @code
         * for (const NkProperty& prop : classMeta.GetProperties()) {
         *     if (IsPropertySerializable(prop)) {
         *         SerializeProperty(output, prop, instance);
         *     }
         * }
         * @endcode
         */
        nk_bool IsPropertySerializable(const NkProperty& prop) {
            // Exclure les proprietes transientes par definition
            if (prop.IsTransient()) {
                return false;
            }

            // Exclure les proprietes write-only (rien a lire pour serialiser)
            if (prop.IsWriteOnly()) {
                return false;
            }

            // Exclure les proprietes read-only si la deserialisation est requise
            // (optionnel : depend de si on supporte la lecture seule en serialisation)
            // if (prop.IsReadOnly() && requireDeserialization) { return false; }

            return true;
        }

    } // namespace reflection

} // namespace nkentseu

// =============================================================================
// EXEMPLES D'EXTENSIONS - NkProperty.cpp
// =============================================================================
//
// -----------------------------------------------------------------------------
// Exemple 1 : Serialisation JSON d'une propriete via reflexion
// -----------------------------------------------------------------------------
/*
    #include "NKReflection/NkProperty.h"
    #include "NKSerialization/NkJsonWriter.h" // Hypothetique

    namespace nkentseu {
    namespace reflection {
    namespace serialization {

        // Helper pour ecrire une propriete dans un flux JSON
        void WritePropertyToJson(
            NkJsonWriter& writer,
            const NkProperty& prop,
            void* instance
        ) {
            // Skip si non serialisable
            if (!IsPropertySerializable(prop)) {
                return;
            }

            // Debut du champ JSON : "nom": valeur
            writer.WriteKey(prop.GetName());

            // Dispatch sur le type pour le formatage approprie
            const NkType& type = prop.GetType();
            switch (type.GetCategory()) {
                case NkTypeCategory::NK_BOOL: {
                    nk_bool value = prop.GetValue<nk_bool>(instance);
                    writer.WriteBool(value);
                    break;
                }
                case NkTypeCategory::NK_INT32: {
                    nk_int32 value = prop.GetValue<nk_int32>(instance);
                    writer.WriteInt(value);
                    break;
                }
                case NkTypeCategory::NK_FLOAT32: {
                    nk_float32 value = prop.GetValue<nk_float32>(instance);
                    writer.WriteFloat(value);
                    break;
                }
                case NkTypeCategory::NK_STRING: {
                    // Hypothetique : gestion des chaines via NkString
                    // const NkString& value = prop.GetValue<NkString>(instance);
                    // writer.WriteString(value.Data());
                    break;
                }
                default:
                    // Types non supportes : ecriture d'un placeholder
                    writer.WriteNull();
                    NK_FOUNDATION_LOG_WARNING(
                        "Property '%s' has unsupported type for JSON serialization",
                        prop.GetName()
                    );
                    break;
            }
        }

    } // namespace serialization
    } // namespace reflection
    } // namespace nkentseu
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Deserialisation avec validation de type
// -----------------------------------------------------------------------------
/*
    namespace nkentseu {
    namespace reflection {
    namespace serialization {

        // Helper pour lire une propriete depuis un flux JSON avec validation
        template<typename JsonValue>
        nk_bool ReadPropertyFromJson(
            const JsonValue& jsonValue,
            const NkProperty& prop,
            void* instance
        ) {
            // Skip si non assignable (read-only)
            if (prop.IsReadOnly()) {
                NK_FOUNDATION_LOG_WARNING(
                    "Cannot deserialize read-only property '%s'",
                    prop.GetName()
                );
                return false;
            }

            // Validation du type JSON vs type declare
            const NkType& type = prop.GetType();
            nk_bool typeMatch = false;

            switch (type.GetCategory()) {
                case NkTypeCategory::NK_BOOL:
                    typeMatch = jsonValue.IsBool();
                    break;
                case NkTypeCategory::NK_INT32:
                case NkTypeCategory::NK_INT64:
                    typeMatch = jsonValue.IsNumber() && !jsonValue.IsFloat();
                    break;
                case NkTypeCategory::NK_FLOAT32:
                case NkTypeCategory::NK_FLOAT64:
                    typeMatch = jsonValue.IsNumber();
                    break;
                case NkTypeCategory::NK_STRING:
                    typeMatch = jsonValue.IsString();
                    break;
                default:
                    break;
            }

            if (!typeMatch) {
                NK_FOUNDATION_LOG_ERROR(
                    "Type mismatch for property '%s': expected %s, got JSON %s",
                    prop.GetName(),
                    type.GetName(),
                    jsonValue.GetTypeName()
                );
                return false;
            }

            // Extraction et assignation selon le type
            switch (type.GetCategory()) {
                case NkTypeCategory::NK_BOOL:
                    prop.SetValue<nk_bool>(instance, jsonValue.AsBool());
                    break;
                case NkTypeCategory::NK_INT32:
                    prop.SetValue<nk_int32>(instance, static_cast<nk_int32>(jsonValue.AsInt()));
                    break;
                case NkTypeCategory::NK_FLOAT32:
                    prop.SetValue<nk_float32>(instance, static_cast<nk_float32>(jsonValue.AsFloat()));
                    break;
                // ... autres cas
                default:
                    return false;
            }

            return true;
        }

    } // namespace serialization
    } // namespace reflection
    } // namespace nkentseu
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Cache de valeur pour proprietes calculees couteuses
// -----------------------------------------------------------------------------
/*
    namespace nkentseu {
    namespace reflection {
    namespace internal {

        // Structure pour cacher le resultat d'un getter couteux
        struct PropertyCache {
            const NkProperty* property;
            void* lastInstance;
            void* cachedValue;
            nk_uint64 lastAccessTick;
            nk_uint64 validityDurationNs; // Duree de validite en nanosecondes
            nk_usize hitCount;
            nk_usize missCount;

            PropertyCache(nk_uint64 validityMs = 100)
                : property(nullptr)
                , lastInstance(nullptr)
                , cachedValue(nullptr)
                , lastAccessTick(0)
                , validityDurationNs(validityMs * 1000000)
                , hitCount(0)
                , missCount(0)
            {}

            // Tentative de recuperation depuis le cache
            bool TryGet(void* instance, nk_uint64 currentTick, void** outValue) {
                if (property == nullptr || instance != lastInstance) {
                    ++missCount;
                    return false;
                }

                if (currentTick - lastAccessTick > validityDurationNs) {
                    ++missCount;
                    return false;
                }

                ++hitCount;
                if (outValue && cachedValue) {
                    *outValue = cachedValue;
                }
                return true;
            }

            // Mise a jour du cache avec un nouveau resultat
            void Update(const NkProperty* prop, void* instance, void* value, nk_uint64 tick) {
                property = prop;
                lastInstance = instance;
                cachedValue = value;
                lastAccessTick = tick;
            }

            // Statistiques d'efficacite du cache
            float GetHitRatio() const {
                nk_usize total = hitCount + missCount;
                return (total > 0) ? static_cast<float>(hitCount) / total : 0.0f;
            }

            // Liberation de la valeur cachee si allouee dynamiquement
            void Clear(memory::NkAllocator* allocator = nullptr) {
                if (cachedValue && allocator) {
                    allocator->Deallocate(cachedValue, 0); // Taille inconnue, a adapter
                }
                cachedValue = nullptr;
                property = nullptr;
                lastInstance = nullptr;
            }
        };

    } // namespace internal
    } // namespace reflection
    } // namespace nkentseu
*/

// ============================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// ============================================================