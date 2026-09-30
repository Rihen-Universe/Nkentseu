// =============================================================================
// NkUnkenyRenduParticules.cpp
// =============================================================================
#include "Unkeny/Rendu/NkUnkenyRenduParticules.h"
#include "NKMath/NkFunctions.h"

namespace nkentseu {
	namespace unkeny {

		using nkgui::NkColor;
		using nkgui::NkGuiBlend;
		using nkgui::NkGuiDrawList;
		using physics::NkCorpsP2D;
		using physics::NkLien2D;
		using physics::NkMateriauP2D;
		using physics::NkParticule2D;
		using physics::NkParticules2D;

		const char *NkModeRenduParticulesNom(NkModeRenduParticules m) noexcept {
			switch (m) {
				case NkModeRenduParticules::NK_ECLAIRE:
					return "Éclairé";
				case NkModeRenduParticules::NK_FILAIRE:
					return "Filaire";
				case NkModeRenduParticules::NK_CONTRAINTES:
					return "Contraintes";
				case NkModeRenduParticules::NK_VITESSE:
					return "Vitesse";
				default:
					return "?";
			}
		}

		namespace {
			float32 Clampf(float32 v, float32 a, float32 b) noexcept {
				return v < a ? a : (v > b ? b : v);
			}
			float32 Maxf(float32 a, float32 b) noexcept {
				return a > b ? a : b;
			}
			float32 Longueur(const NkVec2f &v) noexcept {
				return math::NkSqrt(v.x * v.x + v.y * v.y);
			}
			float32 Cross(const NkVec2f &a, const NkVec2f &b) noexcept {
				return a.x * b.y - a.y * b.x;
			}
			NkColor Rgba(uint32 c, uint8 a = 255) noexcept {
				return NkColor(static_cast<uint8>((c >> 24) & 0xFFu), static_cast<uint8>((c >> 16) & 0xFFu),
							   static_cast<uint8>((c >> 8) & 0xFFu), a);
			}
			NkColor Alpha(const NkColor &c, uint8 a) noexcept {
				return NkColor(c.r, c.g, c.b, a);
			}
			NkColor Melange(const NkColor &a, const NkColor &b, float32 t) noexcept {
				t = Clampf(t, 0.f, 1.f);
				auto m = [t](uint8 x, uint8 y) {
					return static_cast<uint8>(static_cast<float32>(x) + (static_cast<float32>(y) - static_cast<float32>(x)) * t);
				};
				return NkColor(m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), m(a.a, b.a));
			}
			NkColor Eclaircir(const NkColor &c, float32 t) noexcept {
				return Melange(c, NkColor(255, 255, 255, c.a), t);
			}
			NkColor Assombrir(const NkColor &c, float32 t) noexcept {
				return Melange(c, NkColor(0, 0, 0, c.a), t);
			}
			NkColor CarteChaleur(float32 t) noexcept {
				t = Clampf(t, 0.f, 1.f);
				const NkColor c0(40, 70, 200), c1(40, 200, 230), c2(250, 220, 60), c3(250, 60, 40);
				if (t < 0.33f) {
					return Melange(c0, c1, t / 0.33f);
				}
				if (t < 0.66f) {
					return Melange(c1, c2, (t - 0.33f) / 0.33f);
				}
				return Melange(c2, c3, (t - 0.66f) / 0.34f);
			}
			NkColor CouleurTension(float32 t) noexcept {
				const float32 k = Clampf(t * 10.f, -1.f, 1.f);
				const NkColor neutre(90, 210, 110);
				return k >= 0.f ? Melange(neutre, NkColor(255, 64, 40), k) : Melange(neutre, NkColor(60, 120, 255), -k);
			}
			int32 Segments(float32 rayonPx) noexcept {
				const int32 s = static_cast<int32>(rayonPx * 0.9f) + 6;
				return s > 24 ? 24 : s;
			}

