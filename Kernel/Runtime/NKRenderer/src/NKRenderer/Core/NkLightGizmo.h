#pragma once
// =============================================================================
// NkLightGizmo.h — REPRESENTATION VISUELLE DES LUMIERES (facon Blender)
//
// PROBLEME RESOLU
//   Une lumiere eclaire mais ne se voit pas : sans marqueur a l'ecran, elle n'est
//   ni cliquable, ni deplacable, ni orientable. On ne peut regler que ce qu'on
//   voit. Ce module dessine, pour chaque type, un widget qui montre les
//   parametres REELS de la lumiere — pas une icone decorative :
//
//     POINT         petite sphere pleine + cercle en pointilles au rayon `range`
//     DIRECTIONNEL  disque + rayons PARALLELES dans la direction (un soleil n'a
//                   pas de position utile : seule sa direction compte)
//     SPOT          apex plein + cone au VRAI `outerAngle`, cone interne en
//                   pointilles au `innerAngle`, longueur = `range`
//     AREA          rectangle a ses DIMENSIONS reelles + rayon normal
//
//   Le widget est donc lisible comme une mesure : agrandir le cercle d'une
//   ponctuelle, c'est voir sa portee changer.
//
// POURQUOI ICI ET PAS DANS LA DEMO
//   NK3DModeler, NkAnima et l'editeur Noge ont tous besoin des memes widgets.
//   Header-only, sans etat, sans dependance au peripherique : on passe deux
//   callbacks de dessin (meme convention que NkGizmo3D::Draw), l'appelant les
//   branche sur DrawDebugLine / DrawDebugTriangle.
//
// TRANSFORMATIONS QUI ONT UN SENS (cf. LightScaleMeaning)
//   Deplacer  : toutes SAUF la directionnelle (sa position n'influence rien).
//   Orienter  : directionnelle, spot, area. Une ponctuelle est isotrope :
//               la faire tourner ne change RIEN, et proposer la rotation
//               mentirait sur l'effet obtenu.
//   Redimensionner : area (ses dimensions) et spot/point (leur portee). Pas la
//               directionnelle. On ne propose donc pas un scale uniforme partout
//               « pour faire comme les objets » : chaque axe de manipulation doit
//               correspondre a un parametre reel.
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"

namespace nkentseu {
	namespace renderer {

		// Ce qu'une poignee d'echelle signifie pour un type de lumiere donne.
		// Sert a l'editeur pour n'exposer que les manipulations qui ont un effet.
		enum class NkLightScaleMeaning : uint8 {
			None = 0,	// aucune (directionnelle)
			Range,		// portee (ponctuelle, spot)
			Dimensions, // largeur/hauteur (area)
		};

		class NkLightGizmo {
			public:
				// Couleurs : jaune pour une lumiere non selectionnee, teinte plus claire
				// pour l'active — meme grammaire que le lisere de selection des objets,
				// pour ne pas avoir a apprendre deux codes couleur.
				static NkVec4f ColorFor(bool selected, bool active) {
					if (active)
						return {1.f, 0.94f, 0.62f, 1.f};
					if (selected)
						return {1.f, 0.78f, 0.15f, 1.f};
					return {0.62f, 0.58f, 0.30f, 1.f};
				}

				static NkLightScaleMeaning ScaleMeaning(NkLightType t) {
					switch (t) {
						case NkLightType::NK_DIRECTIONAL: return NkLightScaleMeaning::None;
						case NkLightType::NK_AREA: return NkLightScaleMeaning::Dimensions;
						default: return NkLightScaleMeaning::Range;
					}
				}

				static bool CanTranslate(NkLightType t) {
					// Une directionnelle n'a pas de position utile : le shader n'utilise que
					// sa direction. La deplacer donnerait un retour visuel sans aucun effet
					// sur l'image — donc on ne le propose pas.
					return t != NkLightType::NK_DIRECTIONAL;
				}

				static bool CanRotate(NkLightType t) {
					// Une ponctuelle est ISOTROPE : la tourner ne change rien.
					return t != NkLightType::NK_POINT;
				}

				// Rayon a l'ecran du corps du widget, en unites monde, pour que le
				// marqueur garde une taille lisible quelle que soit la distance.
				// L'appelant fournit la distance camera->lumiere ; a defaut, passer 0
				// pour une taille fixe en monde.
				static float32 BodyRadius(float32 camDistance) {
					if (camDistance <= 0.f)
						return 0.09f;
					// ~1,6 % de la distance : reste visible de loin sans ecraser la scene
					// de pres. Borne pour ne jamais devenir un point ni un ballon.
					const float32 r = camDistance * 0.016f;
					return r < 0.05f ? 0.05f : (r > 0.6f ? 0.6f : r);
				}

