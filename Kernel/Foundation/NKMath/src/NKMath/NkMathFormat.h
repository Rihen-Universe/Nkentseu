// -----------------------------------------------------------------------------
// FICHIER: NKMath\NkMathFormat.h
// DESCRIPTION: Spécialisations de NkFormatter<T> pour tous les types NkMath.
//
// UTILISATION :
//   #include "NKMath/NkMathFormat.h"
//   // Ensuite NkFormat("{0}", someVec3) fonctionne automatiquement.
//
// MODES D'AFFICHAGE pour NkVec4T (via le caractère de type dans la spec) :
//   {0}   → (x, y, z, w)       affichage normal  (4 composantes)
//   {0:v} → vec3(x, y, z)      mode vecteur/direction (ignore w)
//   {0:p} → pos(x, y, z)       mode position (ignore w)
//   {0:c} → rgba(r, g, b, a)   mode couleur/RGBA
//   {0:n} → norm(x, y, z)      mode direction normalisée (ignore w)
//
// MODES D'AFFICHAGE pour NkAngleT :
//   {0}   → valeur en degrés
//   {0:d} → "valeur_deg"
//   {0:r} → "valeur_rad"
//
// NOTE: Ce fichier utilise des spécialisations de NkFormatter<T> (point
// d'extension principal) plutôt que NkToString pour éviter les problèmes
// de recherche ADL en deux phases avec les types dans nkentseu::math.
//
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_MATH_NKMATHFORMAT_H
#define NKENTSEU_MATH_NKMATHFORMAT_H

    #include "NKContainers/String/NkFormat.h"
    #include "NKMath/NkVec.h"
    #include "NKMath/NkAngle.h"
    #include "NKMath/NkEulerAngle.h"
    #include "NKMath/NkRange.h"
    #include "NKMath/NkMat.h"
    #include "NKMath/NkQuat.h"
    #include "NKMath/NkRectangle.h"
    #include "NKMath/NkColor.h"

    namespace nkentseu {

        // =====================================================================
        // NkVec2T<T>  →  (x, y)
        // =====================================================================
        template<typename T>
        struct NkFormatter<math::NkVec2T<T>> {
            static NkString Convert(
                const math::NkVec2T<T>& val,
                const NkFormatProps& props
            ) {
                return props.ApplyWidth(
                    NkStringView(NkFormat("({0}, {1})", val.x, val.y)),
                    false
                );
            }
        };


        // =====================================================================
        // NkVec3T<T>  →  (x, y, z)
        // =====================================================================
        template<typename T>
        struct NkFormatter<math::NkVec3T<T>> {
            static NkString Convert(
                const math::NkVec3T<T>& val,
                const NkFormatProps& props
            ) {
                return props.ApplyWidth(
                    NkStringView(NkFormat("({0}, {1}, {2})", val.x, val.y, val.z)),
                    false
                );
            }
        };


        // =====================================================================
        // NkVec4T<T>  →  multi-mode selon props.type
        //
        //   '\0'  (défaut) : (x, y, z, w)
        //   'v'            : vec3(x, y, z)   — direction 3D, ignore w
        //   'p'            : pos(x, y, z)    — position 3D, ignore w
        //   'c'            : rgba(r, g, b, a) — interprétation couleur
        //   'n'            : norm(x, y, z)   — direction normalisée, ignore w
        // =====================================================================
        template<typename T>
        struct NkFormatter<math::NkVec4T<T>> {
            static NkString Convert(
                const math::NkVec4T<T>& val,
                const NkFormatProps& props
            ) {
                NkString result;
                switch (props.type) {
                    case 'v':
                        result = NkFormat("vec3({0}, {1}, {2})", val.x, val.y, val.z);
                        break;
                    case 'p':
                        result = NkFormat("pos({0}, {1}, {2})", val.x, val.y, val.z);
                        break;
                    case 'c':
                        result = NkFormat("rgba({0}, {1}, {2}, {3})", val.r, val.g, val.b, val.a);
                        break;
                    case 'n':
                        result = NkFormat("norm({0}, {1}, {2})", val.x, val.y, val.z);
                        break;
                    default:
                        result = NkFormat("({0}, {1}, {2}, {3})", val.x, val.y, val.z, val.w);
                        break;
                }
                return props.ApplyWidth(NkStringView(result), false);
            }
        };


        // =====================================================================
        // NkAngleT<float32>
        //
        // ATTENTION: NE PAS appeler val.ToString() ici car ToString() appelle
        // NkFormat("{0}", *this) ce qui provoquerait une récursion infinie.
        // =====================================================================
        template<>
        struct NkFormatter<math::NkAngleT<float32>> {
            static NkString Convert(
                const math::NkAngleT<float32>& val,
                const NkFormatProps& props
            ) {
                NkString result;
                if (props.type == 'd') {
                    result = NkFormat("{0}_deg", val.Deg());
                } else if (props.type == 'r') {
                    result = NkFormat("{0}_rad", val.Rad());
                } else {
                    result = NkFormat("{0}", val.Deg());
                }
                return props.ApplyWidth(NkStringView(result), false);
            }
        };


        // =====================================================================
        // NkAngleT<float64>  (même logique que float32)
        // =====================================================================
        template<>
        struct NkFormatter<math::NkAngleT<float64>> {
            static NkString Convert(
                const math::NkAngleT<float64>& val,
                const NkFormatProps& props
            ) {
                NkString result;
                if (props.type == 'd') {
                    result = NkFormat("{0}_deg", val.Deg());
                } else if (props.type == 'r') {
                    result = NkFormat("{0}_rad", val.Rad());
                } else {
                    result = NkFormat("{0}", val.Deg());
                }
                return props.ApplyWidth(NkStringView(result), false);
            }
        };


        // =====================================================================
        // NkEulerAngle  →  euler(pitch, yaw, roll)  en degrés
        // =====================================================================
        template<>
        struct NkFormatter<math::NkEulerAngle> {
            static NkString Convert(
                const math::NkEulerAngle& val,
                const NkFormatProps& props
            ) {
                return props.ApplyWidth(NkStringView(val.ToString()), false);
            }
        };


        // =====================================================================
        // NkRangeT<T>  →  [min, max]
        // =====================================================================
        template<typename T>
        struct NkFormatter<math::NkRangeT<T>> {
            static NkString Convert(
                const math::NkRangeT<T>& val,
                const NkFormatProps& props
            ) {
                return props.ApplyWidth(NkStringView(val.ToString()), false);
            }
        };


        // =====================================================================
        // NkMat2T<T>  →  multi-ligne column-major
        // =====================================================================
        template<typename T>
        struct NkFormatter<math::NkMat2T<T>> {
            static NkString Convert(
                const math::NkMat2T<T>& val,
                const NkFormatProps& props
            ) {
                return props.ApplyWidth(NkStringView(val.ToString()), false);
            }
        };


        // =====================================================================
        // NkMat3T<T>  →  multi-ligne column-major
        // =====================================================================
        template<typename T>
        struct NkFormatter<math::NkMat3T<T>> {
            static NkString Convert(
                const math::NkMat3T<T>& val,
                const NkFormatProps& props
            ) {
                return props.ApplyWidth(NkStringView(val.ToString()), false);
            }
        };


        // =====================================================================
        // NkMat4T<T>  →  multi-ligne column-major
        // =====================================================================
        template<typename T>
        struct NkFormatter<math::NkMat4T<T>> {
            static NkString Convert(
                const math::NkMat4T<T>& val,
                const NkFormatProps& props
            ) {
                return props.ApplyWidth(NkStringView(val.ToString()), false);
            }
        };


        // =====================================================================
        // NkQuatT<T>  →  [x, y, z; w]
        // =====================================================================
        template<typename T>
        struct NkFormatter<math::NkQuatT<T>> {
            static NkString Convert(
                const math::NkQuatT<T>& val,
                const NkFormatProps& props
            ) {
                return props.ApplyWidth(NkStringView(val.ToString()), false);
            }
        };


        // =====================================================================
        // NkRectangle  →  format défini dans NkRectangle.cpp
        // =====================================================================
        template<>
        struct NkFormatter<math::NkRectangle> {
            static NkString Convert(
                const math::NkRectangle& val,
                const NkFormatProps& props
            ) {
                return props.ApplyWidth(NkStringView(val.ToString()), false);
            }
        };


        // =====================================================================
        // NkRectT<T>  →  NkRectT[pos(x, y); size(w, h)]
        // =====================================================================
        template<typename T>
        struct NkFormatter<math::NkRectT<T>> {
            static NkString Convert(
                const math::NkRectT<T>& val,
                const NkFormatProps& props
            ) {
                return props.ApplyWidth(NkStringView(val.ToString()), false);
            }
        };


        // =====================================================================
        // NkSegment  →  format défini dans NkSegment.cpp
        // =====================================================================
        template<>
        struct NkFormatter<math::NkSegment> {
            static NkString Convert(
                const math::NkSegment& val,
                const NkFormatProps& props
            ) {
                return props.ApplyWidth(NkStringView(val.ToString()), false);
            }
        };


        // =====================================================================
        // NkColor  →  format défini dans NkColor.cpp (RGBA 8-bit)
        //
        // Mode supplémentaire via props.type :
        //   '\0' (défaut) : délègue à ToString()
        //   'h'           : #RRGGBBAA  (hexadécimal)
        // =====================================================================
        template<>
        struct NkFormatter<math::NkColor> {
            static NkString Convert(
                const math::NkColor& val,
                const NkFormatProps& props
            ) {
                if (props.type == 'h') {
                    NkString result = NkFormat(
                        "#{0:02X}{1:02X}{2:02X}{3:02X}",
                        static_cast<unsigned>(val.r),
                        static_cast<unsigned>(val.g),
                        static_cast<unsigned>(val.b),
                        static_cast<unsigned>(val.a)
                    );
                    return props.ApplyWidth(NkStringView(result), false);
                }
                return props.ApplyWidth(NkStringView(val.ToString()), false);
            }
        };


        // =====================================================================
        // NkColorF  →  format défini dans NkColor.cpp (RGBA float [0,1])
        //
        // Mode supplémentaire via props.type :
        //   '\0' (défaut) : délègue à ToString()
        //   'h'           : #RRGGBBAA  (hexadécimal, conversion float→8bit)
        // =====================================================================
        template<>
        struct NkFormatter<math::NkColorF> {
            static NkString Convert(
                const math::NkColorF& val,
                const NkFormatProps& props
            ) {
                if (props.type == 'h') {
                    auto clamp01 = [](float32 v) -> unsigned {
                        if (v < 0.0f) v = 0.0f;
                        if (v > 1.0f) v = 1.0f;
                        return static_cast<unsigned>(v * 255.0f + 0.5f);
                    };
                    NkString result = NkFormat(
                        "#{0:02X}{1:02X}{2:02X}{3:02X}",
                        clamp01(val.r),
                        clamp01(val.g),
                        clamp01(val.b),
                        clamp01(val.a)
                    );
                    return props.ApplyWidth(NkStringView(result), false);
                }
                return props.ApplyWidth(NkStringView(val.ToString()), false);
            }
        };

    } // namespace nkentseu

