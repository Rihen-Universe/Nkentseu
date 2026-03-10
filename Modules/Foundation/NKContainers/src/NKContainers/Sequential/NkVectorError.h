// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\Containers\Sequential\NkVectorError.h
// DESCRIPTION: Vector error handling and exceptions
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_SEQUENTIAL_NKVECTORERROR_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_SEQUENTIAL_NKVECTORERROR_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKContainers/NkContainersExport.h"
#include "NKContainers/NkContainersExport.h"

namespace nkentseu {
    
        
        // ========================================
        // ERROR CODES
        // ========================================
        
        /**
         * @brief Vector error codes
         */
        enum class NkVectorError : uint32 {
            NK_SUCCESS = 0,
            NK_OUT_OF_RANGE,
            NK_OUT_OF_MEMORY,
            NK_INVALID_ARGUMENT,
            NK_LENGTH_ERROR,
            NK_OVERFLOW,
            NK_UNDERFLOW,
            NK_BAD_ALLOC
        };
        
        // ========================================
        // ERROR EXCEPTION (SI EXCEPTIONS ACTIVÉES)
        // ========================================
        
        #if !defined(NK_NO_EXCEPTIONS)
        
        /**
         * @brief Base exception pour NkVector
         */
        class NKENTSEU_CORE_API NkVectorException {
        private:
            NkVectorError mError;
            const char* mMessage;
            usize mIndex;
            
        public:
            explicit NkVectorException(NkVectorError error, const char* message = nullptr, usize index = 0)
                : mError(error)
                , mMessage(message)
                , mIndex(index) {
            }
            
            NkVectorError GetError() const NK_NOEXCEPT { return mError; }
            const char* GetMessage() const NK_NOEXCEPT { return mMessage; }
            usize GetIndex() const NK_NOEXCEPT { return mIndex; }
            
            const char* What() const NK_NOEXCEPT {
                if (mMessage) return mMessage;
                
                switch (mError) {
                    case NkVectorError::NK_OUT_OF_RANGE:
                        return "NkVector: index out of range";
                    case NkVectorError::NK_OUT_OF_MEMORY:
                        return "NkVector: out of memory";
                    case NkVectorError::NK_INVALID_ARGUMENT:
                        return "NkVector: invalid argument";
                    case NkVectorError::NK_LENGTH_ERROR:
                        return "NkVector: length error";
                    case NkVectorError::NK_OVERFLOW:
                        return "NkVector: overflow";
                    case NkVectorError::NK_UNDERFLOW:
                        return "NkVector: underflow";
                    case NkVectorError::NK_BAD_ALLOC:
                        return "NkVector: bad allocation";
                    default:
                        return "NkVector: unknown error";
                }
            }
        };
        
        /**
         * @brief Out of range exception
         */
        class NKENTSEU_CORE_API NkVectorOutOfRangeException : public NkVectorException {
        public:
            explicit NkVectorOutOfRangeException(usize index, usize size)
                : NkVectorException(NkVectorError::NK_OUT_OF_RANGE, "index out of range", index) {
                (void)size; // Unused for now
            }
        };
        
        /**
         * @brief Out of memory exception
         */
        class NKENTSEU_CORE_API NkVectorBadAllocException : public NkVectorException {
        public:
            explicit NkVectorBadAllocException(usize requestedSize)
                : NkVectorException(NkVectorError::NK_BAD_ALLOC, "memory allocation failed", requestedSize) {
            }
        };
        
        /**
         * @brief Length error exception
         */
        class NKENTSEU_CORE_API NkVectorLengthException : public NkVectorException {
        public:
            explicit NkVectorLengthException(const char* message = "length error")
                : NkVectorException(NkVectorError::NK_LENGTH_ERROR, message) {
            }
        };
        
        #endif // !NK_NO_EXCEPTIONS
        
        // ========================================
        // ERROR HANDLING MACROS
        // ========================================
        
        #if !defined(NK_NO_EXCEPTIONS)
        
        /**
         * @brief Throw out of range exception
         */
        #define NK_VECTOR_THROW_OUT_OF_RANGE(index, size) \
            throw ::nkentseu::NkVectorOutOfRangeException(index, size)
        
        /**
         * @brief Throw bad alloc exception
         */
        #define NK_VECTOR_THROW_BAD_ALLOC(size) \
            throw ::nkentseu::NkVectorBadAllocException(size)
        
        /**
         * @brief Throw length error exception
         */
        #define NK_VECTOR_THROW_LENGTH_ERROR(msg) \
            throw ::nkentseu::NkVectorLengthException(msg)
        
        #else
        
        /**
         * @brief Assert instead of throw (no exceptions)
         */
        #define NK_VECTOR_THROW_OUT_OF_RANGE(index, size) \
            NK_ASSERT_MSG(false, "Vector index out of range")
        
        #define NK_VECTOR_THROW_BAD_ALLOC(size) \
            NK_ASSERT_MSG(false, "Vector bad allocation")
        
        #define NK_VECTOR_THROW_LENGTH_ERROR(msg) \
            NK_ASSERT_MSG(false, "Vector length error")
        
        #endif // !NK_NO_EXCEPTIONS
        
        // ========================================
        // ERROR CODES TO STRING
        // ========================================
        
        /**
         * @brief Convert error code to string
         */
        inline const char* NkVectorErrorToString(NkVectorError error) NK_NOEXCEPT {
            switch (error) {
                case NkVectorError::NK_SUCCESS:
                    return "Success";
                case NkVectorError::NK_OUT_OF_RANGE:
                    return "Out of range";
                case NkVectorError::NK_OUT_OF_MEMORY:
                    return "Out of memory";
                case NkVectorError::NK_INVALID_ARGUMENT:
                    return "Invalid argument";
                case NkVectorError::NK_LENGTH_ERROR:
                    return "Length error";
                case NkVectorError::NK_OVERFLOW:
                    return "Overflow";
                case NkVectorError::NK_UNDERFLOW:
                    return "Underflow";
                case NkVectorError::NK_BAD_ALLOC:
                    return "Bad allocation";
                default:
                    return "Unknown error";
            }
        }
        
    
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_SEQUENTIAL_NKVECTORERROR_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-09
// ============================================================