			struct Contexte {
					NkGuiDrawList &dl;
					const NkParticules2D &p;
					const NkVue2D &cam;
					NkRect visible;
					float32 z;

					bool Visible(const NkVec2f &q, float32 marge) const noexcept {
						return q.x > visible.x - marge && q.x < visible.x + visible.w + marge && q.y > visible.y - marge &&
							   q.y < visible.y + visible.h + marge;
					}
					NkVec2f E(const NkVec2f &q) const noexcept {
						return cam.MondeVersEcran(q);
					}
					NkVec2f E(uint32 i) const noexcept {
						return cam.MondeVersEcran(p.particules[i].pos);
					}
			};

			void Ballon(Contexte &cx, const NkCorpsP2D &c, const NkColor &couleur) {
				const uint32 n = c.nombre;
				if (n < 3) {
					return;
				}
				NkVec2f centre(0.f, 0.f);
				for (uint32 k = 0; k < n; ++k) {
					centre += cx.p.particules[c.debut + k].pos;
				}
				centre = centre / static_cast<float32>(n);
				const bool creve = c.pression <= 0.f;
				const NkColor base = creve ? Assombrir(couleur, 0.35f) : couleur;
				const NkVec2f ce = cx.E(centre);
				for (uint32 k = 0; k < n; ++k) {
					cx.dl.AddTriangleMultiColor(ce, cx.E(c.debut + k), cx.E(c.debut + (k + 1) % n), Eclaircir(base, 0.18f), base, base);
				}
				NkVec2f anneau[128];
				const uint32 nn = n < 127 ? n : 127;
				for (uint32 k = 0; k < nn; ++k) {
					anneau[k] = cx.E(c.debut + k);
				}
				cx.dl.AddPolyline(anneau, static_cast<int32>(nn), Assombrir(base, 0.45f), Maxf(1.5f, 0.03f * cx.z), true);
				if (!creve) {
					float32 R = 0.f;
					for (uint32 k = 0; k < n; ++k) {
						R = Maxf(R, Longueur(cx.p.particules[c.debut + k].pos - centre));
					}
					cx.dl.AddEllipseFilled(cx.E(centre + NkVec2f(-R * 0.35f, R * 0.38f)), R * 0.20f * cx.z, R * 0.13f * cx.z,
										   NkColor(255, 255, 255, 90), 14);
				}
			}

			bool CelluleDechiree(const NkParticules2D &p, const NkCorpsP2D &c, int32 i, int32 j) noexcept {
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
				return p.liens[h0].casse || p.liens[h1].casse || p.liens[v0].casse || p.liens[v1].casse;
			}