#endif // NKENTSEU_MATH_NKMATHFORMAT_H

// =============================================================================
// EXEMPLES D'UTILISATION
// =============================================================================
/*
    #include "NKMath/NkMathFormat.h"
    using namespace nkentseu::math;

    // Vecteurs
    NkVec2f uv(0.5f, 1.0f);
    NkVec3f pos(1.0f, 2.0f, 3.0f);
    NkVec4f color(1.0f, 0.5f, 0.0f, 1.0f);

    NkFormat("{0}", uv);          // (0.5, 1)
    NkFormat("{0}", pos);         // (1, 2, 3)
    NkFormat("{0}", color);       // (1, 0.5, 0, 1)     — mode normal
    NkFormat("{0:v}", color);     // vec3(1, 0.5, 0)    — mode vecteur
    NkFormat("{0:p}", color);     // pos(1, 0.5, 0)     — mode position
    NkFormat("{0:c}", color);     // rgba(1, 0.5, 0, 1) — mode couleur
    NkFormat("{0:n}", color);     // norm(1, 0.5, 0)    — mode normalisé

    // Angles
    NkAngle angle(45.0f);
    NkFormat("{0}", angle);       // 45
    NkFormat("{0:d}", angle);     // 45_deg
    NkFormat("{0:r}", angle);     // 0.785398_rad

    // Angles d'Euler
    NkEulerAngle euler(NkAngle(10.0f), NkAngle(45.0f), NkAngle(0.0f));
    NkFormat("{0}", euler);       // euler(10, 45, 0)

    // Matrices
    NkMat4f identity;
    NkFormat("{0}", identity);    // affichage multi-ligne column-major

    // Quaternions
    NkQuatf q;
    NkFormat("{0}", q);           // [0, 0, 0; 1]

    // Intervalles
    NkRangeT<float32> range(0.0f, 1.0f);
    NkFormat("{0}", range);       // [0, 1]

    // Couleurs
    NkColor red(255, 0, 0, 255);
    NkFormat("{0}", red);         // délègue à ToString()
    NkFormat("{0:h}", red);       // #FF0000FF

    NkColorF redf(1.0f, 0.0f, 0.0f, 1.0f);
    NkFormat("{0:h}", redf);      // #FF0000FF
*/
