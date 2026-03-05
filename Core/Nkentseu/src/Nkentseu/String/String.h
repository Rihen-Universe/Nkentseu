#pragma once

#include "Nkentseu/Platform/Types.h"
#include "Nkentseu/Platform/Inline.h"
#include "Nkentseu/Platform/Assertion.h"
#include "Nkentseu/Platform/Export.h"

#include "StringConverter.h"
#include "StringUtils.h"

#include <new>
#include <memory>
#include <cstdlib>

namespace nkentseu {
    
    struct NKENTSEU_TEMPLATE_API StringAllocator {
    private:
        NKENTSEU_FORCE_INLINE static usize AlignUp(usize size, usize alignment) NKENTSEU_NOEXCEPT {
            return (size + alignment - 1) & ~(alignment - 1);
        }
    
    public:
        // Allocation brute sans construction
        NKENTSEU_FORCE_INLINE static void* Allocate(usize size, usize alignment) {
            if (NKENTSEU_UNLIKELY(size == 0)) return nullptr;
    
            alignment = alignment < memory::MIN_ALIGNMENT ? memory::DEFAULT_ALIGNMENT : 
                        alignment > memory::MAX_ALIGNMENT ? memory::MAX_ALIGNMENT : 
                        AlignUp(alignment, memory::MIN_ALIGNMENT);
    
            void* ptr = nullptr;
    
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                ptr = _aligned_malloc(size, alignment);
            #elif defined(NKENTSEU_PLATFORM_APPLE)
                if(alignment <= memory::MIN_ALIGNMENT) {
                    ptr = malloc(size);
                } else {
                    if(posix_memalign(&ptr, alignment, size) != 0)
                        ptr = nullptr;
                }
            #else
                ptr = std::aligned_alloc(alignment, size);
            #endif
    
            if (NKENTSEU_UNLIKELY(!ptr)) {
                if constexpr(memory::ALLOCATOR_THROW_ON_FAIL) {
                    throw std::bad_alloc();
                }
                return nullptr;
            }
            return ptr;
        }
    
        // Désallocation brute sans destruction
        NKENTSEU_FORCE_INLINE static void Deallocate(void* ptr) NKENTSEU_NOEXCEPT {
            (void)ptr; // Supprime l'avertissement unused parameter
            if (NKENTSEU_LIKELY(ptr != nullptr)) {
                #if defined(NKENTSEU_PLATFORM_WINDOWS)
                    _aligned_free(ptr);
                #else
                    free(ptr);
                #endif
            }
        }
    };

    template<typename CharT, typename Allocator = StringAllocator>
    class NKENTSEU_TEMPLATE_API BasicString {
        public:
            // Configuration SSO (16 bytes)
            static constexpr usize kSSOBufferBytes = 16;
            static constexpr usize kSSOCapacity = (kSSOBufferBytes / sizeof(CharT)) - 1;
        private:

            union Storage {
                CharT ssoBuffer[kSSOCapacity + 1];
                struct HeapData {
                    CharT* ptr;
                    usize capacity;
                    bool owner;
        
                    // Constructeur explicite pour le membre HeapData
                    HeapData() : ptr(nullptr), capacity(0), owner(true) {}
                } heap;
        
                // Constructeur explicite pour l'union
                Storage() { 
                    // Initialise le buffer SSO par défaut
                    ssoBuffer[0] = SLTT(CharT); 
                }
        
                ~Storage() noexcept {} // Destructeur trivial
            };

            Storage m_Storage;
            usize m_Length;
            bool m_IsSso;

        public:
            //------------------------------------------
            // 1. Constructeurs de base
            //------------------------------------------

            BasicString() noexcept : m_Length(0), m_IsSso(true) {
                m_Storage.ssoBuffer[0] = SLTT(CharT);
            }

            template<usize N>
            explicit BasicString(const CharT (&literal)[N]) : BasicString(literal, N - 1) {} // Exclure le \0 automatique

            explicit BasicString(const CharT* str) {
                m_Length = CalculateLength(str);
                InitFromCString(str, m_Length);
            }

