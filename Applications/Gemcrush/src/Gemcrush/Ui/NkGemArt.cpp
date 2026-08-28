// -----------------------------------------------------------------------------
// FICHIER: Gemcrush/Ui/NkGemArt.cpp
// DESCRIPTION: Implémentation du dessin d'une gemme.
//
//              L'ORDRE DES COUCHES EST LA MOITIÉ DU RÉSULTAT — le voici, et il
//              ne doit pas être réordonné à la légère :
//                1. ombre portée      -> pose la gemme sur le plateau
//                2. halo              -> signale sélection / survol
//                3. corps dégradé     -> donne le VOLUME (c'est l'étape clé)
//                4. facette de table  -> suggère la taille du joyau
//                5. arêtes de taille  -> lit la gemme comme un objet facetté
//                6. reflet spéculaire -> donne la matière (verre, pas plastique)
//                7. liseré            -> détache la gemme du fond
//                8. marqueur spécial  -> dit ce que la gemme FAIT
//
// AUTEUR: Rihen
// DATE: 2026-08-27
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "Gemcrush/Ui/NkGemArt.h"

namespace nkentseu {
	namespace game {
		namespace ui {

			using nkgui::NkGuiDrawList;
			using math::NkVec2f;

			namespace {

				constexpr float32 kTau = 6.28318530718f;

				// Direction de la lumière, en haut à GAUCHE. Une seule et même
				// direction pour toute la scène : c'est ce qui fait que huit
				// gemmes différentes ont l'air d'être dans la même pièce.
				constexpr float32 kLightX = -0.55f;
				constexpr float32 kLightY = -0.83f;

				/// Éventail plein depuis le centre. Valable pour toute forme
				/// « étoilée » par rapport à son centre — ce qui couvre nos six
				/// silhouettes, étoile comprise. AddConvexPolyFilled ne le
				/// pourrait PAS pour l'étoile (elle n'est pas convexe).
				void FanFilled(NkGuiDrawList &dl, const NkVec2f &center, const NkVec2f *pts, int32 n,
							   const NkColor &centerColor, const NkColor *rimColors) {
					for (int32 i = 0; i < n; ++i) {
						const int32 j = (i + 1) % n;
						dl.AddTriangleMultiColor(center, pts[i], pts[j], centerColor, rimColors[i], rimColors[j]);
					}
				}

				/// Ellipse pleine (convexe) — ombre au sol et reflet spéculaire.
				void EllipseFilled(NkGuiDrawList &dl, const NkVec2f &center, float32 rx, float32 ry,
								   const NkColor &color, int32 segments = 18) {
					if (rx <= 0.f || ry <= 0.f) {
						return;
					}
					NkVec2f pts[32];
					if (segments > 32) {
						segments = 32;
					}
					for (int32 i = 0; i < segments; ++i) {
						const float32 a = kTau * static_cast<float32>(i) / static_cast<float32>(segments);
						pts[i] = NkVec2f(center.x + math::NkCos(a) * rx, center.y + math::NkSin(a) * ry);
					}
					dl.AddConvexPolyFilled(pts, segments, color);
				}

				/// Anneau partiel (secteur) — sert à la couronne de la bombe couleur.
				void ArcRing(NkGuiDrawList &dl, const NkVec2f &center, float32 innerRadius, float32 outerRadius,
							 float32 fromAngle, float32 toAngle, const NkColor &color, int32 segments = 6) {
					for (int32 i = 0; i < segments; ++i) {
						const float32 a0 =
							fromAngle + (toAngle - fromAngle) * (static_cast<float32>(i) / static_cast<float32>(segments));
						const float32 a1 =
							fromAngle + (toAngle - fromAngle) * (static_cast<float32>(i + 1) / static_cast<float32>(segments));
						const NkVec2f o0(center.x + math::NkCos(a0) * outerRadius, center.y + math::NkSin(a0) * outerRadius);
						const NkVec2f o1(center.x + math::NkCos(a1) * outerRadius, center.y + math::NkSin(a1) * outerRadius);
						const NkVec2f i0(center.x + math::NkCos(a0) * innerRadius, center.y + math::NkSin(a0) * innerRadius);
						const NkVec2f i1(center.x + math::NkCos(a1) * innerRadius, center.y + math::NkSin(a1) * innerRadius);
						dl.AddTriangleFilled(o0, o1, i1, color);
						dl.AddTriangleFilled(o0, i1, i0, color);
					}
				}

			} // namespace

