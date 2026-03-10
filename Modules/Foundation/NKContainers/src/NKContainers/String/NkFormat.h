#pragma once

// =============================================================================
// @File    NkFormat.h
// @Brief   Formatage positionnel {i:props} sans STL — NkFormat + NkToString
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis
// @Date    2026-03-06
// @License Apache-2.0
//
// =============================================================================
// SYNTAXE DES PLACEHOLDERS
// =============================================================================
//
//   {i}          → argument à la position i (base 0), format par défaut
//   {i:props}    → argument i avec propriétés de formatage
//
// Propriétés (props) — ordre libre, séparées par espace ou virgule :
//
//   Alignement & rembourrage :
//     w=N          largeur minimale
//     <            aligner à gauche
//     >            aligner à droite
//     ^            centrer
//     fill=C       caractère de rembourrage (défaut espace)
//     0            rembourrage par zéros (fill=0 >)
//
//   Nombres :
//     .N           précision (décimales pour float, taille max pour string)
//     +            toujours afficher le signe
//     #            préfixe 0x / 0b / 0o selon base
//     hex          base 16
//     HEX          base 16 majuscules
//     bin          base 2
//     oct          base 8
//     sci          notation scientifique (e)
//     SCI          notation scientifique (E)
//     gen          notation générale (g)
//     base=N       base explicite (2, 8, 10, 16)
//
//   Chaînes :
//     upper        convertir en majuscules
//     lower        convertir en minuscules
//
// EXEMPLES :
//   NkFormat("valeur = {0}", 42)                     → "valeur = 42"
//   NkFormat("{0:w=8 >}", 42)                        → "      42"
//   NkFormat("{0:w=8 < fill=.}", 42)                 → "42......"
//   NkFormat("{0:.3}", 3.14159)                      → "3.142"
//   NkFormat("{0:hex #}", 255)                       → "0xff"
//   NkFormat("{0:HEX # w=8 0}", 255)                 → "0X0000FF"
//   NkFormat("{0:bin # w=8}", 5u)                    → "0b00000101"
//   NkFormat("{0:sci .4}", 0.000123)                 → "1.2300e-04"
//   NkFormat("{0} + {1} = {2}", 1, 2, 3)             → "1 + 2 = 3"
//
// =============================================================================
// BUFFER D'ARGUMENTS — HYBRID SSO
// =============================================================================
//
//  NkFormatArgBuffer stocke les NkFormatArg :
//    ≤ NK_FORMAT_SSO_ARGS (16)  → tableau stack (zéro allocation)
//    >  NK_FORMAT_SSO_ARGS      → débordement sur heap via NkIAllocator
//
//  Cela permet un nombre d'arguments illimité sans jamais toucher STL,
//  tout en évitant toute allocation pour les cas courants (< 16 args).
//
// =============================================================================
// EXTENSION UTILISATEUR — NkToString<T>
// =============================================================================
//
//   namespace nkentseu { 
//       template<>
//       struct NkToString<MonType> {
//           static NkString Convert(const MonType& v, const NkFormatProps& props) {
//               NkString s = ...;           // convertir v en string
//               return NkApplyFormatProps(s, props);  // respecter width/align
//           }
//       };
//   }}
//
//   // Utilisation :
//   NkFormat("pos = {0:w=30 ^}", myMonType);
//
// =============================================================================

#ifndef NK_CORE_NKFORMAT_H_INCLUDED
#define NK_CORE_NKFORMAT_H_INCLUDED

#include <cstdio>
#include "NKContainers/NkCompat.h"
#include "NKContainers/String/NkString.h"
#include "NKMemory/NkAllocator.h"

namespace nkentseu {
    

        // =====================================================================
        // NkFormatProps — propriétés parsées d'un placeholder {i:props}
        // =====================================================================

        enum class NkFormatAlign : unsigned char {
            Unset  = 0,  ///< aucun (X11 #define None 0L — évite le conflit)
            Left   = 1,  ///< <
            Right  = 2,  ///< >
            Center = 3,  ///< ^
        };

        enum class NkFormatBase : unsigned char {
            Dec      = 10,
            Hex      = 16,
            HexUpper = 17,
            Oct      = 8,
            Bin      = 2,
        };

        enum class NkFormatFloat : unsigned char {
            Default    = 0,  ///< %f
            Scientific = 1,  ///< %e
            SciUpper   = 2,  ///< %E
            General    = 3,  ///< %g
        };