            // Conversion depuis différent encodage (C-string)
            template<typename CharU>
            explicit BasicString(const CharU* str) {
                const usize srcLen = StringUtils::ComputeStringLength(str);
                InitializeFromDifferentEncoding(str, srcLen);
            }

            // Conversion depuis différent encodage (buffer + longueur)
            template<typename CharU>
            BasicString(const CharU* data, usize length, bool takeOwnership = false) {
                if (takeOwnership) {
                    static_assert(IsSame_v<CharU, CharT>, 
                        "Ownership requires same encoding");
                    TakeOwnership(data, length);
                } else {
                    InitializeFromDifferentEncoding(data, length);
                }
            }

            // Conversion depuis littéral
            template<typename CharU, usize N>
            explicit BasicString(const CharU (&literal)[N]) 
                : BasicString(literal, N - 1) {} // Exclure \0

            BasicString(const BasicString& other) {
                if (other.m_IsSso) {
                    memory::MemoryCopy(m_Storage.ssoBuffer, other.m_Storage.ssoBuffer, kSSOBufferBytes);
                } else {
                    m_Storage.heap.ptr = static_cast<CharT*>(Allocator::Allocate((other.m_Length + 1) * sizeof(CharT), alignof(CharT)));
                    memory::MemoryCopy(m_Storage.heap.ptr, other.m_Storage.heap.ptr, (other.m_Length + 1) * sizeof(CharT));
                    m_Storage.heap.owner = true;
                }
                m_Length = other.m_Length;
                m_IsSso = other.m_IsSso;
                // CopyFrom(other);
            }

            BasicString(BasicString&& other) noexcept {
                if (other.m_IsSso) {
                    memory::MemoryCopy(m_Storage.ssoBuffer, other.m_Storage.ssoBuffer, kSSOBufferBytes);
                    m_Length = other.m_Length;
                    m_IsSso = true;
                } else {
                    m_Storage.heap = other.m_Storage.heap;
                    m_Length = other.m_Length;
                    m_IsSso = false;
                    other.m_Storage.heap.ptr = nullptr;
                    other.m_Storage.heap.owner = false;
                    other.m_Length = 0;
                }
            }

            BasicString(const CharT* data, usize length, bool takeOwnership = false) {
                if (takeOwnership) {
                    // Prise de possession explicite (doit être alloué avec StringAllocator)
                    m_IsSso = false;
                    m_Storage.heap.ptr = const_cast<CharT*>(data);
                    m_Storage.heap.capacity = length; 
                    m_Storage.heap.owner = true;
                    m_Length = length;
                    
                    // Garantir le terminateur nul
                    if (NKENTSEU_UNLIKELY(data[length] != SLTT(CharT))) {
                        m_Storage.heap.ptr[length] = SLTT(CharT);
                    }
                } else {
                    // Copie sécurisée avec buffer dédié
                    if (length <= kSSOCapacity) {
                        m_IsSso = true;
                        memory::MemoryCopy(m_Storage.ssoBuffer, data, length * sizeof(CharT));
                        m_Storage.ssoBuffer[length] = SLTT(CharT);
                    } else {
                        m_IsSso = false;
                        const usize allocSize = (length + 1) * sizeof(CharT);
                        m_Storage.heap.ptr = static_cast<CharT*>(Allocator::Allocate(allocSize, alignof(CharT)));
                        memory::MemoryCopy(m_Storage.heap.ptr, data, length * sizeof(CharT));
                        m_Storage.heap.ptr[length] = SLTT(CharT);
                        m_Storage.heap.owner = true;
                        m_Storage.heap.capacity = length;
                    }
                    m_Length = length;
                }
                
                // Validation finale
                NKENTSEU_ASSERT(Data()[m_Length] == SLTT(CharT), "Chaîne non terminée correctement");
            }

            //------------------------------------------
            // 2. Constructeurs de conversion
            //------------------------------------------

            template<typename OtherCharT, typename OtherAllocator>
            BasicString(const BasicString<OtherCharT, OtherAllocator>& other) {
                ConvertFromOtherString(other);
            }