			// =========================================================
			// BuildOutline — la SILHOUETTE, c'est-à-dire l'identité de la
			// gemme pour un joueur qui ne distingue pas les couleurs.
			// =========================================================
			int32 NkGemArt::BuildOutline(NkGemShape shape, const NkVec2f &center, float32 radius, NkVec2f *outPoints) {
				switch (shape) {
					// -- Losange (rubis) : haut et bas étirés, flancs rentrés --
					case NkGemShape::NK_GEM_SHAPE_DIAMOND: {
						const float32 kx = radius * 0.78f;
						outPoints[0] = NkVec2f(center.x, center.y - radius);
						outPoints[1] = NkVec2f(center.x + kx, center.y - radius * 0.22f);
						outPoints[2] = NkVec2f(center.x + kx * 0.62f, center.y + radius * 0.72f);
						outPoints[3] = NkVec2f(center.x, center.y + radius);
						outPoints[4] = NkVec2f(center.x - kx * 0.62f, center.y + radius * 0.72f);
						outPoints[5] = NkVec2f(center.x - kx, center.y - radius * 0.22f);
						return 6;
					}
					// -- Hexagone pointe en haut (émeraude) --
					case NkGemShape::NK_GEM_SHAPE_HEXAGON: {
						for (int32 i = 0; i < 6; ++i) {
							const float32 a = -math::NK_PI_F * 0.5f + kTau * static_cast<float32>(i) / 6.f;
							outPoints[i] = NkVec2f(center.x + math::NkCos(a) * radius, center.y + math::NkSin(a) * radius);
						}
						return 6;
					}
					// -- Coussin (ambre) : carré aux coins coupés = octogone --
					case NkGemShape::NK_GEM_SHAPE_SQUARE: {
						const float32 s = radius * 0.92f;
						const float32 c = s * 0.42f; // profondeur du pan coupé
						outPoints[0] = NkVec2f(center.x - s + c, center.y - s);
						outPoints[1] = NkVec2f(center.x + s - c, center.y - s);
						outPoints[2] = NkVec2f(center.x + s, center.y - s + c);
						outPoints[3] = NkVec2f(center.x + s, center.y + s - c);
						outPoints[4] = NkVec2f(center.x + s - c, center.y + s);
						outPoints[5] = NkVec2f(center.x - s + c, center.y + s);
						outPoints[6] = NkVec2f(center.x - s, center.y + s - c);
						outPoints[7] = NkVec2f(center.x - s, center.y - s + c);
						return 8;
					}
					// -- Trilliant (améthyste) : triangle aux angles adoucis --
					case NkGemShape::NK_GEM_SHAPE_TRIANGLE: {
						int32 n = 0;
						for (int32 i = 0; i < 3; ++i) {
							const float32 a = -math::NK_PI_F * 0.5f + kTau * static_cast<float32>(i) / 3.f;
							// Trois points par sommet : le coin est « rogné », ce qui
							// évite la pointe agressive d'un triangle pur.
							const float32 spread = 0.16f;
							for (int32 k = -1; k <= 1; ++k) {
								const float32 aa = a + spread * static_cast<float32>(k);
								const float32 rr = (k == 0) ? radius : radius * 0.97f;
								outPoints[n++] = NkVec2f(center.x + math::NkCos(aa) * rr, center.y + math::NkSin(aa) * rr);
							}
						}
						return n;
					}
					// -- Étoile à 5 branches (citrine) --
					case NkGemShape::NK_GEM_SHAPE_STAR: {
						const int32 spikes = 5;
						const float32 inner = radius * 0.47f;
						for (int32 i = 0; i < spikes * 2; ++i) {
							const float32 rr = (i % 2 == 0) ? radius : inner;
							const float32 a = -math::NK_PI_F * 0.5f +
											  math::NK_PI_F * static_cast<float32>(i) / static_cast<float32>(spikes);
							outPoints[i] = NkVec2f(center.x + math::NkCos(a) * rr, center.y + math::NkSin(a) * rr);
						}
						return spikes * 2;
					}
					// -- Brillant rond (saphir) --
					case NkGemShape::NK_GEM_SHAPE_CIRCLE:
					default: {
						const int32 segments = 20;
						for (int32 i = 0; i < segments; ++i) {
							const float32 a = kTau * static_cast<float32>(i) / static_cast<float32>(segments);
							outPoints[i] = NkVec2f(center.x + math::NkCos(a) * radius, center.y + math::NkSin(a) * radius);
						}
						return segments;
					}
				}
			}

