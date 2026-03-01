/**
* @File NkMemory.h
* @Description Système central de gestion mémoire avec tracking complet
* @Author TEUGUIA TADJUIDJE Rodolf Séderis
* @Date 2025-01-05
* @License Rihen
*
* @Fonctionnalités :
* - Allocation/désallocation sécurisée
* - Tracking des allocations (fichier/ligne/fonction)
* - Intégration native avec UniquePtr/SharedPtr
* - Détection de fuites mémoire
* - Gestion thread-safe
*/

#pragma once

#include <string>
#include <functional>
#include <unordered_map>
#include <type_traits>
#include <mutex>
#include <stdexcept>
#include <sstream>
#include <iostream>

#include "Nkentseu/Platform/Export.h"
#include "Nkentseu/Platform/Platform.h"
#include "Nkentseu/Platform/Types.h"
#include "Nkentseu/Logger/Logger.h"
#include "NkUniquePtr.h"
#include "NkSharedPtr.h"

namespace nkentseu {

    /**
    * - NkMemorySystem : Gestionnaire central de mémoire
    *
    * @Description :
    * Singleton responsable de la gestion de toutes les allocations mémoire.
    * Fournit un système complet de tracking avec informations de contexte.
    * Intégré aux smart pointers personnalisés pour une gestion automatique.
    *
    * @Avertissements :
    * - Usage obligatoire via les méthodes New/Delete
    * - Ne pas mélanger avec les opérateurs new/delete standards
    */
    class NKENTSEU_API NkMemorySystem {
    public:
        /**
        * - Instance : Accès au singleton
        *
        * @Description :
        * Récupère l'instance unique en capturant le contexte d'appel
        *
        * @param (const char*) file : Fichier d'appel (__FILE__)
        * @param (int) line : Ligne d'appel (__LINE__)
        * @param (const char*) func : Fonction appelante (__func__)
        */
        static NkMemorySystem& Instance(const char* file, int line, const char* func);

        /**
        * - Initialize : Initialise le système mémoire
        * @return (bool) : true si initialisation réussie
        */
        bool Initialize();

        /**
        * - Shutdown : Arrêt sécurisé du système
        * @return (bool) : true si arrêt réussi, false si fuites détectées
        */
        bool Shutdown();

        /**
        * - New : Allocation d'objet
        *
        * @Template :
        * - (typename T) : Type à instancier
        * - (typename... Args) : Types des arguments du constructeur
        *
        * @param (const std::string&) name : Nom logique de l'allocation
        * @param (Args&&... args) : Arguments pour le constructeur de T
        * @return (T*) : Pointeur vers l'objet alloué
        *
        * @Exception :
        * Peut propager les exceptions du constructeur de T
        */
        template<typename T, typename... Args>
        T* New(const std::string& name, Args&&... args) {
            T* ptr = new T(std::forward<Args>(args)...);

            {
                RegisterAllocation(ptr, {
                    name,
                    mCurrentFile,
                    mCurrentFunc,
                    mCurrentLine,
                    false,
                    1,
                    false,
                    [](void* p) { delete static_cast<T*>(p); }, // Capture du bon type
                });
            }

            mIsSmartPtr = false;
            return ptr;
        }

        /**
        * - NewArray : Allocation de tableau
        *
        * @Template :
        * - (typename T) : Type des éléments du tableau
        * - (typename... Args) : Types des arguments du constructeur
        *
        * @param (const std::string&) name : Nom logique de l'allocation
        * @param (usize) count : Nombre d'éléments
        * @param (Args&&... args) : Arguments pour la construction des éléments
        * @return (T*) : Pointeur vers le tableau alloué
        *
        * @Exception :
        * - std::bad_alloc si échec d'allocation
        * - Peut propager les exceptions des constructeurs d'éléments
        */
        template<typename T, typename... Args>
        T* NewArray(const std::string& name, usize count, Args&&... args) {
            T* ptr = static_cast<T*>(operator new[](count * sizeof(T)));
            for (usize i = 0; i < count; ++i) {
                try {
                    new (ptr + i) T(std::forward<Args>(args)...);
                } catch (...) {
                    delete[] ptr;
                    ptr = nullptr;
                    throw;
                }
            }

            {
                RegisterAllocation(ptr, {
                    name,
                    mCurrentFile,
                    mCurrentFunc,
                    mCurrentLine,
                    true,
                    count,
                    false,
                    [](void* p) { delete[] static_cast<T*>(p); }, // Delete[] typé
                });
            }

            mIsSmartPtr = false;
            return ptr;
        }