            // template<typename OtherCharT>
            // BasicString(const OtherCharT* str) {
            //     ConvertFromCString(str);
            // }

            //------------------------------------------
            // 3. Destructeur
            //------------------------------------------
            ~BasicString() {
                Clear();
            }

            void Clear() noexcept {
                // Version optimisée avec memset pour le SSO
                if (!m_IsSso && m_Storage.heap.owner && m_Storage.heap.ptr) {
                    Allocator::Deallocate(m_Storage.heap.ptr);
                    m_Storage.heap.ptr = nullptr;
                    m_Storage.heap.capacity = 0;
                }
                else if(m_IsSso) {
                    // Reset complet du buffer SSO
                    memory::MemorySet(m_Storage.ssoBuffer, 0, kSSOBufferBytes);
                }
                
                m_Length = 0;
                m_IsSso = true;
                m_Storage.ssoBuffer[0] = SLTT(CharT);
            }

            //------------------------------------------
            // Méthodes d'accès
            //------------------------------------------
            usize Length() const noexcept { return m_Length; }
            usize Capacity() const noexcept { return m_IsSso ? kSSOCapacity : m_Storage.heap.capacity; }
            const CharT* Data() const noexcept { return m_IsSso ? m_Storage.ssoBuffer : m_Storage.heap.ptr; }
            CharT* Data() noexcept { return m_IsSso ? m_Storage.ssoBuffer : m_Storage.heap.ptr; }
            bool IsSso() const noexcept { return m_IsSso; }

            bool IsEmpty() const noexcept {
                return m_Length == 0;
            }

            // Opérateur d'affectation
            BasicString& operator=(const BasicString& other) {
                if (this != &other) {
                    this->~BasicString(); // Détruire l'ancienne donnée
                    new (this) BasicString(other); // Constructeur de copie
                }
                return *this;
            }

            BasicString& operator=(BasicString&& other) noexcept {
                if (this != &other) {
                    Clear();
                    
                    if (other.m_IsSso) {
                        memory::MemoryCopy(m_Storage.ssoBuffer, other.m_Storage.ssoBuffer, kSSOBufferBytes);
                        m_Length = other.m_Length;
                        m_IsSso = true;
                    } else {
                        m_Storage.heap = other.m_Storage.heap;
                        m_Length = other.m_Length;
                        m_IsSso = false;
                        other.m_Storage.heap.ptr = nullptr;
                        other.m_Storage.heap.owner = false;
                        other.m_Length = 0;
                    }
                }
                return *this;
            }

            // Opérateur d'affectation pour chaîne C native
            BasicString& operator=(const CharT* str) {
                usize length = CalculateLength(str);
                Clear();
                InitFromCString(str, length);
                return *this;
            }

            // Opérateur d'affectation pour chaîne de caractères différents
            template<typename OtherCharT>
            BasicString& operator=(const OtherCharT* str) {
                std::vector<uint32> codePoints;
                StringConverter<OtherCharT>::Decode(str, codePoints);
                InitFromCodePoints(codePoints.data(), codePoints.size());
                return *this;
            }

            // Opérateur d'affectation pour BasicString de type différent
            template<typename OtherCharT, typename OtherAllocator>
            BasicString& operator=(const BasicString<OtherCharT, OtherAllocator>& other) {
                if (static_cast<const void*>(this) != static_cast<const void*>(&other)) {
                    std::vector<uint32> codePoints;
                    StringConverter<OtherCharT>::Decode(other.Data(), codePoints);
                    InitFromCodePoints(codePoints.data(), codePoints.size());
                }
                return *this;
            }

            // Opérateur d'affectation pour littéraux de chaîne
            template<usize N>
            BasicString& operator=(const CharT (&literal)[N]) {
                return operator=(literal); // Délègue à l'opérateur const CharT*
            }

            static BasicString CreateView(const CharT* data, usize length) noexcept {
                BasicString s;
                s.m_IsSso = false;
                s.m_Storage.heap.ptr = const_cast<CharT*>(data);
                s.m_Storage.heap.owner = false;
                s.m_Length = length;
                return s;
            }

