// =============================================================================
// FICHIER  : Modules/System/NKReflection/src/NKReflection/NkClass.h
// MODULE   : NKReflection
// AUTEUR   : Rihen
// DATE     : 2026-02-07
// VERSION  : 1.1.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Meta-donnees runtime d'une classe : nom, type associe, heritage,
//   proprietes/methodes et callbacks de construction/destruction encapsules
//   dans des NkFunction pour une gestion type-safe et sans fuites memoire.
//
// AMELIORATIONS (v1.1.0) :
//   - Remplacement des pointeurs de fonction bruts par NkFunction<void*()> et NkFunction<void(void*)>
//   - Ajout de MakeFromClassType() pour l'enregistrement simplifie via templates
//   - Correction usize → nk_usize pour la coherence avec NKCore
//   - Documentation Doxygen complete pour chaque element public
//   - Exemples d'utilisation de NkBind pour l'enregistrement de methodes/proprietes
// =============================================================================

#pragma once

#ifndef NK_REFLECTION_NKCLASS_H
#define NK_REFLECTION_NKCLASS_H

    // -------------------------------------------------------------------------
    // SECTION 1 : DEPENDANCES INTERNES
    // -------------------------------------------------------------------------
    // Inclusion des meta-donnees de types, proprietes et methodes.
    // Inclusion de NkFunction pour le stockage type-safe des constructeurs/destructeurs.

    #include "NKReflection/NkType.h"
    #include "NKReflection/NkProperty.h"
    #include "NKReflection/NkMethod.h"
    #include "NKContainers/Functional/NkFunction.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : EN-TETES STANDARD MINIMAUX
    // -------------------------------------------------------------------------
    // Inclusion minimale de cstring pour les comparaisons de noms via strcmp.
    // Aucune autre dependance STL n'est utilisee pour respecter la contrainte
    // d'independance des conteneurs runtime du module.

    #include <cstring>

    // -------------------------------------------------------------------------
    // SECTION 3 : ESPACE DE NOMS PRINCIPAL
    // -------------------------------------------------------------------------
    // Declaration du namespace principal nkentseu et de son sous-namespace
    // reflection qui encapsule toutes les fonctionnalites du systeme de reflexion.

    namespace nkentseu {

        namespace reflection {

            // =================================================================
            // CLASSE : NkClass
            // =================================================================
            /**
             * @class NkClass
             * @brief Representation runtime des meta-donnees d'une classe
             *
             * Cette classe encapsule toutes les informations necessaires pour
             * decrire et manipuler une classe via le systeme de reflexion :
             * son identifiant, sa taille, son type de base, son arbre d'heritage,
             * ses proprietes, ses methodes et ses callbacks de cycle de vie.
             *
             * @note Le mecanisme de construction/destruction utilise NkFunction
             *       pour beneficier du Small Buffer Optimization et de la gestion
             *       automatique de la memoire, eliminant les risques de fuites.
             *
             * @note Les collections de proprietes et methodes utilisent des tableaux
             *       statiques de capacite fixe (64 elements) pour eviter les
             *       allocations dynamiques. Pour des besoins plus importants,
             *       envisagez d'utiliser NKContainers/NkArray avec un allocateur.
             */
            class NKENTSEU_REFLECTION_API NkClass {
                public:
                    // ---------------------------------------------------------
                    // ALIASES ET TYPES INTERNES
                    // ---------------------------------------------------------

                    /**
                     * @typedef ConstructorFn
                     * @brief Signature du callable pour la construction d'instance
                     *
                     * @return Pointeur void* vers l'instance nouvellement creee
                     *
                     * @note Cette signature est encapsulee dans un NkFunction pour
                     *       beneficier du type-erasure et de la gestion memoire automatique.
                     * @note Le callable doit allouer et initialiser l'instance,
                     *       puis retourner un pointeur valide ou nullptr en cas d'echec.
                     */
                    using ConstructorFn = nkentseu::NkFunction<void*(void)>;

                    /**
                     * @typedef DestructorFn
                     * @brief Signature du callable pour la destruction d'instance
                     *
                     * @param instance Pointeur vers l'instance a detruire
                     * @return void
                     *
                     * @note Cette signature est encapsulee dans un NkFunction pour
                     *       beneficier du type-erasure et de la gestion memoire automatique.
                     * @note Le callable doit liberer les ressources de l'instance
                     *       sans dealloquer la memoire de l'objet lui-meme
                     *       (la gestion de la memoire est externe au systeme de reflexion).
                     */
                    using DestructorFn = nkentseu::NkFunction<void(void*)>;

                    // ---------------------------------------------------------
                    // CONSTRUCTEURS
                    // ---------------------------------------------------------

                    /**
                     * @brief Constructeur principal de NkClass
                     * @param name Nom de la classe (chaine statique ou en duree de vie etendue)
                     * @param size Taille de la classe en octets (sizeof(T))
                     * @param type Reference vers le NkType associe a cette classe
                     *
                     * @note Les collections de proprietes et methodes sont initialisees
                     *       avec des tableaux statiques de capacite 64 elements.
                     * @note La classe de base est initialisee a nullptr et doit etre
                     *       definie via SetBaseClass() pour supporter l'heritage.
                     */
                    NkClass(
                        const nk_char* name,
                        nk_usize size,
                        const NkType& type
                    )
                        : mName(name)
                        , mSize(size)
                        , mType(&type)
                        , mBaseClass(nullptr)
                        , mProperties(mPropertyStorage)
                        , mPropertyCount(0)
                        , mMethods(mMethodStorage)
                        , mMethodCount(0)
                        , mConstructor()
                        , mDestructor() {
                    }

                    // ---------------------------------------------------------
                    // ACCESSEURS PUBLICS : INFORMATIONS GENERALES
                    // ---------------------------------------------------------

                    /**
                     * @brief Retourne le nom de la classe
                     * @return Pointeur vers une chaine de caracteres constante
                     */
                    const nk_char* GetName() const {
                        return mName;
                    }

                    /**
                     * @brief Retourne la taille de la classe en octets
                     * @return Valeur nk_usize representant sizeof(T) pour cette classe
                     */
                    nk_usize GetSize() const {
                        return mSize;
                    }

                    /**
                     * @brief Retourne le NkType associe a cette classe
                     * @return Reference constante vers le NkType de la classe
                     */
                    const NkType& GetType() const {
                        return *mType;
                    }

                    // ---------------------------------------------------------
                    // GESTION DE L'HERITAGE
                    // ---------------------------------------------------------

                    /**
                     * @brief Retourne la classe de base directe
                     * @return Pointeur constant vers le NkClass de la classe parente ou nullptr
                     * @note Retourne nullptr si cette classe n'a pas de classe de base
                     */
                    const NkClass* GetBaseClass() const {
                        return mBaseClass;
                    }

                    /**
                     * @brief Definit la classe de base pour supporter l'heritage
                     * @param base Pointeur constant vers le NkClass de la classe parente
                     * @note Cette methode etablit le lien d'heritage pour les recherches
                     *       de proprietes/methodes dans la hierarchie de classes
                     */
                    void SetBaseClass(const NkClass* base) {
                        mBaseClass = base;
                    }

                    /**
                     * @brief Verifie si cette classe derive (directement ou indirectement) d'une autre
                     * @param other Pointeur constant vers le NkClass a tester comme ancetre
                     * @return nk_bool vrai si other est dans la chaine d'heritage de this
                     *
                     * @note Cette methode parcourt recursivement la chaine mBaseClass
                     *       jusqu'a trouver une correspondance ou atteindre nullptr
                     *
                     * @example
                     * @code
                     * if (animalClass->IsSubclassOf(livingBeingClass)) {
                     *     // Animal derive de LivingBeing
                     * }
                     * @endcode
                     */
                    nk_bool IsSubclassOf(const NkClass* other) const {
                        const NkClass* current = this;
                        while (current) {
                            if (current == other) {
                                return true;
                            }
                            current = current->mBaseClass;
                        }
                        return false;
                    }

                    /**
                     * @brief Verifie si cette classe est un ancetre (direct ou indirect) d'une autre
                     * @param other Pointeur constant vers le NkClass a tester comme descendant
                     * @return nk_bool vrai si this est dans la chaine d'heritage de other
                     *
                     * @note Cette methode est l'inverse de IsSubclassOf()
                     * @note Retourne faux si other est nullptr
                     */
                    nk_bool IsSuperclassOf(const NkClass* other) const {
                        return other && other->IsSubclassOf(this);
                    }

                    // ---------------------------------------------------------
                    // GESTION DES PROPRIETES
                    // ---------------------------------------------------------

                    /**
                     * @brief Ajoute une propriete a la classe
                     * @param property Pointeur constant vers le NkProperty a enregistrer
                     * @note Les doublons ne sont pas detectes automatiquement :
                     *       c'est a l'appelant de garantir l'unicite des noms
                     * @note La capacite maximale est de 64 proprietes par classe
                     *       (limite du tableau statique mPropertyStorage)
                     */
                    void AddProperty(const NkProperty* property) {
                        if (!property) {
                            return;
                        }
                        if (mPropertyCount < 64) {
                            mProperties[mPropertyCount++] = property;
                        }
                    }

                    /**
                     * @brief Recherche une propriete par son nom dans la classe et ses bases
                     * @param name Nom de la propriete a rechercher
                     * @return Pointeur constant vers le NkProperty trouve ou nullptr
                     *
                     * @note La recherche commence dans la classe courante puis remonte
                     *       recursivement dans la chaine d'heritage via GetBaseClass()
                     * @note La comparaison des noms utilise strcmp pour une egalite exacte
                     *
                     * @example
                     * @code
                     * const NkProperty* healthProp = playerClass->GetProperty("health");
                     * if (healthProp) {
                     *     nk_int32 value = healthProp->GetValue<nk_int32>(instance);
                     * }
                     * @endcode
                     */
                    const NkProperty* GetProperty(const nk_char* name) const {
                        if (!name) {
                            return nullptr;
                        }
                        for (nk_usize i = 0; i < mPropertyCount; ++i) {
                            if (std::strcmp(mProperties[i]->GetName(), name) == 0) {
                                return mProperties[i];
                            }
                        }
                        if (mBaseClass) {
                            return mBaseClass->GetProperty(name);
                        }
                        return nullptr;
                    }

                    /**
                     * @brief Retourne une propriete par son index dans la classe courante
                     * @param index Index zero-base de la propriete a recuperer
                     * @return Pointeur constant vers le NkProperty ou nullptr si hors limites
                     * @note Cette methode ne remonte pas dans l'heritage : elle accede
                     *       uniquement aux proprietes declarees directement dans cette classe
                     */
                    const NkProperty* GetPropertyAt(nk_usize index) const {
                        return index < mPropertyCount ? mProperties[index] : nullptr;
                    }

                    /**
                     * @brief Retourne le nombre de proprietes declarees dans cette classe
                     * @return Valeur nk_usize representant le nombre de proprietes
                     * @note Ne compte pas les proprietes heritees des classes de base
                     */
                    nk_usize GetPropertyCount() const {
                        return mPropertyCount;
                    }

                    /**
                     * @brief Retourne le nombre total de proprietes accessibles (incluant heritage)
                     * @return Valeur nk_usize representant le nombre cumule de proprietes
                     * @note Parcours recursif de la chaine d'heritage pour le comptage
                     */
                    nk_usize GetTotalPropertyCount() const {
                        nk_usize total = mPropertyCount;
                        if (mBaseClass) {
                            total += mBaseClass->GetTotalPropertyCount();
                        }
                        return total;
                    }

                    // ---------------------------------------------------------
                    // GESTION DES METHODES
                    // ---------------------------------------------------------

                    /**
                     * @brief Ajoute une methode a la classe
                     * @param method Pointeur constant vers le NkMethod a enregistrer
                     * @note Les doublons ne sont pas detectes automatiquement :
                     *       c'est a l'appelant de garantir l'unicite des noms
                     * @note La capacite maximale est de 64 methodes par classe
                     *       (limite du tableau statique mMethodStorage)
                     */
                    void AddMethod(const NkMethod* method) {
                        if (!method) {
                            return;
                        }
                        if (mMethodCount < 64) {
                            mMethods[mMethodCount++] = method;
                        }
                    }

                    /**
                     * @brief Recherche une methode par son nom dans la classe et ses bases
                     * @param name Nom de la methode a rechercher
                     * @return Pointeur constant vers le NkMethod trouve ou nullptr
                     *
                     * @note La recherche commence dans la classe courante puis remonte
                     *       recursivement dans la chaine d'heritage via GetBaseClass()
                     * @note Ne gere pas la surcharge de methodes : retourne la premiere
                     *       correspondance par nom, independamment de la signature
                     *
                     * @example
                     * @code
                     * const NkMethod* updateMethod = entityClass->GetMethod("Update");
                     * if (updateMethod && updateMethod->HasInvoke()) {
                     *     updateMethod->Invoke(instance, args);
                     * }
                     * @endcode
                     */
                    const NkMethod* GetMethod(const nk_char* name) const {
                        if (!name) {
                            return nullptr;
                        }
                        for (nk_usize i = 0; i < mMethodCount; ++i) {
                            if (std::strcmp(mMethods[i]->GetName(), name) == 0) {
                                return mMethods[i];
                            }
                        }
                        if (mBaseClass) {
                            return mBaseClass->GetMethod(name);
                        }
                        return nullptr;
                    }

                    /**
                     * @brief Retourne une methode par son index dans la classe courante
                     * @param index Index zero-base de la methode a recuperer
                     * @return Pointeur constant vers le NkMethod ou nullptr si hors limites
                     * @note Cette methode ne remonte pas dans l'heritage : elle accede
                     *       uniquement aux methodes declarees directement dans cette classe
                     */
                    const NkMethod* GetMethodAt(nk_usize index) const {
                        return index < mMethodCount ? mMethods[index] : nullptr;
                    }

                    /**
                     * @brief Retourne le nombre de methodes declarees dans cette classe
                     * @return Valeur nk_usize representant le nombre de methodes
                     * @note Ne compte pas les methodes heritees des classes de base
                     */
                    nk_usize GetMethodCount() const {
                        return mMethodCount;
                    }

                    /**
                     * @brief Retourne le nombre total de methodes accessibles (incluant heritage)
                     * @return Valeur nk_usize representant le nombre cumule de methodes
                     * @note Parcours recursif de la chaine d'heritage pour le comptage
                     */
                    nk_usize GetTotalMethodCount() const {
                        nk_usize total = mMethodCount;
                        if (mBaseClass) {
                            total += mBaseClass->GetTotalMethodCount();
                        }
                        return total;
                    }

                    // ---------------------------------------------------------
                    // CYCLE DE VIE : CONSTRUCTION ET DESTRUCTION
                    // ---------------------------------------------------------

                    /**
                     * @brief Definit le callable pour la construction d'instances
                     * @param ctor NkFunction encapsulant la logique de construction
                     * @note Le callable doit retourner un pointeur void* vers une
                     *       instance valide de cette classe, ou nullptr en cas d'echec
                     * @note NkFunction gere automatiquement SBO ou allocation heap
                     */
                    void SetConstructor(ConstructorFn ctor) {
                        mConstructor = traits::NkMove(ctor);
                    }

                    /**
                     * @brief Definit le callable pour la destruction d'instances
                     * @param dtor NkFunction encapsulant la logique de destruction
                     * @note Le callable doit liberer les ressources de l'instance
                     *       sans dealloquer la memoire de l'objet lui-meme
                     * @note NkFunction gere automatiquement SBO ou allocation heap
                     */
                    void SetDestructor(DestructorFn dtor) {
                        mDestructor = traits::NkMove(dtor);
                    }

                    /**
                     * @brief Cree une nouvelle instance de cette classe via le constructeur enregistre
                     * @return Pointeur void* vers l'instance nouvellement creee ou nullptr
                     * @note Retourne nullptr si aucun constructeur n'est defini
                     * @note La memoire de l'instance doit etre liberee explicitement
                     *       via DestroyInstance() ou par l'appelant selon la politique memoire
                     */
                    void* CreateInstance() const {
                        return mConstructor.IsValid() ? mConstructor() : nullptr;
                    }

                    /**
                     * @brief Detruit une instance de cette classe via le destructeur enregistre
                     * @param instance Pointeur vers l'instance a detruire
                     * @note Ne fait rien si aucun destructeur n'est defini ou si instance est nullptr
                     * @note Ne dealloque pas la memoire de l'objet : c'est la responsabilite
                     *       de l'appelant ou d'un allocateur externe
                     */
                    void DestroyInstance(void* instance) const {
                        if (mDestructor.IsValid() && instance) {
                            mDestructor(instance);
                        }
                    }

                    /**
                     * @brief Verifie si un constructeur est defini pour cette classe
                     * @return nk_bool vrai si mConstructor contient un callable valide
                     */
                    nk_bool HasConstructor() const {
                        return mConstructor.IsValid();
                    }

                    /**
                     * @brief Verifie si un destructeur est defini pour cette classe
                     * @return nk_bool vrai si mDestructor contient un callable valide
                     */
                    nk_bool HasDestructor() const {
                        return mDestructor.IsValid();
                    }

                    // ---------------------------------------------------------
                    // METHODES STATIQUES D'AIDE A LA CREATION
                    // ---------------------------------------------------------

                    /**
                     * @brief Cree un NkClass a partir d'un type C++ via template
                     * @tparam T Type C++ de la classe a reflechir
                     * @return Instance de NkClass configuree avec nom, taille et type
                     *
                     * @note Cette methode utilise typeid(T).name() pour le nom et
                     *       sizeof(T)/alignof(T) pour les meta-donnees de base
                     * @note Le nom retourne par typeid est dependant du compilateur
                     *       (name mangling) : pour un nom lisible, utilisez une
                     *       surcharge avec parametre name explicite
                     *
                     * @example
                     * @code
                     * class Player { nk_int32 health; };
                     * auto playerClass = NkClass::MakeFromClassType<Player>("Player");
                     * @endcode
                     */
                    template<typename T>
                    static NkClass MakeFromClassType(const nk_char* name) {
                        const NkType& typeMeta = NkTypeOf<T>();
                        return NkClass(name, sizeof(T), typeMeta);
                    }

                    /**
                     * @brief Enregistre une propriete membre via NkBind (helper template)
                     * @tparam ClassType Type de la classe contenant la propriete
                     * @tparam ValueType Type de la valeur de la propriete
                     * @param classMeta Reference vers le NkClass a modifier
                     * @param memberPtr Pointeur vers le membre de donnee
                     * @param name Nom de la propriete pour la reflexion
                     * @param flags Drapeaux optionnels de qualification
                     * @return Reference vers le NkProperty cree pour chaining
                     *
                     * @note Cette methode calcule l'offset via offsetof et configure
                     *       la propriete pour l'acces direct par offset memoire
                     *
                     * @example
                     * @code
                     * struct Config { nk_bool fullscreen; };
                     * auto& configClass = NkClass::MakeFromClassType<Config>("Config");
                     * NkClass::RegisterMemberProperty(configClass, &Config::fullscreen, "fullscreen");
                     * @endcode
                     */
                    template<typename ClassType, typename ValueType>
                    static NkProperty& RegisterMemberProperty(
                        NkClass& classMeta,
                        ValueType ClassType::*memberPtr,
                        const nk_char* name,
                        nk_uint32 flags = 0
                    ) {
                        static NkProperty prop(
                            name,
                            NkTypeOf<ValueType>(),
                            reinterpret_cast<nk_usize>(
                                &(reinterpret_cast<ClassType*>(0)->*memberPtr)
                            ),
                            flags
                        );
                        classMeta.AddProperty(&prop);
                        return prop;
                    }

                    /**
                     * @brief Enregistre une methode membre via NkBind (helper template)
                     * @tparam ClassType Type de la classe contenant la methode
                     * @tparam ReturnType Type de retour de la methode
                     * @tparam Args Types des parametres de la methode
                     * @param classMeta Reference vers le NkClass a modifier
                     * @param methodPtr Pointeur vers la methode membre
                     * @param name Nom de la methode pour la reflexion
                     * @param flags Drapeaux optionnels de qualification
                     * @return Reference vers le NkMethod cree pour chaining
                     *
                     * @note Cette methode ne configure pas automatiquement l'invokeur :
                     *       l'appelant doit appeler method.SetInvoke() avec un
                     *       wrapper compatible void*(void*, void**) si l'invocation
                     *       reflexive est requise
                     *
                     * @example
                     * @code
                     * class Player { void TakeDamage(nk_int32 amount); };
                     * auto& playerClass = NkClass::MakeFromClassType<Player>("Player");
                     * auto& damageMethod = NkClass::RegisterMemberMethod(
                     *     playerClass, &Player::TakeDamage, "TakeDamage");
                     * // Configurer l'invokeur si necessaire :
                     * // damageMethod.SetInvoke(...);
                     * @endcode
                     */
                    template<typename ClassType, typename ReturnType, typename... Args>
                    static NkMethod& RegisterMemberMethod(
                        NkClass& classMeta,
                        ReturnType (ClassType::*methodPtr)(Args...),
                        const nk_char* name,
                        nk_uint32 flags = 0
                    ) {
                        static NkMethod method(
                            name,
                            NkTypeOf<ReturnType>(),
                            flags
                        );
                        // Enregistrement des types de parametres si necessaire
                        // method.SetParameters(...);
                        classMeta.AddMethod(&method);
                        return method;
                    }

                private:
                    // ---------------------------------------------------------
                    // MEMBRES PRIVES : METADONNEES DE BASE
                    // ---------------------------------------------------------
                    // Informations fondamentales sur la classe : identifiant,
                    // taille memoire, type de base et lien d'heritage.

                    const nk_char* mName;
                    nk_usize mSize;
                    const NkType* mType;
                    const NkClass* mBaseClass;

                    // ---------------------------------------------------------
                    // MEMBRES PRIVES : COLLECTION DE PROPRIETES
                    // ---------------------------------------------------------
                    // Tableau statique pour le stockage des pointeurs de proprietes.
                    // mProperties pointe vers mPropertyStorage pour permettre une
                    // eventuelle reallocation future sans changer l'API.
                    // Capacite maximale : 64 proprietes par classe.

                    const NkProperty* mPropertyStorage[64];
                    const NkProperty** mProperties;
                    nk_usize mPropertyCount;

                    // ---------------------------------------------------------
                    // MEMBRES PRIVES : COLLECTION DE METHODES
                    // ---------------------------------------------------------
                    // Tableau statique pour le stockage des pointeurs de methodes.
                    // mMethods pointe vers mMethodStorage pour permettre une
                    // eventuelle reallocation future sans changer l'API.
                    // Capacite maximale : 64 methodes par classe.

                    const NkMethod* mMethodStorage[64];
                    const NkMethod** mMethods;
                    nk_usize mMethodCount;

                    // ---------------------------------------------------------
                    // MEMBRES PRIVES : CYCLE DE VIE
                    // ---------------------------------------------------------
                    // Callables pour la construction et la destruction d'instances.
                    // Encapsules dans des NkFunction pour la gestion automatique
                    // de la memoire et le support SBO.

                    ConstructorFn mConstructor;
                    DestructorFn mDestructor;
            };

        } // namespace reflection

    } // namespace nkentseu

