//
// NkRectangle.cpp
// =============================================================================
// Description :
//   Implémentation des méthodes de la classe NkRectangle.
//   Contient les définitions des fonctions géométriques non-inline
//   pour séparer l'interface (header) de l'implémentation (source).
//
// Note : La classe template NkRectT<T> reste entièrement dans le header
//        car ses méthodes doivent être visibles pour l'instanciation.
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
#include "NKMath/NkRectangle.h"

// =====================================================================
// Namespace : nkentseu::math
// =====================================================================
namespace nkentseu {
namespace math {

    // ---------------------------------------------------------------------
    // Constructeur : NkRectangle (par défaut)
    // ---------------------------------------------------------------------
    // Initialise un rectangle à l'origine avec une taille unité (1, 1).
    // Utile comme valeur par défaut ou état initial avant configuration.
    // ---------------------------------------------------------------------
    NkRectangle::NkRectangle()
        : corner(0.0f, 0.0f)
        , size(1.0f, 1.0f)
    {
    }

    // ---------------------------------------------------------------------
    // Constructeur : NkRectangle (vecteurs)
    // ---------------------------------------------------------------------
    // Initialise avec un coin supérieur gauche et des dimensions vectorielles.
    // Paramètres :
    //   cornerPoint : position du coin supérieur gauche
    //   dimensions  : largeur et hauteur du rectangle
    // ---------------------------------------------------------------------
    NkRectangle::NkRectangle(
        const NkVector2f& cornerPoint,
        const NkVector2f& dimensions
    )
        : corner(cornerPoint)
        , size(dimensions)
    {
    }

    // ---------------------------------------------------------------------
    // Constructeur : NkRectangle (scalaires)
    // ---------------------------------------------------------------------
    // Initialise avec des coordonnées et dimensions scalaires explicites.
    // Paramètres :
    //   x, y     : position du coin supérieur gauche
    //   width    : largeur du rectangle
    //   height   : hauteur du rectangle
    // ---------------------------------------------------------------------
    NkRectangle::NkRectangle(
        float32 x,
        float32 y,
        float32 width,
        float32 height
    )
        : corner(x, y)
        , size(width, height)
    {
    }

    // ---------------------------------------------------------------------
    // Constructeur : NkRectangle (coin vectoriel, taille scalaire)
    // ---------------------------------------------------------------------
    // Initialise avec un coin vectoriel et des dimensions scalaires.
    // Paramètres :
    //   cornerPoint : position du coin supérieur gauche
    //   width       : largeur du rectangle
    //   height      : hauteur du rectangle
    // ---------------------------------------------------------------------
    NkRectangle::NkRectangle(
        const NkVector2f& cornerPoint,
        float32 width,
        float32 height
    )
        : corner(cornerPoint)
        , size(width, height)
    {
    }

    // ---------------------------------------------------------------------
    // Constructeur : NkRectangle (coin scalaire, taille vectorielle)
    // ---------------------------------------------------------------------
    // Initialise avec un coin scalaire et des dimensions vectorielles.
    // Paramètres :
    //   x, y        : position du coin supérieur gauche
    //   dimensions  : largeur et hauteur du rectangle
    // ---------------------------------------------------------------------
    NkRectangle::NkRectangle(
        float32 x,
        float32 y,
        const NkVector2f& dimensions
    )
        : corner(x, y)
        , size(dimensions)
    {
    }

    // ---------------------------------------------------------------------
    // Opérateur : operator= (affectation)
    // ---------------------------------------------------------------------
    // Copie les membres corner et size depuis un autre rectangle.
    // Retourne une référence pour permettre le chaînage d'affectations.
    // Paramètre :
    //   other : rectangle source à copier
    // Retour :
    //   Référence vers cette instance après modification
    // ---------------------------------------------------------------------
    NkRectangle& NkRectangle::operator=(const NkRectangle& other)
    {
        corner = other.corner;
        size = other.size;
        return *this;
    }

    // ---------------------------------------------------------------------
    // Méthode : ToString
    // ---------------------------------------------------------------------
    // Génère une représentation textuelle formatée du rectangle.
    // Format : "NkRectT[pos(x, y); size(width, height)]"
    // Retour :
    //   NkString contenant la représentation lisible du rectangle
    // ---------------------------------------------------------------------
    NkString NkRectangle::ToString() const
    {
        return NkFormat(
            "NkRectT[pos({0}, {1}); size({2}, {3})]",
            corner.x,
            corner.y,
            size.x,
            size.y
        );
    }

