// -----------------------------------------------------------------------------
// FICHIER: NKMath\NkColor.h
// DESCRIPTION: Classes NkColor (RGBA 8-bit) et NkColorF (RGBA float [0,1])
//              avec conversions bidirectionnelles et helpers HSV
// AUTEUR: Rihen
// DATE: 2026-04-26
// VERSION: 2.1.0
// LICENCE: Proprietary - Free to use and modify
// -----------------------------------------------------------------------------
//
// RÉSUMÉ:
//   Représentation de couleurs en deux formats :
//   - NkColor : RGBA compact sur 32 bits (uint8 par composante, [0,255])
//   - NkColorF : RGBA flottant sur 128 bits (float32 par composante, [0,1])
//   Conversions bidirectionnelles automatiques, interpolation, HSV, et bibliothèque
//   étendue de couleurs nommées prédéfinies.
//
// CARACTÉRISTIQUES:
//   - NkColor : stockage compact (4 bytes), idéal pour rendu, UI, stockage
//   - NkColorF : précision flottante, idéal pour calculs, shaders, interpolations
//   - Conversions implicites/explicites entre NkColor et NkColorF
//   - Opérations HSV disponibles sur les deux types (via conversion si besoin)
//   - Interpolation linéaire (Lerp) native sur les deux types
//   - Couleurs nommées : constantes statiques accessibles via NkColor::Red, etc.
//   - Lookup par nom : FromName() avec recherche linéaire optimisée
//   - Fonctions critiques inline avec NKENTSEU_MATH_API_FORCE_INLINE
//   - Fonctions non-inline exportées avec NKENTSEU_MATH_API
//
// DÉPENDANCES:
//   - NKCore/NkTypes.h          : Types fondamentaux (uint8, uint32, float32)
//   - NKMath/NkMathApi.h        : Macros d'export NKENTSEU_MATH_API
//   - NKMath/NkFunctions.h      : Fonctions mathématiques NkClamp, NkMin, NkMax
//   - NKCore/NkString.h         : Chaîne de caractères pour ToString()
//   - NKMath/NkVec.h            : Vecteurs NkVector3f/NkVector4f pour conversions
//   - NKCore/NkRandom.h         : Générateur aléatoire pour RandomRGB/RandomRGBA
//
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_MATH_NKCOLOR_H
#define NKENTSEU_MATH_NKCOLOR_H

    // ========================================================================
    // INCLUSIONS DES DÉPENDANCES
    // ========================================================================

    #include "NKCore/NkTypes.h"                    // Types fondamentaux : uint8, uint32, float32
    #include "NKMath/NkMathApi.h"                  // Macros d'export : NKENTSEU_MATH_API, NKENTSEU_MATH_API_FORCE_INLINE
    #include "NKMath/NkFunctions.h"                // Fonctions mathématiques : NkClamp, NkMin, NkMax
    #include "NKContainers/String/NkFormat.h"        // Classe NkString et NkFormatProps pour représentation texte
    #include "NKMath/NkVec.h"                      // Vecteurs NkVector3f/NkVector4f pour conversions

    // Forward declaration pour éviter dépendance circulaire
    namespace nkentseu { class NkRandom; }

    // ========================================================================
    // ESPACE DE NOMS PRINCIPAL
    // ========================================================================

    namespace nkentseu {

        // ====================================================================
        // NAMESPACE : MATH (COMPOSANTES MATHÉMATIQUES)
        // ====================================================================

        namespace math {

            // ====================================================================
            // STRUCTURE : NKHSV (REPRÉSENTATION TEINTE/SATURATION/VALEUR)
            // ====================================================================

            /**
             * @brief Structure légère pour la représentation HSV (Teinte/Saturation/Valeur)
             * 
             * Utilisée principalement pour les conversions et manipulations colorimétriques.
             * Stocke les composantes dans des plages standardisées :
             * - Teinte (hue) : [0, 360) degrés (0=rouge, 120=vert, 240=bleu)
             * - Saturation : [0, 100]% (0=gris, 100=couleur pure)
             * - Valeur : [0, 100]% (0=noir, 100=luminosité maximale)
             * 
             * @note
             *   - Pas de wrapping automatique : les valeurs sont clampées à la construction
             *   - Opérateurs + et - définis pour les calculs vectoriels simples
             *   - Conversion vers NkColor/NkColorF via FromHSV()
             *   - Conversion depuis NkColor/NkColorF via ToHSV()
             */
            struct NkHSV {

                float32 hue;        ///< Teinte en degrés [0, 360)
                float32 saturation; ///< Saturation en pourcentage [0, 100]
                float32 value;      ///< Valeur/luminosité en pourcentage [0, 100]

                // ====================================================================
                // SECTION PUBLIQUE : CONSTRUCTEURS
                // ====================================================================

                /**
                 * @brief Constructeur par défaut : noir (HSV = 0,0,0)
                 * @note Initialisation à zéro pour toutes les composantes
                 * @note constexpr : évaluable à la compilation
                 */
                constexpr NkHSV() noexcept
                    : hue(0.0f), saturation(0.0f), value(0.0f)
                {
                }

                /**
                 * @brief Constructeur depuis composantes HSV avec clamping automatique
                 * @param h Teinte en degrés [0, 360)
                 * @param s Saturation en pourcentage [0, 100]
                 * @param v Valeur en pourcentage [0, 100]
                 * @note Clampe automatiquement chaque composante dans sa plage valide
                 * @note Utile pour construire des couleurs HSV sans validation manuelle
                 */
                NkHSV(float32 h, float32 s, float32 v) noexcept
                    : hue(NkClamp(h, 0.0f, 360.0f))
                    , saturation(NkClamp(s, 0.0f, 100.0f))
                    , value(NkClamp(v, 0.0f, 100.0f))
                {
                }


                // ====================================================================
                // SECTION PUBLIQUE : OPÉRATEURS ARITHMÉTIQUES
                // ====================================================================

                /**
                 * @brief Addition composante par composante de deux structures HSV
                 * @param a Première structure HSV
                 * @param b Seconde structure HSV
                 * @return Nouvelle structure HSV avec somme des composantes
                 * @note Pas de clamping automatique : les résultats peuvent sortir des plages valides
                 * @note Utile pour des calculs intermédiaires avant normalisation
                 */
                friend NKENTSEU_MATH_API_FORCE_INLINE
                NkHSV operator+(const NkHSV& a, const NkHSV& b) noexcept
                {
                    return { a.hue + b.hue, a.saturation + b.saturation, a.value + b.value };
                }

                /**
                 * @brief Soustraction composante par composante de deux structures HSV
                 * @param a Première structure HSV
                 * @param b Seconde structure HSV
                 * @return Nouvelle structure HSV avec différence des composantes
                 * @note Pas de clamping automatique : les résultats peuvent être négatifs
                 * @note Utile pour calculer des deltas ou des différences colorimétriques
                 */
                friend NKENTSEU_MATH_API_FORCE_INLINE
                NkHSV operator-(const NkHSV& a, const NkHSV& b) noexcept
                {
                    return { a.hue - b.hue, a.saturation - b.saturation, a.value - b.value };
                }

            }; // struct NkHSV


            // ====================================================================
            // STRUCTURE : NKCOLORF (COULEUR RGBA FLOTTANTE [0,1])
            // ====================================================================

            /**
             * @brief Structure légère pour la représentation de couleurs en flottants [0,1]
             * 
             * Version flottante de NkColor, idéale pour :
             * - Calculs mathématiques précis (interpolations, mélanges, shaders)
             * - Opérations avec tolérance flottante
             * - Conversion vers/depuis des formats graphiques (OpenGL, DirectX, Vulkan)
             * - Interpolation fluide sans perte de précision due à la quantification 8-bit
             * 
             * PRINCIPE DE FONCTIONNEMENT :
             * - Stockage interne : float32 r, g, b, a ∈ [0, 1]
             * - Conversion depuis/vers NkColor : multiplication/division par 255
             * - Opérations arithmétiques définies (+, -, *, /) avec clamping optionnel
             * - Interpolation linéaire native via Lerp()
             * - Conversion HSV via NkColor (conversion intermédiaire si besoin)
             * 
             * GARANTIES DE PERFORMANCE :
             * - Constructeurs/accesseurs : O(1), tous inline pour élimination d'appel
             * - Mémoire : sizeof(NkColorF) == 16 bytes (4 × float32)
             * - Conversions NkColor ↔ NkColorF : 4 multiplications/divisions, négligeable
             * - Pas d'allocation dynamique : structure POD (Plain Old Data)
             * 
             * CAS D'USAGE TYPQUES :
             * - Shaders GPU : upload direct de couleurs en float [0,1]
             * - Interpolation de couleurs : Lerp fluide sans artefacts de quantification
             * - Calculs colorimétriques : mélanges, contrastes, ajustements précis
             * - Conversion depuis formats flottants : textures HDR, calculs physiques
             * - Debug/visualisation : affichage de valeurs précises sans arrondi 8-bit
             * 
             * @note
             *   - Alpha par défaut = 1.0f (opaque) dans le constructeur par défaut
             *   - Les opérations arithmétiques ne clampent pas automatiquement : responsabilité du caller
             *   - Conversion vers NkColor applique NkClamp pour garantir [0,255]
             *   - Compatible avec NkVector4f via conversion explicite
             * 
             * @warning
             *   - Les valeurs hors [0,1] ne sont pas clampées automatiquement (sauf conversion vers NkColor)
             *   - La précision float32 peut causer des erreurs d'arrondi minimes dans les calculs répétés
             *   - NkColorF n'a pas de couleurs nommées : utiliser NkColor::Red, puis convertir si besoin
             */
            struct NkColorF {

                float32 r; ///< Composante rouge [0, 1]
                float32 g; ///< Composante verte [0, 1]
                float32 b; ///< Composante bleue [0, 1]
                float32 a; ///< Composante alpha [0, 1] (0=transparent, 1=opaque)

                // ====================================================================
                // SECTION PUBLIQUE : CONSTANTES ASSOCIÉES
                // ====================================================================

                /** @brief Constante pour conversion entier → flottant : 1/255 */
                static constexpr float32 kOneOver255 = 1.0f / 255.0f;

                /** @brief Constante pour conversion flottant → entier : 255 */
                static constexpr float32 k255 = 255.0f;


                // ====================================================================
                // SECTION PUBLIQUE : CONSTRUCTEURS
                // ====================================================================

                /**
                 * @brief Constructeur par défaut : noir opaque (0,0,0,1)
                 * @note Initialisation à zéro pour RGB, alpha=1 (opaque)
                 * @note constexpr : évaluable à la compilation
                 */
                constexpr NkColorF() noexcept
                    : r(0.0f), g(0.0f), b(0.0f), a(1.0f)
                {
                }

                /**
                 * @brief Constructeur depuis composantes flottantes RGBA
                 * @param r Composante rouge ∈ [0, 1]
                 * @param g Composante verte ∈ [0, 1]
                 * @param b Composante bleue ∈ [0, 1]
                 * @param a Composante alpha ∈ [0, 1] (défaut: 1.0 = opaque)
                 * @note Pas de clamping automatique : responsabilité du caller
                 * @note constexpr : évaluable à la compilation
                 */
                constexpr NkColorF(float32 r, float32 g, float32 b, float32 a = 1.0f) noexcept
                    : r(r), g(g), b(b), a(a)
                {
                }

                /**
                 * @brief Constructeur depuis NkColor (conversion 8-bit → float)
                 * @param color Couleur NkColor à convertir
                 * @note Conversion : float = uint8 * (1/255)
                 * @note NKENTSEU_MATH_API_FORCE_INLINE : candidate à l'inlining agressif
                 */
                NKENTSEU_MATH_API_FORCE_INLINE
                explicit NkColorF(const class NkColor& color) noexcept;

                /**
                 * @brief Constructeur depuis NkVector4f
                 * @param v Vecteur avec composantes [r, g, b, a] ∈ [0, 1]
                 * @note Copie directe des composantes, pas de clamping
                 * @note NKENTSEU_MATH_API_FORCE_INLINE : candidate à l'inlining
                 */
                NKENTSEU_MATH_API_FORCE_INLINE
                explicit NkColorF(const NkVector4f& v) noexcept
                    : r(v.r), g(v.g), b(v.b), a(v.a)
                {
                }

                /**
                 * @brief Constructeur depuis NkVector3f (alpha=1)
                 * @param v Vecteur avec composantes [r, g, b] ∈ [0, 1]
                 * @note Alpha fixé à 1.0 (opaque) par défaut
                 * @note NKENTSEU_MATH_API_FORCE_INLINE : candidate à l'inlining
                 */
                NKENTSEU_MATH_API_FORCE_INLINE
                explicit NkColorF(const NkVector3f& v) noexcept
                    : r(v.r), g(v.g), b(v.b), a(1.0f)
                {
                }


                // ====================================================================
                // SECTION PUBLIQUE : CONVERSIONS
                // ====================================================================

                /**
                 * @brief Convertit la couleur flottante en NkColor (8-bit)
                 * @return Nouvelle couleur NkColor avec composantes quantifiées [0,255]
                 * @note Applique NkClamp pour garantir [0,1] avant conversion
                 * @note NKENTSEU_MATH_API_FORCE_INLINE : candidate à l'inlining
                 */
                NKENTSEU_MATH_API_FORCE_INLINE
                class NkColor ToColor() const noexcept;

                /**
                 * @brief Conversion explicite vers NkVector4f
                 * @return Vecteur avec composantes [r, g, b, a] ∈ [0, 1]
                 * @note Copie directe des composantes
                 * @note explicit : évite les conversions accidentelles
                 */
                NKENTSEU_MATH_API_FORCE_INLINE
                explicit operator NkVector4f() const noexcept
                {
                    return { r, g, b, a };
                }

                /**
                 * @brief Conversion explicite vers NkVector3f (alpha ignoré)
                 * @return Vecteur avec composantes [r, g, b] ∈ [0, 1]
                 * @note Copie directe des composantes RGB
                 * @note explicit : évite les conversions accidentelles
                 */
                NKENTSEU_MATH_API_FORCE_INLINE
                explicit operator NkVector3f() const noexcept
                {
                    return { r, g, b };
                }

                /**
                 * @brief Conversion implicite depuis NkColor
                 * @param color Couleur NkColor à convertir
                 * @return Nouvelle couleur NkColorF équivalente
                 * @note Permet : NkColorF f = someNkColor;
                 */
                NKENTSEU_MATH_API_FORCE_INLINE
                friend NkColorF FromColor(const class NkColor& color) noexcept;


                // ====================================================================
                // SECTION PUBLIQUE : OPÉRATEURS ARITHMÉTIQUES
                // ====================================================================

                /**
                 * @brief Addition composante par composante de deux couleurs flottantes
                 * @param a Première couleur
                 * @param b Seconde couleur
                 * @return Nouvelle couleur avec somme des composantes
                 * @note Pas de clamping automatique : les résultats peuvent dépasser [0,1]
                 * @note Utile pour des calculs intermédiaires avant normalisation
                 */
                friend NKENTSEU_MATH_API_FORCE_INLINE
                NkColorF operator+(const NkColorF& a, const NkColorF& b) noexcept
                {
                    return { a.r + b.r, a.g + b.g, a.b + b.b, a.a + b.a };
                }

                /**
                 * @brief Soustraction composante par composante de deux couleurs flottantes
                 * @param a Première couleur
                 * @param b Seconde couleur
                 * @return Nouvelle couleur avec différence des composantes
                 * @note Pas de clamping automatique : les résultats peuvent être négatifs
                 * @note Utile pour calculer des deltas ou des différences colorimétriques
                 */
                friend NKENTSEU_MATH_API_FORCE_INLINE
                NkColorF operator-(const NkColorF& a, const NkColorF& b) noexcept
                {
                    return { a.r - b.r, a.g - b.g, a.b - b.b, a.a - b.a };
                }

                /**
                 * @brief Multiplication composante par composante de deux couleurs flottantes
                 * @param a Première couleur
                 * @param b Seconde couleur
                 * @return Nouvelle couleur avec produit des composantes
                 * @note Utile pour des mélanges multiplicatifs ou des masques
                 */
                friend NKENTSEU_MATH_API_FORCE_INLINE
                NkColorF operator*(const NkColorF& a, const NkColorF& b) noexcept
                {
                    return { a.r * b.r, a.g * b.g, a.b * b.b, a.a * b.a };
                }

                /**
                 * @brief Multiplication d'une couleur par un scalaire
                 * @param c Couleur à multiplier
                 * @param s Facteur multiplicatif
                 * @return Nouvelle couleur avec composantes multipliées par s
                 * @note Utile pour ajuster la luminosité ou l'opacité globalement
                 */
                friend NKENTSEU_MATH_API_FORCE_INLINE
                NkColorF operator*(const NkColorF& c, float32 s) noexcept
                {
                    return { c.r * s, c.g * s, c.b * s, c.a * s };
                }

                /**
                 * @brief Multiplication d'un scalaire par une couleur (commutatif)
                 * @param s Facteur multiplicatif
                 * @param c Couleur à multiplier
                 * @return Même résultat que c * s
                 */
                friend NKENTSEU_MATH_API_FORCE_INLINE
                NkColorF operator*(float32 s, const NkColorF& c) noexcept
                {
                    return c * s;
                }

                /**
                 * @brief Division composante par composante de deux couleurs flottantes
                 * @param a Couleur dividende
                 * @param b Couleur diviseur
                 * @return Nouvelle couleur avec quotient des composantes
                 * @warning Risque de division par zéro si une composante de b est 0
                 */
                friend NKENTSEU_MATH_API_FORCE_INLINE
                NkColorF operator/(const NkColorF& a, const NkColorF& b) noexcept
                {
                    return { a.r / b.r, a.g / b.g, a.b / b.b, a.a / b.a };
                }

                /**
                 * @brief Division d'une couleur par un scalaire
                 * @param c Couleur à diviser
                 * @param s Diviseur
                 * @return Nouvelle couleur avec composantes divisées par s
                 * @warning Risque de division par zéro si s == 0
                 */
                friend NKENTSEU_MATH_API_FORCE_INLINE
                NkColorF operator/(const NkColorF& c, float32 s) noexcept
                {
                    return { c.r / s, c.g / s, c.b / s, c.a / s };
                }


                // ====================================================================
                // SECTION PUBLIQUE : OPÉRATEURS D'AFFECTATION COMPOSÉE
                // ====================================================================

                NKENTSEU_MATH_API_FORCE_INLINE NkColorF& operator+=(const NkColorF& o) noexcept { r+=o.r; g+=o.g; b+=o.b; a+=o.a; return *this; }
                NKENTSEU_MATH_API_FORCE_INLINE NkColorF& operator-=(const NkColorF& o) noexcept { r-=o.r; g-=o.g; b-=o.b; a-=o.a; return *this; }
                NKENTSEU_MATH_API_FORCE_INLINE NkColorF& operator*=(const NkColorF& o) noexcept { r*=o.r; g*=o.g; b*=o.b; a*=o.a; return *this; }
                NKENTSEU_MATH_API_FORCE_INLINE NkColorF& operator*=(float32 s) noexcept { r*=s; g*=s; b*=s; a*=s; return *this; }
                NKENTSEU_MATH_API_FORCE_INLINE NkColorF& operator/=(const NkColorF& o) noexcept { r/=o.r; g/=o.g; b/=o.b; a/=o.a; return *this; }
                NKENTSEU_MATH_API_FORCE_INLINE NkColorF& operator/=(float32 s) noexcept { r/=s; g/=s; b/=s; a/=s; return *this; }


                // ====================================================================
                // SECTION PUBLIQUE : COMPARAISON
                // ====================================================================

                /**
                 * @brief Compare l'égalité approximative de deux couleurs flottantes
                 * @param o Couleur à comparer
                 * @param epsilon Tolérance de comparaison (défaut: 1e-5)
                 * @return true si toutes les composantes diffèrent de <= epsilon
                 * @note Utilise NkFabs pour la comparaison flottante robuste
                 * @note NKENTSEU_MATH_API_FORCE_INLINE : candidate à l'inlining
                 */
                NKENTSEU_MATH_API_FORCE_INLINE
                bool IsEqual(const NkColorF& o, float32 epsilon = 1e-5f) const noexcept
                {
                    return NkFabs(r - o.r) <= epsilon &&
                           NkFabs(g - o.g) <= epsilon &&
                           NkFabs(b - o.b) <= epsilon &&
                           NkFabs(a - o.a) <= epsilon;
                }

                /**
                 * @brief Opérateur d'égalité approximative
                 * @param a Première couleur
                 * @param b Seconde couleur
                 * @return Résultat de a.IsEqual(b)
                 * @note Permet l'usage : if (color1 == color2)
                 */
                friend NKENTSEU_MATH_API_FORCE_INLINE
                bool operator==(const NkColorF& a, const NkColorF& b) noexcept
                {
                    return a.IsEqual(b);
                }

                /**
                 * @brief Opérateur d'inégalité approximative
                 * @param a Première couleur
                 * @param b Seconde couleur
                 * @return Résultat de !a.IsEqual(b)
                 */
                friend NKENTSEU_MATH_API_FORCE_INLINE
                bool operator!=(const NkColorF& a, const NkColorF& b) noexcept
                {
                    return !a.IsEqual(b);
                }


                // ====================================================================
                // SECTION PUBLIQUE : INTERPOLATION LINÉAIRE
                // ====================================================================

                /**
                 * @brief Interpole linéairement entre deux couleurs flottantes
                 * @param other Couleur cible de l'interpolation
                 * @param t Facteur d'interpolation ∈ [0, 1] (0=this, 1=other)
                 * @return Couleur interpolée
                 * @note Calcul direct en flottant : pas de perte de précision
                 * @note NKENTSEU_MATH_API_FORCE_INLINE : candidate à l'inlining
                 */
                NKENTSEU_MATH_API_FORCE_INLINE
                NkColorF Lerp(NkColorF other, float32 t) const noexcept
                {
                    return {
                        r + (other.r - r) * t,
                        g + (other.g - g) * t,
                        b + (other.b - b) * t,
                        a + (other.a - a) * t
                    };
                }


                // ====================================================================
                // SECTION PUBLIQUE : AJUSTEMENTS DE COULEUR
                // ====================================================================

                /**
                 * @brief Assombrit la couleur courante d'un facteur donné
                 * @param amount Facteur d'assombrissement ∈ [0, 1] (0=inchangé, 1=noir)
                 * @return Nouvelle couleur assombrie
                 * @note Multiplie chaque composante RGB par (1 - amount)
                 * @note L'alpha reste inchangé
                 * @note NKENTSEU_MATH_API_FORCE_INLINE : candidate à l'inlining
                 */
                NKENTSEU_MATH_API_FORCE_INLINE
                NkColorF Darken(float32 amount) const noexcept
                {
                    return { r * (1.0f - amount), g * (1.0f - amount), b * (1.0f - amount), a };
                }

                /**
                 * @brief Éclaircit la couleur courante d'un facteur donné
                 * @param amount Facteur d'éclaircissement ∈ [0, 1] (0=inchangé, 1=blanc)
                 * @return Nouvelle couleur éclaircie
                 * @note Ajoute amount * (1 - composante) à chaque composante RGB
                 * @note L'alpha reste inchangé
                 * @note NKENTSEU_MATH_API_FORCE_INLINE : candidate à l'inlining
                 */
                NKENTSEU_MATH_API_FORCE_INLINE
                NkColorF Lighten(float32 amount) const noexcept
                {
                    return {
                        NkMin(r + amount * (1.0f - r), 1.0f),
                        NkMin(g + amount * (1.0f - g), 1.0f),
                        NkMin(b + amount * (1.0f - b), 1.0f),
                        a
                    };
                }

                /**
                 * @brief Convertit la couleur en niveaux de gris (alpha=1)
                 * @return Nouvelle couleur en niveaux de gris opaque
                 * @note Utilise les coefficients luminance standards : 0.299R + 0.587G + 0.114B
                 * @note Alpha fixé à 1.0 (opaque)
                 * @note NKENTSEU_MATH_API_FORCE_INLINE : candidate à l'inlining
                 */
                NKENTSEU_MATH_API_FORCE_INLINE
                NkColorF ToGrayscale() const noexcept
                {
                    float32 v = 0.299f * r + 0.587f * g + 0.114f * b;
                    return { v, v, v, 1.0f };
                }

                /**
                 * @brief Convertit la couleur en niveaux de gris en préservant l'alpha
                 * @return Nouvelle couleur en niveaux de gris avec alpha original
                 * @note Utilise les coefficients luminance standards : 0.299R + 0.587G + 0.114B
                 * @note Alpha préservé de la couleur originale
                 * @note NKENTSEU_MATH_API_FORCE_INLINE : candidate à l'inlining
                 */
                NKENTSEU_MATH_API_FORCE_INLINE
                NkColorF ToGrayscaleWithAlpha() const noexcept
                {
                    float32 v = 0.299f * r + 0.587f * g + 0.114f * b;
                    return { v, v, v, a };
                }


                // ====================================================================
                // SECTION PUBLIQUE : MANIPULATION DE L'ALPHA
                // ====================================================================

                /**
                 * @brief Retourne une copie de la couleur avec un alpha personnalisé
                 * @param alpha Nouvelle valeur d'alpha ∈ [0, 1]
                 * @return Nouvelle couleur avec alpha modifié
                 * @note Les composantes RGB restent inchangées
                 * @note NKENTSEU_MATH_API_FORCE_INLINE : candidate à l'inlining
                 */
                NKENTSEU_MATH_API_FORCE_INLINE
                NkColorF WithAlpha(float32 alpha) const noexcept
                {
                    return { r, g, b, alpha };
                }


                // ====================================================================
                // SECTION PUBLIQUE : REPRÉSENTATION TEXTE (I/O)
                // ====================================================================

                /**
                 * @brief Convertit la couleur en chaîne de caractères lisible
                 * @return NkString au format "(r, g, b, a)" avec précision flottante
                 * @note Utilise NkFormat pour la mise en forme
                 * @note Non-inline : allocation de string, pas critique en performance
                 * @note Méthode const : ne modifie pas l'état de la couleur
                 */
                NKENTSEU_MATH_API
                NkString ToString() const;

                /**
                 * @brief Fonction libre pour conversion texte (ADL-friendly)
                 * @param c Couleur à convertir
                 * @return Même résultat que c.ToString()
                 * @note Permet l'usage : ToString(color) via Argument-Dependent Lookup
                 */
                friend NKENTSEU_MATH_API
                NkString ToString(const NkColorF& c);

                /**
                 * @brief Opérateur de flux pour sortie std::ostream
                 * @param os Flux de sortie (std::cout, fichier, etc.)
                 * @param c Couleur à écrire
                 * @return Référence vers os pour chaînage d'opérations
                 * @note Délègue à ToString() puis CStr() pour compatibilité C++ standard
                 * @note Utile pour logging/debug avec std::cout << color
                 */
                friend NKENTSEU_MATH_API
                std::ostream& operator<<(std::ostream& os, const NkColorF& c);

                /**
                 * @brief Convertit la couleur flottante courante en structure HSV
                 * @return Structure NkHSV équivalente
                 * @note Implémentation dans NkColor.cpp (algorithme standard RGB→HSV)
                 */
                NKENTSEU_MATH_API
                NkHSV ToHSVf() const noexcept;

            }; // struct NkColorF


            // ====================================================================
            // CLASSE : NKCOLOR (REPRÉSENTATION COULEUR RGBA 8 BITS)
            // ====================================================================

            /**
             * @brief Classe compacte pour la manipulation de couleurs RGBA sur 32 bits
             * 
             * Stocke une couleur dans un format mémoire efficace (4 octets) avec
             * support complet des opérations colorimétriques courantes.
             * 
             * PRINCIPE DE FONCTIONNEMENT :
             * - Stockage interne : uint8 r, g, b, a ∈ [0, 255]
             * - Construction multiple : depuis entiers, flottants [0,1], vecteurs, uint32, NkColorF
             * - Conversion HSV : FromHSV/ToHSV pour manipulations teinte/saturation/valeur
             * - Interpolation : Lerp linéaire avec gestion correcte des composantes entières
             * - Ajustements : Darken/Lighten/ToGrayscale pour transformations rapides
             * - Sérialisation : formats RGBA (ToUint32A) et ABGR (ToU32) pour différents usages
             * - Conversion NkColorF : conversion bidirectionnelle pour flexibilité
             * 
             * GARANTIES DE PERFORMANCE :
             * - Constructeurs/accesseurs : O(1), tous inline pour élimination d'appel
             * - Mémoire : sizeof(NkColor) == 4 bytes, optimal pour stockage et transfert
             * - Conversion flottante : multiplication par constante pré-calculée (1/255)
             * - Couleurs nommées : constantes statiques inline (ODR-safe C++17)
             * 
             * CAS D'USAGE TYPQUES :
             * - Interface utilisateur : couleurs de widgets, thèmes, feedback visuel
             * - Graphisme 2D/3D : matériaux, textures, lumières, post-processing
             * - Jeux vidéo : palettes de couleurs, effets visuels, UI dynamique
             * - Visualisation de données : cartographie de couleurs, heatmaps
             * - Design graphique : manipulation HSV pour outils de création
             * - Stockage/réseau : format compact 4 bytes pour persistance ou transmission
             * 
             * @note
             *   - Alpha par défaut = 255 (opaque) dans le constructeur par défaut
             *   - Conversions flottantes utilisent NkClamp pour sécurité [0,1]
             *   - Opérations bit-à-bit (AND/OR/XOR) disponibles via ToUint32A()
             *   - Couleurs nommées accessibles via NkColor::Red, NkColor::Blue, etc.
             *   - Conversion depuis/vers NkColorF pour flexibilité calcul/storage
             * 
             * @warning
             *   - Les opérations arithmétiques (+, -, *, /) ne sont pas définies directement
             *     (préférer Lerp ou opérations bit-à-bit si besoin)
             *   - La conversion HSV peut avoir des erreurs d'arrondi dues à la quantification 8-bit
             *   - FromName() utilise une recherche linéaire : O(n) mais n=60 est acceptable
             */
            class NkColor {

                // ====================================================================
                // SECTION PUBLIQUE : CONSTANTES ET TYPES ASSOCIÉS
                // ====================================================================
                public:

                    /** @brief Constante pour conversion flottante → entier : 1/255 */
                    static constexpr float32 kOneOver255 = 1.0f / 255.0f;


                // ====================================================================
                // SECTION PUBLIQUE : MEMBRES DONNÉES PUBLICS
                // ====================================================================
                public:

                    uint8 r; ///< Composante rouge [0, 255]
                    uint8 g; ///< Composante verte [0, 255]
                    uint8 b; ///< Composante bleue [0, 255]
                    uint8 a; ///< Composante alpha [0, 255] (0=transparent, 255=opaque)


                // ====================================================================
                // SECTION PUBLIQUE : CONSTRUCTEURS ET RÈGLE DE CINQ
                // ====================================================================
                public:

                    /**
                     * @brief Constructeur par défaut : noir opaque (0,0,0,255)
                     * @note Alpha initialisé à 255 (opaque) par convention
                     * @note constexpr : évaluable à la compilation pour initialisation statique
                     */
                    constexpr NkColor() noexcept
                        : r(0), g(0), b(0), a(255)
                    {
                    }

                    /**
                     * @brief Constructeur depuis composantes entières RGBA
                     * @param r Composante rouge [0, 255]
                     * @param g Composante verte [0, 255]
                     * @param b Composante bleue [0, 255]
                     * @param a Composante alpha [0, 255] (défaut: 255 = opaque)
                     * @note Pas de validation/clamping : responsabilité du caller
                     * @note constexpr : évaluable à la compilation
                     */
                    constexpr NkColor(uint8 r, uint8 g, uint8 b, uint8 a = 255) noexcept
                        : r(r), g(g), b(b), a(a)
                    {
                    }

                    /**
                     * @brief Constructeur depuis uint32 au format RGBA big-endian
                     * @param rgba Valeur 32-bit au format 0xRRGGBBAA (comme codes HTML)
                     * @note Extraction des composantes via décalages de bits
                     * @note Big-endian : RR dans les bits 31-24, AA dans les bits 7-0
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    NkColor(uint32 rgba) noexcept
                        : r(static_cast<uint8>((rgba >> 24) & 0xFF))
                        , g(static_cast<uint8>((rgba >> 16) & 0xFF))
                        , b(static_cast<uint8>((rgba >>  8) & 0xFF))
                        , a(static_cast<uint8>( rgba         & 0xFF))
                    {
                    }

                    /**
                     * @brief Constructeur depuis vecteur flottant NkVector4f
                     * @param v Vecteur avec composantes [r, g, b, a] ∈ [0, 1]
                     * @note Convertit chaque composante flottante en entier via *255
                     * @note Applique NkClamp pour garantir [0, 1] avant conversion
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    NkColor(const NkVector4f& v) noexcept
                        : r(static_cast<uint8>(NkClamp(v.r, 0.0f, 1.0f) * 255.0f))
                        , g(static_cast<uint8>(NkClamp(v.g, 0.0f, 1.0f) * 255.0f))
                        , b(static_cast<uint8>(NkClamp(v.b, 0.0f, 1.0f) * 255.0f))
                        , a(static_cast<uint8>(NkClamp(v.a, 0.0f, 1.0f) * 255.0f))
                    {
                    }

                    /**
                     * @brief Constructeur depuis vecteur flottant NkVector3f (alpha=255)
                     * @param v Vecteur avec composantes [r, g, b] ∈ [0, 1]
                     * @note Alpha fixé à 255 (opaque) par défaut
                     * @note Convertit chaque composante flottante en entier via *255
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    NkColor(const NkVector3f& v) noexcept
                        : r(static_cast<uint8>(NkClamp(v.r, 0.0f, 1.0f) * 255.0f))
                        , g(static_cast<uint8>(NkClamp(v.g, 0.0f, 1.0f) * 255.0f))
                        , b(static_cast<uint8>(NkClamp(v.b, 0.0f, 1.0f) * 255.0f))
                        , a(255)
                    {
                    }

                    /**
                     * @brief Constructeur depuis NkColorF (conversion float → 8-bit)
                     * @param color Couleur flottante à convertir
                     * @note Conversion : uint8 = clamp(float, 0, 1) * 255
                     * @note NKENTSEU_MATH_API_FORCE_INLINE : candidate à l'inlining
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    explicit NkColor(const NkColorF& color) noexcept;

                    /**
                     * @brief Constructeur de copie par défaut
                     * @note Copie bit-à-bit des 4 octets : opération triviale
                     * @note noexcept : copie de type triviallement copiable
                     */
                    NkColor(const NkColor&) noexcept = default;

                    /**
                     * @brief Opérateur d'affectation par copie par défaut
                     * @return Référence vers *this pour chaînage
                     * @note Copie bit-à-bit des 4 octets : opération triviale
                     * @note noexcept : affectation de type triviallement copiable
                     */
                    NkColor& operator=(const NkColor&) noexcept = default;


                // ====================================================================
                // SECTION PUBLIQUE : CONVERSIONS FLOTTANTES
                // ====================================================================
                public:

                    /**
                     * @brief Convertit une composante entière [0,255] en flottant [0,1]
                     * @param v Valeur entière à convertir
                     * @return Valeur flottante équivalente dans [0, 1]
                     * @note Utilise la constante kOneOver255 pour performance
                     * @note NKENTSEU_MATH_API_FORCE_INLINE : candidate à l'inlining agressif
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    static float32 ToFloat(uint8 v) noexcept
                    {
                        return static_cast<float32>(v) * kOneOver255;
                    }

                    /**
                     * @brief Convertit la couleur en NkColorF (8-bit → float)
                     * @return Nouvelle couleur NkColorF équivalente
                     * @note Conversion : float = uint8 * (1/255)
                     * @note NKENTSEU_MATH_API_FORCE_INLINE : candidate à l'inlining
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    NkColorF ToColorF() const noexcept;


                // ====================================================================
                // SECTION PUBLIQUE : OPÉRATEURS DE CONVERSION
                // ====================================================================
                public:

                    /**
                     * @brief Conversion explicite vers NkVector4f (RGBA flottant)
                     * @return Vecteur avec composantes [r, g, b, a] ∈ [0, 1]
                     * @note Utilise ToFloat() pour chaque composante
                     * @note explicit : évite les conversions accidentelles
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    explicit operator NkVector4f() const noexcept
                    {
                        return { r * kOneOver255, g * kOneOver255, b * kOneOver255, a * kOneOver255 };
                    }

                    /**
                     * @brief Conversion explicite vers NkVector3f (RGB flottant, alpha ignoré)
                     * @return Vecteur avec composantes [r, g, b] ∈ [0, 1]
                     * @note Utilise ToFloat() pour les composantes RGB
                     * @note explicit : évite les conversions accidentelles
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    explicit operator NkVector3f() const noexcept
                    {
                        return { r * kOneOver255, g * kOneOver255, b * kOneOver255 };
                    }

                    /**
                     * @brief Conversion explicite vers uint32 au format RGBA
                     * @return Valeur 32-bit au format 0xRRGGBBAA
                     * @note Délègue à ToUint32A() pour cohérence
                     * @note explicit : évite les conversions accidentelles
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    explicit operator uint32() const noexcept
                    {
                        return ToUint32A();
                    }

                    /**
                     * @brief Conversion implicite depuis NkColorF
                     * @param color Couleur NkColorF à convertir
                     * @return Nouvelle couleur NkColor équivalente
                     * @note Permet : NkColor c = someNkColorF;
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    friend NkColor FromColorF(const NkColorF& color) noexcept;


                // ====================================================================
                // SECTION PUBLIQUE : SÉRIALISATION BINAIRE
                // ====================================================================
                public:

                    /**
                     * @brief Convertit la couleur en uint32 au format RGBA (big-endian)
                     * @return Valeur 32-bit au format 0xRRGGBBAA
                     * @note Format standard pour les codes couleur HTML/CSS (#RRGGBBAA)
                     * @note NKENTSEU_MATH_API_FORCE_INLINE : candidate à l'inlining
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    uint32 ToUint32A() const noexcept
                    {
                        return (static_cast<uint32>(r) << 24)
                             | (static_cast<uint32>(g) << 16)
                             | (static_cast<uint32>(b) <<  8)
                             |  static_cast<uint32>(a);
                    }

                    /**
                     * @brief Convertit la couleur en uint32 au format RGBA sans alpha (alpha=255)
                     * @return Valeur 32-bit au format 0xRRGGBBFF
                     * @note Utile quand l'alpha n'est pas pertinent pour l'usage
                     * @note NKENTSEU_MATH_API_FORCE_INLINE : candidate à l'inlining
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    uint32 ToUint32() const noexcept
                    {
                        return (static_cast<uint32>(r) << 24)
                             | (static_cast<uint32>(g) << 16)
                             | (static_cast<uint32>(b) <<  8)
                             | 255u;
                    }

                    /**
                     * @brief Convertit la couleur en uint32 au format ABGR (little-endian)
                     * @return Valeur 32-bit au format 0xAABBGGRR
                     * @note Format utilisé par certains systèmes de rendu (ex: ImGui DrawList)
                     * @note NKENTSEU_MATH_API_FORCE_INLINE : candidate à l'inlining
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    uint32 ToU32() const noexcept
                    {
                        return (static_cast<uint32>(a) << 24)
                             | (static_cast<uint32>(b) << 16)
                             | (static_cast<uint32>(g) <<  8)
                             |  static_cast<uint32>(r);
                    }


                // ====================================================================
                // SECTION PUBLIQUE : CONVERSIONS HSV
                // ====================================================================
                public:

                    /**
                     * @brief Crée une couleur depuis une structure HSV
                     * @param hsv Structure NkHSV avec teinte/saturation/valeur
                     * @return Nouvelle couleur NkColor équivalente en RGB
                     * @note Implémentation dans NkColor.cpp (algorithme standard HSV→RGB)
                     * @note Gère correctement les cas limites (saturation=0, valeur=0)
                     */
                    NKENTSEU_MATH_API
                    static NkColor FromHSV(const NkHSV& hsv) noexcept;

                    /**
                     * @brief Crée une couleur flottante depuis une structure HSV
                     * @param hsv Structure NkHSV avec teinte/saturation/valeur
                     * @return Nouvelle couleur NkColorF équivalente en RGB flottant
                     * @note Implémentation dans NkColor.cpp (algorithme standard HSV→RGB)
                     * @note Plus précis que FromHSV() car pas de quantification 8-bit intermédiaire
                     */
                    NKENTSEU_MATH_API
                    static NkColorF FromHSVf(const NkHSV& hsv) noexcept;

                    /**
                     * @brief Convertit la couleur courante en structure HSV
                     * @return Structure NkHSV équivalente
                     * @note Implémentation dans NkColor.cpp (algorithme standard RGB→HSV)
                     * @note Précision limitée par la quantification 8-bit des composantes RGB
                     */
                    NKENTSEU_MATH_API
                    NkHSV ToHSV() const noexcept;

                    /**
                     * @brief Convertit la couleur flottante courante en structure HSV
                     * @return Structure NkHSV équivalente
                     * @note Implémentation dans NkColor.cpp (algorithme standard RGB→HSV)
                     * @note Plus précis que ToHSV() car pas de quantification 8-bit
                     */
                    NKENTSEU_MATH_API
                    NkHSV ToHSVf() const noexcept;

                    /**
                     * @brief Modifie la teinte de la couleur courante
                     * @param h Nouvelle teinte en degrés [0, 360)
                     * @note Convertit en HSV, modifie la teinte, puis reconvertit en RGB
                     * @note Clampe automatiquement h dans [0, 360)
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    void SetHue(float32 h) noexcept
                    {
                        NkHSV hsv = ToHSV();
                        hsv.hue = NkClamp(h, 0.0f, 360.0f);
                        *this = FromHSV(hsv);
                    }

                    /**
                     * @brief Modifie la saturation de la couleur courante
                     * @param s Nouvelle saturation en pourcentage [0, 100]
                     * @note Convertit en HSV, modifie la saturation, puis reconvertit en RGB
                     * @note Clampe automatiquement s dans [0, 100]
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    void SetSaturation(float32 s) noexcept
                    {
                        NkHSV hsv = ToHSV();
                        hsv.saturation = NkClamp(s, 0.0f, 100.0f);
                        *this = FromHSV(hsv);
                    }

                    /**
                     * @brief Modifie la valeur (luminosité) de la couleur courante
                     * @param v Nouvelle valeur en pourcentage [0, 100]
                     * @note Convertit en HSV, modifie la valeur, puis reconvertit en RGB
                     * @note Clampe automatiquement v dans [0, 100]
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    void SetValue(float32 v) noexcept
                    {
                        NkHSV hsv = ToHSV();
                        hsv.value = NkClamp(v, 0.0f, 100.0f);
                        *this = FromHSV(hsv);
                    }

                    /**
                     * @brief Augmente la teinte de la couleur courante
                     * @param d Delta de teinte à ajouter (peut être négatif)
                     * @note Équivalent à SetHue(hue + d) avec clamping
                     * @note Utile pour créer des variations chromatiques
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    void IncreaseHue(float32 d) noexcept
                    {
                        NkHSV h = ToHSV();
                        h.hue = NkClamp(h.hue + d, 0.0f, 360.0f);
                        *this = FromHSV(h);
                    }

                    /**
                     * @brief Diminue la teinte de la couleur courante
                     * @param d Delta de teinte à soustraire
                     * @note Équivalent à IncreaseHue(-d)
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    void DecreaseHue(float32 d) noexcept
                    {
                        IncreaseHue(-d);
                    }

                    /**
                     * @brief Augmente la saturation de la couleur courante
                     * @param d Delta de saturation à ajouter
                     * @note Équivalent à SetSaturation(saturation + d) avec clamping
                     * @note Utile pour intensifier ou désaturer une couleur
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    void IncreaseSaturation(float32 d) noexcept
                    {
                        NkHSV h = ToHSV();
                        h.saturation = NkClamp(h.saturation + d, 0.0f, 100.0f);
                        *this = FromHSV(h);
                    }

                    /**
                     * @brief Diminue la saturation de la couleur courante
                     * @param d Delta de saturation à soustraire
                     * @note Équivalent à IncreaseSaturation(-d)
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    void DecreaseSaturation(float32 d) noexcept
                    {
                        IncreaseSaturation(-d);
                    }

                    /**
                     * @brief Augmente la valeur (luminosité) de la couleur courante
                     * @param d Delta de valeur à ajouter
                     * @note Équivalent à SetValue(value + d) avec clamping
                     * @note Utile pour éclaircir ou assombrir une couleur
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    void IncreaseValue(float32 d) noexcept
                    {
                        NkHSV h = ToHSV();
                        h.value = NkClamp(h.value + d, 0.0f, 100.0f);
                        *this = FromHSV(h);
                    }

                    /**
                     * @brief Diminue la valeur (luminosité) de la couleur courante
                     * @param d Delta de valeur à soustraire
                     * @note Équivalent à IncreaseValue(-d)
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    void DecreaseValue(float32 d) noexcept
                    {
                        IncreaseValue(-d);
                    }


                // ====================================================================
                // SECTION PUBLIQUE : AJUSTEMENTS DE COULEUR
                // ====================================================================
                public:

                    /**
                     * @brief Assombrit la couleur courante d'un facteur donné
                     * @param amount Facteur d'assombrissement ∈ [0, 1] (0=inchangé, 1=noir)
                     * @return Nouvelle couleur assombrie
                     * @note Multiplie chaque composante RGB par (1 - amount)
                     * @note L'alpha reste inchangé
                     * @note NKENTSEU_MATH_API_FORCE_INLINE : candidate à l'inlining
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    NkColor Darken(float32 amount) const noexcept
                    {
                        return {
                            static_cast<uint8>(static_cast<float32>(r) * (1.0f - amount)),
                            static_cast<uint8>(static_cast<float32>(g) * (1.0f - amount)),
                            static_cast<uint8>(static_cast<float32>(b) * (1.0f - amount)),
                            a
                        };
                    }

                    /**
                     * @brief Éclaircit la couleur courante d'un facteur donné
                     * @param amount Facteur d'éclaircissement ∈ [0, 1] (0=inchangé, 1=blanc)
                     * @return Nouvelle couleur éclaircie
                     * @note Ajoute amount * (255 - composante) à chaque composante RGB
                     * @note L'alpha reste inchangé
                     * @note NKENTSEU_MATH_API_FORCE_INLINE : candidate à l'inlining
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    NkColor Lighten(float32 amount) const noexcept
                    {
                        return {
                            static_cast<uint8>(NkMin(static_cast<float32>(r) + amount * (255.0f - static_cast<float32>(r)), 255.0f)),
                            static_cast<uint8>(NkMin(static_cast<float32>(g) + amount * (255.0f - static_cast<float32>(g)), 255.0f)),
                            static_cast<uint8>(NkMin(static_cast<float32>(b) + amount * (255.0f - static_cast<float32>(b)), 255.0f)),
                            a
                        };
                    }

                    /**
                     * @brief Convertit la couleur en niveaux de gris (alpha=255)
                     * @return Nouvelle couleur en niveaux de gris opaque
                     * @note Utilise les coefficients luminance standards : 0.299R + 0.587G + 0.114B
                     * @note Alpha fixé à 255 (opaque)
                     * @note NKENTSEU_MATH_API_FORCE_INLINE : candidate à l'inlining
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    NkColor ToGrayscale() const noexcept
                    {
                        uint8 v = static_cast<uint8>(0.299f * r + 0.587f * g + 0.114f * b);
                        return { v, v, v, 255 };
                    }

                    /**
                     * @brief Convertit la couleur en niveaux de gris en préservant l'alpha
                     * @return Nouvelle couleur en niveaux de gris avec alpha original
                     * @note Utilise les coefficients luminance standards : 0.299R + 0.587G + 0.114B
                     * @note Alpha préservé de la couleur originale
                     * @note NKENTSEU_MATH_API_FORCE_INLINE : candidate à l'inlining
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    NkColor ToGrayscaleWithAlpha() const noexcept
                    {
                        uint8 v = static_cast<uint8>(0.299f * r + 0.587f * g + 0.114f * b);
                        return { v, v, v, a };
                    }


                // ====================================================================
                // SECTION PUBLIQUE : INTERPOLATION LINÉAIRE
                // ====================================================================
                public:

                    /**
                     * @brief Interpole linéairement entre deux couleurs
                     * @param other Couleur cible de l'interpolation
                     * @param t Facteur d'interpolation ∈ [0, 1] (0=this, 1=other)
                     * @return Couleur interpolée
                     * @note Conversion en flottant, interpolation, puis reconversion en entier
                     * @note Gère correctement les composantes entières avec arrondi implicite
                     * @note NKENTSEU_MATH_API_FORCE_INLINE : candidate à l'inlining
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    NkColor Lerp(NkColor other, float32 t) const noexcept
                    {
                        return {
                            static_cast<uint8>(static_cast<float32>(r) + (static_cast<float32>(other.r) - static_cast<float32>(r)) * t),
                            static_cast<uint8>(static_cast<float32>(g) + (static_cast<float32>(other.g) - static_cast<float32>(g)) * t),
                            static_cast<uint8>(static_cast<float32>(b) + (static_cast<float32>(other.b) - static_cast<float32>(b)) * t),
                            static_cast<uint8>(static_cast<float32>(a) + (static_cast<float32>(other.a) - static_cast<float32>(a)) * t)
                        };
                    }


                // ====================================================================
                // SECTION PUBLIQUE : MANIPULATION DE L'ALPHA
                // ====================================================================
                public:

                    /**
                     * @brief Retourne une copie de la couleur avec un alpha personnalisé
                     * @param alpha Nouvelle valeur d'alpha ∈ [0, 255]
                     * @return Nouvelle couleur avec alpha modifié
                     * @note Les composantes RGB restent inchangées
                     * @note NKENTSEU_MATH_API_FORCE_INLINE : candidate à l'inlining
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    NkColor WithAlpha(int32 alpha) const noexcept
                    {
                        return { r, g, b, static_cast<uint8>(alpha) };
                    }


                // ====================================================================
                // SECTION PUBLIQUE : OPÉRATEURS BIT-À-BIT
                // ====================================================================
                public:

                    /**
                     * @brief Opérateur AND bit-à-bit entre deux couleurs
                     * @param o Couleur opérande
                     * @return Nouvelle couleur avec AND bit-à-bit des composantes
                     * @note Convertit d'abord en uint32 RGBA, applique AND, puis reconstruit
                     * @note Utile pour des masques de couleur ou des filtres binaires
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    NkColor operator&(const NkColor& o) const noexcept
                    {
                        return NkColor(ToUint32A() & o.ToUint32A());
                    }

                    /**
                     * @brief Opérateur OR bit-à-bit entre deux couleurs
                     * @param o Couleur opérande
                     * @return Nouvelle couleur avec OR bit-à-bit des composantes
                     * @note Convertit d'abord en uint32 RGBA, applique OR, puis reconstruit
                     * @note Utile pour des combinaisons de masques de couleur
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    NkColor operator|(const NkColor& o) const noexcept
                    {
                        return NkColor(ToUint32A() | o.ToUint32A());
                    }

                    /**
                     * @brief Opérateur XOR bit-à-bit entre deux couleurs
                     * @param o Couleur opérande
                     * @return Nouvelle couleur avec XOR bit-à-bit des composantes
                     * @note Convertit d'abord en uint32 RGBA, applique XOR, puis reconstruit
                     * @note Utile pour des effets de toggle ou des comparaisons binaires
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    NkColor operator^(const NkColor& o) const noexcept
                    {
                        return NkColor(ToUint32A() ^ o.ToUint32A());
                    }


                // ====================================================================
                // SECTION PUBLIQUE : COMPARAISON
                // ====================================================================
                public:

                    /**
                     * @brief Compare l'égalité exacte de deux couleurs
                     * @param o Couleur à comparer
                     * @return true si toutes les composantes (r,g,b,a) sont identiques
                     * @note Comparaison bit-à-bit exacte, pas de tolérance flottante
                     * @note NKENTSEU_MATH_API_FORCE_INLINE : candidate à l'inlining
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    bool IsEqual(const NkColor& o) const noexcept
                    {
                        return r == o.r && g == o.g && b == o.b && a == o.a;
                    }

                    /**
                     * @brief Opérateur d'égalité pour std::map/std::set
                     * @param a Première couleur
                     * @param b Seconde couleur
                     * @return Résultat de IsEqual(a, b)
                     * @note Permet l'utilisation de NkColor comme clé dans les conteneurs associatifs
                     */
                    friend NKENTSEU_MATH_API_FORCE_INLINE
                    bool operator==(const NkColor& a, const NkColor& b) noexcept
                    {
                        return a.IsEqual(b);
                    }

                    /**
                     * @brief Opérateur d'inégalité pour std::map/std::set
                     * @param a Première couleur
                     * @param b Seconde couleur
                     * @return Résultat de !IsEqual(a, b)
                     */
                    friend NKENTSEU_MATH_API_FORCE_INLINE
                    bool operator!=(const NkColor& a, const NkColor& b) noexcept
                    {
                        return !a.IsEqual(b);
                    }


                // ====================================================================
                // SECTION PUBLIQUE : CONSTRUCTEURS FLOTTANTS (FACTORY METHODS)
                // ====================================================================
                public:

                    /**
                     * @brief Crée une couleur opaque depuis composantes flottantes RGB
                     * @param r Composante rouge ∈ [0, 1]
                     * @param g Composante verte ∈ [0, 1]
                     * @param b Composante bleue ∈ [0, 1]
                     * @return Nouvelle couleur opaque (alpha=255)
                     * @note Applique NkClamp pour garantir [0, 1] avant conversion
                     * @note NKENTSEU_MATH_API_FORCE_INLINE : candidate à l'inlining
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    static NkColor RGBf(float32 r, float32 g, float32 b) noexcept
                    {
                        return {
                            static_cast<uint8>(NkClamp(r, 0.0f, 1.0f) * 255.0f),
                            static_cast<uint8>(NkClamp(g, 0.0f, 1.0f) * 255.0f),
                            static_cast<uint8>(NkClamp(b, 0.0f, 1.0f) * 255.0f),
                            255
                        };
                    }

                    /**
                     * @brief Crée une couleur depuis composantes flottantes RGBA
                     * @param r Composante rouge ∈ [0, 1]
                     * @param g Composante verte ∈ [0, 1]
                     * @param b Composante bleue ∈ [0, 1]
                     * @param a Composante alpha ∈ [0, 1]
                     * @return Nouvelle couleur avec alpha personnalisé
                     * @note Applique NkClamp pour garantir [0, 1] avant conversion
                     * @note NKENTSEU_MATH_API_FORCE_INLINE : candidate à l'inlining
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    static NkColor RGBAf(float32 r, float32 g, float32 b, float32 a) noexcept
                    {
                        return {
                            static_cast<uint8>(NkClamp(r, 0.0f, 1.0f) * 255.0f),
                            static_cast<uint8>(NkClamp(g, 0.0f, 1.0f) * 255.0f),
                            static_cast<uint8>(NkClamp(b, 0.0f, 1.0f) * 255.0f),
                            static_cast<uint8>(NkClamp(a, 0.0f, 1.0f) * 255.0f)
                        };
                    }


                // ====================================================================
                // SECTION PUBLIQUE : COULEURS ALÉATOIRES
                // ====================================================================
                public:

                    /**
                     * @brief Génère une couleur RGB aléatoire opaque
                     * @return Nouvelle couleur avec r,g,b aléatoires ∈ [0,255], a=255
                     * @note Utilise NkRandom::Instance() pour la génération
                     * @note Implémentation dans NkColor.cpp
                     */
                    NKENTSEU_MATH_API
                    static NkColor RandomRGB() noexcept;

                    /**
                     * @brief Génère une couleur RGBA aléatoire
                     * @return Nouvelle couleur avec r,g,b,a aléatoires ∈ [0,255]
                     * @note Utilise NkRandom::Instance() pour la génération
                     * @note Implémentation dans NkColor.cpp
                     */
                    NKENTSEU_MATH_API
                    static NkColor RandomRGBA() noexcept;


                // ====================================================================
                // SECTION PUBLIQUE : LOOKUP PAR NOM
                // ====================================================================
                public:

                    /**
                     * @brief Trouve une couleur par son nom textuel
                     * @param name Nom de la couleur (ex: "Red", "SkyBlue", "Transparent")
                     * @return Référence const vers la couleur correspondante
                     * @note Recherche linéaire dans ~60 entrées : O(n) mais n petit
                     * @note Retourne Black si le nom n'est pas trouvé
                     * @note Implémentation dans NkColor.cpp
                     */
                    NKENTSEU_MATH_API
                    static const NkColor& FromName(const NkString& name) noexcept;


                // ====================================================================
                // SECTION PUBLIQUE : REPRÉSENTATION TEXTE (I/O)
                // ====================================================================
                public:

                    /**
                     * @brief Convertit la couleur en chaîne de caractères lisible
                     * @return NkString au format "(r, g, b, a)" (ex: "(255, 0, 0, 255)")
                     * @note Utilise NkFormat pour la mise en forme
                     * @note Non-inline : allocation de string, pas critique en performance
                     * @note Méthode const : ne modifie pas l'état de la couleur
                     */
                    NKENTSEU_MATH_API
                    NkString ToString() const;

                    /**
                     * @brief Fonction libre pour conversion texte (ADL-friendly)
                     * @param c Couleur à convertir
                     * @return Même résultat que c.ToString()
                     * @note Permet l'usage : ToString(color) via Argument-Dependent Lookup
                     */
                    friend NKENTSEU_MATH_API
                    NkString ToString(const NkColor& c);

                    /**
                     * @brief Opérateur de flux pour sortie std::ostream
                     * @param os Flux de sortie (std::cout, fichier, etc.)
                     * @param c Couleur à écrire
                     * @return Référence vers os pour chaînage d'opérations
                     * @note Délègue à ToString() puis CStr() pour compatibilité C++ standard
                     * @note Utile pour logging/debug avec std::cout << color
                     */
                    friend NKENTSEU_MATH_API
                    std::ostream& operator<<(std::ostream& os, const NkColor& c);


                // ====================================================================
                // SECTION PUBLIQUE : FACTORIES DE BASE
                // ====================================================================
                public:

                    /**
                     * @brief Crée une couleur grise avec valeur et alpha personnalisés
                     * @param v Valeur de gris ∈ [0, 255] (0=noir, 255=blanc)
                     * @param alpha Valeur alpha ∈ [0, 255] (défaut: 255 = opaque)
                     * @return Nouvelle couleur grise
                     * @note NKENTSEU_MATH_API_FORCE_INLINE : candidate à l'inlining
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    static NkColor GrayValue(int32 v, int32 alpha = 255) noexcept
                    {
                        return { static_cast<uint8>(v), static_cast<uint8>(v), static_cast<uint8>(v), static_cast<uint8>(alpha) };
                    }


                // ====================================================================
                // SECTION PUBLIQUE : COULEURS NOMMÉES (CONSTANTES STATIQUES)
                // ====================================================================
                public:

                    /**
                     * @defgroup NamedColors Couleurs nommées prédéfinies
                     * @brief Bibliothèque étendue de couleurs constantes accessibles globalement
                     * @ingroup ColorConstants
                     *
                     * Ces constantes statiques sont définies dans NkColor.cpp via les
                     * factory methods RGBf/RGBAf ou constructeurs directs. Elles sont
                     * ODR-safe depuis C++17 grâce au mot-clé inline implicite pour
                     * les variables static dans les classes.
                     *
                     * @usage
                     * @code
                     * // Utilisation directe des couleurs nommées
                     * NkColor textColor = NkColor::White;
                     * NkColor backgroundColor = NkColor::SkyBlue;
                     * NkColor errorColor = NkColor::Red;
                     *
                     * // Lookup dynamique par nom
                     * NkColor dynamicColor = NkColor::FromName("Green");
                     *
                     * // Conversion vers NkColorF pour calculs
                     * NkColorF textColorF = NkColor::White.ToColorF();
                     * @endcode
                     */
                    ///@{

                    static const NkColor White;              ///< Blanc opaque (255,255,255,255)
                    static const NkColor Black;              ///< Noir opaque (0,0,0,255)
                    static const NkColor Transparent;        ///< Transparent (0,0,0,0)
                    static const NkColor Gray;               ///< Gris moyen (128,128,128,255)
                    static const NkColor Red;                ///< Rouge pur (255,0,0,255)
                    static const NkColor Green;              ///< Vert pur (0,255,0,255)
                    static const NkColor Blue;               ///< Bleu pur (0,0,255,255)
                    static const NkColor Yellow;             ///< Jaune (255,255,0,255)
                    static const NkColor Cyan;               ///< Cyan (0,255,255,255)
                    static const NkColor Magenta;            ///< Magenta (255,0,255,255)
                    static const NkColor Orange;             ///< Orange (255,128,0,255)
                    static const NkColor Pink;               ///< Rose (255,191,204,255)
                    static const NkColor Purple;             ///< Violet (128,0,128,255)
                    static const NkColor DarkGray;           ///< Gris foncé (26,26,26,255)
                    static const NkColor Lime;               ///< Citron vert (191,255,0,255)
                    static const NkColor Teal;               ///< Sarcelle (0,128,128,255)
                    static const NkColor Brown;              ///< Marron (166,41,41,255)
                    static const NkColor SaddleBrown;        ///< Brun selle (140,69,19,255)
                    static const NkColor Olive;              ///< Olive (128,128,0,255)
                    static const NkColor Maroon;             ///< Bordeaux (128,0,0,255)
                    static const NkColor Navy;               ///< Bleu marine (0,0,128,255)
                    static const NkColor Indigo;             ///< Indigo (74,0,130,255)
                    static const NkColor Turquoise;          ///< Turquoise (64,224,208,255)
                    static const NkColor Silver;             ///< Argent (192,192,192,255)
                    static const NkColor Gold;               ///< Or (255,215,0,255)
                    static const NkColor SkyBlue;            ///< Bleu ciel (135,206,250,255)
                    static const NkColor ForestGreen;        ///< Vert forêt (34,139,34,255)
                    static const NkColor SteelBlue;          ///< Bleu acier (69,130,181,255)
                    static const NkColor DarkSlateGray;      ///< Ardoise foncée (47,79,79,255)
                    static const NkColor Chocolate;          ///< Chocolat (210,105,30,255)
                    static const NkColor HotPink;            ///< Rose chaud (255,105,180,255)
                    static const NkColor SlateBlue;          ///< Bleu ardoise (106,90,205,255)
                    static const NkColor RoyalBlue;          ///< Bleu royal (65,105,225,255)
                    static const NkColor Tomato;             ///< Tomate (255,99,71,255)
                    static const NkColor MediumSeaGreen;     ///< Vert mer moyen (60,179,113,255)
                    static const NkColor DarkOrange;         ///< Orange foncé (255,140,0,255)
                    static const NkColor MediumPurple;       ///< Violet moyen (147,112,219,255)
                    static const NkColor CornflowerBlue;     ///< Bleu bleuet (100,149,237,255)
                    static const NkColor DarkGoldenrod;      ///< Or foncé (184,134,11,255)
                    static const NkColor DodgerBlue;         ///< Bleu dodger (30,144,255,255)
                    static const NkColor MediumVioletRed;    ///< Rouge violet moyen (199,21,133,255)
                    static const NkColor Peru;               ///< Pérou (205,133,63,255)
                    static const NkColor MediumAquamarine;   ///< Aquamarin moyen (102,205,170,255)
                    static const NkColor DarkTurquoise;      ///< Turquoise foncé (0,206,209,255)
                    static const NkColor MediumSlateBlue;    ///< Bleu ardoise moyen (123,104,238,255)
                    static const NkColor YellowGreen;        ///< Vert jaune (154,205,50,255)
                    static const NkColor LightCoral;         ///< Corail clair (240,128,128,255)
                    static const NkColor DarkSlateBlue;      ///< Bleu ardoise foncé (72,61,139,255)
                    static const NkColor DarkOliveGreen;     ///< Vert olive foncé (85,107,47,255)
                    static const NkColor Firebrick;          ///< Brique (178,34,34,255)
                    static const NkColor MediumOrchid;       ///< Orchidée moyenne (186,85,211,255)
                    static const NkColor RosyBrown;          ///< Brun rosé (188,143,143,255)
                    static const NkColor DarkCyan;           ///< Cyan foncé (0,139,139,255)
                    static const NkColor CadetBlue;          ///< Bleu cadet (95,158,160,255)
                    static const NkColor PaleVioletRed;      ///< Rouge violet pâle (219,112,147,255)
                    static const NkColor DeepPink;           ///< Rose profond (255,20,147,255)
                    static const NkColor LawnGreen;          ///< Vert gazon (124,252,0,255)
                    static const NkColor MediumSpringGreen;  ///< Vert printemps moyen (0,250,154,255)
                    static const NkColor MediumTurquoise;    ///< Turquoise moyen (72,209,204,255)
                    static const NkColor PaleGreen;          ///< Vert pâle (152,251,152,255)
                    static const NkColor DarkKhaki;          ///< Kaki foncé (189,183,107,255)
                    static const NkColor MediumBlue;         ///< Bleu moyen (0,0,205,255)
                    static const NkColor MidnightBlue;       ///< Bleu minuit (25,25,112,255)
                    static const NkColor NavajoWhite;        ///< Blanc navajo (255,222,173,255)
                    static const NkColor DarkSalmon;         ///< Saumon foncé (233,150,122,255)
                    static const NkColor MediumCoral;        ///< Corail moyen (205,91,91,255)
                    static const NkColor DefaultBackground;  ///< Arrière-plan par défaut (0,162,232,255)
                    static const NkColor CharcoalBlack;      ///< Noir charbon (31,31,31,255)
                    static const NkColor SlateGray;          ///< Gris ardoise (46,46,46,255)
                    static const NkColor SkyBlueRef;         ///< Référence bleu ciel (50,130,246,255)
                    static const NkColor DuckBlue;           ///< Bleu canard (0,162,232,255)

                    ///@}

            }; // class NkColor


            // ====================================================================
            // IMPLEMENTATIONS INLINE DES CONVERSIONS NKCOLOR ↔ NKCOLORF
            // ====================================================================
            // Note : Ces implémentations doivent être inline dans le header
            // car elles dépendent des deux classes.

            /**
             * @brief Constructeur NkColorF depuis NkColor (implémentation inline)
             * @param color Couleur NkColor à convertir
             */
            NKENTSEU_MATH_API_FORCE_INLINE
            NkColorF::NkColorF(const NkColor& color) noexcept
                : r(color.r * kOneOver255)
                , g(color.g * kOneOver255)
                , b(color.b * kOneOver255)
                , a(color.a * kOneOver255)
            {
            }

            /**
             * @brief Conversion NkColorF vers NkColor (implémentation inline)
             * @return Nouvelle couleur NkColor avec composantes quantifiées [0,255]
             */
            NKENTSEU_MATH_API_FORCE_INLINE
            NkColor NkColorF::ToColor() const noexcept
            {
                return {
                    static_cast<uint8>(NkClamp(r, 0.0f, 1.0f) * k255),
                    static_cast<uint8>(NkClamp(g, 0.0f, 1.0f) * k255),
                    static_cast<uint8>(NkClamp(b, 0.0f, 1.0f) * k255),
                    static_cast<uint8>(NkClamp(a, 0.0f, 1.0f) * k255)
                };
            }

            /**
             * @brief Factory function : crée NkColorF depuis NkColor
             * @param color Couleur NkColor à convertir
             * @return Nouvelle couleur NkColorF équivalente
             */
            NKENTSEU_MATH_API_FORCE_INLINE
            NkColorF FromColor(const NkColor& color) noexcept
            {
                return NkColorF(color);
            }

            /**
             * @brief Constructeur NkColor depuis NkColorF (implémentation inline)
             * @param color Couleur NkColorF à convertir
             */
            NKENTSEU_MATH_API_FORCE_INLINE
            NkColor::NkColor(const NkColorF& color) noexcept
                : r(static_cast<uint8>(NkClamp(color.r, 0.0f, 1.0f) * NkColorF::k255))
                , g(static_cast<uint8>(NkClamp(color.g, 0.0f, 1.0f) * NkColorF::k255))
                , b(static_cast<uint8>(NkClamp(color.b, 0.0f, 1.0f) * NkColorF::k255))
                , a(static_cast<uint8>(NkClamp(color.a, 0.0f, 1.0f) * NkColorF::k255))
            {
            }

            /**
             * @brief Conversion NkColor vers NkColorF (implémentation inline)
             * @return Nouvelle couleur NkColorF équivalente
             */
            NKENTSEU_MATH_API_FORCE_INLINE
            NkColorF NkColor::ToColorF() const noexcept
            {
                return NkColorF(*this);
            }

            /**
             * @brief Factory function : crée NkColor depuis NkColorF
             * @param color Couleur NkColorF à convertir
             * @return Nouvelle couleur NkColor équivalente
             */
            NKENTSEU_MATH_API_FORCE_INLINE
            NkColor FromColorF(const NkColorF& color) noexcept
            {
                return NkColor(color);
            }

        } // namespace math

    } // namespace nkentseu

    // ============================================================================
    // POINT D'EXTENSION ADL : NkToString pour NkColor/NkColorF (namespace nkentseu)
    // ============================================================================

    namespace nkentseu {

        /**
         * @brief Surcharge ADL de NkToString pour NkColorF avec support de formatage.
         */
        NKENTSEU_MATH_API
        NkString NkToString(const math::NkColorF& c, const NkFormatProps& props = {});

        /**
         * @brief Surcharge ADL de NkToString pour NkColor avec support de formatage.
         */
        NKENTSEU_MATH_API
        NkString NkToString(const math::NkColor& c, const NkFormatProps& props = {});

    } // namespace nkentseu