			void Grille(Contexte &cx, const NkCorpsP2D &c, const NkColor &base) {
				const int32 nx = c.nx;
				const int32 ny = c.ny;
				if (nx < 2 || ny < 2 || c.nombre != static_cast<uint32>(nx * ny)) {
					return;
				}
				auto id = [&](int32 i, int32 j) { return c.debut + static_cast<uint32>(j * nx + i); };
				const float32 aireRepos = c.espacement * c.espacement;
				const bool tissu = c.mat == NkMateriauP2D::NK_TISSU;
				for (int32 j = 0; j + 1 < ny; ++j) {
					for (int32 i = 0; i + 1 < nx; ++i) {
						if (tissu && CelluleDechiree(cx.p, c, i, j)) {
							continue;
						}
						const NkVec2f &pa = cx.p.particules[id(i, j)].pos;
						const NkVec2f &pb = cx.p.particules[id(i + 1, j)].pos;
						const NkVec2f &pc = cx.p.particules[id(i + 1, j + 1)].pos;
						const NkVec2f &pd = cx.p.particules[id(i, j + 1)].pos;
						if (!cx.Visible(pa, 0.6f)) {
							continue;
						}
						const float32 aire = 0.5f * (Cross(pb - pa, pc - pa) + Cross(pc - pa, pd - pa));
						NkColor col = base;
						if (tissu) {
							col = ((i / 3) & 1) != 0 ? Eclaircir(base, 0.16f) : base;
							if (aire > 0.f) {
								col = Assombrir(col, 0.45f); // grille descendante : l'endroit a une aire NEGATIVE
							}
							const float32 r = (aire < 0.f ? -aire : aire) / aireRepos;
							col = Assombrir(col, Clampf((1.f - r) * 0.8f, 0.f, 0.4f));
						} else {
							const float32 r = aire / aireRepos;
							col = r < 1.f ? Eclaircir(base, Clampf((1.f - r) * 1.2f, 0.f, 0.5f))
										  : Assombrir(base, Clampf((r - 1.f) * 1.2f, 0.f, 0.4f));
							col = Alpha(col, 235);
						}
						const NkVec2f a = cx.E(pa), b = cx.E(pb), cc = cx.E(pc), d = cx.E(pd);
						cx.dl.AddTriangleFilled(a, b, cc, col);
						cx.dl.AddTriangleFilled(a, cc, d, col);
					}
				}
				if (!tissu) {
					NkVec2f bord[256];
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
					cx.dl.AddPolyline(bord, n, Assombrir(base, 0.4f), Maxf(1.5f, 0.03f * cx.z), true);
					if (nx > 3 && ny > 2) {
						cx.dl.AddLine(cx.E(id(1, ny - 2)), cx.E(id(3, ny - 2)), NkColor(255, 255, 255, 110), Maxf(2.f, 0.05f * cx.z));
					}
				} else {
					NkVec2f haut[64];
					int32 n = 0;
					for (int32 i = 0; i < nx && n < 64; ++i) {
						if (cx.p.particules[id(i, 0)].epingle) {
							haut[n++] = cx.E(id(i, 0));
						}
					}
					if (n >= 2) {
						cx.dl.AddLine(haut[0], haut[n - 1], NkColor(150, 152, 160), Maxf(2.f, 0.05f * cx.z));
					}
				}
			}

