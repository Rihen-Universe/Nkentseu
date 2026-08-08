#pragma once
// =============================================================================
// NkcBoardRender — projection ECRAN du plateau. Cote ATELIER, jamais cote regle.
//
// POURQUOI CE FICHIER EST ICI ET PAS DANS LE CONTRAT
// --------------------------------------------------
// La raison d'origine etait TECHNIQUE : les modules compilaient avec trois -I et
// ne liaient rien, donc `NkRound` de NKMath — un symbole *lie* — leur etait
// interdit. CETTE RAISON N'EXISTE PLUS : l'atelier lie desormais toute la pile
// sous NKCanvas aux modules, NKMath compris.
//
// Le fichier reste ici, et c'est delibere : la vraie raison etait la seconde.
// UNE REGLE DE JEU NE CONNAIT PAS LES PIXELS. La distance entiere vit dans le
// contrat, la geometrie flottante vit cote atelier. Une frontiere qui ne tient
// que par une contrainte d'edition de liens n'est pas une frontiere ; celle-ci
// tient par ce qu'elle separe.
//
// CE QU'IL Y A DEDANS
//   NkcBoardLayout    le cadrage courant (origine, taille de cellule, topologie)
//   NkcProjector      QUI decide de la geometrie : le module, ou la topologie
//   CoordToPixel      coordonnee de jeu   -> centre de la cellule a l'ecran
//   PixelToCoord      point ecran         -> coordonnee de jeu (arrondi CUBE en hex)
//   CellPolygon       contour de la cellule (6 sommets en hex, 4 en carre)
//   FitBoard          cadrage automatique : zoome pour remplir un rectangle
//
// CONVENTION DE TAILLE — `cell` designe :
//   hexagone : le CIRCONRAYON (centre -> sommet)
//   carre    : le DEMI-COTE   (centre -> milieu d'arete)
// Dans les deux cas c'est « la moitie de la plus petite largeur visible », ce qui
// rend les cibles tactiles comparables d'une topologie a l'autre (HANDOFF §2.5).
//
// DEUX SOURCES DE GEOMETRIE, ET LE MODULE PASSE D'ABORD
// -----------------------------------------------------
// Historiquement ce fichier deduisait TOUT de `NkcTopology` : hexagone ou carre,
// point final. Le voisinage, les cases bloquees et la forme du plateau etaient
// pourtant deja l'affaire du module — seule la PROJECTION lui echappait, ce qui
// rendait impossible d'AFFICHER un plateau que ses regles savaient jouer.
//
// Depuis l'ABI 3, un module peut declarer `GetCellCenter` et `GetCellShape`
// (ConquerorRulesABI.h). Quand il le fait, `NkcProjector` les interroge et la
// topologie n'est plus qu'un repli. Consequence : triangles, octogones, cellules
// de tailles inegales, plateaux libres — tout devient affichable sans toucher a
// l'atelier, et SANS passer par un fichier JSON.
// =============================================================================

#include "Conqueror/ConquerorRulesABI.h"
// Pour le repli topologique de ProjNeighbors quand le module ne declare rien.
#include "Conqueror/ConquerorGeometry.h"

#include "NKMath/NkFunctions.h"
#include "NKGui/Core/NkGuiTypes.h"

namespace nkentseu {
	namespace conqueror {

		using nkgui::NkRect;
		using nkgui::NkVec2;

		/// racine de 3, en dur : la valeur ne depend d'aucune donnee et
		/// `NkSqrt(3)` a chaque cellule couterait un appel par sommet.
		inline constexpr float32 kSqrt3	   = 1.7320508075688772f;
		inline constexpr float32 kSqrt3Div2 = 0.8660254037844386f;

		// ---------------------------------------------------------------------
		/// Cadrage courant du plateau. Tout est derive de ces trois champs : le
		/// panneau n'a aucun autre etat de vue a maintenir.
		struct NkcBoardLayout {
				NkcTopology topology = NkcTopology::HexPointy;
				float32		cell	 = 24.f;		///< cf. convention de taille ci-dessus
				NkVec2		origin	 = {0.f, 0.f};	///< pixel de la coordonnee (0, 0)
		};

