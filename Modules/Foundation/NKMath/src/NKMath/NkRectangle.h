//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 4/12/2024 at 6:16:36 PM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __NKENTSEU_RECTANGLE_H__
#define __NKENTSEU_RECTANGLE_H__

#pragma once

#include "NKMath/NkLegacySystem.h" // Inclure le fichier d'en-tête System/System.h
#include "NKMath/NkVec.h" // Inclure le fichier d'en-tête NkVector2f.h depuis le répertoire Ntsm/Vector
#include "NKMath/NkSegment.h"

namespace nkentseu {

	namespace math {
		// Définition de la classe NkRectangle
		class NkRectangle {
			public:

				NkVector2f corner{}; // Coin supérieur gauche du rectangle
				NkVector2f size{}; // Taille du rectangle

				// Constructeurs
				NkRectangle();
				NkRectangle(const NkVector2f& c, const NkVector2f& r);
				NkRectangle(float32 x, float32 y, float32 w, float32 h);
				NkRectangle(const NkVector2f& c, float32 w, float32 h);
				NkRectangle(float32 x, float32 y, const NkVector2f& r);
				// Surcharge de l'opérateur d'assignation
				NkRectangle operator=(const NkRectangle& other);

				// Surcharge de l'opérateur de flux de sortie
				friend std::ostream& operator<<(std::ostream& os, const NkRectangle& e) { // Surcharge de l'opérateur << pour l'affichage d'un NkRectangle
					return os << e.ToString().CStr(); // Renvoyer le résultat de l'appel à la méthode ToString() du NkRectangle
				}

				// Méthode pour obtenir une représentation sous forme de chaîne de caractères du NkRectangle
				NkString ToString() const;

				friend NkString ToString(const NkRectangle& r) {
					return r.ToString();
				}

				// Méthodes membres de la classe NkRectangle (non implémentées dans ce contexte)
				NkVector2f Clamp(const NkVector2f& p);
				NkVector2f Corner(int nr);
				bool SeparatingAxis(const NkSegment& axis);
				NkRectangle Enlarge(const NkVector2f& p);
				NkRectangle Enlarge(const NkRectangle& extender);
				static NkRectangle AABB(const NkRectangle* rectangles, int count);
				NkVector2f Center();
		};

		template <typename T>
		class NkRectT {
			public:
				union {
					struct { union { T x; T left; }; union { T y; T top; }; union { T width; T w; }; union { T height; T h; }; };
					struct { NkVec2T<T> position; NkVec2T<T> size; };
				};

				NkRectT() : position(T(0), T(0)), size(T(0), T(0)) {}
				NkRectT(T x, T y, T width, T height) : position(x, y), size(width, height) {}
				NkRectT(const NkVec2T<T>& position, const NkVec2T<T>& size) : position(position), size(size) {}
				NkRectT(const NkRectT& rect) : position(rect.position), size(rect.size) {}

				template <typename U>
				NkRectT(const NkVec2T<U>& position, const NkVec2T<U>& size) : position(position), size(size) {}
				template <typename U>
				NkRectT(const NkRectT<U>& rect) : position((NkVec2T<T>)rect.position), size((NkVec2T<T>)rect.size) {}

				//~RectT() {}

				NkRectT& operator=(const NkRectT& rect) {
					this->x = rect.x;
					this->y = rect.y;
					this->width = rect.width;
					this->height = rect.height;

					return *this;
				}

				NkVec2T<T> GetCenter() const {
					return NkVector2(x + width / 2, y + height / 2);
				}

				T GetCenterX() const {
					return x + width / 2;
				}

				T GetCenterY() const {
					return y + height / 2;
				}

				bool Contains(T px, T py) const {
					return px >= x && px < (x + width) &&
						py >= y && py < (y + height);
				}

				template <typename U>
				bool Contains(const NkVec2T<U>& point) const {
					return Contains(static_cast<T>(point.x), static_cast<T>(point.y));
				}

				friend bool operator==(const NkRectT& left, const NkRectT& right) {
					return left.x == right.x && left.y == right.y && left.width == right.width && left.height == right.height;
				}

				friend bool operator!=(const NkRectT& left, const NkRectT& right) {
					return !(left == right);
				}
		};

		using NkIntRect = NkRectT<int32>;
		using NkFloatRect = NkRectT<float32>;
	}
} // namespace nkentseu

#endif  // __NKENTSEU_RECTANGLE_H__ // Fin de la définition de la classe NkRectangle