        /**
        * - Delete : Libération sécurisée de mémoire
        *
        * @Template :
        * - (typename T) : Type de l'objet à détruire
        *
        * @param (T*) ptr : Pointeur à libérer
        * @return (bool) : true si libération réussie, false si pointeur inconnu
        */
        template<typename T>
        bool Delete(T* ptr) {
            if (!ptr) return false;

            NkAllocationInfo info;
            {
                std::lock_guard<std::mutex> lock(mMutex);
                auto it = mAllocations.find(ptr);
                if(it == mAllocations.end()) return false;

                if (it->second.isSmartPtr){
                    return false;
                }
                info = NkMemorySystem::Move(it->second);

                mAllocations.erase(it);
            }
            if (info.isArray) delete[] static_cast<T*>(ptr);
            else delete static_cast<T*>(ptr);
            ptr = nullptr;
            return true;
        }

        /**
        * - MakeUnique : Crée un UniquePtr managé
        *
        * @Template :
        * - (typename T) : Type de l'objet
        * - (typename... Args) : Arguments du constructeur
        *
        * @param (const std::string&) name : Nom logique de l'allocation
        * @param (Args&&... args) : Arguments pour le constructeur
        * @return (UniquePtr<T>) : UniquePtr configuré avec le deleter approprié
        */
        template<typename T, typename... Args>
        UniquePtr<T> MakeUnique(const std::string& name, Args&&... args) {
            mIsSmartPtr = true;
            T* ptr = New<T>(name, std::forward<Args>(args)...);
            return UniquePtr<T>(ptr, MakeDeleter<T>(false), 1);
        }

        /**
        * - MakeUniqueArray : Crée un UniquePtr pour tableau
        *
        * @Template :
        * - (typename T) : Type des éléments
        * - (typename... Args) : Arguments du constructeur
        *
        * @param (const std::string&) name : Nom logique
        * @param (usize) count : Taille du tableau
        * @return (UniquePtr<T>) : UniquePtr configuré pour tableau
        */
        template<typename T, typename... Args>
        UniquePtr<T> MakeUniqueArray(const std::string& name, usize count, Args&&... args) {
            mIsSmartPtr = true;
            T* ptr = NewArray<T>(name, count, std::forward<Args>(args)...);
            return  UniquePtr<T>(ptr, MakeDeleter<T>(true), count);
        }

        /**
        * - MakeShared : Crée un SharedPtr managé
        *
        * @Template :
        * - (typename T) : Type de l'objet
        * - (typename... Args) : Arguments du constructeur
        *
        * @return (SharedPtr<T>) : SharedPtr intégré au système de tracking
        */
        template<typename T, typename... Args>
        SharedPtr<T> MakeShared(const std::string& name, Args&&... args) {
            mIsSmartPtr = true;

            T* ptr = new T(std::forward<Args>(args)...);

            NkAllocationInfo& alloc = RegisterAllocation(ptr, {
                name,
                mCurrentFile,
                mCurrentFunc,
                mCurrentLine,
                false,
                1,
                true,
                [](void* p) { delete static_cast<T*>(p); }, // Capture du bon type
            });

            return  SharedPtr<T>(ptr, MakeDeleter<T>(false), 1, &alloc.isValid);
        }