		// ---------------------------------------------------------------------
		/// Position d'une coordonnee en unites de `cell` (origine non appliquee).
		/// Isolee pour que FitBoard puisse mesurer le plateau sans le cadrer.
		inline NkVec2 CoordToUnit(NkcTopology t, NkcCoord c) noexcept {
			const float32 q = static_cast<float32>(c.q);
			const float32 r = static_cast<float32>(c.r);
			switch (t) {
				case NkcTopology::HexPointy: return {kSqrt3 * (q + 0.5f * r), 1.5f * r};
				case NkcTopology::HexFlat:	 return {1.5f * q, kSqrt3 * (r + 0.5f * q)};
				case NkcTopology::Square4:
				case NkcTopology::Square8:	 return {2.f * q, 2.f * r};
			}
			return {0.f, 0.f};
		}

		/// Demi-encombrement d'une cellule, en unites de `cell`. Sert au cadrage :
		/// sans lui, les cellules du bord depassent du panneau.
		inline NkVec2 CellHalfExtent(NkcTopology t) noexcept {
			switch (t) {
				case NkcTopology::HexPointy: return {kSqrt3Div2, 1.f};
				case NkcTopology::HexFlat:	 return {1.f, kSqrt3Div2};
				case NkcTopology::Square4:
				case NkcTopology::Square8:	 return {1.f, 1.f};
			}
			return {1.f, 1.f};
		}

		inline NkVec2 CoordToPixel(const NkcBoardLayout &L, NkcCoord c) noexcept {
			const NkVec2 u = CoordToUnit(L.topology, c);
			return {L.origin.x + u.x * L.cell, L.origin.y + u.y * L.cell};
		}

		// ---------------------------------------------------------------------
		/// Arrondi CUBE : la seule facon correcte de retrouver l'hexagone sous un
		/// point. Arrondir q et r separement donne des losanges, pas des hexagones,
		/// et le joueur clique a cote pres des aretes. On arrondit les trois
		/// coordonnees cube puis on corrige CELLE dont l'arrondi a le plus devie —
		/// c'est ce qui retablit l'invariant x + y + z = 0.
		inline NkcCoord CubeRound(float32 fq, float32 fr) noexcept {
			const float32 fx = fq;
			const float32 fz = fr;
			const float32 fy = -fx - fz;

			float32 rx = math::NkRound(fx);
			float32 ry = math::NkRound(fy);
			float32 rz = math::NkRound(fz);

			// NkAbs/NkMin existent AUSSI en macros dans NkMacros.h : on ne s'y fie
			// pas dans un header inclus par d'autres, et on compare a la main.
			const float32 dx = (rx - fx) < 0.f ? (fx - rx) : (rx - fx);
			const float32 dy = (ry - fy) < 0.f ? (fy - ry) : (ry - fy);
			const float32 dz = (rz - fz) < 0.f ? (fz - rz) : (rz - fz);

			if (dx > dy && dx > dz)		 rx = -ry - rz;
			else if (dy > dz)			 ry = -rx - rz;
			else						 rz = -rx - ry;

			NkcCoord out;
			out.q = static_cast<int16>(rx);
			out.r = static_cast<int16>(rz);
			return out;
		}

		/// Point ecran -> coordonnee de jeu. Renvoie TOUJOURS une coordonnee : c'est
		/// a l'appelant de verifier qu'elle appartient au plateau (la vue expose
		/// `coords`, une simple recherche lineaire sur 42 cases).
		inline NkcCoord PixelToCoord(const NkcBoardLayout &L, const NkVec2 &p) noexcept {
			const float32 s = L.cell > 0.0001f ? L.cell : 0.0001f;
			const float32 x = (p.x - L.origin.x) / s;
			const float32 y = (p.y - L.origin.y) / s;

			switch (L.topology) {
				case NkcTopology::HexPointy:
					return CubeRound((kSqrt3 / 3.f) * x - (1.f / 3.f) * y, (2.f / 3.f) * y);
				case NkcTopology::HexFlat:
					return CubeRound((2.f / 3.f) * x, -(1.f / 3.f) * x + (kSqrt3 / 3.f) * y);
				case NkcTopology::Square4:
				case NkcTopology::Square8: {
					NkcCoord out;
					out.q = static_cast<int16>(math::NkRound(x * 0.5f));
					out.r = static_cast<int16>(math::NkRound(y * 0.5f));
					return out;
				}
			}
			return NkcCoord{};
		}

