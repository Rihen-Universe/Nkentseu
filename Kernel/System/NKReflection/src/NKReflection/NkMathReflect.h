// =============================================================================
// FICHIER  : Modules/System/NKReflection/src/NKReflection/NkMathReflect.h
// MODULE   : NKReflection
// AUTEUR   : Rihen
// DATE     : 2026-06-25
// VERSION  : 1.1.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Reflexion des types mathematiques NKMath pour la Phase 5. Header-only et
//   OPT-IN : NKReflection ne depend PAS de NKMath, mais tout consommateur
//   disposant de NKMath peut inclure ce header et appeler
//   NkRegisterMathReflection() pour rendre ces types reflechis.
//
//   Une fois enregistre, chaque type math possede un NkClass dont les champs
//   (composantes) sont des proprietes editables a offset fixe. Le serializer
//   (NkReflectSerializer) les traite alors automatiquement comme des sous-objets
//   imbriques (round-trip JSON propre), et l'inspecteur expose chaque composante
//   comme widget separe.
//
//   Approche : chaque composante est une NkProperty a offset fixe. Les types
//   NKMath ciblés ont tous un layout de scalaires CONTIGUS (unions de data[N]
//   ou champs scalaires nommes), de sorte qu'aucune modification de NKMath n'est
//   requise (non invasif). L'offset de la composante i vaut i*sizeof(CompType).
//
//   Types reflechis (v1.1.0) :
//     - Vecteurs : NkVec2/3/4 en variantes f (float32), d (float64), i (int32),
//       u (uint32). Composantes x/y/z/w.
//     - Quaternions : NkQuatf (float32), NkQuatd (float64). Composantes x/y/z/w.
//     - Matrices (column-major, layout T data[N*N] contigu) : NkMat2/3/4 f et d.
//       Composantes nommees m0..mN-1 (suffisant pour round-trip + inspecteur).
//     - Couleurs : NkColorF (4 float32 r/g/b/a), NkColor (4 uint8 r/g/b/a).
//     - Rectangles : NkRect2f/2d (float), NkRect2i/2u (entier). Layout x/y/w/h
//       (4 scalaires contigus, verifie dans NkRectangle.h : union { x,y,width,
//       height } en tete de la classe NkRectT<T>).
//     - Angles : NkAngle (NkAngleT<float32> : un seul membre prive mDeg @0,
//       valeur en DEGRES, sizeof==4, pas de vtable) -> 1 composante "degrees".
//       NkAngleD (NkAngleT<float64>) -> 1 composante "degrees" double.
//     - Angles d'Euler : NkEulerAngle (struct { NkAngle pitch, yaw, roll }) ->
//       3 composantes float32 "pitch"/"yaw"/"roll" @0/4/8 (degres). Pas de
//       variante double (NkEulerAngle est basee sur NkAngle float32 uniquement).
//
//   Layouts NON-triviaux (NON reflechis ici, documentes) :
//     - NkRectangle (specialisation float32 distincte de NkRectT) : compose de
//       deux NkVector2f (corner,size). C'est 4 floats contigus mais la classe a
//       des methodes non-inline ; on prefere exposer NkRect2f (NkRectT<float32>)
//       qui a le meme layout et est la forme recommandee. NkRectangle : reporte.
//     - NkHSV (hue/saturation/value) : 3 floats simples, pourrait etre ajoute si
//       besoin ; non demande -> reporte.
//
//   Zero-STL : conteneurs/chaines NKContainers, allocations NKMemory.
// =============================================================================

#pragma once

