// =============================================================================
// NkPhysRendu.cpp — le rendu de la vue
//
// UN MATERIAU, UNE FACON DE SE MONTRER
//   ballon    eventail plein depuis le centre + reflet speculaire
//   gelee     cellules de la grille ombrees par leur COMPRESSION
//   caisse    le quadrilatere des quatre coins + planches
//   tissu     cellules a rayures, le VERSO plus sombre quand il se replie
//   fluides   halo translucide sous un coeur opaque : les particules se
//             fondent en une nappe continue ; l'eau blanchit quand elle va vite
//   atomes    liaisons + spheres ; ils ROUGEOIENT quand ils s'agitent
//   corde     polyligne epaisse, lest metallique ; le pont en planches
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Physic2D/Editeur/NkPhysRendu.h"
#include "Physic2D/Editeur/NkUE5Theme.h"

#include <cmath>
#include <cstring>

namespace nkentseu {
	namespace physic2d {

		using namespace ue5;
		using nkgui::NkColor;
		using nkgui::NkGuiBlend;
		using nkgui::NkGuiDrawList;
		using nkgui::NkRect;
		using nkgui::NkVec2;

		const char *NkModeVueNom(NkModeVue m) noexcept {
			switch (m) {
				case NkModeVue::NK_ECLAIRE:
					return "Éclairé";
				case NkModeVue::NK_FILAIRE:
					return "Filaire";
				case NkModeVue::NK_CONTRAINTES:
					return "Contraintes";
				case NkModeVue::NK_VITESSE:
					return "Vitesse";
				default:
					return "?";
			}
		}

		float32 NkPasGrille(float32 zoom) noexcept {
			float32 pas = 1.f;
			while (pas * zoom < 9.f && pas < 100000.f) {
				pas *= 10.f;
			}
			return pas;
		}

		namespace {
			NkColor CouleurCorps(const NkCorps &c, uint8 a = 255) {
				return NkColor(c.couleur[0], c.couleur[1], c.couleur[2], a);
			}

			NkColor CarteChaleur(float32 t) {
				t = NkClampf(t, 0.f, 1.f);
				const NkColor c0(40, 70, 200), c1(40, 200, 230), c2(250, 220, 60), c3(250, 60, 40);
				if (t < 0.33f) {
					return NkMelange(c0, c1, t / 0.33f);
				}
				if (t < 0.66f) {
					return NkMelange(c1, c2, (t - 0.33f) / 0.33f);
				}
				return NkMelange(c2, c3, (t - 0.66f) / 0.34f);
			}

			NkColor CouleurTension(float32 t) {
				const float32 k = NkClampf(t * 10.f, -1.f, 1.f);
				const NkColor neutre(90, 210, 110);
				return k >= 0.f ? NkMelange(neutre, NkColor(255, 64, 40), k) : NkMelange(neutre, NkColor(60, 120, 255), -k);
			}

			int32 Segments(float32 rayonPx) {
				const int32 s = static_cast<int32>(rayonPx * 0.9f) + 6;
				return s > 24 ? 24 : s;
			}

			struct Contexte {
					NkGuiDrawList &dl;
					const NkMonde &m;
					const NkCamera2D &cam;
					const NkOptionsVue &o;
					NkRect visible; // en coordonnees monde (x0, y0, x1, y1)

					bool Visible(const NkV2 &p, float32 marge) const {
						return p.x > visible.x - marge && p.x < visible.w + marge && p.y > visible.y - marge &&
							   p.y < visible.h + marge;
					}
					NkVec2 E(const NkV2 &p) const {
						return cam.Ecran(p);
					}
					NkVec2 E(uint32 i) const {
						return cam.Ecran(m.particules[i].pos);
					}
			};