		// ---------------------------------------------------------------------
		/// Contour de la cellule. Ecrit au plus 6 sommets dans `out`, renvoie leur
		/// nombre. `shrink` (0..1) retrecit le polygone pour laisser une gouttiere
		/// entre voisins — sans elle les contours se confondent et le plateau
		/// devient un pave illisible.
		/// FORME DESSINEE d'une cellule. PRESENTATION PURE — a ne pas confondre
		/// avec la topologie, qui decide du VOISINAGE.
		///
		/// Les deux etaient liees : une grille Square4 se dessinait forcement en
		/// carres. C'est une confusion, et elle coute cher — un plateau carre dessine
		/// en pastilles rondes se LIT tres differemment alors que les regles sont
		/// rigoureusement les memes. Les separer permet de faire varier l'un SANS
		/// l'autre, ce qui est exactement ce qu'on veut pouvoir mesurer.
		enum class NkcCellShape : uint8 {
			Auto   = 0,	 ///< deduite de la topologie — le comportement d'origine
			Square = 1,
			Hex	   = 2,
			Circle = 3	 ///< approximee par un polygone regulier
		};

		inline const char *NkcCellShapeName(NkcCellShape s) noexcept {
			switch (s) {
				case NkcCellShape::Auto:   return "Automatique";
				case NkcCellShape::Square: return "Carree";
				case NkcCellShape::Hex:	   return "Hexagonale";
				case NkcCellShape::Circle: return "Ronde";
			}
			return "?";
		}

		/// Lue depuis le JSON de plateau (`"cell_shape"`). Tolerante a la casse : un
		/// stagiaire qui ecrit "circle" ou "CIRCLE" a raison les deux fois.
		inline NkcCellShape NkcCellShapeFromName(const char *s) noexcept {
			if (!s || !*s) return NkcCellShape::Auto;
			const char c = (s[0] >= 'a' && s[0] <= 'z') ? static_cast<char>(s[0] - 32) : s[0];
			switch (c) {
				case 'S': return NkcCellShape::Square;
				case 'H': return NkcCellShape::Hex;
				case 'C':								// CIRCLE
				case 'R': return NkcCellShape::Circle;	// RONDE
				default:  return NkcCellShape::Auto;
			}
		}

		/// Sommets d'un cercle approxime. 16 suffit : au-dela la difference passe
		/// sous le pixel pour une cellule de plateau, et chaque sommet coute un
		/// triangle a l'eventail de remplissage de NkGui.
		inline constexpr int32 kCircleSegments = 16;

		/// Nombre maximal de sommets qu'un contour peut produire — dimensionne les
		/// tampons des panneaux.
		inline constexpr int32 kMaxPolyPoints = kCircleSegments;

		// ---------------------------------------------------------------------
		inline int32 CellPolygonShaped(const NkcBoardLayout &L, NkcCoord c, NkVec2 *out,
									   NkcCellShape shape, float32 shrink = 0.94f) noexcept {
			if (!out) return 0;
			const NkVec2  ctr = CoordToPixel(L, c);
			const float32 R	  = L.cell * shrink;

			if (shape == NkcCellShape::Circle) {
				for (int32 i = 0; i < kCircleSegments; ++i) {
					const float32 a = 6.2831853f * static_cast<float32>(i) /
									  static_cast<float32>(kCircleSegments);
					out[i] = {ctr.x + math::NkCos(a) * R, ctr.y + math::NkSin(a) * R};
				}
				return kCircleSegments;
			}
			if (shape == NkcCellShape::Square) {
				out[0] = {ctr.x - R, ctr.y - R};
				out[1] = {ctr.x + R, ctr.y - R};
				out[2] = {ctr.x + R, ctr.y + R};
				out[3] = {ctr.x - R, ctr.y + R};
				return 4;
			}
			if (shape == NkcCellShape::Hex) {
				// Orientation alignee sur la topologie quand elle est hexagonale ;
				// pointy sinon, car un hexagone pose sur une grille carree n'a aucune
				// raison de pencher d'un cote plutot que de l'autre.
				if (L.topology == NkcTopology::HexFlat) {
					static const float32 kx[6] = {1.f, 0.5f, -0.5f, -1.f, -0.5f, 0.5f};
					static const float32 ky[6] = {0.f, kSqrt3Div2, kSqrt3Div2, 0.f, -kSqrt3Div2, -kSqrt3Div2};
					for (int32 i = 0; i < 6; ++i) out[i] = {ctr.x + kx[i] * R, ctr.y + ky[i] * R};
				} else {
					static const float32 kx[6] = {0.f, -kSqrt3Div2, -kSqrt3Div2, 0.f, kSqrt3Div2, kSqrt3Div2};
					static const float32 ky[6] = {-1.f, -0.5f, 0.5f, 1.f, 0.5f, -0.5f};
					for (int32 i = 0; i < 6; ++i) out[i] = {ctr.x + kx[i] * R, ctr.y + ky[i] * R};
				}
				return 6;
			}

			switch (L.topology) {
				case NkcTopology::HexPointy: {
					// Sommet en haut : angles 90, 150, 210, 270, 330, 30 degres.
					static const float32 kx[6] = {0.f, -kSqrt3Div2, -kSqrt3Div2, 0.f, kSqrt3Div2, kSqrt3Div2};
					static const float32 ky[6] = {-1.f, -0.5f, 0.5f, 1.f, 0.5f, -0.5f};
					for (int32 i = 0; i < 6; ++i) out[i] = {ctr.x + kx[i] * R, ctr.y + ky[i] * R};
					return 6;
				}
				case NkcTopology::HexFlat: {
					// Sommet a gauche/droite : angles 0, 60, 120, 180, 240, 300.
					static const float32 kx[6] = {1.f, 0.5f, -0.5f, -1.f, -0.5f, 0.5f};
					static const float32 ky[6] = {0.f, kSqrt3Div2, kSqrt3Div2, 0.f, -kSqrt3Div2, -kSqrt3Div2};
					for (int32 i = 0; i < 6; ++i) out[i] = {ctr.x + kx[i] * R, ctr.y + ky[i] * R};
					return 6;
				}
				case NkcTopology::Square4:
				case NkcTopology::Square8: {
					out[0] = {ctr.x - R, ctr.y - R};
					out[1] = {ctr.x + R, ctr.y - R};
					out[2] = {ctr.x + R, ctr.y + R};
					out[3] = {ctr.x - R, ctr.y + R};
					return 4;
				}
			}
			return 0;
		}