#endif // NK_REFLECTION_NKCLASS_H

// =============================================================================
// EXEMPLES D'UTILISATION - NkClass.h (avec NkBind/NkFunction)
// =============================================================================
//
// -----------------------------------------------------------------------------
// Exemple 1 : Creation basique d'une meta-classe
// -----------------------------------------------------------------------------
/*
    #include "NKReflection/NkClass.h"
    #include "NKReflection/NkType.h"

    namespace nkentseu {
    namespace example {

        void BasicClassRegistration() {
            // Creation via le helper template
            auto& playerClass = reflection::NkClass::MakeFromClassType<struct Player>("Player");

            // Affichage des meta-donnees de base
            printf("Class: %s\n", playerClass.GetName());
            printf("Size: %zu bytes\n", playerClass.GetSize());
            printf("Type category: %d\n",
                static_cast<int>(playerClass.GetType().GetCategory()));
        }

    }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Enregistrement de proprietes membres via helper
// -----------------------------------------------------------------------------
/*
    #include "NKReflection/NkClass.h"
    #include "NKReflection/NkProperty.h"

    namespace nkentseu {
    namespace example {

        struct Player {
            nk_int32 health;
            nk_float32 speed;
            nk_bool isAlive;
        };

        void RegisterPlayerProperties() {
            auto& playerClass = reflection::NkClass::MakeFromClassType<Player>("Player");

            // Enregistrement des proprietes via le helper template
            reflection::NkClass::RegisterMemberProperty<Player, nk_int32>(
                playerClass, &Player::health, "health");

            reflection::NkClass::RegisterMemberProperty<Player, nk_float32>(
                playerClass, &Player::speed, "speed");

            reflection::NkClass::RegisterMemberProperty<Player, nk_bool>(
                playerClass, &Player::isAlive, "isAlive",
                static_cast<nk_uint32>(reflection::NkPropertyFlags::NK_READ_ONLY));

            // Iteration sur les proprietes enregistrees
            for (nk_usize i = 0; i < playerClass.GetPropertyCount(); ++i) {
                const reflection::NkProperty* prop = playerClass.GetPropertyAt(i);
                if (prop) {
                    printf("Property: %s (type: %s)\n",
                        prop->GetName(),
                        prop->GetType().GetName());
                }
            }
        }

    }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Gestion de l'heritage et recherche dans la hierarchie
// -----------------------------------------------------------------------------
/*
    #include "NKReflection/NkClass.h"

    namespace nkentseu {
    namespace example {

        class Entity { public: nk_int32 id; };
        class Character : public Entity { public: nk_int32 level; };
        class Player : public Character { public: nk_char* name; };

        void InheritanceExample() {
            // Creation des meta-classes avec liens d'heritage
            auto& entityClass = reflection::NkClass::MakeFromClassType<Entity>("Entity");
            auto& characterClass = reflection::NkClass::MakeFromClassType<Character>("Character");
            auto& playerClass = reflection::NkClass::MakeFromClassType<Player>("Player");

            // Etablissement de la chaine d'heritage
            characterClass.SetBaseClass(&entityClass);
            playerClass.SetBaseClass(&characterClass);

            // Verification des relations d'heritage
            if (playerClass.IsSubclassOf(&entityClass)) {
                printf("Player is a subclass of Entity\n");
            }

            if (entityClass.IsSuperclassOf(&playerClass)) {
                printf("Entity is a superclass of Player\n");
            }

            // Recherche de propriete dans la hierarchie : "id" est declare dans Entity
            // mais accessible via playerClass.GetProperty() grace a la remontee d'heritage
            const reflection::NkProperty* idProp = playerClass.GetProperty("id");
            if (idProp) {
                printf("Found property 'id' via inheritance chain\n");
            }
        }

    }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Construction et destruction d'instances via NkFunction
// -----------------------------------------------------------------------------
/*
    #include "NKReflection/NkClass.h"
    #include "NKContainers/Functional/NkFunction.h"

    namespace nkentseu {
    namespace example {

        class GameObject {
        public:
            GameObject() : m_initialized(false) {}
            ~GameObject() { Cleanup(); }
            void Cleanup() { m_initialized = false; }
        private:
            nk_bool m_initialized;
        };

        void InstanceLifecycleExample() {
            auto& gameClass = reflection::NkClass::MakeFromClassType<GameObject>("GameObject");

            // Definition du constructeur via lambda encapsule dans NkFunction
            gameClass.SetConstructor(reflection::NkClass::ConstructorFn(
                []() -> void* {
                    return new GameObject();
                }
            ));

            // Definition du destructeur via lambda encapsule dans NkFunction
            gameClass.SetDestructor(reflection::NkClass::DestructorFn(
                [](void* instance) -> void {
                    GameObject* obj = static_cast<GameObject*>(instance);
                    if (obj) {
                        delete obj;
                    }
                }
            ));

            // Creation d'instance via le systeme de reflexion
            if (gameClass.HasConstructor()) {
                void* instance = gameClass.CreateInstance();
                if (instance) {
                    printf("Created GameObject instance at %p\n", instance);

                    // Utilisation de l'instance...

                    // Destruction via le systeme de reflexion
                    if (gameClass.HasDestructor()) {
                        gameClass.DestroyInstance(instance);
                        printf("Destroyed GameObject instance\n");
                    }
                }
            }
        }

    }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Verification de l'utilisation SBO pour les constructeurs
// -----------------------------------------------------------------------------
/*
    #include "NKReflection/NkClass.h"
    #include "NKContainers/Functional/NkFunction.h"

    namespace nkentseu {
    namespace example {

        void SboConstructorExample() {
            auto& simpleClass = reflection::NkClass::MakeFromClassType<struct Simple>("Simple");

            // Petit lambda constructeur : devrait tenir dans le buffer SBO (64 bytes)
            auto simpleCtor = []() -> void* {
                static int dummy;
                return &dummy;
            };

            simpleClass.SetConstructor(reflection::NkClass::ConstructorFn(simpleCtor));

            // Verification de l'utilisation du Small Buffer Optimization
            // Note : necessite un accesseur GetConstructor() public si besoin d'inspection
            // const auto& ctorFn = simpleClass.GetConstructor();
            // if (ctorFn.UsesSbo()) {
            //     printf("Constructor uses SBO (no heap allocation)\n");
            // }

            #if NK_FUNCTION_ENABLE_STATS
            // Statistiques memoire (uniquement en mode debug)
            printf("Global heap usage for NkFunction: %zu bytes\n",
                reflection::NkClass::ConstructorFn::GetGlobalStats().totalMemoryBytes);
            #endif
        }

    }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Iteration sur toutes les proprietes (incluant heritage)
// -----------------------------------------------------------------------------
/*
    #include "NKReflection/NkClass.h"

    namespace nkentseu {
    namespace example {

        void IterateAllProperties(const reflection::NkClass& classMeta, void* instance) {
            // Fonction recursive pour parcourir la chaine d'heritage
            auto visitClass = [&](const reflection::NkClass* current, auto&& self) -> void {
                if (!current) {
                    return;
                }

                // Traiter les proprietes de la classe courante
                for (nk_usize i = 0; i < current->GetPropertyCount(); ++i) {
                    const reflection::NkProperty* prop = current->GetPropertyAt(i);
                    if (prop && !prop->IsStatic()) {
                        printf("Property: %s (class: %s)\n",
                            prop->GetName(),
                            current->GetName());
                        // Exemple d'acces a la valeur si le type est connu
                        // auto value = prop->GetValue<nk_int32>(instance);
                    }
                }

                // Recursivement traiter la classe de base
                self(current->GetBaseClass(), self);
            };

            visitClass(&classMeta, visitClass);
        }

    }
    }
*/

// ============================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// ============================================================