			void Fluide(Contexte &cx, const NkCorpsP2D &c, const NkColor &base) {
				const bool eau = c.mat == NkMateriauP2D::NK_EAU;
				if (c.mat == NkMateriauP2D::NK_SABLE) {
					for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
						const NkParticule2D &q = cx.p.particules[i];
						if (!cx.Visible(q.pos, 0.2f)) {
							continue;
						}
						const float32 t = static_cast<float32>((i * 2654435761u) >> 24) / 255.f;
						const NkColor col = t < 0.5f ? Assombrir(base, t * 0.35f) : Eclaircir(base, (t - 0.5f) * 0.3f);
						cx.dl.AddCircleFilled(cx.E(q.pos), q.rayon * cx.z * 1.05f, col, Segments(q.rayon * cx.z));
					}
					return;
				}
				const NkColor halo = Alpha(base, eau ? 70 : 95);
				for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
					const NkParticule2D &q = cx.p.particules[i];
					if (cx.Visible(q.pos, 0.3f)) {
						const float32 r = q.rayon * cx.z * 2.1f;
						cx.dl.AddCircleFilled(cx.E(q.pos), r, halo, Segments(r));
					}
				}
				const NkColor fonce = Assombrir(base, 0.18f);
				for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
					const NkParticule2D &q = cx.p.particules[i];
					if (!cx.Visible(q.pos, 0.3f)) {
						continue;
					}
					const NkColor col = eau ? Melange(base, NkColor(230, 245, 255), Clampf((Longueur(q.vit) - 1.5f) / 7.f, 0.f, 0.8f))
											: Melange(base, fonce, Clampf(q.densite / 3.f, 0.f, 1.f));
					const float32 r = q.rayon * cx.z * 1.45f;
					cx.dl.AddCircleFilled(cx.E(q.pos), r, col, Segments(r));
				}
				if (!eau) {
					const NkColor brille(255, 255, 255, 55);
					for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
						const NkParticule2D &q = cx.p.particules[i];
						if (q.densite > 1.3f || !cx.Visible(q.pos, 0.3f)) {
							continue; // seulement la PEAU brille
						}
						const float32 r = q.rayon * cx.z * 0.55f;
						cx.dl.AddCircleFilled(cx.E(q.pos + NkVec2f(-q.rayon * 0.4f, q.rayon * 0.5f)), r, brille, Segments(r));
					}
				}
			}

			void Atomes(Contexte &cx, const NkCorpsP2D &c, const NkColor &base) {
				for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
					const NkParticule2D &q = cx.p.particules[i];
					if (!cx.Visible(q.pos, 0.3f)) {
						continue;
					}
					const float32 r = q.rayon * cx.z;
					const float32 chaleur = Clampf((Longueur(q.vit) - 0.8f) / 7.f, 0.f, 1.f);
					const NkColor col = Melange(base, NkColor(255, 140, 50), chaleur * 0.85f);
					cx.dl.AddCircleFilled(cx.E(q.pos), r, Assombrir(col, 0.5f), Segments(r));
					cx.dl.AddCircleFilled(cx.E(q.pos), r * 0.78f, col, Segments(r));
					cx.dl.AddCircleFilled(cx.E(q.pos + NkVec2f(-q.rayon * 0.3f, q.rayon * 0.3f)), r * 0.28f, NkColor(255, 255, 255, 120), 8);
				}
			}

			void Corde(Contexte &cx, const NkCorpsP2D &c, const NkColor &base) {
				// Un pont a des particules EPAISSES (planches), une corde de fines.
				const bool pont = c.nombre > 0 && cx.p.particules[c.debut].rayon > 0.06f;
				if (pont) {
					for (uint32 i = c.debut; i + 1 < c.debut + c.nombre; ++i) {
						const NkParticule2D &a = cx.p.particules[i];
						const NkParticule2D &b = cx.p.particules[i + 1];
						const float32 ep = a.rayon * 2.f * cx.z;
						cx.dl.AddLine(cx.E(a.pos), cx.E(b.pos), Assombrir(base, 0.5f), ep + 2.f);
						cx.dl.AddLine(cx.E(a.pos), cx.E(b.pos), (i & 1) ? base : Eclaircir(base, 0.1f), ep);
					}
					for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
						cx.dl.AddCircleFilled(cx.E(i), Maxf(1.5f, 0.022f * cx.z), NkColor(60, 50, 40), 6);
					}
					return;
				}
				NkVec2f pts[256];
				int32 n = 0;
				for (uint32 i = c.debut; i < c.debut + c.nombre && n < 256; ++i) {
					pts[n++] = cx.E(i);
				}
				cx.dl.AddPolyline(pts, n, Assombrir(base, 0.45f), Maxf(3.5f, 0.08f * cx.z));
				cx.dl.AddPolyline(pts, n, base, Maxf(2.f, 0.05f * cx.z));
				for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
					const NkParticule2D &q = cx.p.particules[i];
					if (q.rayon > 0.08f) { // le lest
						const float32 r = Maxf(q.rayon * cx.z, 5.f);
						cx.dl.AddCircleFilled(cx.E(q.pos), r, NkColor(70, 72, 80), Segments(r));
						cx.dl.AddCircleFilled(cx.E(q.pos), r * 0.82f, NkColor(128, 132, 146), Segments(r));
						cx.dl.AddCircleFilled(cx.E(q.pos + NkVec2f(-q.rayon * 0.3f, q.rayon * 0.3f)), r * 0.3f, NkColor(220, 225, 235, 160), 8);
					}
				}
			}

			void Liens(Contexte &cx, bool tension, const NkVector<uint8> &visible) {
				const float32 ep = Maxf(1.f, 0.016f * cx.z);
				for (uint32 k = 0; k < cx.p.liens.Size(); ++k) {
					const NkLien2D &l = cx.p.liens[k];
					if (l.casse || !visible[l.corps]) {
						continue;
					}
					const NkVec2f &a = cx.p.particules[l.a].pos;
					if (!cx.Visible(a, 0.6f)) {
						continue;
					}
					NkColor col;
					if (tension) {
						col = CouleurTension(l.tension);
					} else {
						switch (l.genre) {
							case physics::NkGenreLien2D::NK_STRUCTURE:
								col = NkColor(220, 220, 220, 200);
								break;
							case physics::NkGenreLien2D::NK_CISAILLEMENT:
								col = NkColor(110, 160, 255, 110);
								break;
							case physics::NkGenreLien2D::NK_FLEXION:
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

			void Particules(Contexte &cx, bool vitesse, bool pleines, const NkVector<uint8> &visible) {
				for (uint32 i = 0; i < cx.p.particules.Size(); ++i) {
					const NkParticule2D &q = cx.p.particules[i];
					if (!visible[q.corps] || !cx.Visible(q.pos, 0.2f)) {
						continue;
					}
					const float32 r = Maxf(1.5f, q.rayon * cx.z);
					if (vitesse) {
						cx.dl.AddCircleFilled(cx.E(q.pos), r, CarteChaleur(Longueur(q.vit) / 9.f), Segments(r));
					} else if (pleines) {
						cx.dl.AddCircleFilled(cx.E(q.pos), r, NkColor(170, 170, 176), Segments(r));
					} else {
						cx.dl.AddCircle(cx.E(q.pos), r, NkColor(235, 235, 235, 150), 1.f, Segments(r));
					}
				}
			}
		} // namespace

		void NkDessinerCorpsMous(NkGuiDrawList &dl, NkScene &scene, const NkOptionsRenduParticules &o) {
			const NkParticules2D *pp = scene.Particules();
			if (pp == nullptr) {
				return;
			}
			const NkParticules2D &p = *pp;
			const NkVue2D &cam = scene.Camera();
			Contexte cx{dl, p, cam, cam.ZoneVisible(), cam.Zoom()};

			// Couleur et visibilite viennent de l'ENTITE (NkCorpsMou2D).
			const uint32 nc = static_cast<uint32>(p.corps.Size());
			NkVector<uint32> couleur;
			NkVector<uint8> visible;
			couleur.Resize(nc);
			visible.Resize(nc);
			for (uint32 c = 0; c < nc; ++c) {
				couleur[c] = 0xC8C8C8FFu;
				visible[c] = 1;
			}
			scene.Monde().Query<NkCorpsMou2D>().ForEach([&](ecs::NkEntityId id, NkCorpsMou2D &m) {
				const int32 ci = p.IndexCorps(m.corpsId);
				if (ci >= 0) {
					couleur[static_cast<uint32>(ci)] = m.couleur;
					// Une entite ETEINTE (NkUnkenyActif.h) : sa matiere n'est pas rendue.
					visible[static_cast<uint32>(ci)] = (m.visible && scene.EstActive(id)) ? 1u : 0u;
				}
			});

			if (o.mode == NkModeRenduParticules::NK_ECLAIRE) {
				// Les fluides d'abord : les solides passent DEVANT la nappe.
				for (uint32 ci = 0; ci < nc; ++ci) {
					const NkCorpsP2D &c = p.corps[ci];
					if (visible[ci] && (physics::NkEstFluideP2D(c.mat) || c.mat == NkMateriauP2D::NK_SABLE)) {
						Fluide(cx, c, Rgba(couleur[ci]));
					}
				}
				const float32 ep = Maxf(1.f, 0.03f * cx.z);
				for (uint32 k = 0; k < p.liens.Size(); ++k) {
					const NkLien2D &l = p.liens[k];
					if (l.casse || l.genre != physics::NkGenreLien2D::NK_LIAISON || !visible[l.corps] ||
						p.corps[l.corps].mat != NkMateriauP2D::NK_ATOMES) {
						continue;
					}
					dl.AddLine(cx.E(l.a), cx.E(l.b), Alpha(Eclaircir(Rgba(couleur[l.corps]), 0.3f), 150), ep);
				}
				for (uint32 ci = 0; ci < nc; ++ci) {
					const NkCorpsP2D &c = p.corps[ci];
					if (!visible[ci]) {
						continue;
					}
					const NkColor col = Rgba(couleur[ci]);
					switch (c.mat) {
						case NkMateriauP2D::NK_BALLON:
							Ballon(cx, c, col);
							break;
						case NkMateriauP2D::NK_GELEE:
						case NkMateriauP2D::NK_TISSU:
							Grille(cx, c, col);
							break;
						case NkMateriauP2D::NK_ATOMES:
							Atomes(cx, c, col);
							break;
						case NkMateriauP2D::NK_CORDE:
							Corde(cx, c, col);
							break;
						default:
							break;
					}
				}
				// Halos de chaleur, en additif : discrets par atome, ils font
				// ROUGEOYER le cristal sans le noyer de blanc.
				dl.PushBlend(NkGuiBlend::PlusLighter);
				for (uint32 ci = 0; ci < nc; ++ci) {
					const NkCorpsP2D &c = p.corps[ci];
					if (c.mat != NkMateriauP2D::NK_ATOMES || !visible[ci]) {
						continue;
					}
					for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
						const NkParticule2D &q = p.particules[i];
						const float32 chaleur = Clampf((Longueur(q.vit) - 1.2f) / 7.f, 0.f, 1.f);
						if (chaleur > 0.02f && cx.Visible(q.pos, 0.4f)) {
							const float32 r = q.rayon * cx.z * 2.f;
							dl.AddCircleFilled(cx.E(q.pos), r, NkColor(255, 90, 20, static_cast<uint8>(chaleur * 38.f)), Segments(r));
						}
					}
				}
				dl.PopBlend();
				if (o.liens) {
					Liens(cx, false, visible);
				}
				if (o.particules) {
					Particules(cx, false, false, visible);
				}
			} else if (o.mode == NkModeRenduParticules::NK_FILAIRE) {
				Liens(cx, false, visible);
				Particules(cx, false, false, visible);
			} else if (o.mode == NkModeRenduParticules::NK_CONTRAINTES) {
				Particules(cx, false, true, visible);
				Liens(cx, true, visible);
			} else {
				Liens(cx, false, visible);
				Particules(cx, true, true, visible);
			}

			if (o.vitesses) {
				for (uint32 i = 0; i < p.particules.Size(); ++i) {
					const NkParticule2D &q = p.particules[i];
					if (q.vit.x * q.vit.x + q.vit.y * q.vit.y < 0.04f || !cx.Visible(q.pos, 0.2f)) {
						continue;
					}
					dl.AddLine(cx.E(q.pos), cx.E(q.pos + q.vit * 0.05f), NkColor(255, 230, 90, 170), 1.f);
				}
			}
			if (o.epingles) {
				for (uint32 i = 0; i < p.particules.Size(); ++i) {
					const NkParticule2D &q = p.particules[i];
					if (!q.epingle || !cx.Visible(q.pos, 0.2f)) {
						continue;
					}
					const NkVec2f e = cx.E(q.pos);
					dl.AddCircleFilled(e, 4.5f, NkColor(30, 10, 10), 10);
					dl.AddCircleFilled(e, 3.5f, NkColor(236, 60, 60), 10);
					dl.AddCircleFilled(NkVec2f(e.x - 1.f, e.y - 1.f), 1.2f, NkColor(255, 220, 220), 6);
				}
			}
		}

	} // namespace unkeny
} // namespace nkentseu
