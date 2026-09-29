// =============================================================================
// Physic2DEcran.cpp — la mise en page et le dessin
//
// LA MISE EN PAGE — celle d'UE5, repliable
//   ┌──────────────── barre d'outils (transport, niveau, vue) ─────────────┐
//   │ palette  │               vue                          │  details     │
//   │ outils   │    (la scene, en metres, par NkVue2D)      │  selection   │
//   │ acteurs  │                                            │  monde       │
//   └──────────────────────────── barre d'etat ───────────────────────────┘
//   En PORTRAIT (ou sur un ecran etroit), la palette descend sous la vue et
//   le panneau de details FLOTTE par-dessus, ouvert par un bouton. La barre
//   d'outils passe a la ligne au lieu de deborder. Une seule mise en page,
//   calculee depuis la taille — pas une par plateforme.
//
// ⚠️ LE DESSIN LIT, IL NE MODIFIE RIEN (Unkeny.h, ordre d'une trame).
//   Planifier() est l'exception assumee : elle recalcule les RECTANGLES de
//   l'interface, qui ne sont pas de l'etat de simulation.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Physic2D/Physic2D.h"

#include <cstdio>

namespace nkentseu {

	using namespace unkeny;
	using nkgui::NkGuiDrawList;
	using physic2d::NkActeur;
	using physic2d::NkActeurInfo;
	using renderer::NkTexte;
	using renderer::NkTexteADroite;
	using renderer::NkTexteDansBoite;
	using renderer::NkTexteLargeur;

	namespace {
		NkColor Rgba(uint32 c) {
			return NkColor(static_cast<uint8>(c >> 24), static_cast<uint8>(c >> 16), static_cast<uint8>(c >> 8),
						   static_cast<uint8>(c));
		}
		NkColor Mix(const NkColor &a, const NkColor &b, float32 t) {
			auto m = [t](uint8 x, uint8 y) {
				return static_cast<uint8>(static_cast<float32>(x) + (static_cast<float32>(y) - static_cast<float32>(x)) * t);
			};
			return NkColor(m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), m(a.a, b.a));
		}
		NkColor Alpha(const NkColor &c, uint8 a) {
			return NkColor(c.r, c.g, c.b, a);
		}
		float32 Maxf(float32 a, float32 b) {
			return a > b ? a : b;
		}
		float32 Minf(float32 a, float32 b) {
			return a < b ? a : b;
		}

		// ---- Icones dessinees : aucun atlas, aucune police d'icones ----------
		void IconeLecture(NkGuiDrawList &dl, const NkVec2f &c, float32 s, const NkColor &col) {
			dl.AddTriangleFilled(NkVec2f(c.x - s * 0.35f, c.y - s * 0.42f), NkVec2f(c.x + s * 0.45f, c.y),
								 NkVec2f(c.x - s * 0.35f, c.y + s * 0.42f), col);
		}
		void IconePause(NkGuiDrawList &dl, const NkVec2f &c, float32 s, const NkColor &col) {
			dl.AddRectFilled(NkRect{c.x - s * 0.36f, c.y - s * 0.40f, s * 0.24f, s * 0.80f}, col);
			dl.AddRectFilled(NkRect{c.x + s * 0.12f, c.y - s * 0.40f, s * 0.24f, s * 0.80f}, col);
		}
		void IconePas(NkGuiDrawList &dl, const NkVec2f &c, float32 s, const NkColor &col) {
			dl.AddTriangleFilled(NkVec2f(c.x - s * 0.40f, c.y - s * 0.38f), NkVec2f(c.x + s * 0.20f, c.y),
								 NkVec2f(c.x - s * 0.40f, c.y + s * 0.38f), col);
			dl.AddRectFilled(NkRect{c.x + s * 0.22f, c.y - s * 0.38f, s * 0.18f, s * 0.76f}, col);
		}
		void IconeArret(NkGuiDrawList &dl, const NkVec2f &c, float32 s, const NkColor &col) {
			dl.AddRectFilled(NkRect{c.x - s * 0.34f, c.y - s * 0.34f, s * 0.68f, s * 0.68f}, col, s * 0.06f);
		}
		void IconeFleche(NkGuiDrawList &dl, const NkVec2f &c, float32 s, const NkColor &col, bool droite) {
			const float32 k = droite ? 1.f : -1.f;
			dl.AddTriangleFilled(NkVec2f(c.x - k * s * 0.22f, c.y - s * 0.34f), NkVec2f(c.x + k * s * 0.26f, c.y),
								 NkVec2f(c.x - k * s * 0.22f, c.y + s * 0.34f), col);
		}