        struct NkFormatProps {
            int           width      = 0;
            int           precision  = -1;
            NkFormatAlign align      = NkFormatAlign::Unset;
            char          fill       = ' ';
            NkFormatBase  base       = NkFormatBase::Dec;
            NkFormatFloat floatStyle = NkFormatFloat::Default;
            bool          showSign   = false;
            bool          showPrefix = false;
            bool          upperStr   = false;
            bool          lowerStr   = false;

            bool HasWidth()     const { return width > 0; }
            bool HasPrecision() const { return precision >= 0; }
        };

        // =====================================================================
        // API publique — helpers partagés
        // =====================================================================

        /** Parse une chaîne de props → NkFormatProps */
        NkFormatProps NkParseFormatProps(const char* props, int propsLen);

        /** Applique alignement/rembourrage à un string déjà converti */
        NkString NkApplyFormatProps(const NkString& value, const NkFormatProps& props);

        // =====================================================================
        // NkToString<T>
        // Trait de conversion. Spécialisé pour tous les primitifs.
        // Surcharger pour vos propres types.
        // =====================================================================

        template<typename T>
        struct NkToString {
            // Pas de Convert() → erreur de compilation si type non spécialisé.
            // Message d'erreur intentionnellement clair :
            //   "NkToString<YourType> has no member 'Convert'"
        };

        // Helpers internes exposés (utiles dans les spécialisations custom)
        NkString NkIntToString  (long long          v, const NkFormatProps& props);
        NkString NkUIntToString (unsigned long long v, const NkFormatProps& props);
        NkString NkFloatToString(double             v, const NkFormatProps& props);

        // --- bool ---
        template<> struct NkToString<bool> {
            static NkString Convert(bool v, const NkFormatProps& props) {
                NkString s(v ? "true" : "false");
                if (props.upperStr) s.ToUpper();
                return NkApplyFormatProps(s, props);
            }
        };

        // --- char ---
        template<> struct NkToString<char> {
            static NkString Convert(char v, const NkFormatProps& props) {
                return NkApplyFormatProps(NkString(1u, v), props);
            }
        };

        // --- Entiers signés ---
        template<> struct NkToString<signed char> {
            static NkString Convert(signed char v, const NkFormatProps& p) { return NkIntToString(v, p); }
        };
        template<> struct NkToString<short> {
            static NkString Convert(short v, const NkFormatProps& p) { return NkIntToString(v, p); }
        };
        template<> struct NkToString<int> {
            static NkString Convert(int v, const NkFormatProps& p) { return NkIntToString(v, p); }
        };
        template<> struct NkToString<long> {
            static NkString Convert(long v, const NkFormatProps& p) { return NkIntToString(v, p); }
        };
        template<> struct NkToString<long long> {
            static NkString Convert(long long v, const NkFormatProps& p) { return NkIntToString(v, p); }
        };

        // --- Entiers non-signés ---
        template<> struct NkToString<unsigned char> {
            static NkString Convert(unsigned char v, const NkFormatProps& p) { return NkUIntToString(v, p); }
        };
        template<> struct NkToString<unsigned short> {
            static NkString Convert(unsigned short v, const NkFormatProps& p) { return NkUIntToString(v, p); }
        };
        template<> struct NkToString<unsigned int> {
            static NkString Convert(unsigned int v, const NkFormatProps& p) { return NkUIntToString(v, p); }
        };
        template<> struct NkToString<unsigned long> {
            static NkString Convert(unsigned long v, const NkFormatProps& p) { return NkUIntToString(v, p); }
        };
        template<> struct NkToString<unsigned long long> {
            static NkString Convert(unsigned long long v, const NkFormatProps& p) { return NkUIntToString(v, p); }
        };

        // --- Flottants ---
        template<> struct NkToString<float> {
            static NkString Convert(float v, const NkFormatProps& p) { return NkFloatToString(static_cast<double>(v), p); }
        };
        template<> struct NkToString<double> {
            static NkString Convert(double v, const NkFormatProps& p) { return NkFloatToString(v, p); }
        };
        template<> struct NkToString<long double> {
            static NkString Convert(long double v, const NkFormatProps& p) { return NkFloatToString(static_cast<double>(v), p); }
        };

        // --- Chaînes ---
        template<> struct NkToString<const char*> {
            static NkString Convert(const char* v, const NkFormatProps& props) {
                NkString s(v ? v : "(null)");
                if (props.upperStr)      s.ToUpper();
                else if (props.lowerStr) s.ToLower();
                if (props.HasPrecision() && static_cast<int>(s.Length()) > props.precision)
                    s.Resize(static_cast<NkString::SizeType>(props.precision));
                return NkApplyFormatProps(s, props);
            }
        };
        template<> struct NkToString<char*> {
            static NkString Convert(char* v, const NkFormatProps& p) {
                return NkToString<const char*>::Convert(v, p);
            }
        };
        template<> struct NkToString<NkString> {
            static NkString Convert(const NkString& v, const NkFormatProps& props) {
                NkString s(v);
                if (props.upperStr)      s.ToUpper();
                else if (props.lowerStr) s.ToLower();
                if (props.HasPrecision() && static_cast<int>(s.Length()) > props.precision)
                    s.Resize(static_cast<NkString::SizeType>(props.precision));
                return NkApplyFormatProps(s, props);
            }
        };

