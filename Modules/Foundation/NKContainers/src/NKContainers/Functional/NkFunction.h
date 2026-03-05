// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\Functional\NkFunction.h
// DESCRIPTION: Function wrapper - std::function equivalent
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_FUNCTIONAL_NKFUNCTION_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_FUNCTIONAL_NKFUNCTION_H_INCLUDED

#include "NKContainers/NkCompat.h"
#include "NKCore/NkTypes.h"
#include "NKCore/NkExport.h"
#include "NKCore/NkTraits.h"
#include "NKMemory/NkAllocator.h"
#include "NKCore/Assert/NkAssert.h"

namespace nkentseu {
    namespace core {
        
        #if defined(NK_CPP11)
        
        /**
         * @brief Function wrapper - std::function equivalent
         * 
         * Stocke n'importe quel callable (function, lambda, functor).
         * Type-erasure avec small buffer optimization.
         * 
         * @example
         * NkFunction<int(int, int)> add = [](int a, int b) {
         *     return a + b;
         * };
         * 
         * int result = add(2, 3);  // 5
         */
        template<typename Signature>
        class NkFunction;
        
        template<typename Ret, typename... Args>
        class NkFunction<Ret(Args...)> {
        private:
            static constexpr usize SMALL_BUFFER_SIZE = 32;
            
            struct Concept {
                virtual ~Concept() {}
                virtual Ret Invoke(Args... args) = 0;
                virtual Concept* Clone() const = 0;
            };
            
            template<typename Functor>
            struct Model : Concept {
                Functor mFunctor;
                
                Model(const Functor& f) : mFunctor(f) {}
                Model(Functor&& f) : mFunctor(traits::NkMove(f)) {}
                
                Ret Invoke(Args... args) override {
                    return mFunctor(traits::NkForward<Args>(args)...);
                }
                
                Concept* Clone() const override {
                    return new Model(mFunctor);
                }
            };
            
            Concept* mImpl;
            unsigned char mBuffer[SMALL_BUFFER_SIZE];
            bool mUsingBuffer;
            
            void Destroy() {
                if (mImpl) {
                    if (mUsingBuffer) {
                        mImpl->~Concept();
                    } else {
                        delete mImpl;
                    }
                    mImpl = nullptr;
                }
            }
            
        public:
            // Constructors
            NkFunction() : mImpl(nullptr), mUsingBuffer(false) {}
            
            template<typename Functor>
            NkFunction(Functor f) : mImpl(nullptr), mUsingBuffer(false) {
                using ModelType = Model<Functor>;
                
                if (sizeof(ModelType) <= SMALL_BUFFER_SIZE) {
                    mImpl = new (mBuffer) ModelType(traits::NkMove(f));
                    mUsingBuffer = true;
                } else {
                    mImpl = new ModelType(traits::NkMove(f));
                    mUsingBuffer = false;
                }
            }
            
            NkFunction(const NkFunction& other) : mImpl(nullptr), mUsingBuffer(false) {
                if (other.mImpl) {
                    if (other.mUsingBuffer) {
                        mImpl = other.mImpl->Clone();
                        // Copy to buffer
                        void* ptr = mBuffer;
                        mImpl = static_cast<Concept*>(ptr);
                        mUsingBuffer = true;
                    } else {
                        mImpl = other.mImpl->Clone();
                        mUsingBuffer = false;
                    }
                }
            }
            
            NkFunction(NkFunction&& other) NK_NOEXCEPT
                : mImpl(other.mImpl), mUsingBuffer(other.mUsingBuffer) {
                if (mUsingBuffer) {
                    // Copy buffer
                    for (usize i = 0; i < SMALL_BUFFER_SIZE; ++i) {
                        mBuffer[i] = other.mBuffer[i];
                    }
                    mImpl = reinterpret_cast<Concept*>(mBuffer);
                }
                other.mImpl = nullptr;
            }
            
            ~NkFunction() {
                Destroy();
            }
            
            // Assignment
            NkFunction& operator=(const NkFunction& other) {
                if (this != &other) {
                    Destroy();
                    if (other.mImpl) {
                        mImpl = other.mImpl->Clone();
                        mUsingBuffer = other.mUsingBuffer;
                    }
                }
                return *this;
            }
            
            NkFunction& operator=(NkFunction&& other) NK_NOEXCEPT {
                if (this != &other) {
                    Destroy();
                    mImpl = other.mImpl;
                    mUsingBuffer = other.mUsingBuffer;
                    if (mUsingBuffer) {
                        for (usize i = 0; i < SMALL_BUFFER_SIZE; ++i) {
                            mBuffer[i] = other.mBuffer[i];
                        }
                        mImpl = reinterpret_cast<Concept*>(mBuffer);
                    }
                    other.mImpl = nullptr;
                }
                return *this;
            }
            
            // Invocation
            Ret operator()(Args... args) const {
                NK_ASSERT(mImpl);
                return mImpl->Invoke(traits::NkForward<Args>(args)...);
            }
            
            // State
            explicit operator bool() const NK_NOEXCEPT {
                return mImpl != nullptr;
            }
            
            bool IsValid() const NK_NOEXCEPT {
                return mImpl != nullptr;
            }
            
            void Reset() {
                Destroy();
            }
        };
        
        #endif // NK_CPP11
        
    } // namespace core
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_FUNCTIONAL_NKFUNCTION_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// 
// USAGE:
// NkFunction<int(int, int)> add = [](int a, int b) {
//     return a + b;
// };
// 
// int result = add(2, 3);  // 5
// 
// // Store member function
// struct Foo {
//     int Bar(int x) { return x * 2; }
// };
// 
// Foo foo;
// NkFunction<int(int)> f = [&foo](int x) {
//     return foo.Bar(x);
// };
// 
// // Callbacks
// using Callback = NkFunction<void()>;
// NkVector<Callback> callbacks;
// callbacks.PushBack([]() { printf("Hello\n"); });
// 
// for (auto& cb : callbacks) {
//     cb();
// }
// ============================================================

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================