			// -----------------------------------------------------------------
			// Decor
			// -----------------------------------------------------------------
			void DessinerDecor(Contexte &cx) {
				NkGuiDrawList &dl = cx.dl;
				const NkCamera2D &cam = cx.cam;
				const NkRect &v = cam.vue;
				dl.AddRectFilledMultiColor(v, kVueHaut, kVueHaut, kVueBas, kVueBas);

				const float32 L = cx.m.reglages.largeur * 0.5f;
				const float32 H = cx.m.reglages.hauteur;

				if (cx.o.grille) {
					const float32 pas = NkPasGrille(cam.zoom);
					const float32 fort = pas * 10.f;
					const float32 x0 = std::floor(cx.visible.x / pas) * pas;
					const float32 y0 = std::floor(cx.visible.y / pas) * pas;
					// Les lignes fines s'estompent quand elles se resserrent : pas de
					// "moire" au moment ou le pas change.
					const float32 fondu = NkClampf((pas * cam.zoom - 9.f) / 20.f, 0.f, 1.f);
					const NkColor fine = NkAvecAlpha(kGrilleFine, static_cast<uint8>(static_cast<float32>(kGrilleFine.a) * (0.35f + 0.65f * fondu)));
					int32 garde = 0;
					for (float32 x = x0; x <= cx.visible.w && garde < 600; x += pas, ++garde) {
						const float32 sx = cam.Ecran(NkV2(x, 0.f)).x;
						const bool maj = std::fabs(std::fmod(std::fabs(x) + 0.5f, fort)) < 1.f;
						dl.AddLine({sx, v.y}, {sx, v.y + v.h}, maj ? kGrilleForte : fine, 1.f);
					}
					garde = 0;
					for (float32 y = y0; y <= cx.visible.h && garde < 600; y += pas, ++garde) {
						const float32 sy = cam.Ecran(NkV2(0.f, y)).y;
						const bool maj = std::fabs(std::fmod(std::fabs(y) + 0.5f, fort)) < 1.f;
						dl.AddLine({v.x, sy}, {v.x + v.w, sy}, maj ? kGrilleForte : fine, 1.f);
					}
					// Axe Y (vert) : x = 0.
					const float32 sx0 = cam.Ecran(NkV2(0.f, 0.f)).x;
					dl.AddLine({sx0, v.y}, {sx0, v.y + v.h}, NkAvecAlpha(kAxeY, 110), 1.f);
				}

				// Hors du monde : voile.
				const NkVec2 hg = cam.Ecran(NkV2(-L, H));
				const NkVec2 bd = cam.Ecran(NkV2(L, 0.f));
				if (cx.m.reglages.murs) {
					if (hg.x > v.x) {
						dl.AddRectFilled({v.x, v.y, hg.x - v.x, v.h}, kVueHorsMonde);
					}
					if (bd.x < v.x + v.w) {
						dl.AddRectFilled({bd.x, v.y, v.x + v.w - bd.x, v.h}, kVueHorsMonde);
					}
					if (hg.y > v.y) {
						dl.AddRectFilled({NkMaxf(hg.x, v.x), v.y, NkMinf(bd.x, v.x + v.w) - NkMaxf(hg.x, v.x), hg.y - v.y}, kVueHorsMonde);
					}
					dl.AddLine({hg.x, hg.y}, {hg.x, bd.y}, NkAvecAlpha(kSolBord, 160), 2.f);
					dl.AddLine({bd.x, hg.y}, {bd.x, bd.y}, NkAvecAlpha(kSolBord, 160), 2.f);
					dl.AddLine({hg.x, hg.y}, {bd.x, hg.y}, NkAvecAlpha(kSolBord, 90), 1.f);
				}

				// Sol.
				const float32 solY = bd.y;
				if (solY < v.y + v.h) {
					const float32 xa = cx.m.reglages.murs ? v.x : NkMaxf(v.x, hg.x);
					const float32 xb = cx.m.reglages.murs ? v.x + v.w : NkMinf(v.x + v.w, bd.x);
					const float32 haut = NkMaxf(solY, v.y);
					dl.AddRectFilledMultiColor({xa, haut, xb - xa, v.y + v.h - haut}, kSol, kSol, NkAssombrir(kSol, 0.5f),
											   NkAssombrir(kSol, 0.5f));
					// Hachures de sol, comme un plan de coupe.
					const float32 pasH = NkMaxf(18.f, 40.f * cam.zoom);
					for (float32 x = xa - std::fmod(xa - v.x, pasH); x < xb; x += pasH) {
						dl.AddLine({x, solY + 3.f}, {x - pasH * 0.6f, solY + 3.f + pasH * 0.6f}, NkColor(255, 255, 255, 10), 1.f);
					}
					dl.AddLine({xa, solY}, {xb, solY}, kSolBord, 2.f);
					dl.AddLine({xa, solY - 1.f}, {xb, solY - 1.f}, NkAvecAlpha(kAxeX, 90), 1.f);
				}

				// Obstacles : capsules en relief.
				for (uint32 k = 0; k < cx.m.obstacles.Size(); ++k) {
					const NkObstacle &ob = cx.m.obstacles[k];
					const NkVec2 a = cam.Ecran(ob.a);
					const NkVec2 b = cam.Ecran(ob.b);
					const float32 r = ob.rayon * cam.zoom;
					dl.AddLine(a, b, kObstacleBord, r * 2.f + 3.f);
					dl.AddCircleFilled(a, r + 1.5f, kObstacleBord, Segments(r));
					dl.AddCircleFilled(b, r + 1.5f, kObstacleBord, Segments(r));
					dl.AddLine(a, b, kObstacle, r * 2.f);
					dl.AddCircleFilled(a, r, kObstacle, Segments(r));
					dl.AddCircleFilled(b, r, kObstacle, Segments(r));
					const NkVec2 d{0.f, -r * 0.45f};
					dl.AddLine({a.x + d.x, a.y + d.y}, {b.x + d.x, b.y + d.y}, NkAvecAlpha(kObstacleReflet, 120), NkMaxf(1.f, r * 0.35f));
				}
			}