		/// Compatibilite : le contour « naturel » de la topologie.
		inline int32 CellPolygon(const NkcBoardLayout &L, NkcCoord c, NkVec2 *out,
								 float32 shrink = 0.94f) noexcept {
			return CellPolygonShaped(L, c, out, NkcCellShape::Auto, shrink);
		}

		// ---------------------------------------------------------------------
		/// Cadrage automatique (HANDOFF §2.2) : calcule la boite englobante REELLE
		/// des cellules et zoome pour remplir `area`, marge comprise. Recalculer a
		/// chaque frame coute 42 additions — bien moins qu'un etat de vue a
		/// invalider au redimensionnement.
		///
		/// `minCell` impose un rayon plancher (cibles tactiles Android >= 44 px
		/// logiques) : au-dela, le plateau deborde et c'est a l'appelant de le
		/// rendre defilable.
		inline NkcBoardLayout FitBoard(NkcTopology topology, const NkcCoord *coords, uint32 count,
									   const NkRect &area, float32 margin = 0.08f,
									   float32 minCell = 0.f) noexcept {
			NkcBoardLayout L;
			L.topology = topology;
			if (!coords || count == 0 || area.w <= 1.f || area.h <= 1.f) {
				L.cell	 = minCell > 0.f ? minCell : 24.f;
				L.origin = {area.x + area.w * 0.5f, area.y + area.h * 0.5f};
				return L;
			}

			const NkVec2 half = CellHalfExtent(topology);
			NkVec2		 lo	  = CoordToUnit(topology, coords[0]);
			NkVec2		 hi	  = lo;
			for (uint32 i = 1; i < count; ++i) {
				const NkVec2 u = CoordToUnit(topology, coords[i]);
				if (u.x < lo.x) lo.x = u.x;
				if (u.y < lo.y) lo.y = u.y;
				if (u.x > hi.x) hi.x = u.x;
				if (u.y > hi.y) hi.y = u.y;
			}
			lo.x -= half.x; lo.y -= half.y;
			hi.x += half.x; hi.y += half.y;

			const float32 spanX = hi.x - lo.x > 0.001f ? hi.x - lo.x : 0.001f;
			const float32 spanY = hi.y - lo.y > 0.001f ? hi.y - lo.y : 0.001f;
			const float32 usableW = area.w * (1.f - 2.f * margin);
			const float32 usableH = area.h * (1.f - 2.f * margin);

			const float32 byW = usableW / spanX;
			const float32 byH = usableH / spanY;
			L.cell = byW < byH ? byW : byH;
			if (L.cell < minCell) L.cell = minCell;
			if (L.cell < 1.f)	  L.cell = 1.f;

			// Origine : on centre la boite englobante dans `area`.
			const float32 midX = (lo.x + hi.x) * 0.5f;
			const float32 midY = (lo.y + hi.y) * 0.5f;
			L.origin = {area.x + area.w * 0.5f - midX * L.cell,
						area.y + area.h * 0.5f - midY * L.cell};
			return L;
		}

