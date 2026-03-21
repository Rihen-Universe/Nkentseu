//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 4/12/2024 at 6:22:06 PM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __NKENTSEU_SEGMENT_H__
#define __NKENTSEU_SEGMENT_H__

#pragma once

#include "NKMath/NkLegacySystem.h" // Inclure le fichier d'en-tête System/System.h
#include "NKMath/NkVec.h"
#include "NKMath/NkRange.h"

namespace nkentseu {

	namespace math
	{
		// Définition de la classe NkSegment
		class NkSegment {
		public:
			union {
				NkVector2f points[2];
				struct {
					float32 x1; float32 y1; // Coordonnées x et y du point A
					float32 x2; float32 y2; // Coordonnées x et y du point B
				};
				float32 ptr[4]; // Tableau de pointeurs vers les coordonnées des points A et B
			};

			// Constructeurs
			NkSegment() : points{ NkVector2f(0.0f, 0.0f), NkVector2f(0.0f, 0.0f) } {} // Constructeur par défaut initialisant les points A et B à l'origine (0, 0)
			NkSegment(const NkVector2f& point1, const NkVector2f& point2) : points{ point1, point2 } {} // Constructeur initialisant les points A et B avec des vecteurs donnés
			NkSegment(float32 x1, float32 y1, float32 x2, float32 y2) : points{ NkVector2f(x1, y1), NkVector2f(x2, y2) } {} // Constructeur initialisant les points A et B avec des coordonnées données
			NkSegment(const NkSegment& segment) : points{ segment.points[0], segment.points[1] } {} // Constructeur de copie initialisant les points A et B avec ceux d'un autre segment

			// Surcharge de l'opérateur d'assignation
			NkSegment& operator=(const NkSegment& other) { // Opérateur d'assignation pour affecter un NkSegment à un autre NkSegment
				if (this != &other) { // Vérifier si l'objet est différent de celui à affecter
					points[0] = other.points[0]; // Affecter le point A du segment 'other' à celui-ci
					points[1] = other.points[1]; // Affecter le point B du segment 'other' à celui-ci
				}
				return *this; // Retourner le segment après l'assignation
			}

			// Surcharge de l'opérateur de flux de sortie
			friend std::ostream& operator<<(std::ostream& os, const NkSegment& e) { // Surcharge de l'opérateur << pour l'affichage d'un NkSegment
				return os << e.ToString().CStr(); // Renvoyer le résultat de l'appel à la méthode ToString() du NkSegment
			}

			// Méthode pour obtenir une représentation sous forme de chaîne de caractères du NkSegment
			NkString ToString() const { // Méthode ToString() pour obtenir une représentation sous forme de chaîne de caractères du NkSegment
				return NkFormat("NkSegment[A({0}, {1}); B({2}, {3})]", points[0].x, points[0].y, points[1].x, points[1].y);
			}

			// Méthodes membres de la classe NkSegment (non implémentées dans ce contexte)
			NkRangeFloat Project(const NkVector2f& onto);
			float32 Len();
			bool Equivalent(const NkSegment& b);

			// Surcharge des opérateurs de comparaison
			friend bool operator==(const NkSegment& l, const NkSegment& r) { // Surcharge de l'opérateur ==
				return (l.points[0] == r.points[0] && l.points[1] == r.points[1]); // Renvoyer vrai si les points A et B des deux segments sont égaux
			}

			friend bool operator!=(const NkSegment& l, const NkSegment& r) { // Surcharge de l'opérateur !=
				return !(l == r); // Renvoyer faux si les segments sont égaux, vrai sinon
			}
		};
	}
} // namespace nkentseu

#endif    // __NKENTSEU_SEGMENT_H__ // Fin de la définition de la classe NkSegment