			// -----------------------------------------------------------------
			// Corps
			// -----------------------------------------------------------------
			void DessinerBallon(Contexte &cx, const NkCorps &c) {
				const uint32 n = c.nombre;
				if (n < 3) {
					return;
				}
				NkV2 centre;
				for (uint32 k = 0; k < n; ++k) {
					centre += cx.m.particules[c.debut + k].pos;
				}
				centre = centre / static_cast<float32>(n);
				const bool creve = c.pression <= 0.f;
				const NkColor base = creve ? NkAssombrir(CouleurCorps(c), 0.35f) : CouleurCorps(c);
				const NkColor bord = NkAssombrir(base, 0.45f);
				const NkVec2 ce = cx.E(centre);
				// Eventail : sur, meme ecrase, tant que l'anneau reste etoile.
				for (uint32 k = 0; k < n; ++k) {
					const NkVec2 a = cx.E(c.debut + k);
					const NkVec2 b = cx.E(c.debut + (k + 1) % n);
					cx.dl.AddTriangleMultiColor(ce, a, b, NkEclaircir(base, 0.18f), base, base);
				}
				NkVec2 anneau[128];
				const uint32 nn = n < 127 ? n : 127;
				for (uint32 k = 0; k < nn; ++k) {
					anneau[k] = cx.E(c.debut + k);
				}
				cx.dl.AddPolyline(anneau, static_cast<int32>(nn), bord, NkMaxf(1.5f, 3.f * cx.cam.zoom), true);
				if (!creve) {
					// Reflet : place dans le repere du ballon, en haut a gauche.
					NkV2 mn, mx;
					cx.m.BoiteCorps(static_cast<uint32>(&c - cx.m.corps.Data()), mn, mx);
					const float32 R = (mx.x - mn.x) * 0.5f;
					const NkVec2 hl = cx.E(centre + NkV2(-R * 0.35f, R * 0.38f));
					cx.dl.AddEllipseFilled(hl, R * 0.20f * cx.cam.zoom, R * 0.13f * cx.cam.zoom, NkColor(255, 255, 255, 90), 14);
				}
			}

			bool CelluleDechiree(const NkMonde &m, const NkCorps &c, int32 i, int32 j) {
				const int32 nx = c.nx;
				const int32 ny = c.ny;
				const uint32 attendus = static_cast<uint32>(ny * (nx - 1) + (ny - 1) * nx);
				if (c.lienNombre < attendus) {
					return false;
				}
				const uint32 h0 = c.lienDebut + static_cast<uint32>(j * (nx - 1) + i);
				const uint32 h1 = c.lienDebut + static_cast<uint32>((j + 1) * (nx - 1) + i);
				const uint32 base = c.lienDebut + static_cast<uint32>(ny * (nx - 1));
				const uint32 v0 = base + static_cast<uint32>(j * nx + i);
				const uint32 v1 = base + static_cast<uint32>(j * nx + i + 1);
				return m.liens[h0].casse || m.liens[h1].casse || m.liens[v0].casse || m.liens[v1].casse;
			}