        // --- Pointeurs ---
        template<> struct NkToString<void*> {
            static NkString Convert(void* v, const NkFormatProps&) {
                char buf[32]; ::snprintf(buf, sizeof(buf), "%p", v); return NkString(buf);
            }
        };
        template<> struct NkToString<const void*> {
            static NkString Convert(const void* v, const NkFormatProps&) {
                char buf[32]; ::snprintf(buf, sizeof(buf), "%p", v); return NkString(buf);
            }
        };
        template<typename T>
        struct NkToString<T*> {
            static NkString Convert(T* v, const NkFormatProps&) {
                char buf[32]; ::snprintf(buf, sizeof(buf), "%p", static_cast<const void*>(v));
                return NkString(buf);
            }
        };

        // =====================================================================
        // NkFormatArg — argument type-erased (pointeur + fonction de conversion)
        // =====================================================================

        struct NkFormatArg {
            const void* data    = nullptr;
            NkString (*convert)(const void*, const NkFormatProps&) = nullptr;
        };

        template<typename T>
        inline NkFormatArg NkMakeFormatArg(const T& value) {
            NkFormatArg arg;
            arg.data = static_cast<const void*>(&value);
            arg.convert = [](const void* d, const NkFormatProps& p) -> NkString {
                return NkToString<T>::Convert(*static_cast<const T*>(d), p);
            };
            return arg;
        }

        // Spécialisation const char* : le pointeur est lui-même la donnée
        inline NkFormatArg NkMakeFormatArg(const char* value) {
            NkFormatArg arg;
            arg.data = static_cast<const void*>(value);
            arg.convert = [](const void* d, const NkFormatProps& p) -> NkString {
                return NkToString<const char*>::Convert(static_cast<const char*>(d), p);
            };
            return arg;
        }

        // =====================================================================
        // NkFormatArgBuffer — buffer hybride SSO / heap
        //
        //  ≤ NK_FORMAT_SSO_ARGS (16) → stockage sur le stack, zéro allocation
        //  > NK_FORMAT_SSO_ARGS      → débordement sur heap (capacité doublée)
        //
        //  Le débordement est géré via NkIAllocator si fourni, sinon via
        //  ::operator new (fallback garanti sans dépendance STL).
        //
        //  Non-copiable : les NkFormatArg contiennent des pointeurs vers des
        //  temporaires sur le stack de l'appelant — copier serait dangereux.
        // =====================================================================

        static constexpr int NK_FORMAT_SSO_ARGS = 16;

        class NkFormatArgBuffer {
        public:
            explicit NkFormatArgBuffer(memory::NkIAllocator* alloc = nullptr) noexcept
                : mData(mSSO)
                , mCount(0)
                , mCapacity(NK_FORMAT_SSO_ARGS)
                , mAllocator(alloc)
                , mHeap(nullptr)
            {}

            ~NkFormatArgBuffer() {
                FreeHeap();
            }

            NkFormatArgBuffer(const NkFormatArgBuffer&)            = delete;
            NkFormatArgBuffer& operator=(const NkFormatArgBuffer&) = delete;

            void Push(const NkFormatArg& arg) {
                if (mCount == mCapacity) Grow();
                mData[mCount++] = arg;
            }

            const NkFormatArg* Data()  const noexcept { return mData;  }
            int                Count() const noexcept { return mCount; }

        private:
            NkFormatArg  mSSO[NK_FORMAT_SSO_ARGS]; ///< buffer stack (pas d'allocation)
            NkFormatArg* mData;                     ///< pointe sur mSSO ou mHeap
            int          mCount;
            int          mCapacity;
            memory::NkIAllocator* mAllocator;       ///< allocateur custom (peut être null)
            NkFormatArg* mHeap;                     ///< buffer heap (non-nul si débordement)

            void FreeHeap() {
                if (mHeap) {
                    if (mAllocator)
                        mAllocator->Deallocate(mHeap);
                    else
                        ::operator delete(static_cast<void*>(mHeap));
                    mHeap = nullptr;
                    mData = mSSO;
                }
            }

