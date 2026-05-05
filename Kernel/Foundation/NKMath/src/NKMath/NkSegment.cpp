//
// NkSegment.cpp
// =============================================================================
// Description :
//   Implémentation des méthodes de la classe NkSegment.
//   Contient les définitions des fonctions géométriques non-inline
//   pour séparer l'interface (header) de l'implémentation (source).
//
// Auteur   : TEUGUIA TADJUIDJE Rodolf Séderis
// Copyright: (c) 2024 Rihen. Tous droits réservés.
// =============================================================================

// =====================================================================
// Inclusion de l'en-tête précompilé (requis en premier)
// =====================================================================
#include "pch.h"

// =====================================================================
// Inclusion de l'en-tête de déclaration de la classe
// =====================================================================
#include "NKMath/NkSegment.h"

// =====================================================================
// Namespace : nkentseu::math
// =====================================================================
namespace nkentseu {
namespace math {

    // ---------------------------------------------------------------------
    // Méthode : Project
    // ---------------------------------------------------------------------
    // Projette le segment courant sur un axe directionnel donné.
    // Retourne l'intervalle [min, max] des projections scalaires des
    // deux extrémités, utile pour les tests de chevauchement (SAT).
    //
    // Paramètre :
    //   onto : vecteur directionnel de l'axe de projection (sera normalisé)
    //
    // Retour :
    //   NkRangeFloat contenant les bornes min/max de la projection
    // ---------------------------------------------------------------------
    NkRangeFloat NkSegment::Project(const NkVector2f& axisDirection)
    {
        // Normalisation de l'axe pour obtenir un vecteur unitaire
        NkVector2f unitAxis = NkVector2f(axisDirection).Normalized();

        // Projection scalaire de chaque extrémité via produit dot
        float32 projectionA = unitAxis.Dot(points[0]);
        float32 projectionB = unitAxis.Dot(points[1]);

        // Construction de l'intervalle avec tri automatique min/max
        return NkRangeFloat(projectionA, projectionB);
    }

    // ---------------------------------------------------------------------
    // Méthode : Len
    // ---------------------------------------------------------------------
    // Calcule la longueur euclidienne du segment.
    // Alias sémantique pour Length() pour compatibilité avec le codebase.
    //
    // Retour :
    //   Distance float32 entre les points A et B
    // ---------------------------------------------------------------------
    float32 NkSegment::Len()
    {
        // Délégation à Length() pour éviter la duplication de code
        return Length();
    }

    // ---------------------------------------------------------------------
    // Méthode : Length
    // ---------------------------------------------------------------------
    // Calcule la longueur euclidienne du segment.
    // Formule : ||B - A|| = sqrt((Bx-Ax)² + (By-Ay)²)
    //
    // Retour :
    //   Distance float32 entre les points A et B
    // ---------------------------------------------------------------------
    float32 NkSegment::Length() const
    {
        // Vecteur différence B - A
        NkVector2f difference = points[1] - points[0];

        // Norme euclidienne du vecteur différence
        return difference.Len();
    }

    // ---------------------------------------------------------------------
    // Méthode : Equivalent
    // ---------------------------------------------------------------------
    // Teste l'équivalence géométrique entre deux segments.
    // Deux segments sont équivalents si :
    //   - Ils sont strictement égaux (mêmes points dans le même ordre), OU
    //   - Ils ont la même longueur (indépendamment de l'orientation)
    //
    // Paramètre :
    //   other : segment à comparer avec l'instance courante
    //
    // Retour :
    //   true si les segments sont géométriquement équivalents
    // ---------------------------------------------------------------------
    bool NkSegment::Equivalent(const NkSegment& other)
    {
        // Cas 1 : égalité stricte (points identiques dans le même ordre)
        if (*this == other) {
            return true;
        }

        // Cas 2 : même longueur géométrique (orientation ignorée)
        // Note : comparaison flottante directe - à remplacer par NkApproxEqual si besoin
        return Length() == other.Length();
    }

    // ---------------------------------------------------------------------
    // Méthode : ToString
    // ---------------------------------------------------------------------
    // Génère une représentation textuelle formatée du segment.
    // Format : "NkSegment[A(x1, y1); B(x2, y2)]"
    //
    // Retour :
    //   NkString contenant la représentation lisible du segment
    // ---------------------------------------------------------------------
    NkString NkSegment::ToString() const
    {
        return NkFormat(
            "NkSegment[A({0}, {1}); B({2}, {3})]",
            points[0].x,
            points[0].y,
            points[1].x,
            points[1].y
        );
    }

    // ---------------------------------------------------------------------
    // Opérateur : operator<< (friend)
    // ---------------------------------------------------------------------
    // Surcharge de l'opérateur de flux pour permettre l'affichage direct
    // d'un NkSegment dans un std::ostream (std::cout, fichiers, etc.)
    //
    // Paramètres :
    //   outputStream : flux de sortie cible
    //   segment : segment à afficher
    //
    // Retour :
    //   Référence au flux pour chaînage des opérations <<
    // ---------------------------------------------------------------------
    std::ostream& operator<<(
        std::ostream& outputStream,
        const NkSegment& segment
    ) {
        return outputStream << segment.ToString().CStr();
    }

    // ---------------------------------------------------------------------
    // Opérateur : operator== (friend)
    // ---------------------------------------------------------------------
    // Comparaison d'égalité stricte entre deux segments.
    // Les deux points doivent être identiques dans le même ordre.
    //
    // Paramètres :
    //   left : premier segment à comparer
    //   right : second segment à comparer
    //
    // Retour :
    //   true si points[0] et points[1] sont égaux dans les deux segments
    // ---------------------------------------------------------------------
    bool operator==(
        const NkSegment& left,
        const NkSegment& right
    ) noexcept {
        return (left.points[0] == right.points[0])
            && (left.points[1] == right.points[1]);
    }

    // ---------------------------------------------------------------------
    // Opérateur : operator!= (friend)
    // ---------------------------------------------------------------------
    // Comparaison d'inégalité entre deux segments.
    // Déléguée à l'opérateur d'égalité pour éviter la duplication de code.
    //
    // Paramètres :
    //   left : premier segment à comparer
    //   right : second segment à comparer
    //
    // Retour :
    //   true si les segments diffèrent sur au moins un point
    // ---------------------------------------------------------------------
    bool operator!=(
        const NkSegment& left,
        const NkSegment& right
    ) noexcept {
        return !(left == right);
    }

}  // namespace math
}  // namespace nkentseu