			void DessinerGrille(Contexte &cx, const NkCorps &c) {
				const int32 nx = c.nx;
				const int32 ny = c.ny;
				if (nx < 2 || ny < 2 || c.nombre != static_cast<uint32>(nx * ny)) {
					return;
				}
				auto id = [&](int32 i, int32 j) { return c.debut + static_cast<uint32>(j * nx + i); };
				const NkColor base = CouleurCorps(c);
				const float32 aireRepos = c.espacement * c.espacement;
				const bool tissu = c.mat == NkMateriau::NK_TISSU;

				if (c.mat == NkMateriau::NK_CAISSE) {
					const NkVec2 a = cx.E(id(0, 0)), b = cx.E(id(nx - 1, 0)), cc = cx.E(id(nx - 1, ny - 1)), d = cx.E(id(0, ny - 1));
					// Le bord exterieur englobe le rayon des particules.
					const NkV2 ctr = (cx.m.particules[id(0, 0)].pos + cx.m.particules[id(nx - 1, ny - 1)].pos) * 0.5f;
					const float32 r = cx.m.particules[id(0, 0)].rayon;
					auto gonfle = [&](uint32 i) {
						const NkV2 p = cx.m.particules[i].pos;
						const NkV2 dir = p - ctr;
						const float32 l = NkLen(dir);
						return cx.E(l > 0.f ? p + dir / l * (r * 1.3f) : p);
					};
					const NkVec2 A = gonfle(id(0, 0)), B = gonfle(id(nx - 1, 0)), C = gonfle(id(nx - 1, ny - 1)), D = gonfle(id(0, ny - 1));
					const NkColor bord = NkAssombrir(base, 0.55f);
					cx.dl.AddTriangleFilled(A, B, C, bord);
					cx.dl.AddTriangleFilled(A, C, D, bord);
					auto lerp = [](const NkVec2 &p, const NkVec2 &q, float32 t) { return NkVec2{p.x + (q.x - p.x) * t, p.y + (q.y - p.y) * t}; };
					const float32 e = 0.11f;
					const NkVec2 A2 = lerp(A, C, e), C2 = lerp(C, A, e), B2 = lerp(B, D, e), D2 = lerp(D, B, e);
					cx.dl.AddTriangleMultiColor(A2, B2, C2, base, base, NkAssombrir(base, 0.15f));
					cx.dl.AddTriangleMultiColor(A2, C2, D2, base, NkAssombrir(base, 0.15f), NkEclaircir(base, 0.08f));
					cx.dl.AddLine(A2, C2, bord, NkMaxf(1.5f, 4.f * cx.cam.zoom));
					cx.dl.AddLine(B2, D2, bord, NkMaxf(1.5f, 4.f * cx.cam.zoom));
					for (int32 k = 1; k < 3; ++k) {
						const float32 t = static_cast<float32>(k) / 3.f;
						cx.dl.AddLine(lerp(A2, D2, t), lerp(B2, C2, t), NkAvecAlpha(bord, 120), NkMaxf(1.f, 1.5f * cx.cam.zoom));
					}
					(void)a;
					(void)b;
					(void)cc;
					(void)d;
					return;
				}

				for (int32 j = 0; j + 1 < ny; ++j) {
					for (int32 i = 0; i + 1 < nx; ++i) {
						if (tissu && CelluleDechiree(cx.m, c, i, j)) {
							continue;
						}
						const NkV2 &pa = cx.m.particules[id(i, j)].pos;
						const NkV2 &pb = cx.m.particules[id(i + 1, j)].pos;
						const NkV2 &pc = cx.m.particules[id(i + 1, j + 1)].pos;
						const NkV2 &pd = cx.m.particules[id(i, j + 1)].pos;
						if (!cx.Visible(pa, 60.f)) {
							continue;
						}
						const float32 aire = 0.5f * (NkCross(pb - pa, pc - pa) + NkCross(pc - pa, pd - pa));
						NkColor col = base;
						if (tissu) {
							const bool rayure = ((i / 3) & 1) != 0;
							col = rayure ? NkEclaircir(base, 0.16f) : base;
							// Grille descendante : l'endroit a une aire NEGATIVE.
							if (aire > 0.f) {
								col = NkAssombrir(col, 0.45f); // le verso
							}
							const float32 ratio = std::fabs(aire) / aireRepos;
							col = NkAssombrir(col, NkClampf((1.f - ratio) * 0.8f, 0.f, 0.4f));
						} else {
							const float32 ratio = aire / aireRepos;
							col = ratio < 1.f ? NkEclaircir(base, NkClampf((1.f - ratio) * 1.2f, 0.f, 0.5f))
											  : NkAssombrir(base, NkClampf((ratio - 1.f) * 1.2f, 0.f, 0.4f));
							col = NkAvecAlpha(col, 235);
						}
						const NkVec2 a = cx.E(pa), b = cx.E(pb), cc = cx.E(pc), d = cx.E(pd);
						cx.dl.AddTriangleFilled(a, b, cc, col);
						cx.dl.AddTriangleFilled(a, cc, d, col);
					}
				}
				if (!tissu) {
					// Contour de la gelee + reflet.
					NkVec2 bord[256];
					int32 n = 0;
					for (int32 i = 0; i < nx && n < 255; ++i) {
						bord[n++] = cx.E(id(i, 0));
					}
					for (int32 j = 1; j < ny && n < 255; ++j) {
						bord[n++] = cx.E(id(nx - 1, j));
					}
					for (int32 i = nx - 2; i >= 0 && n < 255; --i) {
						bord[n++] = cx.E(id(i, ny - 1));
					}
					for (int32 j = ny - 2; j > 0 && n < 255; --j) {
						bord[n++] = cx.E(id(0, j));
					}
					cx.dl.AddPolyline(bord, n, NkAssombrir(base, 0.4f), NkMaxf(1.5f, 3.f * cx.cam.zoom), true);
					const NkVec2 h0 = cx.E(id(1, ny - 2));
					const NkVec2 h1 = cx.E(id(3, ny - 2));
					cx.dl.AddLine(h0, h1, NkColor(255, 255, 255, 110), NkMaxf(2.f, 5.f * cx.cam.zoom));
				} else {
					// Barre de tringle au-dessus des epingles.
					NkVec2 haut[64];
					int32 n = 0;
					for (int32 i = 0; i < nx && n < 64; ++i) {
						if (cx.m.particules[id(i, 0)].epingle) {
							haut[n++] = cx.E(id(i, 0));
						}
					}
					if (n >= 2) {
						cx.dl.AddLine(haut[0], haut[n - 1], NkColor(150, 152, 160), NkMaxf(2.f, 5.f * cx.cam.zoom));
					}
				}
			}