    // ---------------------------------------------------------------------
    // Opérateur : operator<< (friend)
    // ---------------------------------------------------------------------
    // Surcharge de l'opérateur de flux pour permettre l'affichage direct
    // d'un NkRectangle dans un std::ostream (std::cout, fichiers, etc.)
    // Paramètres :
    //   outputStream : flux de sortie cible
    //   rectangle    : rectangle à afficher
    // Retour :
    //   Référence au flux pour chaînage des opérations <<
    // ---------------------------------------------------------------------
    std::ostream& operator<<(
        std::ostream& outputStream,
        const NkRectangle& rectangle
    ) {
        return outputStream << rectangle.ToString().CStr();
    }

    // ---------------------------------------------------------------------
    // Opérateur : ToString (global friend)
    // ---------------------------------------------------------------------
    // Surcharge globale permettant l'appel fonctionnel NkToString(rect).
    // Délègue à la méthode membre ToString() pour éviter la duplication.
    // Paramètre :
    //   rectangle : rectangle à convertir en chaîne
    // Retour :
    //   NkString contenant la représentation formatée
    // ---------------------------------------------------------------------
    NkString ToString(const NkRectangle& rectangle)
    {
        return rectangle.ToString();
    }

    // ---------------------------------------------------------------------
    // Méthode : Clamp
    // ---------------------------------------------------------------------
    // Projette un point dans les limites du rectangle [corner, corner+size].
    // Chaque composante est clampée indépendamment via NkRangeFloat.
    // Paramètre :
    //   point : point à contraindre dans le rectangle
    // Retour :
    //   Nouveau vecteur dont les composantes sont dans [min, max] du rectangle
    // ---------------------------------------------------------------------
    NkVector2f NkRectangle::Clamp(const NkVector2f& point)
    {
        NkVector2f clampedPoint;
        NkRangeFloat rangeX(corner.x, corner.x + size.x);
        NkRangeFloat rangeY(corner.y, corner.y + size.y);
        clampedPoint.x = rangeX.Clamp(point.x);
        clampedPoint.y = rangeY.Clamp(point.y);
        return clampedPoint;
    }

    // ---------------------------------------------------------------------
    // Méthode : Corner
    // ---------------------------------------------------------------------
    // Retourne les coordonnées du Nième coin du rectangle (index cyclique 0-3).
    // Convention d'indexation :
    //   0 : coin droit        (corner.x + size.x, corner.y)
    //   1 : coin bas-droit    (corner.x + size.x, corner.y + size.y)
    //   2 : coin bas          (corner.x, corner.y + size.y)
    //   3 : coin gauche/origine (corner.x, corner.y)
    // Paramètre :
    //   cornerIndex : index du coin souhaité (modulo 4 appliqué)
    // Retour :
    //   NkVector2f contenant les coordonnées du coin demandé
    // ---------------------------------------------------------------------
    NkVector2f NkRectangle::Corner(int cornerIndex)
    {
        NkVector2f resultCorner = corner;
        switch (cornerIndex % 4) {
            case 0:
                resultCorner.x += size.x;
                break;
            case 1:
                resultCorner = corner + size;
                break;
            case 2:
                resultCorner.y += size.y;
                break;
            default:
                // case 3 : retourne le coin d'origine sans modification
                break;
        }
        return resultCorner;
    }

    // ---------------------------------------------------------------------
    // Méthode : SeparatingAxis
    // ---------------------------------------------------------------------
    // Test de l'axe séparateur (Separating Axis Theorem) pour détection
    // de collision entre ce rectangle et un autre projeté sur un axe.
    //
    // Algorithme :
    //   1. Calcule la normale de l'axe de test
    //   2. Projette deux arêtes opposées du rectangle sur cette normale
    //   3. Fusionne les projections pour obtenir l'étendue totale
    //   4. Compare avec la projection de l'axe de test
    //   5. Retourne true si les projections ne se chevauchent pas
    //
    // Paramètre :
    //   axis : segment représentant l'axe de projection à tester
    // Retour :
    //   true si l'axe sépare les rectangles (pas de collision)
    //   false si les projections se chevauchent (collision possible)
    // ---------------------------------------------------------------------
    bool NkRectangle::SeparatingAxis(const NkSegment& testAxis)
    {
        // Calcul de la normale de l'axe (perpendiculaire)
        NkVector2f normal = testAxis.points[0] - testAxis.points[1];

        // Extraction des deux arêtes opposées du rectangle via Corner()
        NkSegment edgeA;
        NkSegment edgeB;
        edgeA.points[0] = Corner(0);
        edgeA.points[1] = Corner(1);
        edgeB.points[0] = Corner(2);
        edgeB.points[1] = Corner(3);

        // Projection des arêtes sur la normale
        NkRangeFloat projectionEdgeA = edgeA.Project(normal);
        NkRangeFloat projectionEdgeB = edgeB.Project(normal);

        // Fusion des deux projections pour obtenir l'étendue totale du rectangle
        NkRangeFloat rectangleProjection = NkRangeFloat::Hull(
            projectionEdgeA,
            projectionEdgeB
        );

        // Projection de l'axe de test sur lui-même
        NkRangeFloat axisProjection = NkSegment(testAxis).Project(normal);

        // Retourne true si les projections sont disjointes (séparation)
        return !axisProjection.Overlaps(rectangleProjection);
    }