        /**
        * - MakeSharedArray : Crée un SharedPtr pour tableau
        *
        * @Template :
        * - (typename T) : Type des éléments
        * - (typename... Args) : Arguments du constructeur
        *
        * @return (SharedPtr<T>) : SharedPtr configuré pour tableau
        */
        template<typename T, typename... Args>
        SharedPtr<T> MakeSharedArray(const std::string& name, usize count, Args&&... args) {
            mIsSmartPtr = true;
            //T* ptr = NewArray<T>(name, count, std::forward<Args>(args)...);
            T* ptr = static_cast<T*>(operator new[](count * sizeof(T)));
            for (usize i = 0; i < count; ++i) {
                try {
                    new (ptr + i) T(std::forward<Args>(args)...);
                } catch (...) {
                    delete[] ptr;
                    ptr = nullptr;
                    throw;
                }
            }

            NkAllocationInfo& alloc = RegisterAllocation(ptr, {
                name,
                mCurrentFile,
                mCurrentFunc,
                mCurrentLine,
                true,
                count,
                true,
                [](void* p) { delete[] static_cast<T*>(p); }, // Delete[] typé
            });

            return  SharedPtr<T>(ptr, MakeDeleter<T>(true), count, &alloc.isValid);
        }

            // RawMemoryCopy - Copie mémoire brute (équivalent memcpy)
            static void* Copy(void* dest, const void* src, usize count) {
                uint8* d = static_cast<uint8*>(dest);
                const uint8* s = static_cast<const uint8*>(src);

                for (usize i = 0; i < count; ++i)
                    d[i] = s[i];

                return dest;
            }

            // SafeMemoryMove - Copie mémoire sécurisée (équivalent memmove)
            static void* Move(void* dest, const void* src, usize count) {
                uint8* d = static_cast<uint8*>(dest);
                const uint8* s = static_cast<const uint8*>(src);

                if (d == s) return dest;

                if (d < s) {
                    for (usize i = 0; i < count; ++i)
                        d[i] = s[i];
                } else {
                    for (usize i = count; i > 0; --i)
                        d[i-1] = s[i-1];
                }

                return dest;
            }

            // Helper type trait pour enlever les références
            template<typename T>
            struct RemoveReference {
                using type = T;
            };

            template<typename T>
            struct RemoveReference<T&> {
                using type = T;
            };

            template<typename T>
            struct RemoveReference<T&&> {
                using type = T;
            };

            // Notre implémentation de move
            // template<typename T>
            // constexpr typename RemoveReference<T>::type&& Move(T&& arg) noexcept {
            //     return static_cast<typename RemoveReference<T>::type&&>(arg);
            // }

            template<typename T>
            static constexpr auto&& Move(T&& t) noexcept {
                using ReturnType = RemoveReference<T>::type&&;
                return static_cast<ReturnType>(t);
            }

            // MemoryCompare - Comparaison mémoire (équivalent memcmp)
            static int32 Compare(const void* lhs, const void* rhs, usize count) {
                const uint8* a = static_cast<const uint8*>(lhs);
                const uint8* b = static_cast<const uint8*>(rhs);

                for (usize i = 0; i < count; ++i) {
                    if (a[i] != b[i])
                        return static_cast<int>(a[i]) - static_cast<int>(b[i]);
                }
                return 0;
            }

            // MemorySet - Remplissage mémoire (équivalent memset)
            static void* Set(void* dest, int32 value, usize count) {
                uint8* d = static_cast<uint8*>(dest);
                const uint8 v = static_cast<uint8>(value);

                for (usize i = 0; i < count; ++i)
                    d[i] = v;

                return dest;
            }
    private:
        NkMemorySystem() = default;

