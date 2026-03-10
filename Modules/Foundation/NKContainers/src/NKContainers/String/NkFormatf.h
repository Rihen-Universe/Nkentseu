#pragma once

// =============================================================================
// @File    NkFormatf.h
// @Brief   Formatage style printf sans STL — NkFormatf + NkToFormatf
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis
// @Date    2026-03-06
// @License Apache-2.0
//
// @Design
//
//  DEUX POINTS D'ENTRÉE :
//
//  1) NkFormatf(fmt, ...)  — remplace printf, retourne un NkString
//     Supporte %d %i %u %lu %llu %f %lf %e %g %s %c %p %x %X %o %b %%
//     Modificateurs : largeur, précision, flags (-, +, 0, espace, #)
//
//  2) NkToFormatf<T>  — trait de conversion d'un type T vers NkString
//     spécialisé pour tous les primitifs, extensible par l'utilisateur :
//
//       namespace nkentseu { 
//           template<>
//           struct NkToFormatf<MonType> {
//               static NkString Convert(const MonType& v, const char* opts) {
//                   // opts = la chaîne après le % (ex: "8.2" pour %8.2f)
//                   ...
//               }
//           };
//       }}
//
// =============================================================================

#ifndef NK_CORE_NKFORMATF_H_INCLUDED
#define NK_CORE_NKFORMATF_H_INCLUDED