#endif // NKENTSEU_MATH_NKCOLOR_H

// ============================================================================
// EXEMPLES D'UTILISATION ET PATTERNS COURANTS
// ============================================================================
/*
 * @section examples Exemples pratiques d'utilisation de NkColor et NkColorF
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 1 : Création et conversion entre NkColor et NkColorF
 * --------------------------------------------------------------------------
 * @code
 * #include "NKMath/NkColor.h"
 * #include <cstdio>
 * 
 * void exempleConversionCouleurs()
 * {
 *     using nkentseu::math::NkColor;
 *     using nkentseu::math::NkColorF;
 *     
 *     // Création NkColor (8-bit)
 *     NkColor red8(255, 0, 0);  // Rouge opaque
 *     
 *     // Conversion vers NkColorF (flottant)
 *     NkColorF redF = red8.ToColorF();
 *     printf("Red as float: (%.3f, %.3f, %.3f, %.3f)\n", 
 *            redF.r, redF.g, redF.b, redF.a);  // (1.000, 0.000, 0.000, 1.000)
 *     
 *     // Conversion retour vers NkColor
 *     NkColor red8Again = redF.ToColor();
 *     printf("Back to 8-bit: (%d, %d, %d, %d)\n", 
 *            red8Again.r, red8Again.g, red8Again.b, red8Again.a);  // (255, 0, 0, 255)
 *     
 *     // Factory functions alternatives
 *     NkColorF blueF = NkColor::FromColorF(NkColor::Blue);  // Via factory
 *     NkColor blue8 = NkColorF::FromColor(NkColorF(0, 0, 1));  // Via factory inverse
 *     
 *     // Conversion implicite/explicite
 *     NkColorF implicit = NkColor::Green;  // Conversion implicite via FromColor
 *     NkColor explicitConv = static_cast<NkColor>(NkColorF(0.5f, 0.5f, 0.5f));  // Explicite
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 2 : Calculs précis avec NkColorF, stockage avec NkColor
 * --------------------------------------------------------------------------
 * @code
 * #include "NKMath/NkColor.h"
 * 
 * void exempleCalculsPrecis()
 * {
 *     using nkentseu::math::NkColor;
 *     using nkentseu::math::NkColorF;
 *     
 *     // Couleurs de départ en 8-bit (stockage compact)
 *     NkColor start = NkColor::Red;
 *     NkColor end = NkColor::Blue;
 *     
 *     // Conversion en flottant pour calculs précis
 *     NkColorF startF = start.ToColorF();
 *     NkColorF endF = end.ToColorF();
 *     
 *     // Interpolation fluide sans perte de précision
 *     for (int i = 0; i <= 100; ++i) {
 *         float32 t = static_cast<float32>(i) / 100.0f;
 *         NkColorF interpolatedF = startF.Lerp(endF, t);
 *         
 *         // Conversion retour en 8-bit pour stockage/rendu
 *         NkColor interpolated8 = interpolatedF.ToColor();
 *         
 *         // Utilisation dans un système de rendu...
 *         // renderer.SetColor(interpolated8.ToU32());
 *     }
 *     
 *     // Avantage : l'interpolation en flottant évite les artefacts de quantification
 *     // qui pourraient apparaître avec NkColor::Lerp() direct sur 8-bit
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 3 : Opérations arithmétiques sur NkColorF
 * --------------------------------------------------------------------------
 * @code
 * #include "NKMath/NkColor.h"
 * 
 * void exempleOperationsArithmetiques()
 * {
 *     using nkentseu::math::NkColorF;
 *     
 *     // Couleurs de base
 *     NkColorF red(1.0f, 0.0f, 0.0f);
 *     NkColorF blue(0.0f, 0.0f, 1.0f);
 *     NkColorF white(1.0f, 1.0f, 1.0f);
 *     
 *     // Addition (peut dépasser [0,1])
 *     NkColorF magenta = red + blue;  // (1.0, 0.0, 1.0)
 *     
 *     // Multiplication par scalaire (ajustement luminosité)
 *     NkColorF dimRed = red * 0.5f;  // (0.5, 0.0, 0.0)
 *     
 *     // Multiplication composante par composante (masquage)
 *     NkColorF masked = magenta * white;  // (1.0, 0.0, 1.0)
 *     
 *     // Mélange multiplicatif pour effets spéciaux
 *     NkColorF overlay = red * NkColorF(1.0f, 0.5f, 0.5f);  // (1.0, 0.0, 0.0)
 *     
 *     // Attention : pas de clamping automatique
 *     NkColorF overflow = red + white;  // (2.0, 1.0, 1.0) - hors [0,1]
 *     
 *     // Clamping manuel si nécessaire avant conversion vers NkColor
 *     NkColorF clamped = {
 *         NkClamp(overflow.r, 0.0f, 1.0f),
 *         NkClamp(overflow.g, 0.0f, 1.0f),
 *         NkClamp(overflow.b, 0.0f, 1.0f),
 *         NkClamp(overflow.a, 0.0f, 1.0f)
 *     };
 *     NkColor safe = clamped.ToColor();  // Conversion sûre vers 8-bit
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 4 : Intégration avec shaders et API graphiques
 * --------------------------------------------------------------------------
 * @code
 * #include "NKMath/NkColor.h"
 * 
 * // Fonction pour uploader une couleur à un shader OpenGL
 * void SetShaderColor(GLuint shaderProgram, const char* uniformName, 
 *                     const nkentseu::math::NkColorF& color)
 * {
 *     // NkColorF est directement compatible avec glUniform4f
 *     GLint loc = glGetUniformLocation(shaderProgram, uniformName);
 *     glUniform4f(loc, color.r, color.g, color.b, color.a);
 * }
 * 
 * // Fonction pour récupérer une couleur depuis une texture HDR
 * nkentseu::math::NkColorF SampleHDRTexture(TextureHandle tex, float u, float v)
 * {
 *     // Lecture de texture HDR retourne des valeurs flottantes [0, +∞)
 *     float32 r, g, b, a;
 *     ReadTexture(tex, u, v, &r, &g, &b, &a);
 *     
 *     // Retour direct en NkColorF (pas de quantification prématurée)
 *     return { r, g, b, a };
 * }
 * 
 * void exempleIntegrationGraphique()
 * {
 *     using nkentseu::math::NkColor;
 *     using nkentseu::math::NkColorF;
 *     
 *     // Couleur de thème en 8-bit (stockage UI)
 *     NkColor uiColor = NkColor::SkyBlue;
 *     
 *     // Conversion pour shader (précision flottante requise)
 *     NkColorF shaderColor = uiColor.ToColorF();
 *     SetShaderColor(shaderProgram, "uBaseColor", shaderColor);
 *     
 *     // Lecture depuis texture HDR
 *     NkColorF hdrSample = SampleHDRTexture(hdrTex, 0.5f, 0.5f);
 *     
 *     // Tonemapping simple pour affichage
 *     NkColorF tonemapped = {
 *         hdrSample.r / (hdrSample.r + 1.0f),
 *         hdrSample.g / (hdrSample.g + 1.0f),
 *         hdrSample.b / (hdrSample.b + 1.0f),
 *         hdrSample.a
 *     };
 *     
 *     // Conversion finale vers 8-bit pour affichage écran
 *     NkColor displayColor = tonemapped.ToColor();
 *     DrawPixel(x, y, displayColor.ToU32());  // Format ABGR pour DrawList
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 5 : Comparaison et tolérance flottante
 * --------------------------------------------------------------------------
 * @code
 * #include "NKMath/NkColor.h"
 * #include "NKCore/Assert/NkAssert.h"
 * 
 * void exempleComparaisonFlottante()
 * {
 *     using nkentseu::math::NkColorF;
 *     
 *     // Création de couleurs "identiques" avec erreurs d'arrondi
 *     NkColorF a(0.1f, 0.2f, 0.3f);
 *     NkColorF b(0.1f + 1e-7f, 0.2f - 1e-7f, 0.3f);  // Différences minimes
 *     
 *     // Comparaison exacte (échouera à cause des erreurs d'arrondi)
 *     bool exactEqual = (a == b);  // false
 *     
 *     // Comparaison avec tolérance (recommandé)
 *     bool approxEqual = a.IsEqual(b, 1e-5f);  // true
 *     
 *     // Utilisation dans des tests unitaires
 *     NKENTSEU_ASSERT(a.IsEqual(b, 1e-4f));  // Passe avec tolérance appropriée
 *     
 *     // Attention : la tolérance par défaut est 1e-5
 *     NKENTSEU_ASSERT(a.IsEqual(b));  // Passe avec tolération par défaut
 *     
 *     // Pour des comparaisons strictes (ex: clés de map)
 *     // Préférer NkColor (8-bit) qui a une égalité bit-à-bit exacte
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * BONNES PRATIQUES ET RECOMMANDATIONS
 * --------------------------------------------------------------------------
 * 
 * 1. CHOIX ENTRE NKCOLOR ET NKCOLORF :
 *    - NkColor (8-bit) : stockage compact, rendu UI, réseau, persistance
 *    - NkColorF (float) : calculs précis, shaders, interpolations, HDR
 *    - Conversion bidirectionnelle : utiliser ToColorF()/ToColor() aux frontières
 *    - Règle : calculer en float, stocker en 8-bit sauf besoin spécifique
 * 
 * 2. GESTION DES VALEURS HORS [0,1] DANS NKCOLORF :
 *    - Les opérations arithmétiques ne clampent pas automatiquement
 *    - Clamper manuellement avant conversion vers NkColor : NkClamp(v, 0, 1)
 *    - Pour HDR/tonemapping : garder les valeurs >1 jusqu'au dernier moment
 *    - Documenter si une fonction attend des valeurs dans [0,1] ou peut gérer HDR
 * 
 * 3. PRÉCISION ET ERREURS D'ARRONDI :
 *    - NkColorF utilise float32 : précision ~7 décimales significatives
 *    - Les conversions répétées NkColor ↔ NkColorF accumulent des erreurs
 *    - Pour calculs critiques : garder en NkColorF le plus longtemps possible
 *    - Comparaisons : utiliser IsEqual(epsilon) au lieu de == pour NkColorF
 * 
 * 4. PERFORMANCE DES CONVERSIONS :
 *    - NkColor → NkColorF : 4 multiplications par 1/255 (inline, négligeable)
 *    - NkColorF → NkColor : 4 clamps + 4 multiplications par 255 + 4 casts (inline)
 *    - Éviter les conversions dans des boucles serrées : convertir une fois en amont
 *    - Pour tableaux : considérer NkVector<NkColorF> vs NkVector<NkColor> selon l'usage
 * 
 * 5. INTÉGRATION AVEC LES SHADERS ET APIS GRAPHIQUES :
 *    - NkColorF est directement compatible avec glUniform4f, vkCmdPushConstants, etc.
 *    - Pour les textures : NkColorF pour HDR, NkColor pour LDR/8-bit
 *    - Tonemapping/exposure : effectuer en NkColorF, convertir en NkColor pour affichage
 *    - Documenter le format attendu par chaque fonction graphique (float vs 8-bit)
 * 
 * 6. SÉRIALISATION ET PORTABILITÉ :
 *    - NkColor : 4 bytes, format binaire natif (attention à l'endianness)
 *    - NkColorF : 16 bytes, format binaire natif (float32 IEEE 754)
 *    - Pour portabilité cross-platform : normaliser endianness ou utiliser texte
 *    - NkColorF en texte : plus lisible mais plus volumineux (ex: "1.0,0.0,0.0,1.0")
 * 
 * 7. LIMITATIONS CONNUES :
 *    - NkColorF n'a pas de couleurs nommées : utiliser NkColor::Red.ToColorF() si besoin
 *    - Pas d'opérations HSV natives sur NkColorF : convertir via NkColor si nécessaire
 *    - Pas de sérialisation binaire dédiée : utiliser ToColor()/ToColorF() + sérialisation générique
 *    - Comparaison == sur NkColorF utilise epsilon : peut masquer des différences significatives
 * 
 * 8. EXTENSIONS RECOMMANDÉES :
 *    - Ajouter des opérateurs de blending alpha compositing pour NkColorF
 *    - Supporter les espaces colorimétriques linéaires (sRGB ↔ linear)
 *    - Ajouter des méthodes de tonemapping (Reinhard, ACES, etc.) pour NkColorF
 *    - Supporter la sérialisation texte hexadécimale (#RRGGBBAA) pour NkColorF
 *    - Ajouter des factory methods pour gradients, palettes, ou couleurs procédurales
 * 
 * 9. INTÉGRATION AVEC LE RESTE DU FRAMEWORK :
 *    - Compatible avec NkVector<NkColor> et NkVector<NkColorF> pour tableaux
 *    - Utilisable comme clé dans NkMap<NkColor, V> grâce à operator== exact
 *    - NkToString() compatible avec NkFormat/NkLogger pour logging structuré
 *    - Conversion vers NkVector3f/NkVector4f compatible avec les maths du framework
 * 
 * 10. DEBUGGING ET VALIDATION :
 *    - Activer NKENTSEU_ASSERT en debug pour valider les inputs [0,1] dans NkColorF
 *    - Utiliser ToString() pour logging des couleurs dans les traces d'exécution
 *    - Tester les cas limites : (0,0,0,0), (1,1,1,1), valeurs négatives, >1, NaN/Inf
 *    - Valider la précision des conversions HSV↔RGB sur les deux types
 *    - Profiler les conversions dans les chemins critiques pour optimisation
 */

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Création : 2026-04-26
// Dernière modification : 2026-04-26 (v2.1 : ajout NkColorF + conversions bidirectionnelles)
// ============================================================