// =============================================================================
// FICHIER  : Modules/System/NKReflection/src/NKReflection/NkProperty.h
// MODULE   : NKReflection
// AUTEUR   : Rihen
// DATE     : 2026-02-07
// VERSION  : 1.1.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Meta-donnees runtime d'une propriete (nom, type, offset, flags), avec
//   acces direct par offset memoire ou indirect via getter/setter encapsules
//   dans des NkFunction pour une gestion type-safe et sans fuites memoire.
//
// AMELIORATIONS (v1.1.0) :
//   - Remplacement des pointeurs de fonction bruts par NkFunction<void*(void*)>
//   - Ajout de MakeFromMember() pour lier automatiquement via NkBind
//   - Correction uint32 → nk_uint32 pour la coherence avec NKCore
//   - Documentation Doxygen complete pour chaque element public
//   - Exemples d'utilisation de NkBind en fin de fichier
// =============================================================================

#pragma once

#ifndef NK_REFLECTION_NKPROPERTY_H
#define NK_REFLECTION_NKPROPERTY_H

    // -------------------------------------------------------------------------
    // SECTION 1 : DEPENDANCES INTERNES
    // -------------------------------------------------------------------------
    // Inclusion des meta-donnees de types pour la description des signatures.
    // Inclusion du systeme d'assertion pour la validation des parametres runtime.
    // Inclusion de NkFunction pour le stockage type-safe des accesseurs.

    #include "NKReflection/NkType.h"
    #include "NKReflection/NkReflectMeta.h"
    #include "NKCore/Assert/NkAssert.h"
    #include "NKContainers/Functional/NkFunction.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : ESPACE DE NOMS PRINCIPAL
    // -------------------------------------------------------------------------
    // Declaration du namespace principal nkentseu et de son sous-namespace
    // reflection qui encapsule toutes les fonctionnalites du systeme de reflexion.

    namespace nkentseu {

        namespace reflection {

            // Forward-declaration : NkReflectVariant (valeur type-erased).
            // L'en-tete complet (NkReflectVariant.h, qui tire NkString) n'est
            // inclus que dans NkProperty.cpp pour garder ce header leger.
            class NkReflectVariant;

            // Forward-declaration : descripteur de conteneur (Phase 3). Defini
            // dans NkContainerTrait.h ; seul un pointeur est stocke ici.
            struct NkContainerDescriptor;

            // =================================================================
            // ENUMERATION : NkPropertyFlags
            // =================================================================
            /**
             * @enum NkPropertyFlags
             * @brief Drapeaux de qualification pour les proprietes reflechies
             *
             * Cette enumeration utilise un codage bit-a-bit pour permettre la
             * combinaison de plusieurs qualificatifs sur une meme propriete.
             * Chaque drapeau correspond a un attribut ou contrainte d'acces.
             *
             * @note Les valeurs sont des puissances de 2 pour permettre le masquage
             *       via l'operateur AND bit-a-bit (&).
             */
            enum class NkPropertyFlags : nk_uint32 {
                // Aucun drapeau positionne : propriete standard en lecture/ecriture
                NK_NONE = 0,

                // Propriete en lecture seule (aucun setter disponible)
                NK_READ_ONLY = 1 << 0,

                // Propriete en ecriture seule (aucun getter disponible)
                NK_WRITE_ONLY = 1 << 1,

                // Propriete statique de classe (non liee a une instance)
                NK_STATIC = 1 << 2,

                // Propriete constante (valeur immuable apres initialisation)
                NK_PCONST = 1 << 3,

                // Propriete transiente (exclue de la serialisation persistante)
                NK_TRANSIENT = 1 << 4,

                // Propriete marquee comme obsolete (depreciation)
                NK_PDEPRECATED = 1 << 5
            };

            // =================================================================
            // CLASSE : NkProperty
            // =================================================================
            /**
             * @class NkProperty
             * @brief Representation runtime des meta-donnees d'une propriete
             *
             * Cette classe encapsule toutes les informations necessaires pour
             * decrire et acceder a une propriete via le systeme de reflexion :
             * son identifiant, son type, son offset memoire, ses qualificatifs
             * et ses accesseurs (getter/setter) pour l'acces indirect.
             *
             * @note Le mecanisme d'acces utilise NkFunction pour beneficier du
             *       Small Buffer Optimization et de la gestion automatique de
             *       la memoire, eliminant les risques de fuites lors des copies.
             *
             * @note Deux modes d'acces sont supportes :
             *       1. Acces direct par offset (instance + offset → valeur)
             *       2. Acces indirect via getter/setter (pour proprietes calculees)
             */
            class NKENTSEU_REFLECTION_API NkProperty {
                public:
                    // ---------------------------------------------------------
                    // ALIASES ET TYPES INTERNES
                    // ---------------------------------------------------------

                    /**
                     * @typedef GetterFn
                     * @brief Signature du callable pour l'acces en lecture
                     *
                     * @param instance Pointeur vers l'instance de l'objet (nullptr pour static)
                     * @return Pointeur vers la valeur de la propriete
                     *
                     * @note Cette signature est encapsulee dans un NkFunction pour
                     *       beneficier du type-erasure et de la gestion memoire automatique.
                     * @note Pour les proprietes statiques, le parametre instance est ignore.
                     */
                    using GetterFn = nkentseu::NkFunction<void*(void*)>;

                    /**
                     * @typedef SetterFn
                     * @brief Signature du callable pour l'acces en ecriture
                     *
                     * @param instance Pointeur vers l'instance de l'objet (nullptr pour static)
                     * @param value Pointeur vers la nouvelle valeur a assigner
                     * @return void
                     *
                     * @note Cette signature est encapsulee dans un NkFunction pour
                     *       beneficier du type-erasure et de la gestion memoire automatique.
                     * @note Pour les proprietes statiques, le parametre instance est ignore.
                     */
                    using SetterFn = nkentseu::NkFunction<void(void*, void*)>;

                    // ---------------------------------------------------------
                    // CONSTRUCTEURS
                    // ---------------------------------------------------------

                    /**
                     * @brief Constructeur principal de NkProperty
                     * @param name Nom de la propriete (chaine statique ou en duree de vie etendue)
                     * @param type Reference vers le NkType du type de la propriete
                     * @param offset Offset memoire de la propriete dans la classe (offsetof)
                     * @param flags Combinaison de drapeaux NkPropertyFlags (defaut : NK_NONE)
                     *
                     * @note Les accesseurs getter/setter doivent etre definis separatement
                     *       via SetGetter()/SetSetter() si l'acces indirect est requis.
                     * @note Pour les proprietes statiques, l'offset est ignore.
                     */
                    NkProperty(
                        const nk_char* name,
                        const NkType& type,
                        nk_usize offset,
                        nk_uint32 flags = 0
                    )
                        : mName(name)
                        , mType(&type)
                        , mOffset(offset)
                        , mFlags(flags)
                        , mMetaFlags(NK_REFLECT_META_NONE)
                        , mEditMeta()
                        , mContainer(nullptr)
                        , mGetter()
                        , mSetter() {
                    }

                    // ---------------------------------------------------------
                    // ACCESSEURS PUBLICS : IDENTIFIANTS ET METADONNEES
                    // ---------------------------------------------------------

                    /**
                     * @brief Retourne le nom de la propriete
                     * @return Pointeur vers une chaine de caracteres constante
                     */
                    const nk_char* GetName() const {
                        return mName;
                    }

                    /**
                     * @brief Retourne le type de la propriete
                     * @return Reference constante vers le NkType de la propriete
                     */
                    const NkType& GetType() const {
                        return *mType;
                    }

                    /**
                     * @brief Retourne l'offset memoire de la propriete
                     * @return Valeur nk_usize representant l'offset en octets
                     * @note Retourne 0 pour les proprietes statiques
                     */
                    nk_usize GetOffset() const {
                        return mOffset;
                    }

                    /**
                     * @brief Retourne les drapeaux de qualification de la propriete
                     * @return Valeur nk_uint32 contenant la combinaison de NkPropertyFlags
                     */
                    nk_uint32 GetFlags() const {
                        return mFlags;
                    }

                    // ---------------------------------------------------------
                    // ACCESSEURS PUBLICS : QUALIFICATIFS DE PROPRIETE
                    // ---------------------------------------------------------

                    /**
                     * @brief Verifie si la propriete est en lecture seule
                     * @return nk_bool vrai si le drapeau NK_READ_ONLY est positionne
                     * @note SetValue() provoquera une assertion si appele sur une propriete read-only
                     */
                    nk_bool IsReadOnly() const {
                        return (mFlags & static_cast<nk_uint32>(NkPropertyFlags::NK_READ_ONLY)) != 0
                            || (mMetaFlags & NK_REFLECT_READONLY) != 0;
                    }

                    /**
                     * @brief Verifie si la propriete est en ecriture seule
                     * @return nk_bool vrai si le drapeau NK_WRITE_ONLY est positionne
                     * @note GetValue() retournera une valeur indefinie si appele sur une propriete write-only
                     */
                    nk_bool IsWriteOnly() const {
                        return (mFlags & static_cast<nk_uint32>(NkPropertyFlags::NK_WRITE_ONLY)) != 0;
                    }

                    /**
                     * @brief Verifie si la propriete est statique (de classe)
                     * @return nk_bool vrai si le drapeau NK_STATIC est positionne
                     * @note Pour les proprietes statiques, le parametre instance des accesseurs est ignore
                     */
                    nk_bool IsStatic() const {
                        return (mFlags & static_cast<nk_uint32>(NkPropertyFlags::NK_STATIC)) != 0;
                    }

                    /**
                     * @brief Verifie si la propriete est constante (immuable)
                     * @return nk_bool vrai si le drapeau NK_PCONST est positionne
                     * @note Les proprietes const ne devraient pas avoir de setter defini
                     */
                    nk_bool IsConst() const {
                        return (mFlags & static_cast<nk_uint32>(NkPropertyFlags::NK_PCONST)) != 0;
                    }

                    /**
                     * @brief Verifie si la propriete est transiente (exclue de la serialisation)
                     * @return nk_bool vrai si le drapeau NK_TRANSIENT est positionne
                     */
                    nk_bool IsTransient() const {
                        return (mFlags & static_cast<nk_uint32>(NkPropertyFlags::NK_TRANSIENT)) != 0
                            || (mMetaFlags & NK_REFLECT_TRANSIENT) != 0;
                    }

                    /**
                     * @brief Verifie si la propriete est marquee comme depreciee
                     * @return nk_bool vrai si le drapeau NK_PDEPRECATED est positionne
                     */
                    nk_bool IsDeprecated() const {
                        return (mFlags & static_cast<nk_uint32>(NkPropertyFlags::NK_PDEPRECATED)) != 0;
                    }

                    // ---------------------------------------------------------
                    // METADONNEES D'EDITION (Phase 3 — inspecteur editeur)
                    // ---------------------------------------------------------

                    /**
                     * @brief Retourne les drapeaux d'edition 64 bits (NkReflectMeta).
                     * @return Combinaison de NkReflectMeta (0 = aucun).
                     */
                    nk_uint64 GetMetaFlags() const {
                        return mMetaFlags;
                    }

                    /**
                     * @brief Definit les drapeaux d'edition 64 bits (remplace).
                     * @param flags Combinaison de NkReflectMeta.
                     */
                    void SetMetaFlags(nk_uint64 flags) {
                        mMetaFlags = flags;
                    }

                    /**
                     * @brief Ajoute (OR) des drapeaux d'edition aux existants.
                     */
                    void AddMetaFlags(nk_uint64 flags) {
                        mMetaFlags |= flags;
                    }

                    /**
                     * @brief Teste la presence d'un (ou plusieurs) drapeau meta.
                     * @param flag Drapeau(x) NkReflectMeta a tester.
                     * @return true si tous les bits de flag sont positionnes.
                     */
                    nk_bool HasFlag(NkReflectMeta flag) const {
                        return (mMetaFlags & static_cast<nk_uint64>(flag)) == static_cast<nk_uint64>(flag);
                    }

                    /**
                     * @brief Variante prenant un masque brut (nk_uint64).
                     */
                    nk_bool HasMetaFlag(nk_uint64 flag) const {
                        return (mMetaFlags & flag) == flag && flag != 0;
                    }

                    /**
                     * @brief Definit la plage d'edition (slider) + arme NK_REFLECT_RANGE.
                     */
                    void SetRange(nk_float32 minValue, nk_float32 maxValue) {
                        mEditMeta.rangeMin = minValue;
                        mEditMeta.rangeMax = maxValue;
                        mMetaFlags |= NK_REFLECT_RANGE;
                    }

                    /**
                     * @brief Recupere la plage d'edition.
                     * @param outMin Recoit la borne min.
                     * @param outMax Recoit la borne max.
                     * @return true si NK_REFLECT_RANGE est arme (plage valide).
                     */
                    nk_bool GetRange(nk_float32& outMin, nk_float32& outMax) const {
                        outMin = mEditMeta.rangeMin;
                        outMax = mEditMeta.rangeMax;
                        return (mMetaFlags & NK_REFLECT_RANGE) != 0;
                    }

                    /** @brief Definit le texte d'aide (tooltip). */
                    void SetTooltip(const nk_char* tooltip) {
                        mEditMeta.tooltip = tooltip;
                    }

                    /** @brief Retourne le tooltip (nullptr si absent). */
                    const nk_char* GetTooltip() const {
                        return mEditMeta.tooltip;
                    }

                    /** @brief Definit la categorie d'inspecteur. */
                    void SetCategory(const nk_char* category) {
                        mEditMeta.category = category;
                    }

                    /** @brief Retourne la categorie (nullptr si absente). */
                    const nk_char* GetCategory() const {
                        return mEditMeta.category;
                    }

                    /** @brief Definit le nom d'affichage lisible. */
                    void SetDisplayName(const nk_char* display) {
                        mEditMeta.display = display;
                    }

                    /** @brief Retourne le nom d'affichage (nullptr si absent). */
                    const nk_char* GetDisplayName() const {
                        return mEditMeta.display;
                    }

                    /** @brief Acces direct au bloc de metadonnees d'edition. */
                    const NkEditMeta& GetEditMeta() const {
                        return mEditMeta;
                    }

                    // ---------------------------------------------------------
                    // CONTENEUR (Phase 3 — NkVector<T> reflechi)
                    // ---------------------------------------------------------

                    /**
                     * @brief Associe un descripteur de conteneur a la propriete.
                     * @param desc Descripteur (duree de vie statique) ou nullptr.
                     * @note Pose la propriete comme conteneur sequentiel ; le
                     *       serializer ecrit/lit alors un tableau d'elements.
                     */
                    void SetContainer(const NkContainerDescriptor* desc) {
                        mContainer = desc;
                    }

                    /** @brief Descripteur de conteneur (nullptr si scalaire/objet). */
                    const NkContainerDescriptor* GetContainer() const {
                        return mContainer;
                    }

                    /** @brief Vrai si la propriete est un conteneur sequentiel reflechi. */
                    nk_bool IsContainer() const {
                        return mContainer != nullptr;
                    }

                    /**
                     * @brief Vrai si la propriete est cachee dans l'editeur.
                     * @note Combinaison NK_REFLECT_HIDE_EDITOR / NK_REFLECT_NO_EDIT.
                     */
                    nk_bool IsHiddenInEditor() const {
                        return (mMetaFlags & (NK_REFLECT_HIDE_EDITOR | NK_REFLECT_NO_EDIT)) != 0;
                    }

                    // ---------------------------------------------------------
                    // CONFIGURATION DES ACCESSEURS INDIRECTS
                    // ---------------------------------------------------------

                    /**
                     * @brief Definit le callable pour l'acces en lecture indirect
                     * @param getter NkFunction encapsulant la logique de lecture
                     * @note Le callable doit respecter la signature void*(void*)
                     * @note NkFunction gere automatiquement SBO ou allocation heap
                     */
                    void SetGetter(GetterFn getter) {
                        mGetter = traits::NkMove(getter);
                    }

                    /**
                     * @brief Definit le callable pour l'acces en ecriture indirect
                     * @param setter NkFunction encapsulant la logique d'ecriture
                     * @note Le callable doit respecter la signature void(void*, void*)
                     * @note NkFunction gere automatiquement SBO ou allocation heap
                     */
                    void SetSetter(SetterFn setter) {
                        mSetter = traits::NkMove(setter);
                    }

                    /**
                     * @brief Verifie si un getter est defini pour cette propriete
                     * @return nk_bool vrai si mGetter contient un callable valide
                     */
                    nk_bool HasGetter() const {
                        return mGetter.IsValid();
                    }

                    /**
                     * @brief Verifie si un setter est defini pour cette propriete
                     * @return nk_bool vrai si mSetter contient un callable valide
                     */
                    nk_bool HasSetter() const {
                        return mSetter.IsValid();
                    }

                    // ---------------------------------------------------------
                    // ACCES AUX VALEURS : TEMPLATE TYPE-SAFE
                    // ---------------------------------------------------------

                    /**
                     * @brief Lit la valeur de la propriete via getter ou acces direct
                     * @tparam T Type attendu de la valeur (doit correspondre au type declare)
                     * @param instance Pointeur vers l'instance de l'objet (nullptr pour static)
                     * @return Reference constante vers la valeur de type T
                     * @note Si un getter est defini, il est utilise ; sinon, acces direct par offset
                     * @note L'appelant doit garantir que T correspond au type reel de la propriete
                     */
                    template<typename T>
                    const T& GetValue(void* instance) const {
                        if (mGetter.IsValid()) {
                            return *static_cast<T*>(mGetter(instance));
                        }
                        return *reinterpret_cast<T*>(static_cast<nk_char*>(instance) + mOffset);
                    }

                    /**
                     * @brief Ecrit une valeur dans la propriete via setter ou acces direct
                     * @tparam T Type de la valeur a assigner (doit correspondre au type declare)
                     * @param instance Pointeur vers l'instance de l'objet (nullptr pour static)
                     * @param value Reference constante vers la valeur a assigner
                     * @note Une assertion est levee si la propriete est read-only
                     * @note Si un setter est defini, il est utilise ; sinon, ecriture directe par offset
                     */
                    template<typename T>
                    void SetValue(void* instance, const T& value) const {
                        NKENTSEU_ASSERT(!IsReadOnly());
                        if (mSetter.IsValid()) {
                            mSetter(instance, const_cast<T*>(&value));
                        } else {
                            *reinterpret_cast<T*>(static_cast<nk_char*>(instance) + mOffset) = value;
                        }
                    }

                    /**
                     * @brief Retourne un pointeur brut vers la valeur de la propriete
                     * @param instance Pointeur vers l'instance de l'objet (nullptr pour static)
                     * @return Pointeur void* vers l'emplacement memoire de la propriete
                     * @note Cette methode contourne les accesseurs et accede directement a la memoire
                     * @note A utiliser avec precaution : aucun cast de type n'est effectue
                     */
                    void* GetValuePtr(void* instance) const {
                        return static_cast<nk_char*>(instance) + mOffset;
                    }

                    /**
                     * @brief Variante const de GetValuePtr (lecture seule).
                     */
                    const void* GetValuePtr(const void* instance) const {
                        return static_cast<const nk_char*>(instance) + mOffset;
                    }

                    // ---------------------------------------------------------
                    // ACCES GENERIQUE TYPE-ERASED (non-templated)
                    // ---------------------------------------------------------

                    /**
                     * @brief Lit la valeur de la propriete sans connaitre son type a la compilation.
                     * @param instance Pointeur const vers l'instance.
                     * @return NkReflectVariant portant une copie de la valeur (invalide si echec).
                     *
                     * @note Mode offset direct uniquement. Si un getter indirect est defini,
                     *       celui-ci n'est PAS utilise (limite Phase 1) : on lit par offset.
                     * @note Pour NK_STRING, copie profonde de la NkString. Pour les primitifs
                     *       et types triviaux, copie binaire de taille connue.
                     */
                    NkReflectVariant GetValueGeneric(const void* instance) const;

                    /**
                     * @brief Ecrit une valeur depuis un NkReflectVariant (acces type-erased).
                     * @param instance Pointeur vers l'instance a modifier.
                     * @param value Variant source (doit etre du type/taille de la propriete).
                     * @return true si l'ecriture a eu lieu, false sinon (read-only, type incompatible).
                     *
                     * @note Respecte NK_READ_ONLY (no-op + retour false).
                     * @note Mode offset direct uniquement (le setter indirect n'est pas utilise
                     *       en Phase 1). Coercion numerique best-effort si la categorie differe
                     *       mais reste primitive.
                     */
                    nk_bool SetValueGeneric(void* instance, const NkReflectVariant& value) const;

                    // ---------------------------------------------------------
                    // METHODES STATIQUES D'AIDE A LA CREATION
                    // ---------------------------------------------------------

                    /**
                     * @brief Cree un NkProperty avec accesseurs via callables generiques
                     * @tparam GetterType Type du callable getter (signature compatible void*(void*))
                     * @tparam SetterType Type du callable setter (signature compatible void(void*, void*))
                     * @param name Nom de la propriete
                     * @param type Type de la propriete
                     * @param offset Offset memoire (ignore si accesseurs indirects fournis)
                     * @param getter Callable optionnel pour la lecture indirecte
                     * @param setter Callable optionnel pour l'ecriture indirecte
                     * @param flags Drapeaux optionnels de qualification
                     * @return Instance de NkProperty configuree et prete a l'emploi
                     *
                     * @note Cette methode utilise les constructeurs de NkFunction pour
                     *       encapsuler les callables avec gestion automatique SBO/heap.
                     *
                     * @example
                     * @code
                     * auto getter = [](void* obj) -> void* {
                     *     return &static_cast<MyClass*>(obj)->computedValue;
                     * };
                     * auto prop = NkProperty::MakeWithAccessors(
                     *     "Computed", NkTypeOf<int>(), 0, getter);
                     * @endcode
                     */
                    template<typename GetterType = void, typename SetterType = void>
                    static NkProperty MakeWithAccessors(
                        const nk_char* name,
                        const NkType& type,
                        nk_usize offset,
                        GetterType&& getter = GetterType(),
                        SetterType&& setter = SetterType(),
                        nk_uint32 flags = 0
                    ) {
                        NkProperty prop(name, type, offset, flags);
                        if constexpr (!traits::NkIsSame<GetterType, void>::value) {
                            prop.SetGetter(GetterFn(traits::NkForward<GetterType>(getter)));
                        }
                        if constexpr (!traits::NkIsSame<SetterType, void>::value) {
                            prop.SetSetter(SetterFn(traits::NkForward<SetterType>(setter)));
                        }
                        return prop;
                    }

                    /**
                     * @brief Cree un NkProperty lie a un membre de classe via NkBind
                     * @tparam ClassType Type de la classe contenant la propriete
                     * @tparam ValueType Type de la valeur de la propriete
                     * @param instance Pointeur vers l'instance cible (nullptr pour static)
                     * @param memberPtr Pointeur vers le membre de donnee
                     * @param name Nom de la propriete pour la reflexion
                     * @param typeMeta Meta-donnees du type de la propriete
                     * @param flags Drapeaux optionnels de qualification
                     * @return Instance de NkProperty configuree pour l'acces direct par offset
                     *
                     * @note Cette methode calcule automatiquement l'offset via offsetof
                     *       et configure la propriete pour l'acces direct (sans getter/setter).
                     *
                     * @example
                     * @code
                     * struct Player { nk_int32 health; };
                     * Player p;
                     * auto prop = NkProperty::MakeFromMember<Player, nk_int32>(
                     *     &p,
                     *     &Player::health,
                     *     "health",
                     *     NkTypeOf<nk_int32>()
                     * );
                     * @endcode
                     */
                    template<typename ClassType, typename ValueType>
                    static NkProperty MakeFromMember(
                        ClassType* instance,
                        ValueType ClassType::*memberPtr,
                        const nk_char* name,
                        const NkType& typeMeta,
                        nk_uint32 flags = 0
                    ) {
                        NKENTSEU_UNUSED(instance);
                        nk_usize offset = reinterpret_cast<nk_usize>(
                            &(reinterpret_cast<ClassType*>(0)->*memberPtr)
                        );
                        return NkProperty(name, typeMeta, offset, flags);
                    }

                    /**
                     * @brief Cree un NkProperty avec getter/setter lies a des methodes membres
                     * @tparam ClassType Type de la classe contenant les accesseurs
                     * @tparam ValueType Type de la valeur de la propriete
                     * @param instance Pointeur vers l'instance cible
                     * @param getterPtr Pointeur vers la methode getter (const ou non)
                     * @param setterPtr Pointeur vers la methode setter (optionnel)
                     * @param name Nom de la propriete pour la reflexion
                     * @param typeMeta Meta-donnees du type de la propriete
                     * @param flags Drapeaux optionnels de qualification
                     * @return Instance de NkProperty configuree pour l'acces indirect
                     *
                     * @note Cette methode utilise NkBind pour lier l'instance aux methodes
                     *       et genere automatiquement les wrappers compatibles avec
                     *       les signatures void*(void*) et void(void*, void*).
                     *
                     * @example
                     * @code
                     * class Config {
                     * public:
                     *     nk_int32 GetVolume() const { return m_volume; }
                     *     void SetVolume(nk_int32 v) { m_volume = v; }
                     * private:
                     *     nk_int32 m_volume;
                     * };
                     * Config cfg;
                     * auto prop = NkProperty::MakeFromAccessors(
                     *     &cfg,
                     *     &Config::GetVolume,
                     *     &Config::SetVolume,
                     *     "volume",
                     *     NkTypeOf<nk_int32>()
                     * );
                     * @endcode
                     */
                    template<typename ClassType, typename ValueType>
                    static NkProperty MakeFromAccessors(
                        ClassType* instance,
                        ValueType (ClassType::*getterPtr)() const,
                        void (ClassType::*setterPtr)(ValueType),
                        const nk_char* name,
                        const NkType& typeMeta,
                        nk_uint32 flags = 0
                    ) {
                        // Wrapper getter : adapte la signature membre vers void*(void*)
                        auto getterWrapper = [instance, getterPtr](void* obj) -> void* {
                            NKENTSEU_UNUSED(obj);
                            static ValueType result;
                            result = (instance->*getterPtr)();
                            return &result;
                        };

                        // Wrapper setter : adapte la signature membre vers void(void*, void*)
                        auto setterWrapper = [instance, setterPtr](void* obj, void* value) -> void {
                            NKENTSEU_UNUSED(obj);
                            (instance->*setterPtr)(*static_cast<ValueType*>(value));
                        };

                        NkProperty prop(name, typeMeta, 0, flags | static_cast<nk_uint32>(NkPropertyFlags::NK_READ_ONLY));
                        prop.SetGetter(GetterFn(traits::NkMove(getterWrapper)));
                        if (setterPtr != nullptr) {
                            prop.SetSetter(SetterFn(traits::NkMove(setterWrapper)));
                            // Retire le drapeau read-only si un setter est fourni
                            prop.mFlags &= ~static_cast<nk_uint32>(NkPropertyFlags::NK_READ_ONLY);
                        }
                        return prop;
                    }

                private:
                    // ---------------------------------------------------------
                    // UTILITAIRES INTERNES POUR L'ACCES GENERIQUE
                    // ---------------------------------------------------------

                    /** @brief Ecrit une valeur coercee vers un primitif destination. */
                    static nk_bool WritePrimitiveCoerced(void* dst, NkTypeCategory dstCat, const NkReflectVariant& value);

                    /** @brief Copie binaire brute de n octets (zero-STL). */
                    static void CopyRawBytes(void* dst, const void* src, nk_usize n);

                    // ---------------------------------------------------------
                    // MEMBRES PRIVES
                    // ---------------------------------------------------------
                    // Stockage des meta-donnees fondamentales de la propriete.
                    // Ces membres sont initialises dans le constructeur ou via
                    // les setters dedies et ne sont jamais modifies ensuite.

                    const nk_char* mName;
                    const NkType* mType;
                    nk_usize mOffset;
                    nk_uint32 mFlags;       ///< Flags legacy (NkPropertyFlags, nk_uint32)
                    nk_uint64 mMetaFlags;   ///< Flags d'edition 64 bits (NkReflectMeta)
                    NkEditMeta mEditMeta;   ///< Metadonnees d'edition (range/tooltip/categorie)
                    const NkContainerDescriptor* mContainer; ///< Descripteur conteneur (ou nullptr)
                    GetterFn mGetter;
                    SetterFn mSetter;
            };

        } // namespace reflection

    } // namespace nkentseu

