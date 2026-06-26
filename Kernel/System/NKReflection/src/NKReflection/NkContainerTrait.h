// =============================================================================
// FICHIER  : Modules/System/NKReflection/src/NKReflection/NkContainerTrait.h
// MODULE   : NKReflection
// AUTEUR   : Rihen
// DATE     : 2026-06-24
// VERSION  : 1.0.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Reflexion des conteneurs sequentiels (Phase 3). Fournit un descripteur
//   type-erased NkContainerDescriptor permettant d'iterer/muter un conteneur
//   (NkVector<T>) via une instance void* sans connaitre T a la compilation :
//   GetCount, GetElementPtr, PushBackDefault, Clear + NkType de l'element.
//
//   Mecanique : NkContainerOf<NkVector<T>>() genere (static local) un descripteur
//   dont les callbacks sont des thunks templates instancies pour T. Aucune
//   allocation : function pointers + NkType*.
//
//   Zero-STL : conteneurs maison, aucune dependance std::.
// =============================================================================

#pragma once

#ifndef NK_REFLECTION_NKCONTAINERTRAIT_H
#define NK_REFLECTION_NKCONTAINERTRAIT_H

    #include "NKCore/NkTypes.h"
    #include "NKCore/NkTraits.h"
    #include "NKReflection/NkType.h"
    #include "NKContainers/Sequential/NkVector.h"

    namespace nkentseu {

        namespace reflection {

            // =================================================================
            // STRUCTURE : NkContainerDescriptor (type-erased)
            // =================================================================
            /**
             * @struct NkContainerDescriptor
             * @brief Vue type-erased d'un conteneur sequentiel reflechi.
             *
             * Toutes les operations prennent un void* vers l'instance du conteneur
             * (ex. NkVector<nk_int32>*). Le NkType de l'element permet de router la
             * (de)serialisation de chaque element via les memes regles que les
             * proprietes scalaires/objets.
             */
            struct NkContainerDescriptor {
                using GetCountFn       = nk_usize (*)(const void* container);
                using GetElementFn     = void*    (*)(void* container, nk_usize index);
                using GetElementCFn     = const void* (*)(const void* container, nk_usize index);
                using PushBackFn       = void*    (*)(void* container); // retourne ptr du nouvel element
                using ClearFn          = void     (*)(void* container);
                using ResizeFn         = void     (*)(void* container, nk_usize count);

                const NkType*  elementType = nullptr;  ///< NkType de l'element
                GetCountFn     getCount    = nullptr;
                GetElementFn   getElement  = nullptr;
                GetElementCFn  getElementC = nullptr;
                PushBackFn     pushBack     = nullptr;  ///< push_back(T{}) -> &element
                ClearFn        clear        = nullptr;
                ResizeFn       resize       = nullptr;

                nk_bool IsValid() const {
                    return elementType != nullptr && getCount != nullptr;
                }

                nk_usize GetCount(const void* container) const {
                    return getCount ? getCount(container) : 0;
                }
                void* GetElementPtr(void* container, nk_usize index) const {
                    return getElement ? getElement(container, index) : nullptr;
                }
                const void* GetElementPtr(const void* container, nk_usize index) const {
                    return getElementC ? getElementC(container, index) : nullptr;
                }
                void* PushBackDefault(void* container) const {
                    return pushBack ? pushBack(container) : nullptr;
                }
                void Clear(void* container) const {
                    if (clear) {
                        clear(container);
                    }
                }
            };

            // =================================================================
            // THUNKS TEMPLATES : NkVector<T>
            // =================================================================
            namespace detail {

                template<typename T>
                nk_usize NkVecGetCount(const void* c) {
                    return static_cast<const NkVector<T>*>(c)->Size();
                }

                template<typename T>
                void* NkVecGetElement(void* c, nk_usize i) {
                    NkVector<T>* v = static_cast<NkVector<T>*>(c);
                    return &(*v)[i];
                }

                template<typename T>
                const void* NkVecGetElementC(const void* c, nk_usize i) {
                    const NkVector<T>* v = static_cast<const NkVector<T>*>(c);
                    return &(*v)[i];
                }

                template<typename T>
                void* NkVecPushBack(void* c) {
                    NkVector<T>* v = static_cast<NkVector<T>*>(c);
                    v->PushBack(T());
                    return &(*v)[v->Size() - 1];
                }

                template<typename T>
                void NkVecClear(void* c) {
                    static_cast<NkVector<T>*>(c)->Clear();
                }

                template<typename T>
                void NkVecResize(void* c, nk_usize n) {
                    static_cast<NkVector<T>*>(c)->Resize(n);
                }

            } // namespace detail

            // =================================================================
            // FABRIQUE : NkContainerOf<Container>()
            // =================================================================
            /**
             * @brief Trait primaire : par defaut, un type n'est PAS un conteneur.
             *        IsContainer = false et descriptor invalide.
             */
            template<typename C>
            struct NkContainerTrait {
                static constexpr nk_bool IsContainer = false;
                static const NkContainerDescriptor* Descriptor() { return nullptr; }
            };

            /**
             * @brief Specialisation pour NkVector<T> : conteneur sequentiel.
             */
            template<typename T>
            struct NkContainerTrait<NkVector<T>> {
                static constexpr nk_bool IsContainer = true;

                static const NkContainerDescriptor* Descriptor() {
                    static NkContainerDescriptor desc = MakeDescriptor();
                    return &desc;
                }

                private:
                    static NkContainerDescriptor MakeDescriptor() {
                        NkContainerDescriptor d;
                        d.elementType = &NkTypeOf<T>();
                        d.getCount    = &detail::NkVecGetCount<T>;
                        d.getElement  = &detail::NkVecGetElement<T>;
                        d.getElementC = &detail::NkVecGetElementC<T>;
                        d.pushBack    = &detail::NkVecPushBack<T>;
                        d.clear       = &detail::NkVecClear<T>;
                        d.resize      = &detail::NkVecResize<T>;
                        return d;
                    }
            };

            /**
             * @brief Helper de confort : descripteur pour un type conteneur C.
             * @return Pointeur vers le descripteur statique, nullptr si non-conteneur.
             */
            template<typename C>
            const NkContainerDescriptor* NkContainerOf() {
                return NkContainerTrait<C>::Descriptor();
            }

        } // namespace reflection

    } // namespace nkentseu

#endif // NK_REFLECTION_NKCONTAINERTRAIT_H

// ============================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// ============================================================