            void Grow() {
                // Double la capacité à chaque débordement (comme NkString)
                int newCap = mCapacity * 2;
                NkFormatArg* newBuf;

                if (mAllocator) {
                    newBuf = static_cast<NkFormatArg*>(
                        mAllocator->Allocate(static_cast<usize>(newCap) * sizeof(NkFormatArg))
                    );
                } else {
                    newBuf = static_cast<NkFormatArg*>(
                        ::operator new(static_cast<size_t>(newCap) * sizeof(NkFormatArg))
                    );
                }

                // Copie triviale des args existants
                for (int i = 0; i < mCount; ++i) newBuf[i] = mData[i];

                FreeHeap();
                mHeap     = newBuf;
                mData     = newBuf;
                mCapacity = newCap;
            }
        };

        // =====================================================================
        // NkFormatImpl — moteur de rendu (implémenté dans NkFormat.cpp)
        // =====================================================================

        NkString NkFormatImpl(const char* fmt, const NkFormatArg* args, int argCount);

        // =====================================================================
        // NkFormat — interface publique variadique C++11
        //
        // Construit le NkFormatArgBuffer sur le stack de l'appelant
        // (SSO pour ≤16 args → zéro allocation dans le cas commun),
        // puis délègue à NkFormatImpl().
        // =====================================================================

        // 0 argument
        inline NkString NkFormat(const char* fmt) {
            return NkFormatImpl(fmt, nullptr, 0);
        }

        namespace detail {
            // Expansion récursive du pack → remplit le buffer un arg à la fois
            inline void NkFillArgBuffer(NkFormatArgBuffer& /*buf*/) {}

            template<typename First, typename... Rest>
            void NkFillArgBuffer(NkFormatArgBuffer& buf,
                                 const First& first,
                                 const Rest&... rest)
            {
                buf.Push(NkMakeFormatArg(first));
                NkFillArgBuffer(buf, rest...);
            }
        } // namespace detail

        // N arguments (illimité grâce au buffer SSO/heap)
        template<typename... Args>
        NkString NkFormat(const char* fmt, const Args&... args) {
            NkFormatArgBuffer buf;
            detail::NkFillArgBuffer(buf, args...);
            return NkFormatImpl(fmt, buf.Data(), buf.Count());
        }

        // Surcharge avec allocateur custom (utile dans les game loops
        // où l'allocateur par défaut est proscrit)
        template<typename... Args>
        NkString NkFormatAlloc(memory::NkIAllocator& alloc,
                               const char* fmt,
                               const Args&... args)
        {
            NkFormatArgBuffer buf(&alloc);
            detail::NkFillArgBuffer(buf, args...);
            return NkFormatImpl(fmt, buf.Data(), buf.Count());
        }

    
} // namespace nkentseu

// =============================================================================
// NkString::Fmt — corps du template (définition hors-classe)
// Activé en incluant ce fichier après NkString.h.
// Usage : NkString::Fmt("{0:w=8 >} = {1:.3}", 42, 3.14)
// =============================================================================
namespace nkentseu {
    
        template<typename... Args>
        inline NkString NkString::Fmt(const Char* format, const Args&... args) {
            NkFormatArgBuffer buf;
            detail::NkFillArgBuffer(buf, args...);
            return NkFormatImpl(format, buf.Data(), buf.Count());
        }
    
} // namespace nkentseu

#endif // NK_CORE_NKFORMAT_H_INCLUDED

/*
## `NkFormat` — style `{i:props}`

**Interface :**
```cpp
NkString s = NkFormat("{0:w=10 >} | {1:.3} | {2:hex # w=6 0}", 42, 3.14159, 255);
// →  "        42 | 3.142 | 0x00ff"
```

**Propriétés disponibles :** `w=N` `<` `>` `^` `fill=C` `0` `.N` `+` `#` `hex` `HEX` `bin` `oct` `sci` `SCI` `upper` `lower` `base=N`

**Extension pour tes types :**
```cpp
template<>
struct NkToString<Vec3> {
    static NkString Convert(const Vec3& v, const NkFormatProps& props) {
        NkString s = NkFormat("({0:.3}, {1:.3}, {2:.3})", v.x, v.y, v.z);
        return NkApplyFormatProps(s, props); // respecte width/align de l'appelant
    }
};
// Utilisation : NkFormat("pos = {0:w=30 ^}", myVec3);
```

---

**Zéro STL** — tout passe par `snprintf` sur des buffers stack, `NkString::Append`, et type erasure via tableau fixe de 32 `NkFormatArg` (aucun `NkVector`, `std::function`, ou `std::string`).
*/