			// =========================================================
			// Draw — la gemme complète
			// =========================================================
			void NkGemArt::Draw(NkGuiDrawList &dl, const NkGemVisual &v) {
				const float32 alpha = math::NkClamp(v.alpha, 0.f, 1.f);
				const float32 radius = v.size * 0.5f * math::NkMax(v.scale, 0.f);
				if (radius < 1.f || alpha <= 0.01f) {
					return; // rien de visible : on ne dépense pas de sommets
				}

				const NkGemPaletteEntry pal = NkGemPalette(v.color);
				const NkGemTheme &th = NkTheme();
				const float32 lift = math::NkClamp(v.lift, 0.f, 1.f);
				const NkVec2f center(v.center.x, v.center.y - lift * radius * 0.10f);

				// -- 1. Ombre portée --------------------------------------
				// Elle s'écarte et pâlit quand la gemme se soulève : c'est ce
				// qui fait « décoller » la gemme qu'on est en train de glisser.
				if (v.shadow) {
					const float32 shadowY = center.y + radius * (0.62f + lift * 0.30f);
					const float32 spread = 1.f + lift * 0.22f;
					EllipseFilled(dl, NkVec2f(center.x, shadowY), radius * 0.70f * spread, radius * 0.24f * spread,
								  NkWithAlpha(th.shadow, alpha * (1.f - lift * 0.30f)));
				}

				// -- 2. Halo de sélection / survol -------------------------
				if (v.selected || v.hovered) {
					const float32 pulse = 0.5f + 0.5f * math::NkSin(v.time * 5.0f);
					const float32 strength = v.selected ? (0.55f + 0.45f * pulse) : 0.28f;
					// Trois cercles concentriques de plus en plus transparents :
					// un dégradé radial ne coûte pas plus cher que ça.
					dl.AddCircleFilled(center, radius * 1.55f, NkWithAlpha(pal.glow, alpha * strength * 0.16f));
					dl.AddCircleFilled(center, radius * 1.28f, NkWithAlpha(pal.glow, alpha * strength * 0.20f));
					dl.AddCircleFilled(center, radius * 1.10f, NkWithAlpha(pal.glow, alpha * strength * 0.26f));
				}

				// -- Silhouette + éclairage par sommet ---------------------
				NkVec2f points[kMaxOutlinePoints];
				const int32 pointCount = BuildOutline(v.shape, center, radius, points);

				NkColor rim[kMaxOutlinePoints];
				for (int32 i = 0; i < pointCount; ++i) {
					const float32 dx = points[i].x - center.x;
					const float32 dy = points[i].y - center.y;
					const float32 len = math::NkSqrt(dx * dx + dy * dy);
					const float32 nx = (len > 0.0001f) ? dx / len : 0.f;
					const float32 ny = (len > 0.0001f) ? dy / len : 0.f;
					// Produit scalaire avec la lumière : 1 = face éclairée.
					const float32 ndl = nx * kLightX + ny * kLightY;
					const float32 t = 0.5f + 0.5f * ndl;
					NkColor c = NkLerpColor(pal.deep, pal.mid, 0.30f + 0.70f * t);
					if (ndl > 0.f) {
						c = NkLerpColor(c, pal.core, ndl * ndl * 0.40f); // arête qui capte la lumière
					}
					rim[i] = NkWithAlpha(c, alpha);
				}

				// -- 3. Corps : dégradé radial cœur clair -> arêtes ---------
				const NkColor centerColor = NkWithAlpha(NkLerpColor(pal.mid, pal.core, 0.55f), alpha);
				FanFilled(dl, center, points, pointCount, centerColor, rim);

				// -- 4. Facette de table : polygone intérieur décalé --------
				// Décalée vers la lumière, elle donne l'impression d'un plan
				// poli au sommet du joyau.
				{
					const NkVec2f tableCenter(center.x + kLightX * radius * 0.12f, center.y + kLightY * radius * 0.12f);
					NkVec2f table[kMaxOutlinePoints];
					const int32 tableCount = BuildOutline(v.shape, tableCenter, radius * 0.52f, table);
					NkColor tableRim[kMaxOutlinePoints];
					for (int32 i = 0; i < tableCount; ++i) {
						tableRim[i] = NkWithAlpha(pal.core, alpha * 0.10f);
					}
					FanFilled(dl, tableCenter, table, tableCount, NkWithAlpha(pal.core, alpha * 0.38f), tableRim);
				}

				// -- 5. Arêtes de taille : centre -> chaque sommet ----------
				//
				// ⚠️ SEULEMENT sur les silhouettes ANGULEUSES (<= 8 sommets).
				// MESURÉ à l'écran : tracées sur le cercle, échantillonné à 20
				// sommets, ces arêtes produisent 20 rayons réguliers et la gemme
				// se lit comme un PARASOL, pas comme un joyau. Le nombre de
				// sommets est une donnée de rendu, pas une intention de taille :
				// une facette n'a de sens que là où il y a un angle.
				if (pointCount <= 8) {
					const NkColor edge = NkWithAlpha(pal.deep, alpha * 0.28f);
					const float32 thickness = math::NkMax(1.f, radius * 0.04f);
					for (int32 i = 0; i < pointCount; ++i) {
						dl.AddLine(center, points[i], edge, thickness);
					}
				} else {
					// Taille brillant : une COURONNE (anneau sombre entre la table
					// et le bord) remplace les arêtes. Même rôle — dire que la
					// pierre est taillée — sans le rayonnement.
					dl.AddCircle(center, radius * 0.74f, NkWithAlpha(pal.deep, alpha * 0.30f),
								 math::NkMax(1.f, radius * 0.07f));
				}

				// -- 6. Reflet spéculaire : la matière ---------------------
				{
					const NkVec2f spec(center.x + kLightX * radius * 0.42f, center.y + kLightY * radius * 0.42f);
					EllipseFilled(dl, spec, radius * 0.26f, radius * 0.17f, NkColor(255, 255, 255, static_cast<uint8>(190.f * alpha)));
					// Le petit point secondaire : sans lui le reflet a l'air peint.
					EllipseFilled(dl, NkVec2f(center.x - kLightX * radius * 0.30f, center.y - kLightY * radius * 0.10f),
								  radius * 0.11f, radius * 0.08f,
								  NkColor(255, 255, 255, static_cast<uint8>(85.f * alpha)), 12);
				}

				// -- 7. Liseré : détache la gemme du damier ----------------
				dl.AddPolyline(points, pointCount, NkWithAlpha(pal.deep, alpha * 0.85f),
							   math::NkMax(1.5f, radius * 0.09f), true);

				// -- 8. Marqueur de gemme spéciale -------------------------
				if (v.special != NkGemSpecialKind::NK_GEM_SPECIAL_NONE) {
					DrawSpecialOverlay(dl, v, center, radius, alpha);
				}

				// -- Anneau net de sélection (par-dessus tout) -------------
				if (v.selected) {
					const float32 pulse = 0.5f + 0.5f * math::NkSin(v.time * 5.0f);
					dl.AddCircle(center, radius * (1.16f + pulse * 0.06f),
								 NkWithAlpha(NkTheme().accent, alpha * (0.75f + 0.25f * pulse)),
								 math::NkMax(2.f, radius * 0.09f));
				}
			}

