//
// NkSegment.h
// =============================================================================
// Description :
//   Définition de la classe NkSegment représentant un segment de droite 2D
//   délimité par deux points d'extrémité A et B.
//
// Fonctionnalités principales :
//   - Stockage optimisé via union (accès par vecteurs ou coordonnées scalaires)
//   - Opérations géométriques : longueur, projection, équivalence
//   - Comparaisons et sérialisation pour débogage
//
// Conception :
//   - Structure POD-friendly pour performances et compatibilité SIMD
//   - Méthodes non-inline définies dans NkSegment.cpp (règle projet)
//   - Alias NkVector2f pour cohérence avec le module mathématique
//
// Auteur   : TEUGUIA TADJUIDJE Rodolf Séderis
// Copyright: (c) 2024 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_SEGMENT_H__
#define __NKENTSEU_SEGMENT_H__

    // =====================================================================
    // Inclusions des dépendances du projet
    // =====================================================================
    #include "NKMath/NkLegacySystem.h"  // Types fondamentaux : float32, NkString, etc.
    #include "NKMath/NkVec.h"           // Type NkVector2f pour les points 2D
    #include "NKMath/NkRange.h"         // Type NkRangeFloat pour les projections

    // =====================================================================
    // Namespace principal du projet
    // =====================================================================
    namespace nkentseu {

        // =================================================================
        // Sous-namespace mathématique : types et opérations géométriques 2D
        // =================================================================
        namespace math {

            // =================================================================
            // Classe : NkSegment
            // =================================================================
            // Représente un segment de droite dans le plan 2D défini par
            // deux points d'extrémité : A (origine) et B (extrémité).
            //
            // Mémoire : union permettant l'accès flexible aux données :
            //   - points[2] : accès par vecteurs NkVector2f
            //   - x1,y1,x2,y2 : accès scalaire direct aux coordonnées
            //   - ptr[4] : accès tableau brut pour opérations SIMD/low-level
            // =================================================================
            class NkSegment {

                // -----------------------------------------------------------------
                // Section : Membres publics (interface)
                // -----------------------------------------------------------------
                public:

                    // -----------------------------------------------------------------
                    // Sous-section : Union de données (stockage flexible)
                    // -----------------------------------------------------------------
                    // Union permettant plusieurs vues sur les mêmes données mémoire
                    union {

                        // Vue par vecteurs : deux points NkVector2f
                        NkVector2f points[2];

                        // Vue scalaire : coordonnées individuelles nommées
                        struct {
                            float32 x1;  ///< Coordonnée X du point A (origine)
                            float32 y1;  ///< Coordonnée Y du point A (origine)
                            float32 x2;  ///< Coordonnée X du point B (extrémité)
                            float32 y2;  ///< Coordonnée Y du point B (extrémité)
                        };

                        // Vue tableau brut : pour accès indexé ou opérations SIMD
                        float32 ptr[4];

                    };  // union

                    // -----------------------------------------------------------------
                    // Sous-section : Constructeurs
                    // -----------------------------------------------------------------
                    // Constructeur par défaut : segment nul à l'origine (0,0)-(0,0)
                    NkSegment() noexcept
                        : points{ NkVector2f(0.0f, 0.0f), NkVector2f(0.0f, 0.0f) }
                    {
                    }

                    // Constructeur par vecteurs : initialise avec deux points explicites
                    NkSegment(
                        const NkVector2f& startPoint,
                        const NkVector2f& endPoint
                    ) noexcept
                        : points{ startPoint, endPoint }
                    {
                    }

                    // Constructeur par coordonnées scalaires : initialise A(x1,y1) et B(x2,y2)
                    NkSegment(
                        float32 startX,
                        float32 startY,
                        float32 endX,
                        float32 endY
                    ) noexcept
                        : points{ NkVector2f(startX, startY), NkVector2f(endX, endY) }
                    {
                    }

                    // Constructeur de copie : duplique les points d'un segment existant
                    NkSegment(const NkSegment& source) noexcept
                        : points{ source.points[0], source.points[1] }
                    {
                    }

                    // -----------------------------------------------------------------
                    // Sous-section : Opérateur d'affectation
                    // -----------------------------------------------------------------
                    // Affectation avec protection contre l'auto-affectation
                    NkSegment& operator=(const NkSegment& other) noexcept
                    {
                        if (this != &other) {
                            points[0] = other.points[0];
                            points[1] = other.points[1];
                        }
                        return *this;
                    }

                    // -----------------------------------------------------------------
                    // Sous-section : Méthodes géométriques (définies dans .cpp)
                    // -----------------------------------------------------------------
                    // Projection du segment sur un axe directionnel 'onto'
                    // Retourne l'intervalle [min, max] des projections scalaires
                    NkRangeFloat Project(const NkVector2f& axisDirection);

                    // Longueur euclidienne du segment : distance entre A et B
                    float32 Length() const;

                    // Alias sémantique pour Length()
                    float32 Len();

                    // Test d'équivalence géométrique : même longueur ou mêmes points
                    bool Equivalent(const NkSegment& other);

                    // -----------------------------------------------------------------
                    // Sous-section : Sérialisation et E/S
                    // -----------------------------------------------------------------
                    // Conversion en chaîne formatée pour débogage et logging
                    NkString ToString() const;

                    // Surcharge de l'opérateur de flux pour affichage std::ostream
                    friend std::ostream& operator<<(
                        std::ostream& outputStream,
                        const NkSegment& segment
                    );

                    // -----------------------------------------------------------------
                    // Sous-section : Opérateurs de comparaison
                    // -----------------------------------------------------------------
                    // Égalité stricte : les deux points doivent être identiques
                    friend bool operator==(
                        const NkSegment& left,
                        const NkSegment& right
                    ) noexcept;

                    // Inégalité : déléguée à l'opérateur d'égalité (principe DRY)
                    friend bool operator!=(
                        const NkSegment& left,
                        const NkSegment& right
                    ) noexcept;

            };  // class NkSegment

        }  // namespace math

        // =====================================================================
        // POINT D'EXTENSION ADL : NkToString pour NkSegment (namespace nkentseu)
        // =====================================================================
        // Doit être dans namespace nkentseu (pas math) pour être trouvé par ADL
        // depuis nkentseu::detail::adl::Invoke (utilisé par NkFormat/NkFormatter).
        // =====================================================================
        inline NkString NkToString(
            const math::NkSegment& segment,
            const NkFormatProps& formatProperties = {}
        ) {
            return formatProperties.ApplyWidth(NkStringView(segment.ToString()), false);
        }

    }  // namespace nkentseu