		void IconeOutil(NkGuiDrawList &dl, const NkVec2f &c, float32 s, int32 outil, const NkColor &col) {
			const float32 e = Maxf(1.5f, s * 0.11f);
			switch (outil) {
				case 0: // poser : un plus
					dl.AddLine(NkVec2f(c.x - s * 0.38f, c.y), NkVec2f(c.x + s * 0.38f, c.y), col, e * 1.3f);
					dl.AddLine(NkVec2f(c.x, c.y - s * 0.38f), NkVec2f(c.x, c.y + s * 0.38f), col, e * 1.3f);
					break;
				case 1: // saisir : une poignee
					dl.AddCircle(c, s * 0.34f, col, e);
					dl.AddCircleFilled(c, s * 0.14f, col);
					break;
				case 2: { // couteau : une lame
					const NkVec2f a(c.x - s * 0.42f, c.y + s * 0.42f), b(c.x + s * 0.42f, c.y - s * 0.42f);
					dl.AddLine(a, NkVec2f(c.x - s * 0.12f, c.y + s * 0.12f), col, e * 1.8f);
					dl.AddTriangleFilled(NkVec2f(c.x - s * 0.16f, c.y + s * 0.04f), b, NkVec2f(c.x - s * 0.04f, c.y + s * 0.16f), col);
					break;
				}
				case 3: // explosion : une etoile
					for (int32 i = 0; i < 8; ++i) {
						const float32 a = static_cast<float32>(i) * 0.785398f;
						const float32 r = (i & 1) ? 0.26f : 0.44f;
						dl.AddLine(c, NkVec2f(c.x + math::NkCos(a) * s * r, c.y + math::NkSin(a) * s * r), col, e);
					}
					dl.AddCircleFilled(c, s * 0.12f, col);
					break;
				case 4: { // aimant : un U
					NkVec2f u[5] = {{c.x - s * 0.30f, c.y - s * 0.38f}, {c.x - s * 0.30f, c.y + s * 0.06f}, {c.x, c.y + s * 0.34f},
									{c.x + s * 0.30f, c.y + s * 0.06f}, {c.x + s * 0.30f, c.y - s * 0.38f}};
					dl.AddPolyline(u, 5, col, e * 1.8f);
					break;
				}
				case 5: // obstacle : une capsule
					dl.AddLine(NkVec2f(c.x - s * 0.36f, c.y + s * 0.2f), NkVec2f(c.x + s * 0.36f, c.y - s * 0.2f), col, e * 2.6f);
					break;
				case 6: { // gomme : un bloc incline
					NkVec2f q[4] = {{c.x - s * 0.40f, c.y + s * 0.10f}, {c.x - s * 0.06f, c.y - s * 0.26f},
									{c.x + s * 0.40f, c.y + s * 0.14f}, {c.x + s * 0.06f, c.y + s * 0.44f}};
					dl.AddConvexPolyFilled(q, 4, col);
					break;
				}
				case 7: // epingle
					dl.AddLine(NkVec2f(c.x, c.y), NkVec2f(c.x, c.y + s * 0.44f), col, e);
					dl.AddCircleFilled(NkVec2f(c.x, c.y - s * 0.12f), s * 0.24f, col);
					break;
				default: // camera : quatre fleches
					for (int32 i = 0; i < 4; ++i) {
						const float32 a = static_cast<float32>(i) * 1.570796f;
						const NkVec2f d(math::NkCos(a), math::NkSin(a));
						const NkVec2f n(-d.y, d.x);
						const NkVec2f p = c + d * (s * 0.44f);
						dl.AddTriangleFilled(p, c + d * (s * 0.22f) + n * (s * 0.14f), c + d * (s * 0.22f) - n * (s * 0.14f), col);
					}
					dl.AddLine(NkVec2f(c.x - s * 0.26f, c.y), NkVec2f(c.x + s * 0.26f, c.y), col, e);
					dl.AddLine(NkVec2f(c.x, c.y - s * 0.26f), NkVec2f(c.x, c.y + s * 0.26f), col, e);
					break;
			}
		}

		/// Hauteur de la section Selection du panneau de details, en lignes :
		/// titre (1,7) + quatre lignes (4 x 1,05) + un blanc. FIXE, pour que les
		/// boutons - / + des reglages ne sautent pas quand la selection change.
		constexpr float32 kHautSelection = 6.2f;