				// ── DESSIN ──────────────────────────────────────────────────────────
				// drawLine(a, b, color) et drawTri(a, b, c, color) : meme convention que
				// NkGizmo3D::Draw. camDistance sert au dimensionnement ecran-constant.
				template <class DrawLine, class DrawTri>
				static void Draw(const NkLightDesc &L, bool selected, bool active, float32 camDistance,
								 DrawLine drawLine, DrawTri drawTri) {
					const NkVec4f col = ColorFor(selected, active);
					const float32 r = BodyRadius(camDistance);
					switch (L.type) {
						case NkLightType::NK_DIRECTIONAL: DrawSun(L, col, r, drawLine, drawTri); break;
						case NkLightType::NK_SPOT: DrawSpot(L, col, r, selected, drawLine, drawTri); break;
						case NkLightType::NK_AREA: DrawArea(L, col, r, drawLine, drawTri); break;
						default: DrawPoint(L, col, r, selected, drawLine, drawTri); break;
					}
				}

			private:
				// Base orthonormee autour d'une direction. `up` de secours choisi hors
				// axe pour eviter un produit vectoriel nul quand la direction est
				// verticale — sinon la base degenere et le cone s'aplatit.
				static void Basis(NkVec3f d, NkVec3f &x, NkVec3f &y) {
					const NkVec3f up = (d.y > 0.99f || d.y < -0.99f) ? NkVec3f{1.f, 0.f, 0.f} : NkVec3f{0.f, 1.f, 0.f};
					x = up.Cross(d);
					const float32 l = x.Len();
					x = (l > 1e-5f) ? x * (1.f / l) : NkVec3f{1.f, 0.f, 0.f};
					y = d.Cross(x);
					const float32 l2 = y.Len();
					y = (l2 > 1e-5f) ? y * (1.f / l2) : NkVec3f{0.f, 0.f, 1.f};
				}

				// Petit corps PLEIN (octaedre) : lisible a toute distance, et distinct du
				// fil de fer des maillages — on voit tout de suite que ce n'est pas
				// de la geometrie de scene.
				template <class DrawTri> static void Body(NkVec3f c, float32 r, NkVec4f col, DrawTri drawTri) {
					const NkVec3f px = {c.x + r, c.y, c.z}, nx = {c.x - r, c.y, c.z};
					const NkVec3f py = {c.x, c.y + r, c.z}, ny = {c.x, c.y - r, c.z};
					const NkVec3f pz = {c.x, c.y, c.z + r}, nz = {c.x, c.y, c.z - r};
					drawTri(px, py, pz, col);
					drawTri(py, nx, pz, col);
					drawTri(nx, ny, pz, col);
					drawTri(ny, px, pz, col);
					drawTri(py, px, nz, col);
					drawTri(nx, py, nz, col);
					drawTri(ny, nx, nz, col);
					drawTri(px, ny, nz, col);
				}

				// Cercle dans le plan (x, y) autour de c. `dashed` saute un segment sur
				// deux : c'est ainsi que Blender distingue une LIMITE (portee, angle
				// interne) d'une arete reelle.
				template <class DrawLine>
				static void Circle(NkVec3f c, NkVec3f x, NkVec3f y, float32 rad, NkVec4f col, bool dashed,
								   DrawLine drawLine) {
					const int32 N = 32;
					NkVec3f prev{};
					for (int32 i = 0; i <= N; i++) {
						const float32 a = 6.2831853f * (float32)i / (float32)N;
						const NkVec3f p = c + x * (cosf(a) * rad) + y * (sinf(a) * rad);
						if (i > 0 && (!dashed || (i & 1)))
							drawLine(prev, p, col);
						prev = p;
					}
				}

				template <class DrawLine, class DrawTri>
				static void DrawPoint(const NkLightDesc &L, NkVec4f col, float32 r, bool selected, DrawLine drawLine,
									  DrawTri drawTri) {
					Body(L.position, r, col, drawTri);
					// Petites branches : identifient une ponctuelle (rayonnement isotrope)
					// sans encombrer. Longueur proportionnelle au corps, donc constante a
					// l'ecran.
					const NkVec3f X{1.f, 0.f, 0.f}, Y{0.f, 1.f, 0.f}, Z{0.f, 0.f, 1.f};
					const NkVec3f dirs[6] = {X, {-1.f, 0.f, 0.f}, Y, {0.f, -1.f, 0.f}, Z, {0.f, 0.f, -1.f}};
					for (int32 i = 0; i < 6; i++)
						drawLine(L.position + dirs[i] * (r * 1.4f), L.position + dirs[i] * (r * 2.6f), col);
					// PORTEE : dessinee SEULEMENT quand la lumiere est selectionnee.
					// Toujours l'afficher noyait la scene — avec range=10, trois cercles
					// en pointilles produisent une centaine de tirets sur tout l'ecran.
					// Blender fait le meme choix : le rayon d'influence n'apparait qu'a la
					// selection, quand on cherche justement a le regler.
					if (selected) {
						const float32 rng = L.range > 0.f ? L.range : 1.f;
						Circle(L.position, X, Y, rng, col, true, drawLine);
						Circle(L.position, Y, Z, rng, col, true, drawLine);
						Circle(L.position, Z, X, rng, col, true, drawLine);
					}
				}