#endif // NK_REFLECTION_NKPROPERTY_H

// =============================================================================
// EXEMPLES D'UTILISATION - NkProperty.h (avec NkBind/NkFunction)
// =============================================================================
//
// -----------------------------------------------------------------------------
// Exemple 1 : Acces direct par offset (cas standard)
// -----------------------------------------------------------------------------
/*
    #include "NKReflection/NkProperty.h"
    #include "NKReflection/NkType.h"

    namespace nkentseu {
    namespace example {

        struct Player {
            nk_int32 health;
            nk_float32 speed;
        };

        void DirectAccessExample() {
            Player p { 100, 5.5f };
            const reflection::NkType& intType = reflection::NkTypeOf<nk_int32>();

            // Creation d'une propriete avec acces direct par offset
            reflection::NkProperty healthProp(
                "health",
                intType,
                offsetof(Player, health)
            );

            // Lecture via template type-safe
            nk_int32 currentHealth = healthProp.GetValue<nk_int32>(&p);
            printf("Health: %d\n", currentHealth);

            // Ecriture via template type-safe
            healthProp.SetValue<nk_int32>(&p, currentHealth + 10);
            printf("New health: %d\n", p.health);

            // Acces brut via pointeur (usage avance)
            void* rawPtr = healthProp.GetValuePtr(&p);
            nk_int32* typedPtr = static_cast<nk_int32*>(rawPtr);
            *typedPtr = 200;
            printf("Direct write: %d\n", p.health);
        }

    }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Acces indirect via getter/setter avec lambdas
// -----------------------------------------------------------------------------
/*
    #include "NKReflection/NkProperty.h"
    #include "NKReflection/NkType.h"

    namespace nkentseu {
    namespace example {

        class Config {
        public:
            nk_float32 GetVolume() const { return m_volume * 100.0f; }
            void SetVolume(nk_float32 percent) { m_volume = percent / 100.0f; }
        private:
            nk_float32 m_volume; // Stocke en [0.0, 1.0]
        };

        void IndirectAccessExample() {
            Config cfg;
            const reflection::NkType& floatType = reflection::NkTypeOf<nk_float32>();

            // Getter lambda : retourne la valeur en pourcentage
            auto getter = [](void* instance) -> void* {
                static nk_float32 result;
                Config* cfg = static_cast<Config*>(instance);
                result = cfg->GetVolume();
                return &result;
            };

            // Setter lambda : convertit le pourcentage en valeur interne
            auto setter = [](void* instance, void* value) -> void {
                Config* cfg = static_cast<Config*>(instance);
                nk_float32 percent = *static_cast<nk_float32*>(value);
                cfg->SetVolume(percent);
            };

            // Creation de la propriete avec accesseurs indirects
            reflection::NkProperty volumeProp("volume", floatType, 0);
            volumeProp.SetGetter(reflection::NkProperty::GetterFn(getter));
            volumeProp.SetSetter(reflection::NkProperty::SetterFn(setter));

            // Utilisation via l'API reflexive
            nk_float32 vol = volumeProp.GetValue<nk_float32>(&cfg);
            printf("Volume: %.1f%%\n", vol);

            volumeProp.SetValue<nk_float32>(&cfg, 75.0f);
            printf("Set to 75%%, internal value: %.2f\n", cfg.GetVolume() / 100.0f);
        }

    }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Utilisation de MakeFromAccessors avec NkBind (conceptuel)
// -----------------------------------------------------------------------------
/*
    #include "NKReflection/NkProperty.h"
    #include "NKContainers/Functional/NkBind.h"

    namespace nkentseu {
    namespace example {

        class Settings {
        public:
            nk_bool IsFullscreen() const { return m_fullscreen; }
            void SetFullscreen(nk_bool value) { m_fullscreen = value; }
        private:
            nk_bool m_fullscreen;
        };

        void BoundAccessorsExample() {
            Settings settings;
            const reflection::NkType& boolType = reflection::NkTypeOf<nk_bool>();

            // Utilisation de la methode statique d'aide
            reflection::NkProperty fsProp = reflection::NkProperty::MakeFromAccessors(
                &settings,
                &Settings::IsFullscreen,
                &Settings::SetFullscreen,
                "fullscreen",
                boolType
            );

            // Lecture via l'API reflexive
            nk_bool current = fsProp.GetValue<nk_bool>(&settings);
            printf("Fullscreen: %s\n", current ? "yes" : "no");

            // Ecriture via l'API reflexive
            fsProp.SetValue<nk_bool>(&settings, !current);
            printf("Toggled to: %s\n", settings.IsFullscreen() ? "yes" : "no");
        }

    }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Propriete statique de classe
// -----------------------------------------------------------------------------
/*
    #include "NKReflection/NkProperty.h"
    #include "NKReflection/NkType.h"

    namespace nkentseu {
    namespace example {

        class GameConstants {
        public:
            static nk_int32 MaxPlayers;
        };

        nk_int32 GameConstants::MaxPlayers = 16;

        void StaticPropertyExample() {
            const reflection::NkType& intType = reflection::NkTypeOf<nk_int32>();

            // Creation d'une propriete statique : instance = nullptr, offset ignore
            reflection::NkProperty maxPlayersProp(
                "MaxPlayers",
                intType,
                0, // Offset non utilise pour les statiques
                static_cast<nk_uint32>(reflection::NkPropertyFlags::NK_STATIC)
            );

            // Acces via pointeur vers la variable statique
            void* staticInstance = nullptr;
            void* valuePtr = &GameConstants::MaxPlayers;

            // Lecture via acces direct (offset ignore, on utilise le pointeur fourni)
            nk_int32 max = *static_cast<nk_int32*>(valuePtr);
            printf("Max players: %d\n", max);

            // Ecriture directe
            *static_cast<nk_int32*>(valuePtr) = 32;
            printf("Updated to: %d\n", GameConstants::MaxPlayers);
        }

    }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Verification des flags et gestion read-only
// -----------------------------------------------------------------------------
/*
    #include "NKReflection/NkProperty.h"
    #include "NKReflection/NkType.h"
    #include "NKCore/Assert/NkAssert.h"

    namespace nkentseu {
    namespace example {

        void ReadOnlyPropertyExample() {
            struct ReadOnlyData {
                const nk_int32 id;
                ReadOnlyData(nk_int32 i) : id(i) {}
            };

            ReadOnlyData data(42);
            const reflection::NkType& intType = reflection::NkTypeOf<nk_int32>();

            // Propriete marquee read-only
            reflection::NkProperty idProp(
                "id",
                intType,
                offsetof(ReadOnlyData, id),
                static_cast<nk_uint32>(reflection::NkPropertyFlags::NK_READ_ONLY)
            );

            // Lecture autorisee
            nk_int32 currentId = idProp.GetValue<nk_int32>(&data);
            printf("ID: %d\n", currentId);

            // Ecriture interdite : assertion en mode debug
            #ifdef NKENTSEU_DEBUG
            // idProp.SetValue<nk_int32>(&data, 99); // Assertion failure
            #endif

            // Verification programmatique avant ecriture
            if (!idProp.IsReadOnly()) {
                idProp.SetValue<nk_int32>(&data, 99);
            } else {
                printf("Property '%s' is read-only, write skipped\n", idProp.GetName());
            }
        }

    }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Integration avec NkFunction SBO et statistiques memoire
// -----------------------------------------------------------------------------
/*
    #include "NKReflection/NkProperty.h"
    #include "NKContainers/Functional/NkFunction.h"

    namespace nkentseu {
    namespace example {

        void SboPropertyExample() {
            const reflection::NkType& intType = reflection::NkTypeOf<nk_int32>();

            // Petit lambda getter : devrait tenir dans le buffer SBO (64 bytes)
            auto simpleGetter = [](void* instance) -> void* {
                static nk_int32 cached = 0;
                NKENTSEU_UNUSED(instance);
                return &cached;
            };

            reflection::NkProperty cachedProp("cached", intType, 0);
            cachedProp.SetGetter(reflection::NkProperty::GetterFn(simpleGetter));

            // Verification de l'utilisation du Small Buffer Optimization
            const auto& getterFn = cachedProp.GetGetter(); // Hypothetique, a ajouter si besoin
            if (getterFn.UsesSbo()) {
                printf("Getter uses SBO (no heap allocation)\n");
            }

            #if NK_FUNCTION_ENABLE_STATS
            // Statistiques memoire (uniquement en mode debug)
            printf("Global heap usage for NkFunction: %zu bytes\n",
                reflection::NkProperty::GetterFn::GetGlobalStats().totalMemoryBytes);
            #endif
        }

    }
    }
*/

// ============================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// ============================================================