		// =====================================================================
		// LE PROJECTEUR — qui decide de la geometrie
		//
		// Il ne porte aucun etat : c'est un couple (vtable, instance) plus un
		// drapeau, reconstruit a chaque frame. Le rendre persistant obligerait a
		// l'invalider quand le module change, et c'est exactement le genre
		// d'invalidation qu'on oublie.
		// =====================================================================
		struct NkcProjector {
				const NkcRulesVTable *vt	   = nullptr;
				NkcRules			  inst	   = nullptr;
				NkcTopology			  topology = NkcTopology::HexPointy;
				bool				  custom   = false;	 ///< le module declare sa geometrie
				/// Forme demandee : JSON du plateau, ou choix de l'utilisateur.
				/// IGNOREE quand le module fournit GetCellShape — un module qui
				/// dessine ses propres cellules a le dernier mot sur SA geometrie.
				NkcCellShape		  shape	   = NkcCellShape::Auto;
		};

		inline NkcProjector NkcMakeProjector(const NkcRulesVTable &vt, NkcRules inst,
											 NkcTopology topology,
											 NkcCellShape shape = NkcCellShape::Auto) noexcept {
			NkcProjector p;
			p.vt	   = &vt;
			p.inst	   = inst;
			p.topology = topology;
			p.shape	   = shape;
			// « custom » = le module sait placer ses cellules. La FORME seule ne
			// suffit pas : sans centre, on ne saurait pas ou la poser.
			p.custom = (inst != nullptr) && (vt.GetCellCenter != nullptr);
			return p;
		}

		/// Centre d'une cellule, en unites de cellule. Le module d'abord, la
		/// topologie ensuite — et cellule par cellule : un module peut ne savoir
		/// placer QUE certaines cases et laisser l'atelier deduire les autres.
		inline NkVec2 ProjUnitCenter(const NkcProjector &p, NkcCoord c) noexcept {
			if (p.custom) {
				float32 xy[2] = {0.f, 0.f};
				if (p.vt->GetCellCenter(p.inst, c, xy)) return {xy[0], xy[1]};
			}
			return CoordToUnit(p.topology, c);
		}

		inline NkVec2 ProjPixelCenter(const NkcProjector &p, const NkcBoardLayout &L,
									  NkcCoord c) noexcept {
			const NkVec2 u = ProjUnitCenter(p, c);
			return {L.origin.x + u.x * L.cell, L.origin.y + u.y * L.cell};
		}

		/// Voisins d'une cellule, tels que LE MODULE les declare — repli sur la
		/// topologie s'il ne les declare pas.
		///
		/// L'atelier n'a jamais besoin du voisinage pour JOUER : les coups viennent
		/// de GenerateLegalMoves, la menace d'une simulation. Il en a besoin pour le
		/// MONTRER, et c'est tout l'interet : sans affichage, un stagiaire dont
		/// l'adjacence est fausse ne le decouvre que par un coup legal inexplicable,
		/// trois heures plus tard.
		inline uint32 ProjNeighbors(const NkcProjector &p, NkcTopology fallback,
									NkcCoord c, NkcCoord *out, uint32 cap) noexcept {
			if (!out || cap == 0) return 0;
			if (p.inst && p.vt && p.vt->GetNeighbors)
				return p.vt->GetNeighbors(p.inst, c, out, cap);

			const int32 n = NeighborCount(fallback);
			uint32		w = 0;
			for (int32 i = 0; i < n; ++i) {
				if (w >= cap) break;
				out[w++] = Neighbor(fallback, c, i);
			}
			return static_cast<uint32>(n);
		}

