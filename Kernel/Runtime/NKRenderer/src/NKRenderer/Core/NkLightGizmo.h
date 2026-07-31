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

#include <stdlib.h> // getenv : choisir le design sans recompiler

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
				// ── DEUX DESIGNS AU CHOIX (Rihen : « fais les deux au choix ») ──────
				// Le premier jet dessinait un SOLIDE 3D (octaedre plein). C'etait l'erreur
				// de fond : un volume se lit comme de la GEOMETRIE DE SCENE, pas comme un
				// marqueur. Blender comme Unreal utilisent un symbole FACE CAMERA a taille
				// ecran constante — ce qui les rend reconnaissables sous tout angle et
				// impossibles a confondre avec un objet.
				//
				//   BlenderWire      symbole filaire fin : disque + anneau. Aucun asset,
				//                    tres discret, adapte a un affichage permanent.
				//   UnrealBillboard  quad face camera (l'encombrement d'un sprite) portant
				//                    un glyphe distinct par type. Plus lisible de loin.
				//                    Le glyphe est TRACE, pas echantillonne : meme lecture
				//                    qu'un sprite sans imposer d'atlas d'icones au moteur ;
				//                    passer a une vraie texture ne touchera que BodyIcon().
				//   Solid3D          l'ancien octaedre, conserve : utile en debogage pour
				//                    verifier une position en profondeur.
				enum class Style : uint8 { BlenderWire = 0, UnrealBillboard = 1, Solid3D = 2 };

				static Style &StyleRef() {
					// NK_LIGHT_STYLE (0/1/2) permet de comparer les trois sans recompiler.
					static Style s = ParseStyleEnv();
					return s;
				}
				static void SetStyle(Style s) { StyleRef() = s; }
				static Style GetStyle() { return StyleRef(); }

				// AXES ECRAN DE LA CAMERA — a poser une fois par frame avant tout Draw().
				// Sans eux un symbole « face camera » resterait plaque dans un plan du
				// monde et disparaitrait de profil : c'est toute la difference entre un
				// symbole et un morceau de geometrie. On les passe par un etat de classe
				// plutot que par la signature de Draw() pour ne casser aucun appelant ;
				// non renseignes, on retombe sur les axes du monde (comportement d'avant).
				static NkVec3f &CamRightRef() {
					static NkVec3f v{1.f, 0.f, 0.f};
					return v;
				}
				static NkVec3f &CamUpRef() {
					static NkVec3f v{0.f, 1.f, 0.f};
					return v;
				}
				static void SetCameraAxes(NkVec3f right, NkVec3f up) {
					const float32 lr = right.Len(), lu = up.Len();
					if (lr > 1e-5f)
						CamRightRef() = right * (1.f / lr);
					if (lu > 1e-5f)
						CamUpRef() = up * (1.f / lu);
				}

				// ── PICK PAR DISTANCE ECRAN ─────────────────────────────────────────
				// Meme arbitration que le pick d'objets. On NE teste PAS une intersection
				// geometrique : le widget n'a pas de volume reel et sa taille depend de la
				// distance, donc un test 3D rendrait une lumiere lointaine impossible a
				// cliquer alors qu'elle reste grosse a l'ecran. On compare une distance EN
				// PIXELS a un rayon en pixels : ce qu'on voit gros se clique facilement.
				static float32 PickRadiusPx() {
					return 14.f; // aligne sur le pick de sommets en mode edition
				}
				static bool HitScreen(float32 lx, float32 ly, float32 px, float32 py) {
					const float32 dx = px - lx, dy = py - ly, r = PickRadiusPx();
					return (dx * dx + dy * dy) <= (r * r);
				}
				// Point d'ANCRAGE du widget : c'est lui qu'on projette pour le pick, et il
				// doit rester le meme que celui du dessin, sinon on cliquerait a cote.
				static NkVec3f Anchor(const NkLightDesc &L) {
					return L.position;
				}

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

				static Style ParseStyleEnv() {
					const char *v = getenv("NK_LIGHT_STYLE");
					if (v && v[0] == '1')
						return Style::UnrealBillboard;
					if (v && v[0] == '2')
						return Style::Solid3D;
					return Style::BlenderWire; // defaut : le plus discret, et sans asset
				}

				// CORPS du marqueur — seul element qui depend du design. Les indications
				// de portee, de cone et de direction restent communes aux trois styles :
				// elles decrivent la LUMIERE, pas sa representation.
				template <class DrawLine, class DrawTri>
				static void Body(NkVec3f c, float32 r, NkVec4f col, NkLightType type, DrawLine drawLine,
								 DrawTri drawTri) {
					switch (StyleRef()) {
						case Style::UnrealBillboard: BodyIcon(c, r, col, type, drawLine, drawTri); break;
						case Style::Solid3D: BodySolid(c, r, col, drawTri); break;
						default: BodyWire(c, r, col, type, drawLine); break;
					}
				}

				// Cercle FACE CAMERA — brique commune aux deux nouveaux designs.
				template <class DrawLine>
				static void ScreenCircle(NkVec3f c, float32 rad, NkVec4f col, int32 seg, bool dashed, DrawLine drawLine) {
					const NkVec3f cr = CamRightRef(), cu = CamUpRef();
					NkVec3f prev{};
					for (int32 i = 0; i <= seg; i++) {
						const float32 a = 6.2831853f * (float32)i / (float32)seg;
						const NkVec3f p = c + cr * (cosf(a) * rad) + cu * (sinf(a) * rad);
						if (i > 0 && (!dashed || (i & 1)))
							drawLine(prev, p, col);
						prev = p;
					}
				}

				// DESIGN BLENDER : symbole filaire fin, face camera. Un petit disque (le
				// « point » de la lumiere) cercle d'un anneau — pointille pour une
				// ponctuelle, plein sinon, ce qui suffit a les distinguer d'un coup d'oeil.
				template <class DrawLine>
				static void BodyWire(NkVec3f c, float32 r, NkVec4f col, NkLightType type, DrawLine drawLine) {
					ScreenCircle(c, r * 0.42f, col, 10, false, drawLine);
					ScreenCircle(c, r, col, 20, type == NkLightType::NK_POINT, drawLine);
				}

				// DESIGN UNREAL : quad face camera + glyphe par type. Le fond assombri
				// detache l'icone d'un decor clair sans masquer la scene (alpha bas).
				template <class DrawLine, class DrawTri>
				static void BodyIcon(NkVec3f c, float32 r, NkVec4f col, NkLightType type, DrawLine drawLine,
									 DrawTri drawTri) {
					const NkVec3f cr = CamRightRef(), cu = CamUpRef();
					const NkVec3f a = c - cr * r - cu * r, b = c + cr * r - cu * r;
					const NkVec3f d = c + cr * r + cu * r, e = c - cr * r + cu * r;
					const NkVec4f bg = {col.x * 0.18f, col.y * 0.18f, col.z * 0.18f, 0.55f};
					drawTri(a, b, d, bg);
					drawTri(a, d, e, bg);
					drawLine(a, b, col);
					drawLine(b, d, col);
					drawLine(d, e, col);
					drawLine(e, a, col);
					const float32 g = r * 0.58f;
					if (type == NkLightType::NK_DIRECTIONAL) {
						// Soleil : disque + rayons courts.
						ScreenCircle(c, g * 0.45f, col, 10, false, drawLine);
						for (int32 i = 0; i < 8; i++) {
							const float32 an = 6.2831853f * (float32)i / 8.f;
							const NkVec3f dir = cr * cosf(an) + cu * sinf(an);
							drawLine(c + dir * (g * 0.66f), c + dir * g, col);
						}
					} else if (type == NkLightType::NK_SPOT) {
						// Projecteur : triangle pointe en haut.
						drawLine(c + cu * g, c - cr * g - cu * g, col);
						drawLine(c + cu * g, c + cr * g - cu * g, col);
						drawLine(c - cr * g - cu * g, c + cr * g - cu * g, col);
					} else if (type == NkLightType::NK_AREA) {
						// Surfacique : rectangle.
						const float32 h = g * 0.62f;
						drawLine(c - cr * g - cu * h, c + cr * g - cu * h, col);
						drawLine(c + cr * g - cu * h, c + cr * g + cu * h, col);
						drawLine(c + cr * g + cu * h, c - cr * g + cu * h, col);
						drawLine(c - cr * g + cu * h, c - cr * g - cu * h, col);
					} else {
						// Ponctuelle : ampoule stylisee (globe + culot).
						ScreenCircle(c + cu * (g * 0.28f), g * 0.5f, col, 12, false, drawLine);
						drawLine(c - cr * (g * 0.26f) - cu * (g * 0.42f), c + cr * (g * 0.26f) - cu * (g * 0.42f), col);
						drawLine(c - cr * (g * 0.26f) - cu * (g * 0.70f), c + cr * (g * 0.26f) - cu * (g * 0.70f), col);
					}
				}

				// Corps PLEIN d'origine (octaedre), conserve sous Style::Solid3D : utile
				// en debogage pour verifier une position en profondeur.
				template <class DrawTri> static void BodySolid(NkVec3f c, float32 r, NkVec4f col, DrawTri drawTri) {
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
					Body(L.position, r, col, L.type, drawLine, drawTri);
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
					Body(c, r, col, L.type, drawLine, drawTri);
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
					Body(L.position, r, col, L.type, drawLine, drawTri);
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
					Body(c, r * 0.6f, col, L.type, drawLine, drawTri);
					drawLine(c, c + d * (hw + hh), col); // normale = sens d'emission
				}
		};

	} // namespace renderer
} // namespace nkentseu
