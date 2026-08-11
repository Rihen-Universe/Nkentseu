#pragma once
// ============================================================================
// NkRefBoard.h — le MODÈLE de la planche de références, PUR (aucune dépendance
// rendu). C'est lui que l'Étape 2 sérialisera dans le .nkref.
// ----------------------------------------------------------------------------
// Conventions :
//  - un item = une image posée : centre monde, échelle UNIFORME (comme
//    PureRef — pas d'étirement anisotrope, le recadrage viendra à l'Étape 3),
//    rotation en DEGRÉS (l'unité de NkSprite::SetRotation), miroirs X/Y.
//  - l'ORDRE du tableau EST la profondeur : dernier = dessus. Le survol/clic
//    parcourt donc du dernier au premier.
//  - le miroir ne change PAS la géométrie (coins, hit test) : c'est un
//    retournement de la texture à l'intérieur du même rectangle.
//  - la glue (main.cpp) tient un tableau de textures ALIGNÉ SUR LES INDEX :
//    toute opération qui change l'ordre passe par les méthodes d'ici et la
//    glue applique le même mouvement à son tableau (Swap/RemoveAt renvoyés).
// ============================================================================

#include "NKMath/NKMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkref {

	using nkentseu::float32;
	using nkentseu::int32;
	using nkentseu::uint32;
	using nkentseu::usize;
	using nkentseu::NkString;
	using nkentseu::NkVector;
	using nkentseu::math::NkVec2f;

	struct NkRefItem {
			uint32 id = 0;
			NkVec2f pos{0.0f, 0.0f}; ///< centre, en monde
			float32 scale = 1.0f;	 ///< uniforme (1 = taille native à 100 %)
			float32 rotationDeg = 0.0f;
			bool mirrorX = false;
			bool mirrorY = false;
			uint32 texW = 0; ///< taille native de l'image (pixels source)
			uint32 texH = 0;
			bool selected = false;
			NkString sourcePath; ///< vide = collée du presse-papiers (Étape 2 : octets embarqués)
	};

	class NkRefBoard {
		public:
			NkVector<NkRefItem> items; ///< ordre = profondeur (dernier = dessus)

			/// L'item ACTIF = celui qui porte les poignées (dernier sélectionné).
			int32 active = -1;

			uint32 AddItem(const NkVec2f &worldPos, uint32 texW, uint32 texH, const NkString &sourcePath) {
				NkRefItem it;
				it.id = mNextId++;
				it.pos = worldPos;
				it.texW = texW;
				it.texH = texH;
				it.sourcePath = sourcePath;
				items.PushBack(it);
				return it.id;
			}

			// ── Géométrie d'un item ─────────────────────────────────────────
			// Demi-taille monde (l'échelle est déjà appliquée).
			static NkVec2f HalfSize(const NkRefItem &it) {
				return {(float32)it.texW * it.scale * 0.5f, (float32)it.texH * it.scale * 0.5f};
			}

			/// Monde → repère local NON tourné de l'item (centre = origine).
			static NkVec2f WorldToLocal(const NkRefItem &it, const NkVec2f &p) {
				const float32 rad = -it.rotationDeg * (nkentseu::math::NK_PI_F / 180.0f);
				const float32 c = nkentseu::math::NkCos(rad), s = nkentseu::math::NkSin(rad);
				const NkVec2f d{p.x - it.pos.x, p.y - it.pos.y};
				return {d.x * c - d.y * s, d.x * s + d.y * c};
			}

			static bool Contains(const NkRefItem &it, const NkVec2f &worldPoint) {
				const NkVec2f l = WorldToLocal(it, worldPoint);
				const NkVec2f h = HalfSize(it);
				return l.x >= -h.x && l.x <= h.x && l.y >= -h.y && l.y <= h.y;
			}

			/// Les 4 coins monde, ordre : HG, HD, BD, BG (haut = -Y local).
			static void Corners(const NkRefItem &it, NkVec2f out[4]) {
				const float32 rad = it.rotationDeg * (nkentseu::math::NK_PI_F / 180.0f);
				const float32 c = nkentseu::math::NkCos(rad), s = nkentseu::math::NkSin(rad);
				const NkVec2f h = HalfSize(it);
				const NkVec2f local[4] = {{-h.x, -h.y}, {h.x, -h.y}, {h.x, h.y}, {-h.x, h.y}};
				for (int32 i = 0; i < 4; ++i) {
					out[i] = {it.pos.x + local[i].x * c - local[i].y * s,
							  it.pos.y + local[i].x * s + local[i].y * c};
				}
			}

			// ── Sélection ───────────────────────────────────────────────────
			/// Item le plus HAUT sous le point (l'ordre du tableau est la
			/// profondeur, donc on parcourt à rebours). -1 si vide.
			int32 HitTest(const NkVec2f &worldPoint) const {
				for (int32 i = (int32)items.Size() - 1; i >= 0; --i) {
					if (Contains(items[(usize)i], worldPoint))
						return i;
				}
				return -1;
			}

			void ClearSelection() {
				for (usize i = 0; i < items.Size(); ++i)
					items[i].selected = false;
				active = -1;
			}

			void Select(int32 index, bool additive) {
				if (!additive)
					ClearSelection();
				if (index >= 0 && index < (int32)items.Size()) {
					items[(usize)index].selected = true;
					active = index;
				}
			}

			/// Sélection au rectangle (monde, non tourné) : un item entre s'il
			/// INTERSECTE le rectangle — critère PureRef, plus permissif que
			/// l'inclusion complète. Test grossier par cercle englobant puis
			/// coins (suffisant pour des images ; pas de faux négatif gênant).
			void SelectInRect(const NkVec2f &a, const NkVec2f &b, bool additive) {
				if (!additive)
					ClearSelection();
				const float32 minX = a.x < b.x ? a.x : b.x, maxX = a.x < b.x ? b.x : a.x;
				const float32 minY = a.y < b.y ? a.y : b.y, maxY = a.y < b.y ? b.y : a.y;
				for (usize i = 0; i < items.Size(); ++i) {
					NkVec2f c[4];
					Corners(items[i], c);
					float32 iMinX = c[0].x, iMaxX = c[0].x, iMinY = c[0].y, iMaxY = c[0].y;
					for (int32 k = 1; k < 4; ++k) {
						if (c[k].x < iMinX)
							iMinX = c[k].x;
						if (c[k].x > iMaxX)
							iMaxX = c[k].x;
						if (c[k].y < iMinY)
							iMinY = c[k].y;
						if (c[k].y > iMaxY)
							iMaxY = c[k].y;
					}
					if (iMaxX >= minX && iMinX <= maxX && iMaxY >= minY && iMinY <= maxY) {
						items[i].selected = true;
						active = (int32)i;
					}
				}
			}

			bool HasSelection() const {
				for (usize i = 0; i < items.Size(); ++i)
					if (items[i].selected)
						return true;
				return false;
			}

			// ── Opérations sur la sélection ─────────────────────────────────
			void MoveSelected(const NkVec2f &worldDelta) {
				for (usize i = 0; i < items.Size(); ++i) {
					if (items[i].selected) {
						items[i].pos.x += worldDelta.x;
						items[i].pos.y += worldDelta.y;
					}
				}
			}

			void MirrorSelected(bool axisX) {
				for (usize i = 0; i < items.Size(); ++i) {
					if (items[i].selected) {
						if (axisX)
							items[i].mirrorX = !items[i].mirrorX;
						else
							items[i].mirrorY = !items[i].mirrorY;
					}
				}
			}

			/// Supprime les sélectionnés. Remplit `removed` avec les INDEX
			/// supprimés (croissants) pour que la glue retire les textures
			/// correspondantes dans le même ordre.
			void RemoveSelected(NkVector<int32> &removed) {
				removed.Clear();
				for (int32 i = 0; i < (int32)items.Size(); ++i)
					if (items[(usize)i].selected)
						removed.PushBack(i);
				for (int32 k = (int32)removed.Size() - 1; k >= 0; --k)
					items.RemoveAt((usize)removed[(usize)k]);
				active = -1;
			}

			/// Monte/descend l'item actif d'un cran (échange avec le voisin).
			/// Retourne l'index échangé (-1 si rien à faire) pour que la glue
			/// fasse le MÊME échange dans son tableau de textures.
			int32 RaiseActive(bool up) {
				if (active < 0 || active >= (int32)items.Size())
					return -1;
				const int32 other = up ? active + 1 : active - 1;
				if (other < 0 || other >= (int32)items.Size())
					return -1;
				NkRefItem tmp = items[(usize)active];
				items[(usize)active] = items[(usize)other];
				items[(usize)other] = tmp;
				const int32 swappedWith = other;
				active = other;
				return swappedWith;
			}

		private:
			uint32 mNextId = 1;
	};

} // namespace nkref