		/// Contour d'une cellule a l'ecran. Interroge `GetCellShape` s'il existe,
		/// sinon retombe sur la forme de la topologie.
		inline int32 ProjCellPolygon(const NkcProjector &p, const NkcBoardLayout &L,
									 NkcCoord c, NkVec2 *out, int32 cap,
									 float32 shrink = 0.94f) noexcept {
			if (!out || cap < 3) return 0;

			if (p.inst && p.vt->GetCellShape) {
				float32		 raw[kMaxCellPoints * 2] = {};
				const uint32 want = static_cast<uint32>(cap < static_cast<int32>(kMaxCellPoints)
														   ? cap
														   : static_cast<int32>(kMaxCellPoints));
				const uint32 n = p.vt->GetCellShape(p.inst, c, raw, want);
				if (n >= 3) {
					const NkVec2  ctr = ProjPixelCenter(p, L, c);
					const float32 R	  = L.cell * shrink;
					const int32	  m	  = static_cast<int32>(n < want ? n : want);
					for (int32 i = 0; i < m; ++i)
						out[i] = {ctr.x + raw[i * 2] * R, ctr.y + raw[i * 2 + 1] * R};
					return m;
				}
			}

			// Le module ne dit rien : on applique la forme DEMANDEE (JSON du
			// plateau ou choix de l'utilisateur), qui retombe elle-meme sur la
			// topologie quand elle vaut Auto.
			//
			// On ne peut pas appeler CellPolygonShaped tel quel : le centre peut
			// venir du module et pas de la topologie, donc on calcule le contour
			// autour de l'origine puis on le translate sur le vrai centre.
			NkVec2		  tmp[kMaxPolyPoints];
			NkcBoardLayout at = L;
			at.origin		  = {0.f, 0.f};
			const int32 n	  = CellPolygonShaped(at, c, tmp, p.shape, shrink);
			if (n <= 0 || n > cap) return 0;
			const NkVec2 topoCtr = CoordToPixel(at, c);
			const NkVec2 realCtr = ProjPixelCenter(p, L, c);
			for (int32 i = 0; i < n; ++i)
				out[i] = {realCtr.x + (tmp[i].x - topoCtr.x), realCtr.y + (tmp[i].y - topoCtr.y)};
			return n;
		}

		/// Demi-encombrement d'une cellule, en unites — mesure REELLE quand le
		/// module donne sa forme, sinon celle de la topologie. Sert au cadrage :
		/// sans elle, les cellules du bord depassent du panneau.
		inline NkVec2 ProjHalfExtent(const NkcProjector &p, NkcCoord c) noexcept {
			if (p.inst && p.vt->GetCellShape) {
				float32		 raw[kMaxCellPoints * 2] = {};
				const uint32 n = p.vt->GetCellShape(p.inst, c, raw, kMaxCellPoints);
				if (n >= 3) {
					NkVec2 h = {0.f, 0.f};
					for (uint32 i = 0; i < n && i < kMaxCellPoints; ++i) {
						const float32 ax = raw[i * 2] < 0.f ? -raw[i * 2] : raw[i * 2];
						const float32 ay = raw[i * 2 + 1] < 0.f ? -raw[i * 2 + 1] : raw[i * 2 + 1];
						if (ax > h.x) h.x = ax;
						if (ay > h.y) h.y = ay;
					}
					if (h.x > 0.f && h.y > 0.f) return h;
				}
			}
			return CellHalfExtent(p.topology);
		}

		/// Cadrage automatique qui respecte la geometrie du module.
		inline NkcBoardLayout ProjFitBoard(const NkcProjector &p, const NkcCoord *coords,
										   uint32 count, const NkRect &area,
										   float32 margin = 0.08f,
										   float32 minCell = 0.f) noexcept {
			if (!p.custom && (!p.inst || !p.vt->GetCellShape))
				return FitBoard(p.topology, coords, count, area, margin, minCell);

			NkcBoardLayout L;
			L.topology = p.topology;
			if (!coords || count == 0 || area.w <= 1.f || area.h <= 1.f) {
				L.cell	 = minCell > 0.f ? minCell : 24.f;
				L.origin = {area.x + area.w * 0.5f, area.y + area.h * 0.5f};
				return L;
			}

			// Boite englobante REELLE : centre + demi-encombrement, cellule par
			// cellule — une grille libre n'a aucune raison d'etre homogene.
			NkVec2 lo = {1.0e30f, 1.0e30f};
			NkVec2 hi = {-1.0e30f, -1.0e30f};
			for (uint32 i = 0; i < count; ++i) {
				const NkVec2 u = ProjUnitCenter(p, coords[i]);
				const NkVec2 h = ProjHalfExtent(p, coords[i]);
				if (u.x - h.x < lo.x) lo.x = u.x - h.x;
				if (u.y - h.y < lo.y) lo.y = u.y - h.y;
				if (u.x + h.x > hi.x) hi.x = u.x + h.x;
				if (u.y + h.y > hi.y) hi.y = u.y + h.y;
			}

			const float32 spanX	  = hi.x - lo.x > 0.001f ? hi.x - lo.x : 0.001f;
			const float32 spanY	  = hi.y - lo.y > 0.001f ? hi.y - lo.y : 0.001f;
			const float32 usableW = area.w * (1.f - 2.f * margin);
			const float32 usableH = area.h * (1.f - 2.f * margin);
			const float32 byW	  = usableW / spanX;
			const float32 byH	  = usableH / spanY;

			L.cell = byW < byH ? byW : byH;
			if (L.cell < minCell) L.cell = minCell;
			if (L.cell < 1.f)	  L.cell = 1.f;

			const float32 midX = (lo.x + hi.x) * 0.5f;
			const float32 midY = (lo.y + hi.y) * 0.5f;
			L.origin = {area.x + area.w * 0.5f - midX * L.cell,
						area.y + area.h * 0.5f - midY * L.cell};
			return L;
		}