            void Reserve(usize newCapacity) {
                // Garantir la capacité minimale
                if (newCapacity < m_Length) {
                    newCapacity = m_Length;
                }
                
                if (newCapacity <= Capacity()) return;

                // Calcul de capacité optimisée
                newCapacity = CalculateOptimalCapacity(newCapacity);

                if (newCapacity <= kSSOCapacity && !m_IsSso) {
                    // Conversion dynamique -> SSO
                    CharT tempBuffer[kSSOCapacity + 1];
                    memory::MemoryCopy(tempBuffer, m_Storage.heap.ptr, (m_Length + 1) * sizeof(CharT));
                    Clear();
                    memory::MemoryCopy(m_Storage.ssoBuffer, tempBuffer, (m_Length + 1) * sizeof(CharT));
                    m_IsSso = true;
                } else if (newCapacity > kSSOCapacity) {
                    // Version optimisée avec réallocation géométrique
                    newCapacity = (newCapacity * 3) / 2; // Facteur 1.5x
                    
                    // Réallocation dynamique
                    const usize newSize = (newCapacity + 1) * sizeof(CharT);
                    CharT* newPtr = static_cast<CharT*>(Allocator::Allocate(newSize, alignof(CharT)));
                    
                    if (m_Length > 0) {
                        memory::MemoryCopy(newPtr, Data(), (m_Length + 1) * sizeof(CharT));
                    }
                    
                    if (!m_IsSso && m_Storage.heap.owner) {
                        Allocator::Deallocate(m_Storage.heap.ptr);
                    }
                    
                    m_Storage.heap.ptr = newPtr;
                    m_Storage.heap.capacity = newCapacity;
                    m_Storage.heap.owner = true;
                    m_IsSso = false;
                }
            }

            void ShrinkToFit() {
                if(m_IsSso || m_Length == m_Storage.heap.capacity) return;
        
                if(m_Length <= kSSOCapacity) {
                    // Conversion vers SSO
                    CharT tempBuffer[kSSOCapacity + 1];
                    memory::MemoryCopy(tempBuffer, m_Storage.heap.ptr, (m_Length + 1) * sizeof(CharT));
                    Allocator::Deallocate(m_Storage.heap.ptr);
                    memory::MemoryCopy(m_Storage.ssoBuffer, tempBuffer, (m_Length + 1) * sizeof(CharT));
                    m_IsSso = true;
                }
                else {
                    // Réallocation exacte
                    const usize requiredCapacity = m_Length;
                    CharT* newPtr = static_cast<CharT*>(
                        Allocator::Allocate((requiredCapacity + 1) * sizeof(CharT), alignof(CharT)));
                    
                    memory::MemoryCopy(newPtr, m_Storage.heap.ptr, (m_Length + 1) * sizeof(CharT));
                    Allocator::Deallocate(m_Storage.heap.ptr);
                    
                    m_Storage.heap.ptr = newPtr;
                    m_Storage.heap.capacity = requiredCapacity;
                }
            }
        
            const CharT* CStr() const noexcept {
                return Data(); // Garanti null-terminé par construction
            }
        
            BasicString Clone() const {
                if(m_IsSso) {
                    return *this; // Copie SSO légère
                }
                else {
                    // Copie profonde contrôlée
                    BasicString clone;
                    clone.m_IsSso = false;
                    clone.m_Length = m_Length;
                    clone.m_Storage.heap.capacity = m_Storage.heap.capacity;
                    clone.m_Storage.heap.owner = true;
                    clone.m_Storage.heap.ptr = static_cast<CharT*>(Allocator::Allocate((m_Length + 1) * sizeof(CharT), alignof(CharT)));
                    
                    memory::MemoryCopy(clone.m_Storage.heap.ptr, m_Storage.heap.ptr, (m_Length + 1) * sizeof(CharT));
                    
                    return clone;
                }
            }