    // ---------------------------------------------------------------------
    // Méthode : Enlarge (point)
    // ---------------------------------------------------------------------
    // Étend ce rectangle pour inclure un point supplémentaire.
    // Calcule le nouveau bounding box englobant l'original et le point.
    //
    // Algorithme :
    //   1. Détermine les nouveaux min/max sur chaque axe
    //   2. Recalcule la taille comme différence max - min
    //
    // Paramètre :
    //   point : point à inclure dans le rectangle étendu
    // Retour :
    //   Nouveau NkRectangle englobant l'original et le point
    // ---------------------------------------------------------------------
    NkRectangle NkRectangle::Enlarge(const NkVector2f& point)
    {
        NkRectangle enlargedRect;

        // Calcul des nouvelles bornes minimales
        enlargedRect.corner.x = NkMin(corner.x, point.x);
        enlargedRect.corner.y = NkMin(corner.y, point.y);

        // Calcul des nouvelles bornes maximales
        float32 maxCornerX = NkMax(corner.x + size.x, point.x);
        float32 maxCornerY = NkMax(corner.y + size.y, point.y);

        // Recalcul de la taille à partir des nouvelles bornes
        enlargedRect.size.x = maxCornerX - enlargedRect.corner.x;
        enlargedRect.size.y = maxCornerY - enlargedRect.corner.y;

        return enlargedRect;
    }

    // ---------------------------------------------------------------------
    // Méthode : Enlarge (rectangle)
    // ---------------------------------------------------------------------
    // Étend ce rectangle pour inclure entièrement un autre rectangle.
    // Utilise la méthode Enlarge(point) sur les coins de l'extender.
    //
    // Paramètre :
    //   extender : rectangle à inclure dans le résultat
    // Retour :
    //   Nouveau NkRectangle englobant les deux rectangles
    // ---------------------------------------------------------------------
    NkRectangle NkRectangle::Enlarge(const NkRectangle& extender)
    {
        // Calcul du coin opposé (bas-droit) du rectangle extender
        NkVector2f extenderMaxCorner = extender.corner + extender.size;

        // Première extension avec le coin bas-droit
        NkRectangle enlargedRect = Enlarge(extenderMaxCorner);

        // Seconde extension avec le coin haut-gauche de extender
        enlargedRect = enlargedRect.Enlarge(extender.corner);

        return enlargedRect;
    }

    // ---------------------------------------------------------------------
    // Méthode : AABB (statique)
    // ---------------------------------------------------------------------
    // Calcule l'enveloppe axis-aligned bounding box (AABB) englobant
    // un tableau de rectangles. Utilise l'algorithme itératif Enlarge.
    //
    // Paramètres :
    //   rectangleArray : pointeur vers le premier rectangle du tableau
    //   rectangleCount : nombre de rectangles dans le tableau
    // Retour :
    //   NkRectangle représentant l'AABB englobant tous les rectangles
    //   Retourne un rectangle nul si count == 0
    // ---------------------------------------------------------------------
    NkRectangle NkRectangle::AABB(
        const NkRectangle* rectangleArray,
        int rectangleCount
    ) {
        // Cas de base : tableau vide retourne un rectangle nul
        if (rectangleCount == 0) {
            return NkRectangle();
        }

        // Initialisation avec le premier rectangle comme hypothèse de départ
        NkRectangle boundingBox = rectangleArray[0];

        // Itération sur les rectangles restants pour étendre l'enveloppe
        for (int index = 1; index < rectangleCount; ++index) {
            boundingBox = boundingBox.Enlarge(rectangleArray[index]);
        }

        return boundingBox;
    }

    // ---------------------------------------------------------------------
    // Méthode : Center
    // ---------------------------------------------------------------------
    // Calcule et retourne le point central géométrique du rectangle.
    // Formule : center = corner + (size / 2)
    //
    // Retour :
    //   NkVector2f contenant les coordonnées du centre du rectangle
    // ---------------------------------------------------------------------
    NkVector2f NkRectangle::Center()
    {
        NkVector2f halfSize = size / 2.0f;
        return corner + halfSize;
    }

}  // namespace math
}  // namespace nkentseu