        template<typename T>
        auto MakeDeleter(bool isArray) {
            return [isArray, this](T* p) {
                if (p == nullptr) return;

                NkAllocationInfo info;
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    auto it = mAllocations.find(p);
                    if(it == mAllocations.end()) return;

                    info = NkMemorySystem::Move(it->second);
                    mAllocations.erase(it);
                }

                if (info.isArray) delete[] static_cast<T*>(p);
                else delete static_cast<T*>(p);
                p = nullptr;

                // Log supplémentaire optionnel
                #if defined(NKENTSEU_DEBUG)
                    if (info.isArray != isArray) {
                        std::cerr << "Array mismatch in deleter!\n";
                    }
                #endif
            };
        }

        /**
        * @Struct NkAllocationInfo : Métadonnées d'allocation
        *
        * @Description :
        * Contient toutes les informations de tracking pour une allocation
        *
        * @Membres :
        * - (std::string) name : Identifiant logique
        * - (std::string) filename : Fichier d'origine
        * - (int) line : Ligne d'allocation
        * - (bool) isArray : Si allocation de tableau
        * - (std::function<void()>) deleter : Méthode de libération
        */
        struct NkAllocationInfo {
            std::string name;
            std::string filename;
            std::string function;
            int line = 0;
            bool isArray = false;
            usize elementCount = 0;
            bool isSmartPtr = false;
            bool isValid = false;
            usize id = 0; // ID unique pour le suivi

            static usize counter; // Compteur d'allocations
            static std::mutex counterMutex; // Mutex pour le compteur

            std::function<void(void*)> deleter; // Stocker le deleter typé

            /**
            * - ToString : Formatage des informations
            * @return (std::string) : Description complète de l'allocation
            */
            std::string ToString(const void* ptr) const;

            NkAllocationInfo() {}

            // Constructeur complet avec initialisation thread-safe de l'ID
            NkAllocationInfo(std::string allocName,
                std::string sourceFile,
                std::string funcName,
                int lineNumber,
                bool arrayFlag,
                usize count,
                bool smartPtrFlag,
                std::function<void(void*)> del, bool isValid = true)
            : name(NkMemorySystem::Move(allocName)),
            filename(NkMemorySystem::Move(sourceFile)),
            function(NkMemorySystem::Move(funcName)),
            line(lineNumber),
            isArray(arrayFlag),
            elementCount(count),
            isSmartPtr(smartPtrFlag), isValid(isValid),
            deleter(NkMemorySystem::Move(del))
            {
            // Gestion thread-safe du compteur
            std::lock_guard<std::mutex> lock(counterMutex);
            id = counter++;
            }
        };

        // Membres privés
        const char* mCurrentFile = nullptr;
        int mCurrentLine = 0;
        const char* mCurrentFunc = nullptr;
        bool mIsSmartPtr = false;
        std::unordered_map<void*, NkAllocationInfo> mAllocations;
        std::mutex mMutex;

        /**
        * - RegisterAllocation : Enregistre une nouvelle allocation
        * @param (void*) ptr : Pointeur alloué
        * @param (const NkAllocationInfo&) info : Métadonnées associées
        */
        NkAllocationInfo& RegisterAllocation(void* ptr, const NkAllocationInfo& info);

        /**
        * - ReportLeaks : Signale les fuites mémoire
        * @Description :
        * Appelé à la fermeture du système, loggue toutes les allocations non libérées
        */
        void ReportLeaks();
    };

    // Alias for backward compatibility
    using MemorySystem = NkMemorySystem;
}

/**
* @Macro NK_MEMORY_INSTANCE : Accès contextuel au NkMemorySystem
*
* @Description :
* Capture automatiquement __FILE__, __LINE__ et __func__
* pour le tracking d'allocation
*/
#define NK_MEMORY_INSTANCE ::nkentseu::NkMemorySystem::Instance(__FILE__, __LINE__, __func__)

// Backward compatibility alias
#define Memorys NK_MEMORY_INSTANCE

// Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
// Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
// © Rihen 2025 - Tous droits réservés.