		const char *kNomsReglages[] = {"Température", "Vent", "Gravité", "Frein de l'air", "Sous-pas"};
	} // namespace

	const char *Physic2D::OutilNom(NkOutil o) noexcept {
		static const char *k[] = {"Poser", "Saisir", "Couteau", "Explosion", "Aimant",
								  "Obstacle", "Gomme", "Épingle", "Caméra"};
		const int32 i = static_cast<int32>(o);
		return (i >= 0 && i < static_cast<int32>(NkOutil::NK_COUNT)) ? k[i] : "";
	}

	const char *Physic2D::OutilAide(NkOutil o) noexcept {
		switch (o) {
			case NkOutil::NK_SAISIR:
				return "Saisir : attraper et lancer un corps mou ou rigide.";
			case NkOutil::NK_COUTEAU:
				return "Couteau : glisser à travers la matière pour la trancher.";
			case NkOutil::NK_EXPLOSION:
				return "Explosion : un souffle au point cliqué.";
			case NkOutil::NK_AIMANT:
				return "Aimant : maintenir pour attirer tout ce qui est proche.";
			case NkOutil::NK_OBSTACLE:
				return "Obstacle : glisser pour tracer une barre fixe.";
			case NkOutil::NK_GOMME:
				return "Gomme : maintenir pour effacer matière, rigides et obstacles.";
			case NkOutil::NK_EPINGLE:
				return "Épingle : fixer ou libérer une particule dans l'espace.";
			case NkOutil::NK_CAMERA:
				return "Caméra : glisser pour déplacer la vue (molette, pincement : zoom).";
			default:
				return "";
		}
	}

	// =========================================================================
	// Planifier — les rectangles, depuis la taille de l'ecran
	// =========================================================================
	void Physic2D::Planifier() {
		const renderer::NkLayoutInfo &L = Layout();
		NkPlan &P = mPlan;
		P.cibles.Clear();
		P.titres.Clear();
		P.ecran = NkRect{0.f, 0.f, static_cast<float32>(L.width), static_cast<float32>(L.height)};
		// La zone sure : ce qui se lit ou se touche reste DEDANS.
		const NkRect z{L.safeArea.left, L.safeArea.top, P.ecran.w - L.safeArea.left - L.safeArea.right,
					   P.ecran.h - L.safeArea.top - L.safeArea.bottom};
		P.ligne = LineH(FontSmall(), 16.f);
		const float32 m = Maxf(4.f, P.ligne * 0.28f);
		const float32 bh = P.ligne * 1.55f; // hauteur d'un bouton
		P.portrait = z.w < z.h * 1.05f || z.w < 760.f;
		const bool large = !P.portrait && z.w >= 1180.f;
		P.detailsFlottant = !large;

		// --- Barre d'outils : un flux qui passe a la ligne -------------------
		float32 x = z.x + m, y = z.y + m;
		auto placer = [&](float32 w) {
			if (x + w > z.x + z.w - m && x > z.x + m) {
				x = z.x + m;
				y += bh + m;
			}
			const NkRect r{x, y, w, bh};
			x += w + m;
			return r;
		};
		auto commande = [&](float32 w, int32 cmd) { P.cibles.PushBack(NkCible{placer(w), NkGenre::NK_COMMANDE, cmd}); };
		if (!P.portrait) {
			x += NkTexteLargeur(FontBody(), "Physic2D") + m * 3.f; // le titre
		}
		commande(bh, CMD_JOUER);
		commande(bh, CMD_PAS);
		commande(bh, CMD_ARRET);
		x += m * 2.f;
		float32 largeurNom = 0.f;
		for (int32 i = 0; i < physic2d::NK_NB_NIVEAUX; ++i) {
			largeurNom = Maxf(largeurNom, NkTexteLargeur(FontSmall(), physic2d::NkNiveauNom(i)));
		}
		largeurNom += P.ligne * 1.2f;
		if (x + bh * 2.f + largeurNom + m * 2.f > z.x + z.w - m) {
			x = z.x + z.w; // le groupe du niveau ne se coupe pas : il passe a la ligne entier
		}
		commande(bh, CMD_NIV_PREC);
		P.nomNiveau = placer(largeurNom);
		commande(bh, CMD_NIV_SUIV);
		x += m * 2.f;
		const float32 wVue = NkTexteLargeur(FontSmall(), "Vue : Contraintes") + P.ligne * 1.2f;
		commande(wVue, CMD_VUE);
		commande(NkTexteLargeur(FontSmall(), "Grille") + P.ligne * 1.2f, CMD_GRILLE);
		commande(NkTexteLargeur(FontSmall(), "Cadrer") + P.ligne * 1.2f, CMD_CADRER);
		x += m * 2.f;
		commande(NkTexteLargeur(FontSmall(), "Enregistrer") + P.ligne * 1.2f, CMD_SAUVER);
		commande(NkTexteLargeur(FontSmall(), "Ouvrir") + P.ligne * 1.2f, CMD_OUVRIR);
		if (P.detailsFlottant) {
			commande(NkTexteLargeur(FontSmall(), "Détails") + P.ligne * 1.2f, CMD_PANNEAU);
		}
		P.barre = NkRect{P.ecran.x, P.ecran.y, P.ecran.w, y + bh + m - P.ecran.y};

		// --- Barre d'etat ----------------------------------------------------
		const float32 hs = P.ligne * 1.35f;
		P.statut = NkRect{P.ecran.x, z.y + z.h - hs, P.ecran.w, hs + L.safeArea.bottom};

		// --- Le milieu -------------------------------------------------------
		const float32 haut = P.barre.y + P.barre.h;
		const float32 bas = P.statut.y;
		const int32 nOutils = static_cast<int32>(NkOutil::NK_COUNT);
		const int32 nActeurs = static_cast<int32>(NkActeur::NK_COUNT);
		float32 ch = P.ligne * 1.45f; // hauteur d'une case

		auto grille = [&](const NkRect &zone, int32 cols, float32 y0, int32 n, NkGenre genre) {
			const float32 cw = (zone.w - m * static_cast<float32>(cols + 1)) / static_cast<float32>(cols);
			for (int32 i = 0; i < n; ++i) {
				const int32 cx = i % cols, cy = i / cols;
				P.cibles.PushBack(NkCible{NkRect{zone.x + m + static_cast<float32>(cx) * (cw + m),
												 y0 + static_cast<float32>(cy) * (ch + m), cw, ch},
										  genre, i});
			}
			return y0 + static_cast<float32>((n + cols - 1) / cols) * (ch + m);
		};

		if (P.portrait) {
			const int32 cols = static_cast<int32>(Maxf(3.f, Minf(7.f, z.w / (P.ligne * 6.2f))));
			const int32 rangs = (nOutils + cols - 1) / cols + (nActeurs + cols - 1) / cols;
			const float32 titre = P.ligne * 1.1f;
			// La palette ne prend jamais plus de 40 % de la hauteur : la vue d'abord.
			ch = Minf(ch, (0.40f * (bas - haut) - titre * 2.f) / static_cast<float32>(rangs) - m);
			ch = Maxf(ch, P.ligne * 1.05f);
			const float32 hp = titre * 2.f + static_cast<float32>(rangs) * (ch + m) + m;
			P.palette = NkRect{P.ecran.x, bas - hp, P.ecran.w, hp};
			const NkRect zp{z.x, P.palette.y, z.w, hp};
			P.titres.PushBack(NkRect{zp.x + m, zp.y, zp.w, titre});
			float32 yy = grille(zp, cols, zp.y + titre, nOutils, NkGenre::NK_OUTIL);
			P.titres.PushBack(NkRect{zp.x + m, yy, zp.w, titre});
			grille(zp, cols, yy + titre, nActeurs, NkGenre::NK_ACTEUR);
			P.vue = NkRect{P.ecran.x, haut, P.ecran.w, P.palette.y - haut};
		} else {
			// La palette se dimensionne sur son libelle le PLUS LONG : a une
			// largeur fixe, "Disque atomique" debordait de sa case.
			float32 wl = 0.f;
			for (int32 i = 0; i < nOutils; ++i) {
				wl = Maxf(wl, NkTexteLargeur(FontSmall(), OutilNom(static_cast<NkOutil>(i))));
			}
			for (int32 i = 0; i < nActeurs; ++i) {
				wl = Maxf(wl, NkTexteLargeur(FontSmall(), NkActeurInfo(static_cast<NkActeur>(i)).nom));
			}
			const float32 wCase = ch * 1.05f + wl + P.ligne * 0.5f;
			const float32 wp = Maxf(P.ligne * 9.5f, Minf(wCase * 2.f + m * 3.f, z.w * 0.24f));
			P.palette = NkRect{P.ecran.x, haut, z.x - P.ecran.x + wp, bas - haut};
			const NkRect zp{z.x, haut, wp, bas - haut};
			const float32 titre = P.ligne * 1.4f;
			const int32 rangs = (nOutils + 1) / 2 + (nActeurs + 1) / 2;
			ch = Minf(ch, (zp.h - titre * 2.f - m) / static_cast<float32>(rangs) - m);
			ch = Maxf(ch, P.ligne * 1.05f);
			P.titres.PushBack(NkRect{zp.x + m, zp.y + m * 0.5f, zp.w, titre});
			float32 yy = grille(zp, 2, zp.y + titre + m * 0.5f, nOutils, NkGenre::NK_OUTIL);
			P.titres.PushBack(NkRect{zp.x + m, yy, zp.w, titre});
			grille(zp, 2, yy + titre, nActeurs, NkGenre::NK_ACTEUR);
			float32 droite = P.ecran.x + P.ecran.w;
			if (large) {
				const float32 wd = Maxf(P.ligne * 12.f, Minf(z.w * 0.19f, P.ligne * 15.f));
				droite = z.x + z.w - wd;
				P.details = NkRect{droite, haut, P.ecran.x + P.ecran.w - droite, bas - haut};
			}
			P.vue = NkRect{P.palette.x + P.palette.w, haut, droite - (P.palette.x + P.palette.w), bas - haut};
		}

		// --- Details : range a droite, ou FLOTTANT sur la vue ---------------
		if (P.detailsFlottant) {
			if (mDetailsOuvert) {
				const float32 wd = Minf(P.ligne * 14.f, P.vue.w - m * 2.f);
				const float32 hd = P.ligne * (kHautSelection + 1.7f + static_cast<float32>(REG_COUNT) * 1.7f + 1.7f + 2.3f) + m * 2.f;
				P.details = NkRect{P.vue.x + P.vue.w - wd - m, P.vue.y + m, wd, Minf(hd, P.vue.h - m * 2.f)};
			} else {
				P.details = NkRect{0.f, 0.f, 0.f, 0.f};
			}
		}
		// Les boutons - / + des reglages : leur place se calcule ICI, avec le
		// reste, pour que le test du clic et le dessin ne puissent pas diverger.
		if (P.details.w > 0.f) {
			const float32 b = P.ligne * 1.3f;
			float32 yy = P.details.y + m + P.ligne * (kHautSelection + 1.7f);
			for (int32 r = 0; r < REG_COUNT; ++r) {
				const float32 xd = P.details.x + P.details.w - m - b;
				P.cibles.PushBack(NkCible{NkRect{xd, yy, b, b}, NkGenre::NK_REGLAGE, r * 2 + 1});
				P.cibles.PushBack(NkCible{NkRect{xd - b - m * 0.5f, yy, b, b}, NkGenre::NK_REGLAGE, r * 2});
				yy += P.ligne * 1.7f;
			}
		}
	}

	void Physic2D::Cadrer() {
		NkVue2D &cam = mScene.Camera();
		cam.PoserViseur(mPlan.vue);
		// La boite du monde, murs compris, plus une marge.
		cam.Cadrer(NkVec2f(0.f, physic2d::NK_HAUTEUR * 0.5f),
				   NkVec2f(physic2d::NK_DEMI_LARGEUR * 2.f + 2.2f, physic2d::NK_HAUTEUR + 2.2f));
		// En portrait la boite ne remplit que le milieu : on la POSE en bas de
		// la vue, pres des pouces et de la palette, plutot que de laisser la
		// moitie basse de l'ecran vide sous le sol.
		const float32 visibleH = mPlan.vue.h / cam.Zoom();
		const float32 bas = -1.1f;
		if (mPlan.portrait && visibleH > physic2d::NK_HAUTEUR + 2.2f) {
			cam.PoserCentre(NkVec2f(0.f, bas + visibleH * 0.5f));
		}
	}

	const Physic2D::NkCible *Physic2D::CibleSous(const NkVec2f &p) const {
		// A l'envers : ce qui est dessine en dernier (les reglages flottants)
		// est au-dessus, donc teste d'abord.
		for (int32 i = static_cast<int32>(mPlan.cibles.Size()) - 1; i >= 0; --i) {
			if (NkDansRect(mPlan.cibles[static_cast<uint32>(i)].r, p)) {
				return &mPlan.cibles[static_cast<uint32>(i)];
			}
		}
		return nullptr;
	}

	// =========================================================================
	// Dessin
	// =========================================================================
	void Physic2D::OnDraw(NkGuiDrawList &dl) {
		Planifier();
		mScene.Camera().PoserViseur(mPlan.vue);
		dl.AddRectFilled(mPlan.ecran, mTheme.fond);
		DessinerVue(dl);
		DessinerPalette(dl);
		DessinerBarre(dl);
		DessinerStatut(dl);
		DessinerDetails(dl);
	}

	void Physic2D::DessinerVue(NkGuiDrawList &dl) {
		const NkRect &v = mPlan.vue;
		dl.PushClipRect(v);
		// Un ciel sombre, a peine degrade : la matiere doit ressortir, pas le fond.
		dl.AddRectFilledMultiColor(v, NkColor(40, 43, 52), NkColor(40, 43, 52), NkColor(20, 21, 25), NkColor(20, 21, 25));
		if (mGrille) {
			NkDessinerGrille(dl, mScene.Camera(), 1.f, 0xFFFFFF0Du, 0xFFFFFF26u);
		}
		DessinerFormes(dl);
		NkDessinerScene(dl, mScene); // les sprites textures (caisses, balles)
		NkDessinerCorpsMous(dl, mScene, mRendu);
		DessinerSurimpressions(dl);
		dl.PopClipRect();
	}

	/// Le decor et les corps rigides : NkDessinerFormes d'Unkeny, avec la
	/// couleur propre de la BOITE des niveaux (le sol plus sombre que les
	/// obstacles poses).
	void Physic2D::DessinerFormes(NkGuiDrawList &dl) {
		NkOptionsFormes o;
		o.decor = 0x606472FFu;
		o.decorBord = 0x969CACFFu;
		o.couleurDecor = [](ecs::NkWorld &monde, ecs::NkEntityId id, void *) -> uint32 {
			const physic2d::NkDecor2D *d = monde.Get<physic2d::NkDecor2D>(id);
			return (d != nullptr && d->sol) ? 0x2E2F35FFu : 0u;
		};
		NkDessinerFormes(dl, mScene, o);
	}

	void Physic2D::DessinerSurimpressions(NkGuiDrawList &dl) {
		const NkVue2D &cam = mScene.Camera();
		physics::NkParticules2D *p = mScene.Particules();

		// La selection : une boite d'accent et son nom, comme le contour d'UE5.
		if (p != nullptr && mSelection != 0u) {
			const int32 ci = p->IndexCorps(mSelection);
			if (ci >= 0) {
				NkVec2f mn, mx;
				p->BoiteCorps(static_cast<uint32>(ci), mn, mx);
				const NkVec2f a = cam.MondeVersEcran(NkVec2f(mn.x - 0.08f, mx.y + 0.08f));
				const NkVec2f b = cam.MondeVersEcran(NkVec2f(mx.x + 0.08f, mn.y - 0.08f));
				dl.AddRect(NkRect{a.x, a.y, b.x - a.x, b.y - a.y}, mTheme.or_, 1.5f, 3.f);
				const ecs::NkEntityId e = mScene.EntiteDuCorpsMou(mSelection);
				if (const NkEtiquette *et = mScene.Monde().Get<NkEtiquette>(e)) {
					NkTexte(dl, FontSmall(), a.x, a.y - mPlan.ligne * 1.05f, et->nom, mTheme.or_);
				}
			}
		}

		// Les explosions recentes : un anneau qui s'etend et s'efface.
		for (const NkEclair &e : mEclairs) {
			if (e.age < 0.45f) {
				const float32 t = e.age / 0.45f;
				const NkVec2f c = cam.MondeVersEcran(e.pos);
				const float32 r = cam.LongueurVersEcran(0.3f + t * 2.6f);
				dl.AddCircleFilled(c, r, NkColor(255, 170, 60, static_cast<uint8>(70.f * (1.f - t))));
				dl.AddCircle(c, r, NkColor(255, 210, 120, static_cast<uint8>(230.f * (1.f - t))), 3.f);
			}
		}

		// La trace du couteau.
		if (mTraceN >= 2) {
			NkVec2f pts[kTrace];
			for (int32 i = 0; i < mTraceN; ++i) {
				pts[i] = cam.MondeVersEcran(mTrace[i]);
			}
			const float32 a = mGeste ? 1.f : Maxf(0.f, 1.f - mTraceAge / 0.35f);
			dl.AddPolyline(pts, mTraceN, NkColor(255, 90, 80, static_cast<uint8>(230.f * a)), 3.f);
		}

		// Les traces en cours : obstacle, pont.
		if (mGeste && (mOutil == NkOutil::NK_OBSTACLE || (mOutil == NkOutil::NK_POSER && mActeur == NkActeur::NK_PONT))) {
			const NkVec2f a = cam.MondeVersEcran(mGesteDebut);
			const NkVec2f b = mSouris;
			const bool pont = mOutil == NkOutil::NK_POSER;
			dl.AddLine(a, b, pont ? Alpha(Rgba(NkActeurInfo(NkActeur::NK_PONT).couleur), 200) : NkColor(150, 156, 172, 200),
					   pont ? 5.f : cam.LongueurVersEcran(0.2f));
			dl.AddCircleFilled(a, 4.f, mTheme.accent);
			dl.AddCircleFilled(b, 4.f, mTheme.accent);
		}

		// Le curseur de l'outil : sa VRAIE portee.
		if (NkDansRect(mPlan.vue, mSouris) && !(mPlan.details.w > 0.f && NkDansRect(mPlan.details, mSouris))) {
			float32 rayon = 0.f;
			switch (mOutil) {
				case NkOutil::NK_SAISIR:
					rayon = 0.6f;
					break;
				case NkOutil::NK_GOMME:
					rayon = 0.5f;
					break;
				case NkOutil::NK_AIMANT:
					rayon = 3.0f;
					break;
				case NkOutil::NK_EXPLOSION:
					rayon = 2.6f;
					break;
				case NkOutil::NK_EPINGLE:
					rayon = 0.3f;
					break;
				default:
					break;
			}
			if (rayon > 0.f) {
				const bool actif = mGeste && (mOutil == NkOutil::NK_AIMANT || mOutil == NkOutil::NK_GOMME);
				dl.AddCircle(mSouris, cam.LongueurVersEcran(rayon), actif ? mTheme.accent : NkColor(255, 255, 255, 110), actif ? 2.f : 1.f);
			} else if (mOutil == NkOutil::NK_POSER) {
				const NkColor col = Alpha(Rgba(NkActeurInfo(mActeur).couleur), 170);
				dl.AddCircle(mSouris, 9.f, col, 2.f);
				dl.AddLine(NkVec2f(mSouris.x - 5.f, mSouris.y), NkVec2f(mSouris.x + 5.f, mSouris.y), col, 1.5f);
				dl.AddLine(NkVec2f(mSouris.x, mSouris.y - 5.f), NkVec2f(mSouris.x, mSouris.y + 5.f), col, 1.5f);
			}
		}

		// L'etat, en haut a gauche de la vue — et le liseré de simulation.
		const NkRect &v = mPlan.vue;
		const char *etat = mEtat == NkEtat::NK_JEU ? "SIMULATION" : (mEtat == NkEtat::NK_PAUSE ? "EN PAUSE" : "ÉDITION");
		const NkColor col = mEtat == NkEtat::NK_JEU ? mTheme.succes : (mEtat == NkEtat::NK_PAUSE ? mTheme.or_ : mTheme.texteFaible);
		const float32 m = mPlan.ligne * 0.35f;
		char titre[96];
		std::snprintf(titre, sizeof(titre), "%s  ·  %s", etat, NkModeRenduParticulesNom(mRendu.mode));
		const float32 w = NkTexteLargeur(FontSmall(), titre) + m * 3.f;
		const NkRect badge{v.x + m, v.y + m, w, mPlan.ligne * 1.25f};
		dl.AddRectFilled(badge, NkColor(10, 10, 12, 170), badge.h * 0.2f);
		dl.AddCircleFilled(NkVec2f(badge.x + m * 1.1f, badge.y + badge.h * 0.5f), m * 0.45f, col);
		NkTexte(dl, FontSmall(), badge.x + m * 2.f, badge.y + (badge.h - mPlan.ligne) * 0.5f, titre, mTheme.texte);
		if (mEtat != NkEtat::NK_EDITION) {
			dl.AddRect(v, Alpha(col, 150), 2.f);
		}

		// L'annonce : 2,5 s, puis elle s'efface en 0,5 s.
		if (mAnnonceAge < 3.f && !mAnnonce.Empty()) {
			const float32 t = mAnnonceAge < 2.5f ? 1.f : 1.f - (mAnnonceAge - 2.5f) / 0.5f;
			const float32 wa = NkTexteLargeur(FontSmall(), mAnnonce.CStr()) + mPlan.ligne * 1.6f;
			const NkRect r{v.x + (v.w - wa) * 0.5f, v.y + v.h - mPlan.ligne * 2.6f, wa, mPlan.ligne * 1.6f};
			dl.AddRectFilled(r, NkColor(12, 12, 14, static_cast<uint8>(220.f * t)), r.h * 0.3f);
			dl.AddRect(r, Alpha(mAnnonceErreur ? mTheme.alerte : mTheme.accent, static_cast<uint8>(255.f * t)), 1.5f, r.h * 0.3f);
			NkTexteDansBoite(dl, FontSmall(), r, mAnnonce.CStr(), Alpha(mTheme.texte, static_cast<uint8>(255.f * t)));
		}
	}

	void Physic2D::DessinerBarre(NkGuiDrawList &dl) {
		const NkPlan &P = mPlan;
		dl.AddRectFilled(P.barre, mTheme.panneau);
		dl.AddLine(NkVec2f(P.barre.x, P.barre.y + P.barre.h), NkVec2f(P.barre.x + P.barre.w, P.barre.y + P.barre.h), mTheme.bord, 1.f);
		if (!P.portrait) {
			const float32 m = Maxf(4.f, P.ligne * 0.28f);
			NkTexte(dl, FontBody(), Layout().safeArea.left + m * 1.5f,
					P.barre.y + m + (P.ligne * 1.55f - LineH(FontBody(), 20.f)) * 0.5f, "Physic2D", mTheme.texte);
		}
		for (uint32 i = 0; i < P.cibles.Size(); ++i) {
			const NkCible &c = P.cibles[i];
			if (c.genre != NkGenre::NK_COMMANDE) {
				continue;
			}
			const NkVec2f centre(c.r.x + c.r.w * 0.5f, c.r.y + c.r.h * 0.5f);
			const float32 s = c.r.h * 0.62f;
			const bool survol = NkDansRect(c.r, mSouris);
			switch (c.valeur) {
				case CMD_JOUER: {
					const bool joue = mEtat == NkEtat::NK_JEU;
					NkPanneau(dl, c.r, mTheme, survol);
					joue ? IconePause(dl, centre, s, mTheme.texte) : IconeLecture(dl, centre, s, mTheme.succes);
					break;
				}
				case CMD_PAS:
					NkPanneau(dl, c.r, mTheme, survol);
					IconePas(dl, centre, s, mTheme.texte);
					break;
				case CMD_ARRET:
					NkPanneau(dl, c.r, mTheme, survol);
					IconeArret(dl, centre, s, mEtat == NkEtat::NK_EDITION ? mTheme.texteFaible : mTheme.alerte);
					break;
				case CMD_NIV_PREC:
				case CMD_NIV_SUIV:
					NkPanneau(dl, c.r, mTheme, survol);
					IconeFleche(dl, centre, s, mTheme.texte, c.valeur == CMD_NIV_SUIV);
					break;
				case CMD_VUE: {
					char t[64];
					std::snprintf(t, sizeof(t), "Vue : %s", NkModeRenduParticulesNom(mRendu.mode));
					NkBouton(dl, c.r, FontSmall(), t, mTheme, survol);
					break;
				}
				case CMD_GRILLE:
					NkBouton(dl, c.r, FontSmall(), "Grille", mTheme, mGrille);
					break;
				case CMD_CADRER:
					NkBouton(dl, c.r, FontSmall(), "Cadrer", mTheme, survol);
					break;
				case CMD_SAUVER:
					NkBouton(dl, c.r, FontSmall(), "Enregistrer", mTheme, survol);
					break;
				case CMD_OUVRIR:
					NkBouton(dl, c.r, FontSmall(), "Ouvrir", mTheme, survol);
					break;
				case CMD_PANNEAU:
					NkBouton(dl, c.r, FontSmall(), "Détails", mTheme, mDetailsOuvert);
					break;
				default:
					break;
			}
		}
		// Le nom du niveau, entre ses fleches.
		dl.AddRectFilled(P.nomNiveau, mTheme.fond, P.nomNiveau.h * mTheme.arrondi);
		NkTexteDansBoite(dl, FontSmall(), P.nomNiveau, physic2d::NkNiveauNom(mNiveau), mTheme.texte);
	}

	void Physic2D::DessinerPalette(NkGuiDrawList &dl) {
		const NkPlan &P = mPlan;
		dl.AddRectFilled(P.palette, mTheme.panneau);
		if (P.portrait) {
			dl.AddLine(NkVec2f(P.palette.x, P.palette.y), NkVec2f(P.palette.x + P.palette.w, P.palette.y), mTheme.bord, 1.f);
		} else {
			dl.AddLine(NkVec2f(P.palette.x + P.palette.w, P.palette.y), NkVec2f(P.palette.x + P.palette.w, P.palette.y + P.palette.h), mTheme.bord, 1.f);
		}
		static const char *kTitres[2] = {"OUTILS", "ACTEURS"};
		for (uint32 i = 0; i < P.titres.Size() && i < 2u; ++i) {
			const NkRect &t = P.titres[i];
			NkTexte(dl, FontSmall(), t.x, t.y + (t.h - P.ligne) * 0.5f, kTitres[i], mTheme.texteFaible);
		}
		for (uint32 i = 0; i < P.cibles.Size(); ++i) {
			const NkCible &c = P.cibles[i];
			if (c.genre == NkGenre::NK_OUTIL) {
				const bool choisi = static_cast<int32>(mOutil) == c.valeur;
				const NkRect &r = c.r;
				dl.AddRectFilled(r, choisi ? Mix(mTheme.panneauActif, mTheme.accent, 0.35f) : mTheme.panneauActif, r.h * mTheme.arrondi);
				if (choisi) {
					dl.AddRect(r, mTheme.accent, 1.5f, r.h * mTheme.arrondi);
				}
				const float32 s = r.h * 0.62f;
				IconeOutil(dl, NkVec2f(r.x + r.h * 0.5f, r.y + r.h * 0.5f), s, c.valeur, choisi ? mTheme.texte : mTheme.texteFaible);
				char n[40];
				std::snprintf(n, sizeof(n), "%s", OutilNom(static_cast<NkOutil>(c.valeur)));
				NkTexte(dl, FontSmall(), r.x + r.h * 0.95f, r.y + (r.h - P.ligne) * 0.5f, n, choisi ? mTheme.texte : mTheme.texteFaible,
						r.w - r.h * 1.05f);
			} else if (c.genre == NkGenre::NK_ACTEUR) {
				// Meme dessin que les outils (et non NkBoutonPastille, qui CENTRE
				// son libelle et ne sait pas le tronquer) : pastille a gauche,
				// texte cale a gauche, coupe proprement s'il ne tient pas.
				const NkActeur a = static_cast<NkActeur>(c.valeur);
				const bool choisi = mOutil == NkOutil::NK_POSER && mActeur == a;
				const physic2d::NkInfoActeur &info = NkActeurInfo(a);
				const NkRect &r = c.r;
				dl.AddRectFilled(r, choisi ? Mix(mTheme.panneauActif, mTheme.accent, 0.35f) : mTheme.panneauActif, r.h * mTheme.arrondi);
				if (choisi) {
					dl.AddRect(r, mTheme.accent, 1.5f, r.h * mTheme.arrondi);
				}
				const NkVec2f pc(r.x + r.h * 0.5f, r.y + r.h * 0.5f);
				if (info.rigide) {
					// Un rigide : un carre plein, pour le distinguer d'un coup d'oeil.
					dl.AddRectFilled(NkRect{pc.x - r.h * 0.2f, pc.y - r.h * 0.2f, r.h * 0.4f, r.h * 0.4f}, Rgba(info.couleur), 2.f);
				} else {
					dl.AddCircleFilled(pc, r.h * 0.21f, Rgba(info.couleur));
				}
				NkTexte(dl, FontSmall(), r.x + r.h * 0.95f, r.y + (r.h - P.ligne) * 0.5f, info.nom, choisi ? mTheme.texte : mTheme.texteFaible,
						r.w - r.h * 1.05f);
			}
		}
	}

	void Physic2D::DessinerDetails(NkGuiDrawList &dl) {
		const NkPlan &P = mPlan;
		if (P.details.w <= 0.f) {
			return;
		}
		const NkRect &d = P.details;
		const float32 m = Maxf(4.f, P.ligne * 0.28f);
		if (P.detailsFlottant) {
			dl.AddRectFilled(NkRect{d.x + 3.f, d.y + 4.f, d.w, d.h}, NkColor(0, 0, 0, 90), 6.f); // ombre
			dl.AddRectFilled(d, Alpha(mTheme.panneau, 245), 6.f);
			dl.AddRect(d, mTheme.bord, 1.f, 6.f);
		} else {
			dl.AddRectFilled(d, mTheme.panneau);
			dl.AddLine(NkVec2f(d.x, d.y), NkVec2f(d.x, d.y + d.h), mTheme.bord, 1.f);
		}
		nkgui::NkGuiFont *f = FontSmall();
		const float32 x = d.x + m * 2.f;
		const float32 w = d.w - m * 4.f;
		float32 y = d.y + m;
		auto titre = [&](const char *t) {
			dl.AddRectFilled(NkRect{d.x + 1.f, y, d.w - 2.f, P.ligne * 1.3f}, mTheme.panneauActif);
			NkTexte(dl, f, x, y + P.ligne * 0.15f, t, mTheme.texte);
			y += P.ligne * 1.7f;
		};
		auto ligne = [&](const char *cle, const char *val) {
			NkTexte(dl, f, x, y, cle, mTheme.texteFaible);
			NkTexteADroite(dl, f, x + w, y, val, mTheme.texte);
			y += P.ligne * 1.05f;
		};

		// --- Selection -------------------------------------------------------
		titre("DÉTAILS · Sélection");
		physics::NkParticules2D *p = mScene.Particules();
		const int32 ci = (p != nullptr && mSelection != 0u) ? p->IndexCorps(mSelection) : -1;
		char v[64];
		if (ci >= 0) {
			const physics::NkCorpsP2D &c = p->corps[static_cast<uint32>(ci)];
			const ecs::NkEntityId e = mScene.EntiteDuCorpsMou(mSelection);
			const NkEtiquette *et = mScene.Monde().Get<NkEtiquette>(e);
			ligne("Nom", et != nullptr ? et->nom : "?");
			ligne("Matériau", physics::NkMateriauP2DNom(c.mat));
			std::snprintf(v, sizeof(v), "%u · %u liens", static_cast<unsigned>(c.nombre),
						  static_cast<unsigned>(p->LiensActifsDuCorps(static_cast<uint32>(ci))));
			ligne("Particules", v);
			const NkVec2f vit = p->VitesseCorps(static_cast<uint32>(ci));
			std::snprintf(v, sizeof(v), "%.2f m/s", static_cast<double>(math::NkSqrt(vit.x * vit.x + vit.y * vit.y)));
			ligne("Vitesse", v);
		} else {
			NkTexte(dl, f, x, y, "Cliquez un corps avec Saisir,", mTheme.texteFaible, w);
			y += P.ligne * 1.05f;
			NkTexte(dl, f, x, y, "ou posez un acteur.", mTheme.texteFaible, w);
			y += P.ligne * 3.15f;
		}
		y = d.y + m + P.ligne * kHautSelection; // la section suivante a une place FIXE

		// --- Monde : reglages avec - / + (cibles posees par Planifier) ------
		titre("MONDE");
		for (int32 r = 0; r < REG_COUNT; ++r) {
			const physics::NkReglagesP2D *g = p != nullptr ? &p->reglages : nullptr;
			switch (r) {
				case REG_TEMPERATURE:
					std::snprintf(v, sizeof(v), "%.0f", g ? static_cast<double>(g->temperature) : 0.0);
					break;
				case REG_VENT:
					std::snprintf(v, sizeof(v), "%+.0f", g ? static_cast<double>(g->vent) : 0.0);
					break;
				case REG_GRAVITE:
					std::snprintf(v, sizeof(v), "%.1f g", g ? static_cast<double>(-g->gravite.y / 9.81f) : 1.0);
					break;
				case REG_AIR:
					std::snprintf(v, sizeof(v), "%.2f", g ? static_cast<double>(g->amortAir) : 0.0);
					break;
				default:
					std::snprintf(v, sizeof(v), "%d", g ? static_cast<int>(g->sousPas) : 0);
					break;
			}
			const float32 b = P.ligne * 1.3f;
			const float32 ty = y + (b - P.ligne) * 0.5f;
			NkTexte(dl, f, x, ty, kNomsReglages[r], mTheme.texteFaible);
			NkTexteADroite(dl, f, d.x + d.w - m * 2.5f - b * 2.f, ty, v, mTheme.texte);
			y += P.ligne * 1.7f;
		}
		for (uint32 i = 0; i < P.cibles.Size(); ++i) {
			const NkCible &c = P.cibles[i];
			if (c.genre == NkGenre::NK_REGLAGE) {
				NkBouton(dl, c.r, FontSmall(), (c.valeur & 1) ? "+" : "-", mTheme, NkDansRect(c.r, mSouris));
			}
		}

		// --- Scene -----------------------------------------------------------
		titre("SCÈNE");
		NkVector<ecs::NkEntityId> ids;
		mScene.Entites(ids);
		std::snprintf(v, sizeof(v), "%u", static_cast<unsigned>(ids.Size()));
		ligne("Entités (NKECS)", v);
		std::snprintf(v, sizeof(v), "%u", p != nullptr ? static_cast<unsigned>(p->rupturesTotal) : 0u);
		ligne("Ruptures", v);
	}

	void Physic2D::DessinerStatut(NkGuiDrawList &dl) {
		const NkPlan &P = mPlan;
		const NkRect &s = P.statut;
		dl.AddRectFilled(s, mTheme.panneau);
		dl.AddLine(NkVec2f(s.x, s.y), NkVec2f(s.x + s.w, s.y), mTheme.bord, 1.f);
		const float32 m = Maxf(4.f, P.ligne * 0.28f);
		const float32 ty = s.y + (P.ligne * 1.35f - P.ligne) * 0.5f;
		const float32 x0 = Layout().safeArea.left + m * 2.f;
		const float32 x1 = s.x + s.w - Layout().safeArea.right - m * 2.f;

		physics::NkParticules2D *p = mScene.Particules();
		char droite[128];
		std::snprintf(droite, sizeof(droite), "%u particules  ·  %u liens  ·  %.2f ms/pas",
					  p != nullptr ? static_cast<unsigned>(p->particules.Size()) : 0u, p != nullptr ? p->LiensActifs() : 0u,
					  static_cast<double>(mMsLisse));
		const float32 wd = NkTexteLargeur(FontSmall(), droite);
		NkTexteADroite(dl, FontSmall(), x1, ty, droite, mTheme.texteFaible);

		char aide[160];
		if (mOutil == NkOutil::NK_POSER) {
			const physic2d::NkInfoActeur &info = NkActeurInfo(mActeur);
			std::snprintf(aide, sizeof(aide), "%s — %s", info.nom, info.description);
		} else {
			std::snprintf(aide, sizeof(aide), "%s", OutilAide(mOutil));
		}
		NkTexte(dl, FontSmall(), x0, ty, aide, mTheme.texte, Maxf(0.f, x1 - wd - m * 4.f - x0));
	}

} // namespace nkentseu