#endif  // __NKENTSEU_SEGMENT_H__

// =============================================================================
// EXEMPLES D'UTILISATION (documentation - ne pas compiler)
// =============================================================================
/*
    // ---------------------------------------------------------------------
    // Exemple 1 : Création de segments avec différents constructeurs
    // ---------------------------------------------------------------------
    #include "NKMath/NkSegment.h"
    using namespace nkentseu::math;

    // Segment nul à l'origine
    NkSegment nullSegment;  // A(0,0) -> B(0,0)

    // Segment via vecteurs de points
    NkVector2f start(1.0f, 2.0f);
    NkVector2f end(4.0f, 6.0f);
    NkSegment vectorSegment(start, end);  // A(1,2) -> B(4,6)

    // Segment via coordonnées scalaires directes
    NkSegment scalarSegment(0.0f, 0.0f, 10.0f, 0.0f);  // Axe X de 0 à 10

    // Segment via copie
    NkSegment copiedSegment(vectorSegment);  // Duplique A(1,2)->B(4,6)

    // ---------------------------------------------------------------------
    // Exemple 2 : Accès aux données via l'union
    // ---------------------------------------------------------------------
    NkSegment segment(1.0f, 2.0f, 4.0f, 6.0f);

    // Accès par vecteurs (recommandé pour les opérations géométriques)
    NkVector2f pointA = segment.points[0];  // (1, 2)
    NkVector2f pointB = segment.points[1];  // (4, 6)

    // Accès scalaire direct (pratique pour les calculs composante par composante)
    float32 deltaX = segment.x2 - segment.x1;  // 3.0f
    float32 deltaY = segment.y2 - segment.y1;  // 4.0f

    // Accès tableau brut (pour boucles ou SIMD)
    for (size_t i = 0; i < 4; ++i) {
        float32 value = segment.ptr[i];  // [x1, y1, x2, y2]
    }

    // ---------------------------------------------------------------------
    // Exemple 3 : Calcul de longueur et opérations géométriques
    // ---------------------------------------------------------------------
    NkSegment diagonal(0.0f, 0.0f, 3.0f, 4.0f);

    // Longueur euclidienne : sqrt(3² + 4²) = 5
    float32 length = diagonal.Len();  // 5.0f

    // Alias Length() pour lisibilité
    float32 sameLength = diagonal.Length();  // 5.0f

    // Projection sur un axe (ex: axe X unitaire)
    NkVector2f xAxis(1.0f, 0.0f);
    NkRangeFloat xProjection = diagonal.Project(xAxis);
    // Résultat : [0.0f, 3.0f] - l'étendue du segment sur l'axe X

    // Projection sur un axe arbitraire (ex: diagonale normalisée)
    NkVector2f diagonalAxis(1.0f, 1.0f);
    NkRangeFloat diagProjection = diagonal.Project(diagonalAxis);

    // ---------------------------------------------------------------------
    // Exemple 4 : Comparaisons et équivalence
    // ---------------------------------------------------------------------
    NkSegment segA(0.0f, 0.0f, 1.0f, 1.0f);
    NkSegment segB(0.0f, 0.0f, 1.0f, 1.0f);
    NkSegment segC(1.0f, 1.0f, 0.0f, 0.0f);  // Même segment, sens inverse

    // Égalité stricte : ordre des points important
    bool strictEqual = (segA == segB);  // true
    bool reversedEqual = (segA == segC);  // false (points dans l'ordre inverse)

    // Équivalence géométrique : ignore l'ordre, compare la longueur
    bool geoEquivalent = segA.Equivalent(segC);  // true (même longueur sqrt(2))

    // Inégalité
    bool different = (segA != segC);  // true

    // ---------------------------------------------------------------------
    // Exemple 5 : Affichage et débogage
    // ---------------------------------------------------------------------
    NkSegment debugSeg(3.14f, 2.71f, 9.99f, 1.41f);

    // Via méthode membre ToString()
    NkString segmentStr = debugSeg.ToString();
    // Résultat : "NkSegment[A(3.14, 2.71); B(9.99, 1.41)]"

    // Via flux de sortie standard
    std::cout << "Segment : " << debugSeg << std::endl;

    // Via fonction globale NkToString avec formatage personnalisé
    NkString formatted = NkToString(
        debugSeg,
        NkFormatProps().SetPrecision(2)
    );  // "NkSegment[A(3.14, 2.71); B(9.99, 1.41)]"

    // ---------------------------------------------------------------------
    // Exemple 6 : Utilisation dans un algorithme de détection de collision
    // ---------------------------------------------------------------------
    bool SegmentsMayIntersect(const NkSegment& seg1, const NkSegment& seg2)
    {
        // Test rapide par projection sur les axes X et Y (Sweep and Prune 1D)
        NkVector2f xAxis(1.0f, 0.0f);
        NkVector2f yAxis(0.0f, 1.0f);

        NkRangeFloat proj1X = seg1.Project(xAxis);
        NkRangeFloat proj2X = seg2.Project(xAxis);
        if (!proj1X.Overlaps(proj2X)) {
            return false;  // Pas de chevauchement sur X → pas d'intersection
        }

        NkRangeFloat proj1Y = seg1.Project(yAxis);
        NkRangeFloat proj2Y = seg2.Project(yAxis);
        if (!proj1Y.Overlaps(proj2Y)) {
            return false;  // Pas de chevauchement sur Y → pas d'intersection
        }

        return true;  // Chevauchement sur les deux axes → test précis nécessaire
    }

    // ---------------------------------------------------------------------
    // Exemple 7 : Intégration dans un système de rendu de lignes
    // ---------------------------------------------------------------------
    struct LineRenderer {
        void DrawSegment(const NkSegment& segment, uint32 color)
        {
            // Extraction des coordonnées pour l'API graphique
            float32 x1 = segment.x1;
            float32 y1 = segment.y1;
            float32 x2 = segment.x2;
            float32 y2 = segment.y2;

            // Appel à la fonction de dessin bas niveau
            DrawLineLowLevel(x1, y1, x2, y2, color);
        }

        void DrawSegmentVectorized(const NkSegment& segment, uint32 color)
        {
            // Accès direct par vecteurs pour les APIs modernes
            DrawLineVectorized(
                segment.points[0],  // Point A
                segment.points[1],  // Point B
                color
            );
        }
    };

    // ---------------------------------------------------------------------
    // Exemple 8 : Modification via l'opérateur d'affectation
    // ---------------------------------------------------------------------
    NkSegment source(0.0f, 0.0f, 1.0f, 1.0f);
    NkSegment destination(10.0f, 10.0f, 20.0f, 20.0f);

    // Affectation standard
    destination = source;  // destination devient (0,0)->(1,1)

    // Auto-affectation (gérée sans effet de bord)
    destination = destination;  // Aucun changement, pas de corruption mémoire

    // ---------------------------------------------------------------------
    // Bonnes pratiques d'utilisation
    // ---------------------------------------------------------------------
    // ✓ Préférer l'accès par points[0]/points[1] pour la lisibilité géométrique
    // ✓ Utiliser Len() pour les comparaisons de distance (évite sqrt redondant)
    // ✓ Vérifier Equivalent() quand l'orientation du segment n'a pas d'importance
    // ✓ Profiter de l'union pour optimiser les boucles de traitement massif
    // ✗ Ne pas modifier directement ptr[] sans comprendre l'alignement mémoire
    // ✗ Attention : l'égalité == est sensible à l'ordre des points (A->B ≠ B->A)
*/