#include <cstdarg>
#include <cstdio>
#include "NKContainers/NkCompat.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
    

        // =====================================================================
        // NkToFormatf<T>
        // Trait de conversion style printf.
        // opts = string brute entre % et le specifier (ex: "08.3" pour %08.3f)
        // =====================================================================

        template<typename T>
        struct NkToFormatf {
            // Pas de Convert() par défaut → erreur de compilation claire si
            // l'utilisateur utilise un type non spécialisé.
            // Pour étendre : définir une spécialisation dans votre propre header.
        };

        // --- Primitifs intégraux ---

        template<> struct NkToFormatf<bool> {
            static NkString Convert(bool v, const char* /*opts*/) {
                return NkString(v ? "true" : "false");
            }
        };

        template<> struct NkToFormatf<char> {
            static NkString Convert(char v, const char* opts) {
                char buf[8];
                char fmt[16];
                if (opts && *opts) {
                    ::snprintf(fmt, sizeof(fmt), "%%%sc", opts);
                } else {
                    fmt[0] = '%'; fmt[1] = 'c'; fmt[2] = '\0';
                }
                ::snprintf(buf, sizeof(buf), fmt, v);
                return NkString(buf);
            }
        };

        template<> struct NkToFormatf<signed char> {
            static NkString Convert(signed char v, const char* opts) {
                char buf[32]; char fmt[16];
                ::snprintf(fmt, sizeof(fmt), "%%%sd", opts && *opts ? opts : "");
                ::snprintf(buf, sizeof(buf), fmt, static_cast<int>(v));
                return NkString(buf);
            }
        };

        template<> struct NkToFormatf<unsigned char> {
            static NkString Convert(unsigned char v, const char* opts) {
                char buf[32]; char fmt[16];
                ::snprintf(fmt, sizeof(fmt), "%%%su", opts && *opts ? opts : "");
                ::snprintf(buf, sizeof(buf), fmt, static_cast<unsigned>(v));
                return NkString(buf);
            }
        };

        template<> struct NkToFormatf<short> {
            static NkString Convert(short v, const char* opts) {
                char buf[32]; char fmt[16];
                ::snprintf(fmt, sizeof(fmt), "%%%shd", opts && *opts ? opts : "");
                ::snprintf(buf, sizeof(buf), fmt, v);
                return NkString(buf);
            }
        };

        template<> struct NkToFormatf<unsigned short> {
            static NkString Convert(unsigned short v, const char* opts) {
                char buf[32]; char fmt[16];
                ::snprintf(fmt, sizeof(fmt), "%%%shu", opts && *opts ? opts : "");
                ::snprintf(buf, sizeof(buf), fmt, v);
                return NkString(buf);
            }
        };

        template<> struct NkToFormatf<int> {
            static NkString Convert(int v, const char* opts) {
                char buf[32]; char fmt[16];
                ::snprintf(fmt, sizeof(fmt), "%%%sd", opts && *opts ? opts : "");
                ::snprintf(buf, sizeof(buf), fmt, v);
                return NkString(buf);
            }
        };

        template<> struct NkToFormatf<unsigned int> {
            static NkString Convert(unsigned int v, const char* opts) {
                char buf[32]; char fmt[16];
                ::snprintf(fmt, sizeof(fmt), "%%%su", opts && *opts ? opts : "");
                ::snprintf(buf, sizeof(buf), fmt, v);
                return NkString(buf);
            }
        };

        template<> struct NkToFormatf<long> {
            static NkString Convert(long v, const char* opts) {
                char buf[32]; char fmt[16];
                ::snprintf(fmt, sizeof(fmt), "%%%sld", opts && *opts ? opts : "");
                ::snprintf(buf, sizeof(buf), fmt, v);
                return NkString(buf);
            }
        };

        template<> struct NkToFormatf<unsigned long> {
            static NkString Convert(unsigned long v, const char* opts) {
                char buf[32]; char fmt[16];
                ::snprintf(fmt, sizeof(fmt), "%%%slu", opts && *opts ? opts : "");
                ::snprintf(buf, sizeof(buf), fmt, v);
                return NkString(buf);
            }
        };

        template<> struct NkToFormatf<long long> {
            static NkString Convert(long long v, const char* opts) {
                char buf[32]; char fmt[16];
                ::snprintf(fmt, sizeof(fmt), "%%%slld", opts && *opts ? opts : "");
                ::snprintf(buf, sizeof(buf), fmt, v);
                return NkString(buf);
            }
        };

        template<> struct NkToFormatf<unsigned long long> {
            static NkString Convert(unsigned long long v, const char* opts) {
                char buf[32]; char fmt[16];
                ::snprintf(fmt, sizeof(fmt), "%%%sllu", opts && *opts ? opts : "");
                ::snprintf(buf, sizeof(buf), fmt, v);
                return NkString(buf);
            }
        };

        // --- Flottants ---

        template<> struct NkToFormatf<float> {
            static NkString Convert(float v, const char* opts) {
                char buf[64]; char fmt[16];
                ::snprintf(fmt, sizeof(fmt), "%%%sf", opts && *opts ? opts : "");
                ::snprintf(buf, sizeof(buf), fmt, static_cast<double>(v));
                return NkString(buf);
            }
        };

        template<> struct NkToFormatf<double> {
            static NkString Convert(double v, const char* opts) {
                char buf[64]; char fmt[16];
                ::snprintf(fmt, sizeof(fmt), "%%%sf", opts && *opts ? opts : "");
                ::snprintf(buf, sizeof(buf), fmt, v);
                return NkString(buf);
            }
        };

        template<> struct NkToFormatf<long double> {
            static NkString Convert(long double v, const char* opts) {
                char buf[64]; char fmt[16];
                ::snprintf(fmt, sizeof(fmt), "%%%sLf", opts && *opts ? opts : "");
                ::snprintf(buf, sizeof(buf), fmt, v);
                return NkString(buf);
            }
        };

        // --- Chaînes ---

        template<> struct NkToFormatf<const char*> {
            static NkString Convert(const char* v, const char* opts) {
                if (!v) return NkString("(null)");
                if (!opts || !*opts) return NkString(v);
                char buf[512]; char fmt[16];
                ::snprintf(fmt, sizeof(fmt), "%%%ss", opts);
                ::snprintf(buf, sizeof(buf), fmt, v);
                return NkString(buf);
            }
        };

        template<> struct NkToFormatf<char*> {
            static NkString Convert(char* v, const char* opts) {
                return NkToFormatf<const char*>::Convert(v, opts);
            }
        };

        template<> struct NkToFormatf<NkString> {
            static NkString Convert(const NkString& v, const char* opts) {
                return NkToFormatf<const char*>::Convert(v.CStr(), opts);
            }
        };

        // --- Pointeur ---

        template<> struct NkToFormatf<void*> {
            static NkString Convert(void* v, const char* /*opts*/) {
                char buf[32];
                ::snprintf(buf, sizeof(buf), "%p", v);
                return NkString(buf);
            }
        };

        template<typename T>
        struct NkToFormatf<T*> {
            static NkString Convert(T* v, const char* /*opts*/) {
                char buf[32];
                ::snprintf(buf, sizeof(buf), "%p", static_cast<void*>(v));
                return NkString(buf);
            }
        };

        // =====================================================================
        // NkFormatf — moteur de formatage style printf
        //
        // Syntaxe identique à printf : %[flags][width][.precision]specifier
        // Spécifiers supportés :
        //   d i    → int (signé)
        //   u      → unsigned int
        //   ld li  → long
        //   lu     → unsigned long
        //   lld lli → long long
        //   llu    → unsigned long long
        //   f lf   → double
        //   e E    → notation scientifique
        //   g G    → format court
        //   s      → const char* / NkString
        //   c      → char
        //   p      → pointeur
        //   x X    → hexadécimal
        //   o      → octal
        //   b      → binaire (extension)
        //   %%     → littéral %
        // =====================================================================

        /**
        * @brief Formatage style printf, retourne un NkString (pas d'allocation STL)
        * @param fmt  Chaîne de format (style printf)
        * @param ...  Arguments variadiques
        */
        NkString NkFormatf(const char* fmt, ...);

        /**
        * @brief Version avec va_list (pour chaîner depuis d'autres fonctions)
        */
        NkString NkVFormatf(const char* fmt, va_list args);

        // =====================================================================
        // Aide : conversion binaire (non standard dans printf)
        // Accessible séparément pour usage direct
        // =====================================================================

        /**
        * @brief Convertit un entier non-signé en représentation binaire
        * @param value  Valeur à convertir
        * @param width  Largeur minimale (0 = automatique)
        * @param fill   Caractère de remplissage ('0' ou ' ')
        */
        NkString NkToBinaryString(unsigned long long value, int width = 0, char fill = '0');

    
} // namespace nkentseu

#endif // NK_CORE_NKFORMATF_H_INCLUDED


/*
## `NkFormatf` — style printf

**Interface :**
```cpp
NkString s = NkFormatf("%08.3f | %-10s | %b", 3.14159, "hello", 42u);
// → "   3.142 | hello      | 101010"
```

**Extension pour tes types :**
```cpp
template<>
struct NkToFormatf<Vec3> {
    static NkString Convert(const Vec3& v, const char* opts) {
        return NkFormatf("(%s, %s, %s)",
            NkToFormatf<float>::Convert(v.x, opts).CStr(),
            NkToFormatf<float>::Convert(v.y, opts).CStr(),
            NkToFormatf<float>::Convert(v.z, opts).CStr());
    }
};
```
*/