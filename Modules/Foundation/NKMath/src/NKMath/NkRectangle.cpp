//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 4/12/2024 at 6:16:36 PM.
// Copyright (c) 2024 Rihen. All rights reserved.
//


#include "NkRectangle.h"

namespace nkentseu {

	namespace math
	{
		// Constructeurs
		NkRectangle::NkRectangle() : corner(0.0f), size(1) {} // Constructeur par défaut initialisant le coin supérieur gauche à l'origine (0, 0) et la taille à (1, 1)
		NkRectangle::NkRectangle(const NkVector2f& c, const NkVector2f& r) : corner(c), size(r) {} // Constructeur initialisant le coin supérieur gauche et la taille avec des vecteurs donnés
		NkRectangle::NkRectangle(float32 x, float32 y, float32 w, float32 h) : corner(x, y), size(w, h) {} // Constructeur initialisant le coin supérieur gauche avec des coordonnées et la taille avec des dimensions données
		NkRectangle::NkRectangle(const NkVector2f& c, float32 w, float32 h) : corner(c), size(w, h) {} // Constructeur initialisant le coin supérieur gauche avec un vecteur et la taille avec des dimensions données
		NkRectangle::NkRectangle(float32 x, float32 y, const NkVector2f& r) : corner(x, y), size(r) {} // Constructeur initialisant le coin supérieur gauche avec des coordonnées et la taille avec un vecteur donné

		// Surcharge de l'opérateur d'assignation
		NkRectangle NkRectangle::operator=(const NkRectangle& other) { // Opérateur d'assignation pour affecter un NkRectangle à un autre NkRectangle
			if (this->corner != other.corner && this->size != other.size) { // Vérifier si les coins supérieurs gauches et les tailles des deux rectangles sont différents
				this->corner = other.corner; // Affecter le coin supérieur gauche du rectangle 'other' à celui-ci
				this->size = other.size; // Affecter la taille du rectangle 'other' à celui-ci
			}
			return *this; // Retourner le rectangle après l'assignation
		}

		NkString NkRectangle::ToString() const { // Méthode ToString() pour obtenir une représentation sous forme de chaîne de caractères du NkRectangle
			return NkFormat("NkRectT[pos({0}, {1}); size({2}, {3})]", corner.x, corner.y, size.x, size.y);
		}

		NkVector2f NkRectangle::Clamp(const NkVector2f& p) {
			NkVector2f clamp;
			clamp.x = NkRangeFloat(corner.x, corner.x + size.x).Clamp(p.x);
			clamp.y = NkRangeFloat(corner.y, corner.y + size.y).Clamp(p.y);
			return clamp;
		}

		NkVector2f NkRectangle::Corner(int nr) {
			NkVector2f corner_i = corner;
			switch (nr % 4) {
			case 0:
				corner_i.x += size.x;
				break;
			case 1:
				corner_i = (corner + size);
				break;
			case 2:
				corner_i.y += size.y;
			break; default:
				/* corner = r.corner */
				break;
			}
			return corner_i;
		}

		bool NkRectangle::SeparatingAxis(const NkSegment& axis) {
			NkSegment rEdgeA, rEdgeB;
			NkRangeFloat axisRange, rEdgeARange, rEdgeBRange, rProjection;
			NkVector2f n = (axis.points[0] - axis.points[1]);
			rEdgeA.points[0] = Corner(0);
			rEdgeA.points[1] = Corner(1);
			rEdgeB.points[0] = Corner(2);
			rEdgeB.points[1] = Corner(3);
			rEdgeARange = rEdgeA.Project(n);
			rEdgeBRange = rEdgeB.Project(n);
			rProjection = NkRangeFloat::Hull(rEdgeARange, rEdgeBRange);
			axisRange = NkSegment(axis).Project(n);
			return !axisRange.Overlaps(rProjection);
		}

		NkRectangle NkRectangle::Enlarge(const NkVector2f& p) {
			NkRectangle enlarged;
			enlarged.corner.x = NkMin(corner.x, p.x);
			enlarged.corner.y = NkMin(corner.y, p.y);
			enlarged.size.x = NkMax(corner.x + size.x, p.x);
			enlarged.size.y = NkMax(corner.y + size.y, p.y);
			enlarged.size = (enlarged.size - enlarged.corner);
			return enlarged;
		}

		NkRectangle NkRectangle::Enlarge(const NkRectangle& extender) {
			NkVector2f maxCorner = (extender.corner + extender.size);
			NkRectangle enlarged = Enlarge(maxCorner);

			return enlarged.Enlarge(extender.corner);
		}

		NkRectangle NkRectangle::AABB(const NkRectangle* rectangles, int count) {
			int i; NkRectangle h = { {0, 0}, {0, 0} };

			if (0 == count) return h;

			h = rectangles[0];

			for (i = 1; i < count; i++)
				h = h.Enlarge(rectangles[i]);

			return h;
		}

		NkVector2f NkRectangle::Center() {
			NkVector2f halfSize = (size / 2);
			return (corner + halfSize);
		}
	}
}    // namespace nkentseu