#ifndef NK_REFLECTION_NKMATHREFLECT_H
#define NK_REFLECTION_NKMATHREFLECT_H

    #include "NKReflection/NkType.h"
    #include "NKReflection/NkClass.h"
    #include "NKReflection/NkProperty.h"
    #include "NKReflection/NkRegistry.h"

    // NKMath (Foundation) : le consommateur de ce header en dispose forcement.
    #include "NKMath/NkVec.h"
    #include "NKMath/NkQuat.h"
    #include "NKMath/NkMat.h"
    #include "NKMath/NkColor.h"
    #include "NKMath/NkRectangle.h"
    #include "NKMath/NkAngle.h"
    #include "NKMath/NkEulerAngle.h"

    namespace nkentseu {

        namespace reflection {

            // -----------------------------------------------------------------
            // Detail : fabrique generique de NkClass pour un type math.
            // -----------------------------------------------------------------
            namespace detail {

                // Nombre maximal de composantes supporte (Mat4 = 16 floats).
                static constexpr nk_usize NK_MATH_MAX_COMPONENTS = 16;

                /**
                 * @brief Construit (une seule fois) un NkClass pour un type math T
                 *        compose de N composantes scalaires CompType contigues.
                 *
                 * @tparam T         Type math reflechi (ex. NkVec3f, NkMat4f, NkColor).
                 * @tparam CompType  Type scalaire de chaque composante (float32,
                 *                   float64, int32, uint32, uint8).
                 * @tparam N         Nombre de composantes (<= NK_MATH_MAX_COMPONENTS).
                 * @param  className Nom litteral de la classe (duree de vie programme).
                 * @param  names     Tableau de N noms de composantes (duree statique).
                 *
                 * Le NkClass et les NkProperty sont des static locals (duree de vie
                 * programme). Idempotent : le second appel renvoie la meme instance.
                 * Chaque composante i devient une NkProperty(name[i],
                 * NkTypeOf<CompType>(), i*sizeof(CompType)).
                 */
                template<typename T, typename CompType, nk_usize N>
                const NkClass& MakeMathClass(const nk_char* className,
                                             const nk_char* const (&names)[N]) {
                    static_assert(N <= NK_MATH_MAX_COMPONENTS,
                                  "NkMathReflect : N composantes depasse le maximum (16)");

                    static NkClass cls(className, sizeof(T), NkTypeOf<T>());

                    // Construction des NkProperty : on doit fournir un nombre fixe
                    // d'initialiseurs (stockage statique), mais seules les N
                    // premieres sont liees a la classe. Les emplacements au-dela de
                    // N pointent sur le dernier nom valide (jamais utilises).
                    static NkProperty props[NK_MATH_MAX_COMPONENTS] = {
                        NkProperty(names[0 < N ? 0 : 0],  NkTypeOf<CompType>(),  0 * sizeof(CompType)),
                        NkProperty(names[1 < N ? 1 : 0],  NkTypeOf<CompType>(),  1 * sizeof(CompType)),
                        NkProperty(names[2 < N ? 2 : 0],  NkTypeOf<CompType>(),  2 * sizeof(CompType)),
                        NkProperty(names[3 < N ? 3 : 0],  NkTypeOf<CompType>(),  3 * sizeof(CompType)),
                        NkProperty(names[4 < N ? 4 : 0],  NkTypeOf<CompType>(),  4 * sizeof(CompType)),
                        NkProperty(names[5 < N ? 5 : 0],  NkTypeOf<CompType>(),  5 * sizeof(CompType)),
                        NkProperty(names[6 < N ? 6 : 0],  NkTypeOf<CompType>(),  6 * sizeof(CompType)),
                        NkProperty(names[7 < N ? 7 : 0],  NkTypeOf<CompType>(),  7 * sizeof(CompType)),
                        NkProperty(names[8 < N ? 8 : 0],  NkTypeOf<CompType>(),  8 * sizeof(CompType)),
                        NkProperty(names[9 < N ? 9 : 0],  NkTypeOf<CompType>(),  9 * sizeof(CompType)),
                        NkProperty(names[10 < N ? 10 : 0], NkTypeOf<CompType>(), 10 * sizeof(CompType)),
                        NkProperty(names[11 < N ? 11 : 0], NkTypeOf<CompType>(), 11 * sizeof(CompType)),
                        NkProperty(names[12 < N ? 12 : 0], NkTypeOf<CompType>(), 12 * sizeof(CompType)),
                        NkProperty(names[13 < N ? 13 : 0], NkTypeOf<CompType>(), 13 * sizeof(CompType)),
                        NkProperty(names[14 < N ? 14 : 0], NkTypeOf<CompType>(), 14 * sizeof(CompType)),
                        NkProperty(names[15 < N ? 15 : 0], NkTypeOf<CompType>(), 15 * sizeof(CompType)),
                    };

                    static bool s_init = [&]() -> bool {
                        for (nk_usize i = 0; i < N; ++i) {
                            cls.AddProperty(&props[i]);
                        }
                        // Lie NkType(T) -> NkClass pour que le serializer/inspecteur
                        // traite T comme un objet imbrique reflechi.
                        const_cast<NkType&>(NkTypeOf<T>()).SetClass(&cls);
                        return true;
                    }();
                    (void)s_init;
                    return cls;
                }

            } // namespace detail

            /**
             * @brief Enregistre la reflexion des types math NKMath usuels.
             *
             * Idempotent (peut etre appele plusieurs fois sans effet de bord :
             * les NkClass sont des static locals, le re-enregistrement dans le
             * NkRegistry est dedoublonne par adresse).
             *
             * Apres appel, NkTypeOf<...>().GetClass() != nullptr et chaque
             * composante est une NkProperty editable, pour les types :
             *   - NkVec2/3/4 {f,d,i,u}
             *   - NkQuatf, NkQuatd
             *   - NkMat2/3/4 {f,d}  (composantes m0..mN-1, column-major)
             *   - NkColorF (float r/g/b/a), NkColor (uint8 r/g/b/a)
             *   - NkRect2 {f,d,i,u}  (composantes x/y/w/h)
             *   - NkAngle (1 composante "degrees" float32), NkAngleD (double)
             *   - NkEulerAngle (composantes "pitch"/"yaw"/"roll" float32, degres)
             *
             * @note A appeler au demarrage (ou avant toute (de)serialisation /
             *       inspection d'un composant portant des champs math).
             */
            inline void NkRegisterMathReflection() {
                using namespace nkentseu::math;

                // --- Noms ---
                static const nk_char* const vec2Names[2] = { "x", "y" };
                static const nk_char* const vec3Names[3] = { "x", "y", "z" };
                static const nk_char* const vec4Names[4] = { "x", "y", "z", "w" };
                static const nk_char* const rect4Names[4] = { "x", "y", "w", "h" };
                static const nk_char* const rgbaNames[4]  = { "r", "g", "b", "a" };
                static const nk_char* const angleNames[1] = { "degrees" };
                static const nk_char* const eulerNames[3] = { "pitch", "yaw", "roll" };

                // Noms de matrices m0..m15 (statique, duree programme).
                static const nk_char* const matNames[16] = {
                    "m0",  "m1",  "m2",  "m3",  "m4",  "m5",  "m6",  "m7",
                    "m8",  "m9",  "m10", "m11", "m12", "m13", "m14", "m15"
                };
                // Sous-vues de longueur exacte pour Mat2 (4) et Mat3 (9).
                static const nk_char* const mat2Names[4] = {
                    "m0", "m1", "m2", "m3"
                };
                static const nk_char* const mat3Names[9] = {
                    "m0", "m1", "m2", "m3", "m4", "m5", "m6", "m7", "m8"
                };

                NkRegistry& reg = NkRegistry::Get();

                // --- Vecteurs float32 ---
                reg.RegisterClass(&detail::MakeMathClass<NkVec2f, nk_float32, 2>("NkVec2f", vec2Names));
                reg.RegisterClass(&detail::MakeMathClass<NkVec3f, nk_float32, 3>("NkVec3f", vec3Names));
                reg.RegisterClass(&detail::MakeMathClass<NkVec4f, nk_float32, 4>("NkVec4f", vec4Names));

                // --- Vecteurs float64 ---
                reg.RegisterClass(&detail::MakeMathClass<NkVec2d, nk_float64, 2>("NkVec2d", vec2Names));
                reg.RegisterClass(&detail::MakeMathClass<NkVec3d, nk_float64, 3>("NkVec3d", vec3Names));
                reg.RegisterClass(&detail::MakeMathClass<NkVec4d, nk_float64, 4>("NkVec4d", vec4Names));

                // --- Vecteurs int32 ---
                reg.RegisterClass(&detail::MakeMathClass<NkVec2i, nk_int32, 2>("NkVec2i", vec2Names));
                reg.RegisterClass(&detail::MakeMathClass<NkVec3i, nk_int32, 3>("NkVec3i", vec3Names));
                reg.RegisterClass(&detail::MakeMathClass<NkVec4i, nk_int32, 4>("NkVec4i", vec4Names));

                // --- Vecteurs uint32 ---
                reg.RegisterClass(&detail::MakeMathClass<NkVec2u, nk_uint32, 2>("NkVec2u", vec2Names));
                reg.RegisterClass(&detail::MakeMathClass<NkVec3u, nk_uint32, 3>("NkVec3u", vec3Names));
                reg.RegisterClass(&detail::MakeMathClass<NkVec4u, nk_uint32, 4>("NkVec4u", vec4Names));

                // --- Quaternions ---
                reg.RegisterClass(&detail::MakeMathClass<NkQuatf, nk_float32, 4>("NkQuatf", vec4Names));
                reg.RegisterClass(&detail::MakeMathClass<NkQuatd, nk_float64, 4>("NkQuatd", vec4Names));

                // --- Matrices (column-major, data[N*N] contigu) ---
                reg.RegisterClass(&detail::MakeMathClass<NkMat2f, nk_float32, 4>("NkMat2f", mat2Names));
                reg.RegisterClass(&detail::MakeMathClass<NkMat3f, nk_float32, 9>("NkMat3f", mat3Names));
                reg.RegisterClass(&detail::MakeMathClass<NkMat4f, nk_float32, 16>("NkMat4f", matNames));

                reg.RegisterClass(&detail::MakeMathClass<NkMat2d, nk_float64, 4>("NkMat2d", mat2Names));
                reg.RegisterClass(&detail::MakeMathClass<NkMat3d, nk_float64, 9>("NkMat3d", mat3Names));
                reg.RegisterClass(&detail::MakeMathClass<NkMat4d, nk_float64, 16>("NkMat4d", matNames));

                // --- Couleurs ---
                reg.RegisterClass(&detail::MakeMathClass<NkColorF, nk_float32, 4>("NkColorF", rgbaNames));
                reg.RegisterClass(&detail::MakeMathClass<NkColor, nk_uint8, 4>("NkColor", rgbaNames));

                // --- Rectangles (NkRectT<T> : x/y/w/h contigus) ---
                reg.RegisterClass(&detail::MakeMathClass<NkRect2f, nk_float32, 4>("NkRect2f", rect4Names));
                reg.RegisterClass(&detail::MakeMathClass<NkRect2d, nk_float64, 4>("NkRect2d", rect4Names));
                reg.RegisterClass(&detail::MakeMathClass<NkRect2i, nk_int32, 4>("NkRect2i", rect4Names));
                reg.RegisterClass(&detail::MakeMathClass<NkRect2u, nk_uint32, 4>("NkRect2u", rect4Names));

                // --- Angles (valeur stockee en DEGRES) ---
                // NkAngle = NkAngleT<float32> : un seul float mDeg @0, pas de vtable.
                reg.RegisterClass(&detail::MakeMathClass<NkAngle, nk_float32, 1>("NkAngle", angleNames));
                reg.RegisterClass(&detail::MakeMathClass<NkAngleD, nk_float64, 1>("NkAngleD", angleNames));

                // --- Angles d'Euler (3 NkAngle float32 contigus : pitch/yaw/roll) ---
                reg.RegisterClass(&detail::MakeMathClass<NkEulerAngle, nk_float32, 3>("NkEulerAngle", eulerNames));
            }

        } // namespace reflection

    } // namespace nkentseu

#endif // NK_REFLECTION_NKMATHREFLECT_H

// ============================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// ============================================================