            void Resize(usize newSize, CharT fillChar = CharT(0)) {
                if(newSize == m_Length) return;
                
                if(newSize < m_Length) {
                    // Troncature
                    m_Length = newSize;
                    GetBuffer()[newSize] = SLTT(CharT);
                    return;
                }
        
                // Extension avec remplissage
                Reserve(newSize);
                CharT* buffer = GetBuffer();
                
                for(usize i = m_Length; i < newSize; ++i) {
                    buffer[i] = fillChar;
                }
                
                m_Length = newSize;
                buffer[newSize] = SLTT(CharT);
            }

        private:

            //------------------------------------------
            // Méthodes internes
            //------------------------------------------

            usize CalculateLength(const CharT* str) const {
                const CharT* ptr = str;
                while(*ptr != SLTT(CharT)) ++ptr;
                return ptr - str;
            }

            void InitFromCString(const CharT* str, usize length) {
                if(length <= kSSOCapacity) {
                    m_IsSso = true;
                    for(usize i = 0; i <= length; ++i) {
                        m_Storage.ssoBuffer[i] = str[i];
                    }
                } else {
                    m_IsSso = false;
                    m_Length = length;
                    m_Storage.heap.capacity = length;
                    m_Storage.heap.ptr = static_cast<CharT*>(Allocator::Allocate((length + 1) * sizeof(CharT), alignof(CharT)));

                    for(usize i = 0; i <= length; ++i) {
                        m_Storage.heap.ptr[i] = str[i];
                    }
                }
                m_Length = length;
            }

            void CopyFrom(const BasicString& other) {
                m_Length = other.m_Length;
                m_IsSso = other.m_IsSso;

                if(m_IsSso) {
                    for(usize i = 0; i <= m_Length; ++i) {
                        m_Storage.ssoBuffer[i] = other.m_Storage.ssoBuffer[i];
                    }
                } else {
                    m_Storage.heap.capacity = other.m_Storage.heap.capacity;
                    m_Storage.heap.ptr = static_cast<CharT*>(Allocator::Allocate((m_Length + 1) * sizeof(CharT), alignof(CharT)));

                    for(usize i = 0; i <= m_Length; ++i) {
                        m_Storage.heap.ptr[i] = other.m_Storage.heap.ptr[i];
                    }
                }
            }

            template<typename OtherCharT>
            void ConvertFromCString(const OtherCharT* str) {
                std::vector<uint32> codePoints;
                StringConverter<OtherCharT>::Decode(str, codePoints);
                InitFromCodePoints(codePoints.data(), codePoints.size());
            }

            template<typename OtherCharT, typename OtherAllocator>
            void ConvertFromOtherString(const BasicString<OtherCharT, OtherAllocator>& other) {
                std::vector<uint32> codePoints;
                StringConverter<OtherCharT>::Decode(other.Data(), codePoints);
                InitFromCodePoints(codePoints.data(), codePoints.size());
            }

            void InitFromCodePoints(const uint32* codePoints, usize length) {
                Clear();
                
                try {
                    if (length <= kSSOCapacity) {
                        m_IsSso = true;
                        StringConverter<CharT>::EncodeFromCodePoints(
                            std::vector<uint32>(codePoints, codePoints + length), 
                            m_Storage.ssoBuffer
                        );
                        m_Storage.ssoBuffer[length] = SLTT(CharT);
                    } else {
                        m_IsSso = false;
                        m_Storage.heap.capacity = length;
                        m_Storage.heap.ptr = static_cast<CharT*>(Allocator::Allocate((length + 1) * sizeof(CharT), alignof(CharT)));
                        m_Storage.heap.owner = true;
                        
                        StringConverter<CharT>::EncodeFromCodePoints(
                            std::vector<uint32>(codePoints, codePoints + length), 
                            m_Storage.heap.ptr
                        );
                        m_Storage.heap.ptr[length] = SLTT(CharT);
                    }
                    m_Length = length;
                } catch (const std::bad_alloc&) {
                    m_IsSso = true;
                    m_Length = 0;
                    m_Storage.ssoBuffer[0] = SLTT(CharT);
                    if constexpr(memory::ALLOCATOR_THROW_ON_FAIL) {
                        throw;
                    }
                }
            }