		/// Cellule sous le curseur. Renvoie son INDEX dans `coords`, ou -1.
		///
		/// Deux chemins, et c'est assume :
		///   - geometrie standard : inversion analytique en O(1) (arrondi cube),
		///     exacte jusqu'aux aretes ;
		///   - geometrie du module : aucune inverse n'existe, on cherche le centre
		///     le plus proche parmi les cases. O(n) sur quelques dizaines de cases
		///     ne se mesure pas, et c'est la seule methode qui marche pour une
		///     grille quelconque.
		inline int32 ProjPickCell(const NkcProjector &p, const NkcBoardLayout &L,
								  const NkcCoord *coords, uint32 count,
								  const NkVec2 &pos) noexcept {
			if (!coords || count == 0) return -1;

			if (!p.custom) {
				const NkcCoord c = PixelToCoord(L, pos);
				for (uint32 i = 0; i < count; ++i)
					if (CoordEqual(coords[i], c)) return static_cast<int32>(i);
				return -1;
			}

			int32	best  = -1;
			float32 bestD = 1.0e30f;
			for (uint32 i = 0; i < count; ++i) {
				const NkVec2  ctr = ProjPixelCenter(p, L, coords[i]);
				const float32 dx  = pos.x - ctr.x;
				const float32 dy  = pos.y - ctr.y;
				const float32 d2  = dx * dx + dy * dy;
				if (d2 < bestD) { bestD = d2; best = static_cast<int32>(i); }
			}
			// Hors de toute cellule : on refuse plutot que de designer la plus
			// proche a l'autre bout du plateau.
			const float32 reach = L.cell * 1.05f;
			return (best >= 0 && bestD <= reach * reach) ? best : -1;
		}

		/// Rectangle reellement occupe par le plateau une fois cadre — sert au
		/// bandeau de score (le poser au-dessus du plateau, pas au-dessus du vide)
		/// et au defilement quand `minCell` a force un debordement.
		inline NkRect BoardPixelBounds(const NkcBoardLayout &L, const NkcCoord *coords,
									   uint32 count) noexcept {
			if (!coords || count == 0) return NkRect{L.origin.x, L.origin.y, 0.f, 0.f};
			const NkVec2 half = CellHalfExtent(L.topology);
			NkVec2		 lo	  = CoordToUnit(L.topology, coords[0]);
			NkVec2		 hi	  = lo;
			for (uint32 i = 1; i < count; ++i) {
				const NkVec2 u = CoordToUnit(L.topology, coords[i]);
				if (u.x < lo.x) lo.x = u.x;
				if (u.y < lo.y) lo.y = u.y;
				if (u.x > hi.x) hi.x = u.x;
				if (u.y > hi.y) hi.y = u.y;
			}
			const float32 x0 = L.origin.x + (lo.x - half.x) * L.cell;
			const float32 y0 = L.origin.y + (lo.y - half.y) * L.cell;
			const float32 x1 = L.origin.x + (hi.x + half.x) * L.cell;
			const float32 y1 = L.origin.y + (hi.y + half.y) * L.cell;
			return NkRect{x0, y0, x1 - x0, y1 - y0};
		}

	} // namespace conqueror
} // namespace nkentseu
