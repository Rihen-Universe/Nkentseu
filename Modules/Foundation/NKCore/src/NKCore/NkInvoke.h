#pragma once

#ifndef NKENTSEU_CORE_NKINVOKE_H_INCLUDED
#define NKENTSEU_CORE_NKINVOKE_H_INCLUDED

#include "NkTraits.h"

namespace nkentseu {
    

        namespace detail {

            template <typename T>
            constexpr T& NkInvokeObject(T& object) noexcept {
                return object;
            }

            template <typename T>
            constexpr T& NkInvokeObject(T* object) noexcept {
                return *object;
            }

            template <typename T>
            constexpr const T& NkInvokeObject(const T* object) noexcept {
                return *object;
            }

        } // namespace detail

        template <typename F, typename... Args,
                typename traits::NkEnableIf_t<
                    !__is_member_function_pointer(traits::NkRemoveReference_t<F>) &&
                    !__is_member_object_pointer(traits::NkRemoveReference_t<F>),
                    int> = 0>
        constexpr decltype(auto) NkInvoke(F&& function, Args&&... args) noexcept(
            noexcept(traits::NkForward<F>(function)(traits::NkForward<Args>(args)...))) {
            return traits::NkForward<F>(function)(traits::NkForward<Args>(args)...);
        }

        template <typename MemFn, typename Obj, typename... Args,
                typename traits::NkEnableIf_t<
                    __is_member_function_pointer(traits::NkRemoveReference_t<MemFn>),
                    int> = 0>
        constexpr decltype(auto) NkInvoke(MemFn&& memberFunction, Obj&& object, Args&&... args) noexcept(
            noexcept((detail::NkInvokeObject(traits::NkForward<Obj>(object)).*memberFunction)(
                traits::NkForward<Args>(args)...))) {
            return (detail::NkInvokeObject(traits::NkForward<Obj>(object)).*memberFunction)(
                traits::NkForward<Args>(args)...);
        }

        template <typename MemObj, typename Obj,
                typename traits::NkEnableIf_t<
                    __is_member_object_pointer(traits::NkRemoveReference_t<MemObj>),
                    int> = 0>
        constexpr decltype(auto) NkInvoke(MemObj&& memberObject, Obj&& object) noexcept(
            noexcept(detail::NkInvokeObject(traits::NkForward<Obj>(object)).*memberObject)) {
            return detail::NkInvokeObject(traits::NkForward<Obj>(object)).*memberObject;
        }

        template <typename F, typename... Args>
        using NkInvokeResult_t =
            decltype(NkInvoke(traits::NkDeclVal<F>(), traits::NkDeclVal<Args>()...));

        template <typename F, typename... Args>
        class NkIsInvocable {
            private:
                template <typename Dummy = void>
                static auto Test(int)
                    -> decltype(NkInvoke(traits::NkDeclVal<F>(), traits::NkDeclVal<Args>()...),
                                traits::NkTrueType{});

                template <typename Dummy = void>
                static auto Test(...) -> traits::NkFalseType;

            public:
                static constexpr nk_bool value = decltype(Test<>(0))::value;
        };

        template <typename F, typename... Args>
        inline constexpr nk_bool NkIsInvocable_v = NkIsInvocable<F, Args...>::value;

    
} // namespace nkentseu

#endif // NKENTSEU_CORE_NKINVOKE_H_INCLUDED