            template<typename CharU>
            void InitializeFromDifferentEncoding(const CharU* src, usize srcLen) {
                if constexpr (std::is_same_v<CharU, CharT>) {
                    InitFromCString(src, srcLen);
                } else {
                    // Construction temporaire + conversion
                    BasicString<CharU> tmp(src, srcLen);
                    BasicString<CharT> converted = tmp.template ConvertEncoding<CharT>();

                    // Transfert des données
                    if (converted.IsSso()) {
                        m_IsSso = true;
                        memory::MemoryCopy(
                            m_Storage.ssoBuffer, 
                            converted.Data(), 
                            (converted.Length() + 1) * sizeof(CharT)
                        );
                        m_Length = converted.Length();
                    } else {
                        m_IsSso = false;
                        m_Storage.heap = converted.m_Storage.heap;
                        m_Length = converted.Length();
                        converted.m_Storage.heap.ptr = nullptr; // Empêche la désallocation
                    }
                }
            }

            //------------------------------------------
            // Méthode helper pour le calcul de capacité
            //------------------------------------------
            static usize CalculateOptimalCapacity(usize requested) noexcept {
                // Alignement sur la puissance de 2 supérieure
                if(requested == 0) return 0;
                
                usize power = 1;
                while(power < requested) {
                    power <<= 1;
                }
                return power;
            }

            CharT* GetBuffer() noexcept {
                return m_IsSso ? m_Storage.ssoBuffer : m_Storage.heap.ptr;
            }

        public:
            
            template<typename TargetChar>
            BasicString<TargetChar> ConvertEncoding() const {
                std::vector<uint32> codePoints;
                StringConverter<CharT>::Decode(Data(), codePoints);

                // Calcul capacité dynamique
                usize capacity = 0;
                if constexpr (IsSame_v<TargetChar, charb>) {
                    for (auto cp : codePoints) {
                        if (cp <= 0x7F) capacity += 1;
                        else if (cp <= 0x7FF) capacity += 2;
                        else if (cp <= 0xFFFF) capacity += 3;
                        else capacity += 4;
                    }
                }
                else if constexpr (IsSame_v<TargetChar, char16>) {
                    capacity = codePoints.size() * 2; // Pire cas
                }
                else {
                    capacity = codePoints.size();
                }

                // Allocation dynamique sûre
                TargetChar* buffer = static_cast<TargetChar*>(
                    Allocator::Allocate((capacity + 1) * sizeof(TargetChar), alignof(TargetChar))
                );

                // Encodage
                if constexpr (IsSame_v<TargetChar, char8>) {
                    StringConverter<TargetChar>::EncodeToUTF8(codePoints, buffer);
                }
                else if constexpr (IsSame_v<TargetChar, char16>) {
                    StringConverter<TargetChar>::EncodeToUTF16(codePoints, buffer);
                }
                else if constexpr (IsSame_v<TargetChar, char32>) {
                    StringConverter<TargetChar>::EncodeToUTF32(codePoints, buffer);
                }
                #if defined(NKENTSEU_PLATFORM_WINDOWS)
                else if constexpr (IsSame_v<TargetChar, wchar>) {
                    StringConverter<TargetChar>::EncodeToWChar(codePoints, buffer);
                }
                #endif

                buffer[capacity] = TargetChar(0);
                return BasicString<TargetChar>(buffer, capacity, true); // Prise de possession
            }
    };

    template<>
    template<>
    BasicString<charb> BasicString<char16>::ConvertEncoding<charb>() const {
        std::vector<uint32> codePoints;
        StringConverter<char16>::Decode(Data(), codePoints);
        
        charb buffer[constants::STRING_BUFFER_SIZE];
        StringConverter<charb>::EncodeFromCodePoints(codePoints, buffer);
        
        return BasicString<charb>(buffer);
    }

    // Aliases communs
    using String        = BasicString<charb>;
    using WString       = BasicString<wchar>;
    using StringU8      = BasicString<char8>;
    using StringU16     = BasicString<char16>;
    using StringU32     = BasicString<char32>;
} // namespace nkentseu