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
			float32 opacity = 1.0f; ///< opacité PAR IMAGE (propriété PureRef, Étape 3)
			bool mirrorX = false;
			bool mirrorY = false;
			uint32 texW = 0; ///< taille native de l'image (pixels source)
			uint32 texH = 0;
			bool selected = false;
			NkString sourcePath; ///< vide = collée du presse-papiers (Étape 2 : octets embarqués)
	};

	/// Un TRAIT DE CRAYON posé sur la planche (annotation libre, demande Rihen :
	/// « des marques au crayon coloré, n'importe quelle marque »). Les points et
	/// l'épaisseur vivent en MONDE : le trait zoome avec la planche, comme une
	/// image. Sérialisé dans le .nkref à l'Étape 2.
	struct NkRefStroke {
			nkentseu::uint8 r = 247, g = 154, b = 40, a = 255; ///< couleur (défaut orange Rihen)
			float32 widthWorld = 3.0f;						   ///< épaisseur en unités monde
			NkVector<NkVec2f> points;						   ///< polyligne monde
	};

	class NkRefBoard {
		public:
			NkVector<NkRefItem> items; ///< ordre = profondeur (dernier = dessus)
			NkVector<NkRefStroke> strokes; ///< annotations crayon, par-dessus les images

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

			// ── « Pack » : rangement compact (le rituel PureRef, Ctrl+P) ────
			// V1 en ÉTAGÈRES : AABB de chaque item (rotation comprise), tri par
			// hauteur décroissante, remplissage ligne par ligne vers une rangée
			// cible ~carrée, puis recentrage du bloc sur le CENTROÏDE d'origine
			// (on range SUR PLACE : la vue n'a pas à sauter). S'applique à la
			// sélection si elle compte ≥ 2 items, sinon à toute la planche.
			void Pack(float32 gap) {
				NkVector<int32> idx;
				int32 selCount = 0;
				for (usize i = 0; i < items.Size(); ++i)
					if (items[i].selected)
						++selCount;
				const bool useSel = selCount >= 2;
				for (usize i = 0; i < items.Size(); ++i)
					if (!useSel || items[i].selected)
						idx.PushBack((int32)i);
				if (idx.Size() < 2)
					return;

				// AABB (monde) de chaque item + centroïde + aire totale.
				NkVector<NkVec2f> dims;
				NkVec2f centroid{0.0f, 0.0f};
				float32 area = 0.0f, widest = 0.0f;
				for (usize k = 0; k < idx.Size(); ++k) {
					const NkRefItem &it = items[(usize)idx[k]];
					NkVec2f c[4];
					Corners(it, c);
					float32 minX = c[0].x, maxX = c[0].x, minY = c[0].y, maxY = c[0].y;
					for (int32 j = 1; j < 4; ++j) {
						if (c[j].x < minX)
							minX = c[j].x;
						if (c[j].x > maxX)
							maxX = c[j].x;
						if (c[j].y < minY)
							minY = c[j].y;
						if (c[j].y > maxY)
							maxY = c[j].y;
					}
					const NkVec2f d{maxX - minX, maxY - minY};
					dims.PushBack(d);
					centroid.x += it.pos.x;
					centroid.y += it.pos.y;
					area += d.x * d.y;
					if (d.x > widest)
						widest = d.x;
				}
				centroid.x /= (float32)idx.Size();
				centroid.y /= (float32)idx.Size();

				// Tri par hauteur décroissante (insertion — quelques dizaines
				// d'images, inutile de sortir l'artillerie).
				for (usize a = 1; a < idx.Size(); ++a) {
					const int32 vi = idx[a];
					const NkVec2f vd = dims[a];
					usize b = a;
					while (b > 0 && dims[b - 1].y < vd.y) {
						idx[b] = idx[b - 1];
						dims[b] = dims[b - 1];
						--b;
					}
					idx[b] = vi;
					dims[b] = vd;
				}

				// Largeur de rangée cible : viser un BLOC ~carré en NOMBRE
				// d'images — somme des largeurs (gaps compris) répartie sur
				// ceil(sqrt(n)) rangées. L'ancienne cible sqrt(aire totale)
				// s'effondrait dès que les images étaient larges : chaque
				// étagère ne recevait qu'une image → une COLONNE (retour de
				// Rihen, 2026-08-11). Jamais plus étroit que l'item le plus large.
				(void)area;
				float32 totalW = 0.0f;
				for (usize k = 0; k < dims.Size(); ++k)
					totalW += dims[k].x;
				totalW += gap * (float32)(idx.Size() - 1);
				const float32 rows = nkentseu::math::NkCeil(nkentseu::math::NkSqrt((float32)idx.Size()));
				float32 rowWidth = (totalW / rows) * 1.02f; // marge : le dernier de rangée + gap
				if (rowWidth < widest)
					rowWidth = widest;

				// Remplissage en étagères, coin haut-gauche du bloc à (0,0)
				// provisoirement ; le recentrage vient après.
				float32 x = 0.0f, y = 0.0f, shelfH = 0.0f;
				float32 blockMaxX = 0.0f, blockMaxY = 0.0f;
				NkVector<NkVec2f> newPos;
				for (usize k = 0; k < idx.Size(); ++k) {
					const NkVec2f d = dims[k];
					if (x > 0.0f && x + d.x > rowWidth) { // étagère suivante
						x = 0.0f;
						y += shelfH + gap;
						shelfH = 0.0f;
					}
					newPos.PushBack({x + d.x * 0.5f, y + d.y * 0.5f}); // centre de l'AABB
					x += d.x + gap;
					if (d.y > shelfH)
						shelfH = d.y;
					if (x - gap > blockMaxX)
						blockMaxX = x - gap;
					if (y + d.y > blockMaxY)
						blockMaxY = y + d.y;
				}

				// Recentrage : le centre du bloc rangé retombe sur le centroïde.
				const NkVec2f shift{centroid.x - blockMaxX * 0.5f, centroid.y - blockMaxY * 0.5f};
				for (usize k = 0; k < idx.Size(); ++k)
					items[(usize)idx[k]].pos = {newPos[k].x + shift.x, newPos[k].y + shift.y};
			}

			// ── Crayon ──────────────────────────────────────────────────────
			void BeginStroke(nkentseu::uint8 r, nkentseu::uint8 g, nkentseu::uint8 b, float32 widthWorld,
							 const NkVec2f &worldPoint) {
				NkRefStroke s;
				s.r = r;
				s.g = g;
				s.b = b;
				s.widthWorld = widthWorld;
				s.points.PushBack(worldPoint);
				strokes.PushBack(s);
			}

			/// Ajoute un point au trait EN COURS (le dernier), en ignorant les
			/// micro-mouvements (minDist en monde) — sinon un glisser lent pond
			/// des milliers de segments inutiles.
			void AppendStrokePoint(const NkVec2f &worldPoint, float32 minDist) {
				if (strokes.Empty())
					return;
				NkRefStroke &s = strokes[strokes.Size() - 1];
				if (!s.points.Empty()) {
					const NkVec2f &last = s.points[s.points.Size() - 1];
					const float32 dx = worldPoint.x - last.x, dy = worldPoint.y - last.y;
					if (dx * dx + dy * dy < minDist * minDist)
						return;
				}
				s.points.PushBack(worldPoint);
			}

			void UndoStroke() {
				if (!strokes.Empty())
					strokes.RemoveAt(strokes.Size() - 1);
			}

			void ClearStrokes() {
				strokes.Clear();
			}

		private:
			uint32 mNextId = 1;
	};

} // namespace nkref