				template <class DrawLine, class DrawTri>
				static void DrawSun(const NkLightDesc &L, NkVec4f col, float32 r, DrawLine drawLine, DrawTri drawTri) {
					NkVec3f d = L.direction;
					const float32 dl = d.Len();
					d = (dl > 1e-5f) ? d * (1.f / dl) : NkVec3f{0.f, -1.f, 0.f};
					// Une directionnelle n'a pas de position dans le shader ; on ancre donc
					// le widget sur `position` s'il est renseigne, sinon a l'origine, et on
					// le documente : ce n'est qu'un point d'accrochage visuel.
					const NkVec3f c = L.position;
					Body(c, r, col, drawTri);
					NkVec3f x, y;
					Basis(d, x, y);
					Circle(c, x, y, r * 2.2f, col, false, drawLine);
					// Rayons PARALLELES : c'est ce qui dit « directionnelle ». Des rayons
					// divergents signifieraient une ponctuelle. Courts (2,5x le corps) :
					// a r*6 ils balayaient la moitie de l'ecran et se lisaient comme du
					// bruit, pas comme un soleil.
					const int32 N = 8;
					const float32 len = r * 2.5f;
					for (int32 i = 0; i < N; i++) {
						const float32 a = 6.2831853f * (float32)i / (float32)N;
						const NkVec3f o = c + x * (cosf(a) * r * 2.2f) + y * (sinf(a) * r * 2.2f);
						drawLine(o, o + d * len, col);
					}
				}

				template <class DrawLine, class DrawTri>
				static void DrawSpot(const NkLightDesc &L, NkVec4f col, float32 r, bool selected, DrawLine drawLine,
									 DrawTri drawTri) {
					NkVec3f d = L.direction;
					const float32 dl = d.Len();
					d = (dl > 1e-5f) ? d * (1.f / dl) : NkVec3f{0.f, -1.f, 0.f};
					Body(L.position, r, col, drawTri);
					NkVec3f x, y;
					Basis(d, x, y);
					const float32 len = L.range > 0.f ? L.range : 5.f;
					// Angles REELS de la lumiere (degres -> radians) : le widget doit
					// mesurer le cone effectif, pas en suggerer un.
					const float32 kDeg2Rad = 3.14159265f / 180.f;
					const float32 ro = len * tanf(L.outerAngle * kDeg2Rad);
					const float32 ri = len * tanf(L.innerAngle * kDeg2Rad);
					const NkVec3f base = L.position + d * len;
					Circle(base, x, y, ro, col, false, drawLine);
					// Angle INTERNE : seulement a la selection (c'est un reglage fin ;
					// l'afficher en permanence double le nombre de traits par spot).
					if (selected && ri > 0.f && ri < ro)
						Circle(base, x, y, ri, col, true, drawLine);
					// Quatre generatrices : assez pour lire le volume, pas assez pour
					// encombrer la vue.
					for (int32 i = 0; i < 4; i++) {
						const float32 a = 6.2831853f * (float32)i / 4.f;
						drawLine(L.position, base + x * (cosf(a) * ro) + y * (sinf(a) * ro), col);
					}
				}

				template <class DrawLine, class DrawTri>
				static void DrawArea(const NkLightDesc &L, NkVec4f col, float32 r, DrawLine drawLine, DrawTri drawTri) {
					NkVec3f d = L.direction;
					const float32 dl = d.Len();
					d = (dl > 1e-5f) ? d * (1.f / dl) : NkVec3f{0.f, -1.f, 0.f};
					NkVec3f x, y;
					Basis(d, x, y);
					// Dimensions REELLES si renseignees, sinon un carre neutre : mieux vaut
					// un widget visiblement generique qu'un widget qui invente une taille.
					const float32 hw = (L.areaWidth > 0.f ? L.areaWidth : 1.f) * 0.5f;
					const float32 hh = (L.areaHeight > 0.f ? L.areaHeight : 1.f) * 0.5f;
					const NkVec3f c = L.position;
					const NkVec3f a = c - x * hw - y * hh, b = c + x * hw - y * hh;
					const NkVec3f e = c + x * hw + y * hh, f = c - x * hw + y * hh;
					drawLine(a, b, col);
					drawLine(b, e, col);
					drawLine(e, f, col);
					drawLine(f, a, col);
					Body(c, r * 0.6f, col, drawTri);
					drawLine(c, c + d * (hw + hh), col); // normale = sens d'emission
				}
		};

	} // namespace renderer
} // namespace nkentseu