			void DessinerFluide(Contexte &cx, const NkCorps &c) {
				const NkColor base = CouleurCorps(c);
				const float32 z = cx.cam.zoom;
				const bool eau = c.mat == NkMateriau::NK_EAU;
				const bool sable = c.mat == NkMateriau::NK_SABLE;
				if (sable) {
					for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
						const NkParticule &p = cx.m.particules[i];
						if (!cx.Visible(p.pos, 20.f)) {
							continue;
						}
						// Chaque grain a SA teinte : un tas uniforme ne se lit pas.
						const uint32 hsh = (i * 2654435761u) >> 24;
						const float32 t = static_cast<float32>(hsh) / 255.f;
						const NkColor col = t < 0.5f ? NkAssombrir(base, t * 0.35f) : NkEclaircir(base, (t - 0.5f) * 0.3f);
						cx.dl.AddCircleFilled(cx.E(p.pos), p.rayon * z * 1.05f, col, Segments(p.rayon * z));
					}
					return;
				}
				// 1. halo
				const NkColor halo = NkAvecAlpha(base, eau ? 70 : 95);
				for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
					const NkParticule &p = cx.m.particules[i];
					if (!cx.Visible(p.pos, 30.f)) {
						continue;
					}
					const float32 r = p.rayon * z * 2.1f;
					cx.dl.AddCircleFilled(cx.E(p.pos), r, halo, Segments(r));
				}
				// 2. coeur
				const NkColor fonce = NkAssombrir(base, 0.18f);
				for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
					const NkParticule &p = cx.m.particules[i];
					if (!cx.Visible(p.pos, 30.f)) {
						continue;
					}
					NkColor col = fonce;
					if (eau) {
						// L'ecume : une eau rapide blanchit.
						col = NkMelange(base, NkColor(230, 245, 255), NkClampf((NkLen(p.vit) - 150.f) / 700.f, 0.f, 0.8f));
					} else {
						// Le coeur d'un blob est plus sombre que sa peau.
						col = NkMelange(base, fonce, NkClampf(p.densite / 3.f, 0.f, 1.f));
					}
					const float32 r = p.rayon * z * 1.45f;
					cx.dl.AddCircleFilled(cx.E(p.pos), r, col, Segments(r));
				}
				// 3. brillance (visqueux)
				if (!eau) {
					const NkColor brille(255, 255, 255, 55);
					for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
						const NkParticule &p = cx.m.particules[i];
						if (p.densite > 1.3f || !cx.Visible(p.pos, 30.f)) {
							continue; // seulement la PEAU brille
						}
						const float32 r = p.rayon * z * 0.55f;
						cx.dl.AddCircleFilled(cx.E(p.pos + NkV2(-p.rayon * 0.4f, p.rayon * 0.5f)), r, brille, Segments(r));
					}
				}
			}

			void DessinerAtomes(Contexte &cx, const NkCorps &c) {
				const NkColor base = CouleurCorps(c);
				const float32 z = cx.cam.zoom;
				for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
					const NkParticule &p = cx.m.particules[i];
					if (!cx.Visible(p.pos, 30.f)) {
						continue;
					}
					const float32 r = p.rayon * z;
					const float32 chaleur = NkClampf((NkLen(p.vit) - 80.f) / 700.f, 0.f, 1.f);
					const NkColor col = NkMelange(base, NkColor(255, 140, 50), chaleur * 0.85f);
					cx.dl.AddCircleFilled(cx.E(p.pos), r, NkAssombrir(col, 0.5f), Segments(r));
					cx.dl.AddCircleFilled(cx.E(p.pos), r * 0.78f, col, Segments(r));
					cx.dl.AddCircleFilled(cx.E(p.pos + NkV2(-p.rayon * 0.3f, p.rayon * 0.3f)), r * 0.28f,
										  NkColor(255, 255, 255, 120), 8);
				}
			}

			void DessinerHalosChaleur(Contexte &cx) {
				bool aucun = true;
				for (uint32 ci = 0; ci < cx.m.corps.Size(); ++ci) {
					if (cx.m.corps[ci].mat == NkMateriau::NK_ATOMES && cx.m.corps[ci].visible) {
						aucun = false;
					}
				}
				if (aucun) {
					return;
				}
				cx.dl.PushBlend(NkGuiBlend::PlusLighter);
				for (uint32 ci = 0; ci < cx.m.corps.Size(); ++ci) {
					const NkCorps &c = cx.m.corps[ci];
					if (c.mat != NkMateriau::NK_ATOMES || !c.visible) {
						continue;
					}
					for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
						const NkParticule &p = cx.m.particules[i];
						const float32 chaleur = NkClampf((NkLen(p.vit) - 120.f) / 700.f, 0.f, 1.f);
						if (chaleur <= 0.02f || !cx.Visible(p.pos, 40.f)) {
							continue;
						}
						// Additif : les halos voisins s'additionnent. Discret par atome,
						// il suffit a faire ROUGEOYER le cristal sans le noyer de blanc.
						const float32 r = p.rayon * cx.cam.zoom * 2.0f;
						cx.dl.AddCircleFilled(cx.E(p.pos), r, NkColor(255, 90, 20, static_cast<uint8>(chaleur * 38.f)), Segments(r));
					}
				}
				cx.dl.PopBlend();
			}

			void DessinerCorde(Contexte &cx, const NkCorps &c) {
				const NkColor base = CouleurCorps(c);
				const float32 z = cx.cam.zoom;
				const bool pont = std::strncmp(c.nom, "Pont", 4) == 0;
				if (pont) {
					for (uint32 i = c.debut; i + 1 < c.debut + c.nombre; ++i) {
						const NkParticule &a = cx.m.particules[i];
						const NkParticule &b = cx.m.particules[i + 1];
						const float32 ep = a.rayon * 2.f * z;
						cx.dl.AddLine(cx.E(a.pos), cx.E(b.pos), NkAssombrir(base, 0.5f), ep + 2.f);
						cx.dl.AddLine(cx.E(a.pos), cx.E(b.pos), (i & 1) ? base : NkEclaircir(base, 0.1f), ep);
					}
					for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
						cx.dl.AddCircleFilled(cx.E(i), NkMaxf(1.5f, 2.2f * z), NkColor(60, 50, 40), 6);
					}
					return;
				}
				NkVec2 pts[256];
				int32 n = 0;
				for (uint32 i = c.debut; i < c.debut + c.nombre && n < 256; ++i) {
					pts[n++] = cx.E(i);
				}
				cx.dl.AddPolyline(pts, n, NkAssombrir(base, 0.45f), NkMaxf(3.5f, 8.f * z));
				cx.dl.AddPolyline(pts, n, base, NkMaxf(2.f, 5.f * z));
				for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
					const NkParticule &p = cx.m.particules[i];
					if (p.rayon > 8.f) { // le lest
						const float32 r = NkMaxf(p.rayon * z, 5.f);
						cx.dl.AddCircleFilled(cx.E(p.pos), r, NkColor(70, 72, 80), Segments(r));
						cx.dl.AddCircleFilled(cx.E(p.pos), r * 0.82f, NkColor(128, 132, 146), Segments(r));
						cx.dl.AddCircleFilled(cx.E(p.pos + NkV2(-p.rayon * 0.3f, p.rayon * 0.3f)), r * 0.3f, NkColor(220, 225, 235, 160), 8);
					}
				}
			}

			// -----------------------------------------------------------------
			// Modes de diagnostic
			// -----------------------------------------------------------------
			void DessinerLiens(Contexte &cx, bool tension) {
				const float32 ep = NkMaxf(1.f, 1.6f * cx.cam.zoom);
				for (uint32 k = 0; k < cx.m.liens.Size(); ++k) {
					const NkLien &l = cx.m.liens[k];
					if (l.casse || !cx.m.corps[l.corps].visible) {
						continue;
					}
					const NkV2 &a = cx.m.particules[l.a].pos;
					if (!cx.Visible(a, 60.f)) {
						continue;
					}
					NkColor col;
					if (tension) {
						col = CouleurTension(l.tension);
					} else {
						switch (l.genre) {
							case NkGenreLien::NK_STRUCTURE:
								col = NkColor(220, 220, 220, 200);
								break;
							case NkGenreLien::NK_CISAILLEMENT:
								col = NkColor(110, 160, 255, 110);
								break;
							case NkGenreLien::NK_FLEXION:
								col = NkColor(255, 170, 80, 70);
								break;
							default:
								col = NkColor(120, 255, 170, 170);
								break;
						}
					}
					cx.dl.AddLine(cx.E(a), cx.E(l.b), col, ep);
				}
			}

			void DessinerParticules(Contexte &cx, bool vitesse, bool pleines) {
				for (uint32 i = 0; i < cx.m.particules.Size(); ++i) {
					const NkParticule &p = cx.m.particules[i];
					if (!cx.m.corps[p.corps].visible || !cx.Visible(p.pos, 20.f)) {
						continue;
					}
					const float32 r = NkMaxf(1.5f, p.rayon * cx.cam.zoom);
					if (vitesse) {
						cx.dl.AddCircleFilled(cx.E(p.pos), r, CarteChaleur(NkLen(p.vit) / 900.f), Segments(r));
					} else if (pleines) {
						cx.dl.AddCircleFilled(cx.E(p.pos), r, NkColor(170, 170, 176), Segments(r));
					} else {
						cx.dl.AddCircle(cx.E(p.pos), r, NkColor(235, 235, 235, 150), 1.f, Segments(r));
					}
				}
			}

			void DessinerSelection(Contexte &cx) {
				const int32 ci = cx.m.IndexCorps(cx.o.selectionId);
				if (ci < 0) {
					return;
				}
				NkV2 mn, mx;
				cx.m.BoiteCorps(static_cast<uint32>(ci), mn, mx);
				const NkVec2 a = cx.E(NkV2(mn.x, mx.y));
				const NkVec2 b = cx.E(NkV2(mx.x, mn.y));
				const float32 m = 5.f;
				const NkRect r{a.x - m, a.y - m, b.x - a.x + 2.f * m, b.y - a.y + 2.f * m};
				const float32 coin = NkMinf(NkMinf(r.w, r.h) * 0.3f, 18.f);
				const NkColor c = kSelectionVue;
				const float32 ep = 2.f;
				cx.dl.AddRect(r, NkAvecAlpha(c, 60), 1.f);
				// Equerres aux quatre coins.
				cx.dl.AddLine({r.x, r.y}, {r.x + coin, r.y}, c, ep);
				cx.dl.AddLine({r.x, r.y}, {r.x, r.y + coin}, c, ep);
				cx.dl.AddLine({r.x + r.w, r.y}, {r.x + r.w - coin, r.y}, c, ep);
				cx.dl.AddLine({r.x + r.w, r.y}, {r.x + r.w, r.y + coin}, c, ep);
				cx.dl.AddLine({r.x, r.y + r.h}, {r.x + coin, r.y + r.h}, c, ep);
				cx.dl.AddLine({r.x, r.y + r.h}, {r.x, r.y + r.h - coin}, c, ep);
				cx.dl.AddLine({r.x + r.w, r.y + r.h}, {r.x + r.w - coin, r.y + r.h}, c, ep);
				cx.dl.AddLine({r.x + r.w, r.y + r.h}, {r.x + r.w, r.y + r.h - coin}, c, ep);
				// Pivot au centre de masse.
				const NkVec2 ce = cx.E(cx.m.CentreCorps(static_cast<uint32>(ci)));
				cx.dl.AddCircleFilled(ce, 4.f, c, 10);
				cx.dl.AddLine({ce.x, ce.y}, {ce.x + 22.f, ce.y}, kAxeX, 2.f);
				cx.dl.AddLine({ce.x, ce.y}, {ce.x, ce.y - 22.f}, kAxeY, 2.f);
			}
		} // namespace

		void NkDessinerMonde(NkGuiDrawList &dl, const NkMonde &m, const NkCamera2D &cam, const NkOptionsVue &o) noexcept {
			const NkV2 hg = cam.Monde(cam.vue.x, cam.vue.y);
			const NkV2 bd = cam.Monde(cam.vue.x + cam.vue.w, cam.vue.y + cam.vue.h);
			Contexte cx{dl, m, cam, o, NkRect{hg.x, bd.y, bd.x, hg.y}};

			DessinerDecor(cx);

			if (o.mode == NkModeVue::NK_ECLAIRE) {
				// Les fluides d'abord : les solides passent DEVANT la nappe.
				for (uint32 ci = 0; ci < m.corps.Size(); ++ci) {
					const NkCorps &c = m.corps[ci];
					if (c.visible && (NkEstFluide(c.mat) || c.mat == NkMateriau::NK_SABLE)) {
						DessinerFluide(cx, c);
					}
				}
				// Liaisons atomiques sous les atomes.
				const float32 ep = NkMaxf(1.f, 3.f * cam.zoom);
				for (uint32 k = 0; k < m.liens.Size(); ++k) {
					const NkLien &l = m.liens[k];
					if (l.casse || l.genre != NkGenreLien::NK_LIAISON) {
						continue;
					}
					const NkCorps &c = m.corps[l.corps];
					if (c.mat != NkMateriau::NK_ATOMES || !c.visible) {
						continue;
					}
					cx.dl.AddLine(cx.E(l.a), cx.E(l.b), NkAvecAlpha(NkEclaircir(CouleurCorps(c), 0.3f), 150), ep);
				}
				for (uint32 ci = 0; ci < m.corps.Size(); ++ci) {
					const NkCorps &c = m.corps[ci];
					if (!c.visible) {
						continue;
					}
					switch (c.mat) {
						case NkMateriau::NK_BALLON:
							DessinerBallon(cx, c);
							break;
						case NkMateriau::NK_GELEE:
						case NkMateriau::NK_CAISSE:
						case NkMateriau::NK_TISSU:
							DessinerGrille(cx, c);
							break;
						case NkMateriau::NK_ATOMES:
							DessinerAtomes(cx, c);
							break;
						case NkMateriau::NK_CORDE:
							DessinerCorde(cx, c);
							break;
						default:
							break;
					}
				}
				DessinerHalosChaleur(cx);
				if (o.liens) {
					DessinerLiens(cx, false);
				}
				if (o.particules) {
					DessinerParticules(cx, false, false);
				}
			} else if (o.mode == NkModeVue::NK_FILAIRE) {
				DessinerLiens(cx, false);
				DessinerParticules(cx, false, false);
			} else if (o.mode == NkModeVue::NK_CONTRAINTES) {
				DessinerParticules(cx, false, true);
				DessinerLiens(cx, true);
			} else {
				DessinerLiens(cx, false);
				DessinerParticules(cx, true, true);
			}

			if (o.vitesses) {
				for (uint32 i = 0; i < m.particules.Size(); ++i) {
					const NkParticule &p = m.particules[i];
					if (NkLen2(p.vit) < 400.f || !cx.Visible(p.pos, 20.f)) {
						continue;
					}
					dl.AddLine(cx.E(p.pos), cx.E(p.pos + p.vit * 0.05f), NkColor(255, 230, 90, 170), 1.f);
				}
			}
			if (o.epingles) {
				for (uint32 i = 0; i < m.particules.Size(); ++i) {
					const NkParticule &p = m.particules[i];
					if (!p.epingle || !cx.Visible(p.pos, 20.f)) {
						continue;
					}
					const NkVec2 e = cx.E(p.pos);
					dl.AddCircleFilled(e, 4.5f, NkColor(30, 10, 10), 10);
					dl.AddCircleFilled(e, 3.5f, kEpingle, 10);
					dl.AddCircleFilled({e.x - 1.f, e.y - 1.f}, 1.2f, NkColor(255, 220, 220), 6);
				}
			}
			DessinerSelection(cx);
		}

	} // namespace physic2d
} // namespace nkentseu