			// =========================================================
			// DrawSpecialOverlay — ce que la gemme FAIT doit se lire en un
			// coup d'oeil, sans texte et sans avoir joué la partie d'avant.
			//
			// PRINCIPE DE LECTURE, et c'est lui qui a guidé chaque forme :
			//   rayée      -> une DIRECTION  (ça part en ligne, dans ce sens-là)
			//   enveloppée -> une ZONE       (ça explose autour)
			//   bombe      -> TOUTES LES COULEURS (ça n'en vise aucune en
			//                 particulier)
			//   poisson    -> quelque chose qui SE DÉPLACE et va chercher
			//
			// Les quatre partagent un halo pulsant : avant même de distinguer
			// laquelle c'est, le joueur doit voir qu'elle n'est pas ordinaire.
			// =========================================================
			void NkGemArt::DrawSpecialOverlay(NkGuiDrawList &dl, const NkGemVisual &v, const NkVec2f &center,
											  float32 radius, float32 alpha) {
				const NkGemPaletteEntry pal = NkGemPalette(v.color);
				const NkColor white(255, 255, 255, static_cast<uint8>(240.f * alpha));
				const NkColor whiteSoft(255, 255, 255, static_cast<uint8>(120.f * alpha));
				const NkColor whiteFaint(255, 255, 255, 0);
				const NkVec2f c = center;
				const float32 pulse = 0.5f + 0.5f * math::NkSin(v.time * 3.2f);

				// -- Halo commun : « cette gemme n'est pas ordinaire » ------
				dl.AddCircleFilled(c, radius * (1.12f + pulse * 0.06f),
								   NkWithAlpha(pal.glow, alpha * (0.10f + 0.08f * pulse)));

				switch (v.special) {
					// ---------------------------------------------------------
					// RAYÉE : trois barres à dégradé + chevrons aux extrémités.
					// Les chevrons sont l'information utile — ils disent OÙ ça
					// part. Des barres seules laissent le sens ambigu.
					// ---------------------------------------------------------
					case NkGemSpecialKind::NK_GEM_SPECIAL_STRIPED_HORIZONTAL:
					case NkGemSpecialKind::NK_GEM_SPECIAL_STRIPED_VERTICAL: {
						const bool horizontal = (v.special == NkGemSpecialKind::NK_GEM_SPECIAL_STRIPED_HORIZONTAL);
						const float32 half = radius * 0.98f;
						// Trois barres : une large au centre, deux fines de part et
						// d'autre. Une seule barre se confond avec un reflet.
						const float32 widths[3] = {radius * 0.10f, radius * 0.17f, radius * 0.10f};
						const float32 offsets[3] = {-radius * 0.42f, 0.f, radius * 0.42f};
						for (int32 k = 0; k < 3; ++k) {
							const float32 w = widths[k];
							const float32 o = offsets[k];
							const NkColor edge = NkWithAlpha(white, 0.15f);
							if (horizontal) {
								// Dégradé transparent -> plein -> transparent : la barre
								// se fond dans la gemme au lieu d'être posée dessus.
								dl.AddRectFilledMultiColor(nkgui::NkRect{c.x - half, c.y + o - w, half, w * 2.f}, edge,
														   white, white, edge);
								dl.AddRectFilledMultiColor(nkgui::NkRect{c.x, c.y + o - w, half, w * 2.f}, white, edge,
														   edge, white);
							} else {
								dl.AddRectFilledMultiColor(nkgui::NkRect{c.x + o - w, c.y - half, w * 2.f, half}, edge,
														   edge, white, white);
								dl.AddRectFilledMultiColor(nkgui::NkRect{c.x + o - w, c.y, w * 2.f, half}, white, white,
														   edge, edge);
							}
						}
						// Chevrons : deux pointes opposées, vers l'extérieur.
						const float32 tip = radius * 1.02f;
						const float32 back = radius * 0.66f;
						const float32 spread = radius * 0.34f;
						for (int32 side = -1; side <= 1; side += 2) {
							const float32 d = static_cast<float32>(side);
							NkVec2f p[3];
							if (horizontal) {
								p[0] = NkVec2f(c.x + d * tip, c.y);
								p[1] = NkVec2f(c.x + d * back, c.y - spread);
								p[2] = NkVec2f(c.x + d * back, c.y + spread);
							} else {
								p[0] = NkVec2f(c.x, c.y + d * tip);
								p[1] = NkVec2f(c.x - spread, c.y + d * back);
								p[2] = NkVec2f(c.x + spread, c.y + d * back);
							}
							dl.AddConvexPolyFilled(p, 3, white);
						}
						break;
					}
					// ---------------------------------------------------------
					// ENVELOPPÉE : un ruban qui CEINT la gemme + quatre éclats
					// qui partent en diagonale. La diagonale est délibérée :
					// elle dit « zone », là où l'horizontale dirait « ligne ».
					// ---------------------------------------------------------
					case NkGemSpecialKind::NK_GEM_SPECIAL_WRAPPED: {
						const float32 band = radius * 0.20f;
						// Ruban horizontal + vertical, aux bouts fondus.
						dl.AddRectFilledMultiColor(nkgui::NkRect{c.x - radius, c.y - band, radius, band * 2.f},
												   whiteFaint, whiteSoft, whiteSoft, whiteFaint);
						dl.AddRectFilledMultiColor(nkgui::NkRect{c.x, c.y - band, radius, band * 2.f}, whiteSoft,
												   whiteFaint, whiteFaint, whiteSoft);
						dl.AddRectFilledMultiColor(nkgui::NkRect{c.x - band, c.y - radius, band * 2.f, radius},
												   whiteFaint, whiteFaint, whiteSoft, whiteSoft);
						dl.AddRectFilledMultiColor(nkgui::NkRect{c.x - band, c.y, band * 2.f, radius}, whiteSoft,
												   whiteSoft, whiteFaint, whiteFaint);
						// Noeud central : c'est lui qui fait « emballé ».
						dl.AddRectFilled(nkgui::NkRect{c.x - band * 1.5f, c.y - band * 1.5f, band * 3.f, band * 3.f},
										 white, band);
						dl.AddCircleFilled(c, band * 0.62f, NkWithAlpha(pal.deep, alpha * 0.75f));
						// Quatre éclats en diagonale, pulsants.
						for (int32 k = 0; k < 4; ++k) {
							const float32 a = math::NK_PI_F * 0.25f + kTau * static_cast<float32>(k) / 4.f;
							const float32 d = radius * (0.78f + pulse * 0.10f);
							DrawSparkle(dl, NkVec2f(c.x + math::NkCos(a) * d, c.y + math::NkSin(a) * d),
										radius * 0.20f, NkWithAlpha(white, 0.55f + 0.45f * pulse), a);
						}
						break;
					}
					// ---------------------------------------------------------
					// BOMBE COULEUR : sphère noire lustrée, ceinte des SIX
					// teintes du jeu. Elle dit « n'importe quelle couleur » sans
					// un mot — et c'est la seule gemme qui n'a PAS la couleur de
					// sa case, ce qui la rend repérable de loin.
					// ---------------------------------------------------------
					case NkGemSpecialKind::NK_GEM_SPECIAL_COLOR_BOMB: {
						const NkGemColor wheel[6] = {NkGemColor::NK_GEM_COLOR_RED,	 NkGemColor::NK_GEM_COLOR_ORANGE,
													 NkGemColor::NK_GEM_COLOR_YELLOW, NkGemColor::NK_GEM_COLOR_GREEN,
													 NkGemColor::NK_GEM_COLOR_BLUE,	 NkGemColor::NK_GEM_COLOR_PURPLE};
						const float32 spin = v.time * 0.75f; // la couronne tourne : l'oeil y va
						for (int32 i = 0; i < 6; ++i) {
							const float32 a0 = spin + kTau * static_cast<float32>(i) / 6.f;
							const float32 a1 = spin + kTau * static_cast<float32>(i + 1) / 6.f;
							const NkGemPaletteEntry seg = NkGemPalette(wheel[i]);
							// Deux anneaux concentriques : le clair à l'extérieur,
							// le saturé à l'intérieur — la couronne prend du volume.
							ArcRing(dl, c, radius * 0.78f, radius * 1.00f, a0, a1 - 0.06f, NkWithAlpha(seg.core, alpha));
							ArcRing(dl, c, radius * 0.62f, radius * 0.78f, a0, a1 - 0.06f, NkWithAlpha(seg.mid, alpha));
						}
						// Sphère noire par-dessus le centre de la couronne.
						dl.AddCircleFilled(c, radius * 0.64f, NkColor(10, 8, 22, static_cast<uint8>(248.f * alpha)));
						dl.AddCircle(c, radius * 0.64f, NkWithAlpha(white, 0.30f), math::NkMax(1.f, radius * 0.05f));
						// Reflet : sans lui la sphère est un trou, pas un objet.
						EllipseFilled(dl, NkVec2f(c.x + kLightX * radius * 0.30f, c.y + kLightY * radius * 0.30f),
									  radius * 0.18f, radius * 0.12f,
									  NkColor(255, 255, 255, static_cast<uint8>(170.f * alpha)), 14);
						DrawSparkle(dl, NkVec2f(c.x - radius * 0.16f, c.y + radius * 0.20f), radius * 0.16f,
									NkWithAlpha(white, 0.4f + 0.6f * pulse), v.time);
						break;
					}
					// ---------------------------------------------------------
					// POISSON : corps + queue échancrée + nageoire + oeil. Il
					// ONDULE légèrement — c'est le seul marqueur qui bouge de
					// lui-même, parce que c'est le seul qui va CHERCHER sa cible.
					// ---------------------------------------------------------
					case NkGemSpecialKind::NK_GEM_SPECIAL_FISH: {
						const float32 wobble = math::NkSin(v.time * 4.5f) * radius * 0.07f;
						const NkColor body = NkWithAlpha(NkTheme().accentCool, alpha);
						const NkColor bodyLight = NkWithAlpha(NkColor(220, 250, 255, 255), alpha);

						// Queue : deux pointes séparées par une échancrure.
						const NkVec2f tail[4] = {NkVec2f(c.x - radius * 0.30f, c.y + wobble),
												 NkVec2f(c.x - radius * 0.92f, c.y - radius * 0.40f + wobble),
												 NkVec2f(c.x - radius * 0.62f, c.y + wobble),
												 NkVec2f(c.x - radius * 0.92f, c.y + radius * 0.40f + wobble)};
						dl.AddTriangleFilled(tail[0], tail[1], tail[2], body);
						dl.AddTriangleFilled(tail[0], tail[2], tail[3], body);

						// Nageoire dorsale.
						const NkVec2f fin[3] = {NkVec2f(c.x + radius * 0.04f, c.y - radius * 0.30f + wobble * 0.5f),
												NkVec2f(c.x - radius * 0.18f, c.y - radius * 0.72f + wobble * 0.5f),
												NkVec2f(c.x + radius * 0.26f, c.y - radius * 0.26f + wobble * 0.5f)};
						dl.AddConvexPolyFilled(fin, 3, NkWithAlpha(body, 0.85f));

						// Corps + ventre plus clair : un aplat cyan ne se lit pas
						// comme un poisson, il se lit comme une tache.
						EllipseFilled(dl, NkVec2f(c.x + radius * 0.14f, c.y + wobble * 0.6f), radius * 0.56f,
									  radius * 0.36f, body);
						EllipseFilled(dl, NkVec2f(c.x + radius * 0.18f, c.y + radius * 0.10f + wobble * 0.6f),
									  radius * 0.40f, radius * 0.18f, NkWithAlpha(bodyLight, 0.55f));

						// Oeil + son reflet : c'est ce détail qui rend l'animal vivant.
						const NkVec2f eye(c.x + radius * 0.42f, c.y - radius * 0.08f + wobble * 0.6f);
						dl.AddCircleFilled(eye, radius * 0.11f, NkColor(16, 14, 34, static_cast<uint8>(240.f * alpha)));
						dl.AddCircleFilled(NkVec2f(eye.x - radius * 0.03f, eye.y - radius * 0.03f), radius * 0.04f, white);
						break;
					}
					default:
						break;
				}
			}

			// =========================================================
			// DrawIcon — gemme à plat pour le HUD (objectifs, légende)
			// =========================================================
			void NkGemArt::DrawIcon(NkGuiDrawList &dl, NkGemColor color, const NkVec2f &center, float32 size) {
				NkGemVisual v;
				v.center = center;
				v.size = size;
				v.color = color;
				v.shape = NkGemShapeForColor(color);
				v.shadow = false;
				Draw(dl, v);
			}

			// =========================================================
			// DrawSparkle — éclat à quatre branches
			// =========================================================
			void NkGemArt::DrawSparkle(NkGuiDrawList &dl, const NkVec2f &center, float32 radius, const NkColor &color,
									   float32 rotation) {
				if (radius <= 0.5f) {
					return;
				}
				const float32 thin = radius * 0.22f;
				for (int32 k = 0; k < 2; ++k) {
					const float32 a = rotation + math::NK_PI_F * 0.5f * static_cast<float32>(k);
					const float32 ca = math::NkCos(a);
					const float32 sa = math::NkSin(a);
					// Losange allongé : long dans l'axe, fin en travers.
					const NkVec2f p[4] = {NkVec2f(center.x + ca * radius, center.y + sa * radius),
										  NkVec2f(center.x - sa * thin, center.y + ca * thin),
										  NkVec2f(center.x - ca * radius, center.y - sa * radius),
										  NkVec2f(center.x + sa * thin, center.y - ca * thin)};
					dl.AddConvexPolyFilled(p, 4, color);
				}
				dl.AddCircleFilled(center, radius * 0.18f, color);
			}

		} // namespace ui
	} // namespace game
} // namespace nkentseu
